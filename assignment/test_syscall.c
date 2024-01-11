#include <kernel/proc.h>
#include <kernel/thread.h>
#include <kernel/console.h>
#include <kernel/kmalloc.h>
#include <kernel/fs.h>
#include <lib/syscall-num.h>
#include <lib/errcode.h>
#include <lib/stddef.h>
#include <lib/string.h>
#include <arch/asm.h>

// syscall handlers
static sysret_t sys_fork(void *arg);
static sysret_t sys_spawn(void *arg);
static sysret_t sys_wait(void *arg);
static sysret_t sys_exit(void *arg);
static sysret_t sys_getpid(void *arg);
static sysret_t sys_sleep(void *arg);
static sysret_t sys_open(void *arg);
static sysret_t sys_close(void *arg);
static sysret_t sys_read(void *arg);
static sysret_t sys_write(void *arg);
static sysret_t sys_link(void *arg);
static sysret_t sys_unlink(void *arg);
static sysret_t sys_mkdir(void *arg);
static sysret_t sys_chdir(void *arg);
static sysret_t sys_readdir(void *arg);
static sysret_t sys_rmdir(void *arg);
static sysret_t sys_fstat(void *arg);
static sysret_t sys_sbrk(void *arg);
static sysret_t sys_meminfo(void *arg);
static sysret_t sys_dup(void *arg);
static sysret_t sys_pipe(void *arg);
static sysret_t sys_info(void *arg);
static sysret_t sys_halt(void *arg);

extern size_t user_pgfault;
struct sys_info
{
    size_t num_pgfault;
};

/*
 * Machine dependent syscall implementation: fetches the nth syscall argument.
 */
extern bool fetch_arg(void *arg, int n, sysarg_t *ret);

/*
 * Validate string passed by user.
 */
static bool validate_str(char *s);
/*
 * Validate buffer passed by user.
 */
static bool validate_ptr(void *ptr, size_t size);

static sysret_t (*syscalls[])(void *) = {
    [SYS_fork] = sys_fork,
    [SYS_spawn] = sys_spawn,
    [SYS_wait] = sys_wait,
    [SYS_exit] = sys_exit,
    [SYS_getpid] = sys_getpid,
    [SYS_sleep] = sys_sleep,
    [SYS_open] = sys_open,
    [SYS_close] = sys_close,
    [SYS_read] = sys_read,
    [SYS_write] = sys_write,
    [SYS_link] = sys_link,
    [SYS_unlink] = sys_unlink,
    [SYS_mkdir] = sys_mkdir,
    [SYS_chdir] = sys_chdir,
    [SYS_readdir] = sys_readdir,
    [SYS_rmdir] = sys_rmdir,
    [SYS_fstat] = sys_fstat,
    [SYS_sbrk] = sys_sbrk,
    [SYS_meminfo] = sys_meminfo,
    [SYS_dup] = sys_dup,
    [SYS_pipe] = sys_pipe,
    [SYS_info] = sys_info,
    [SYS_halt] = sys_halt,
};

static bool
validate_str(char *s)
{
    struct memregion *mr;
    // find given string's memory region
    if ((mr = as_find_memregion(&proc_current()->as, (vaddr_t)s, 1)) == NULL)
    {
        return False;
    }
    // check in case the string keeps growing past user specified amount
    for (; s < (char *)mr->end; s++)
    {
        if (*s == 0)
        {
            return True;
        }
    }
    return False;
}

static bool
validate_ptr(void *ptr, size_t size)
{
    vaddr_t ptraddr = (vaddr_t)ptr;
    if (ptraddr + size < ptraddr)
    {
        return False;
    }
    // verify argument ptr points to a valid chunk of memory of size bytes
    return as_find_memregion(&proc_current()->as, ptraddr, size) != NULL;
}

// int fork(void);
static sysret_t
sys_fork(void *arg)
{
    struct proc *p;
    if ((p = proc_fork()) == NULL)
    {
        return ERR_NOMEM;
    }
    return p->pid;
}

// int spawn(const char *args);
static sysret_t
sys_spawn(void *arg)
{
    int argc = 0;
    sysarg_t args;
    size_t len;
    char *token, *buf, **argv;
    struct proc *p;
    err_t err;

    // argument fetching and validating
    kassert(fetch_arg(arg, 1, &args));
    if (!validate_str((char *)args))
    {
        return ERR_FAULT;
    }

    len = strlen((char *)args) + 1;
    if ((buf = kmalloc(len)) == NULL)
    {
        return ERR_NOMEM;
    }
    // make a copy of the string to not modify user data
    memcpy(buf, (void *)args, len);
    // figure out max number of arguments possible
    len = len / 2 < PROC_MAX_ARG ? len / 2 : PROC_MAX_ARG;
    if ((argv = kmalloc((len + 1) * sizeof(char *))) == NULL)
    {
        kfree(buf);
        return ERR_NOMEM;
    }
    // parse arguments
    while ((token = strtok_r(NULL, " ", &buf)) != NULL)
    {
        argv[argc] = token;
        argc++;
    }
    argv[argc] = NULL;

    if ((err = proc_spawn(argv[0], argv, &p)) != ERR_OK)
    {
        return err;
    }
    return p->pid;
}

// int wait(int pid, int *wstatus);
static sysret_t
sys_wait(void *arg)
{
    /* remove when writing your own solution */
    for (;;)
    {
    }
    panic("unreacchable");
}

// void exit(int status);
static sysret_t
sys_exit(void *arg)
{
    // temp code for lab1 to terminate the kernel after one process exits
    // remove for lab 2
    kprintf("shutting down\n");
    shutdown();
    kprintf("oops still running\n");
    for (;;)
    {
    }
    panic("syscall exit not implemented");
}

// int getpid(void);
static sysret_t
sys_getpid(void *arg)
{
    return proc_current()->pid;
}

// void sleep(unsigned int, seconds);
static sysret_t
sys_sleep(void *arg)
{
    panic("syscall sleep not implemented");
}

// int open(const char *pathname, int flags, fmode_t mode);
static sysret_t
sys_open(void *arg)
{
    sysarg_t pathname_arg, flags_arg, mode_arg;
    kassert(fetch_arg(arg, 1, &pathname_arg));
    kassert(fetch_arg(arg, 3, &flags_arg));
    kassert(fetch_arg(arg, 2, &mode_arg));

    // Note on kassert:
    /*
    #define kassert(expr) \
        ((expr) ? (void)0 : panic(#expr))

    if (expr){
        (void) 0
    }
    else{
        panic(#expr)
    }
    */

    // Convert argument to proper tu[es]
    char *pathname = (char *)pathname_arg;
    int flags = (int)flags_arg;
    fmode_t mode = (fmode_t)mode_arg;

    // Validate the address of pathname
    if (!validate_str((char *)pathname))
    {
        return ERR_FAULT;
    }

    // Current Process. Look it from other code
    struct proc *p = proc_current();
    kassert(p);

    err_t res = fs_open_file(pathname, flags, mode, &(p->file2));

    if (res != ERR_OK)
    {
        return res;
    }
    // panic("syscall open not implemented");
    return (sysret_t)2;

    // SUCH A LIE -- return the fd we picked

    // Call fs_open_file; fs_close_file
}

// int close(int fd);
static sysret_t
sys_close(void *arg)
{
    panic("syscall close not implemented");
}

// int read(int fd, void *buf, size_t count);
static sysret_t
sys_read(void *arg)
{
    sysarg_t fd, buf, count;

    kassert(fetch_arg(arg, 1, &fd));
    kassert(fetch_arg(arg, 3, &count));
    kassert(fetch_arg(arg, 2, &buf));

    if (!validate_ptr((void *)buf, (size_t)count))
    {
        return ERR_FAULT;
    }

    if (fd == 0)
    {
        return console_read((void *)buf, (size_t)count);
    }
    return ERR_INVAL;
}

// int write(int fd, const void *buf, size_t count)
static sysret_t
sys_write(void *arg)
{
    sysarg_t fd, count, buf;

    kassert(fetch_arg(arg, 1, &fd));
    kassert(fetch_arg(arg, 3, &count));
    kassert(fetch_arg(arg, 2, &buf));

    if (!validate_ptr((void *)buf, (size_t)count))
    {
        return ERR_FAULT;
    }

    if (fd == 1)
    {
        // write some stuff for now assuming one string
        return console_write((void *)buf, (size_t)count);
    }
    return ERR_INVAL;
}

// int link(const char *oldpath, const char *newpath)
static sysret_t
sys_link(void *arg)
{
    sysarg_t oldpath, newpath;

    kassert(fetch_arg(arg, 1, &oldpath));
    kassert(fetch_arg(arg, 2, &newpath));

    if (!validate_str((char *)oldpath) || !validate_str((char *)newpath))
    {
        return ERR_FAULT;
    }

    return fs_link((char *)oldpath, (char *)newpath);
}

// int unlink(const char *pathname)
static sysret_t
sys_unlink(void *arg)
{
    sysarg_t pathname;

    kassert(fetch_arg(arg, 1, &pathname));

    if (!validate_str((char *)pathname))
    {
        return ERR_FAULT;
    }

    return fs_unlink((char *)pathname);
}

// int mkdir(const char *pathname)
static sysret_t
sys_mkdir(void *arg)
{
    sysarg_t pathname;

    kassert(fetch_arg(arg, 1, &pathname));

    if (!validate_str((char *)pathname))
    {
        return ERR_FAULT;
    }

    return fs_mkdir((char *)pathname);
}

// int chdir(const char *path)
static sysret_t
sys_chdir(void *arg)
{
    sysarg_t path;
    struct inode *inode;
    struct proc *p;
    err_t err;

    kassert(fetch_arg(arg, 1, &path));

    if (!validate_str((char *)path))
    {
        return ERR_FAULT;
    }

    if ((err = fs_find_inode((char *)path, &inode)) != ERR_OK)
    {
        return err;
    }

    p = proc_current();
    kassert(p);
    kassert(p->cwd);
    fs_release_inode(p->cwd);
    p->cwd = inode;
    return ERR_OK;
}

// int readdir(int fd, struct dirent *dirent);
static sysret_t
sys_readdir(void *arg)
{
    panic("syscall readdir not implemented");
}

// int rmdir(const char *pathname);
static sysret_t
sys_rmdir(void *arg)
{
    sysarg_t pathname;

    kassert(fetch_arg(arg, 1, &pathname));

    if (!validate_str((char *)pathname))
    {
        return ERR_FAULT;
    }

    return fs_rmdir((char *)pathname);
}

// int fstat(int fd, struct stat *stat);
static sysret_t
sys_fstat(void *arg)
{
    panic("syscall fstat not implemented");
}

// void *sbrk(size_t increment);
static sysret_t
sys_sbrk(void *arg)
{
    panic("syscall sbrk not implemented");
}

// void memifo();
static sysret_t
sys_meminfo(void *arg)
{
    as_meminfo(&proc_current()->as);
    return ERR_OK;
}

// int dup(int fd);
static sysret_t
sys_dup(void *arg)
{
    panic("syscall dup not implemented");
}

// int pipe(int* fds);
static sysret_t
sys_pipe(void *arg)
{
    panic("syscall pipe not implemented");
}

// void sys_info(struct sys_info *info);
static sysret_t
sys_info(void *arg)
{
    sysarg_t info;

    kassert(fetch_arg(arg, 1, &info));

    if (!validate_ptr((void *)info, sizeof(struct sys_info)))
    {
        return ERR_FAULT;
    }
    // fill in using user_pgfault
    ((struct sys_info *)info)->num_pgfault = user_pgfault;
    return ERR_OK;
}

// void halt();
static sysret_t
sys_halt(void *arg)
{
    shutdown();
    panic("shutdown failed");
}

sysret_t
syscall(int num, void *arg)
{
    kassert(proc_current());
    if (num > 0 && num < NELEM(syscalls) && syscalls[num])
    {
        return syscalls[num](arg);
    }
    else
    {
        panic("Unknown system call");
    }
}
