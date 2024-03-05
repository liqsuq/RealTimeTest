/*
 *               timer->t0[i]--------------+---------------+----------------+
 * send client->server        Send Time    |               |                |
 *                      t1[i]--------------+               |                |
 *  nanosleep (100 us)        nanosleep    | Response Time |                |
 *                      t2[i]--------------+               | Timer Interval |
 * recv client<-server        Receive Time |               |                |
 *                      t3[i]------------------------------+                |
 *                                                                          |
 *               timer->t0[i]-----------------------------------------------+
 *                        :
 */

#define _GNU_SOURCE  /* POLLRDHUP */
#include "rttest.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <poll.h>
#include <sched.h>
#include <math.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <netinet/ip.h>

#define BINNAME "pdl"
#define COUNT 10000
#define SERV_ADDR "127.0.0.1"
#define SERV_PORT "20000"
#define MAX_PACKET 65536
#define N 65536
#define PRIO_SERVER 51
#define PRIO_CLIENT 51

typedef struct {
    double r;
    double i;
} complex;

/* option flag */
#define NONE   0x00000000
#define SERVER 0x00000001
#define CLIENT 0x00000002
#define DEBUG  0x00000010
#define ASYNC  0x00000020
int optflg = SERVER|CLIENT;

/* 
 * チェビシェフ多項式と三角関数の対称性を利用してn次のsin配列を作る
 * 配列サイズをn+n/4とすることで位相をずらしてcos配列を求められる
 */
void make_sintbl(int n, double *sintbl) {
    int n2 = n/2, n4 = n/4, n8 = n/8;
    double c, s, dc, ds, t;

    /*
     * t = sin(theta/n), dc = 2sin^2(theta/2) = 2*t^2, ds = sqrt(dc*(2-dc))
     * C_0 = 1, C_n+1 = C_n - sigma(1, n, (2*dc*C_i - dc))
     * S_0 = 0, S_n+1 = S_n + sigma(1, n, (2*dc*S_i - ds))
     */
    t = sin(M_PI / n);
    dc = 2 * t * t; ds = sqrt(dc*(2-dc));
    t = 2 * dc;
    /* 0 <= theta <= pi/4 */
    c = sintbl[n4] = 1; s = sintbl[0] = 0;
    for(int i=1; i<n8; i++) {
        c -= dc; dc += t * c;
        s += ds; ds -= t * s;
        sintbl[n4-i] = c; sintbl[i] = s;
    }
    if(n8 != 0)
        sintbl[n8] = sqrt(0.5);
    /* pi/4 <= theta <= pi/2 */
    for(int i=0; i<n4; i++)
        sintbl[n2-i] = sintbl[i];
    /* pi/2 <= theta <= pi + pi/4 */
    for(int i=0; i<n2+n4; i++)
        sintbl[n2+i] = -sintbl[i];
}

/*
 * 配列[i]に対してiのビット順逆転対jを求める
 * (ex: i=0b011001 -> j=0b100110 )
 */
void make_bitrev(int n, int *bitrev) {
    int j = 0;
    for(int i=0; i<n; i++) {
        bitrev[i] = j;
        for(int k=n>>1; k>(j^=k); k>>=1);
    }
}

/*
 * 高速フーリエ変換
 * nが前回の値と異なる場合テーブルの再生成を行う
 * n = 0でテーブルをメモリ解放しn < 0なら逆変換を行う
 */
int fft(int n, complex *v) {
    static int last_n = 0;
    static double *sintbl = NULL;
    static int *bitrev = NULL;
    int ik, h, d, k2, n4, inverse = 0;
    double s, c, dx, dy;
    complex t;

    if(n < 0) {
        n = -n;
        inverse = 1;
    }
    n4 = n / 4;
    if(n != last_n || n == 0) {  /* テーブル作成/解放 */
        last_n = n;
        free(sintbl);
        sintbl = NULL;
        free(bitrev);
        bitrev = NULL;
        if(n == 0)
            return 0;
        sintbl = malloc((n + n4) * sizeof(double));
        bitrev = malloc(n * sizeof(int));
        if(sintbl == NULL || bitrev == NULL) {
            ERRORMSG("%s\n", strerror(errno));
            return 1;
        }
        make_sintbl(n, sintbl);
        make_bitrev(n, bitrev);
    }
    for(int i=0; i<n; i++) {  /* ビット反転 */
        int j = bitrev[i];
        if(i<j){
            t = v[i]; v[i] = v[j]; v[j] = t;
        }
    }
    for(int k=1; k<n; k=k2) {  /* 変換 */
        h = 0; k2 = k + k; d = n / k2;
        for(int j=0; j<k; j++) {
            c = sintbl[h+n4];
            if(inverse)
                s = -sintbl[h];
            else
                s =  sintbl[h];
            for(int i=j; i<n; i+=k2) {
                ik = i + k;
                dx = s * v[ik].i + c * v[ik].r;
                dy = c * v[ik].i - s * v[ik].r;
                v[ik].r = v[i].r - dx; v[i].r += dx;
                v[ik].i = v[i].i - dy; v[i].i += dy;
            }
            h += d;
        }
    }
    if(!inverse) {  /* 逆変換でないならnで割る */
        for(int i=0; i<n; i++) {
            v[i].r /= n; v[i].i /= n;
        }
    }
    return 0;
}

static inline void calc(timespec *t0, timespec *t1, char *tag, int target) {
    double diff[COUNT], min = 999999999.0, max = 0, avr = 0, sd = 0;

    for(int i=0; i<COUNT-1; i++)
        adjust(&t0[i], &t1[i], &diff[i]);
    stats(diff, COUNT-1, target, &min, &avr, &max, &sd);

    printf("%-18s min %7.1f us   avr %7.1f us   max %7.1f us   SD %6.1f\n",
            tag, min, avr, max, sd);
}

int server(char *port) {
    int sockfd, acptfd, err;
    struct addrinfo hint, *addr;
    const timespec ts_100us = {0, 100000};

    set_sched(SCHED_FIFO, PRIO_SERVER, 1);

    memset(&hint, 0, sizeof(hint));
    hint.ai_flags = AF_INET;
    hint.ai_socktype = SOCK_STREAM;
    hint.ai_flags = AI_PASSIVE;
    if((err = getaddrinfo(NULL, port, &hint, &addr)) != 0) {
        ERRORMSG("[SERVER] getaddrinfo() failed - %s\n", gai_strerror(err));
        goto ABORT;
    }
    sockfd = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
    if(sockfd < 0) {
        ERRORMSG("[SERVER] socket() failed - %s\n", strerror(errno));
        goto ABORT;
    }
    if(bind(sockfd, addr->ai_addr, addr->ai_addrlen) < 0) {
        ERRORMSG("[SERVER] bind() failed - %s\n", strerror(errno));
        goto ABORT_SOCKFD;
    }
    freeaddrinfo(addr);
    if(listen(sockfd, 1) < 0) {
        ERRORMSG("[SERVER] listen() failed - %s\n", strerror(errno));
        goto ABORT_SOCKFD;
    }

    while(1) { /* 接続->通信->切断->接続待ちを繰り返す */
        unsigned char buff[MAX_PACKET];
        timespec t1t2;
        struct pollfd fds[1];
        nfds_t nfds = 1;

        acptfd = accept(sockfd, NULL, NULL);
        if(acptfd < 0) {
            ERRORMSG("[SERVER] accept() failed - %s\n", strerror(errno));
            goto ABORT_SOCKFD;
        }

        fds[0].fd = acptfd;
        fds[0].events = POLLIN|POLLRDHUP;

        while(1) { /* 接続後の処理 */
            if(poll(fds, nfds, -1) > 0) {
                if(fds[0].revents & (POLLRDHUP|POLLHUP))
                    break;
                if(read(acptfd, buff, sizeof(buff)) < 0) {
                    ERRORMSG("[SERVER] read() failed - %s\n", strerror(errno));
                    goto ABORT_ACPTFD;
                }
                readtime(&t1t2);
                memcpy(buff, &t1t2, sizeof(timespec));
                nanosleep(&ts_100us, NULL);
                readtime(&t1t2);
                /* buffに2つのtimespecを並べて格納 */
                memcpy(&buff[sizeof(timespec)], &t1t2, sizeof(timespec));
                if(write(acptfd, buff, 2*sizeof(timespec)) < 0) {
                    ERRORMSG("[SERVER] write() failed - %s\n", strerror(errno));
                    goto ABORT_ACPTFD;
                }
                //sched_yield();
            }
        }
        if(optflg & DEBUG)
            DEBUGMSG("connection closed\n");
        close(acptfd);
    }

    close(sockfd);
    if(optflg & DEBUG)
        DEBUGMSG("process %d exit\n", getpid());
    return 0;

ABORT_ACPTFD:
    close(acptfd);
ABORT_SOCKFD:
    close(sockfd);
ABORT:
    if(optflg & DEBUG)
        DEBUGMSG("process %d exit\n", getpid());
    return -1;
}

int client(char *host, char *port, int freq) {
    int sockfd;
    struct addrinfo hint, *addr;
    unsigned char buff[MAX_PACKET];
    int err, signum;
    sigset_t sigset;
    timer_t timer_id;
    timespec t0[COUNT], t1[COUNT], t2[COUNT], t3[COUNT];
    timespec ts_1s = {1, 0};

    nanosleep(&ts_1s, NULL);

    set_sched(SCHED_FIFO, PRIO_CLIENT, 1);

    /* block SIGRTMIN for timer signal handling */
    sigemptyset(&sigset);
    sigaddset(&sigset, SIGRTMIN);
    if(sigprocmask(SIG_BLOCK, &sigset, NULL) == -1) {
        ERRORMSG("%s\n", strerror(errno));
        goto ABORT;
    }

    memset(&hint, 0, sizeof(hint));
    hint.ai_flags = AF_INET;
    hint.ai_socktype = SOCK_STREAM;
    if((err = getaddrinfo(host, port, &hint, &addr)) != 0) {
        ERRORMSG("%s\n", gai_strerror(err));
        goto ABORT;
    }
    sockfd = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
    if(sockfd < 0) {
        ERRORMSG("%s\n", strerror(errno));
        goto ABORT;
    }
    int yes = 1;
    if(setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(yes)) < 0) {
        ERRORMSG("%s\n", strerror(errno));
        goto ABORT_SOCKFD;
    }
    if(connect(sockfd, addr->ai_addr, addr->ai_addrlen) < 0) {
        ERRORMSG("%s\n", strerror(errno));
        goto ABORT_SOCKFD;
    }
    freeaddrinfo(addr);

    if(optflg & ASYNC)
        fcntl(sockfd, F_SETFL, O_ASYNC);

    if(freq == 1)
        timer_id = create_posix_timer(SIGRTMIN, 1, 0);
    else
        timer_id = create_posix_timer(SIGRTMIN, 0, (1000*1000*1000)/freq);
    if(timer_id < 0) {
        ERRORMSG("%s\n", strerror(errno));
        goto ABORT_SOCKFD;
    }

    sigwait(&sigset, &signum);
    for(int i=0; i<COUNT; i++) {
        sigwait(&sigset, &signum);
        readtime(&t0[i]);
        memset(buff, 0, sizeof(timespec));
        if(write(sockfd, buff, sizeof(timespec)) < 0)
            ERRORMSG("%s\n", strerror(errno));
        sched_yield();
        if(read(sockfd, buff, 2*sizeof(timespec)) < 0)
            ERRORMSG("%s\n", strerror(errno));
        readtime(&t3[i]);
        memcpy(&t1[i], &buff[0], sizeof(timespec));
        memcpy(&t2[i], &buff[sizeof(timespec)], sizeof(timespec));
    }

    timer_delete(timer_id);

    calc(&t0[0], &t0[1], "(Timer Interval)", 1000000.0 / freq);
    calc(&t0[0], &t1[0], "Send Time", 0);
    calc(&t1[0], &t2[0], "nanosleep(100us)", 100);
    calc(&t2[0], &t3[0], "Receive Time", 0);
    calc(&t0[0], &t3[0], "Response Time", 0);

    /* total jitter means (Response Time - nanosleep) / 2 *
     *                  = (Send Time + Receive Time)  / 2 */
    double send[COUNT], recv[COUNT], total[COUNT], min, avr, max, sd;
    for(int i=0; i<COUNT-1; i++) {
        adjust(&t0[i], &t1[i], &send[i]);
        adjust(&t2[i], &t3[i], &recv[i]);
        total[i] = send[i] + recv[i];
        total[i] /= 2;
    }

    stats(total, COUNT-1, 0, &min, &avr, &max, &sd);
    printf("%-18s min %7.1f us   avr %7.1f us   max %7.1f us   SD %6.1f\n",
            "Total Jitter", min, avr, max, sd);
    printf("Total Jitter Range is %s (fabs(%.1f) <= %d us)\n",
            (fabs(max-min) <= MAXJITTER)?"GOOD":"FAIL", max-min, MAXJITTER);

    close(sockfd);
    if(optflg & DEBUG)
        DEBUGMSG("process %d exit\n", getpid());
    return 0;

ABORT_SOCKFD:
    close(sockfd);
ABORT:
    if(optflg &DEBUG)
        DEBUGMSG("process %d exit\n", getpid());
    return 1;
}

int load(int no) {
    char fname[256];
    FILE *fp = NULL;
    complex c1[N], c2[N];

    set_sched(SCHED_RR, sched_get_priority_min(SCHED_RR), 1);
    sprintf(fname, "/tmp/rttest-pdl%d", no);
    unlink(fname);
    fp = fopen(fname, "w");
    while(1) {
        for(int i=0; i<N; i++) {
            c1[i].r = c2[i].r = 6*cos(6*M_PI*i / N) + 4*sin(18*M_PI*i / N);
            c1[i].i = c2[i].i = 0;
        }
        fft(N, c2);
        fft(-N, c2);
        for(int i=0; i<N; i++) {
            if(fp)
                fprintf(fp, "(%f,%f)\n", c1[i].r, c1[i].i);
        }
    }
    fft(0, NULL); /* メモリ解放 */
    if(fp)
        fclose(fp);
    return 0;
}

void pdl_test(char *host, char *port, int freq, int iter, int num_load) {
    pid_t pid_server = 0, pid_client = 0, pid_load[128];
    int status;

    title("pdl");

    for(int i=0; i<num_load; i++) {
        pid_load[i] = fork();
        if(pid_load[i] < 0) {
            ERRORMSG("fork() faild - %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }
        if(pid_load[i] == 0)
            exit(load(i));
    }
    if(optflg & SERVER) {
        pid_server = fork();
        if(pid_server < 0) {
            ERRORMSG("fork() faild - %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }
        if(pid_server == 0)
            exit(server(port));
    }
    if(optflg & CLIENT) {
        for(int i=0; i<iter; i++) {
            printf("********************************************* \n");
            printf("****  Num of Low-prio Load Process: %-4d **** \n",num_load);
            printf("****  Posix Timer Cycle %4d Hz          **** \n",freq);
            printf("********************************************* \n");
            pid_client = fork();
            if(pid_client < 0) {
                ERRORMSG("fork() faild - %s\n", strerror(errno));
                exit(EXIT_FAILURE);
            }
            if(pid_client == 0)
                exit(client(host, port, freq));
            waitpid(pid_client, &status, 0);
        }
        if(pid_server)
            kill(pid_server, SIGINT);
    }
    if(optflg & SERVER) {
        waitpid(pid_client, &status, 0);
    }
    for(int i=0; i<num_load; i++) {
        kill(pid_load[i], SIGINT);
        if(optflg & DEBUG)
            DEBUGMSG("send SIGINT to process %d\n", pid_load[i]);
        waitpid(pid_load[i], &status, 0);
    }

    return;
}

void version(void) {
    printf("%s ver. %s\n", BINNAME, VERSION);
}

void usage(void) {
    version();
    printf("\nUsage: %s [options]\n\n", BINNAME);
    printf("Options:\n");
    printf("    -s      : Run in server mode\n");
    printf("    -c addr : Run in client mode (default: 127.0.0.1)\n");
    printf("    -l num  : Run with load process (exec FFT)\n");
    printf("    -p port : Set port (default: 20000)\n");
    printf("    -f freq : Set timer frequency (default: 1000 Hz)\n");
    printf("    -i iter : Set test iteration (default: 1)\n");
    printf("    -a      : Enable O_ASYNC to sockets\n");
    printf("    -D      : Enable debug mode\n");
    printf("    -v      : Print version information and exit\n");
    printf("    -h      : Print this message and exit\n");
}

int main(int argc, char **argv) {
    int c;
    char host[256] = SERV_ADDR, port[256] = SERV_PORT;
    int freq = 1000, iter = 1, num_load = 0;


    while((c = getopt(argc, argv, "sc:l:p:f:i:aDhv")) != EOF) {
        switch(c) {
        case 's':
            optflg &= ~CLIENT;
            optflg |= SERVER;
            break;
        case 'c':
            optflg &= ~SERVER;
            optflg |= CLIENT;
            strcpy(host, optarg);
            printf("address %s\n",host);
            break;
        case 'l':
            num_load = atoi(optarg);
            break;
        case 'p':
            strcpy(port, optarg);
            printf("PORT %s\n", port);
            break;
        case 'f':
            freq = atoi(optarg);
            if(freq <= 0)
                freq = 1;
            if(freq >= 10000)
                freq = 10000;
            printf("CYCLE %d Hz\n", freq);
            break;
        case 'i':
            iter = atoi(optarg);
            break;
        case 'a':
            optflg |= ASYNC;
            break;
        case 'D':
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
    pdl_test(host, port, freq, iter, num_load);
    munlockall();
    return 0;
}
