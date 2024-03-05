#define _GNU_SOURECE
#include "rttest.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <mqueue.h>
#include <signal.h>
#include <errno.h>
#include <string.h>

#define BINNAME "msgqueue"
#define MQ_NAME "/rttest-mq0"

/* option flag */
#define NONE  0x00000000
#define DEBUG 0x00000001
unsigned int optflg = NONE;

mqd_t fd_mq;

void receive_mq(int sig) {
    int msg;
    unsigned int msg_prio;
    struct sigevent sigev;
    static int no = 0;

    /* 通知は一度だけ行われる。mq_notify() を再度呼び出すのは読み出していない
     * メッセージを全部読み出してキューが空になる前にすべきである。*/
    sigev.sigev_notify = SIGEV_SIGNAL;
    sigev.sigev_signo = SIGRTMIN;
    mq_notify(fd_mq, &sigev);

    mq_receive(fd_mq, (char *)&msg, sizeof(int), &msg_prio);
    printf("from signal handler mq_receive data %d priority %d", msg, msg_prio);
    if((msg == no) && (msg_prio == no))
        printf(" ... OK\n");
    else
        printf(" ... NG\n");
    no++;
}

void msgqueue_test() {
    struct mq_attr attr;
    int msg, err = 0;
    unsigned int msg_prio;
    struct sigaction sigact;
    struct sigevent sigev;
    const struct timespec ts_100us = {0, 100*1000};
    const int oflag = O_CREAT|O_RDWR|O_NONBLOCK;

    title("msgqueue test");
    printf("Message Queue Polling Test\n");

    attr.mq_maxmsg = 10;
    attr.mq_msgsize = sizeof(int);
    if((fd_mq = mq_open(MQ_NAME, oflag, 0666, &attr)) == (mqd_t)(-1)) {
        ERRORMSG("mq_open() failed - %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    printf("send message queue    : ");
    for(int i=0; i<10; i++) {
        msg = i;
        if(mq_send(fd_mq, (char *)&msg, sizeof(int), i) < 0) {
            ERRORMSG("mq_send() failed - %s\n", strerror(errno));
            goto ABORT;
        }
        printf("%d ", msg);
    }
    printf("\n");

    if(optflg & DEBUG)
        press_any_key();

    printf("receive message queue : ");
    for(int i=0; i<10; i++) {
        mq_receive(fd_mq, (char *)&msg, sizeof(int), &msg_prio);
        if((msg_prio != 9-i) || (msg != 9-i)) {
            err++;
        }
        printf("%x:%x ", msg_prio, msg);
    }
    if(err == 0)
        printf(" ... OK\n");
    else
        printf(" ... NG\n");

    printf("Message Queue Interrupt Test\n");
    memset(&sigact, 0, sizeof(sigact));
    sigact.sa_handler = receive_mq;
    sigact.sa_flags = SA_RESTART;
    if(sigaction(SIGRTMIN, &sigact, NULL) == -1) {
        ERRORMSG("sigaction() failed - %s\n", strerror(errno));
        goto ABORT;
    }
    sigev.sigev_notify = SIGEV_SIGNAL;
    sigev.sigev_signo = SIGRTMIN;
    mq_notify(fd_mq, &sigev);

    if(optflg & DEBUG)
        press_any_key();

    for(int i=0; i<10; i++) {
        printf("mq_send data = %d priority = %d \n", i, i);
        msg = i;
        mq_send(fd_mq, (char *)&msg, sizeof(int), i);
        nanosleep(&ts_100us, NULL);
    }

    mq_close(fd_mq);
    mq_unlink(MQ_NAME);
    exit(EXIT_SUCCESS);
ABORT:
    mq_close(fd_mq);
    mq_unlink(MQ_NAME);
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
    msgqueue_test();
    return 0;
}
