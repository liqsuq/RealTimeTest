#!/bin/bash -eu

NAME="loop"
SHIELD="1"
BIAS="1"
POLICY="fifo"
PRIO="51"
DEBUG=

cd $(dirname $0)
source ../.functions.sh
force_root

ARGS=$(getopt \
-o "a:b:s:P:d" \
-l "all:,bias:,policy:,priority:,debug" \
-n ${NAME} -- $*)
eval set -- "$ARGS"
while [[ $# -gt 0 ]]; do
    case "$1" in
        -a|--all) SHIELD="$2"; shift;;
        -b|--bias) BIAS="$2"; shift;;
        -s|--policy) POLICY="$2"; shift;;
        -P|--priority) PRIO="$2"; shift;;
        -d|--debug) DEBUG=1; shift;;
        --) shift; break;
    esac
    shift
done
validate_policy

kill_rtkit
set_ksoftirqd_rr
if is_redhawk; then
    shield -r -a ${SHIELD} -c
    echo "running CPU${BIAS}"
    if [[ ${DEBUG} ]]; then
        echo "run -b ${BIAS} -s ${POLICY} -P ${PRIO} ./${NAME}"
    fi
    run -b ${BIAS} -s ${POLICY} -P ${PRIO} ./${NAME} $*
    shield -r
else
    warn_isolcpus
    sysctl kernel.sched_rt_period_us=1000000 > /dev/null
    sysctl kernel.sched_rt_runtime_us=1000000 > /dev/null
    echo "running CPU${BIAS}"
    if [[ ${DEBUG} ]]; then
        echo "taskset -c ${BIAS} chrt --${POLICY} ${PRIO} ./${NAME}"
    fi
    taskset -c ${BIAS} chrt --${POLICY} ${PRIO} ./${NAME} $*
    sysctl kernel.sched_rt_runtime_us=950000 > /dev/null
fi
