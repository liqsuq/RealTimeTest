#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#define SHM_KEY 0x12345678
#define SHM_NAME "rttest-shm0"

typedef struct {
    int i, j, k;
    float a, b, c;
} shm_t;

#define POSIX

#ifdef POSIX
static inline int get_shm(shm_t **shm_pp) {
    int shmfd;
    const int prot = PROT_READ|PROT_WRITE;
    const int flags = MAP_SHARED|MAP_LOCKED;

    /* open shared memory */
    if((shmfd = shm_open(SHM_NAME, O_CREAT|O_RDWR, 0644)) < 0) {
        ERRORMSG("%s\n", strerror(errno));
        *shm_pp = NULL;
        return -1;
    }

    /* set size of the memory object */
    if(ftruncate(shmfd, sizeof(shm_t)) < 0) {
        ERRORMSG("%s\n", strerror(errno));
        close(shmfd);
        shm_unlink(SHM_NAME);
        *shm_pp = NULL;
        return -1;
    }

    /* map the memory object */
    *shm_pp = mmap(NULL, sizeof(shm_t), prot, flags, shmfd, 0);
    if(*shm_pp == MAP_FAILED) {
        ERRORMSG("%s\n", strerror(errno));
        close(shmfd);
        shm_unlink(SHM_NAME);
        *shm_pp = NULL;
        return -1;
    }
    return shmfd;
}
static inline void del_shm(int shmfd, shm_t **shm_pp) {
    if(munmap(*shm_pp, sizeof(shm_t)) < 0)
        ERRORMSG("%s\n", strerror(errno));
    if(close(shmfd) < 0)
        ERRORMSG("%s\n", strerror(errno));
    if(shm_unlink(SHM_NAME) < 0)
        ERRORMSG("%s\n", strerror(errno));
}
#else // SYSV
static inline int get_shm(shm_t **shm_pp) {
    int shmid;

    if((shmid = shmget(SHM_KEY, sizeof(shm_t), IPC_CREAT|SHM_R|SHM_W)) < 0) {
        ERRORMSG("%s\n", strerror(errno));
        return -1;
    }
    if((*shm_pp = shmat(shmid, NULL, 0)) < 0) {
        ERRORMSG("%s\n", strerror(errno));
        shmctl(shmid, IPC_RMID, NULL);
        return -1;
    }
    if(shmctl(shmid, SHM_LOCK, NULL) < 0)
        WARNMSG("共有メモリをロックできませんでした\n");
    return shmid;
}
static inline void del_shm(int shmid, shm_t **shm_pp) {
    if(shmctl(shmid, IPC_RMID, NULL) < 0)
        ERRORMSG("%s\n", strerror(errno));
    if(shmdt(*shm_pp) < 0)
        ERRORMSG("%s\n", strerror(errno));
}
#endif // POSIX

void confirm(void) {
#ifdef POSIX
    printf("\n------ 共有メモリオブジェクト ------\n");
    if(system("ls -l /dev/shm/") < 0)
        ERRORMSG("%s\n", strerror(errno));
    printf("\n");
#else
    if(system("ipcs -m") < 0)
        ERRORMSG("%s\n", strerror(errno));
#endif
}
