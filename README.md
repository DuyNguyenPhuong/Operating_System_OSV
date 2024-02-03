# Multiprocessing - Design Document

## Design Document

Authur: Dave Nguyen

## Overview

The goal of this design is to implement system call fuction `fork`, `wait`, and `exit` in UNIX system

## Major parts

_System calls:_ The syscall interface provides a barrier for the kernel to validate user program input.  This way, we can keep the I/O device state consistent.  No user program can directly affect the state of the kernel's data structures.  Furthermore, when we are already in the kernel code, we don't have to go through the syscall interface, which cuts down on superfluous error checking for trusted code.

### Design Interface (Needed Data Structures)

- Process Table: There is a process table (ptable) which tracks all processes, and a lock (ptable_lock) to protect this process table. Both are defined in `kernel/proc.c`. 

If a process needs to find child processes, it can loop through the process table. The function `ptable_dump` (in `proc.c`) can be used for debugging; it iterates over the process table and prints every process’s name and pid. Feel free to modify this function to dump more information if you extend the process struct

* Exit Status Table: A table, which each rows is a process and it will have a columns Exit status (initially is `STATUS_ALIVE`). Because exit status provided to `exit` needs to be saved somewhere (even though the resources for the exiting process are being released), as its parent may ask for it in `wait`. So we will save it the Exit Status Table

Note that `STATUS_ALIVE` (`kernel/proc.h`) is a reserved exit status value that will not be used by any process, so you can safely use `STATUS_ALIVE` as an initial value for exit status.

This can be mereged in the `Process Table`

* Child to Parent Table/Dictionary: A table to keep track of the parent of the process to Access the parent of a any process in constant. If a child doesn't have a parent, its value will be `NULL`

* Parent to Child Dictionary: A table to keep track of the child of the process. Each Process will have a list of its child. If a process doesn't have any child, the list will be empty

### `sys_fork`:

Already implemented

* Call a function `proc_fork` in `kernel/proc.c`

### `proc_fork`:

* A new process needs to be created through `kernel/proc.c:proc_init`.

* The parent must copy its memory to the child via `kernel/mm/vm.c:as_copy_as`.

* All opened files must be duplicated in the new process (make sure to call `fs_reopen_file`). For example: calling read in the parent should advance the offset in both the parent and the child

* A new thread needs to be created to run the process, and a new process needs to be added to the process table (see the example in `proc_spawn`).

* The current thread (parent's process)'s trapframe needs to be duplicated in the new thread (child process).  (Recall that assigning one struct to another results in a copy.)

* The call to `fork()` has two different return values; the trapframe needs to be set up to return 0 in the child via `kernel/trap.c:tf_set_return`, while returning the child's pid in the parent.

* The child process should have its own address space, meaning: Any changes in the child’s memory after fork are not visible to the parent. And any changes in the parent’s memory after fork are not visible to the child.

### `sys_exit`:

* Terminate the calling process (e.g., halt it and reclaim its resources)

* The process will exit with the given status.

* Will call `proc_exit`

* Should never returns

### `proc_exit`:

* Exit a process with a status

* Exit status provided to exit needs to be saved somewhere (for the its parent to access it) so we will save it in a Exit Status Table.

* Check if the parent is exited or not. If not, call the parents, make sure the parents see the exit code, and free the child process (We want to make sure free this struct until the parent process has seen it (or exited))

If the parent is exited, free this struct

* Update the status exit to the `EXIT STATUS TABLE`

* The kernel will take care of cleaning up the process’s address space and its thread for you (`thread_exit`, `thread_cleanup`), but you still need to clean up the rest and communication your exit status back to the parent process

### `sys_wait`

* General: The parent may call `wait` to block until a child exits. Note that a parent can: create multiple child processes, and wait on a specific child or any child to exit, and get its exit status.

* Corresponds to int wait(int pid, int *wstatus)

* Suspend execution until a child process changes state (e.g., terminates)

* If `pid` is not -1, check if it is a child pid of the parent. If not, return `ERR_CHILD`

* If `pid` is -1, wait for any child process. 

* To wait for any child process, we use a do a loop through `EXIT STATUS TABLE`
until its child exit and update their status in the table

* Check if `wstatus` is Valid or not, if not return `ERR_FAULT`

* If `wstatus` is not `NULL`, store the exit status of the child in `wstatus`

* A parent can only wait for the same child once

* Will call `proc_wait`

* Return: On success, the PID of the child process that changed state.
On failure: `ERR_FAULT` - Address of `wstatus` is invalid. Or `ERR_CHILD` - The caller does not have a child with the specified pid.

### `proc_wait`

* Wait for a process to change state (e.g., terminate).

* If pid is `ANY_CHILD`, wait for any child process.

* If wstatus is not NULL, store the the exit status of the child in wstatus.

* Return: On success, the `pid` of the child process that changed state. On failure, `ERR_CHILD` - The caller does not have a child with the specified pid.

## Risk analysis

* Accessing memory after deallocation can result in reading garbage values or cause a kernel panic. In particular, be careful about accessing fields of a `struct proc` after `proc_free` might have been called on it

# Pipe Design Document

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