#define _GNU_SOURCE
#include "rttest.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/shm.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <sched.h>
#include <time.h>
#include <curses.h>
#include <poll.h>
#include <float.h>
#include <limits.h>
#ifdef RCIM
#include <rcim.h>
#include <sys/ioctl.h>
#endif

#define BINNAME "timer_loop"
#define SPAN_US 1000

/* option flag */
#define NONE     0x00000000
#define DEBUG    0x00000001
#define USE_RCIM 0x00000002
int optflg = NONE;

volatile sig_atomic_t loop_flg = 1;
volatile sig_atomic_t t_result[21] = {0,};
volatile sig_atomic_t min = INT_MAX, max = 0;
volatile sig_atomic_t min_under = INT_MAX, max_under = 0.0;
volatile sig_atomic_t min_over = INT_MAX, max_over = 0.0;
volatile sig_atomic_t maxcount = 0, count;

void calc_cycle(timespec *prev, volatile int *min, volatile int *max) {
    timespec ts;
    double diff;
    int ms;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    adjust(prev, &ts, &diff);
    *prev = ts;
    ms = (int)(diff / 1000.0);  /* us -> sec */
    if(ms > *max)
        *max = ms;
    if(ms < *min)
        *min = ms;
}

int calc(timespec *begin, timespec *end, timespec *prev_u, timespec *prev_o) {
    int diff, rtn;
    double result;

    adjust(begin, end, &result);
    diff = (int)result;
    if(diff < SPAN_US-100) {
        if(diff < min)
            min = diff;
        rtn = 0;
        calc_cycle(prev_u, &min_under, &max_under);
    }
    else if(diff <  SPAN_US- 95) rtn =  1;
    else if(diff <  SPAN_US- 85) rtn =  2;
    else if(diff <  SPAN_US- 75) rtn =  3;
    else if(diff <  SPAN_US- 60) rtn =  4;
    else if(diff <  SPAN_US- 45) rtn =  5;
    else if(diff <  SPAN_US- 30) rtn =  6;
    else if(diff <  SPAN_US- 15) rtn =  7;
    else if(diff <  SPAN_US-  5) rtn =  8;
    else if(diff <  SPAN_US)     rtn =  9;
    else if(diff == SPAN_US)     rtn = 10;
    else if(diff <= SPAN_US+  5) rtn = 11;
    else if(diff <= SPAN_US+ 15) rtn = 12;
    else if(diff <= SPAN_US+ 30) rtn = 13;
    else if(diff <= SPAN_US+ 45) rtn = 14;
    else if(diff <= SPAN_US+ 60) rtn = 15;
    else if(diff <= SPAN_US+ 75) rtn = 16;
    else if(diff <= SPAN_US+ 85) rtn = 17;
    else if(diff <= SPAN_US+ 95) rtn = 18;
    else if(diff <= SPAN_US+100) rtn = 19;
    else {
        if(diff > max)
            max = diff;
        rtn = 20;
        calc_cycle(prev_o, &min_over, &max_over);
    }
    return rtn;
}

void sigint_handler(int sig) {
    sigset_t sigset;
    /* mask SIGRTMIN to supress spurious */
    sigemptyset(&sigset);
    sigaddset(&sigset, SIGRTMIN);
    if(sigprocmask(SIG_BLOCK, &sigset, NULL) == -1)
        ERRORMSG("sigprocmask() failed - %s\n", strerror(errno));
    loop_flg = 0;
}

void sigrtmin_handler(int sig) {
    static int warm_up = 0;
    static timespec t0 = {0, 0};
    static timespec prev_under, prev_over;
    timespec t1;

    if(loop_flg) {
        if(!warm_up) {
            clock_gettime(CLOCK_MONOTONIC, &prev_under);
            clock_gettime(CLOCK_MONOTONIC, &prev_over);
        }
        clock_gettime(CLOCK_MONOTONIC, &t1);
        if(warm_up)
            t_result[calc(&t0, &t1, &prev_under, &prev_over)]++;
        t0 = t1;
        if(maxcount) {
            count--;
            if(count <= 0) {
                sigset_t sigset;
                /* mask SIGRTMIN to supress spurious */
                sigemptyset(&sigset);
                sigaddset(&sigset, SIGRTMIN);
                if(sigprocmask(SIG_BLOCK, &sigset, NULL) == -1)
                    ERRORMSG("sigprocmask() failed - %s\n", strerror(errno));
                loop_flg = 0;
            }
        }
        warm_up = 1;
    }
}

void make_disp(void) {
    mvprintw( 0, 1, "Frequency: %6dHz  Test Period:", 1000000 / SPAN_US);
    mvprintw( 1, 1, "    Histogram        Count          "
                    "Value   Cycle(min)   Cycle(max)");
    mvprintw( 2, 1, "Under %5dus", SPAN_US-100);
    mvprintw( 3, 1, "%5d-%5dus ",  SPAN_US-100, SPAN_US- 96);
    mvprintw( 4, 1, "%5d-%5dus ",  SPAN_US- 95, SPAN_US- 86);
    mvprintw( 5, 1, "%5d-%5dus ",  SPAN_US- 85, SPAN_US- 76);
    mvprintw( 6, 1, "%5d-%5dus ",  SPAN_US- 75, SPAN_US- 61);
    mvprintw( 7, 1, "%5d-%5dus ",  SPAN_US- 60, SPAN_US- 46);
    mvprintw( 8, 1, "%5d-%5dus ",  SPAN_US- 45, SPAN_US- 31);
    mvprintw( 9, 1, "%5d-%5dus ",  SPAN_US- 30, SPAN_US- 16);
    mvprintw(10, 1, "%5d-%5dus ",  SPAN_US- 15, SPAN_US-  6);
    mvprintw(11, 1, "%5d-%5dus ",  SPAN_US-  5, SPAN_US-  1);
    mvprintw(12, 1, "      %5dus ", SPAN_US);
    mvprintw(13, 1, "%5d-%5dus ",  SPAN_US+  1, SPAN_US+  5);
    mvprintw(14, 1, "%5d-%5dus ",  SPAN_US+  6, SPAN_US+ 15);
    mvprintw(15, 1, "%5d-%5dus ",  SPAN_US+ 16, SPAN_US+ 30);
    mvprintw(16, 1, "%5d-%5dus ",  SPAN_US+ 31, SPAN_US+ 45);
    mvprintw(17, 1, "%5d-%5dus ",  SPAN_US+ 46, SPAN_US+ 60);
    mvprintw(18, 1, "%5d-%5dus ",  SPAN_US+ 61, SPAN_US+ 75);
    mvprintw(19, 1, "%5d-%5dus ",  SPAN_US+ 76, SPAN_US+ 85);
    mvprintw(20, 1, "%5d-%5dus ",  SPAN_US+ 86, SPAN_US+ 95);
    mvprintw(21, 1, "%5d-%5dus ",  SPAN_US+ 96, SPAN_US+100);
    mvprintw(22, 1, " Over %5dus", SPAN_US+100);
}

void do_disp(time_t time_start) {
    time_t time_current;
    int cur;

    time(&time_current);
    cur = time_current - time_start;
    mvprintw(0, 35, "%02d:%02d:%02d", cur/3600, (cur%3600)/60, cur%60);
    for(int n=0; n<=20; n++)
        mvprintw(2+ n,17,"%10d", t_result[n]);
    if(t_result[ 0])
        mvprintw(2+ 0,30,"%10dus %10dms %10dms",min,min_under,max_under);
    if(t_result[20])
        mvprintw(2+20,30,"%10dus %10dms %10dms",max,min_over,max_over);
    //mvprintw(23, 0, " Count  %d", Count);
}

void do_main(void) {
    int key, disp_flg = 1, cur;
    time_t time_start, time_end;
    struct pollfd fdarray[1];

    fdarray[0].fd = STDIN_FILENO;
    fdarray[0].events = POLLIN|POLLPRI;

    init_curses();
    time(&time_start);
    if(!(optflg & USE_RCIM))
        time_start += 1; /* POSIX TIMER START delay is 1 sec*/;
    make_disp();
    while(loop_flg) {
        if(poll(fdarray, 1, 1) > 0) { /* poll stdin for 1ms */
            key = getch();
            if(key == 'q' || key == 'Q') {
                sigset_t sigset;
                /* mask SIGRTMIN to supress spurious */
                sigemptyset(&sigset);
                sigaddset(&sigset, SIGRTMIN);
                if(sigprocmask(SIG_BLOCK, &sigset, NULL) == -1)
                    ERRORMSG("sigprocmask() failed - %s\n", strerror(errno));
                break;
            } else if(key == 'p' || key == 'P') {
                disp_flg = ~disp_flg & 0x01;  /* toggle disp_flg */
            }
        }
        if(disp_flg)
            do_disp(time_start);
        refresh();
    }
    time(&time_end);
    finish_curses();

    cur = time_end - time_start;
    printf(" Frequency: %6dHz  Test Period: %02d:%02d:%02d\n",
            1000000 / SPAN_US, cur/3600, (cur%3600)/60, cur%60);
    printf("     Histogram        Count          "
           "Value   Cycle(min)   Cycle(max)\n");
    printf(" Under %5dus   %10d", SPAN_US-100, t_result[0]);
    if(t_result[0]) 
        printf("   %10dus %10dms %10dms\n", min, min_under, max_under);
    else
        printf("\n");
    printf(" %5d-%5dus   %10d\n", SPAN_US-100, SPAN_US- 96, t_result[1]);
    printf(" %5d-%5dus   %10d\n", SPAN_US- 95, SPAN_US- 86, t_result[2]);
    printf(" %5d-%5dus   %10d\n", SPAN_US- 85, SPAN_US- 76, t_result[3]);
    printf(" %5d-%5dus   %10d\n", SPAN_US- 75, SPAN_US- 61, t_result[4]);
    printf(" %5d-%5dus   %10d\n", SPAN_US- 60, SPAN_US- 46, t_result[5]);
    printf(" %5d-%5dus   %10d\n", SPAN_US- 45, SPAN_US- 31, t_result[6]);
    printf(" %5d-%5dus   %10d\n", SPAN_US- 30, SPAN_US- 16, t_result[7]);
    printf(" %5d-%5dus   %10d\n", SPAN_US- 15, SPAN_US-  6, t_result[8]);
    printf(" %5d-%5dus   %10d\n", SPAN_US-  5, SPAN_US-  1, t_result[9]);
    printf("       %5dus   %10d\n",SPAN_US,t_result[10]);
    printf(" %5d-%5dus   %10d\n", SPAN_US+  1, SPAN_US+  5, t_result[11]);
    printf(" %5d-%5dus   %10d\n", SPAN_US+  6, SPAN_US+ 15, t_result[12]);
    printf(" %5d-%5dus   %10d\n", SPAN_US+ 16, SPAN_US+ 30, t_result[13]);
    printf(" %5d-%5dus   %10d\n", SPAN_US+ 31, SPAN_US+ 45, t_result[14]);
    printf(" %5d-%5dus   %10d\n", SPAN_US+ 46, SPAN_US+ 60, t_result[15]);
    printf(" %5d-%5dus   %10d\n", SPAN_US+ 61, SPAN_US+ 75, t_result[16]);
    printf(" %5d-%5dus   %10d\n", SPAN_US+ 76, SPAN_US+ 85, t_result[17]);
    printf(" %5d-%5dus   %10d\n", SPAN_US+ 86, SPAN_US+ 95, t_result[18]);
    printf(" %5d-%5dus   %10d\n", SPAN_US+ 96, SPAN_US+100, t_result[19]);
    printf("  Over %5dus   %10d", SPAN_US+100, t_result[20]);
    if(t_result[20])
        printf("   %10dus %10dms %10dms\n", max, max_over, max_over);
    else
        printf("\n");
}

void timer_loop() {
    struct sigaction sigact;
    timer_t timer_id = NULL;

    set_sched(SCHED_FIFO, 51, 1);

    /* register SIGINT handler */
    memset(&sigact, 0, sizeof(sigact));
    sigact.sa_handler = sigint_handler;
    sigact.sa_flags   = SA_RESTART;
    if(sigaction(SIGINT, &sigact, NULL) < 0) {
        ERRORMSG("%s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    /* register SIGRTMIN handler */
    memset(&sigact, 0, sizeof(sigact));
    sigact.sa_handler = sigrtmin_handler;
    sigact.sa_flags   = SA_RESTART;
    if(sigaction(SIGRTMIN, &sigact, NULL) < 0) {
        ERRORMSG("%s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    #ifdef RCIM
    int rcim_fd = -1;
    if(optflg & USE_RCIM) {
        if((rcim_fd = create_rcim_timer(SIGRTMIN, SPAN_US)) < 0)
            exit(EXIT_FAILURE);
    } else {
    #endif
        if((timer_id = create_posix_timer(SIGRTMIN, 0, SPAN_US * 1000)) < 0)
            exit(EXIT_FAILURE);
    #ifdef RCIM
    }
    #endif

    do_main();

    #ifdef RCIM
    if(optflg & USE_RCIM)
        delete_rcim_timer(rcim_fd);
    else
    #endif
        timer_delete(timer_id);
}

void version(void) {
    printf("%s ver. %s\n", BINNAME, VERSION);
}

void usage(void) {
    version();
    printf("\nUsage: %s [options]\n\n", BINNAME);
    printf("Options:\n");
    #ifdef RCIM
    printf("    -r     : Use RCIM\n");
    #endif
    printf("    -S sec : Set test duration\n");
    printf("    -v     : Print version information and exit\n");
    printf("    -h     : Print this message and exit\n");
}

int main(int argc, char **argv) {
    int opt;

    #ifdef RCIM
    while((opt = getopt(argc, argv, "rS:vh")) != EOF) {
    #else
    while((opt = getopt(argc, argv, "S:vh")) != EOF) {
    #endif
        switch(opt) {
        #ifdef RCIM
        case 'r':
            optflg |= USE_RCIM;
            break;
        #endif
        case 'S':
            maxcount = atoi(optarg);
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
    if(maxcount) {
       count = maxcount * 1000000 / SPAN_US;
    }

    #ifdef RCIM
    if(optflg & USE_RCIM)
        printf("Use RCIM\n");
    else
    #endif
        printf("Use POSIX Timer\n");

    mlockall_rt();
    timer_loop();
    munlockall();
    return 0;
}
