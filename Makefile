SUBDIRS := affinity sched semaphore shm_open shm sigqueue msgqueue aio rfile loop timer nanosleep tswitch pdl memtest disk_test timer_loop clock
TESTD := affinity sched semaphore shm_open shm sigqueue msgqueue aio rfile loop timer nanosleep tswitch pdl memtest disk_test
RCIMD := timer tswitch

REDHAWK := $(shell if [ -f /etc/redhawk-release ]; then echo 1; else echo 0; fi)
VERSION := $(shell fgrep "VERSION" include/rttest.h|cut -d' ' -f3)

.PHONY: all test rcim clean spec $(SUBDIRS)

all: $(SUBDIRS)
test: spec $(TESTD)
rcim: spec $(RCIMD)
clean: $(SUBDIRS)
spec:
	@echo "##### System Info #####"
	@echo "uname -r:"
	@uname -r
	@echo ""
	@echo "/proc/cmdline:"
	@cat /proc/cmdline
	@echo ""
ifeq ("$(REDHAWK)", "1")
	@echo "gather:"
	@gather
	@echo ""
endif
	@echo "RealTimeTest:"
	@echo "version $(VERSION)"
	@echo ""
$(SUBDIRS):
	$(MAKE) -s -C $@ $(MAKECMDGOALS)
