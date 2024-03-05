#ifndef RTTEST_H
#define RTTEST_H

#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sched.h>
#include <signal.h>
#include <math.h>
#include <time.h>
#include <ctype.h>
#include <float.h>
#include <sys/mman.h>
#include <pthread.h>
#include <curses.h>
#include <sys/syscall.h>

#define VERSION "1.2403"
#define MAXJITTER 100
//#define BOOT_CPU 0
//#define TARGET_CPU 7

/* Logger functions */
#define FATALMSG(...) MSG("FATAL",__VA_ARGS__)
#define ERRORMSG(...) MSG("ERROR",__VA_ARGS__)
#define WARNMSG(...) MSG("WARN",__VA_ARGS__)
#define INFOMSG(...) MSG("INFO",__VA_ARGS__)
#define DEBUGMSG(...) MSG("DEBUG",__VA_ARGS__)
#define MSG(...) _MSG(__VA_ARGS__,_MSG4,_MSG3,_MSG2,_MSG1,_MSG0)(__VA_ARGS__)
#define _LOG(msg) "[%s] %s %d: %s() - "msg
#define _FMT __FILE__,__LINE__,__func__
#define _MSG0(lv,msg) fprintf(stderr,_LOG(msg),lv,_FMT)
#define _MSG1(lv,msg,...) fprintf(stderr,_LOG(msg),lv,_FMT,__VA_ARGS__)
#define _MSG2(lv,msg,...) fprintf(stderr,_LOG(msg),lv,_FMT,__VA_ARGS__)
#define _MSG3(lv,msg,...) fprintf(stderr,_LOG(msg),lv,_FMT,__VA_ARGS__)
#define _MSG4(lv,msg,...) fprintf(stderr,_LOG(msg),lv,_FMT,__VA_ARGS__)
#define _MSG(_1,_2,_3,_4,_5,_6,name,...) name

#define MAX(A,B) ((A) > (B) ? (A) : (B))
#define MIN(A,B) ((A) < (B) ? (A) : (B))

#ifdef __cplusplus
extern "C" {
#endif

typedef struct timespec timespec;

#ifdef TSC
double mhz;
static inline double get_clock(void) {
    int ret;
    char buf[1000];
    FILE *fd = fopen("/proc/cpuinfo", "r");
    if(fd == NULL){
        ERRORMSG("can't open /proc/cpuinfo\n");
        return 0.0;
    }

    while(1) {
        if(fgets(buf, sizeof(buf), fd) == NULL){
            ERRORMSG("cpu MHz wasn't located in /proc/cpuinfo\n");
            return 0.0;
        }
        ret = sscanf(buf, "cpu MHz         : %lf", &mhz);
        if(ret == 1) {
            fclose(fd);
            return mhz;
        }
    }
}
static inline unsigned long long rdtsc() {
    unsigned int lo, hi;
    /* We cannot use "=A", since this would use %rax on x86_64 */
    asm volatile ("rdtsc" : "=a" (lo), "=d" (hi));
    return (unsigned long long)hi << 32 | lo;
}
#endif

static inline void readtime(timespec *now) {
    #ifdef TSC
    unsigned long long tsc;
    tsc = rdtsc();
    now->tv_nsec = tsc & 0xFFFFFFFF;
    now->tv_sec = (tsc>>32) & 0xFFFFFFFF;
    #else
    clock_gettime(CLOCK_MONOTONIC, now);
    #endif
}

static inline void adjust(timespec *start, timespec *finish, double *diff) {
    #ifdef TSC
    unsigned long long t0, t1;

    if(mhz < 10e-6)
        mhz = get_clock();

    t0 = (unsigned long long)start->tv_sec;
    t0 <<= 32;
    t0 |= 0xFFFFFFFF & (unsigned long long)(start->tv_nsec);
    t1 = (unsigned long long)finish->tv_sec;
    t1 <<= 32;
    t1 |= 0xFFFFFFFF & (unsigned long long)(finish->tv_nsec);
    *diff = (double)(t1-t0) / mhz;
    #else
    int sec, nsec;
    sec = finish->tv_sec - start->tv_sec;
    nsec = finish->tv_nsec - start->tv_nsec;
    if(nsec < 0) {
        sec--;
        nsec += 1000000000;
    }
    *diff = ((double)nsec/1000.0) + ((double)sec*1000000.0);
    #endif
}

static inline void title(char *str) {
    printf("****************************************************\n");
    printf("****  %-40s  ****\n", str);
    printf("****************************************************\n\n");
}

static inline void mlockall_rt(void) {
    #ifdef MCL_ONFAULT
    mlockall(MCL_CURRENT|MCL_FUTURE|MCL_ONFAULT);
    #else
    mlockall(MCL_CURRENT|MCL_FUTURE);
    #endif
}

static inline int set_sched(int policy, int prio, int cpu) {
    pid_t pid = getpid();
    struct sched_param param;
    cpu_set_t cpuset;
    int ret;

    /* set process priority */
    param.sched_priority = prio;
    if((ret = sched_setscheduler(pid, policy, &param)))
        ERRORMSG("sched_setscheduler() failed - %s\n", strerror(errno));
    /* bind CPU */
    CPU_ZERO(&cpuset);
    CPU_SET(cpu, &cpuset);
    if((ret = sched_setaffinity(pid, sizeof(cpuset), &cpuset)))
        ERRORMSG("sched_setaffinity() failed - %s\n", strerror(errno));
    /* show running CPU */
    if((ret = sched_getcpu()) < 0)
        ERRORMSG("sched_getcpu() failed - %s\n", strerror(errno));
    else
        printf("Process %d is running CPU%d\n", pid, ret);

    return ret;
}

static inline int set_sched_thread(int policy, int prio, int cpu) {
    pthread_t tid = pthread_self();
    pid_t pid = syscall(SYS_gettid);
    struct sched_param param;
    cpu_set_t cpuset;
    int ret;

    /* set thread priority */
    param.sched_priority = prio;
    if((ret = pthread_setschedparam(tid, policy, &param)))
        ERRORMSG("pthread_setschedparam() failed - %s\n", strerror(ret));
    /* bind CPU */
    CPU_ZERO(&cpuset);
    CPU_SET(cpu, &cpuset);
    if((ret = pthread_setaffinity_np(tid, sizeof(cpuset), &cpuset)))
        ERRORMSG("pthread_setaffinity_np() failed - %s\n", strerror(ret));
    /* show running CPU */
    if((ret = sched_getcpu()) < 0)
        ERRORMSG("sched_getcpu() failed - %s\n", strerror(errno));
    else
        printf("Thread %d is running CPU%d\n", pid, ret);

    return ret;
}

static inline timer_t create_posix_timer(int signum, int sec, int nsec) {
    struct sigevent event;
    timer_t timer;
    /* interval and initial delay */
    struct itimerspec itspec = {{sec, nsec}, {1, 0}};

    /* Create the POSIX timer to emit signum */
    event.sigev_notify = SIGEV_SIGNAL;
    event.sigev_signo = signum;
    if(timer_create(CLOCK_MONOTONIC, &event, &timer) == -1) {
        ERRORMSG("timer_create() failed - %s\n", strerror(errno));
        return (timer_t)-1;
    }
    /* Set timer spec and arm timer */
    if(timer_settime(timer, 0, &itspec, NULL) == -1) {
        return (timer_t)-1;
    }
    return timer;
}

static inline void stats(double *val, int rep, int target,
                         double *min, double *avr, double *max, double *sd) {
    double tmpmin = DBL_MAX, tmpavr = 0.0, tmpmax = 0.0, tmpsd = 0.0, dev;
    for(int i=0; i<rep; i++) {
        if(tmpmin > val[i])
            tmpmin = val[i];
        if(tmpmax < val[i])
            tmpmax = val[i];
        tmpavr += val[i];
    }
    tmpavr /= rep;
    for(int i=0; i<rep; i++) {
        if(target == 0)
            dev = val[i] - tmpavr;
        else
            dev = val[i] - target;
        tmpsd += dev * dev;
    }
    tmpsd = sqrt(tmpsd/rep);
    if(min != NULL)
        *min = tmpmin;
    if(avr != NULL)
        *avr = tmpavr;
    if(max != NULL)
        *max = tmpmax;
    if(sd != NULL)
        *sd = tmpsd;
}

static inline void press_any_key(void) {
    printf("Press enter key to continue... ");
    fflush(stdout);
    getchar();
}

static inline int isdigit_str(char *buf) {
    int result = 1;
    for(int i=0; buf[i]!='\0' && result!=0; i++)
        result = isdigit(buf[i]);
    return result;
}

#ifdef RCIM
#include <rcim.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

static inline int create_rcim_timer(int signum, int span_us) {
    int fd;
    struct rtcl rtc_s;

    if((fd = open("/dev/rcim/rtc0", O_RDWR)) < 0) {
        ERRORMSG("can't open rtc0 - %s\n", strerror(errno));
        return -1;
    }
    rtc_s.r_count = 1000000;
    rtc_s.r_repeat = span_us;
    rtc_s.r_res = USEC;
    if(ioctl(fd, RTCIOCSETL, &rtc_s) < 0) {
        ERRORMSG("ioctl(RTCIOCSET) failed - %s\n", strerror(errno));
        return -1;
    }
    if(ioctl(fd, IOCTLSIGATTACH, signum) < 0) {
        ERRORMSG("ioctl(IOCTLSIGATTACH) failed - %s\n", strerror(errno));
        return -1;
    }
    if(ioctl(fd, RTCIOCSTART, NULL) < 0) {
        ERRORMSG("ioctl(RTCIOCSTART) failed - %s\n", strerror(errno));
        return -1;
    }

    return fd;
}

static inline void delete_rcim_timer(int fd) {
    if(ioctl(fd, RTCIOCSTOP, NULL) < 0) {
        ERRORMSG("ioctl(RTCIOCSTOP) failed - %s\n", strerror(errno));
        return;
    }
    if(ioctl(fd, IOCTLSIGATTACH, 0) < 0) {
        ERRORMSG("ioctl(IOCTLSIGATTACH) failed - %s\n", strerror(errno));
        return;
    }
    close(fd);
}
#endif

static inline void display_siginfo(siginfo_t *siginfo) {
    printf("%8s%-16s%d\n%8s%-16s%d\n%8s%-16s%d\n%8s%-16s%d\n"
           "%8s%-16s%p\n%8s%-16s%d\n%8s%-16s%d\n\n",
            "", "si_signo", siginfo->si_signo,
            "", "si_code", siginfo->si_code,
            "", "si_pid", siginfo->si_pid,
            "", "si_uid", siginfo->si_uid,
            "", "si_addr", siginfo->si_addr,
            "", "si_status", siginfo->si_status,
            "", "si_value", siginfo->si_value.sival_int);
}

static inline void init_curses(void) {
    initscr();
    cbreak();
    noecho();
    nonl();
    curs_set(0);
    timeout(1);
}

static inline void finish_curses(void) {
    clear();
    curs_set(0);
    echo();
    nl();
    nocbreak();
    nocbreak();
    endwin();
}

#if 0 /* for complie test ex. $ gcc rttest.c -o rttest */
int main(void) {
    printf("Hello World!\n");
    return 0;
}
#endif

#ifdef __cplusplus
}
#endif

#endif // RTTEST_H
