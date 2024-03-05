#define _GNU_SOURCE
#include "rttest.h"
#include "shm_common.h"
#include <stdio.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/wait.h>

#define BINNAME "shm"

void shm_test() {
    int shmid;
    shm_t *shm_p;
    pid_t pid;

    title("shm");

    if((shmid = get_shm(&shm_p)) < 0) {
        ERRORMSG("get_shm() failed - %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    printf("Assigned from main program\n"
           "shm.i =  0;\nshm.j =  1;\nshm.k = -1;\n"
           "shm.a =  0.0;\nshm.b =  1.0;\nshm.c = -1.0;\n");
    shm_p->i = 0, shm_p->j = 1, shm_p->k = -1;
    shm_p->a = 0.0, shm_p->b = 1.0, shm_p->c = -1.0;
    msync(shm_p, sizeof(shm_t), MS_SYNC);

    printf("Call sub program\n");
    if((pid = fork()) < 0) {
        ERRORMSG("%s\n", strerror(errno));
        exit(EXIT_FAILURE);
    } else if(pid == 0) {
        if(execl("sub", "sub", NULL) < 0) {
            ERRORMSG("%s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }
    } else {
        waitpid(pid, NULL, 0);
    }

    printf("Returned sub program\n");
    confirm();
    printf("Assigned from sub program\n"
           "shm.i =  %d;\nshm.j =  %d;\nshm.k = %d;\n"
           "shm.a =  %f;\nshm.b =  %f;\nshm.c = %f;\n",
            shm_p->i, shm_p->j, shm_p->k, shm_p->a, shm_p->b, shm_p->c);

    if(shm_p->i==10 && shm_p->j==0 && shm_p->k==-10 &&
       shm_p->a==10.0 && shm_p->b==0.0 && shm_p->c==-10.0)
        printf("shm test %39s ... OK\n", "");
    else
        printf("shm test %39s ... NG\n", "");

    del_shm(shmid, &shm_p);
    confirm();
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

    while((opt = getopt(argc, argv, "vh")) != EOF){
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
    mlockall_rt();
    shm_test();
    munlockall();
    return 0;
}
