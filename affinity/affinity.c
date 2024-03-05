#define _GNU_SOURCE
#include "rttest.h"
#include <stdio.h>
#include <stdlib.h>
#ifdef REDHAWK
#include <mpadvise.h>
#else
#include <unistd.h>
#include <sched.h>
#endif

#define BINNAME "mpadvise"

/* option flag */
#define NONE  0x00000000
#define DEBUG 0x00000001
#define SCHED 0x00000002
unsigned int optflg = NONE;

#ifdef REDHAWK
void mpadvise_test(void) {
    cpuset_t *cpuset_p;
    char *cpustr;
    int cpu;
    cpu_t max;

    title("affinity test (using mpadvise)");

    cpuset_p = cpuset_alloc();

    /* test MPA_CPU_PRESENT */
    if(mpadvise(MPA_CPU_PRESENT, 0, 0, cpuset_p) != MPA_FAILURE) {
        cpustr = cpuset_get_string(cpuset_p);
        printf("MPA_CPU_PRESENT is %-29s ... OK\n", cpustr);
        cpuset_free(cpustr);
    } else {
        printf("MPA_CPU_PRESENT    %-29s ... NG\n", "");
        exit(EXIT_FAILURE);
    }

    /* test MPA_CPU_ACTIVE */
    if(mpadvise(MPA_CPU_ACTIVE, 0, 0, cpuset_p) != MPA_FAILURE) {
        cpustr = cpuset_get_string(cpuset_p);
        printf("MPA_CPU_ACTIVE is %-30s ... OK\n", cpustr);
        cpuset_free(cpustr);
    } else {
        printf("MPA_CPU_ACTIVE    %-30s ... NG\n", "");
        exit(EXIT_FAILURE);
    }

    /* test MPA_CPU_BOOT */
    if(mpadvise(MPA_CPU_BOOT, 0, 0, cpuset_p) != MPA_FAILURE) {
        cpustr = cpuset_get_string(cpuset_p);
        printf("MPA_CPU_BOOT is %-32s ... OK\n", cpustr);
        cpuset_free(cpustr);
    } else {
        printf("MPA_CPU_BOOT    %-32s ... NG\n", "");
        exit(EXIT_FAILURE);
    }

    /* test MPA_PRC_GETBIAS */
    if(mpadvise(MPA_PRC_GETBIAS, MPA_PID, 0, cpuset_p) != MPA_FAILURE) {
        cpustr = cpuset_get_string(cpuset_p);
        printf("MPA_PRC_GETBIAS is %-29s ... OK\n", cpustr);
        cpuset_free(cpustr);
    } else {
        printf("MPA_PRC_GETBIAS    %-29s ... NG\n", "");
        exit(EXIT_FAILURE);
    }

    /* get current running CPU */
    if(mpadvise(MPA_PRC_GETRUN, MPA_PID, 0, cpuset_p) != MPA_FAILURE) {
        cpustr = cpuset_get_string(cpuset_p);
        printf("MPA_PRC_GETRUN is %-30s ... OK\n", cpustr);
        cpuset_free(cpustr);
    } else {
        printf("MPA_PRC_GETRUN    %-30s ... NG\n", "");
        exit(EXIT_FAILURE);
    }

    /* test forcing process to bind specific CPU (zero to max)*/
    max = cpuset_max_cpu();
    for(cpu=cpuset_min_cpu(); cpu<=max; cpu++) {
        cpuset_init(cpuset_p);  /* clear */
        cpuset_set_cpu(cpuset_p, cpu, 1);  /* 1: set */
        if(mpadvise(MPA_PRC_SETBIAS, MPA_PID, 0, cpuset_p) != MPA_FAILURE) {
            printf("MPA_PRC_SETBIAS %-32d ... OK\n", cpu);
        } else {
            printf("MPA_PRC_SETBIAS %-32d ... NG\n", cpu);
            exit(EXIT_FAILURE);
        }

        if(mpadvise(MPA_PRC_GETRUN, MPA_PID, 0, cpuset_p) != MPA_FAILURE) {
            cpustr = cpuset_get_string(cpuset_p);
            printf("MPA_PRC_GETRUN is %-30s ... OK\n", cpustr);
            cpuset_free(cpustr);
        } else {
            printf("MPA_PRC_GETRUN    %-30s ... NG\n", cpustr);
            exit(EXIT_FAILURE);
        }
    }
    cpuset_free(cpuset_p);
}
#endif /* REDHAWK */

void cpuset_test() {
    cpu_set_t cpu_set;
    int cpu_cur;
    int cpu_num;

    title("affinity test (using sched)");

    if((cpu_num = sysconf(_SC_NPROCESSORS_CONF)) >= 0) {
        printf("_SC_NPROCESSORS_CONF is %-24d ... OK\n", cpu_num);
    } else {
        printf("_SC_NPROCESSORS_CONF    %-24s ... NG\n", "");
        exit(EXIT_FAILURE);
    }

    if((cpu_num = sysconf(_SC_NPROCESSORS_ONLN)) >= 0) {
        printf("_SC_NPROCESSORS_ONLN is %-24d ... OK\n", cpu_num);
    } else {
        printf("_SC_NPROCESSORS_ONLN is %-24s ... NG\n", "");
        exit(EXIT_FAILURE);
    }

    if((cpu_cur = sched_getcpu()) >= 0) {
        printf("sched_getcpu() is %-30d ... OK\n", cpu_cur);
    } else {
        printf("sched_getcpu()    %-30s ... NG\n", "");
        ERRORMSG("asd - %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    CPU_ZERO(&cpu_set);
    if(sched_getaffinity(0, sizeof(cpu_set_t), &cpu_set) >= 0) {
        unsigned long long cpu_mask = 0;
        for(int i=0; i<cpu_num; i++)
            if(CPU_ISSET(i, &cpu_set))
                cpu_mask += 0x01 << i;
        printf("sched_getaffinity() is %-25llx ... OK\n", cpu_mask);
    } else {
        printf("sched_getaffinity()    %-25s ... NG\n", "");
        exit(EXIT_FAILURE);
    }

    for(int i=0; i<cpu_num; i++) {
        CPU_ZERO(&cpu_set);
        CPU_SET(i, &cpu_set);
        if(sched_setaffinity(0, sizeof(cpu_set_t), &cpu_set) >= 0) {
            printf("sched_setaffinity() %-28d ... OK\n", i);
        } else {
            printf("sched_setaffinity() %-28d ... NG\n", i);
            exit(EXIT_FAILURE);
        }
        CPU_ZERO(&cpu_set);
        if(sched_getaffinity(0, sizeof(cpu_set_t), &cpu_set) >= 0) {
            unsigned long long cpu_mask = 0;
            for(int j=0; j<cpu_num; j++)
                if(CPU_ISSET(j, &cpu_set))
                    cpu_mask += 0x01 << j;
            if(CPU_ISSET(i,&cpu_set)) {
                printf("sched_getaffinity() is %-25llx ... OK\n", cpu_mask);
            } else {
                printf("sched_getaffinity() is %-25llx ... NG\n", cpu_mask);
            }
        } else {
            printf("sched_getaffinity()    %-25s ... NG\n", "");
            exit(EXIT_FAILURE);
        }
    }

}

void version(void) {
    printf("%s ver. %s\n", BINNAME, VERSION);
}

void usage(void) {
    version();
    printf("\nUsage: %s [options]\n\n", BINNAME);
    printf("Options:\n");
    #ifdef REDHAWK
    printf("    -s : Use sched_*() instead of mpadvise()\n");
    #endif
    printf("    -v : Print version information and exit\n");
    printf("    -h : Print this message and exit\n");
}

int main(int argc, char **argv) {
    int opt;
    #ifdef REDHAWK
    while((opt = getopt(argc, argv, "svh")) != EOF) {
    #else
    while((opt = getopt(argc, argv, "vh")) != EOF) {
    #endif
        switch(opt) {
        #ifdef REDHAWK
        case 's':
            optflg |= SCHED;
            break;
        #endif
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

    #ifdef REDHAWK
    if(optflg & SCHED)
        cpuset_test();
    else
        mpadvise_test();
    #else
    cpuset_test();
    #endif
    return 0;
}
