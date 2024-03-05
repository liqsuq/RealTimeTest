#define _GNU_SOURCE
#include "rttest.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <curses.h>
#include <time.h>
#include <signal.h>
#include <poll.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/mman.h>
#ifdef REDHAWK
#include <rcim.h>
#include <sys/ioctl.h>
#endif

#define BINNAME "clock"
#define RANGE (2000)
#define FACTOR (100)

volatile sig_atomic_t loop_flg;
volatile sig_atomic_t warm_up;
volatile sig_atomic_t t0_sec, t0_nsec, t1_sec, t1_nsec;
volatile sig_atomic_t diff;
volatile sig_atomic_t hist[RANGE*2 + 1]; /* for +/- 2 micro sec */
volatile sig_atomic_t COUNT, Count;

int fd_sclk;
int fd_rtc;
int fd_di;
unsigned int *sclk_p;

/* option flag */
#define NONE   0x00000000
#define DEBUG  0x00000001
#define POSIX  0x00000002
#define CLIENT 0x00000004
#define DIFF   0x00000008
unsigned int optflg = NONE;

void sigint_handler(int sig) {
    sigset_t sigset;
    /* mask SIGRTMIN to supress spurious */
    sigemptyset(&sigset);
    sigaddset(&sigset, SIGRTMIN);
    if(sigprocmask(SIG_BLOCK, &sigset, NULL) == -1)
        ERRORMSG("sigprocmask() failed - %s\n", strerror(errno));
    loop_flg = 1;
}

#ifdef REDHAWK
void sigrtmin_handler_rcim(int sig) {
    struct timespec now;

    if(!warm_up) {
        warm_up = 1;
        return;
    }

    t0_sec = *(sclk_p + RCIM_SYNCCLOCK_POSIX_OFFSET_HIGH / sizeof(int));
    t0_nsec = *(sclk_p + RCIM_SYNCCLOCK_POSIX_OFFSET_LOW / sizeof(int));

    clock_gettime(CLOCK_REALTIME, &now);
    t1_sec = now.tv_sec;
    t1_nsec = now.tv_nsec;

    diff = (t0_sec*1000000000 + t0_nsec) - (t1_sec*1000000000 + t1_nsec);
    if(optflg & DIFF) {
        if((-RANGE<=(diff/FACTOR)) && ((diff/FACTOR)<=RANGE))
            hist[(diff/FACTOR)+RANGE]++;
        else
            loop_flg = -1;
    }

    if(COUNT)
        Count--;
    if(COUNT && Count<=0) {
        sigset_t sigset;
        /* mask SIGRTMIN to supress spurious */
        sigemptyset(&sigset);
        sigaddset(&sigset, SIGRTMIN);
        if(sigprocmask(SIG_BLOCK, &sigset, NULL) == -1)
            ERRORMSG("sigprocmask() failed - %s\n", strerror(errno));
        loop_flg = 1;
    }
}

void display_rcim(void) {
    mvprintw( 0, 0, "[/dev/rcim/sclk]   RCIM clock: %11d sec %09d ns",
             t0_sec, t0_nsec);
    mvprintw( 1, 0, "[clock_gettime()] POSIX clock: %11d sec %09d ns",
             t1_sec, t1_nsec);
    if(!(optflg & DIFF)) {
        if(COUNT)
            mvprintw(2, 40, "    Count:%9d", Count);
    } else {
        mvprintw( 2, 0, "[diff]  RCIM - POSIX : %23s% 010d ns", " ", diff);
        mvprintw( 3, 0, "    -1000:%9d", hist[RANGE-10]);
        mvprintw( 4, 0, "    - 900:%9d", hist[RANGE- 9]);
        mvprintw( 5, 0, "    - 800:%9d", hist[RANGE- 8]);
        mvprintw( 6, 0, "    - 700:%9d", hist[RANGE- 7]);
        mvprintw( 7, 0, "    - 600:%9d", hist[RANGE- 6]);
        mvprintw( 8, 0, "    - 500:%9d", hist[RANGE- 5]);
        mvprintw( 9, 0, "    - 400:%9d", hist[RANGE- 4]);
        mvprintw(10, 0, "    - 300:%9d", hist[RANGE- 3]);
        mvprintw(11, 0, "    - 200:%9d", hist[RANGE- 2]);
        mvprintw(12, 0, "    - 100:%9d", hist[RANGE- 1]);
        mvprintw(13, 0, "        0:%9d", hist[RANGE]);
        mvprintw(14, 0, "    + 100:%9d", hist[RANGE+ 1]);
        mvprintw(15, 0, "    + 200:%9d", hist[RANGE+ 2]);
        mvprintw(16, 0, "    + 300:%9d", hist[RANGE+ 3]);
        mvprintw(17, 0, "    + 400:%9d", hist[RANGE+ 4]);
        mvprintw(18, 0, "    + 500:%9d", hist[RANGE+ 5]);
        mvprintw(19, 0, "    + 600:%9d", hist[RANGE+ 6]);
        mvprintw(20, 0, "    + 700:%9d", hist[RANGE+ 7]);
        mvprintw(21, 0, "    + 800:%9d", hist[RANGE+ 8]);
        mvprintw(22, 0, "    + 900:%9d", hist[RANGE+ 9]);
        mvprintw(23, 0, "    +1000:%9d", hist[RANGE+10]);
        if(COUNT)
            mvprintw(23, 40, "    Count:%9d", Count);
    }
}

int rcim_open(void) {
    struct rtc rtc_s;

    if((fd_sclk = open("/dev/rcim/sclk", O_RDWR)) < 0) {
        ERRORMSG("can't open /dev/rcim/sclk - %s\n", strerror(errno));
        return -1;
    }
    sclk_p = mmap(0, 0x1000, PROT_READ, MAP_SHARED, fd_sclk, 0);
    if(sclk_p == MAP_FAILED) {
        ERRORMSG("mmap() failed - %s\n", strerror(errno));
        return -1;
    }

    /* create rcim timer to emit di0 */
    if(!(optflg & CLIENT)) {
        if((fd_rtc = open("/dev/rcim/rtc0", O_RDWR)) < 0) {
            ERRORMSG("can't open rtc0 - %s\n", strerror(errno));
            return -1;
        }
        rtc_s.r_modes = RTC_REPEAT;
        rtc_s.r_clkcnt = 1000;
        rtc_s.r_res = USEC;
        if(ioctl(fd_rtc, RTCIOCSET, &rtc_s) < 0 ) {
            ERRORMSG("ioctl(RTCIOCSET) failed - %s\n", strerror(errno));
            return -1;
        }
        if(ioctl(fd_rtc, RTCIOCSTART, NULL) < 0) {
            ERRORMSG("ioct(RTCIOCSTART) failed - %s\n", strerror(errno));
            return -1;
        }
    }

    /* get di0 to emit SIGRTMIN */
    if((fd_di = open("/dev/rcim/di0", O_RDWR)) < 0) {
        ERRORMSG("can't open di0 - %s\n", strerror(errno));
        return -1;
    }
    ioctl(fd_di, DISTRIB_INTR_ATTACH_SIGNAL, SIGRTMIN);
    ioctl(fd_di, DISTRIB_INTR_ARM);
    ioctl(fd_di, DISTRIB_INTR_ENABLE);

    return 0;
}

void rcim_close(void) {
    if(fd_sclk > 0)
        close(fd_sclk);
    if(fd_rtc > 0) {
        ioctl(fd_rtc, RTCIOCSTOP);
        close(fd_rtc);
    }
    if(fd_di > 0) {
        ioctl(fd_di, DISTRIB_INTR_DISABLE);
        ioctl(fd_di, DISTRIB_INTR_DISARM);
        close(fd_di);
    }
}

void clock_rcim(void) {
    int key, disp_flg = 1;
    struct sigaction sigact;
    struct pollfd fdarray[1];

    #ifdef RESCHED
    resched_cntl(RESCHED_SET_VARIABLE, (char *)&rv);
    #endif
    fdarray[0].fd = STDIN_FILENO;
    fdarray[0].events = POLLIN|POLLPRI;

    if(optflg & CLIENT) {
        if(system("/usr/sbin/rcimdate") < 0)
            ERRORMSG("system() failed - %s\n", strerror(errno));
    } else { /* master */ 
        if(system("/bin/echo \"rtc0|di0\" > /proc/driver/rcim/config") < 0)
            ERRORMSG("system() failed - %s\n", strerror(errno));
    }

    /* register SIGINT handler */
    memset(&sigact, 0, sizeof(sigact));
    sigact.sa_handler = sigint_handler;
    sigact.sa_flags = SA_RESTART;
    if(sigaction(SIGINT, &sigact, NULL) < 0) {
        ERRORMSG("sigaction(SIGINT) failed - %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    /* register SIGRTMIN handler() */
    memset(&sigact, 0, sizeof(sigact));
    sigact.sa_handler = sigrtmin_handler_rcim;
    sigact.sa_flags = SA_RESTART;
    if(sigaction(SIGRTMIN, &sigact, NULL) < 0) {
        ERRORMSG("sigaction(SIGRTMIN) failed - %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    if(rcim_open() < 0)
        exit(EXIT_FAILURE);

    init_curses();
    while(loop_flg == 0) {
        if(poll(fdarray, 1, 1) > 0) { /* poll stdin for 1ms */
            key = getch();
            if(key=='q' || key=='Q') {
                sigset_t sigset;
                /* mask SIGRTMIN to supress spurious */
                sigemptyset(&sigset);
                sigaddset(&sigset, SIGRTMIN);
                if(sigprocmask(SIG_BLOCK, &sigset, NULL) == -1)
                    ERRORMSG("sigprocmask() failed - %s\n", strerror(errno));
                break;
            } else if(key == 'p' || key == 'P') {
                disp_flg = ~disp_flg & 0x01; /* toggle disp_flg */
            }
        }
        if(disp_flg)
            display_rcim();
        refresh();
    }
    finish_curses();

    rcim_close();
    if(loop_flg == -1)
        printf("over range error\n");
    if(optflg & DIFF) {
        printf("RCIM - POSIX (ns):\n");
        for(int i=0; i<(RANGE*2)+1; i++)
            if(hist[i])
                printf("%9d:%9d\n", (i-RANGE)*FACTOR, hist[i]);
    }
}
#endif
void sigrtmin_handler_posix(int sig) {
    struct timespec mono, real;

    clock_gettime(CLOCK_MONOTONIC, &mono);
    clock_gettime(CLOCK_REALTIME, &real);
    t0_sec = mono.tv_sec;
    t0_nsec = mono.tv_nsec;
    t1_sec = real.tv_sec;
    t1_nsec = real.tv_nsec;

    if(COUNT)
        Count--;
    if(COUNT && Count<=0) {
        sigset_t sigset;
        /* mask SIGRTMIN to supress spurious */
        sigemptyset(&sigset);
        sigaddset(&sigset, SIGRTMIN);
        if(sigprocmask(SIG_BLOCK, &sigset, NULL) == -1)
            ERRORMSG("sigprocmask() failed - %s\n", strerror(errno));
        loop_flg = 1;
    }
}

void display_posix(void) {
    mvprintw( 0, 0, "[clock_gettime()] CLOCK_MONOTONIC : %11d sec %09d ns",
            t0_sec, t0_nsec);
    mvprintw( 1, 0, "[clock_gettime()]  CLOCK_REALTIME : %11d sec %09d ns",
            t1_sec, t1_nsec);
    mvprintw( 2, 0, "Count:%9d", Count);
}

void clock_posix(void) {
    int key, disp_flg = 1;
    struct sigaction sigact;
    struct pollfd fdarray[1];
    timer_t timer_id;

    fdarray[0].fd = STDIN_FILENO;
    fdarray[0].events = POLLIN|POLLPRI;

    /* register SIGINT handler */
    memset(&sigact, 0, sizeof(sigact));
    sigact.sa_handler = sigint_handler;
    sigact.sa_flags = SA_RESTART;
    if(sigaction(SIGINT, &sigact, NULL) < 0) {
        ERRORMSG("sigaction(SIGINT) failed - %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    /* register SIGRTMIN handler() */
    memset(&sigact, 0, sizeof(sigact));
    sigact.sa_handler = sigrtmin_handler_posix;
    sigact.sa_flags = SA_RESTART;
    if(sigaction(SIGRTMIN, &sigact, NULL) < 0) {
        ERRORMSG("sigaction(SIGRTMIN) failed - %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    timer_id = create_posix_timer(SIGRTMIN, 0, 1000000);
    if(timer_id < 0)
        exit(EXIT_FAILURE);

    init_curses();
    while(loop_flg == 0) {
        if(poll(fdarray, 1, 1) > 0) { /* poll stdin for 1ms */
            key = getch();
            if(key=='q' || key=='Q') {
                sigset_t sigset;
                /* mask SIGRTMIN to supress spurious */
                sigemptyset(&sigset);
                sigaddset(&sigset, SIGRTMIN);
                if(sigprocmask(SIG_BLOCK, &sigset, NULL) == -1)
                    ERRORMSG("sigprocmask() failed - %s\n", strerror(errno));
                break;
            } else if(key == 'p' || key == 'P') {
                disp_flg = ~disp_flg & 0x01; /* toggle disp_flg */
            }
        }
        if(disp_flg)
            display_posix();
        refresh();
    }
    finish_curses();

    timer_delete(timer_id);
}

void version(void) {
    printf("%s ver. %s\n", BINNAME, VERSION);
}

void usage(void) {
    version();
    printf("\nUsage: %s [options]\n\n", BINNAME);
    printf("Options:\n");
    #ifdef REDHAWK
    printf("    -p     : Not use RCIM\n");
    printf("    -c     : Client mode, not emit signal voluntary (RCIM only)\n");
    printf("    -d     : Show diff and histogram (RCIM only)\n");
    #endif
    printf("    -S sec : Set test duration\n");
    printf("    -v     : Print version information and exit\n");
    printf("    -h     : Print this message and exit\n");
}

int main(int argc, char **argv) {
    int opt;

    #ifdef REDHAWK
    while((opt = getopt(argc, argv, "pcdS:vh")) != EOF) {
    #else
    while((opt = getopt(argc, argv, "S:vh")) != EOF) {
    #endif
        switch(opt) {
        #ifdef REDHAWK
        case 'p':
            optflg |= POSIX;
            break;
        case 'c':
            optflg |= CLIENT;
            break;
        case 'd':
            optflg |= DIFF;
            break;
        #endif
        case 'S':
            COUNT = atoi(optarg);
            Count = COUNT * 1000;
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
    #ifdef REDHAWK
    if(!(optflg & POSIX))
        clock_rcim();
    else
    #endif
        clock_posix();
    munlockall();
    return 0;
}
