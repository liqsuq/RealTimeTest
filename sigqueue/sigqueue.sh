#!/bin/bash -eu

NAME="sigqueue"

cd $(dirname $0)
source ../.functions.sh

ARGS=$(getopt -o "" -l "" -n ${NAME} -- $*)
eval set -- "$ARGS"
while [[ $# -gt 0 ]]; do
    case "$1" in
        --) shift; break;
    esac
    shift
done

./${NAME} $*
