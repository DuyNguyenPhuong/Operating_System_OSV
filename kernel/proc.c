#include <kernel/console.h>
#include <kernel/proc.h>
#include <kernel/kmalloc.h>
#include <kernel/thread.h>
#include <kernel/list.h>
#include <kernel/fs.h>
#include <kernel/vpmap.h>
#include <arch/elf.h>
#include <arch/trap.h>
#include <arch/mmu.h>
#include <lib/errcode.h>
#include <lib/stddef.h>
#include <lib/string.h>

List ptable; // process table
struct spinlock ptable_lock;
struct spinlock pid_lock;
static int pid_allocator;
struct kmem_cache *proc_allocator;

/* go through process table */
static void ptable_dump(void);
/* helper function for loading process's binary into its address space */
static err_t proc_load(struct proc *p, char *path, vaddr_t *entry_point);
/* helper function to set up the stack */
static err_t stack_setup(struct proc *p, char **argv, vaddr_t *ret_stackptr);
/* tranlsates a kernel vaddr to a user stack address, assumes stack is a single page */
#define USTACK_ADDR(addr) (pg_ofs(addr) + USTACK_UPPERBOUND - pg_size);

static struct proc *
proc_alloc()
{
    struct proc *p = (struct proc *)kmem_cache_alloc(proc_allocator);
    if (p != NULL)
    {
        spinlock_acquire(&pid_lock);
        p->pid = pid_allocator++;
        spinlock_release(&pid_lock);
    }
    return p;
}

#pragma GCC diagnostic ignored "-Wunused-function"
static void
ptable_dump(void)
{
    kprintf("ptable dump:\n");
    spinlock_acquire(&ptable_lock);
    for (Node *n = list_begin(&ptable); n != list_end(&ptable); n = list_next(n))
    {
        struct proc *p = list_entry(n, struct proc, proc_node);
        kprintf("Process %s: pid %d\n", p->name, p->pid);
    }
    spinlock_release(&ptable_lock);
    kprintf("\n");
}

void proc_free(struct proc *p)
{
    kmem_cache_free(proc_allocator, p);
}

void proc_sys_init(void)
{
    list_init(&ptable);
    spinlock_init(&ptable_lock);
    spinlock_init(&pid_lock);
    proc_allocator = kmem_cache_create(sizeof(struct proc));
    kassert(proc_allocator);
}

/*
 * Allocate and initialize basic proc structure
 */
static struct proc *
proc_init(char *name)
{
    struct super_block *sb;
    inum_t inum;
    err_t err;

    struct proc *p = proc_alloc();
    if (p == NULL)
    {
        return NULL;
    }

    if (as_init(&p->as) != ERR_OK)
    {
        proc_free(p);
        return NULL;
    }

    size_t slen = strlen(name);
    slen = slen < PROC_NAME_LEN - 1 ? slen : PROC_NAME_LEN - 1;
    memcpy(p->name, name, slen);
    p->name[slen] = 0;

    list_init(&p->threads);

    // Initialize the file descriptors to NULL
    for (int i = 0; i < PROC_MAX_FILE; i++)
    {
        p->file_descriptors[i] = NULL;
    }

    // Initialize this
    p->file_descriptors[0] = &stdin;
    p->file_descriptors[1] = &stdout;

    p->curFd = 0;

    p->parent = NULL;

    list_init(&p->children);

    p->has_exited = False;

    p->exit_status = STATUS_ALIVE;

    // cwd for all processes are root for now
    sb = root_sb;
    inum = root_sb->s_root_inum;
    if ((err = fs_get_inode(sb, inum, &p->cwd)) != ERR_OK)
    {
        as_destroy(&p->as);
        proc_free(p);
        return NULL;
    }
    return p;
}

err_t proc_spawn(char *name, char **argv, struct proc **p)
{
    err_t err;
    struct proc *proc;
    struct thread *t;
    vaddr_t entry_point;
    vaddr_t stackptr;

    if ((proc = proc_init(name)) == NULL)
    {
        return ERR_NOMEM;
    }
    // load binary of the process
    if ((err = proc_load(proc, name, &entry_point)) != ERR_OK)
    {
        goto error;
    }

    // set up stack and allocate its memregion
    if ((err = stack_setup(proc, argv, &stackptr)) != ERR_OK)
    {
        goto error;
    }

    if ((t = thread_create(proc->name, proc, DEFAULT_PRI)) == NULL)
    {
        err = ERR_NOMEM;
        goto error;
    }

    // add to ptable
    spinlock_acquire(&ptable_lock);
    list_append(&ptable, &proc->proc_node);
    spinlock_release(&ptable_lock);

    // Add parent-child relationship
    if (p != &init_proc)
    {
        struct proc *parent = proc_current();
        // kprintf("Hi1");
        kassert(parent);
        // kprintf("Hi2");
        proc->parent = parent;
        // list_append(&parent->children, &proc->proc_node);
    }

    // set up trapframe for a new process
    tf_proc(t->tf, t->proc, entry_point, stackptr);
    thread_start_context(t, NULL, NULL);

    // fill in allocated proc
    if (p)
    {
        *p = proc;
    }
    return ERR_OK;
error:
    as_destroy(&proc->as);
    proc_free(proc);
    return err;
}

/*
Helper function: Duplicate a process
// Duplicate the memory space
// pid_t pid;
// char name[PROC_NAME_LEN]; = parent name
// struct addrspace as; Use this: as_copy_as
// struct inode *cwd; // current working directory // same as parent
// List threads;      // list of threads belong to the process, right now just 1 per process
// Node proc_node;
// File descriptors
// struct file *file_descriptors[PROC_MAX_FILE]; // Array of file descriptors
// int curFd;
// For any that open in the parent --> call the fs_reopen_file
 */
struct proc *duplicate_process_state(struct proc *parent)
{
    kassert(parent);

    // Allocate new process structure for child
    struct proc *child = proc_init(parent->name);
    // Memory allocation failed of the child
    if (child == NULL)
    {
        return NULL;
    }

    // Duplicate the address space from parent to child
    if (as_copy_as(&parent->as, &child->as) != ERR_OK)
    {
        proc_free(child);
        return NULL;
    }

    child->cwd = parent->cwd;

    // Duplicate file descriptors from parent to child
    for (int i = 0; i < PROC_MAX_FILE; i++)
    {
        if (parent->file_descriptors[i] != NULL)
        {
            child->file_descriptors[i] = parent->file_descriptors[i];
            fs_reopen_file(child->file_descriptors[i]);
            if (child->file_descriptors[i] == NULL)
            {
                proc_free(child);
                return NULL;
            }
        }
        else
        {
            child->file_descriptors[i] = NULL;
        }
    }

    // Copy the current file descriptor index
    child->curFd = parent->curFd;

    return child;
}

struct proc *
proc_fork()
{
    // kassert(proc_current()); // caller of fork must be a process
    struct proc *parent = proc_current();
    // caller of fork must be a process
    kassert(parent);

    struct proc *child = duplicate_process_state(parent);

    if (child == NULL)
    {
        return NULL;
    }

    // Add the relationship

    child->parent = parent;
    // Dont use this
    // list_append(&parent->children, &child->proc_node);

    // Update child thread
    struct thread *t;
    // err_t err;
    if ((t = thread_create(child->name, child, DEFAULT_PRI)) == NULL)
    {
        // err = ERR_NOMEM;
        goto error;
        // return NULL;
    }
    *t->tf = *thread_current()->tf;
    tf_set_return(t->tf, 0);
    thread_start_context(t, NULL, NULL);

    // Add the child process to the process table
    spinlock_acquire(&ptable_lock);
    list_append(&ptable, &child->proc_node);
    spinlock_release(&ptable_lock);
    return child;

error:
    as_destroy(&child->as);
    proc_free(child);
    return NULL;
}

struct proc *
proc_current()
{
    return thread_current()->proc;
}

void proc_attach_thread(struct proc *p, struct thread *t)
{
    kassert(t);
    if (p)
    {
        list_append(&p->threads, &t->thread_node);
    }
}

bool proc_detach_thread(struct thread *t)
{
    bool last_thread = False;
    struct proc *p = t->proc;
    if (p)
    {
        list_remove(&t->thread_node);
        last_thread = list_empty(&p->threads);
    }
    return last_thread;
}

static void
ptable_dump_without_lock(void)
{
    kprintf("ptable dump:\n");
    for (Node *n = list_begin(&ptable); n != list_end(&ptable); n = list_next(n))
    {
        struct proc *p = list_entry(n, struct proc, proc_node);
        kprintf("Process %s: pid %d\n", p->name, p->pid);
    }
    kprintf("\n");
}

void close_all_fds(struct proc *process)
{
    if (process == NULL)
    {
        return; // Safety check
    }

    for (int i = 0; i < PROC_MAX_FILE; i++)
    {
        if (process->file_descriptors[i] != NULL)
        {
            // Close the file descriptor
            fs_close_file(process->file_descriptors[i]);
            process->file_descriptors[i] = NULL;
        }
    }
}

int proc_wait(pid_t pid, int *status)
{
    struct proc *current_proc = proc_current();
    kassert(current_proc);
    struct proc *child_proc = NULL;
    int found_child = False;
    while (True)
    {
        // if (current_proc->pid > 0)
        // {
        //     kprintf("[%d] In the while \n", current_proc->pid);
        // }

        found_child = False;
        spinlock_acquire(&ptable_lock);

        // Check all processes in the global process table
        for (Node *n = list_begin(&ptable); n != list_end(&ptable); n = list_next(n))
        {
            child_proc = list_entry(n, struct proc, proc_node);

            // Check if the process is a child of the current process
            if (child_proc->parent == current_proc && (pid == ANY_CHILD || child_proc->pid == pid))
            {
                found_child = True;
                if (child_proc->has_exited)
                {
                    // Child has exited, retrieve its exit status
                    if (status != NULL)
                    {
                        *status = child_proc->exit_status;
                    }
                    int child_pid = child_proc->pid;

                    // Clean up the child process's resources

                    list_remove(&child_proc->proc_node);
                    close_all_fds(child_proc);
                    proc_free(child_proc);
                    spinlock_release(&ptable_lock);
                    return child_pid;
                }
            }
        }
        spinlock_release(&ptable_lock);
        if (!found_child)
        {
            return ERR_CHILD; // No matching child process
        }
    }
}

void proc_exit(int status)
{
    struct thread *t = thread_current();
    struct proc *p = proc_current();

    // detach current thread, switch to kernel page table
    // free current address space if proc has no more threads
    // order matters here
    proc_detach_thread(t);
    t->proc = NULL;
    vpmap_load(kas->vpmap);
    as_destroy(&p->as);

    // release process's cwd
    fs_release_inode(p->cwd);

    // Notify the parent process
    if (p->parent != NULL)
    {
        // spinlock_acquire(&ptable_lock);
        p->exit_status = status;
        p->has_exited = True;
        // spinlock_release(&ptable_lock);
    }

    // Exit the current thread
    thread_exit(status);
}

/* helper function for loading process's binary into its address space */
static err_t
proc_load(struct proc *p, char *path, vaddr_t *entry_point)
{
    int i;
    err_t err;
    offset_t ofs = 0;
    struct elfhdr elf;
    struct proghdr ph;
    struct file *f;
    paddr_t paddr;
    vaddr_t vaddr;
    vaddr_t end = 0;

    if ((err = fs_open_file(path, FS_RDONLY, 0, &f)) != ERR_OK)
    {
        return err;
    }

    // check if the file is actually an executable file
    if (fs_read_file(f, (void *)&elf, sizeof(elf), &ofs) != sizeof(elf) || elf.magic != ELF_MAGIC)
    {
        return ERR_INVAL;
    }

    // read elf and load binary
    for (i = 0, ofs = elf.phoff; i < elf.phnum; i++)
    {
        if (fs_read_file(f, (void *)&ph, sizeof(ph), &ofs) != sizeof(ph))
        {
            return ERR_INVAL;
        }
        if (ph.type != PT_LOAD)
            continue;

        if (ph.memsz < ph.filesz || ph.vaddr + ph.memsz < ph.vaddr)
        {
            return ERR_INVAL;
        }

        memperm_t perm = MEMPERM_UR;
        if (ph.flags & PF_W)
        {
            perm = MEMPERM_URW;
        }

        // found loadable section, add as a memregion
        struct memregion *r = as_map_memregion(&p->as, pg_round_down(ph.vaddr),
                                               pg_round_up(ph.memsz + pg_ofs(ph.vaddr)), perm, NULL, ph.off, False);
        if (r == NULL)
        {
            return ERR_NOMEM;
        }
        end = r->end;

        // pre-page in code and data, may span over multiple pages
        int count = 0;
        size_t avail_bytes;
        size_t read_bytes = ph.filesz;
        size_t pages = pg_round_up(ph.memsz + pg_ofs(ph.vaddr)) / pg_size;
        // vaddr may start at a nonaligned address
        vaddr = pg_ofs(ph.vaddr);
        while (count < pages)
        {
            // allocate a physical page and zero it first
            if ((err = pmem_alloc(&paddr)) != ERR_OK)
            {
                return err;
            }
            vaddr += kmap_p2v(paddr);
            memset((void *)pg_round_down(vaddr), 0, pg_size);
            // calculate how many bytes to read from file
            avail_bytes = read_bytes < (pg_size - pg_ofs(vaddr)) ? read_bytes : (pg_size - pg_ofs(vaddr));
            if (avail_bytes && fs_read_file(f, (void *)vaddr, avail_bytes, &ph.off) != avail_bytes)
            {
                return ERR_INVAL;
            }
            // map physical page with code/data content to expected virtual address in the page table
            if ((err = vpmap_map(p->as.vpmap, ph.vaddr + count * pg_size, paddr, 1, perm)) != ERR_OK)
            {
                return err;
            }
            read_bytes -= avail_bytes;
            count++;
            vaddr = 0;
        }
    }
    *entry_point = elf.entry;

    // create memregion for heap after data segment
    if ((p->as.heap = as_map_memregion(&p->as, end, 0, MEMPERM_URW, NULL, 0, 0)) == NULL)
    {
        return ERR_NOMEM;
    }

    return ERR_OK;
}

err_t stack_setup(struct proc *p, char **argv, vaddr_t *ret_stackptr)
{
    err_t err;
    paddr_t paddr;
    vaddr_t stackptr;
    vaddr_t stacktop = USTACK_UPPERBOUND - pg_size;

    // allocate a page of physical memory for stack
    if ((err = pmem_alloc(&paddr)) != ERR_OK)
    {
        return err;
    }
    memset((void *)kmap_p2v(paddr), 0, pg_size);
    // create memregion for stack
    if (as_map_memregion(&p->as, stacktop, pg_size, MEMPERM_URW, NULL, 0, False) == NULL)
    {
        err = ERR_NOMEM;
        goto error;
    }
    // map in first stack page
    if ((err = vpmap_map(p->as.vpmap, stacktop, paddr, 1, MEMPERM_URW)) != ERR_OK)
    {
        goto error;
    }
    // kernel virtual address of the user stack, points to top of the stack
    // as you allocate things on stack, move stackptr downward.
    stackptr = kmap_p2v(paddr) + pg_size;

    /* Your Code Here.  */
    // allocate space for fake return address, argc, argv
    // remove following line when you actually set up the stack
    stackptr -= 3 * sizeof(void *);

    // translates stackptr from kernel virtual address to user stack address
    *ret_stackptr = USTACK_ADDR(stackptr);
    return err;
error:
    pmem_free(paddr);
    return err;
}
