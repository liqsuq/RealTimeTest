#define _GNU_SOURCE  /* O_DIRECT */
#include "rttest.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <aio.h>
#include <sys/mman.h>
#include <malloc.h>
#include <errno.h>

#define BINNAME "aio"
#define FILE_NAME "rttest-rtfile"
#define PAGE 0x1000
#define DATA_BUF_SIZE PAGE
#define DATA_BUF_NUM 128

/* option flag */
#define NONE  0x00000000
#define DEBUG 0x00000001
#define LEAVE 0x00000002
unsigned int optflg = NONE;

int do_aio_test(int fd) {
    int status, err = 0;
    char *aio_buff[DATA_BUF_NUM];
    struct aiocb ctrlblk[DATA_BUF_NUM];
    struct aiocb *blklist[DATA_BUF_NUM];
    struct timespec ts_10sec = {10,0};
    long align;

    align = fpathconf(fd, _PC_REC_XFER_ALIGN);
    memset(&ctrlblk, 0, sizeof(struct aiocb) * DATA_BUF_NUM);
    for(int n=0; n<DATA_BUF_NUM; n++) {
        if(posix_memalign((void **)&aio_buff[n], align, PAGE) != 0) {
            printf("posix_memalign() %31s ... NG\n", "");
            return 1;
        }
        memset(aio_buff[n], n, DATA_BUF_SIZE);

        ctrlblk[n].aio_fildes = fd;
        ctrlblk[n].aio_offset = (long long)(DATA_BUF_SIZE * n);
        ctrlblk[n].aio_buf = aio_buff[n];
        ctrlblk[n].aio_nbytes = (long long)DATA_BUF_SIZE;
        ctrlblk[n].aio_reqprio = 0;
        ctrlblk[n].aio_sigevent.sigev_notify = SIGEV_NONE;
        ctrlblk[n].aio_lio_opcode = LIO_WRITE;
        blklist[n] = &ctrlblk[n];
    }

    /* lio_listio */
    if(lio_listio(LIO_WAIT, blklist, DATA_BUF_NUM, NULL) < 0) {
        printf("lio_listio() %35s ... NG: %s\n", "", strerror(errno));
        return 1;
    } else {
        printf("lio_listio() %35s ... OK\n", "");
    }
    /* aio_suspend */
    if(aio_suspend((void *)blklist, DATA_BUF_NUM, &ts_10sec) < 0) {
        printf("aio_suspend() %34s ... NG: %s\n", "", strerror(errno));
        return 1;
    } else {
    printf("aio_suspend() %34s ... OK\n", "");
    }

    /* aio_error */
    for(int n=0; n<DATA_BUF_NUM; n++) {
        if((status = aio_error(blklist[n])) < 0) {
            ERRORMSG("blklist[%d] failed - %s\n", n, strerror(status));
            err++;
        }
    }
    if(err)
        printf("aio_error() %36s ... NG\n", "");
    else
        printf("aio_error() %36s ... OK\n", "");

    /* aio_return */
    err = 0;
    for(int n=0; n<DATA_BUF_NUM; n++) {
        if(aio_return(blklist[n]) != DATA_BUF_SIZE) {
            ERRORMSG("blklist[%d] write error\n", n);
            err++;
        }
    }
    if(err)
        printf("aio_return() %35s ... NG\n", "");
    else
        printf("aio_return() %35s ... OK\n", "");

    return err;
}

void aio_test() {
    int fd;
    long long bytes, size;
    char *buf;
    struct stat fsts;
    long align;
    const int oflag = O_CREAT|O_WRONLY|O_DIRECT|O_SYNC;

    title("aio");
    printf("POSIX REALTIME FILE TEST PROGRAM\n");
    size = bytes = DATA_BUF_SIZE * DATA_BUF_NUM;

    /* open */
    unlink(FILE_NAME);
    if((fd = open(FILE_NAME, oflag, 0666)) < 0) {
        printf("open() realtime file %27s ... NG\n", "");
        exit(EXIT_FAILURE);
    }
    printf("open() realtime file %27s ... OK\n", "");

    /* memalign */
    align = fpathconf(fd, _PC_REC_XFER_ALIGN);
    if(posix_memalign((void **)&buf, align, PAGE) != 0) {
        printf("posix_memalign() %31s ... NG\n", "");
        goto ABORT;
    }
    printf("posix_memalign() %31s ... OK\n", "");
    memset(buf, 1, PAGE);

    /* fadvise */
    if(posix_fadvise(fd, 0, 0, POSIX_FADV_NOREUSE) < 0) {
        printf("posix_fadvise(POSIX_FADV_NOREUSE) %14s ... NG\n", "");
        goto ABORT;
    }
    printf("posix_fadvise(POSIX_FADV_NOREUSE) %14s ... OK\n", "");

    /* fallocate */
    if(posix_fallocate(fd, 0, bytes) < 0) {
        printf("posix_fallocate() %lld bytes %17s ... NG\n", bytes, "");
        goto ABORT;
    }
    printf("posix_fallocate() %lld bytes %17s ... OK\n", bytes, "");

    /* fstat (1) */
    if(fstat(fd, &fsts) < 0) {
        printf("fstat() %29s ... NG - %s\n", "", strerror(errno));
        goto ABORT;
    }
    if(fsts.st_size != size) {
        printf("fstat() %ld bytes %27s ... NG\n", fsts.st_size, "");
        goto ABORT;
    }
    printf("fstat() %ld bytes %27s ... OK\n", fsts.st_size, "");

    /* lseek (to last page) */
    bytes -= (long long)PAGE;
    if(lseek(fd, bytes, SEEK_SET) < 0) {
        printf("lseek(LAST) %36s ... NG\n", "");
        goto ABORT;
    }
    printf("lseek(LAST) %36s ... OK\n", "");

    /* write (last page)*/
    if(write(fd, buf, PAGE) < 0) {
        printf("write(LAST) %36s ... NG - %s\n","",  strerror(errno));
        goto ABORT;
    }
    printf("write(LAST) %36s ... OK\n", "");

    /* lseek (to zero offset) */
    if(lseek(fd, 0, SEEK_SET) < 0) {
        printf("lseek(TOP) %37s ... NG\n", "");
        goto ABORT;
    }
    printf("lseek(TOP) %37s ... OK\n", "");

    /* fstat (2) */
    if(fstat(fd, &fsts) < 0) {
        printf("fstat() %29s ... NG - %s\n", "", strerror(errno));
        goto ABORT;
    }
    if(fsts.st_size != size) {
        printf("fstat() %ld bytes %27s ... NG\n", fsts.st_size, "");
        goto ABORT;
    }
    printf("fstat() %ld bytes %27s ... OK\n", fsts.st_size, "");

    /* aio test */
    if(do_aio_test(fd)) {
        printf("aio file access %32s ... NG\n", "");
        goto ABORT;
    }
    printf("aio file access %32s ... OK\n", "");

    if(close(fd) < 0)
        printf("close() %40s ... NG\n", "");
    else
        printf("close() %40s ... OK\n", "");
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
    printf("    -v : Print version information and exit\n");
    printf("    -h : Print this message and exit\n");
}

int main(int argc, char **argv) {
    int opt;

    while((opt = getopt(argc, argv, "lvh")) != EOF) {
        switch(opt) {
        case 'l':
            optflg |= LEAVE;
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
    mlockall_rt();
    aio_test();
    munlockall();
    return 0;
}
