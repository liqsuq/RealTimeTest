/*
 * Test1: Server/Client -> Parent
 * Test2:        Parent -> Server/Client
 * Test3:        Client -> Server
 * Test4:        Server -> Client
 */

#define _GNU_SOURCE
#include "rttest.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <time.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#define BINNAME "sigqueue"
#define LOOP 10

/* option flag */
#define NONE  0x00000000
#define DEBUG 0x00000001
#define PAUSE 0x00000002
unsigned int optflg = NONE;

const timespec ts_1sec = {1, 0};
volatile sig_atomic_t loop_flag;

void parent_sigrt_handler(int signo, siginfo_t *siginfo, void *ctx) {
    /* Test1 receive */
    loop_flag++;
    printf("Parent signal handler signo %d val %d\n",
            signo, siginfo->si_value.sival_int);
    if(optflg & DEBUG)
        display_siginfo(siginfo);
}

void server_sigrt_handler(int signo, siginfo_t *siginfo, void *ctx) {
    static int no = 0;

    /* Test3 receive */
    printf("Server signal handler signo %d val %-13d",
            signo, siginfo->si_value.sival_int);
    if((signo == SIGRTMIN) && (no == siginfo->si_value.sival_int))
        printf(" ... OK\n");
    else
        printf(" ... NG\n");
    if(optflg & DEBUG)
        display_siginfo(siginfo);
    no++;
}

void client_sigrt_handler(int signo, siginfo_t *siginfo, void *ctx) {
    static int no = LOOP;

    /* Test4 receive */
    printf("Client signal handler signo %d val %-13d",
            signo, siginfo->si_value.sival_int);
    if((signo == SIGRTMIN) && (no == siginfo->si_value.sival_int))
        printf(" ... OK\n");
    else
        printf(" ... NG\n");
    if(optflg & DEBUG)
        display_siginfo(siginfo);
    no--;
}

static int server(void) {
    struct sigaction sigact;
    union sigval sigvalue;
    sigset_t sigmask;
    siginfo_t siginfo;
    int signum, pid, pid_client = -1, ppid;

    pid = getpid();
    ppid = getppid();

    /* block SIGRTMIN: it is caught by sigwaitinfo() or sigsuspend() */
    sigemptyset(&sigmask);
    sigaddset(&sigmask, SIGRTMIN);
    if(sigprocmask(SIG_BLOCK, &sigmask, NULL) == -1) {
        perror("[Server] cannot set sigprocmask");
        exit(1);
    }

    /* register SIGRTMIN handler */
    memset(&sigact, 0, sizeof(sigact));
    sigact.sa_sigaction = server_sigrt_handler;
    sigact.sa_flags = SA_SIGINFO|SA_RESTART;
    if(sigaction(SIGRTMIN, &sigact, NULL) == -1) {
        perror("[Server] cannot set sigaction");
        exit(1);
    }

    nanosleep(&ts_1sec, NULL);

    /* Test1 send */
    printf("Step1: Server/Client -> Parent\n");
    fflush(stdout);
    printf("sigqueue Server #%d -> Parent #%d val %d\n", pid, ppid, pid);
    sigvalue.sival_int = pid;
    sigqueue(ppid, SIGRTMIN, sigvalue);

    /* Test2 receive */
    while(1) {
        signum = sigwaitinfo(&sigmask, &siginfo);
        if(signum == SIGRTMIN && siginfo.si_pid == getppid()) {
            printf("Server sigwaitinfo test %24s ... OK\n", "");
            pid_client = siginfo.si_int;
            break;
        } else {
            printf("Server sigwaitinfo test %24s ... NG"
                   " return code %d - %s\n", "", signum, strerror(errno));
        }
        if(optflg & DEBUG)
            display_siginfo(&siginfo);
    }

    /* wating Test3 finished */
    sigfillset(&sigmask);
    sigdelset(&sigmask, SIGRTMIN);
    sigdelset(&sigmask, SIGINT);
    for(int i=0; i<LOOP; i++)
        sigsuspend(&sigmask);

    if(optflg & PAUSE)
        press_any_key();

    /* Test4 send */
    printf("Step4: Server -> Client\n");
    fflush(stdout);
    for(int i=0; i<LOOP; i++) {
        sigvalue.sival_int = LOOP - i;
        sigqueue(pid_client, SIGRTMIN, sigvalue);
    }

    printf("Server Process exit\n");
    exit(0);
}

static int client(void) {
    struct sigaction sigact;
    union sigval sigvalue;
    sigset_t sigmask;
    siginfo_t siginfo;
    int signum, pid, pid_server = -1, ppid;

    pid = getpid();
    ppid = getppid();

    /* block SIGRTMIN: it is caught by sigwaitinfo() or sigsuspend() */
    sigemptyset(&sigmask);
    sigaddset(&sigmask, SIGRTMIN);
    if(sigprocmask(SIG_BLOCK, &sigmask, NULL) == -1) {
        perror("[Client] cannot set sigprocmask");
        exit(EXIT_FAILURE);
    }

    /* register SIGRTMIN handler */
    memset(&sigact, 0, sizeof(sigact));
    sigact.sa_sigaction = client_sigrt_handler;
    sigact.sa_flags = SA_SIGINFO|SA_RESTART;
    if(sigaction(SIGRTMIN, &sigact, NULL) == -1) {
        perror("[Client] cannot set sigaction");
        exit(EXIT_FAILURE);
    }

    nanosleep(&ts_1sec, NULL);

    /* Test1 send */
    printf("sigqueue Client #%d -> Parent #%d val %d\n", pid, ppid, pid);
    sigvalue.sival_int = pid;
    sigqueue(ppid, SIGRTMIN, sigvalue);

    /* Test2 receive */
    while(1) {
        signum = sigwaitinfo(&sigmask, &siginfo);
        if((signum == SIGRTMIN) && (siginfo.si_pid == getppid())) {
            printf("Client sigwaitinfo test %24s ... OK\n", "");
            pid_server = siginfo.si_int;
            break;
        } else {
            printf("Client sigwaitinfo test %24s ... NG"
                   " return code %d - %s\n", "", signum, strerror(errno));
        }
        if(optflg & DEBUG)
            display_siginfo(&siginfo);
    }

    if(optflg & PAUSE)
        press_any_key();

    /* Test3 send */
    printf("Step3: Client -> Server\n");
    fflush(stdout);
    for(int i=0; i<LOOP; i++) {
        sigvalue.sival_int = i;
        sigqueue(pid_server, SIGRTMIN, sigvalue);
    }

    /* waiting Test4 finished */
    sigfillset(&sigmask);
    sigdelset(&sigmask, SIGRTMIN);
    sigdelset(&sigmask, SIGINT);
    for(int i=0; i<LOOP; i++)
        sigsuspend(&sigmask);

    printf("Client Process exit\n");
    exit(EXIT_SUCCESS);
}

void sigqueue_test() {
    int status, err = 0;
    pid_t pid_server, pid_client;
    struct sigaction sigact;
    union sigval sigvalue;
    sigset_t sigmask;

    title("sigqueue test");
    printf("Parent PID #%d\n", getpid());

    /* register SIGRTMIN handler */
    memset(&sigact, 0, sizeof(sigact));
    sigact.sa_sigaction = parent_sigrt_handler;
    sigact.sa_flags = SA_SIGINFO|SA_RESTART;
    if(sigaction(SIGRTMIN, &sigact, 0) == -1) {
        perror("[Parent] cannot set sigaction");
        exit(EXIT_FAILURE);
    }

    /* fork to server process */
    pid_server = fork();
    if(pid_server < 0) {
        perror("[Parent] server fork error");
        exit(EXIT_FAILURE);
    }
    if(pid_server == 0)
        server();
    printf("Server PID #%d\n", pid_server);

    /* fork to client process */
    pid_client = fork();
    if(pid_client < 0) {
        perror("[Parent] client fork error");
        exit(EXIT_FAILURE);
    }
    if(pid_client == 0)
        client();
    printf("Client PID #%d\n", pid_client);

    /* waiting Test1 finished */
    sigemptyset(&sigmask);
    while(loop_flag < 2)
        sigsuspend(&sigmask);

    if(optflg & PAUSE)
        press_any_key();

    /* Test2 send */
    printf("Step2: Parent -> Server/Client\n");
    fflush(stdout);
    printf("sigqueue Parent #%d -> Server #%d val %d\n",
            getpid(), pid_server, pid_client);
    sigvalue.sival_int = pid_client;
    sigqueue(pid_server, SIGRTMIN, sigvalue);
    nanosleep(&ts_1sec, NULL);

    printf("sigqueue Parent #%d -> Client #%d val %d\n",
            getpid(), pid_client, pid_server);
    sigvalue.sival_int = pid_server;
    sigqueue(pid_client, SIGRTMIN, sigvalue);

    /* waiting Test3 and Test4 */
    waitpid(pid_server, &status, 0);
    err += status;
    waitpid(pid_client, &status, 0);
    err += status;
    printf("Parent Process exit\n");

    if(err) {
        printf("sigqueue test %34s ... Fail\n", "");
        exit(EXIT_FAILURE);
    } else {
        printf("sigqueue test %34s ... OK\n", "");
        exit(EXIT_SUCCESS);
    }
}

void version(void) {
    printf("%s ver. %s\n", BINNAME, VERSION);
}

void usage(void) {
    version();
    printf("\nUsage: %s [options]\n\n", BINNAME);
    printf("Options:\n");
    printf("    -p : Pause between test\n");
    printf("    -d : Debug mode\n");
    printf("    -v : Print version information and exit\n");
    printf("    -h : Print this message and exit\n");
}

int main(int argc, char **argv) {
    int opt;

    while((opt = getopt(argc, argv, "pdvh")) != EOF) {
        switch(opt) {
        case 'p':
            optflg |= PAUSE;
            break;
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
    sigqueue_test();
    return 0;
}
