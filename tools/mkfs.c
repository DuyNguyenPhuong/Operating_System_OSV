#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <assert.h>
#include <libgen.h>

// Avoid type conflicts
#define size_t osv_size_t
#define ssize_t osv_ssize_t
#define pid_t osv_pid_t
#define dev_t osv_dev_t
#define stat osv_stat
#include <kernel/bdev.h>
#include <kernel/fs.h>
#include <kernel/sfs.h>

// File system parameters
#define SB_BLK 2
#define INODE_BMAP_BLK 3
#define INODE_TABLE_BLK 4
#define ROOT_INUM 1
#define FS_SIZE 131072 // 64MB
#define MAX_INODES 256
#define INODE_TABLE_NUM_BLKS ((MAX_INODES * sizeof(struct sfs_inode)) / BDEV_BLK_SIZE)
#define BMAP_START_BLK (INODE_TABLE_BLK + INODE_TABLE_NUM_BLKS)
#define BMAP_BLKS (FS_SIZE / (BDEV_BLK_SIZE * 8) + (FS_SIZE % (BDEV_BLK_SIZE * 8) > 0 ? 1 : 0))
#define JOURNAL_START_BLK (BMAP_START_BLK + BMAP_BLKS)
#define JOURNAL_BLKS 256
#define DATA_START_BLK (JOURNAL_START_BLK + JOURNAL_BLKS)
#define MAX_FILE_SIZE ((SFS_NDIRECT + SFS_NINDIRECT * (BDEV_BLK_SIZE / sizeof(uint32_t))) * BDEV_BLK_SIZE)

// File system image file descriptor
static int fsfd;

// Root inode number
static inum_t root_inum;

// Inode bitmap
static uint8_t inode_bmap[BDEV_BLK_SIZE];

// Write buffer content to a disk block
static void write_blk(int blk, void *buf);
// Read a disk block into buffer
static void read_blk(int blk, void *buf);
// Allocate a new inode and return the inode number
static inum_t alloc_inode(ftype_t i_ftype);
// Read an inode from disk
static void read_inode(inum_t inum, struct sfs_inode *inode);
// Write an inode to disk
static void write_inode(inum_t inum, struct sfs_inode *inode);
// Append data to an inode. Caller responsible for updating inode on disk.
static void inode_append(struct sfs_inode *inode, char *data, size_t size);
// Get the data block number. Caller responsible for updating inode on disk.
static blk_t get_data_block(struct sfs_inode *inode, off_t ofs);
// Allocate and return a new data block.
static blk_t alloc_data_block(void);
// Search a bitmap and return the index of the first free element. Caller
// responsible for updating bitmap block on disk.
static int bmap_alloc_element(uint8_t *bmap, size_t size);
// Convert inode number to sector number
static int inum_to_blk(inum_t inum);
// Convert inode number to byte offset within a sector
static off_t inum_to_ofs(inum_t inum);

// Return the minimum
#define min(a, b) ((a < b) ? a : b)

static void
write_blk(int blk, void *buf)
{
    if (lseek(fsfd, blk * BDEV_BLK_SIZE, SEEK_SET) != blk * BDEV_BLK_SIZE) {
        perror("lseek failed");
        exit(1);
    }
    if (write(fsfd, buf, BDEV_BLK_SIZE) != BDEV_BLK_SIZE) {
        perror("write failed");
        exit(1);
    }
}

static void
read_blk(int blk, void *buf)
{
    if (lseek(fsfd, blk * BDEV_BLK_SIZE, SEEK_SET) != blk * BDEV_BLK_SIZE) {
        perror("lseek failed");
        exit(1);
    }
    if (read(fsfd, buf, BDEV_BLK_SIZE) != BDEV_BLK_SIZE) {
        perror("read failed");
        exit(1);
    }
}

// Allocate a new inode and return the inode number
static inum_t
alloc_inode(ftype_t ftype)
{
    int index;
    inum_t inum;
    struct sfs_inode inode;

    // Allocate inode from inode bitmap
    if ((index = bmap_alloc_element(inode_bmap, MAX_INODES / 8)) < 0) {
        perror("Failed to allocate inode");
        exit(1);
    }
    write_blk(INODE_BMAP_BLK, inode_bmap);
    inum = index + 1;
    // Update disk inode
    memset(&inode, 0, sizeof(inode));
    inode.i_ftype = ftype;
    inode.i_nlink = 1;
    write_inode(inum, &inode);

    return inum;
}

static void
read_inode(inum_t inum, struct sfs_inode *inode)
{
    char buf[BDEV_BLK_SIZE];

    read_blk(inum_to_blk(inum), buf);
    memmove(inode, buf + inum_to_ofs(inum), sizeof(struct sfs_inode));
}

static void
write_inode(inum_t inum, struct sfs_inode *inode)
{
    char buf[BDEV_BLK_SIZE];

    read_blk(inum_to_blk(inum), buf);
    memmove(buf + inum_to_ofs(inum), inode, sizeof(struct sfs_inode));
    write_blk(inum_to_blk(inum), buf);
}

static void
inode_append(struct sfs_inode *inode, char *data, size_t size)
{
    char buf[BDEV_BLK_SIZE];
    blk_t blk;
    size_t total, s;

    // Append buffer data to inode
    for (total = 0; total < size; inode->i_size += s, data += s, total += s) {
        blk = get_data_block(inode, inode->i_size);
        read_blk(blk, buf);
        s = min(BDEV_BLK_SIZE - inode->i_size % BDEV_BLK_SIZE, size - total);
        memmove(buf + (inode->i_size % BDEV_BLK_SIZE), data, s);
        write_blk(blk, buf);
    }
}

static blk_t
get_data_block(struct sfs_inode *inode, off_t ofs)
{
    int blk_index, indir_blk_index, indir_blk_ofs;
    blk_t blk;
    char buf[BDEV_BLK_SIZE];

    if (ofs >= MAX_FILE_SIZE) {
        perror("File size exceeds limit");
        exit(1);
    }
    blk_index = ofs / BDEV_BLK_SIZE;
    if (blk_index < SFS_NDIRECT) {
        // Direct block
        blk = inode->i_addrs[blk_index];
        if (blk == 0) {
            blk = alloc_data_block();
            inode->i_addrs[blk_index] = blk;
        }
    } else {
        // Indirect block
        indir_blk_index = SFS_NDIRECT + (blk_index - SFS_NDIRECT) / (BDEV_BLK_SIZE / sizeof(uint32_t));
        if (inode->i_addrs[indir_blk_index] == 0) {
            // Indirect block doesn't exist yet
            inode->i_addrs[indir_blk_index] = alloc_data_block();
        }
        // Read indirect block
        read_blk(inode->i_addrs[indir_blk_index], buf);
        indir_blk_ofs = (blk_index - SFS_NDIRECT) % (BDEV_BLK_SIZE / sizeof(uint32_t));
        blk = ((blk_t*)buf)[indir_blk_ofs];
        if (blk == 0) {
            blk = alloc_data_block();
            ((blk_t*)buf)[indir_blk_ofs] = blk;
            write_blk(inode->i_addrs[indir_blk_index], buf);
        }
    }

    return blk;
}

static blk_t
alloc_data_block(void)
{
    uint8_t bmap[BDEV_BLK_SIZE];
    char buf[BDEV_BLK_SIZE];
    blk_t bmap_blk, blk;
    size_t num_blks;
    int index;

    for (bmap_blk = BMAP_START_BLK, num_blks = FS_SIZE; bmap_blk < JOURNAL_START_BLK; bmap_blk++, num_blks -= BDEV_BLK_SIZE * 8) {
        read_blk(bmap_blk, bmap);
        if ((index = bmap_alloc_element(bmap, min(BDEV_BLK_SIZE, num_blks / 8))) >= 0) {
            // Update on-disk bitmap
            write_blk(bmap_blk, bmap);
            blk = DATA_START_BLK + BDEV_BLK_SIZE * 8 * (bmap_blk - BMAP_START_BLK) + index;
            // Fill newly allocated block with zero
            memset(buf, 0, BDEV_BLK_SIZE);
            write_blk(blk, buf);
            return blk;
        }
    }
    perror("Failed to allocate data block");
    exit(1);
}

static int
bmap_alloc_element(uint8_t *bmap, size_t size)
{
    int byte, bit;
    uint8_t mask;

    for (byte = 0; byte < size; byte++) {
        for (bit = 0; bit < 8; bit++) {
            mask = 1 << bit;
            if ((bmap[byte] & mask) == 0) {
                // Found a free element, mark it in-use
                bmap[byte] |= mask;
                return byte * 8 + bit;
            }
        }
    }
    return -1;
}

static int
inum_to_blk(inum_t inum)
{
    return INODE_TABLE_BLK + (inum - 1) / (BDEV_BLK_SIZE / sizeof(struct sfs_inode));
}

static off_t
inum_to_ofs(inum_t inum)
{
    return ((inum - 1) * sizeof(struct sfs_inode)) % BDEV_BLK_SIZE;
}

int
main(int argc, char *argv[])
{
    struct sfs_sb sb;
    int blk, i, fd;
    inum_t inum;
    size_t sz;
    char buf[BDEV_BLK_SIZE];
    struct sfs_dirent dirent;
    struct sfs_inode root_inode, file_inode;

    if (argc < 2) {
        fprintf(stderr, "Usage: mkfs <output image file> <input binary files>\n");
        exit(1);
    }

    if ((fsfd = open(argv[1], O_RDWR|O_CREAT|O_TRUNC, 0666)) < 0) {
        fprintf(stderr, "Failed to open output file %s\n", argv[1]);
        exit(1);
    }

    // Write 0s to the entire disk image
    memset(buf, 0, BDEV_BLK_SIZE);
    for (blk = 0; blk < DATA_START_BLK + FS_SIZE; blk++) {
        write_blk(blk, buf);
    }

    sb.s_size = FS_SIZE;
    sb.s_num_inodes = MAX_INODES;
    sb.s_data_bmap_start = BMAP_START_BLK;
    sb.s_journal_start = JOURNAL_START_BLK;
    sb.s_data_start = DATA_START_BLK;

    // Write the super block
    memcpy(buf, &sb, sizeof(sb));
    write_blk(SB_BLK, buf);

    // Allocate first inode as root inode
    memset(inode_bmap, 0, BDEV_BLK_SIZE);
    root_inum = alloc_inode(FTYPE_DIR);
    assert(root_inum == ROOT_INUM);
    read_inode(root_inum, &root_inode);

    // Add input binary files to the root directory
    for (i = 2; i < argc; i++) {
        if ((fd = open(argv[i], O_RDONLY)) < 0) {
            fprintf(stderr, "Failed to open binary file %s\n", argv[i]);
            exit(1);
        }
        // Allocate an inode for the file, and add it to the root directory
        inum = alloc_inode(FTYPE_FILE);
        read_inode(inum, &file_inode);
        dirent.inum = inum;
        // get rid of previous directory path
        strncpy(dirent.name, basename(argv[i]), SFS_DIRENT_NAMELEN);
        dirent.name[SFS_DIRENT_NAMELEN-1] = 0;
        inode_append(&root_inode, (char*)&dirent, sizeof(dirent));
        // Write file content to file system image
        while ((sz = read(fd, buf, BDEV_BLK_SIZE)) > 0) {
            inode_append(&file_inode, buf, sz);
        }
        // Update file inode
        write_inode(inum, &file_inode);
        close(fd);
    }

    // Update on-disk root inode
    write_inode(root_inum, &root_inode);

    close(fsfd);
}
