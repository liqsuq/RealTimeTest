#define _GNU_SOURCE
#define _LARGEFILE64_SOURCE
#include "rttest.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>

#define BINNAME "rfile"
#define FILE_NAME "rttest-rtfile"

/* option flag */
#define NONE  0x00000000
#define DEBUG 0x00000001
#define LEAVE 0x00000002
unsigned int optflg = NONE;

void rfile_test() {
    int fd, err = 0;
    off64_t align;
    long long bytes = 4294967296LL; /* 0x100000000 */
    char *buf;
    const int oflag = O_LARGEFILE|O_CREAT|O_RDWR|O_DIRECT|O_SYNC;

    title("rfile");
    printf("DIRECT DISK TRANSFER & LARGE FILE TEST PROGRAM\n");

    /* open */
    if((fd = open64(FILE_NAME, oflag, 0666)) < 0) {
        ERRORMSG("open64() failed - %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    /* allocate write buffer */
    align = fpathconf(fd, _PC_REC_XFER_ALIGN);
    if(optflg & DEBUG)
        DEBUGMSG("_PC_REC_XFER_ALIGN %ld\n", align);
    if((err = posix_memalign((void *)&buf, align, 0x1000)) != 0) {
        printf("posix_memalign() failed: %s\n", strerror(err));
        goto ABORT;
    }
    memset(buf, 1, 0x1000);
    printf("allocate %lld bytes\n", bytes);

    /* fadvise */
    if(posix_fadvise64(fd, 0, 0, POSIX_FADV_NOREUSE) != 0) {
        printf("posix_fadvise(POSIX_FADV_NOREUSE) %14s ... NG\n", "");
        goto ABORT;
    }
    printf("posix_fadvise(POSIX_FADV_NOREUSE) %14s ... OK\n", "");

    /* fallocate */
    if(posix_fallocate64(fd, 0, bytes) != 0) {
        printf("posix_fallocate64 %30s ... NG\n", "");
        goto ABORT;
    }
    printf("posix_fallocate64 %30s ... OK\n", "");

    /* lseek */
    bytes -= 0x1000;
    if(lseek64(fd, bytes, SEEK_SET) < 0) {
        printf("lseek %42s ... NG\n", "");
        goto ABORT;
    }
    printf("lseek %42s ... OK\n", "");

    /* write */
    if(write(fd, buf, 0x1000) != 0x1000) {
        printf("write %42s ... NG\n", "");
        goto ABORT;
    }
    printf("write %42s ... OK\n", "");

    if(close(fd) < 0)
        printf("close %42s ... NG\n", "");
    else
        printf("close %42s ... OK\n", "");
    if(!(optflg & LEAVE))
        unlink(FILE_NAME);
    exit(EXIT_SUCCESS);
ABORT:
    close(fd);
    if(!(optflg & LEAVE))
        unlink(FILE_NAME);
    exit(EXIT_FAILURE);
}

void version(void) {
    printf("%s ver. %s\n", BINNAME, VERSION);
}

void usage(void) {
    version();
    printf("\nUsage: %s [options]\n\n", BINNAME);
    printf("Options:\n");
    printf("    -l : Leave temp file; don't unlink temp file\n");
    printf("    -d : Debug mode\n");
    printf("    -v : Print version information and exit\n");
    printf("    -h : Print this message and exit\n");
}

int main(int argc, char **argv) {
    int opt;

    while((opt = getopt(argc, argv, "ldvh")) != EOF) {
        switch(opt) {
        case 'l':
            optflg |= LEAVE;
            break;
        case 'd':
            optflg |= DEBUG;
            break;
        case 'v':
            version();
            exit(EXIT_SUCCESS);
        case 'h':
            usage();
            exit(EXIT_SUCCESS);
        default:
            usage();
            exit(EXIT_FAILURE);
        }
    }
    rfile_test();
    return 0;
}
