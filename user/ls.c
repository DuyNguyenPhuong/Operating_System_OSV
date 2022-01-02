#include <lib/string.h>
#include <lib/stddef.h>
#include <lib/stdio.h>
#include <lib/usyscall.h>
#include <lib/errcode.h>

char*
fmtname(char *path)
{
    static char buf[FNAME_LEN];
    char *p;

    // Find first character after last slash.
    for(p = path+strlen(path); p >= path && *p != '/'; p--) {}
    p++;

    // Return blank-padded name.
    if(strlen(p) >= FNAME_LEN) {
        return p;
    }

    memmove(buf, p, strlen(p));
    memset(buf+strlen(p), ' ', FNAME_LEN-strlen(p));
    return buf;
}

void
ls(char *path)
{
    char buf[512], *p;
    int fd;
    struct dirent dirent;
    struct stat stat;

    if((fd = open(path, FS_RDONLY, 0)) < 0){
        printf("ls: cannot open file %s\n", path);
        return;
    }

    if(fstat(fd, &stat) < 0){
        printf("ls: cannot stat %d\n", fd);
        close(fd);
        return;
    }

    switch(stat.ftype){
        case FTYPE_FILE: {
            printf("%s %d %d %d\n", fmtname(path), stat.ftype, stat.inode_num, stat.size);
            break;
        }
        case FTYPE_DIR: {
            if (strlen(path) + 1 + FNAME_LEN + 1 > sizeof buf) {
                printf("ls: path too long\n");
                break;
            }
            strcpy(buf, path);
            p = buf+strlen(buf);
            *p++ = '/';
            while(readdir(fd, &dirent) == ERR_OK) {
                if(dirent.inode_num == 0) {
                    continue;
                }
                memmove(p, dirent.name, FNAME_LEN);
                p[FNAME_LEN] = 0;
                int entry_fd;
                if((entry_fd = open(buf, FS_RDONLY, 0)) < 0) {
                    printf("ls: cannot open file %s\n", buf);
                    continue;
                }
                if(fstat(entry_fd, &stat) < 0) {
                    printf("ls: cannot stat %d\n", fd);
                } else {
                    printf("%s %d %d %d\n", fmtname(buf), stat.ftype, stat.inode_num, stat.size);
                }
                close(entry_fd);
            }
            break;
        }
        default: {
            printf("unknown ftype %d\n", stat.ftype);
        }
    }
    close(fd);
}

int
main(int argc, char *argv[])
{
    int i;

    if(argc < 2){
        ls("/");
    } else {
        for(i = 1; i < argc; i++) {
            ls(argv[i]);
        }
    }
    exit(0);
}
