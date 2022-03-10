#include <kernel/fs.h>
#include <kernel/sfs.h>
#include <kernel/synch.h>
#include <kernel/kmalloc.h>
#include <kernel/console.h>
#include <kernel/filems.h>
#include <kernel/proc.h>
#include <kernel/jbd.h>
#include <lib/errcode.h>
#include <lib/stddef.h>
#include <lib/string.h>
#include <lib/bits.h>

#define MAX_FILENAME_LEN 64

/*
 * superblock state flags
 */
#define SB_DIRTY 0

/*
 * inode state flags
 */
#define INODE_VALID 0
#define INODE_DIRTY 1

// File system type list
static List fs_type_list;
static struct spinlock fs_type_lock;

// Super block allocator
static struct kmem_cache *fs_sb_allocator;
// Inode allocator
static struct kmem_cache *fs_inode_allocator;
// File allocator
static struct kmem_cache *fs_file_allocator;

/*
 * We use a radix tree to keep track of all the superblocks. Use the block
 * device's device number as index.
 */
static struct radix_tree_root fs_sb_table;
// Use sleeplock because we need to read the superblock from disk
static struct sleeplock fs_sb_table_lock;

/*
 * Structures used by the cleanup thread.
 */
static List cleanup_thread_inodes;
struct spinlock cleanup_thread_lock;
struct condvar cleanup_thread_cv;

/*
 * Copy the next path element into buffer ``name``. Return the string that
 * follows the copied element. The returned string does not have the leading
 * '/'. If no path element is found, return NULL.
 */
static const char *path_next_element(const char *path, char *name);

/*
 * Look up the parent inode and the leaf name for a path name. Argument leaf
 * needs to be pre-allocated with at least MAX_FILENAME_LEN bytes. The caller is
 * responsible for releasing the inode after use.
 *
 * Return:
 * ERR_OK - Parent inode found and written to pointer parent. The caller is
 *          responsible for releasing the inode after use. The leaf name of the
 *          path name is written to leaf.
 * ERR_NOTEXIST - A directory component in pathname does not exist.
 * ERR_FTYPE - A component used as a directory in pathname is not a directory.
 * ERR_NOMEM - Failed to allocate memory.
 */
static err_t fs_find_parent_inode(const char *path, struct inode **parent, char *leaf);

/*
 * Give an inode to the cleanup thread.
 */
static void fs_push_inode_cleanup(struct inode *inode);

/* Validate open flag */
static bool validate_flag(int flags);

static bool
validate_flag(int flags)
{
    // mask out creat first
    flags &= ~FS_CREAT;
    switch(flags) {
        case FS_RDONLY:
        case FS_WRONLY:
        case FS_RDWR:
            return True;
        default:
            return False;
    }
}

static const char*
path_next_element(const char *path, char *name)
{
    const char *s;
    size_t len;

    // Skip leading '/'s
    while (*path == '/') {
        path++;
    }
    // Empty element
    if (*path == 0) {
        *name = 0;
        return path;
    }
    // Copy element to name
    s = path;
    while (*path != '/' && *path != 0) {
        path++;
    }
    len = min(path - s, MAX_FILENAME_LEN-1);
    memmove(name, s, len);
    name[len] = 0;
    // Skip next set of consecutive '/'s
    while (*path == '/') {
        path++;
    }
    return path;
}

static err_t
fs_find_parent_inode(const char *path, struct inode **parent, char *name)
{
    inum_t inum;
    err_t err;
    struct inode *curr, *next;
    struct proc* p;
    struct super_block *sb;

    // default look up starts from root
    p = proc_current();
    inum = (*path !='/' && p && p->cwd) ? p->cwd->i_inum : root_sb->s_root_inum;
    sb = (*path !='/' && p && p->cwd) ? p->cwd->sb : root_sb;
    kassert(inum > 0);
    if ((err = fs_get_inode(sb, inum, &curr)) != ERR_OK) {
        return err;
    }

    // Iteratively search each element of the path, from root or the current
    // directory
    while (True) {
        sleeplock_acquire(&curr->i_lock);
        if (curr->i_ftype != FTYPE_DIR) {
            err = ERR_FTYPE;
            goto fail;
        }
        path = path_next_element(path, name);
        if (*path == 0) {
            // Leaf found
            *parent = curr;
            sleeplock_release(&curr->i_lock);
            return ERR_OK;
        }
        if ((err = curr->i_ops->lookup(curr, name, &next)) != ERR_OK) {
            goto fail;
        }
        sleeplock_release(&curr->i_lock);
        fs_release_inode(curr);
        curr = next;
    }

fail:
    sleeplock_release(&curr->i_lock);
    fs_release_inode(curr);
    return err;
}

static void
fs_push_inode_cleanup(struct inode *inode)
{
    spinlock_acquire(&cleanup_thread_lock);
    list_append(&cleanup_thread_inodes, &inode->node);
    condvar_signal(&cleanup_thread_cv);
    spinlock_release(&cleanup_thread_lock);
}

void
fs_init(void)
{
    struct fs_type *root_fs;

    // File system type list
    list_init(&fs_type_list);
    spinlock_init(&fs_type_lock);

    // Create object allocators
    if ((fs_sb_allocator = kmem_cache_create(sizeof(struct super_block))) == NULL) {
        panic("Failed to create fs_sb_allocator");
    }
    if ((fs_inode_allocator = kmem_cache_create(sizeof(struct inode))) == NULL) {
        panic("Failed to create fs_inode_allocator");
    }
    if ((fs_file_allocator = kmem_cache_create(sizeof(struct file))) == NULL) {
        panic("Failed to create fs_file_allocator");
    }

    // Initialize superblock table
    radix_tree_construct(&fs_sb_table);
    sleeplock_init(&fs_sb_table_lock);

    // Initialize JBD
    jbd_init();

    // Root file system: currently use SFS
    if (sfs_init() != ERR_OK) {
        panic("Failed to initialize root file system");
    }
    if ((root_fs = fs_get_fs("sfs")) == NULL) {
        panic("Failed to find root file system");
    }
    kassert(root_bdev);
    if ((root_sb = fs_get_sb(root_bdev, root_fs)) == NULL) {
        panic("Failed to get root fs super block");
    }

    // Initialize cleanup thread
    list_init(&cleanup_thread_inodes);
    spinlock_init(&cleanup_thread_lock);
    condvar_init(&cleanup_thread_cv);

    // stdout
}

void
fs_register_fs(struct fs_type *fs)
{
    spinlock_acquire(&fs_type_lock);
    list_append(&fs_type_list, &fs->node);
    spinlock_release(&fs_type_lock);
}

struct fs_type*
fs_get_fs(const char *name)
{
    Node *n;
    struct fs_type *fs;

    spinlock_acquire(&fs_type_lock);
    for (n = list_begin(&fs_type_list); n != list_end(&fs_type_list); n = list_next(n)) {
        fs = list_entry(n, struct fs_type, node);
        if (strncmp(fs->fs_name, name, FS_TYPE_NAMELEN) == 0) {
            spinlock_release(&fs_type_lock);
            return fs;
        }
    }
    spinlock_release(&fs_type_lock);
    return NULL;
}

inline int
fs_is_sb_dirty(struct super_block *sb)
{
    return get_state_bit(sb->s_state, SB_DIRTY);
}

inline void
fs_set_sb_dirty(struct super_block *sb, int dirty)
{
    sb->s_state = set_state_bit(sb->s_state, SB_DIRTY, dirty);
}

struct super_block*
fs_alloc_sb(struct bdev *bdev, struct fs_type *fs_type)
{
    struct super_block *sb;

    if ((sb = kmem_cache_alloc(fs_sb_allocator)) != NULL) {
        memset(sb, 0, sizeof(struct super_block));
        sb->bdev = bdev;
        sb->s_fs_type = fs_type;
        radix_tree_construct(&sb->s_icache);
        sb->s_ref = 1;
        fs_set_sb_dirty(sb, False);
        sleeplock_init(&sb->s_lock);
    }
    return sb;
}

void
fs_free_sb(struct super_block *sb)
{
    kassert(radix_tree_empty(&sb->s_icache));
    kassert(sb->s_ref == 0);
    kassert(!fs_is_sb_dirty(sb));

    radix_tree_destroy(&sb->s_icache);
    kmem_cache_free(fs_sb_allocator, sb);
}

struct super_block*
fs_get_sb(struct bdev *bdev, struct fs_type *fs_type)
{
    struct super_block *sb;
    sleeplock_acquire(&fs_sb_table_lock);
    if ((sb = radix_tree_lookup(&fs_sb_table, bdev->dev)) == NULL) {
        if ((sb = fs_type->get_sb(bdev, fs_type)) == NULL) {
            sleeplock_release(&fs_sb_table_lock);
            return NULL;
        }
        if (radix_tree_insert(&fs_sb_table, bdev->dev, sb) != ERR_OK) {
            fs_type->free_sb(sb);
            sleeplock_release(&fs_sb_table_lock);
            return NULL;
        }
        bdev->sb = sb;
    } else {
        sb->s_ref++;
    }
    sleeplock_release(&fs_sb_table_lock);
    return sb;
}

void
fs_release_sb(struct super_block *sb)
{
    sleeplock_acquire(&fs_sb_table_lock);
    if (--sb->s_ref == 0) {
        kassert(radix_tree_remove(&fs_sb_table, sb->bdev->dev) == sb);
        sb->s_fs_type->free_sb(sb);
    }
    sleeplock_release(&fs_sb_table_lock);
}

inline int
fs_is_inode_valid(struct inode *inode)
{
    return get_state_bit(inode->i_state, INODE_VALID);
}

inline void
fs_set_inode_valid(struct inode *inode, int valid)
{
    inode->i_state = set_state_bit(inode->i_state, INODE_VALID, valid);
}

inline int
fs_is_inode_dirty(struct inode *inode)
{
    return get_state_bit(inode->i_state, INODE_DIRTY);
}

inline void
fs_set_inode_dirty(struct inode *inode, int dirty)
{
    inode->i_state = set_state_bit(inode->i_state, INODE_DIRTY, dirty);
}

struct inode*
fs_alloc_inode(struct super_block *sb)
{
    struct inode *inode;

    if ((inode = kmem_cache_alloc(fs_inode_allocator)) != NULL) {
        // Initialize inode fields
        memset(inode, 0, sizeof(struct inode));
        inode->sb = sb;
        inode->i_ref = 1;
        // Initial state of inode is: not valid, not dirty
        fs_set_inode_valid(inode, False);
        fs_set_inode_dirty(inode, False);
        sleeplock_init(&inode->i_lock);
        if ((inode->store = filems_alloc(inode)) == NULL) {
            kmem_cache_free(fs_inode_allocator, inode);
            inode = NULL;
        }
    }
    return inode;
}

void
fs_free_inode(struct inode *inode)
{
    kassert(inode->i_ref == 0);
    filems_free(inode->store);
    kmem_cache_free(fs_inode_allocator, inode);
}

err_t
fs_get_inode(struct super_block *sb, inum_t inum, struct inode **inode)
{
    err_t err;
    struct inode *res;

    sleeplock_acquire(&sb->s_lock);
    // Search for the inode in icache
    if ((res = radix_tree_lookup(&sb->s_icache, inum)) == NULL) {
        // inode not found: allocate a new inode and insert into icache
        if ((res = sb->s_ops->alloc_inode(sb)) == NULL) {
            sleeplock_release(&sb->s_lock);
            return ERR_NOMEM;
        }
        res->i_inum = inum;
        if ((err = radix_tree_insert(&sb->s_icache, inum, res)) != ERR_OK) {
            switch (err) {
                case ERR_RADIX_TREE_ALLOC:
                    sb->s_ops->free_inode(res);
                    sleeplock_release(&sb->s_lock);
                    return ERR_NOMEM;
                case ERR_RADIX_TREE_NODE_EXIST:
                    panic("node should not exist");
                default:
                    panic("unexpected error code");
            }
        }
    } else {
        // inode exists in cache -- just increment its reference counter
        res->i_ref++;
    }
    sleeplock_release(&sb->s_lock);

    // If inode is not valid, read from the corresponding on-disk inode
    sleeplock_acquire(&res->i_lock);
    if (!fs_is_inode_valid(res)) {
        if ((err = sb->s_ops->read_inode(res)) != ERR_OK) {
            sleeplock_release(&res->i_lock);
            fs_release_inode(res);
            return err;
        }
    }
    sleeplock_release(&res->i_lock);
    *inode = res;
    return ERR_OK;
}

void
fs_release_inode(struct inode *inode)
{
    sleeplock_acquire(&inode->i_lock);
    sleeplock_acquire(&inode->sb->s_lock);

    kassert(inode->i_inum > 0);
    kassert(inode->i_ref > 0);

    inode->i_ref--;

    if (inode->i_ref == 0) {
        if (fs_is_inode_valid(inode) &&
            (inode->i_nlink == 0 || fs_is_inode_dirty(inode))) {
            // Hand the inode over to a kernel thread who is responsible for
            // writing dirty inodes to disk or deleting inodes with zero links.

            // The kernel thread now holds a reference to the dirty inode
            inode->i_ref++;
            fs_push_inode_cleanup(inode);
            goto done;
        }

        kassert(radix_tree_remove(&inode->sb->s_icache, inode->i_inum) == inode);
        sleeplock_release(&inode->sb->s_lock);
        inode->sb->s_ops->free_inode(inode);
        return;
    }

done:
    sleeplock_release(&inode->sb->s_lock);
    sleeplock_release(&inode->i_lock);
}

err_t
fs_find_inode(const char *path, struct inode **inode)
{
    struct inode *parent;
    char name[MAX_FILENAME_LEN];
    err_t err;

    if ((err = fs_find_parent_inode(path, &parent, name)) != ERR_OK) {
        return err;
    }

    if (*name == 0) {
        // opening '/'. The root directory doesn't have a parent -- parent now
        // points to '/'
        *inode = parent;
        err = ERR_OK;
    } else {
        err = parent->i_ops->lookup(parent, name, inode);
        fs_release_inode(parent);
    }
    return err;
}

void
fs_inode_cleanup_thread(void)
{
    struct inode *inode;

    while (True) {
        // Pop the first inode from the list
        spinlock_acquire(&cleanup_thread_lock);
        while (list_empty(&cleanup_thread_inodes)) {
            condvar_wait(&cleanup_thread_cv, &cleanup_thread_lock);
        }
        inode = list_entry(list_begin(&cleanup_thread_inodes), struct inode, node);
        kassert(inode);
        list_remove(list_begin(&cleanup_thread_inodes));
        spinlock_release(&cleanup_thread_lock);

        // Either delete the inode if it has zero links, or write the dirty
        // inode to disk.
        inode->sb->s_ops->journal_begin_txn(inode->sb);
        sleeplock_acquire(&inode->i_lock);
        if (inode->i_nlink == 0) {
            while (inode->sb->s_ops->delete_inode(inode) != ERR_OK) {
                // XXX Just retry or check error and decide appropriate action?
                ;
            }
        } else {
            kassert(fs_is_inode_dirty(inode));
            while (inode->sb->s_ops->write_inode(inode) != ERR_OK) {
                // XXX Just retry or check error and decide appropriate action?
                ;
            }
        }
        sleeplock_release(&inode->i_lock);
        inode->sb->s_ops->journal_end_txn(inode->sb);
        fs_release_inode(inode);
    }
}

err_t
fs_link(const char *oldpath, const char *newpath)
{
    char name[MAX_FILENAME_LEN];
    struct inode *dir, *src;
    struct super_block *sb;
    err_t err;

    if ((err = fs_find_inode(oldpath, &src)) != ERR_OK) {
        return err;
    }

    if (src->i_ftype != FTYPE_FILE) {
        fs_release_inode(src);
        return ERR_FTYPE;
    }

    if ((err = fs_find_parent_inode(newpath, &dir, name)) != ERR_OK) {
        fs_release_inode(src);
        return err;
    }

    if (*name == 0) {
        // Trying to create a link '\'
        fs_release_inode(dir);
        fs_release_inode(src);
        return ERR_EXIST;
    }

    sb = src->sb;
    sb->s_ops->journal_begin_txn(sb);

    sleeplock_acquire(&src->i_lock);
    sleeplock_acquire(&dir->i_lock);
    err = dir->i_ops->link(dir, src, name);
    sleeplock_release(&dir->i_lock);
    sleeplock_release(&src->i_lock);
    fs_release_inode(dir);
    fs_release_inode(src);

    sb->s_ops->journal_end_txn(sb);
    return err;
}

err_t
fs_unlink(const char *pathname)
{
    char name[MAX_FILENAME_LEN];
    struct inode *dir;
    struct super_block *sb;
    err_t err;

    if ((err = fs_find_parent_inode(pathname, &dir, name)) != ERR_OK) {
        return err;
    }

    if (*name == 0) {
        // Trying to unlink '\'
        fs_release_inode(dir);
        return ERR_FTYPE;
    }

    sb = dir->sb;
    sb->s_ops->journal_begin_txn(sb);

    sleeplock_acquire(&dir->i_lock);
    err = dir->i_ops->unlink(dir, name);
    sleeplock_release(&dir->i_lock);
    fs_release_inode(dir);

    sb->s_ops->journal_end_txn(sb);
    return err;
}

err_t
fs_mkdir(const char *pathname)
{
    char name[MAX_FILENAME_LEN];
    struct inode *dir;
    struct super_block *sb;
    err_t err;

    if ((err = fs_find_parent_inode(pathname, &dir, name)) != ERR_OK) {
        return err;
    }

    if (*name == 0) {
        // Trying to create '\'
        fs_release_inode(dir);
        return ERR_EXIST;
    }

    sb = dir->sb;
    sb->s_ops->journal_begin_txn(sb);

    sleeplock_acquire(&dir->i_lock);
    // Directories have read/execute permission
    err = dir->i_ops->mkdir(dir, name, FMODE_R | FMODE_X);
    sleeplock_release(&dir->i_lock);
    fs_release_inode(dir);

    sb->s_ops->journal_end_txn(sb);
    return err;
}

err_t
fs_rmdir(const char *pathname)
{
    char name[MAX_FILENAME_LEN];
    struct inode *dir;
    struct super_block *sb;
    err_t err;

    if ((err = fs_find_parent_inode(pathname, &dir, name)) != ERR_OK) {
        return err;
    }

    if (*name == 0) {
        // Trying to delete '\'
        fs_release_inode(dir);
        return ERR_NOTEMPTY;
    }

    sb = dir->sb;
    sb->s_ops->journal_begin_txn(sb);

    sleeplock_acquire(&dir->i_lock);
    err = dir->i_ops->rmdir(dir, name);
    sleeplock_release(&dir->i_lock);
    fs_release_inode(dir);

    sb->s_ops->journal_end_txn(sb);
    return err;
}

struct file*
fs_alloc_file(void)
{
    struct file *file;

    if ((file = kmem_cache_alloc(fs_file_allocator)) != NULL) {
        memset(file, 0, sizeof(struct file));
        sleeplock_init(&file->f_lock);
        file->f_ref = 1;
    }
    return file;
}

void
fs_free_file(struct file *file)
{
    kmem_cache_free(fs_file_allocator, file);
}

err_t
fs_open_file(const char *path, int flags, fmode_t mode, struct file **file)
{
    err_t err;
    char name[MAX_FILENAME_LEN];
    struct inode *parent, *fi;

    if (!validate_flag(flags)) {
        return ERR_INVAL;
    }

    if ((err = fs_find_parent_inode(path, &parent, name)) != ERR_OK) {
        return err;
    }

    parent->sb->s_ops->journal_begin_txn(parent->sb);

    if (*name == 0) {
        // opening '/'. The root directory doesn't have a parent -- parent now
        // points to '/', just return it.
        fi = parent;
    } else {
        sleeplock_acquire(&parent->i_lock);
        if ((err = parent->i_ops->lookup(parent, name, &fi)) != ERR_OK) {
            if (err != ERR_NOTEXIST) {
                goto fail;
            }
            // Leaf file does not exist yet. If FS_CREAT flag is specified, create
            // the leaf file.
            if ((flags & FS_CREAT) == 0) {
                goto fail;
            }
            if ((err = parent->i_ops->create(parent, name, mode)) != ERR_OK) {
                kassert(err != ERR_EXIST);
                goto fail;
            }
            if ((err = parent->i_ops->lookup(parent, name, &fi)) != ERR_OK) {
                kassert(err != ERR_NOTEXIST);
                goto fail;
            }
        }
        kassert(fi);
        sleeplock_release(&parent->i_lock);
    }

    // Allocate a new file object
    if ((*file = fs_alloc_file()) == NULL) {
        parent->sb->s_ops->journal_end_txn(parent->sb);
        fs_release_inode(fi);
        fs_release_inode(parent);
        return ERR_NOMEM;
    }
    (*file)->f_inode = fi;
    // i_fops is read-only, so no need to protect with i_lock
    (*file)->f_ops = fi->i_fops;
    (*file)->oflag = flags & ~FS_CREAT;
    parent->sb->s_ops->journal_end_txn(parent->sb);
    fs_release_inode(parent);
    return ERR_OK;

fail:
    sleeplock_release(&parent->i_lock);
    parent->sb->s_ops->journal_end_txn(parent->sb);
    fs_release_inode(parent);
    return err;
}

void
fs_reopen_file(struct file *file)
{
    sleeplock_acquire(&file->f_lock);
    file->f_ref++;
    sleeplock_release(&file->f_lock);
}

void
fs_close_file(struct file *file)
{
    sleeplock_acquire(&file->f_lock);
    file->f_ref--;
    if (file->f_ref > 0 || file == &stdin || file == &stdout) {
        sleeplock_release(&file->f_lock);
        return;
    }
    sleeplock_release(&file->f_lock);
    // guaranteed that we are the last reference to this file
    if (file->f_inode) {
        fs_release_inode(file->f_inode);
    }
    if (file->f_ops->close) {
        file->f_ops->close(file);
    }
    fs_free_file(file);
}

ssize_t
fs_read_file(struct file *file, void *buf, size_t count, offset_t *ofs)
{
    ssize_t rs = 0;
    if (file->oflag != FS_WRONLY) {
        rs = file->f_ops->read(file, buf, count, ofs);
    }
    return rs;
}

ssize_t
fs_write_file(struct file *file, const void *buf, size_t count, offset_t *ofs)
{
    struct super_block *sb;
    ssize_t ws = 0;

    if (file->f_inode) {
        sb = file->f_inode->sb;
        sb->s_ops->journal_begin_txn(sb);
    }
    if (file->oflag != FS_RDONLY) {
        ws = file->f_ops->write(file, buf, count, ofs);
    }
    if (file->f_inode) {
        sb->s_ops->journal_end_txn(sb);
    }
    return ws;
}

err_t
fs_readdir(struct file *dir, struct dirent *dirent)
{
    err_t err;

    // Not holding dir->f_inode->i_lock here. We don't allow modifying inode's
    // ftype, so should be okay.
    if (dir->f_inode->i_ftype != FTYPE_DIR) {
        return ERR_FTYPE;
    }
    err = dir->f_ops->readdir(dir, dirent);
    return err;
}

