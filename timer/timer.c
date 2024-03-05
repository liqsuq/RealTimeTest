#define _GNU_SOURCE
#include "rttest.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <sched.h>
#include <sys/mman.h>
#include <math.h>
#include <float.h>

#define BINNAME "timer"
#define REPEAT 1001

/* option flag */
#define NONE      0x00000000
#define DEBUG     0x00000001
#define USE_RCIM  0x00000002
int optflg = NONE;

void timer_test(void) {
    int span_us = 1000;
    double diff, min = DBL_MAX, max = 0.0, avr = 0.0;
    sigset_t sigset;
    timer_t timer_id = NULL;
    timespec t0[REPEAT];
    #ifdef RCIM
    int rcim_fd = -1;
    #endif

    #ifdef RCIM
    if(optflg & USE_RCIM)
        title("RCIM timer test wait for 1 sec");
    else
    #endif
        title("POSIX timer test wait for 1 sec");

    /* block SIGRTMIN for timer signaling */
    if(sigprocmask(SIG_BLOCK, NULL, &sigset) == -1) {
        ERRORMSG("Cannot get sigprocmask - %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    sigaddset(&sigset, SIGRTMIN);
    if(sigprocmask(SIG_SETMASK, &sigset, NULL) == -1) {
        ERRORMSG("Cannot set sigprocmask - %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    /* set up timer emitting SIGRTMIN */
    #ifdef RCIM
    if(optflg & USE_RCIM) {
        if((rcim_fd = create_rcim_timer(SIGRTMIN, span_us)) < 0)
            exit(EXIT_FAILURE);
    } else {
    #endif
        if((timer_id = create_posix_timer(SIGRTMIN, 0, span_us * 1000)) < 0)
            exit(EXIT_FAILURE);
    #ifdef RCIM
    }
    #endif

    /* begin test */
    for(int i=0; i<REPEAT; i++) {
        sigwaitinfo(&sigset, NULL);
        readtime(&t0[i]);
    }

    /* finish test */
    #ifdef RCIM
    if(optflg & USE_RCIM)
        delete_rcim_timer(rcim_fd);
    else
    #endif
        timer_delete(timer_id);

    /* calculate result */
    for(int i=1; i<REPEAT; i++) {
        adjust(&t0[i-1], &t0[i], &diff);
        avr += diff;
        if(min > diff)
            min = diff;
        if(max < diff)
            max = diff;
    }
    avr /= REPEAT - 1;

    /* print result */
    printf("min %.3f avr %.3f max %.3f", min, avr, max);
    if((fabs(max-span_us) <= MAXJITTER) && (fabs(min-span_us) <= MAXJITTER))
        printf("  GOOD  ([%.3f,%.3f] <= %d us)\n",
                min-span_us, max-span_us, MAXJITTER);
    else
        printf("  FAIL  ([%.3f,%.3f] <  %d us)\n",
                min-span_us, max-span_us, MAXJITTER);
}

void counter_test_posix(void) {
    struct timespec t0, t1, w0 = {0, 10*1000*1000}, w1 = {0, 0};
    double diff, over = 0.0;

    title("POSIX counter test");

    clock_getres(CLOCK_MONOTONIC, &w0);
    printf("clock resolution is %ld ns\n", w0.tv_nsec);

    for(int i=0; i<100; i++) {
        readtime(&t0);
        nanosleep(&w0, NULL);
        readtime(&t1);
        adjust(&t0, &t1, &diff);
        over += diff - w0.tv_nsec / 1000;
    }
    over /= 100;
    printf("nanosleep resolution test overhead is %10.3f us\n", over);

    for(int usec=10; usec<1000000; usec<<=1) {
        w0.tv_sec = 0;
        w0.tv_nsec = 1000 * usec - (int)over;
        if(w0.tv_nsec < 0)
            w0.tv_nsec = 0;
        readtime(&t0);
        nanosleep(&w0, &w1);
        readtime(&t1);
        adjust(&t0, &t1, &diff);
        printf("request %6d us real %10.3f us ret %ld ns",
                usec, diff, w1.tv_nsec);
        if(fabs(diff - usec) <= MAXJITTER)
            printf(" GOOD (fabs( % 8.3f) <= %d us)\n",
                    (diff - usec), MAXJITTER);
        else
            printf(" FAIL (fabs( % 8.3f) >  %d us)\n",
                    (diff - usec), MAXJITTER);
    }
}
#ifdef RCIM
void counter_test_rcim() {
    int fd, w0;
    double diff, over = 0.0;
    struct rtc rtcs;
    timespec t0, t1;

    title("RCIM counter test");

    /* setup RCIM timer */
    if((fd = open("/dev/rcim/rtc1", O_RDWR)) < 0) {
        ERRORMSG("can't open rtc1 - %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    rtcs.r_modes = RTC_DEFAULT;
    rtcs.r_clkcnt = 1;
    rtcs.r_res = USEC;
    if(ioctl(fd, RTCIOCSET, &rtcs) < 0) {
        ERRORMSG("ioctl(RTCIOCSET) failed - %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    printf("clock resolution is 250 nsec\n"); /* RCIM IV */

    /* measure overhead of returning sleep */
    for(int i=0; i<100; i++) {
        ioctl(fd, RTCIOCSTOP, NULL);
        ioctl(fd, RTCIOCSETCNT, 1);
        readtime(&t0);
        ioctl(fd, RTCIOCSTART, NULL);
        ioctl(fd, RTCIOCWAIT, NULL);
        readtime(&t1);
        adjust(&t0, &t1, &diff);
        over += diff - 1.0;
    }
    over /= 100;
    printf("RTCIOCWAIT resolution test overhead is %.3f us\n", over);

    for(int usec=10; usec<50000; usec<<=1) {
        w0 = usec - (int)over ;
        if(w0 < 1)
            w0 = 1;
        ioctl(fd, RTCIOCSTOP, NULL);
        ioctl(fd, RTCIOCSETCNT, &w0);
        readtime(&t0);
        ioctl(fd, RTCIOCSTART, NULL);
        ioctl(fd, RTCIOCWAIT, NULL);
        readtime(&t1);
        adjust(&t0, &t1, &diff);
        printf("request %10d us real %10.3f us", usec, diff);
        if(fabs(diff-usec) <= MAXJITTER)
            printf(" GOOD (fabs(%.3f) <= %d us)\n", diff-usec, MAXJITTER);
        else
            printf(" FAIL (fabs(%.3f) >  %d us)\n", diff-usec, MAXJITTER);

    }
    close(fd);
}
#endif /* RCIM */

void version(void) {
    printf("%s ver. %s\n", BINNAME, VERSION);
}

void usage(void) {
    version();
    printf("\nUsage: %s [options]\n\n", BINNAME);
    printf("Options:\n");
    #ifdef RCIM
    printf("    -r : Use RCIM\n");
    #endif
    printf("    -v : Print version information and exit\n");
    printf("    -h : Print this message and exit\n");
}

int main(int argc, char **argv) {
    int opt;

    #ifdef RCIM
    while((opt = getopt(argc, argv, "rvh")) != EOF){
    #else
    while((opt = getopt(argc, argv, "rvh")) != EOF){
    #endif
        switch(opt) {
        #ifdef RCIM
        case 'r':
            optflg |= USE_RCIM;
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
    timer_test();
    #ifdef RCIM
    if(optflg & USE_RCIM)
        counter_test_rcim();
    else
    #endif
        counter_test_posix();
    munlockall();
    return 0;
}
