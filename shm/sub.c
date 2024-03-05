#include "rttest.h"
#include "shm_common.h"
#include <stdio.h>
#include <sys/mman.h>

int main(void) {
    int shmid;
    shm_t *shm_p;

    if((shmid = get_shm(&shm_p)) < 0)
        exit(EXIT_FAILURE);
    printf("    View shared variables from sub program\n");
    printf("    INTEGER %d %d %d\n", shm_p->i, shm_p->j, shm_p->k);
    printf("    REAL    %f %f %f\n", shm_p->a, shm_p->b, shm_p->c);
    shm_p->i = 10, shm_p->j = 0, shm_p->k = -10;
    shm_p->a = 10.0, shm_p->b = 0.0, shm_p->c = -10.0;
    msync(shm_p, sizeof(shm_t), MS_SYNC);
    return 0;
}
