#define _GNU_SOURCE
#include "rttest.h"
#include <stdio.h>
#include <stdlib.h>
#include <sched.h>
#include <errno.h>
#include <string.h>

#define BINNAME "sched"

int sched_test(void) {
    int min, max;
    struct sched_param param;
    struct timespec ts_rr;
    double interval = 0.0;

    title("sched test");

    /* get minimum priority of SCHED_FIFO */
    min = sched_get_priority_min(SCHED_FIFO);
    if(min < 0) {
        printf("sched_get_priority_min(SCHED_FIFO) %13s ... NG - %s\n",
                "", strerror(errno));
        exit(EXIT_FAILURE);
    } else {
        printf("sched_get_priority_min(SCHED_FIFO) is %-10d ... OK\n", min);
    }

    /* get maximum priority of SCHED_FIFO */
    max = sched_get_priority_max(SCHED_FIFO);
    if(max < 0) {
        printf("sched_get_priority_max(SCHED_FIFO) %13s ... NG - %s\n",
                "", strerror(errno));
        exit(EXIT_FAILURE);
    } else {
        printf("sched_get_priority_max(SCHED_FIFO) is %-10d ... OK\n", max);
    }

    /* get minimum priority of SCHED_RR */
    min = sched_get_priority_min(SCHED_RR);
    if(min < 0) {
        printf("sched_get_priority_min(SCHED_RR) %15s ... NG - %s\n",
                "", strerror(errno));
        exit(EXIT_FAILURE);
    } else {
        printf("sched_get_priority_min(SCHED_RR) is %-12d ... OK\n",min);
    }

    /* get maximum priority of SCHED_RR */
    max = sched_get_priority_max(SCHED_RR);
    if(max < 0) {
        printf("sched_get_priority_max(SCHED_RR) %15s ... NG - %s\n",
                "", strerror(errno));
        exit(EXIT_FAILURE);
    } else {
        printf("sched_get_priority_max(SCHED_RR) is %-12d ... OK\n", max);
    }

    /* set SCHED_FIFO and maximum priority */
    param.sched_priority = max;
    if(sched_setscheduler(0, SCHED_FIFO, &param) != 0) {
        printf("sched_setscheduler(SCHED_FIFO) %17s ... NG - %s\n",
                "", strerror(errno));
        exit(EXIT_FAILURE);
    } else {
        printf("sched_setscheduler(SCHED_FIFO) %17s ... OK\n", "");
    }

    /* set SCHED_RR and minimum priority */
    param.sched_priority = min;
    if(sched_setscheduler(0, SCHED_RR, &param) < 0) {
        printf("sched_setscheduler(SCHED_RR) %19s ... NG - %s\n",
                "", strerror(errno));
        exit(EXIT_FAILURE);
    } else {
        printf("sched_setscheduler(SCHED_RR) %19s ... OK\n", "");
    }

    /* get interval of SCHED_RR */
    if(sched_rr_get_interval(0, &ts_rr) != 0) {
        printf("sched_rr_get_interval() %-24s ... NG - %s\n",
                "", strerror(errno));
        exit(EXIT_FAILURE);
    } else {
        interval += ts_rr.tv_sec + (double)ts_rr.tv_nsec / 1000000000;
        printf("sched_rr_get_interval() is %-21.9f ... OK\n", interval);
    }
    return 0;
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
    sched_test();
    return 0;
}
