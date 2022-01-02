#ifndef _JBD_H_
#define _JBD_H_

/*
 * Journaling block device layer
 */

#include <kernel/synch.h>

/*
 * On-disk journal layout:
 * [ journal header (1) | block map (JSIZE * sizeof(blk_t) / BLK_SIZE) | data blocks (JSIZE) ]
 * block map maps journal data block to file system block number
 */

// Number of journal data blocks
#define JOURNAL_SIZE 128

struct super_block;

enum journal_state {
    IDLE,
    BUSY
};

struct journal {
    struct spinlock lock;
    struct condvar cv;
    // File system super block this journal belongs to
    struct super_block *sb;
    // Enable journaling
    bool enabled;
    // State of the journal
    enum journal_state state;
    // Next journal log position
    int next_index;
    // Journal data blocks
    struct blk_header *datablks[JOURNAL_SIZE];
};

struct journal_header {
    uint32_t n_blks;
};

/*
 * Initialize JBD layer.
 */
void jbd_init(void);

/*
 * Allocate a new journal for the file system.
 */
struct journal *jbd_alloc_journal(struct super_block *sb);

/*
 * Free a journal.
 */
void jbd_free_journal(struct journal *journal);

/*
 * Start a journal transaction.
 */
void jbd_begin_txn(struct journal *journal);

/*
 * End a journal transaction.
 */
void jbd_end_txn(struct journal *journal);

/*
 * Log a modified block in the journal.
 *
 * Precondition:
 * Caller must hold bh->lock.
 */
void jbd_write_blk(struct journal *journal, struct blk_header *bh);

/*
 * Recover from the file system journal.
 */
err_t jbd_recover(struct journal *journal);

#endif /* _JBD_H_ */
