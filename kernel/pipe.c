#include <kernel/bbq.h>
#include <kernel/pipe.h>

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
    pipe->buffer = kmalloc(sizeof(bbq_t));
    if (!pipe->buffer)
    {
        kfree(pipe);
        return NULL;
    }

    // Initialize the BBQ for the pipe's buffer
    bbq_init(pipe->buffer);
    pipe->read_file = kmalloc(sizeof(struct file));
    pipe->write_file = kmalloc(sizeof(struct file));

    if (!(pipe->read_file) || !(pipe->write_file))
    {
        // Cleanup in case of allocation failure
        if (pipe->read_file)
            kfree(pipe->read_file);
        if (pipe->write_file)
            kfree(pipe->write_file);
        kfree(pipe);
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
            kfree(pipe->buffer); // Clean up the BBQ buffer
        }
        kfree(pipe); // Finally, free the pipe itself
    }
}

ssize_t pipe_read(struct file *file, void *buf, size_t count, offset_t *ofs)
{
    pipe_t *pipe = (pipe_t *)file->info; // Assuming file->info points to our pipe structure
    if (!pipe || !pipe->buffer)
        return -1; // Error handling for invalid pipe

    // Call bbq_read to read data from the pipe's buffer to the user buffer
    ssize_t bytes_read = bbq_read(pipe->buffer, buf, count);
    return bytes_read; // Return the number of bytes read
}

ssize_t pipe_write(struct file *file, const void *buf, size_t count, offset_t *ofs)
{
    pipe_t *pipe = (pipe_t *)file->info; // Cast file->info back to pipe_t
    if (!pipe || !pipe->buffer)
        return -1; // Error handling for invalid pipe
    kprintf("Join Write \n");
    // Call bbq_write to write data from the user buffer to the pipe's buffer
    ssize_t bytes_written = bbq_write(pipe->buffer, buf, count);
    return bytes_written; // Return the number of bytes written
}

void pipe_close(struct file *file)
{
    // Cleanup for closing a pipe
}
