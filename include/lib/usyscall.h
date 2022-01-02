#ifndef _USYSCALL_H_
#define _USYSCALL_H_

#include <arch/types.h>
#include <lib/errcode.h>

/*
 * Duplicate kernel data structures and definitions
 */
// File types
#define FTYPE_DIR 1
#define FTYPE_FILE 2
#define FTYPE_DEV 3

struct stat {
    int ftype;
    int inode_num;
    size_t size;
};

#define FNAME_LEN 28
struct dirent {
    char name[FNAME_LEN];
    int inode_num;
};

#define NUM_FILES 128

// Flags for syscall open
#define FS_RDONLY      0x000
#define FS_WRONLY      0x001
#define FS_RDWR        0x002
#define FS_CREAT       0x100
#define EMPTY_MODE	   0

// Virtual Memory
// need to change this based on the architecture 
#define KMAP_BASE           0xFFFFFFFF80000000
#define USTACK_UPPERBOUND   0xFFFFFF7FFFFFF000

struct sys_info {
    size_t num_pgfault;
};

/*
 * Syscalls
 */

/*
 * Create a child process.
 *
 * Return:
 * PID of child is returned in the parent process.
 * 0 is returned in the newly created child process.
 * ERR_NOMEM if failed to allocate memory
 */
int fork(void);
/*
 * Spawn a new process. The new process immediately executes a user program. 
 * The name of the program and its arguments are passed in as a space separated
 * string.
 *
 * Return:
 * PID of child - Process creation and program execution is successful.
 * ERR_FAULT - Address of args is invalid.
 * ERR_NOMEM - Failed to allocate memory.
 */
int spawn(const char *args);
/*
 * Wait for a process to change state. If pid is -1, wait for any child process.
 * If wstatus is not NULL, store the the exit status of the child in wstatus.
 *
 * Return:
 * PID of the child process that changes state.
 * ERR_FAULT - Address of wstatus is invalid.
 * ERR_CHILD - The caller does not have a child with the specified pid.
 */
int wait(int pid, int *wstatus);
/*
 * Terminate the calling process. The process will exit with the given status.
 * Should never return.
 */
void exit(int status) __attribute__((noreturn));
/*
 * Return the calling process' pid.
 */
int getpid(void);
/*
 * Cause the calling thread to sleep for the specified seconds.
 */
void sleep(unsigned int seconds);
/*
 * Open the file specified by pathname. Argument flags must include exactly one
 * of the following access modes:
 *   FS_RDONLY - Read-only mode
 *   FS_WRONLY - Write-only mode
 *   FS_RDWR - Read-write mode
 * flags can additionally include FS_CREAT. If FS_CREAT is included, a new file
 * is created with the specified permission (mode) if it does not exist yet.
 *
 * Return:
 * Non-negative file descriptor on success.
 * ERR_FAULT - Address of pathname is invalid.
 * ERR_INVAL - flags has invalid value.
 * ERR_NOTEXIST - File specified by pathname does not exist, and FS_CREAT is not
 *                specified in flags.
 * ERR_NOTEXIST - A directory component in pathname does not exist.
 * ERR_NORES - Failed to allocate inode in directory (FS_CREAT is specified)
 * ERR_FTYPE - A component used as a directory in pathname is not a directory.
 * ERR_NOMEM - Failed to allocate memory.
 */
int open(const char *pathname, int flags, int mode); // XXX mode should be fmode_t
/*
 * Close a file descriptor.
 *
 * Return:
 * ERR_OK - File successfully closed.
 * ERR_INVAL - fd isn't a valid open file descriptor.
 */
int close(int fd);
/*
 * Read from a file descriptor.
 *
 * Return:
 * On success, the number of bytes read (non-negative). The file position is
 * advanced by this number.
 * ERR_FAULT - Address of buf is invalid.
 * ERR_INVAL - fd isn't a valid open file descriptor.
 */
ssize_t read(int fd, void *buf, size_t count);
/*
 * Write to a file descriptor.
 *
 * Return:
 * On success, the number of bytes (non-negative) written. The file position is
 * advanced by this number.
 * ERR_FAULT - Address of buf is invalid;
 * ERR_INVAL - fd isn't a valid open file descriptor.
 * ERR_END - if fd refers to a pipe with no open read
 */
ssize_t write(int fd, const void *buf, size_t count);
/*
 * Create a hard link for a file.
 *
 * Return:
 * ERR_OK - Hard link successfully created.
 * ERR_FAULT - Address of oldpath/newpath is invalid.
 * ERR_EXIST - newpath already exists.
 * ERR_NOTEXIST - File specified by oldpath does not exist.
 * ERR_NOTEXIST - A directory component in oldpath/newpath does not exist.
 * ERR_FTYPE - A component used as a directory in oldpath/newpath is not a directory.
 * ERR_FTYPE - oldpath does not point to a file.
 * ERR_NORES - Failed to create hard link.
 * ERR_NOMEM - Failed to allocate memory.
 */
int link(const char *oldpath, const char *newpath);
/*
 * Remove a hard link.
 *
 * Return:
 * ERR_OK - Hard link successfully removed.
 * ERR_FAULT - Address of pathname is invalid.
 * ERR_NOTEXIST - File specified by pathname does not exist.
 * ERR_NOTEXIST - A directory component in pathname does not exist.
 * ERR_FTYPE - A component used as a directory in pathname is not a directory.
 * ERR_FTYPE - pathname does not point to a file.
 * ERR_NOMEM - Failed to allocate memory.
 */
int unlink(const char *pathname);
/*
 * Create a directory.
 *
 * Return:
 * ERR_OK - Directory successfully created.
 * ERR_FAULT - Address of pathname is invalid.
 * ERR_EXIST - pathname already exists.
 * ERR_NOTEXIST - A directory component in pathname does not exist.
 * ERR_FTYPE - A component used as a directory in pathname is not a directory.
 * ERR_NORES - Failed to create new directory.
 * ERR_NOMEM - Failed to allocate memory.
 */
int mkdir(const char *pathname);
/*
 * Change the current working directory.
 *
 * Return:
 * ERR_OK - Current working directory successfully changed.
 * ERR_FAULT - Address of path is invalid.
 * ERR_NOTEXIST - A directory component in path does not exist.
 * ERR_FTYPE - A component used as a directory in pathname is not a directory.
 * ERR_NOMEM - Failed to allocate memory.
 */
int chdir(const char *path);
/*
 * Read a directory entry.
 *
 * Return:
 * ERR_OK - A directory entry is successfully read into dirent.
 * ERR_FAULT - Address of dirent is invalid.
 * ERR_INVAL - fd isn't a valid open file descriptor.
 * ERR_FTYPE - fd does not point to a directory.
 * ERR_NOMEM - Failed to allocate memory.
 * ERR_END - End of the directory is reached.
 */
int readdir(int fd, struct dirent *dirent);
/*
 * Delete a directory.
 *
 * Return:
 * ERR_OK - Directory successfully deleted.
 * ERR_FAULT - Address of pathname is invalid.
 * ERR_NOTEXIST - Directory specified by pathname does not exist.
 * ERR_NOTEXIST - A directory component in pathname does not exist.
 * ERR_FTYPE - A component used as a directory in pathname is not a directory.
 * ERR_FTYPE - pathname does not point to a directory.
 * ERR_NOMEM - Failed to allocate memory.
 */
int rmdir(const char *pathname);
/*
 * Get file status.
 *
 * Return:
 * ERR_OK - File status is written in stat.
 * ERR_FAULT - Address of stat is invalid.
 * ERR_INVAL - fd isn't a valid open file descriptor.
 */
int fstat(int fd, struct stat *stat);
/*
 * Increase/decrement current process' data segment size.
 *
 * Return:
 * On success, address of the previous program break (non-negative).
 * ERR_NOMEM - Failed to extend the data segment.
 */
void *sbrk(int increment);
/*
 * Print information about the current process's address space.
 */
void meminfo();
/*
 * Duplicate the file descriptor fd, must use the smallest unused file descriptor.
 *
 * Return:
 * Non-negative file descriptor on success
 * ERR_INVAL if fd is invalid
 * ERR_NOMEM if no available new file descriptor
 */
int dup(int fd);
/*
 * Creates a pipe and two open file descriptors. The file descriptors
 * are written to the array at arg0, with arg0[0] the read end of the 
 * pipe and arg0[1] as the write end of the pipe.
 * 
 * Return:
 * ERR_OK on success
 * ERR_INVAL if fds address is invalid
 * ERR_NOMEM if no 2 available new file descriptors
 */
int pipe(int* fds);
/*
 * Fill in sysinfo struct
 */
void info(struct sys_info *info);
/*
 * Halt the computer
 */
void halt();
#endif /* _USYSCALL_H_ */
