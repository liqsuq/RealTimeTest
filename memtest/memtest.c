#define _GNU_SOURCE
#include "rttest.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <stdint.h>

#define BINNAME "memtest"

typedef union {
    uint8_t d08[0x10000];
    uint16_t d16[0x10000/sizeof(uint16_t)];
    uint32_t d32[0x10000/sizeof(uint32_t)];
    uint64_t d64[0x10000/sizeof(uint64_t)];
} memory;

void memtest() {
    memory *mem, *p;
    size_t mem_size;
    timespec t0, t1;
    double diff;

    title("memtest");

    mem_size = sizeof(memory);
    if((mem = malloc(mem_size)) == NULL) {
        ERRORMSG("malloc() failed - %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    if((p = malloc(mem_size)) == NULL) {
        ERRORMSG("malloc() failed - %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    /* initialize */
    memset(p, 0xff, mem_size);
    for(int i=0; i<mem_size/sizeof(uint32_t); i++)
        mem->d32[i] = i;

    readtime(&t0);
    for(int i=0; i<mem_size/sizeof(uint32_t); i++)
        mem->d32[i] = p->d32[i];
    readtime(&t1);
    adjust(&t0, &t1, &diff);
    printf(" 32 bit write % 7.3f usec %f MB/s\n", diff, mem_size/diff);

    readtime(&t0);
    for(int i=0; i<mem_size/sizeof(uint32_t); i++)
        p->d32[i] = mem->d32[i];
    readtime(&t1);
    adjust(&t0, &t1, &diff);
    printf(" 32 bit  read % 7.3f usec %f MB/s\n", diff, mem_size/diff);

    readtime(&t0);
    for(int i=0; i<mem_size/sizeof(uint64_t); i++)
        mem->d64[i] = p->d64[i];
    readtime(&t1);
    adjust(&t0, &t1, &diff);
    printf(" 64 bit write % 7.3f usec %f MB/s\n", diff, mem_size/diff);

    readtime(&t0);
    for(int i=0; i<mem_size/sizeof(uint64_t); i++)
        p->d64[i] = mem->d64[i];
    readtime(&t1);
    adjust(&t0, &t1, &diff);
    printf(" 64 bit  read % 7.3f usec %f MB/s\n", diff, mem_size/diff);

    mem_size /= 2;
    readtime(&t0);
    memcpy(mem, p, mem_size);
    readtime(&t1);
    adjust(&t0, &t1, &diff);
    printf(" memcpy write % 7.3f usec %f MB/s\n", diff, mem_size/diff);

    readtime(&t0);
    memcpy(p, mem, mem_size);
    readtime(&t1);
    adjust(&t0, &t1, &diff);
    printf(" memcpy  read % 7.3f usec %f MB/s\n", diff, mem_size/diff);

    readtime(&t0);
    memmove((memory *)&mem->d08[mem_size], mem, mem_size);
    readtime(&t1);
    adjust(&t0, &t1, &diff);
    printf("memmove   r/w % 7.3f usec %f MB/s\n", diff, mem_size/diff);

    free(mem);
    free(p);
}

void version(void) {
    printf("%s ver. %s\n", BINNAME, VERSION);
}

void usage(void) {
    version();
    printf("\nUsage: %s [options]\n\n", BINNAME);
    printf("Options:\n");
    printf("    -v : Print version information and exit\n");
    printf("    -h : Print this message and exit\n");
}

int main(int argc, char **argv) {
    int opt;

    while((opt = getopt(argc, argv, "vh")) != EOF){
        switch(opt) {
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
    memtest();
    munlockall();
    return 0;

}
