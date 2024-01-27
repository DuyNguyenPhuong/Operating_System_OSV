# System Calls - Design Document

## Overview

The goal of this design is to implement an interface for users to interact with persistent media or with other I/O devices, without having to distinguish between their types.

### Major parts

_File interface:_ The file interface provides an abstraction for the user that doesn't depend on the type of file.  This will allow user applications to interact with different types of files without large changes in the code.  For example, the method for attaining bytes will be the same when reading input for a file or from `stdin`.

_System calls:_ The syscall interface provides a barrier for the kernel to validate user program input.  This way, we can keep the I/O device state consistent.  No user program can directly affect the state of the kernel's data structures.  Furthermore, when we are already in the kernel code, we don't have to go through the syscall interface, which cuts down on superfluous error checking for trusted code.

## In-depth analysis and implementation

### File interface

#### Bookkeeping

* `include/kernel/fs.h` provides a `struct file` that we can use to back each file descriptor.  

* `include/kernel/console.h` provides console `struct file`s for `stdin` and `stdout`.

#### Process view

Each process will have an array of open files (bounded by `PROC_MAX_FILE`) in the process struct (`struct proc`).  The file descriptor will be the respective index into the file table.  For example, `stdin` is typically file descriptor 0, so the corresponding file struct will be the first element.  A system call can use `proc_current()` to get a pointer to the process control block (i.e., the `struct proc`) for the currently running process.

### System calls

We need to parse arguments from the user and validate them (we never trust the user!).  There are a few useful functions provided by `osv`:

* `bool fetch_arg(void *arg, int n, sysarg_t *ret)`: Given `args`, fetches the `n`th argument and stores it at `*ret`.  Returns `true` if fetched successfully, or `false` if the `n`th argument is unavailable.

* `static bool validate_str(char *s)`: Given a string `s`, checks whether the whole string is within a valid memory region of the process.

* `static bool validate_ptr(void *ptr, size_t size)`: Given a buffer `ptr` of size `size`, checks whether the buffer is within a valid memory region of the process.

Because all of our system calls will be dealing with files, we think it will be useful to add a function that allocates a file descriptor, and another that validates a file descriptor:
* `static int alloc_fd(struct file *f)`: Given a pointer to a file, looks through the process's open file table to find an available file descriptor, and stores the pointer there.  Returns the chosen file descriptor.
* `static bool validate_fd(int fd)`: Given a file descriptor, checks that it is valid (i.e., that it is in the open file table for the current process).

The main goals of the `sys_*` functions are to do argument parsing+validation, and then call the associated `fs_*_file` functions:
* `sys_write` / `sys_read`:
  - Writes or reads a file.
  - Changes the `f_pos` of the respective file struct.
  - Return values are specified by [Assignment 1](../assignments/a1#sys_read) (also in `include/lib/usyscall.h`).
  
* `sys_open`:
  - Finds an open entry in the process's open file table and stores a pointer to the file opened by the file system.
  - If no open spot is available, this should be considered a failure to allocate memory.
  - Return values are specified by [Assignment 1](../assignments/a1#sys_open) (also in `include/lib/usyscall.h`).

* `sys_close`:
  - Closes a file.
  - Releases the file from the process and clears out its entry in the process's open file table.
  - Returns values specified by [Assignment 1](../assignments/a1#sys_close) (also in `include/lib/usyscall.h`).

* `sys_readdir`:
  - Reads a directory.
  - Return values are specified by [Assignment 1](../assignments/a1#sys_readdir) (also in `include/lib/usyscall.h`).

* `sys_dup`:
  - Finds an open entry in the process's open file table and stores a pointer to the same file struct as the descriptor being duplicated.
  - If no open spot is available, this should be considered a failure to allocate memory.
  - Needs to update the reference count of the file through `fs_reopen_file()`.
  - Return values are specified by [Assignment 1](../assignments/a1#sys_dup) (also in `include/lib/usyscall.h`).

* `sys_fstat`:
  - Retrieves statistics of a file from `struct file` and its `struct inode`.
  - Return values are specified by [Assignment 1](../assignments/a1#sys_fstat) (also in `include/lib/usyscall.h`).

#### File system

We will need to use several file system functions declared in `include/kernel/fs.h`.  These are:
* `fs_open_file`: opens a file
* `fs_reopen_file`: increment a file's reference count
* `fs_read_file`: performs a read operation on the given file
* `fs_write_file`: performs a write operation on the given file
* `fs_close_file`: closes an open file
* `fs_readdir`: reads a directory

## Risk analysis

### Unanswered questions

* What happens when two different processes try to update data on the same file?

* What happens when the user or the kernel has the maximum number of files open?