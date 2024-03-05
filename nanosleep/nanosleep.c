#define _GNU_SOURCE
#include "rttest.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/mman.h>
#include <math.h>

#define BINNAME "nanosleep"
#define STEP_START 10000

/* option flag */
#define NONE    0x00000000
#define DEBUG   0x00000001
#define FUZZ    0x00000002
#define VERBOSE 0x00000004
unsigned int optflg = NONE;

void nanosleep_test(void) {
    struct timespec t0, t1, ts_wait = {0, 1*1000*1000};
    double diff, over = 0.0, cur;
    int step;

    title("posix nanosleep test");

    clock_getres(CLOCK_MONOTONIC, &t0);
    printf("clock resolution is %ld ns\n", t0.tv_nsec);

    for(int i=0; i<1000; i++) {
        readtime(&t0);
        nanosleep(&ts_wait, NULL); /* 1ms = 1000Hz */
        readtime(&t1);
        adjust(&t0, &t1, &diff);
        over += diff - ts_wait.tv_nsec / 1000;
    }
    over /= 1000;
    printf("nanosleep resolution test overhead is %10.3f us\n", over);

    step = STEP_START;
    for(int sec=0; sec<1; sec++) {
        for(int nsec=0; nsec<1000000000; nsec+=step) {
            ts_wait.tv_sec= sec;
            ts_wait.tv_nsec= nsec;
            readtime(&t0);
            nanosleep(&ts_wait, NULL);
            readtime(&t1);
            adjust(&t0, &t1, &diff);
            cur = diff - sec * 1000000 - nsec / 1000;
            cur -= over;
            printf("request %1d.%09d sec real %11.3f us", sec, nsec, diff);
            if(abs(cur) <= MAXJITTER)
                printf(" GOOD (abs(%11.3f) <= %d us)\n", cur, MAXJITTER);
            else
                printf(" FAIL (abs(%11.3f) >  %d us)\n", cur, MAXJITTER);
            /* 直前に繰上がったら(最上位桁が1なら)stepも桁上げする */
            if(nsec != 0 && nsec/(long)pow(10, (long)log10(nsec)) == 1)
                step = (int)pow(10, (long)log10(nsec));
        }
        step = STEP_START;
    }
}

#ifdef REDHAWK
void fuzz_shift_test(void) {
    struct timespec t0, t1, ts_wait = {0, 1*1000*1000};
    double diff, over = 0.0, min, max, sum, avr, cur, results[32];
    int count, step, shift = 5;
    char command[256];

    for(int i=0; i<32; i++)
        results[i] = DBL_MAX;

    title("clockevent_fuzz_shift test");

    for(int i=0; i<1000; i++) {
        readtime(&t0);
        nanosleep(&ts_wait, NULL); /* 1m = 1000Hz */
        readtime(&t1);
        adjust(&t0, &t1, &diff);
        over += diff - ts_wait.tv_nsec / 1000;
    }
    over /= 1000;
    printf("nanosleep resolution test overhead is %10.3f us\n", over);

    printf("current clockevent_fuzz_shift ");
    fflush(stdout);
    if(system("sysctl kernel.clockevent_fuzz_shift") == -1) {
        ERRORMSG("\nsystem() failed - %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    for(int i=9; i>=4; i--) {
        printf("\n### testing ");
        fflush(stdout);
        sprintf(command, "sysctl kernel.clockevent_fuzz_shift=%d", i);
        if(system(command) == -1) {
            ERRORMSG("\nsystem() failed - %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }
        min = DBL_MAX;
        count = 0;
        max = avr = sum = 0.0;
        step = STEP_START;
        for(int sec=0; sec<1; sec++) {
            for(int nsec=0; nsec<1000000000; nsec+=step) {
                ts_wait.tv_sec = sec;
                ts_wait.tv_nsec = nsec;
                readtime(&t0);
                nanosleep(&ts_wait, NULL);
                readtime(&t1);
                adjust(&t0, &t1, &diff);
                cur = diff - sec * 1000000 - nsec / 1000;
                sum += cur;
                if(cur < min)
                    min = cur;
                if(cur > max)
                    max = cur;
                count++;
                if(optflg & VERBOSE) {
                    cur -= over;
                    printf("request %1d.%09d sec real %11.3f us",
                            sec, nsec, diff);
                    if(abs(cur) <= MAXJITTER)
                        printf("  GOOD  (abs(%11.3f) <= %d us)\n",
                                cur, MAXJITTER);
                    else
                        printf("  FAIL  (abs(%11.3f) >  %d us)\n",
                                cur, MAXJITTER);
                }
                /* 直前に繰上がったら(最上位桁が1なら)stepを合わせる */
                if(nsec != 0 && nsec/(long)pow(10, (long)log10(nsec)) == 1)
                    step = (int)pow(10, (long)log10(nsec));
            }
            step = STEP_START;
        }
        avr = sum / count;
        printf("### min = %11.3f avr = %11.3f max = %11.3f sum = %11.3f\n",
                min, avr, max, sum);
        results[i] = sum;
    }
    min = DBL_MAX;
    for(int i=4; i<=9; i++) {
        if(results[i] < min) {
            min = results[i];
            shift = i;
        }
    }
    sprintf(command,"sysctl kernel.clockevent_fuzz_shift=%d >/dev/null",shift);
    if(system(command) == -1)
        ERRORMSG("\nsystem() failed - %s\n", strerror(errno));
    printf("\nBest clockevent_fuzz_shift value is %d\n", shift);
    printf("Add kernel.clockevent_fuzz_shift=%d to /etc/sysctl.conf\n", shift);
}
#endif

void version(void) {
    printf("%s ver. %s\n", BINNAME, VERSION);
}

void usage(void) {
    version();
    printf("\nUsage: %s [options]\n\n", BINNAME);
    printf("Options:\n");
    #ifdef REDHAWK
    printf("    -f : Test fuzz_shift\n");
    printf("    -F : Test fuzz_shift(verbose)\n");
    #endif
    printf("    -v : Print version information and exit\n");
    printf("    -h : Print this message and exit\n");
}

int main(int argc, char **argv) {
    int opt;

    #ifdef REDHAWK
    while((opt = getopt(argc, argv, "fFvh")) != EOF){
    #else
    while((opt = getopt(argc, argv, "vh")) != EOF){
    #endif
        switch(opt) {
        #ifdef REDHAWK
        case 'f':
            optflg |= FUZZ;
            break;
        case 'F':
            optflg |= FUZZ;
            optflg |= VERBOSE;
            break;
        #endif
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
    #ifdef REDHAWK
    if(optflg & FUZZ)
        fuzz_shift_test();
    else
    #endif
        nanosleep_test();
    munlockall();
    return 0;
}
