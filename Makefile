SUBDIRS := affinity sched semaphore shm_open shm sigqueue msgqueue aio rfile loop timer nanosleep tswitch pdl memtest disk_test timer_loop clock
TESTD := affinity sched semaphore shm_open shm sigqueue msgqueue aio rfile loop timer nanosleep tswitch pdl memtest disk_test
RCIMD := timer tswitch

REDHAWK := $(shell if [ -f /etc/redhawk-release ]; then echo 1; else echo 0; fi)
VERSION := $(shell fgrep "VERSION" include/rttest.h|cut -d' ' -f3)

INSTDIR := /usr/local/CNC/RealTimeTest

.PHONY: all test rcim clean wipeout spec install rpm deb $(SUBDIRS)

all: $(SUBDIRS)
test: spec $(TESTD)
rcim: spec $(RCIMD)
clean: $(SUBDIRS)
wipeout: clean
	rm -rf deb rpm root

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

install: all
	install -d $(DESTDIR)$(INSTDIR)
	install -m 0644 Makefile $(DESTDIR)$(INSTDIR)/
	install -m 0644 README.md $(DESTDIR)$(INSTDIR)/
	install -m 0644 .functions.sh $(DESTDIR)$(INSTDIR)/
	install -d $(DESTDIR)$(INSTDIR)/include
	install -m 0644 include/rttest.h $(DESTDIR)$(INSTDIR)/include/
	install -d $(DESTDIR)$(INSTDIR)/affinity
	install -m 0644 affinity/Makefile $(DESTDIR)$(INSTDIR)/affinity/
	install -m 0644 affinity/affinity.c $(DESTDIR)$(INSTDIR)/affinity/
	install -m 0755 affinity/affinity.sh $(DESTDIR)$(INSTDIR)/affinity/
	install -d $(DESTDIR)$(INSTDIR)/sched
	install -m 0644 sched/Makefile $(DESTDIR)$(INSTDIR)/sched/
	install -m 0644 sched/sched.c $(DESTDIR)$(INSTDIR)/sched/
	install -m 0755 sched/sched.sh $(DESTDIR)$(INSTDIR)/sched/
	install -d $(DESTDIR)$(INSTDIR)/semaphore
	install -m 0644 semaphore/Makefile $(DESTDIR)$(INSTDIR)/semaphore/
	install -m 0644 semaphore/semaphore.c $(DESTDIR)$(INSTDIR)/semaphore/
	install -m 0755 semaphore/semaphore.sh $(DESTDIR)$(INSTDIR)/semaphore/
	install -d $(DESTDIR)$(INSTDIR)/shm_open
	install -m 0644 shm_open/Makefile $(DESTDIR)$(INSTDIR)/shm_open/
	install -m 0644 shm_open/shm_open.c $(DESTDIR)$(INSTDIR)/shm_open/
	install -m 0755 shm_open/shm_open.sh $(DESTDIR)$(INSTDIR)/shm_open/
	install -d $(DESTDIR)$(INSTDIR)/shm
	install -m 0644 shm/Makefile $(DESTDIR)$(INSTDIR)/shm/
	install -m 0644 shm/shm_common.h $(DESTDIR)$(INSTDIR)/shm/
	install -m 0644 shm/shm.c $(DESTDIR)$(INSTDIR)/shm/
	install -m 0644 shm/sub.c $(DESTDIR)$(INSTDIR)/shm/
	install -m 0755 shm/shm.sh $(DESTDIR)$(INSTDIR)/shm/
	install -d $(DESTDIR)$(INSTDIR)/sigqueue
	install -m 0644 sigqueue/Makefile $(DESTDIR)$(INSTDIR)/sigqueue/
	install -m 0644 sigqueue/sigqueue.c $(DESTDIR)$(INSTDIR)/sigqueue/
	install -m 0755 sigqueue/sigqueue.sh $(DESTDIR)$(INSTDIR)/sigqueue/
	install -d $(DESTDIR)$(INSTDIR)/msgqueue
	install -m 0644 msgqueue/Makefile $(DESTDIR)$(INSTDIR)/msgqueue/
	install -m 0644 msgqueue/msgqueue.c $(DESTDIR)$(INSTDIR)/msgqueue/
	install -m 0755 msgqueue/msgqueue.sh $(DESTDIR)$(INSTDIR)/msgqueue/
	install -d $(DESTDIR)$(INSTDIR)/aio
	install -m 0644 aio/Makefile $(DESTDIR)$(INSTDIR)/aio/
	install -m 0644 aio/aio.c $(DESTDIR)$(INSTDIR)/aio/
	install -m 0755 aio/aio.sh $(DESTDIR)$(INSTDIR)/aio/
	install -d $(DESTDIR)$(INSTDIR)/rfile
	install -m 0644 rfile/Makefile $(DESTDIR)$(INSTDIR)/rfile/
	install -m 0644 rfile/rfile.c $(DESTDIR)$(INSTDIR)/rfile/
	install -m 0755 rfile/rfile.sh $(DESTDIR)$(INSTDIR)/rfile/
	install -d $(DESTDIR)$(INSTDIR)/loop
	install -m 0644 loop/Makefile $(DESTDIR)$(INSTDIR)/loop/
	install -m 0644 loop/loop.c $(DESTDIR)$(INSTDIR)/loop/
	install -m 0755 loop/loop.sh $(DESTDIR)$(INSTDIR)/loop/
	install -d $(DESTDIR)$(INSTDIR)/timer
	install -m 0644 timer/Makefile $(DESTDIR)$(INSTDIR)/timer/
	install -m 0644 timer/timer.c $(DESTDIR)$(INSTDIR)/timer/
	install -m 0755 timer/timer.sh $(DESTDIR)$(INSTDIR)/timer/
	install -d $(DESTDIR)$(INSTDIR)/nanosleep
	install -m 0644 nanosleep/Makefile $(DESTDIR)$(INSTDIR)/nanosleep/
	install -m 0644 nanosleep/nanosleep.c $(DESTDIR)$(INSTDIR)/nanosleep/
	install -m 0755 nanosleep/nanosleep.sh $(DESTDIR)$(INSTDIR)/nanosleep/
	install -m 0644 nanosleep/fuzz_shift.txt $(DESTDIR)$(INSTDIR)/nanosleep/
	install -d $(DESTDIR)$(INSTDIR)/tswitch
	install -m 0644 tswitch/Makefile $(DESTDIR)$(INSTDIR)/tswitch/
	install -m 0644 tswitch/tswitch.c $(DESTDIR)$(INSTDIR)/tswitch/
	install -m 0755 tswitch/tswitch.sh $(DESTDIR)$(INSTDIR)/tswitch/
	install -d $(DESTDIR)$(INSTDIR)/pdl
	install -m 0644 pdl/Makefile $(DESTDIR)$(INSTDIR)/pdl/
	install -m 0644 pdl/pdl.c $(DESTDIR)$(INSTDIR)/pdl/
	install -m 0755 pdl/pdl.sh $(DESTDIR)$(INSTDIR)/pdl/
	install -d $(DESTDIR)$(INSTDIR)/memtest
	install -m 0644 memtest/Makefile $(DESTDIR)$(INSTDIR)/memtest/
	install -m 0644 memtest/memtest.c $(DESTDIR)$(INSTDIR)/memtest/
	install -m 0755 memtest/memtest.sh $(DESTDIR)$(INSTDIR)/memtest/
	install -d $(DESTDIR)$(INSTDIR)/disk_test
	install -m 0644 disk_test/Makefile $(DESTDIR)$(INSTDIR)/disk_test/
	install -m 0644 disk_test/disk_test.c $(DESTDIR)$(INSTDIR)/disk_test/
	install -m 0755 disk_test/disk_test.sh $(DESTDIR)$(INSTDIR)/disk_test/
	install -d $(DESTDIR)$(INSTDIR)/timer_loop
	install -m 0644 timer_loop/Makefile $(DESTDIR)$(INSTDIR)/timer_loop/
	install -m 0644 timer_loop/timer_loop.c $(DESTDIR)$(INSTDIR)/timer_loop/
	install -m 0755 timer_loop/timer_loop.sh $(DESTDIR)$(INSTDIR)/timer_loop/
	install -d $(DESTDIR)$(INSTDIR)/clock
	install -m 0644 clock/Makefile $(DESTDIR)$(INSTDIR)/clock/
	install -m 0644 clock/clock.c $(DESTDIR)$(INSTDIR)/clock/
	install -m 0755 clock/clock.sh $(DESTDIR)$(INSTDIR)/clock/

rpm:
	make install DESTDIR=${PWD}/root
	rpmbuild --bb --buildroot ${PWD}/root \
		--define "%_topdir ${PWD}" \
		--define "%_builddir ${PWD}" \
		--define "%_buildrootdir ${PWD}/root" \
		--define "%_rpmdir ${PWD}/rpm" \
		--define "%_srcrpmdir ${PWD}/rpm" \
		realtimetest.spec
	rm -rf root

deb:
	make install DESTDIR=${PWD}/root
	cp -r DEBIAN root/
	mkdir deb
	fakeroot dpkg-deb --build root deb
	rm -rf root
