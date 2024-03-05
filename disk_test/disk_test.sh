#!/bin/bash -eu

NAME="disk_test"
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
        -s|--policy) POLICY="$2"; shift;;
        -P|--priority) PRIO="$2"; shift;;
        -d|--debug) DEBUG=1; shift;;
        --) shift; break;
    esac
    shift
done
validate_policy

if is_redhawk; then
     if [[ ${DEBUG} ]]; then
         echo "run -s ${POLICY} -P ${PRIO} ./${NAME}"
     fi
     run -s ${POLICY} -P ${PRIO} ./${NAME} $*
else
    sysctl kernel.sched_rt_period_us=1000000 > /dev/null
    sysctl kernel.sched_rt_runtime_us=1000000 > /dev/null
    if [[ ${DEBUG} ]]; then
        echo "chrt --${POLICY} ${PRIO} ./${NAME}"
    fi
    chrt --${POLICY} ${PRIO} ./${NAME} $*
    sysctl kernel.sched_rt_runtime_us=950000 > /dev/null
fi
