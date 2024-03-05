#define _GNU_SOURCE
#include "rttest.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>    /* ftrunccate(), close()... */
#include <fcntl.h>     /* O_* */
#include <sys/mman.h>  /* shm_open(), mmap()... */
#include <errno.h>
#include <string.h>    /* strerror() */

#define BINNAME "shm_open"
#define SHM_NAME "rttest-shm0"

/* option flag */
#define NONE  0x00000000
#define DEBUG 0x00000001
unsigned int optflg = NONE;

typedef struct {
    int a, b, count, initial;
} shm_t;

void shm_open_test(void) {
    int shmfd;
    const int prot = PROT_READ|PROT_WRITE;
    const int flags = MAP_SHARED|MAP_LOCKED;
    shm_t *shm_p = NULL;

    title("shm_open");

    /* open shared memory */
    if((shmfd = shm_open(SHM_NAME, O_CREAT|O_RDWR|O_TRUNC, 0644)) < 0) {
        printf("shm_open() %37s ... NG %s\n", "", strerror(errno));
        exit(EXIT_FAILURE);
    }
    printf("shm_open() %37s ... OK\n", "");

    /* set size of the memory object */
    if(ftruncate(shmfd, sizeof(shm_t)) < 0) {
        printf("ftruncate() %36s ... NG %s\n", "", strerror(errno));
        goto ERROR;
    }
    printf("ftruncate() %36s ... OK\n", "");

    /* map the memory object */
    shm_p = mmap(NULL, sizeof(shm_t), prot, flags, shmfd, 0);
    if(shm_p == MAP_FAILED) {
        printf("mmap() %41s ... NG %s\n", "", strerror(errno));
        goto ERROR;
    }
    printf("mmap() %41s ... OK\n", "");
    if(optflg & DEBUG) {
        DEBUGMSG("map addr is %p\n", shm_p);
        fflush(stdout);
        press_any_key();
    }

    /* munmap() */
    if(munmap(shm_p, sizeof(shm_t)) < 0) {
        printf("munmap() %39s ... NG %s\n", "", strerror(errno));
        goto ERROR;
    }
    printf("munmap() %39s ... OK\n", "");

    /* test close() */
    if(close(shmfd) == -1) {
        printf("close() %40s ... NG %s\n", "", strerror(errno));
    }
    printf("close() %40s ... OK\n", "");

    /* test shm_unlink */
    if(shm_unlink(SHM_NAME) == -1) {
        printf("shm_unlink() %35s ... NG %s\n", "",strerror(errno));
    }
    printf("shm_unlink() %35s ... OK\n", "");

    exit(EXIT_SUCCESS);

ERROR:
    close(shmfd);
    shm_unlink(SHM_NAME);
    exit(EXIT_FAILURE);
}

void version(void) {
    printf("%s ver. %s\n", BINNAME, VERSION);
}

void usage(void) {
    version();
    printf("\nUsage: %s [options]\n\n", BINNAME);
    printf("Options:\n");
    printf("    -d : Debug mode\n");
    printf("    -v : Print version information and exit\n");
    printf("    -h : Print this message and exit\n");
}

int main(int argc, char **argv) {
    int opt;

    while((opt = getopt(argc, argv, "dvh")) != EOF) {
        switch(opt) {
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
    shm_open_test();
    return 0;
}

