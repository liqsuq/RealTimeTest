#define _GNU_SOURCE
#include "rttest.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <sys/mman.h>

#ifdef RESCNTL
#include <sys/rescntl.h>
#endif

#define BINNAME "loop"
#define SPAN_US 100
#define REP 100001

/* option flag */
#define NONE      0x00000000
#define DEBUG     0x00000001
unsigned int optflg = NONE;

void loop_test(void) {
    timespec t0, t1[REP] = {0}, ts_100ms = {0, 100 * 1000 * 1000};
    double remain, diff[REP] = {0}, min, max, avr, target = SPAN_US;
    char str[64];

    sprintf(str, "posix timer test wait for %4d sec", (REP-1)*SPAN_US/1000000);
    title(str);
    /* Hyperthreading有効でCPU1使用の場合画面が固まるのでフラッシュしておく */
    fflush(stdout);
    nanosleep(&ts_100ms, NULL);

    #ifdef RESCNTL
    struct resched_var rv;
    resched_cntl(RESCHED_SET_VARIABLE, (char *)&rv);
    resched_lock(&rv);
    #endif

    /* busy-loop for SPAN_US us and read time */
    readtime(&t0);
    for(int count=0; count<REP; count++) {
        do{
            readtime(&t1[count]);
            adjust(&t0, &t1[count], &remain);
        } while(remain < target);
        t0 = t1[count];
    }

    #ifdef RESCNTL
    resched_unlock(&rv);
    #endif

    for(int i=0; i<REP-1; i++)
        adjust(&t1[i], &t1[i+1], &diff[i]);
    stats(diff, REP-1, target, &min, &avr, &max, NULL);

    printf("min %.3f avr %.3f max %.3f", min, avr, max);
    if((fabs(max-target) <= MAXJITTER) && (fabs(target-min) <= MAXJITTER))
        printf(" GOOD ([%.3f,%.3f] <= %d us)\n",
                (target-min), (max-target), MAXJITTER);
    else
        printf(" FAIL ([%.3f,%.3f] >  %d us)\n",
                (target-min), (max-target), MAXJITTER);
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

    while((opt = getopt(argc, argv, "vh")) != EOF) {
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
    loop_test();
    munlockall();
    return 0;
}
