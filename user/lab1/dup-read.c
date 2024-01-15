#include <lib/test.h>
#include <lib/string.h>

int main()
{
    int fd1, fd2, i;
    char buf[100];

    printf("Pass 1 \n");

    // test dup with invalid arguments
    if ((i = dup(15)) != ERR_INVAL || (i = dup(130)) != ERR_INVAL)
    {
        error("able to duplicated a non open file, return value was %d", i);
    }

    printf("Pass 2 \n");

    // Test smallest next descriptor
    if ((fd1 = open("/smallfile", FS_RDONLY, EMPTY_MODE)) < 0)
    {
        error("unable to open small file, return value was %d", fd1);
    }

    printf("Pass 3 \n");

    if ((fd2 = dup(fd1)) != fd1 + 1)
    {
        error("returned fd from dup was not the smallest free fd, was '%d'", fd2);
    }

    printf("This is 1st fd2 %d \n", fd2);

    printf("Pass 4 \n");

    // test offsets are respected in dupped files
    // read 10 bytes from the original fd
    assert(read(fd1, buf, 10) == 10);
    buf[10] = 0;

    printf("Pass 5 \n");

    for (int i = 0; buf[i] != '\0'; i++)
    {
        printf("%c\n", buf[i]);
    }

    if (strcmp(buf, "aaaaaaaaaa") != 0)
    {
        error("couldn't read from original fd after dup");
    }

    printf("Pass 6 \n");

    printf("This is the very first fd2. %d \n", fd2);
    // read the next 10 bytes from the dupped fd
    if ((i = read(fd2, buf, 10)) != 10)
    {
        error("coudn't read 10 byytes from the dupped fd, read %d bytes", i);
    }
    buf[10] = 0;

    printf("Pass 7 \n");

    for (int i = 0; buf[i] != '\0'; i++)
    {
        printf("%c\n", buf[i]);
    }

    if (strcmp(buf, "aaaaaaaaaa") == 0)
    {
        error("the duped fd didn't respect the read offset from the other file.");
    }

    printf("Pass 8 \n");

    if (strcmp(buf, "bbbbbbbbbb") != 0)
    {
        error("the duped fd didn't read the correct 10 bytes at the 10 byte offset");
    }

    printf("Pass 9 \n");

    if ((i = close(fd1)) != ERR_OK)
    {
        error("closing the original file, return value was %d", i);
    }

    printf("Pass 10 \n");

    printf("fd2 is %d \n", fd2);

    if ((i = read(fd2, buf, 5)) != 5)
    {
        error("wasn't able to read last 5 bytes from the duped file after the original file"
              "was closed, read %d bytes",
              i);
    }

    printf("Pass 11 \n");

    buf[5] = 0;
    assert(strcmp(buf, "ccccc") == 0);

    if ((i = close(fd2)) != ERR_OK)
    {
        error("closing the duped file, return value was %d", i);
    }

    pass("dup-read");
    exit(0);
    return 0;
}
