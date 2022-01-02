#include <kernel/jbd.h>
#include <kernel/bdev.h>
#include <kernel/fs.h>
#include <kernel/console.h>
#include <lib/errcode.h>
#include <lib/string.h>

#define HEADER_BLK 0
#define BMAP_START_BLK (HEADER_BLK + 1)
#define BMAP_BLKS ((JOURNAL_SIZE * sizeof(blk_t) / BDEV_BLK_SIZE) + (((JOURNAL_SIZE * sizeof(blk_t)) % BDEV_BLK_SIZE) == 0 ? 0 : 1))
#define JDATA_START_BLK (BMAP_START_BLK + BMAP_BLKS)

// Allocators
static struct kmem_cache *journal_allocator;

/*
 * Commit all operations in the journal.
 *
 * Return:
 * ERR_NOMEM - Failed to allocate memory.
 */
static err_t commit_journal(struct journal *journal);

/*
 * Write all journal data blocks to the block device.
 *
 * Return:
 * ERR_NOMEM - Failed to allocate memory
 */
static err_t write_journal_blks(struct journal *journal);

/*
 * Write journal header to the block device.
 *
 * Return:
 * ERR_NOMEM - Failed to allocate memory.
 */
static err_t write_journal_header(struct journal *journal);

/*
 * Apply journal data blocks to the file system.
 *
 * Return:
 * ERR_NOMEM - Failed to allocate memory.
 */
static err_t apply_journal(struct journal *journal);

/*
 * Erase the journal.
 *
 * Return:
 * ERR_NOMEM - Failed to allocate memory.
 */
static err_t erase_journal(struct journal *journal);

static err_t
commit_journal(struct journal *journal)
{
    // Commit the journal in the following steps:
    // 1. Write all journal data blocks to bdev
    // 2. Write header to bdev (journal is now committed)
    // 3. Apply journal data blocks to the file system
    // 4. Erase the journal
    // 5. Change journal to ACTIVE state, and wake up waiters
    err_t err;

    if ((err = write_journal_blks(journal)) != ERR_OK) {
        return err;
    }
    if ((err = write_journal_header(journal)) != ERR_OK) {
        return err;
    }
    if ((err = apply_journal(journal)) != ERR_OK) {
        return err;
    }
    if ((err = erase_journal(journal)) != ERR_OK) {
        return err;
    }

    return ERR_OK;
}

static err_t
write_journal_blks(struct journal *journal)
{
    int i, bi;
    size_t n;
    blk_t pb, bmap[JOURNAL_SIZE];
    struct blk_header *jbh;
    err_t err;

    kassert(journal->next_index < JOURNAL_SIZE);
    for (i = 0; i < journal->next_index; i++) {
        // Write journal data block to the mapped physical block (using jbd_bmap
        // to get the block number) on bdev. Log the data block number in the
        // bmap block.
        pb = journal->sb->s_ops->journal_bmap(journal->sb, JDATA_START_BLK + i);
        bmap[i] = journal->datablks[i]->blk;
        if ((jbh = bdev_get_blk(journal->sb->bdev, pb)) == NULL) {
            return ERR_NOMEM;
        }
        sleeplock_acquire(&journal->datablks[i]->lock);
        memmove(jbh->data, journal->datablks[i]->data, BDEV_BLK_SIZE);
        sleeplock_release(&journal->datablks[i]->lock);
        if ((err = bdev_write_blk(jbh)) != ERR_OK) {
            return err;
        }
        bdev_release_blk(jbh);
    }
    // Write bmap blocks
    for (i = 0, bi = 0; i < journal->next_index && bi < BMAP_BLKS; bi++, i += n) {
        pb = journal->sb->s_ops->journal_bmap(journal->sb, BMAP_START_BLK + bi);
        if ((jbh = bdev_get_blk(journal->sb->bdev, pb)) == NULL) {
            return ERR_NOMEM;
        }
        n = min(BDEV_BLK_SIZE / sizeof(blk_t) , journal->next_index - i);
        memmove(jbh->data, &bmap[i], n * sizeof(blk_t));
        if ((err = bdev_write_blk(jbh)) != ERR_OK) {
            return err;
        }
        bdev_release_blk(jbh);
    }
    return ERR_OK;
}

static err_t
write_journal_header(struct journal *journal)
{
    struct journal_header header;
    struct blk_header *bh;
    blk_t pb;
    err_t err;

    header.n_blks = journal->next_index;
    pb = journal->sb->s_ops->journal_bmap(journal->sb, HEADER_BLK);
    if ((bh = bdev_get_blk(journal->sb->bdev, pb)) == NULL) {
        return ERR_NOMEM;
    }
    memmove(bh->data, &header, sizeof(header));
    if ((err = bdev_write_blk(bh)) != ERR_OK) {
        return err;
    }
    bdev_release_blk(bh);
    return ERR_OK;
}

static err_t
apply_journal(struct journal *journal)
{
    int i;
    err_t err;

    for (i = 0; i < journal->next_index; i++) {
        // Write journal data block to file system block
        sleeplock_acquire(&journal->datablks[i]->lock);
        if ((err = bdev_write_blk(journal->datablks[i])) != ERR_OK) {
            return err;
        }
        // Block is now clean
        bdev_set_blk_dirty(journal->datablks[i], False);
        // Now we can release the block
        bdev_release_blk(journal->datablks[i]);
    }
    return ERR_OK;
}

static err_t
erase_journal(struct journal *journal)
{
    journal->next_index = 0;
    return write_journal_header(journal);
}

void
jbd_init(void)
{
    if ((journal_allocator = kmem_cache_create(sizeof(struct journal))) == NULL) {
        panic("Failed to create journal_allocator");
    }
}

struct journal*
jbd_alloc_journal(struct super_block *sb)
{
    struct journal *journal;

    if ((journal = kmem_cache_alloc(journal_allocator)) != NULL) {
        memset(journal, 0, sizeof(*journal));
        spinlock_init(&journal->lock);
        condvar_init(&journal->cv);
        journal->sb = sb;
        journal->enabled = True;
        journal->state = IDLE;
        journal->next_index = 0;
    }
    return journal;
}

void
jbd_free_journal(struct journal *journal)
{
    kmem_cache_free(journal_allocator, journal);
}

void
jbd_begin_txn(struct journal *journal)
{
    if (!journal->enabled) {
        return;
    }
    spinlock_acquire(&journal->lock);
    // Wait if there is an ongoing transaction.
    while (journal->state != IDLE) {
        condvar_wait(&journal->cv, &journal->lock);
    }
    journal->state = BUSY;
    spinlock_release(&journal->lock);
}

void
jbd_end_txn(struct journal *journal)
{
    if (!journal->enabled) {
        return;
    }
    kassert(journal->state == BUSY);
    if (journal->next_index > 0) {
        // jbd uses a synchronous interface -- only return when commit is
        // successful.
        while (commit_journal(journal) != ERR_OK) {
            ;
        }
    }
    spinlock_acquire(&journal->lock);
    journal->state = IDLE;
    condvar_broadcast(&journal->cv);
    spinlock_release(&journal->lock);
}

void
jbd_write_blk(struct journal *journal, struct blk_header *bh)
{
    int i;

    if (!journal->enabled) {
        return;
    }
    kassert(journal->state == BUSY);
    // A block only need to be recorded once in the journal
    for (i = 0; i < journal->next_index; i++) {
        if (journal->datablks[i]->blk == bh->blk) {
            return;
        }
    }
    if (journal->next_index >= JOURNAL_SIZE) {
        // XXX what should we do if journal is full??
        panic("JBD: journal is filled up");
    }
    // The journal now holds a reference to the block (and the page).
    sleeplock_acquire(&bh->page->lock);
    bh->ref++;
    sleeplock_release(&bh->page->lock);
    journal->datablks[journal->next_index++] = bh;
}

err_t
jbd_recover(struct journal *journal)
{
    // XXX Not implemented yet
    return ERR_OK;
}
