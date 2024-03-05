force_root() {
    if [[ $(whoami) != root ]]; then
        echo "must run as the root user"
        exit 1
    fi
}

is_redhawk() {
    if [[ -f /etc/redhawk-release ]]; then
        return 0
    else
        return 1
    fi
}

is_ubuntu() {
    local dist=$(lsb_release -is)
    if [[ ${dist} == Ubuntu ]]; then
        return 0
    else
        return 1
    fi
}

is_centos() {
    local dist=$(lsb_release -is)
    if [[ ${dist} == CentOS ]]; then
        return 0
    else
        return 1
    fi
}

is_isolcpus() {
    cat /proc/cmdline|grep "isolcpus" > /dev/null
    if [[ $? != 0 ]]; then
        return 0
    else
        return 1
    fi
}

have_rcim() {
    cat /proc/interrupts|grep "rcim" > /dev/null
    if [[ $? != 0 ]]; then
        return 1
    else
        return 0
    fi
}

validate_policy() {
    if ! echo "fifo rr other batch idle"|grep -q ${POLICY}; then
        echo "${NAME}: invalid policy -- ${POLICY}"
        exit 1
    fi
}

warn_isolcpus() {
    if ! cat /proc/cmdline|grep -q "isolcpus"; then
        echo "WARNING: CPUs looks like not isolated"
        echo "WARNING: You should add the kernel parameter \"isolcpus=...\""
    fi
}

kill_rtkit() {
    if [[ $(type -P rtkitctl) ]]; then
        rtkitctl -k
    fi
}

set_ksoftirqd_rr() {
    ps -eo pid,cmd|grep [k]softirqd|awk '{print "chrt -r -p 98 "$1}'|bash
}

set_rcim_affinity() {
    RCIM=$(cat /proc/interrupts|grep rcim|awk '{print$1}'|awk -F: '{print$1}')
    AFFINITY=$(cat "/proc/irq/${RCIM}/smp_affinity")
    echo $(printf '%x' $((1 << ${BIAS}))) > "/proc/irq/${RCIM}/smp_affinity"
}

reset_rcim_affinity() {
    echo ${AFFINITY} > "/proc/irq/${RCIM}/smp_affinity"
}

