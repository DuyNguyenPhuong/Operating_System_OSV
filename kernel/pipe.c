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

    pipe->buffer = bbq_init();
    pipe->read_file = fs_alloc_file();
    pipe->write_file = fs_alloc_file();

    if (!(pipe->read_file) || !(pipe->write_file))
    {
        // Clean up if failed
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

    // Set the OFlag
    pipe->read_file->oflag = FS_RDONLY;
    pipe->write_file->oflag = FS_WRONLY;

    return pipe;
}

void pipe_free(pipe_t *pipe)
{
    if (pipe)
    {
        // Free the buffer
        if (pipe->buffer)
        {
            bbq_free(pipe->buffer);
        }
        kfree(pipe); // Free the pipe itself
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
        pipe->read_file = NULL;
        file->info = NULL;
    }
    else if (file == pipe->write_file)
    {
        pipe->write_file = NULL;
        file->info = NULL;
    }
}

ssize_t pipe_read(struct file *file, void *buf, size_t count, offset_t *ofs)
{
    pipe_t *pipe = (pipe_t *)file->info;
    if (!pipe || !pipe->buffer)
        return -1; // Error if pipe or its buffer doesn't exist.

    char *buffer = (char *)buf;
    ssize_t bytesRead = 0;

    // If the write file is open, read up to the count
    if (pipe->write_file != NULL)
    {
        while (bytesRead < count)
        {
            buffer[bytesRead++] = bbq_remove(pipe->buffer);
        }
        return bytesRead;
    }
    // Else, if the write file is close, read up to the count or
    // bbq is empty
    else
    {
        while (bytesRead < count && !bbq_is_empty(pipe->buffer))
        {
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
