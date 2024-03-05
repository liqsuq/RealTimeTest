#define _GNU_SOURCE
#include "rttest.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <pthread.h>
#include <errno.h>
#include <math.h>
#ifdef RCIM
#include <rcim.h>
#include <sys/ioctl.h>
#endif

#define BINNAME "tswitch"
#define CYCLE 1000
#define NUM_THREADS 3
#define REP_TEST 10000

/* option flag */
#define NONE      0x00000000
#define DEBUG     0x00000001
#define USE_RCIM  0x00000002
unsigned int optflg = NONE;

volatile sig_atomic_t loop_flag = 1, glob_cnt = 0;

int num_threads = 0;
pthread_cond_t cond[NUM_THREADS];
pthread_mutex_t mutex[NUM_THREADS]; /* for num_thread, cond[1], cond[2] */

double ctx_switch1[REP_TEST/2];
double ctx_switch2[REP_TEST/2];
double sig_interval[REP_TEST];
struct timespec ts_start = {0, 0}, ts_end = {0, 0}, ts_prev = {0, 0};

static inline void show_result(char *label, float min, float avr, float max) {
    printf("%-10s %10.3f : %10.3f : %10.3f", label, min, avr, max);
    if(fabs(max-min) <= MAXJITTER)
        printf(" GOOD (fabs(%.3f) <= %d us)\n", max-min, MAXJITTER);
    else
        printf(" FAIL (fabs(%.3f) >  %d us)\n", max-min, MAXJITTER);
}

void *thread_func0(void *arg) {
    sigset_t sigset;
    struct timespec ts_100ms = {0, 100*1000*1000};
    double min = DBL_MAX, avr = 0.0, max = 0.0;

    //set_sched_thread(SCHED_FIFO, 50, 1);

    pthread_mutex_lock(&mutex[0]);
    num_threads++;
    pthread_mutex_unlock(&mutex[0]);

    /* wait for all threads */
    for(int num; num<NUM_THREADS;) {
        pthread_mutex_lock(&mutex[0]);
        num = num_threads;
        pthread_mutex_unlock(&mutex[0]);
        nanosleep(&ts_100ms, NULL);
    }

    /* handling SIGRTMIN and switching other threads */
    sigemptyset(&sigset);
    sigaddset(&sigset, SIGRTMIN);
    while(glob_cnt < REP_TEST) {
        sigwaitinfo(&sigset, NULL);
        readtime(&ts_start);
        if(glob_cnt > 0)
            adjust(&ts_prev, &ts_start, &sig_interval[glob_cnt-1]);
        ts_prev.tv_sec = ts_start.tv_sec;
        ts_prev.tv_nsec = ts_start.tv_nsec;
        if(glob_cnt % 2 == 0) { /* switch to thread1 */
            pthread_mutex_lock(&mutex[1]);
            pthread_cond_signal(&cond[1]);
            pthread_mutex_unlock(&mutex[1]);
        } else {                /* switch to thread2 */
            pthread_mutex_lock(&mutex[2]);
            pthread_cond_signal(&cond[2]);
            pthread_mutex_unlock(&mutex[2]);
        }
        glob_cnt++;
    }
    loop_flag = 0;

    pthread_mutex_lock(&mutex[1]);
    pthread_cond_signal(&cond[1]);
    pthread_mutex_unlock(&mutex[1]);
    pthread_mutex_lock(&mutex[2]);
    pthread_cond_signal(&cond[2]);
    pthread_mutex_unlock(&mutex[2]);

    stats(ctx_switch1, REP_TEST/2, 0, &min, &avr, &max, NULL);
    show_result("Switch1", min, avr, max);

    stats(ctx_switch2, REP_TEST/2, 0, &min, &avr, &max, NULL);
    show_result("Switch2", min, avr, max);

    stats(sig_interval, REP_TEST-1, 0, &min, &avr, &max, NULL);
    show_result("Interval", min, avr, max);

    if(optflg & DEBUG) {
        printf("Switch1: ");
        for(int i=0; i<REP_TEST/2; i++)
            printf("%.3f ", ctx_switch1[i]);
        printf("\n");
        printf("Switch2: ");
        for(int i=0; i<REP_TEST/2; i++)
            printf("%.3f ", ctx_switch2[i]);
        printf("\n");
        printf("Interval: ");
        for(int i=0; i<REP_TEST/2; i++)
            printf("%.3f ", sig_interval[i]);
        printf("\n");
    }

    return NULL;
}

void *thread_func1(void *arg) {
    //set_sched_thread(SCHED_FIFO, 51, 1);

    pthread_mutex_lock(&mutex[0]);
    num_threads++;
    pthread_mutex_unlock(&mutex[0]);

    do {
        pthread_mutex_lock(&mutex[1]); /* lock */
        pthread_cond_wait(&cond[1], &mutex[1]);
        readtime(&ts_end);
        if(glob_cnt < REP_TEST)
            adjust(&ts_start, &ts_end, &ctx_switch1[glob_cnt/2]);
        pthread_mutex_unlock(&mutex[1]); /* unlock */
    } while(loop_flag);

    return NULL;
}

void *thread_func2(void *arg) {
    //set_sched_thread(SCHED_FIFO, 51, 1);

    pthread_mutex_lock(&mutex[0]);
    num_threads++;
    pthread_mutex_unlock(&mutex[0]);

    do {
        pthread_mutex_lock(&mutex[2]); /* lock */
        pthread_cond_wait(&cond[2], &mutex[2]);
        readtime(&ts_end);
        if(glob_cnt < REP_TEST)
            adjust(&ts_start, &ts_end, &ctx_switch2[glob_cnt/2]);
        pthread_mutex_unlock(&mutex[2]); /* unlock */
    } while(loop_flag);

    return NULL;
}

void *(*tfunc[NUM_THREADS])(void *arg) = {
    thread_func0,
    thread_func1,
    thread_func2
};

void tswitch_test() {
    int targ[NUM_THREADS], err = 0;
    pthread_t tid[NUM_THREADS];
    sigset_t sigset;
    timer_t timer_id = NULL;
    #ifdef RCIM
    int rcim_fd = -1;
    #endif

    #ifdef RCIM
    if(optflg & USE_RCIM)
        title("rcim thread context switch test");
    else
    #endif
        title("posix thread context switch test");

    /* mask SIGRTMIN before setting up signal */
    sigemptyset(&sigset);
    sigaddset(&sigset, SIGRTMIN);
    if(sigprocmask(SIG_BLOCK, &sigset, NULL) == -1) {
        ERRORMSG("sigprocmask() failed - %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    /* set up timer emitting SIGRTMIN */
    #ifdef RCIM
    if(optflg & USE_RCIM) {
        printf("Create RCIM timer %d Hz\n", CYCLE);
        if((rcim_fd = create_rcim_timer(SIGRTMIN, 1000000/CYCLE)) < 0)
            exit(EXIT_FAILURE);
    } else {
    #endif
        printf("Create POSIX timer %d Hz\n", CYCLE);
        if((timer_id = create_posix_timer(SIGRTMIN, 0, 1000000000/CYCLE)) < 0)
            exit(EXIT_FAILURE);
    #ifdef RCIM
    }
    #endif

    printf("Create %d threads\n", NUM_THREADS);
    for(int i=0; i<NUM_THREADS; i++) {
        pthread_cond_init(&cond[i], NULL);
        pthread_mutex_init(&mutex[i], NULL);
    }
    for(int i=0; i<NUM_THREADS; i++) {
        targ[i] = i;
        if((err = pthread_create(&tid[i], NULL, tfunc[i], &targ[i]))) {
            ERRORMSG("pthread_create() failed - %s\n", strerror(err));
            exit(EXIT_FAILURE);
        }
    }

    for(int i=0; i<NUM_THREADS; i++)
        pthread_join(tid[i], NULL);

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
    printf("    -r : Use RCIM\n");
    #endif
    printf("    -d : Debug mode\n");
    printf("    -v : Print version information and exit\n");
    printf("    -h : Print this message and exit\n");
}

int main(int argc, char **argv) {
    int opt;

    #ifdef RCIM
    while((opt = getopt(argc, argv, "rdvh")) != EOF){
    #else
    while((opt = getopt(argc, argv, "dvh")) != EOF){
    #endif
        switch(opt) {
        #ifdef RCIM
        case 'r':
            optflg |= USE_RCIM;
            break;
        #endif
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
    mlockall_rt();
    tswitch_test();
    munlockall();
    return 0;
}
