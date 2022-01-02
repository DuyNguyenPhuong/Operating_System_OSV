#ifndef _FS_H_
#define _FS_H_

#include <kernel/types.h>
#include <kernel/synch.h>
#include <kernel/pmem.h>
#include <kernel/bdev.h>
#include <kernel/list.h>
#include <kernel/radix_tree.h>

/*
 * File system interface
 */

/*
 * File system types
 */
#define FS_TYPE_NAMELEN 16
struct fs_type {
    char fs_name[FS_TYPE_NAMELEN];
    Node node; // used by fs_type_list
    /*
     * Allocate a new in-memory superblock and construct it by reading from the
     * on-disk superblock.
     *
     * Return:
     * NULL - Failed to allocate memory.
     */
    struct super_block *(*get_sb)(struct bdev *bdev, struct fs_type *fs_type);
    /*
     * Deallocate a superblock.
     */
    void (*free_sb)(struct super_block *sb);
    /*
     * Update the corresponding on-disk superblock with the content of an
     * in-memory superblock.
     *
     * Precondition:
     * Caller must hold sb->s_lock.
     *
     * Postcondition:
     * sb is clean.
     *
     * Return:
     * ERR_NOMEM - Failed to allocate memory.
     */
    err_t (*write_sb)(struct super_block *sb);
};

/*
 * Initialize the file system.
 */
void fs_init(void);

/*
 * Register a file system type.
 */
void fs_register_fs(struct fs_type *fs);

/*
 * Search for a file system type.
 *
 * Return:
 * NULL - Failed to find the file system.
 */
struct fs_type *fs_get_fs(const char *name);

/*
 * In-memory VFS superblock structure
 */
struct super_block {
    struct bdev *bdev; // Device descriptor
    struct fs_type *s_fs_type; // File system type
    inum_t s_root_inum; // Inode number of the root inode
    struct radix_tree_root s_icache; // Inode cache lookup table
    unsigned int s_ref; // Reference counter.
    state_t s_state; // State of in-memory superblock.
    struct sleeplock s_lock; // Lock protecting superblock data structures.
    void *s_fs_info; // Filesystem specific superblock info
    struct super_operations *s_ops; // Superblock operations
};

/*
 * Superblock operations
 */
struct super_operations {
    /*
     * Map journal logical block number to physical block number.
     */
    blk_t (*journal_bmap)(struct super_block *sb, blk_t lb);
    /*
     * Start a journal transaction.
     */
    void (*journal_begin_txn)(struct super_block *sb);
    /*
     * End a journal transaction.
     */
    void (*journal_end_txn)(struct super_block *sb);
    /*
     * Allocate a new in-memory inode.
     *
     * Return:
     * NULL - Failed to allocate memory.
     */
    struct inode *(*alloc_inode)(struct super_block *sb);
    /*
     * Free an inode.
     */
    void (*free_inode)(struct inode *inode);
    /*
     * Construct an inode by reading from the corresponding on-disk inode.
     *
     * Precondition:
     * Caller must hold inode->i_lock.
     * inode must not be dirty.
     *
     * Postcondition:
     * inode is valid.
     *
     * Return:
     * ERR_NOMEM - Failed to allocate memory.
     */
    err_t (*read_inode)(struct inode *inode);
    /*
     * Update the corresponding on-disk inode with the content of an in-memory
     * inode.
     *
     * Precondition:
     * Caller must hold inode->i_lock.
     *
     * Postcondition:
     * inode is clean.
     *
     * Return:
     * ERR_NOMEM - Failed to allocate memory.
     */
    err_t (*write_inode)(struct inode *inode);
    /*
     * Delete all file data and metadata of an inode.
     *
     * Precondition:
     * Caller must hold inode->i_lock.
     *
     * Return:
     * ERR_NOMEM - Failed to allocate memory.
     */
    err_t (*delete_inode)(struct inode *inode);
};

/*
 * Helper functions that get/set state of a superblock.
 *
 * Precondition:
 * Caller must hold sb->s_lock.
 */
int fs_is_sb_dirty(struct super_block *sb);
void fs_set_sb_dirty(struct super_block *sb, int dirty);

/*
 * Allocate a new in-memory superblock object.
 *
 * Return:
 * NULL - Failed to allocate superblock.
 */
struct super_block *fs_alloc_sb(struct bdev *bdev, struct fs_type *fs_type);

/*
 * Free an in-memory superblock object.
 */
void fs_free_sb(struct super_block *sb);

/*
 * Get the in-memory superblock of a block device. If the superblock object does
 * not exist, this function will allocate and construct one.
 *
 * Return:
 * NULL - Failed to construct the superblock.
 */
struct super_block *fs_get_sb(struct bdev *bdev, struct fs_type *fs_type);

/*
 * Release a superblock reference.
 */
void fs_release_sb(struct super_block *sb);

// Root file system superblock
struct super_block *root_sb;

/*
 * In-memory VFS inode structure
 */

// File types
typedef enum {
    FTYPE_DIR  = 1,
    FTYPE_FILE = 2,
    FTYPE_DEV  = 3
} ftype_t;

// File mode flag bits
#define FMODE_R 4 // Read permission
#define FMODE_W 2 // Write permission
#define FMODE_X 1 // Execute permission

struct inode {
    inum_t i_inum; // Inode number
    struct super_block *sb; // Superblock
    unsigned int i_ref; // Reference counter. Note that reference counter is protected by the superblock's s_lock
    unsigned int i_nlink; // Number of links
    ftype_t i_ftype; // File type
    fmode_t i_mode; // File permission
    size_t i_size; // File length in bytes
    void *i_fs_info; // Filesystem specific inode info
    state_t i_state; // State of in-memory inode
    struct sleeplock i_lock; // Lock protecting inode data structures
    struct inode_operations *i_ops; // Inode operations
    struct file_operations *i_fops; // File operations for this inode
    struct memstore *store; // memstore to read pages from this inode
    Node node; // List of dirty inodes or inodes with zero links (used by the cleanup thread)
};

/*
 * Inode operations
 */
struct inode_operations {
    /*
     * Create a new file inode in dir.
     *
     * Precondition:
     * Caller must hold dir->i_lock.
     *
     * Return:
     * ERR_EXIST - Another file/dir already exist with the same name.
     * ERR_NOMEM - Failed to allocate memory.
     * ERR_NORES - Failed to allocate new inode in dir.
     */
    err_t (*create)(struct inode *dir, const char *name, fmode_t mode);
    /*
     * Create a new directory in dir.
     *
     * Precondition:
     * Caller must hold dir->i_lock.
     *
     * Return:
     * ERR_EXIST - Another file/dir already exist with the same name.
     * ERR_NOMEM - Failed to allocate memory.
     * ERR_NORES - Failed to allocate new inode in dir.
     */
    err_t (*mkdir)(struct inode *dir, const char *name, fmode_t mode);
    /*
     * Remove a directory in dir.
     *
     * Precondition:
     * Caller must hold dir->i_lock.
     *
     * Return:
     * ERR_NOMEM - Failed to allocate memory.
     * ERR_NOTEXIST - Directory does not exist in dir.
     * ERR_FTYPE - 'name' does not refer to a directory.
     * ERR_NOTEMPTY - Directory is not empty.
     */
    err_t (*rmdir)(struct inode *dir, const char *name);
    /*
     * Search directory dir for an inode with the file/directory name. Caller is
     * responsible for releasing the inode reference.
     *
     * Precondition:
     * Caller must hold dir->i_lock.
     *
     * Return:
     * ERR_OK - Inode is found and written to pointer inode.
     * ERR_NOTEXIST - No inode with the specified name exist in dir.
     * ERR_NOMEM - Failed to allocate memory.
     */
    err_t (*lookup)(struct inode *dir, const char *name, struct inode **inode);
    /*
     * Fill in memory page with data read from an inode.
     *
     * Precondition:
     * Caller must hold inode->i_lock.
     *
     * Return:
     * ERR_INCOMP - Failed to fill in the entire page.
     */
    err_t (*fillpage)(struct inode *inode, offset_t ofs, struct page *page);
    /*
     * Create a new hard link in directory dir that refers to inode src. The
     * new hard link has name ``name``.
     *
     * Precondition:
     * Caller must hold dir->i_lock.
     * Caller must hold src->i_lock.
     * src->i_ftype must be FTYPE_FILE.
     * dir and src are on the same file system.
     *
     * Return:
     * ERR_EXIST - Another file/dir already exist with the same name.
     * ERR_NOMEM - Failed to allocate memory.
     * ERR_NORES - Failed to create hard link.
     */
    err_t (*link)(struct inode *dir, struct inode *src, const char *name);
    /*
     * Remove a hard link in directory dir.
     *
     * Precondition:
     * Caller must hold dir->i_lock.
     *
     * Return:
     * ERR_NOTEXIST - Hard link does not exist in dir.
     * ERR_FTYPE - 'name' does no refer to a regular file (e.g. directory).
     * ERR_NOMEM - Failed to allocate memory.
     */
    err_t (*unlink)(struct inode *dir, const char *name);
};

/*
 * Helper functions that get/set state of an inode.
 *
 * Precondition:
 * Caller must hold inode->i_lock.
 */
int fs_is_inode_valid(struct inode *inode);
void fs_set_inode_valid(struct inode *inode, int valid);
int fs_is_inode_dirty(struct inode *inode);
void fs_set_inode_dirty(struct inode *inode, int dirty);

/*
 * Allocate a new inode object. The new inode will have ``sb`` as the
 * superblock, and its reference count is initialized to 1.
 *
 * Return:
 * NULL if failed to allocate.
 */
struct inode *fs_alloc_inode(struct super_block *sb);

/*
 * Free an inode object.
 */
void fs_free_inode(struct inode *inode);

/*
 * Search for an inode in the inode cache. If the inode is not present in the
 * cache, construct one by reading the corresponding on-disk inode and add
 * it to the cache. If successful, the function increases the reference count on
 * the inode (newly constructed inode will have reference count 1).
 *
 * Return:
 * ERR_OK - Operation is successful and inode is written to pointer inode.
 * ERR_NOMEM - Failed to allocate memory.
 */
err_t fs_get_inode(struct super_block *sb, inum_t inum, struct inode **inode);

/*
 * Release an inode reference. If the inode has no more references, perform the
 * following actions:
 *   1. Free the on-disk inode and all allocated disk blocks if the inode has no
 *   links.
 *   2. If the inode is dirty and it still has links, update the on-disk inode.
 *   3. Delete the in-memory inode.
 * Note that actions 1 and 2 are delegated to a separate kernel cleanup thread.
 */
void fs_release_inode(struct inode *inode);

/*
 * Look up an inode using a path name. The caller is responsible for releasing
 * the inode after use.
 *
 * Return:
 * ERR_OK - Inode found and written to pointer inode. The caller is responsible
 *          for releasing the inode after use.
 * ERR_NOTEXIST - A component in pathname does not exist.
 * ERR_FTYPE - A component used as a directory in pathname is not a directory.
 * ERR_NOMEM - Failed to allocate memory.
 */
err_t fs_find_inode(const char *path, struct inode **inode);

/*
 * Kernel thread function that writes dirty inodes to disk and deletes inodes
 * with zero links.
 */
void fs_inode_cleanup_thread(void);

/*
 * Create a hard link.
 *
 * Return:
 * ERR_OK - Hard link successfully created.
 * ERR_EXIST - newpath already exists.
 * ERR_NOTEXIST - File specified by oldpath does not exist.
 * ERR_NOTEXIST - A directory component in oldpath/newpath does not exist.
 * ERR_FTYPE - A component used as a directory in pathname is not a directory.
 * ERR_FTYPE - oldpath does not point to a regular file.
 * ERR_NORES - Failed to create hard link.
 * ERR_NOMEM - Failed to allocate memory.
 */
err_t fs_link(const char *oldpath, const char *newpath);

/*
 * Remove a hard link.
 *
 * Return:
 * ERR_OK - Hard link successfully removed.
 * ERR_NOTEXIST - File specified by pathname does not exist.
 * ERR_NOTEXIST - A directory component in pathname does not exist.
 * ERR_FTYPE - A component used as a directory in pathname is not a directory.
 * ERR_FTYPE - pathname does not point to a regular file.
 * ERR_NOMEM - Failed to allocate memory.
 */
err_t fs_unlink(const char *pathname);

/*
 * Create a directory.
 *
 * Return:
 * ERR_OK - Directory successfully created.
 * ERR_EXIST - pathname already exists.
 * ERR_NOTEXIST - A directory component in pathname does not exist.
 * ERR_FTYPE - A component used as a directory in pathname is not a directory.
 * ERR_NORES - Failed to create new directory.
 * ERR_NOMEM - Failed to allocate memory.
 */
err_t fs_mkdir(const char *pathname);

/*
 * Delete a directory.
 *
 * Return:
 * ERR_OK - Directory successfully deleted.
 * ERR_NOTEMPTY - Directory pointed by pathname is not empty.
 * ERR_NOTEXIST - Directory specified by pathname does not exist.
 * ERR_NOTEXIST - A directory component in pathname does not exist.
 * ERR_FTYPE - A component used as a directory in pathname is not a directory.
 * ERR_FTYPE - pathname does not point to a directory.
 * ERR_NOMEM - Failed to allocate memory.
 */
err_t fs_rmdir(const char *pathname);

/*
 * File structure
 */
struct file {
    int f_ref; // ref count for this file
    int oflag; // open flag
    struct inode *f_inode; // File inode
    offset_t f_pos; // Current file offset
    struct sleeplock f_lock; // Lock protecting file data structures
    struct file_operations *f_ops; // File operations
};

/*
 * File statistics.
 */
struct stat {
    int ftype;
    int inode_num;
    size_t size;
};

/*
 * Directory entry.
 */
#define FNAME_LEN 28
struct dirent {
    char name[FNAME_LEN];
    int inode_num;
};

/*
 * File operations
 */
struct file_operations {
    /*
     * Read count number of bytes at file offset *ofs into a buffer buf. Update
     * ofs with the new offset.
     *
     * Return:
     * The number of bytes read, or -1 if an error occurs. If -1 is returned, no
     * data is read from the file.
     */
    ssize_t (*read)(struct file *file, void *buf, size_t count, offset_t *ofs);
    /* Write count number of bytes from buffer buf to file offset *ofs. Update
     * ofs with the new offset.
     *
     * Return:
     * The number of bytes written, or -1 if an error occurs. If -1 is returned,
     * no data is written to the file.
     */
    ssize_t (*write)(struct file *file, const void *buf, size_t count, offset_t *ofs);
    /*
     * Read the next directory entry from directory file dir and write it into
     * dirent.
     *
     * Precondition:
     * dir->f_inode must have i_ftype FTYPE_DIR
     *
     * Return:
     * ERR_NOMEM - Failed to allocate memory.
     * ERR_END - End of directory is reached.
     */
    err_t (*readdir)(struct file *dir, struct dirent *dirent);
    /*
     * Close a file and do proper clean up. Optional depends on type of file.
     */
    void (*close)(struct file *file);
};

/*
 * Allocate a new file object.
 *
 * Return:
 * NULL if failed to allocate.
 */
struct file *fs_alloc_file(void);

/*
 * Free a file object.
 */
void fs_free_file(struct file *file);

// fs_open_file flags
#define FS_RDONLY    0x000
#define FS_WRONLY    0x001
#define FS_RDWR      0x002
#define FS_CREAT     0x100

/*
 * Open a file object associated with a pathname. Argument flags must include
 * exactly one of the following access modes:
 *   FS_RDONLY - Read-only mode
 *   FS_WRONLY - Write-only mode
 *   FS_RDWR - Read-write mode
 * flags can additionally include FS_CREAT. If FS_CREAT is included, a new file
 * is created with permission mode if it does not exist yet. If FS_CREAT is not
 * included, mode is ignored.
 *
 * Return:
 * ERR_OK - Operation is successful, and the resulting file object is written
 *          into the file pointer.
 * ERR_INVAL - flags has invalid value.
 * ERR_NOTEXIST - File specified by pathname does not exist, and FS_CREAT is not
 *                specified in flags.
 * ERR_NOTEXIST - A directory component in pathname does not exist.
 * ERR_NORES - Failed to allocate inode in directory (FS_CREAT is specified)
 * ERR_FTYPE - A component used as a directory in pathname is not a directory.
 * ERR_NOMEM - Failed to allocate memory.
 */
err_t fs_open_file(const char *pathname, int flags, fmode_t mode, struct file **file);

/*
 * Reopen an opened file, increment the reference count.
 */
void fs_reopen_file(struct file *file);

/*
 * Close a file and decrements the reference count.
 * On the last reference, release the file inode reference, and free the file object.
 */
void fs_close_file(struct file *file);

/*
 * Read count bytes from file f at offset *ofs into a buffer buf. Update ofs with
 * the new offset.
 *
 * Return:
 * The number of bytes read, or -1 if an error occurs. If -1 is returned, no
 * data is read from the file.
 */
ssize_t fs_read_file(struct file *file, void *buf, size_t count, offset_t *ofs);

/*
 * Write count bytes to file f at offset *ofs from a buffer buf. Update ofs with
 * the new offset.
 *
 * Return:
 * The number of bytes written, or -1 if an error occurs. If -1 is returned, no
 * data is written to the file.
 */
ssize_t fs_write_file(struct file *file, const void *buf, size_t count, offset_t *ofs);

/*
 * Read the next directory entry from dir and write it into dirent.
 *
 * Return:
 * ERR_FTYPE - dir is not a directory.
 * ERR_NOMEM - Failed to allocate memory.
 * ERR_END - End of directory is reached.
 */
err_t fs_readdir(struct file *dir, struct dirent *dirent);

#endif /* _FS_H_ */
