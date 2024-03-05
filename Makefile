BUILD := affinity sched semaphore shm_open shm sigqueue msgqueue aio rfile loop timer nanosleep tswitch pdl memtest disk_test timer_loop clock
TESTD := affinity sched semaphore shm_open shm sigqueue msgqueue aio rfile loop timer nanosleep tswitch pdl memtest disk_test
RCIMD := timer tswitch

REDHAWK := $(shell if [ -f /etc/redhawk-release ]; then echo 1; else echo 0; fi)
VERSION := $(shell fgrep "VERSION" include/rttest.h|cut -d' ' -f3)

all:
	@echo "##### Making Files #####"
	@for i in $(BUILD); do echo "$$i"; $(MAKE) -s $@ -C $$i; done;
test:
	@echo "##### System Info #####"
	@echo ""
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
	@echo "RedHawkTest:"
	@echo "version $(VERSION)"
	@for i in $(TESTD); do echo ""; echo "##### $$i #####"; echo ""; $(MAKE) -s $@ -C $$i; done;
rcim:
	@for i in $(RCIMD); do echo ""; echo "##### $$i #####"; echo ""; $(MAKE) -s $@ -C $$i; done;
clean:
	@echo "##### Clean files #####"
	@for i in $(BUILD); do echo "$$i"; $(MAKE) -s $@ -C $$i; done;
