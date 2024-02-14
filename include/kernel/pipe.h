#ifndef _PIPE_H_
#define _PIPE_H_

#include <kernel/types.h>
#include <kernel/fs.h>
#include <kernel/bbq.h>

#define PIPE_BUFFER_SIZE 512 // Define the size of the pipe buffer

// Define the pipe structure
typedef struct pipe
{
    BBQ *buffer;
    struct file *read_file;
    struct file *write_file;
    struct file_operations *pipe_ops;
} pipe_t;

pipe_t *pipe_alloc(void);
void pipe_free(pipe_t *pipe);

ssize_t pipe_read(struct file *file, void *buf, size_t count, offset_t *ofs);
ssize_t pipe_write(struct file *file, const void *buf, size_t count, offset_t *ofs);
void pipe_close(struct file *file);

// Define the file_operations for the pipe
// static struct file_operations pipe_ops = {
//     .read = pipe_read,
//     .write = pipe_write,
//     .close = pipe_close,
// };

#endif // _PIPE_H_