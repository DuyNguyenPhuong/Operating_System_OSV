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