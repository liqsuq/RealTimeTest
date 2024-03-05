#define _GNU_SOURCE
#include "rttest.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sched.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/times.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/vfs.h>
#include <linux/magic.h>

#define BINNAME "disk_test"
#define FILE_NAME "RTTEST-DISK_TEST"
#define MB_F (1024.0 * 1024.0)
#define MB_I (1024 * 1024)
#define BUFFER_SIZE (1024 * MB_I)
#define BLOCK_SIZE 512

/* following value isn't defined at Jetson */
#ifndef XFS_SUPER_MAGIC
#define XFS_SUPER_MAGIC 0x58465342
#endif

/* option flag */
#define NONE     0x00000000
#define DEBUG    0x00000001
#define DIRECT   0x00000002
#define RANDOM   0x00000004
#define CPUTIME  0x00000008
#define LEAVE    0x00000010
unsigned int optflg = NONE;

int iter = 1000;
int blocks_ini = 1;
char *fname = FILE_NAME;
int fsize = 64 * MB_I;
int advise = POSIX_FADV_NORMAL;

void do_disc_io(int rdwr, int fd) {
    void *dbuf;
    int hz, blocks, winsize, counts, offset, error = 0;
    struct timespec ts0, ts1;
    double elapsed_time, bps, tps;
    struct tms tms0, tms1;
    long utick, stick, u_sec, u_ms, s_sec, s_ms;
    long falign;
    int bsize = BLOCK_SIZE;

    hz = sysconf(_SC_CLK_TCK);
    // Linux 2.6からはO_DIRECTの要件はファイルシステムのブロックサイズでなく
    // 512バイトでアライメントされていればよいらしい
    falign = fpathconf(fd, _PC_REC_XFER_ALIGN);
    if((error = posix_memalign(&dbuf, falign, BUFFER_SIZE))) {
        ERRORMSG("%s\n", strerror(error));
        exit(EXIT_FAILURE);
    }
    memset(dbuf, 0xff, BUFFER_SIZE);
    blocks = MIN(BUFFER_SIZE/bsize, fsize/bsize);
    for(int b = blocks_ini; b <= blocks; b *= 2) {
        winsize = (b * bsize);
        if(!rdwr) /* read */
            if(optflg & RANDOM)
                if(optflg & DIRECT)
                    printf("READ  RANDOM(DIRECT)    : ");
                else
                    printf("READ  RANDOM    : ");
            else
                if(optflg & DIRECT)
                    printf("READ  SEQUENTIAL(DIRECT): ");
                else
                    printf("READ  SEQUENTIAL: ");
        else /* write */
            if(optflg & RANDOM)
                if(optflg & DIRECT)
                    printf("WRITE RANDOM(DIRECT)    : ");
                else
                    printf("WRITE RANDOM    : ");
            else
                if(optflg & DIRECT)
                    printf("WRITE SEQUENTIAL(DIRECT): ");
                else
                    printf("WRITE SEQUENTIAL: ");
        printf("bufsize: %7.1f KB ", winsize/1024.0);
        fflush(stdout);

        lseek(fd, 0L, 0);
        /* limits too many repetition ex. 131072 while winsize = 512 */
        counts = MIN(iter, fsize/winsize);
        times(&tms0);
        readtime(&ts0);
        for(int c = 0; c < counts; c++) {
            if(optflg & RANDOM) {
                offset = rand() % (((fsize-winsize)/bsize)+1) * bsize;
                // printf("[DEBUG] offset:%#010x\n", offset);
                lseek(fd, offset, SEEK_SET);
            }
            if(!rdwr) { /* read */
                if(read(fd, dbuf, winsize) != winsize){
                    error = 1;
                    ERRORMSG("read error (%d)\n", c);
                    break;
                }
            } else { /* write */
                if(write(fd, dbuf, winsize) != winsize){
                    error = 1;
                    ERRORMSG("write error (%d)\n", c);
                    break;
                }
            }
        }
 
        if(!error) {
            readtime(&ts1);
            times(&tms1);
            adjust(&ts0, &ts1, &elapsed_time);
            elapsed_time /= 1000000.0;
            bps = (counts * winsize) / (MB_F * elapsed_time);
            tps = counts / elapsed_time;
            printf("%10.2f MB/s %10.1f /sec", bps, tps);
            if(optflg & CPUTIME) {
                utick = (tms1.tms_utime - tms0.tms_utime);
                stick = (tms1.tms_stime - tms0.tms_stime);
                u_sec = utick / hz;
                u_ms = utick % hz * (1000 / hz);
                s_sec = stick / hz;
                s_ms = stick % hz * (1000 / hz);
                printf(" r/s/u = %.2f/%ld.%02ld/%ld.%02ld", elapsed_time,
                        u_sec, u_ms/10, s_sec, s_ms/10);
            }
            printf("\n");
            fflush(stdout);
        }
    }
    free(dbuf);
}

void version(void) {
    printf("%s ver. %s\n", BINNAME, VERSION);
}

void usage(void) {
    version();
    printf("\nUsage: %s [options] [filename]\n\n", BINNAME);
    printf("Options\n");
    printf("    -t iter : Max iterations [1000]\n");
    printf("    -b size : Initial block size [1]\n");
    printf("    -f name : File name [DISK_TEST]\n");
    printf("    -s size : File size in MB [20]\n");
    printf("    -d      : Use O_DIRECT\n");
    printf("    -r      : Random access mode\n");
    printf("    -c      : Show CPU time spent by this process\n");
    printf("    -a info : File advisory information [normal] \n");
    printf("              normal     : No further special treatment\n");
    printf("              random     : Expect random page references\n");
    printf("              sequential : Expect sequential page references\n");
    printf("              willneed   : Will need these pages\n");
    printf("              dontneed   : Don't need these pages\n");
    printf("              noreuse    : Data will be accessed once\n");
    printf("    -l      : Leave temp file; don't unlink tempfile\n");
    printf("    -v      : Print version information and exit\n");
    printf("    -h      : Print this message and exit\n");
}

int main(int argc, char **argv) {
    int opt;
    struct statfs statfsbuff;
    int oflag = O_RDWR|O_CREAT|O_DSYNC;
    int fd;

    while((opt = getopt(argc, argv, "t:b:f:s:drca:lvh")) != EOF) {
        switch(opt) {
        case 't':
            iter = atoi(optarg);
            break;
        case 'b':
            blocks_ini = atoi(optarg);
            break;
        case 'f':
            fname = optarg;
            break;
        case 's':
            fsize = atoi(optarg) * MB_I;
            break;
        case 'd':
            optflg |= DIRECT;
            break;
        case 'r':
            optflg |= RANDOM;
            break;
        case 'c':
            optflg |= CPUTIME;
            break;
        case 'a':
            if(strncmp(optarg, "normal", 4) == 0) {
                advise = POSIX_FADV_NORMAL;
            } else if(strncmp(optarg, "sequential", 1) == 0) {
                advise = POSIX_FADV_SEQUENTIAL;
            } else if (strncmp(optarg, "random", 1) == 0) {
                advise = POSIX_FADV_RANDOM;
            } else if (strncmp(optarg, "willneed", 1) == 0) {
                advise = POSIX_FADV_WILLNEED;
            } else if (strncmp(optarg, "dontneed", 1) == 0) {
                advise = POSIX_FADV_DONTNEED;
            } else if (strncmp(optarg, "noreuse", 4) == 0) {
                advise = POSIX_FADV_NOREUSE;
                optflg |= DIRECT;
            } else {
                ERRORMSG("invalid advise parameter\n"); 
                usage();
            }
            break;
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
            exit(EXIT_SUCCESS);
        }
    }
    if(optind < argc)
        fname = argv[optind];

    title("disk_test");
    printf("RedHawk Disk Performance Exerciser R00-00 ");
    while(unlink(fname) == 0);

    if(optflg & DIRECT)
        oflag |= O_DIRECT;
    if((fd = open(fname, oflag, 0644)) < 0) {
        ERRORMSG("%s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    posix_fadvise(fd, 0, 0, advise);

    statfs(fname, &statfsbuff);
    switch(statfsbuff.f_type) {
    case EXT2_SUPER_MAGIC:
        printf("ext2/ext3/ext4 file system test\n");
        break;
    case MSDOS_SUPER_MAGIC:
        printf("dos file system test\n");
        break;
    case XFS_SUPER_MAGIC:
        printf("xfs file system test\n");
        optflg |= RANDOM;
        break;
    default:
        printf("not support file system\n");
        exit(EXIT_FAILURE);
        break;
    }

    printf("Please Wait now pre allocate file... ");
    /* adjust filesize to allocatable value */
    for(; fd <= 0 && fsize > 0; fsize /= 2)
        if(posix_fallocate(fd, 0, fsize) == 0 )
            break;
    printf("%d bytes\n", fsize);

    mlockall_rt();
    do_disc_io(1, fd);
    do_disc_io(0, fd);
    munlockall();
    close(fd);
    if(!(optflg & LEAVE))
        unlink(fname);
    return 0;
}
