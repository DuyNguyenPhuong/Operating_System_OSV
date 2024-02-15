#include <kernel/bbq.h>
#include <kernel/pipe.h>
#include <kernel/console.h>
#include <lib/errcode.h>

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

void pipe_close(struct file *file)
{
    // Cleanup for closing a pipe
    pipe_t *pipe = (pipe_t *)file->info;
    if (!pipe)
        return;

    if (file == pipe->read_file)
    {
        pipe->read_file->info = NULL;
        pipe->read_file = NULL;
    }
    else if (file == pipe->write_file)
    {
        pipe->write_file->info = NULL;
        pipe->write_file = NULL;
    }

    // If both ends are closed, free the pipe resources
    if (pipe->read_file == NULL && pipe->write_file == NULL)
    {
        bbq_free(pipe->buffer);
        kfree(pipe);
    }
}

ssize_t pipe_read(struct file *file, void *buf, size_t count, offset_t *ofs)
{
    pipe_t *pipe = (pipe_t *)file->info;
    if (!pipe || !pipe->buffer)
        return -1; // Error if pipe or its buffer doesn't exist.

    char *buffer = (char *)buf;
    ssize_t bytesRead = 0;
    // while (bytesRead < count && !bbq_is_empty(pipe->buffer))
    // {
    //     buffer[bytesRead++] = bbq_remove(pipe->buffer);
    // }

    // // If the buffer is empty and the write end is closed, indicate EOF.
    // if (bytesRead == 0 && pipe->write_file == NULL)
    //     return 0;

    if (pipe->write_file != NULL)
    {
        // while (bytesRead < count)
        // {
        //     kprintf("Hi 1\n");
        //     buffer[bytesRead++] = bbq_remove(pipe->buffer);
        // }
        // return bytesRead;
        for (int i = 0; i < count; i++)
        {
            buffer[bytesRead++] = bbq_remove(pipe->buffer);
        }
        return count;
    }
    else
    {
        while (bytesRead < count && !bbq_is_empty(pipe->buffer))
        {
            kprintf("Hi 2\n");
            buffer[bytesRead++] = bbq_remove(pipe->buffer);
        }
        return bytesRead;
    }
}

ssize_t pipe_write(struct file *file, const void *buf, size_t count, offset_t *ofs)
{
    pipe_t *pipe = (pipe_t *)file->info;
    if (!pipe || !pipe->buffer)
        return -1; // Error if pipe or its buffer doesn't exist.

    // If the read end is closed, return an error.
    if (pipe->read_file == NULL)
        return ERR_END;

    const char *buffer = (const char *)buf;
    ssize_t bytesWritten = 0;
    while (bytesWritten < count)
    {
        bbq_insert(pipe->buffer, buffer[bytesWritten++]);
    }

    return bytesWritten;
}
