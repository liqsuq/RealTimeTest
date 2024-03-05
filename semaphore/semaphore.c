#define _GNU_SOURCE
#include "rttest.h"
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>      /* O_* */
#include <semaphore.h>
#include <errno.h>
#include <string.h>     /* strerror() */
#include <unistd.h>     /* unlink() */

#define BINNAME "semaphore"

void semaphore_test(void) {
    const char *semname = "rttest_sem0";
    sem_t *sem;
    int semval, status;
    char buf[128];

    title("semaphore");

    /* initialize /dev/shm/sem.realtime-test_sem0 */
    if(sem_unlink(semname) != 0) {
        if(errno != ENOENT) {
            ERRORMSG("sem_unlink() failed - %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }
        errno = 0;
    }
    sem = sem_open(semname, O_CREAT|O_EXCL, 0644, 0);
    if(sem == (sem_t *)-1) {
        ERRORMSG("sem_open() failed - %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    /* get initial semaphore value */
    sem_getvalue(sem, &semval);
    printf("initial semaphore value is %-21d", semval);
    if(semval != 0) {
        printf(" ... NG\n");
        goto ERROR;
    }
    printf(" ... OK\n");

    /* test sem_post() */
    sem_post(sem);
    sem_getvalue(sem, &semval);
    printf("after sem_post semaphore value is %-14d", semval);
    if(semval != 1) {
        printf(" ... NG\n");
        goto ERROR;
    }
    printf(" ... OK\n");

    /* test sem_wait() */
    sem_wait(sem);
    sem_getvalue(sem, &semval);
    printf("after sem_wait semaphore value is %-14d", semval);
    if(semval != 0) {
        printf(" ... NG\n");
        goto ERROR;
    }
    printf(" ... OK\n");

    /* test sem_trywait() throwing EAGAIN */
    status = sem_trywait(sem);
    if(!status || (errno != EAGAIN)) {
        snprintf(buf, sizeof(buf), "%d %s", status, strerror(errno));
        printf("sem_trywait %-36s ... NG\n", buf);
        goto ERROR;
    }
    snprintf(buf, sizeof(buf), "%d %s", status, strerror(errno));
    printf("sem_trywait %-36s ... OK\n", buf);
    errno = 0;

    /* get semaphore value before count-up */
    sem_getvalue(sem, &semval);
    printf("current semaphore value is %-21d", semval);
    if(semval != 0) {
        printf(" ... NG\n");
        goto ERROR;
    }
    printf(" ... OK\n");

    /* count-up semaphore and get value */
    sem_post(sem);
    sem_getvalue(sem, &semval);
    printf("current semaphore value is %-21d",semval);
    if(semval != 1) {
        printf(" ... NG\n");
        goto ERROR;
    }
    printf(" ... OK\n");

    /* test sem_trywait() success */
    status = sem_trywait(sem);
    if(status) {
        snprintf(buf, sizeof(buf), "%d \"%s\"", status, strerror(errno));
        printf("sem_trywait %-36s ... NG\n", buf);
        goto ERROR;
    }
    snprintf(buf, sizeof(buf), "%d \"%s\"", status, strerror(errno));
    printf("sem_trywait %-36s ... OK\n", buf);

    /* get semaphore value after sem_trywait() */
    sem_getvalue(sem, &semval);
    printf("current semaphore value is %-21d", semval);
    if(semval != 0) {
        printf(" ... NG\n");
        goto ERROR;
    }
    printf(" ... OK\n");

    sem_close(sem);
    sem_unlink(semname);
    exit(EXIT_SUCCESS);

ERROR:
    sem_close(sem);
    sem_unlink(semname);
    exit(EXIT_FAILURE);
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
    semaphore_test();
    return 0;
}
