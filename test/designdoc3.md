# Lab 3: Pipes - Design Document

## Design Document

Authur: Dave Nguyen (Duy Nguyen)

## Overview

The goal of this assignment is to implement UNIX-based system call fuction `pipe`. This opens two file descriptors and maintains a connection between them through a BBQ (Block-based Bounded Queue) to enable inter-process communication.

## Major parts

_System calls:_ The syscall interface provides a barrier for the kernel to validate user program input.  This way, we can keep the I/O device state consistent.  No user program can directly affect the state of the kernel's data structures.  Furthermore, when we are already in the kernel code, we don't have to go through the syscall interface, which cuts down on superfluous error checking for trusted code.

_Pipes:_ A pipe is a sequential communication channel between two endpoints, supporting writes on one end, and reads on the other. Reads and writes are asynchronous and buffered, up to some internally defined size.

Pipes are a simple way to support interprocess communication, especially between processes started by `fork`.

_OS kernel:_ The OS kernel allocates and manages this buffer:

- A user process calling `read` on the read end of a pipe will block (e.g., it does not immediately return, like we saw with waiting on condition variables and acquiring locks) until there are bytes to read (read is allowed to do a partial read, meaning it returns having read fewer bytes than the user requested).

- A user process calling `write` on the write end of a pipe will block if there is no more room in the internal buffer.

## In-depth analysis and implementation

### Design Interface (Needed Data Structures)

_Pipe Structure:_ A structure to represent the pipe, including:

- A fixed-size buffer to store data: BBQ (Block-based Bounded Queue). A good size is `512` bytes

- `Read` and `write` pointers or indexes to manage the buffer

- Synchronization primitives such as `read-write` lock or conditional variables to ensure atomic read and write operations.

- Open/close status for both ends of the pipe to inform whether each end is open

_File Operation:_ This contains four function pointers, and is how we can control the behavior of `read`, `write`, `readdir`, and `close` for a particular file struct

This is the `struct file_operations`

```c
/*
 * File operations
 */
struct file_operations {
    ssize_t (*read)(struct file *file, void *buf, size_t count, offset_t *ofs);
    ssize_t (*write)(struct file *file, const void *buf, size_t count, offset_t *ofs);
    err_t (*readdir)(struct file *dir, struct dirent *dirent);
    void (*close)(struct file *file);
};
```

For example, `stdin` is a file struct that has specialized behavior. in `kernel/console.c`, we find:

```c
static struct file_operations stdin_ops = {
    .read = stdin_read,
};
```

In the `console_init` function that initializes `stdin` and `stdout`, we find:

```c
    stdin.oflag = FS_RDONLY;
    stdout.oflag = FS_WRONLY;
    stdin.f_ops = &stdin_ops;
    stdout.f_ops = &stdout_ops;
```

As mentioned aboved, a pipe is a file struct. We then add generic `void *info field` to the file struct that can be used to point to extra information that various kinds of files might need. 

When creating a pipe, then, you can use `fs_alloc_file` to allocate a file struct, and then initialize the info field. In `pipe_read`, `pipe_write`, and `pipe_close`, we can retrieve this information with

```c
struct pipe *p = (struct pipe *)file->info;
```

### Implementation

_Pipe Operations Implementation_

#### `pipe_read`: 

Manages reading data from the pipe. It blocks if the pipe is empty until data is written into the pipe.

#### `pipe_write`:

Manages writing data to the pipe. It blocks if the pipe is full until data is read from the pipe.

#### `pipe_close`

Closes a pipe end. If one end is closed, the system must handle reads and writes according to whether the pipe is still open at the other end.


#### `sys_pipe`:

Creates a pipe and two open file descriptors. 

The file descriptors are written to the array at `fds`, with `fds[0]` as the read end of the pipe and `fds[1]` as the write end of the pipe.

The fuction return:
 * `ERR_OK` on success
 * `ERR_FAULT` if fds address is invalid
 * `ERR_NOMEM` if 2 new file descriptors are not available


The pipe system call should create a pipe (a holding area for written data) and open two file descriptors, one for reading and one for writing. 

A `write` to a pipe with no open read descriptors should return an error. A read from a pipe with no open write descriptors should return any remaining buffered data, and `0` to indicate `EOF` (end of file) if no buffered data remains.

For memory management, you can use `kmalloc` and `kfree` or the `kmem_cache`-based approach used by the BBQ code.

This design will make changes in

- `include/kernel/fs.h`

- `kernel/syscall.c`

It also added new files:

- `include/kernel/pipe.h`

- `kernel/pipe.c`


## Risk analysis

* A `reader-writer` lock is probably a more complex solution than is necessary

* We need to make sure the implementation is thread-safe when  a process to call `pipe_read` while another calls `pipe_write`

### Staging of work

We plan to implement the work in this lab as follows:

1. `pipe.c` and `pipe.h`

2. `pipe_read`, `pipe_write` and `pipe_close`

3. `sys_pipe`

### Time estimation

1. `pipe.c` and `pipe.h`: 3 hours

2. `pipe_read`, `pipe_write` and `pipe_close`: 3 hours

3. `sys_pipe`: 3 hours

Total: 9 hours