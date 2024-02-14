#include <kernel/bbq.h>
#include <kernel/pipe.h>
#include <kernel/console.h>

struct file_operations pipe_ops = {
    .read = pipe_read,
    .write = pipe_write,
    .close = pipe_close,
};
pipe_t *pipe_alloc(void)
{
    pipe_t *pipe = kmalloc(sizeof(pipe_t));
    if (!pipe)
        return NULL;

    // Initialize the BBQ
    if (!pipe->buffer)
    {
        kfree(pipe);
        return NULL;
    }

    // Initialize the BBQ for the pipe's buffer
    pipe->buffer = bbq_init();
    pipe->read_file = fs_alloc_file();
    pipe->write_file = fs_alloc_file();

    if (!(pipe->read_file) || !(pipe->write_file))
    {
        // Cleanup in case of allocation failure
        if (pipe->read_file)
            fs_free_file(pipe->read_file);
        if (pipe->write_file)
            fs_free_file(pipe->write_file);
        pipe_free(pipe);
        return NULL; // Error handling
    }

    pipe->read_file->f_ops = &pipe_ops;
    pipe->write_file->f_ops = &pipe_ops;
    pipe->read_file->info = pipe;
    pipe->write_file->info = pipe;

    pipe->read_file->oflag = FS_RDONLY;
    pipe->write_file->oflag = FS_WRONLY;

    return pipe;
}

void pipe_free(pipe_t *pipe)
{
    if (pipe)
    {
        // Assuming bbq_free properly cleans up BBQ, including any spinlocks
        if (pipe->buffer)
        {
            bbq_free(pipe->buffer); // Clean up the BBQ buffer
        }
        kfree(pipe); // Finally, free the pipe itself
    }
}

ssize_t pipe_read(struct file *file, void *buf, size_t count, offset_t *ofs)
{
    pipe_t *pipe = (pipe_t *)file->info;
    if (!pipe || !pipe->buffer)
        return -1;

    char *buffer = (char *)buf;
    char remove = bbq_remove(pipe->buffer);
    buffer[0] = remove;
    return (ssize_t)1;
}

ssize_t pipe_write(struct file *file, const void *buf, size_t count, offset_t *ofs)
{
    pipe_t *pipe = (pipe_t *)file->info; // Cast file->info back to pipe_t
    if (!pipe || !pipe->buffer)
        return -1;
    char *buffer = (char *)buf;
    bbq_insert(pipe->buffer, buffer[0]);
    return (ssize_t)1;
}

void pipe_close(struct file *file)
{
    // Cleanup for closing a pipe
}
