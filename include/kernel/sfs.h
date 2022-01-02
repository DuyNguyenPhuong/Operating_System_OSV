#ifndef _SFS_H_
#define _SFS_H_

#include <kernel/types.h>

/*
 * Simple File System
 */

/*
 * SFS disk layout:
 * [ boot block (1) | super block (1) | inode bitmap (1) | inode table blocks
 * (n) | data block bitmap (n) | journal (n) | data blocks (n) ]
 */

/*
 * SFS initialization.
 *
 * Return:
 * ERR_INIT - Initialization failed.
 */
err_t sfs_init(void);

/*
 * On-disk SFS superblock structure
 */
struct sfs_sb {
    uint32_t s_size; // Size of the file system image (blocks)
    uint32_t s_num_inodes; // Maximum number of inodes
    uint32_t s_data_bmap_start; // Block number of the first data bitmap block
    uint32_t s_journal_start; // Block number of the first journal block
    uint32_t s_data_start; // Block number of the first data block
};

/*
 * In-memory SFS superblock structure
 */
struct sfs_sb_info {
    size_t s_size; // Size of the file system image (blocks)
    size_t s_num_inodes; // Maximum number of inodes
    blk_t s_data_bmap_start; // Block number of the first data bitmap block
    blk_t s_journal_start; // Block number of the first journal block
    blk_t s_data_start; // Block number of the first data block
    struct journal *journal;
};

// Each inode has SFS_NDIRECT direct blocks and SFS_NINDIRECT indirect blocks
#define SFS_NDIRECT 12 // Max number of direct data blocks
#define SFS_NINDIRECT 2 // Max number of indirect data blocks

/*
 * On-disk SFS inode structure.
 */
struct sfs_inode {
    uint8_t i_ftype; // File type
    uint8_t i_mode; // File permission
    uint16_t i_nlink; // Number of links to inode
    uint32_t i_size; // Size of file in bytes
    uint32_t i_addrs[SFS_NDIRECT + SFS_NINDIRECT]; // Data block addresses
}; // BDEV_BLK_SIZE need to be a multiple of sizeof(sfs_inode)

/*
 * In-memory SFS inode structure
 */
struct sfs_inode_info {
    blk_t i_addrs[SFS_NDIRECT + SFS_NINDIRECT];
};

/*
 * On-disk SFS directory entry.
 */
#define SFS_DIRENT_NAMELEN 28
struct sfs_dirent {
    uint32_t inum; // inode number
    char name[SFS_DIRENT_NAMELEN]; // file/directory name
}; // BDEV_BLK_SIZE need to be a multiple of sizeof(sfs_dirent)

#endif /* _SFS_H_ */
