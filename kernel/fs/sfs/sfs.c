#include <lib/errcode.h>
#include <kernel/fs.h>
#include <kernel/sfs.h>
#include <kernel/console.h>
#include <kernel/kmalloc.h>
#include <kernel/vpmap.h>
#include <kernel/jbd.h>
#include <lib/string.h>

/*
 * Note: We do not acquire superblock's s_lock when allocating and deallocating
 * on-disk data blocks and inodes -- we rely on buffer cache's lock for
 * synchronization.
 */

// SFS disk layout
#define SFS_SUPER_BLK 2
#define SFS_INODE_BMAP_BLK 3
#define SFS_INODE_TABLE_BLK 4

#define SFS_ROOT_INUM 1

// Limits
#define SFS_MAX_FILE_SIZE ((SFS_NDIRECT + SFS_NINDIRECT * (BDEV_BLK_SIZE / sizeof(uint32_t))) * BDEV_BLK_SIZE)

// Get journal from blk_header
#define BH_JOURNAL(bh) (((struct sfs_sb_info*)bh->bdev->sb->s_fs_info)->journal)

// Get sfs_sb_info from super_block
#define SB_INFO(sb) ((struct sfs_sb_info*)sb->s_fs_info)

// Get sfs_inode_info from inode
#define INODE_INFO(inode) ((struct sfs_inode_info*)inode->i_fs_info)

// Acquire reference to disk inode
#define ACQUIRE_INODE_BH(inode) (bdev_get_blk_unlocked(inode->sb->bdev, inum_to_blk(inode->sb, inode->i_inum)))

/*
 * SFS-specific VFS functiions
 */
// File system type operations
static struct super_block *sfs_get_sb(struct bdev *bdev, struct fs_type *fs_type);
static void sfs_free_sb(struct super_block *sb);
static err_t sfs_write_sb(struct super_block *sb);
static struct fs_type sfs_fs_type = {
    .fs_name = "sfs",
    .get_sb = sfs_get_sb,
    .free_sb = sfs_free_sb,
    .write_sb = sfs_write_sb,
};

// Superblock operations
static blk_t sfs_journal_bmap(struct super_block *sb, blk_t lb);
static void sfs_journal_begin_txn(struct super_block *sb);
static void sfs_journal_end_txn(struct super_block *sb);
static struct inode *sfs_alloc_inode(struct super_block *sb);
static void sfs_free_inode(struct inode *inode);
static err_t sfs_read_inode(struct inode *inode);
static err_t sfs_write_inode(struct inode *inode);
static err_t sfs_delete_inode(struct inode *inode);
static struct super_operations sfs_super_operations = {
    .journal_bmap = sfs_journal_bmap,
    .journal_begin_txn = sfs_journal_begin_txn,
    .journal_end_txn = sfs_journal_end_txn,
    .alloc_inode = sfs_alloc_inode,
    .free_inode = sfs_free_inode,
    .read_inode = sfs_read_inode,
    .write_inode = sfs_write_inode,
    .delete_inode = sfs_delete_inode
};
// Inode operations
static err_t sfs_create(struct inode *dir, const char *name, fmode_t mode);
static err_t sfs_mkdir(struct inode *dir, const char *name, fmode_t mode);
static err_t sfs_rmdir(struct inode *dir, const char *name);
static err_t sfs_lookup(struct inode *dir, const char *name, struct inode **inode);
static err_t sfs_fillpage(struct inode *inode, offset_t ofs, struct page *page);
static err_t sfs_link(struct inode *dir, struct inode *src, const char *name);
static err_t sfs_unlink(struct inode *dir, const char *name);
static struct inode_operations sfs_inode_operations = {
    .create = sfs_create,
    .mkdir = sfs_mkdir,
    .rmdir = sfs_rmdir,
    .lookup = sfs_lookup,
    .fillpage = sfs_fillpage,
    .link = sfs_link,
    .unlink = sfs_unlink
};
// File operations
static ssize_t sfs_read(struct file *file, void *buf, size_t count, offset_t *ofs);
static ssize_t sfs_write(struct file *file, const void *buf, size_t count, offset_t *ofs);
static err_t sfs_readdir(struct file *dir, struct dirent *dirent);
static struct file_operations sfs_file_operations = {
    .read = sfs_read,
    .write = sfs_write,
    .readdir = sfs_readdir
};

// SFS super block allocator
static struct kmem_cache *sfs_sb_allocator = NULL;

// SFS inode allocator
static struct kmem_cache *sfs_inode_allocator = NULL;

// Return the minimum of the two numbers
#define min(a, b) ((a < b) ? a : b)

// Convert inode number to block number
static inline blk_t inum_to_blk(const struct super_block *sb, inum_t inum);

// Convert inode number to byte offset within a block
static inline offset_t inum_to_ofs(inum_t inum);

/*
 * Search the bitmap block and return the index of the first free element, and
 * mark the element as in-use. Return -1 if no free element available.
 *
 * Precondition:
 * Caller must hold bh->lock.
 *
 * Postcondition:
 * If an element is allocated (non-negative value returned), the block buffer is
 * marked dirty.
 */
static int bmap_alloc_element(struct blk_header *bh, size_t size);

/*
 * Mark the element as free in the bitmap block.
 *
 * Precondition:
 * Caller must hold bh->lock.
 *
 * Postcondition:
 * The block buffer is marked dirty.
 */
static void bmap_free_element(struct blk_header *bh, int index);

/*
 * Allocate a new on-disk inode, and write the inode number into *inum. The new
 * on-disk inode will have one hard link and the specified file type and
 * permission.
 *
 * Return:
 * ERR_NOMEM - Failed to allocate memory.
 * ERR_NORES - Failed to allocate on-disk inode.
 */
static err_t alloc_disk_inode(struct super_block *sb, ftype_t ftype, fmode_t mode, inum_t *inum);

/*
 * Free an on-disk inode associated with the in-memory inode structure.
 *
 * Return:
 * ERR_NOMEM - Failed to allocate memory.
 */
static err_t free_disk_inode(struct super_block *sb, inum_t inum);

/*
 * Create a new inode with file type ftype in dir.
 *
 * Precondition:
 * Caller must hold dir->i_lock.
 *
 * Return:
 * ERR_EXIST - Another file/dir already exist with the same name.
 * ERR_NOMEM - Failed to allocate memory.
 * ERR_NORES - Failed to allocate new inode in dir.
 */
static err_t create_inode_in_dir(struct inode *dir, ftype_t ftype, const char *name, fmode_t mode);

/*
 * Unlink an inode in a directory with the dirent name. Only unlink if the inode
 * has file type ``ftype``.
 *
 * Precondition:
 * Caller must hold dir->i_lock.
 *
 * Return:
 * ERR_NOMEM - Failed to allocate memory.
 * ERR_NOTEXIST - Inode does not exist in dir.
 * ERR_FTYPE - Wrong file type.
 * ERR_NOTEMPTY - 'ftype' is FTYPE_DIR and the directory is not empty.
 */
static err_t unlink_inode_in_dir(struct inode *dir, ftype_t ftype, const char *name);

/*
 * Allocate a new data block. Write the block number into *blk.
 *
 * Return:
 * ERR_NOMEM - Failed to allocate memory.
 * ERR_NORES - No more data blocks are available.
 */
static err_t alloc_data_block(struct super_block *sb, blk_t *blk);

/*
 * Free a data block.
 *
 * Return:
 * ERR_NOMEM - Failed to allocate memory.
 */
static err_t free_data_block(struct super_block *sb, blk_t blk);

/*
 * Allocate a directory entry in dir with the specified inode number and name.
 *
 * Precondition:
 * Caller must hold dir->i_lock.
 * Another directory entry with the same name must not exist in dir.
 *
 * Return:
 * ERR_NOMEM - Failed to allocate memory.
 * ERR_NORES - Failed to allocate directory entry.
 */
static err_t alloc_dirent(struct inode *dir, const char *name, inum_t inum);

/*
 * Remove a directory entry in dir.
 *
 * Precondition:
 * Caller must hold dir->i_lock.
 *
 * Return:
 * ERR_NOMEM - Failed to allocate memory.
 * ERR_NOTEXIST - Directory entry does not exist.
 */
static err_t free_dirent(struct inode *dir, const char *name);

/*
 * Check if the directory is empty. Write result in *empty.
 *
 * Precondition:
 * Caller must hold dir->i_lock.
 *
 * Return:
 * ERR_NOMEM - Failed to allocate memory.
 */
static err_t is_dir_empty(struct inode *dir, int *empty);

/*
 * Search for an inode in directory dir.
 *
 * Precondition:
 * Caller must hold dir->i_lock.
 *
 * Return:
 * The inode number, or 0 if lookup failed.
 */
static inum_t search_dir(struct inode *dir, const char *name);

/*
 * Get the data block of an inode that contains inode offset ofs. Write the
 * block buffer header into *buf. If alloc is True, allocate a new data block if block
 * does not exist yet.
 *
 * Precondition:
 * Caller must hold inode->i_lock.
 *
 * Postcondition:
 * If successful, bh->lock is locked.
 *
 * Return:
 * ERR_NOMEM - Failed to allocate memory.
 * ERR_NOTEXIST - Data block has not been allocated yet (for alloc = False).
 * ERR_NORES - No data block available (for alloc = True).
 */
static err_t get_data_block(struct inode *inode, offset_t ofs, struct blk_header **bh, int alloc);

/*
 * Read count number of bytes at inode offset ofs into buffer buf.
 *
 * Precondition:
 * Caller must hold inode->i_lock.
 *
 * Return:
 * The number of bytes read, or -1 if an error occurs.
 */
static ssize_t read_data(struct inode *inode, void *buf, size_t count, offset_t ofs);

/*
 * Write count number of bytes from buffer buf to inode offset ofs.
 *
 * Precondition:
 * Caller must hold inode->i_lock.
 *
 * Return:
 * The number of bytes written, or -1 if an error occurs.
 */
static ssize_t write_data(struct inode *inode, const void *buf, size_t count, offset_t ofs);

static inline blk_t
inum_to_blk(const struct super_block *sb, inum_t inum)
{
    // inode number starts from 1
    return SFS_INODE_TABLE_BLK + (inum - 1) / (BDEV_BLK_SIZE / sizeof(struct sfs_inode));
}

static inline offset_t
inum_to_ofs(inum_t inum)
{
    // inode number starts from 1
    return ((inum - 1) * sizeof(struct sfs_inode)) % BDEV_BLK_SIZE;
}

static int
bmap_alloc_element(struct blk_header *bh, size_t size)
{
    uint8_t *bmap;
    int byte, bit;
    uint8_t mask;

    for (byte = 0, bmap = (uint8_t*)bh->data; byte < size; byte++) {
        for (bit = 0; bit < 8; bit++) {
            mask = 1 << bit;
            if ((bmap[byte] & mask) == 0) {
                // Found a free element, mark it in-use
                bmap[byte] |= mask;
                bdev_set_blk_dirty(bh, True);
                jbd_write_blk(BH_JOURNAL(bh), bh);
                return byte * 8 + bit;
            }
        }
    }
    return -1;
}

static void
bmap_free_element(struct blk_header *bh, int index)
{
    uint8_t *bmap;
    uint8_t mask;

    bmap = (uint8_t*)bh->data;
    mask = 1 << (index % 8);
    bmap[index / 8] &= ~mask;
    bdev_set_blk_dirty(bh, True);
    jbd_write_blk(BH_JOURNAL(bh), bh);
}

static err_t
alloc_disk_inode(struct super_block *sb, ftype_t ftype, fmode_t mode, inum_t *inum)
{
    int index;
    struct blk_header *bmap_bh, *inode_bh;
    struct sfs_inode *sfs_inode;

    // Use inode bitmap to find a free inode
    if ((bmap_bh = bdev_get_blk(sb->bdev, SFS_INODE_BMAP_BLK)) == NULL) {
        return ERR_NOMEM;
    }
    if ((index = bmap_alloc_element(bmap_bh, SB_INFO(sb)->s_num_inodes / 8)) == -1) {
        bdev_release_blk(bmap_bh);
        return ERR_NORES;
    }
    // Not releasing bmap_bh immediately -- we might need to free the inode
    // again in case of errors

    // inode number starts from 1
    *inum = index + 1;

    // Update the new on-disk inode
    if ((inode_bh = bdev_get_blk(sb->bdev, inum_to_blk(sb, *inum))) == NULL) {
        bmap_free_element(bmap_bh, index);
        bdev_release_blk(bmap_bh);
        return ERR_NOMEM;
    }
    bdev_release_blk(bmap_bh);

    sfs_inode = (struct sfs_inode*)((uint8_t*)inode_bh->data + inum_to_ofs(*inum));
    memset(sfs_inode, 0, sizeof(struct sfs_inode));
    sfs_inode->i_ftype = ftype;
    sfs_inode->i_mode = mode;
    sfs_inode->i_nlink = 1;
    bdev_set_blk_dirty(inode_bh, True);
    jbd_write_blk(BH_JOURNAL(inode_bh), inode_bh);
    bdev_release_blk(inode_bh);

    return ERR_OK;
}

static err_t
free_disk_inode(struct super_block *sb, inum_t inum)
{
    struct blk_header *bh;

    kassert(inum != 0);
    // Mark corresponding entry in the inode bitmap as free
    if ((bh = bdev_get_blk(sb->bdev, SFS_INODE_BMAP_BLK)) == NULL) {
        return ERR_NOMEM;
    }
    bmap_free_element(bh, inum - 1); // inode number starts from 1

    // We don't need to update the corresponding on-disk inode in block cache or
    // on disk. We will initialize the on-disk inode properly in
    // alloc_disk_inode.
    bdev_release_blk(bh);

    return ERR_OK;
}

static err_t
create_inode_in_dir(struct inode *dir, ftype_t ftype, const char *name, fmode_t mode)
{
    struct blk_header *bmap_bh;
    inum_t inum;
    err_t err;

    // First check if name already exist
    if ((inum = search_dir(dir, name)) != 0) {
        return ERR_EXIST;
    }
    // Acquire a reference to the inode bitmap (without locking) to pin it in
    // memory, so that if alloc_disk_inode succeeds but alloc_dirent fails, we
    // can free the disk inode without an error.
    if ((bmap_bh = bdev_get_blk_unlocked(dir->sb->bdev, SFS_INODE_BMAP_BLK)) == NULL) {
        return ERR_NOMEM;
    }
    // Allocate a new on-disk inode
    if ((err = alloc_disk_inode(dir->sb, ftype, mode, &inum)) != ERR_OK) {
        bdev_release_blk_unlocked(bmap_bh);
        return err;
    }
    // Allocate a directory entry in dir
    if ((err = alloc_dirent(dir, name, inum)) != ERR_OK) {
        // Free disk inode should never fail, because we have pinned inode
        // bitmap in memory
        free_disk_inode(dir->sb, inum);
        bdev_release_blk_unlocked(bmap_bh);
        return err;
    }
    bdev_release_blk_unlocked(bmap_bh);
    return ERR_OK;
}

static err_t
unlink_inode_in_dir(struct inode *dir, ftype_t ftype, const char *name)
{
    struct inode *inode;
    struct blk_header *inode_bh;
    int empty;
    err_t err;

    // First find the inode that the name refers to
    if ((err = sfs_lookup(dir, name, &inode)) != ERR_OK) {
        return err;
    }

    // Acquire reference to the disk inode
    if ((inode_bh = ACQUIRE_INODE_BH(inode)) == NULL) {
        return ERR_NOMEM;
    }

    sleeplock_acquire(&inode->i_lock);

    kassert(inode->i_inum > 0);
    kassert(inode->i_nlink > 0);
    kassert(fs_is_inode_valid(inode));

    // Check file type
    if (inode->i_ftype != ftype) {
        err = ERR_FTYPE;
        goto fail;
    }
    // If we are unlinking a directory, make sure the directory is empty
    if (ftype == FTYPE_DIR) {
        if ((err = is_dir_empty(inode, &empty)) != ERR_OK) {
            goto fail;
        }
        if (!empty) {
            err = ERR_NOTEMPTY;
            goto fail;
        }
    }
    // Remove directory entry from the parent
    if ((err = free_dirent(dir, name)) != ERR_OK) {
        kassert(err != ERR_NOTEXIST);
        goto fail;
    }
    // Decrement link count
    inode->i_nlink--;
    fs_set_inode_dirty(inode, True);
    // sfs_write_inode will not fail
    sfs_write_inode(inode);

    sleeplock_release(&inode->i_lock);
    bdev_release_blk_unlocked(inode_bh);
    fs_release_inode(inode);

    return ERR_OK;

fail:
    sleeplock_release(&inode->i_lock);
    bdev_release_blk_unlocked(inode_bh);
    fs_release_inode(inode);
    return err;
}

static err_t
alloc_data_block(struct super_block *sb, blk_t *blk)
{
    struct blk_header *bmap_bh, *data_bh;
    blk_t bmap_blk;
    int index;
    size_t num_blks;

    // Use data block bitmap to find a free block
    for (bmap_blk = SB_INFO(sb)->s_data_bmap_start, num_blks = SB_INFO(sb)->s_size; bmap_blk < SB_INFO(sb)->s_journal_start; bmap_blk++, num_blks -= BDEV_BLK_SIZE * 8) {
        if ((bmap_bh = bdev_get_blk(sb->bdev, bmap_blk)) == NULL) {
            return ERR_NOMEM;
        }
        if ((index = bmap_alloc_element(bmap_bh, min(BDEV_BLK_SIZE, num_blks / 8))) >= 0) {
            // Not releasing bmap_bh immediately -- we might need to free the
            // inode again in case of errors
            *blk = SB_INFO(sb)->s_data_start + BDEV_BLK_SIZE * 8 * (bmap_blk - SB_INFO(sb)->s_data_bmap_start) + index;
            // Fill newly allocated block with zero
            if ((data_bh = bdev_get_blk(sb->bdev, *blk)) == NULL) {
                bmap_free_element(bmap_bh, index);
                bdev_release_blk(bmap_bh);
                return ERR_NOMEM;
            }
            bdev_release_blk(bmap_bh);

            memset(data_bh->data, 0, BDEV_BLK_SIZE);
            bdev_set_blk_dirty(data_bh, True);
            jbd_write_blk(BH_JOURNAL(data_bh), data_bh);
            bdev_release_blk(data_bh);

            return ERR_OK;
        }
        bdev_release_blk(bmap_bh);
    }
    return ERR_NORES;
}

static err_t
free_data_block(struct super_block *sb, blk_t blk)
{
    struct blk_header *bh;
    blk_t bmap_blk;

    // Mark data block bitmap entry as free
    kassert(blk >= SB_INFO(sb)->s_data_start);
    bmap_blk = SB_INFO(sb)->s_data_bmap_start + (blk - SB_INFO(sb)->s_data_start) / (BDEV_BLK_SIZE * 8);
    if ((bh = bdev_get_blk(sb->bdev, bmap_blk)) == NULL) {
        return ERR_NOMEM;
    }
    bmap_free_element(bh, (blk - SB_INFO(sb)->s_data_start) % (BDEV_BLK_SIZE * 8));
    bdev_release_blk(bh);
    return ERR_OK;
}

static err_t
alloc_dirent(struct inode *dir, const char *name, inum_t inum)
{
    struct sfs_dirent *dirent;
    struct blk_header *bh, *inode_bh;
    offset_t ofs;
    err_t err;

    // Acquire reference to the disk inode in case we need to update it.
    if ((inode_bh = ACQUIRE_INODE_BH(dir)) == NULL) {
        return ERR_NOMEM;
    }
    // Iterate through all blocks in the dir inode and find the first free
    // directory entry.
    for (ofs = 0, bh = NULL; ofs < SFS_MAX_FILE_SIZE; ofs += sizeof(struct sfs_dirent), dirent++) {
        if (ofs % BDEV_BLK_SIZE == 0) {
            if (bh != NULL) {
                bdev_release_blk(bh);
            }
            if ((err = get_data_block(dir, ofs, &bh, True)) != ERR_OK) {
                bdev_release_blk_unlocked(inode_bh);
                return err;
            }
            dirent = (struct sfs_dirent*)bh->data;
        }
        if (ofs >= dir->i_size || dirent->inum == 0) {
            // Free/new directory entry
            if (ofs >= dir->i_size) {
                // Update dir inode size
                dir->i_size = ofs + sizeof(struct sfs_dirent);
                fs_set_inode_dirty(dir, True);
                // sfs_write_inode should never fail because we acquired the disk
                // inode reference
                sfs_write_inode(dir);
            }
            dirent->inum = inum;
            strncpy(dirent->name, name, SFS_DIRENT_NAMELEN);
            // Make sure name ends with null
            dirent->name[SFS_DIRENT_NAMELEN-1] = 0;
            bdev_set_blk_dirty(bh, True);
            jbd_write_blk(BH_JOURNAL(bh), bh);
            bdev_release_blk(bh);
            bdev_release_blk_unlocked(inode_bh);
            return ERR_OK;
        }
    }
    if (bh != NULL) {
        bdev_release_blk(bh);
    }
    bdev_release_blk_unlocked(inode_bh);
    return ERR_NORES;
}

static err_t
free_dirent(struct inode *dir, const char *name)
{
    struct sfs_dirent *dirent;
    struct blk_header *bh;
    offset_t ofs;
    err_t err;

    // Iterate through all blocks in the dir inode. If the target directory
    // entry is found, remove it.
    for (ofs = 0, bh = NULL; ofs < dir->i_size; ofs += sizeof(struct sfs_dirent), dirent++) {
        if (ofs % BDEV_BLK_SIZE == 0) {
            if (bh != NULL) {
                bdev_release_blk(bh);
            }
            if ((err = get_data_block(dir, ofs, &bh, False)) != ERR_OK) {
                return err;
            }
            dirent = (struct sfs_dirent*)bh->data;
        }
        // Check if there is a match
        if (dirent->inum > 0 && strncmp(dirent->name, name, SFS_DIRENT_NAMELEN) == 0) {
            // Remove directory entry
            dirent->inum = 0;
            bdev_set_blk_dirty(bh, True);
            jbd_write_blk(BH_JOURNAL(bh), bh);
            bdev_release_blk(bh);
            // Do not update dir inode size here.

            return ERR_OK;
        }
    }
    if (bh != NULL) {
        bdev_release_blk(bh);
    }
    return ERR_NOTEXIST;
}

static err_t
is_dir_empty(struct inode *dir, int *empty)
{
    struct sfs_dirent *dirent;
    struct blk_header *bh;
    offset_t ofs;
    err_t err;

    kassert(empty);

    // Iterate through all blocks in the dir inode. Return false if any dirent
    // is allocated.
    for (ofs = 0, bh = NULL; ofs < dir->i_size; ofs += sizeof(struct sfs_dirent), dirent++) {
        if (ofs % BDEV_BLK_SIZE == 0) {
            if (bh != NULL) {
                bdev_release_blk(bh);
            }
            if ((err = get_data_block(dir, ofs, &bh, False)) != ERR_OK) {
                return err;
            }
            dirent = (struct sfs_dirent*)bh->data;
        }
        // Check if dirent is allocated
        if (dirent->inum > 0) {
            *empty = False;
            bdev_release_blk(bh);
            return ERR_OK;
        }
    }
    *empty = True;
    if (bh != NULL) {
        bdev_release_blk(bh);
    }
    return ERR_OK;
}

static inum_t
search_dir(struct inode *dir, const char *name)
{
    struct sfs_dirent *dirent;
    struct blk_header *bh;
    offset_t ofs;
    err_t err;

    // Iterate through all blocks in the dir inode to find a match
    for (ofs = 0, bh = NULL; ofs < dir->i_size; ofs += sizeof(struct sfs_dirent), dirent++) {
        if (ofs % BDEV_BLK_SIZE == 0) {
            if (bh != NULL) {
                bdev_release_blk(bh);
            }
            if ((err = get_data_block(dir, ofs, &bh, False)) != ERR_OK) {
                return 0;
            }
            dirent = (struct sfs_dirent*)bh->data;
        }
        if (dirent->inum > 0 && strncmp(dirent->name, name, SFS_DIRENT_NAMELEN) == 0) {
            // Found a match
            bdev_release_blk(bh);
            return dirent->inum;
        }
    }
    if (bh != NULL) {
        bdev_release_blk(bh);
    }
    return 0;
}

static err_t
get_data_block(struct inode *inode, offset_t ofs, struct blk_header **bh, int alloc)
{
    int blk_index, indir_blk_index, indir_blk_ofs;
    blk_t blk, indir_blk;
    struct blk_header *indir_bh, *inode_bh;
    err_t err;

    kassert(ofs < SFS_MAX_FILE_SIZE);

    indir_bh = NULL;
    // Acquire reference to the disk inode in case we need to update it.
    if ((inode_bh = ACQUIRE_INODE_BH(inode)) == NULL) {
        return ERR_NOMEM;
    }

    blk_index = ofs / BDEV_BLK_SIZE;
    if (blk_index < SFS_NDIRECT) {
        // Direct block
        blk = INODE_INFO(inode)->i_addrs[blk_index];
        if (blk == 0) {
            // Data block has not been allocated before -- allocate one.
            if (!alloc) {
                err = ERR_NOTEXIST;
                goto fail;
            }
            if ((err = alloc_data_block(inode->sb, &blk)) != ERR_OK) {
                goto fail;
            }
            // Update direct block with the newly allocated data block.
            INODE_INFO(inode)->i_addrs[blk_index] = blk;
            fs_set_inode_dirty(inode, True);
            // sfs_write_inode should not fail because we acquired the disk
            // inode reference
            sfs_write_inode(inode);
        }
    } else {
        // Indirect block
        indir_blk_index = SFS_NDIRECT + (blk_index - SFS_NDIRECT) / (BDEV_BLK_SIZE / sizeof(uint32_t));
        kassert(indir_blk_index < SFS_NDIRECT + SFS_NINDIRECT);
        // If indirect block doesn't exist yet, allocate one first.
        if (INODE_INFO(inode)->i_addrs[indir_blk_index] == 0) {
            if (!alloc) {
                err = ERR_NOTEXIST;
                goto fail;
            }
            if ((err = alloc_data_block(inode->sb, &indir_blk)) != ERR_OK) {
                goto fail;
            }
            INODE_INFO(inode)->i_addrs[indir_blk_index] = indir_blk;
            fs_set_inode_dirty(inode, True);
            // sfs_write_inode should not fail because we acquired the disk
            // inode reference
            sfs_write_inode(inode);
        }
        // Now the indirect block is guaranteed to exist. Read the block number
        // from the indirect block.
        kassert(INODE_INFO(inode)->i_addrs[indir_blk_index] > 0);
        if ((indir_bh = bdev_get_blk(inode->sb->bdev, INODE_INFO(inode)->i_addrs[indir_blk_index])) == NULL) {
            // Do not roll back indirect block allocation. Okay to have an empty
            // indirect block. (Same reasoning applies to subsequent errors too)
            err = ERR_NOMEM;
            goto fail;
        }
        indir_blk_ofs = (blk_index - SFS_NDIRECT) % (BDEV_BLK_SIZE / sizeof(uint32_t));
        blk = ((blk_t*)indir_bh->data)[indir_blk_ofs];
        if (blk == 0) {
            // Data block has not been allocated before -- allocate one.
            if (!alloc) {
                err = ERR_NOTEXIST;
                goto fail;
            }
            if ((err = alloc_data_block(inode->sb, &blk)) != ERR_OK) {
                goto fail;
            }
            // Write newly allocate data block to the indirect block.
            ((blk_t*)indir_bh->data)[indir_blk_ofs] = blk;
            bdev_set_blk_dirty(indir_bh, True);
            jbd_write_blk(BH_JOURNAL(indir_bh), indir_bh);
        }
        bdev_release_blk(indir_bh);
    }

    kassert(blk > 0);
    // Now read the data block.
    if ((*bh = bdev_get_blk(inode->sb->bdev, blk)) == NULL) {
        // Do not roll back data block allocation (or indirect block
        // allocation). sfs_delete_inode will free them correctly.
        err = ERR_NOMEM;
        goto fail;
    }

    bdev_release_blk_unlocked(inode_bh);
    return ERR_OK;

fail:
    if (indir_bh != NULL) {
        bdev_release_blk(indir_bh);
    }
    if (inode_bh != NULL) {
        bdev_release_blk_unlocked(inode_bh);
    }
    return err;
}

static ssize_t
read_data(struct inode *inode, void *buf, size_t count, offset_t ofs)
{
    struct blk_header *bh;
    ssize_t total, s;
    uint8_t *dst_buf, *blk_buf;

    dst_buf = (uint8_t*)buf;
    for (total = 0; total < count && ofs < inode->i_size; ofs += s, dst_buf += s, total += s) {
        // Do not allocate new data block here
        if (get_data_block(inode, ofs, &bh, False) != ERR_OK) {
            break;
        }
        blk_buf = (uint8_t*)bh->data;
        s = min(min(BDEV_BLK_SIZE - ofs % BDEV_BLK_SIZE, count - total), inode->i_size - ofs);
        memmove(dst_buf, blk_buf + (ofs % BDEV_BLK_SIZE), s);
        bdev_release_blk(bh);
    }
    return total;
}

static ssize_t
write_data(struct inode *inode, const void *buf, size_t count, offset_t ofs)
{
    struct blk_header *bh;
    ssize_t total, s;
    uint8_t *src_buf, *blk_buf;

    kassert(inode);
    kassert(buf);

    src_buf = (uint8_t*)buf;
    for (total = 0; total < count; ofs += s, src_buf += s, total += s) {
        // Allocate new data block if not exist
        if (get_data_block(inode, ofs, &bh, True) != ERR_OK) {
            break;
        }
        blk_buf = (uint8_t*)bh->data;
        s = min(BDEV_BLK_SIZE - ofs % BDEV_BLK_SIZE, count - total);
        memmove(blk_buf + (ofs % BDEV_BLK_SIZE), src_buf, s);
        bdev_set_blk_dirty(bh, True);
        jbd_write_blk(BH_JOURNAL(bh), bh);
        bdev_release_blk(bh);
    }
    if (count > 0 && ofs > inode->i_size) {
        inode->i_size = ofs;
        fs_set_inode_dirty(inode, True);
        sfs_write_inode(inode);
    }
    return total;
}

static struct super_block*
sfs_get_sb(struct bdev *bdev, struct fs_type *fs_type)
{
    struct super_block *sb;
    struct blk_header *bh;
    struct sfs_sb *sfs_sb;
    struct sfs_sb_info *info;

    info = NULL;
    if ((sb = fs_alloc_sb(bdev, fs_type)) == NULL) {
        goto fail;
    }
    if ((info = kmem_cache_alloc(sfs_sb_allocator)) == NULL) {
        goto fail;
    }
    // Read SFS super block
    if ((bh = bdev_get_blk(bdev, SFS_SUPER_BLK)) == NULL) {
        goto fail;
    }
    sfs_sb = (struct sfs_sb*)bh->data;
    sb->s_root_inum = SFS_ROOT_INUM;
    info->s_size = sfs_sb->s_size;
    info->s_num_inodes = sfs_sb->s_num_inodes;
    info->s_data_bmap_start = sfs_sb->s_data_bmap_start;
    info->s_journal_start = sfs_sb->s_journal_start;
    info->s_data_start = sfs_sb->s_data_start;
    if ((info->journal = jbd_alloc_journal(sb)) == NULL) {
        goto fail;
    }
    // XXX disable journaling for now...
    info->journal->enabled = True;
    sb->s_fs_info = info;
    sb->s_ops = &sfs_super_operations;
    bdev_release_blk(bh);
    return sb;

fail:
    if (info != NULL) {
        kmem_cache_free(sfs_sb_allocator, info);
    }
    if (sb != NULL) {
        fs_free_sb(sb);
    }
    return NULL;
}

static void
sfs_free_sb(struct super_block *sb)
{
    kmem_cache_free(sfs_sb_allocator, sb->s_fs_info);
    fs_free_sb(sb);
}

static err_t
sfs_write_sb(struct super_block *sb)
{
    struct sfs_sb *sfs_sb;
    struct blk_header *bh;

    // Read in block that contains the on-disk superblock
    if ((bh = bdev_get_blk(sb->bdev, SFS_SUPER_BLK)) == NULL) {
        return ERR_NOMEM;
    }

    // Update superblock in block buffer
    sfs_sb = (struct sfs_sb*)bh->data;
    sfs_sb->s_size = SB_INFO(sb)->s_size;
    sfs_sb->s_num_inodes = SB_INFO(sb)->s_num_inodes;
    sfs_sb->s_data_bmap_start = SB_INFO(sb)->s_data_bmap_start;
    sfs_sb->s_journal_start = SB_INFO(sb)->s_journal_start;
    sfs_sb->s_data_start = SB_INFO(sb)->s_data_start;
    bdev_set_blk_dirty(bh, True);
    jbd_write_blk(BH_JOURNAL(bh), bh);
    bdev_release_blk(bh);
    fs_set_sb_dirty(sb, False);

    return ERR_OK;
}

static blk_t
sfs_journal_bmap(struct super_block *sb, blk_t lb)
{
    return SB_INFO(sb)->s_journal_start + lb;
}

static void
sfs_journal_begin_txn(struct super_block *sb)
{
    jbd_begin_txn(SB_INFO(sb)->journal);
}

static void
sfs_journal_end_txn(struct super_block *sb)
{
    jbd_end_txn(SB_INFO(sb)->journal);
}

static struct inode*
sfs_alloc_inode(struct super_block *sb)
{
    struct inode *inode;
    struct sfs_inode_info *inode_info;

    // Allocate a new inode object
    if ((inode = fs_alloc_inode(sb)) == NULL) {
        return NULL;
    }
    if ((inode_info = kmem_cache_alloc(sfs_inode_allocator)) == NULL) {
        fs_free_inode(inode);
        return NULL;
    }
    memset(inode_info, 0, sizeof(struct sfs_inode_info));
    inode->i_fs_info = inode_info;
    inode->i_ops = &sfs_inode_operations;
    inode->i_fops = &sfs_file_operations;

    return inode;
}

static void
sfs_free_inode(struct inode *inode)
{
    kmem_cache_free(sfs_inode_allocator, inode->i_fs_info);
    fs_free_inode(inode);
}

static err_t
sfs_read_inode(struct inode *inode)
{
    struct sfs_inode *sfs_inode;
    struct blk_header *bh;

    kassert(inode->i_inum != 0);
    kassert(!fs_is_inode_dirty(inode));

    // Read in block that contains the on-disk inode
    if ((bh = bdev_get_blk(inode->sb->bdev, inum_to_blk(inode->sb, inode->i_inum))) == NULL) {
        return ERR_NOMEM;
    }

    // Update in-memory inode
    sfs_inode = (struct sfs_inode*)((uint8_t*)bh->data + inum_to_ofs(inode->i_inum));
    inode->i_ftype = sfs_inode->i_ftype;
    inode->i_mode = sfs_inode->i_mode;
    inode->i_nlink = sfs_inode->i_nlink;
    inode->i_size = sfs_inode->i_size;
    memmove(INODE_INFO(inode)->i_addrs, sfs_inode->i_addrs, sizeof(INODE_INFO(inode)->i_addrs));
    fs_set_inode_valid(inode, True);
    bdev_release_blk(bh);

    return ERR_OK;
}

static err_t
sfs_write_inode(struct inode *inode)
{
    struct sfs_inode *sfs_inode;
    struct blk_header *bh;

    kassert(inode->i_inum != 0);

    // Read in block that contains the on-disk inode
    if ((bh = bdev_get_blk(inode->sb->bdev, inum_to_blk(inode->sb, inode->i_inum))) == NULL) {
        return ERR_NOMEM;
    }

    // Update inode in block cache with in-memory object
    sfs_inode = (struct sfs_inode*)((uint8_t*)bh->data + inum_to_ofs(inode->i_inum));
    sfs_inode->i_ftype = inode->i_ftype;
    sfs_inode->i_mode = inode->i_mode;
    sfs_inode->i_nlink = inode->i_nlink;
    sfs_inode->i_size = inode->i_size;
    memmove(sfs_inode->i_addrs, INODE_INFO(inode)->i_addrs, sizeof(sfs_inode->i_addrs));
    bdev_set_blk_dirty(bh, True);
    jbd_write_blk(BH_JOURNAL(bh), bh);
    fs_set_inode_dirty(inode, False);
    bdev_release_blk(bh);

    return ERR_OK;
}

static err_t
sfs_delete_inode(struct inode *inode)
{
    size_t size;
    int index, indir_index, is_journaled;
    blk_t blk;
    struct blk_header *bh;
    err_t err;

    kassert(inode->i_inum > 0);
    kassert(inode->i_nlink == 0);

    // Free all data blocks
    for (size = 0, index = 0; size < inode->i_size; index++) {
        blk = INODE_INFO(inode)->i_addrs[index];
        if (blk == 0) {
            // Already freed
            continue;
        }
        kassert(blk >= SB_INFO(inode->sb)->s_data_start);
        if (index < SFS_NDIRECT) {
            // Direct block
            size += BDEV_BLK_SIZE;
        } else {
            // Indirect block
            if ((bh = bdev_get_blk(inode->sb->bdev, blk)) == NULL) {
                return ERR_NOMEM;
            }
            for (indir_index = 0, is_journaled = False; size < inode->i_size; indir_index++, size += BDEV_BLK_SIZE) {
                if (((blk_t*)bh->data)[indir_index] == 0) {
                    // Already freed
                    continue;
                }
                kassert(((blk_t*)bh->data)[indir_index] >= SB_INFO(inode->sb)->s_data_start);
                if ((err = free_data_block(inode->sb, ((blk_t*)bh->data)[indir_index])) != ERR_OK) {
                    return err;
                }
                ((blk_t*)bh->data)[indir_index] = 0;
                if (!is_journaled) {
                    bdev_set_blk_dirty(bh, True);
                    jbd_write_blk(BH_JOURNAL(bh), bh);
                    is_journaled = True;
                }
            }
            bdev_release_blk(bh);
        }
        if ((err = free_data_block(inode->sb, blk)) != ERR_OK) {
            return err;
        }
        INODE_INFO(inode)->i_addrs[index] = 0;
    }

    // Free the on-disk inode
    if ((err = free_disk_inode(inode->sb, inode->i_inum)) != ERR_OK) {
        return err;
    }
    // Now the in-memory inode has no backing disk inode
    inode->i_inum = 0;
    fs_set_inode_valid(inode, False);
    fs_set_inode_dirty(inode, False);

    return ERR_OK;
}

static err_t
sfs_create(struct inode *dir, const char *name, fmode_t mode)
{
    return create_inode_in_dir(dir, FTYPE_FILE, name, mode);
}

static err_t
sfs_mkdir(struct inode *dir, const char *name, fmode_t mode)
{
    return create_inode_in_dir(dir, FTYPE_DIR, name, mode);
}

static err_t
sfs_rmdir(struct inode *dir, const char *name)
{
    return unlink_inode_in_dir(dir, FTYPE_DIR, name);
}

static err_t
sfs_lookup(struct inode *dir, const char *name, struct inode **inode)
{
    inum_t inum;

    // Find the inode number associated with the search name
    if ((inum = search_dir(dir, name)) == 0) {
        return ERR_NOTEXIST;
    }
    // Search the inode cache to find the corresponding in-memory inode
    return fs_get_inode(dir->sb, inum, inode);
}

static err_t
sfs_fillpage(struct inode *inode, offset_t ofs, struct page *page)
{
    void *buf;

    kassert(inode);
    buf = (void*)kmap_p2v(page_to_paddr(page));
    if (read_data(inode, buf, pg_size, ofs) < pg_size) {
        return ERR_INCOMP;
    }
    return ERR_OK;
}

static err_t
sfs_link(struct inode *dir, struct inode *src, const char *name)
{
    struct blk_header *inode_bh;
    err_t err;

    kassert(src->i_ftype == FTYPE_FILE);

    // Check if name already exist
    if (search_dir(dir, name) != 0) {
        return ERR_EXIST;
    }
    // Acquire reference to the disk inode
    if ((inode_bh = ACQUIRE_INODE_BH(src)) == NULL) {
        return ERR_NOMEM;
    }
    // Create the hard link
    if ((err = alloc_dirent(dir, name, src->i_inum)) != ERR_OK) {
        bdev_release_blk_unlocked(inode_bh);
        return err;
    }
    // Increment the number of links on inode
    src->i_nlink++;
    fs_set_inode_dirty(src, True);
    // sfs_write_inode will not fail
    sfs_write_inode(src);
    bdev_release_blk_unlocked(inode_bh);
    return ERR_OK;
}

static err_t
sfs_unlink(struct inode *dir, const char *name)
{
    return unlink_inode_in_dir(dir, FTYPE_FILE, name);
}

static ssize_t
sfs_read(struct file *file, void *buf, size_t count, offset_t *ofs)
{
    ssize_t rs;

    sleeplock_acquire(&file->f_inode->i_lock);
    if ((rs = read_data(file->f_inode, buf, count, *ofs)) > 0) {
        *ofs += rs;
    }
    sleeplock_release(&file->f_inode->i_lock);
    return rs;
}

static ssize_t
sfs_write(struct file *file, const void *buf, size_t count, offset_t *ofs)
{
    ssize_t ws;

    sleeplock_acquire(&file->f_inode->i_lock);
    if ((ws = write_data(file->f_inode, buf, count, *ofs)) > 0) {
        *ofs += ws;
    }
    sleeplock_release(&file->f_inode->i_lock);
    return ws;
}

static err_t
sfs_readdir(struct file *dir, struct dirent *dirent)
{
    struct sfs_dirent sfs_dirent;
    ssize_t rs;

    sleeplock_acquire(&dir->f_inode->i_lock);
    if (dir->f_inode->i_size < dir->f_pos + sizeof(sfs_dirent)) {
        sleeplock_release(&dir->f_inode->i_lock);
        return ERR_END;
    }
    rs = read_data(dir->f_inode, &sfs_dirent, sizeof(sfs_dirent), dir->f_pos);
    if (rs < sizeof(sfs_dirent)) {
        sleeplock_release(&dir->f_inode->i_lock);
        return ERR_NOMEM;
    }
    kassert(rs == sizeof(sfs_dirent));
    dir->f_pos += rs;
    sleeplock_release(&dir->f_inode->i_lock);
    dirent->inode_num = sfs_dirent.inum;
    strcpy(dirent->name, sfs_dirent.name);
    return ERR_OK;
}

err_t
sfs_init(void)
{
    if ((sfs_sb_allocator = kmem_cache_create(sizeof(struct sfs_sb_info))) == NULL) {
        return ERR_INIT;
    }
    if ((sfs_inode_allocator = kmem_cache_create(sizeof(struct sfs_inode_info))) == NULL) {
        return ERR_INIT;
    }
    fs_register_fs(&sfs_fs_type);
    return ERR_OK;
}
