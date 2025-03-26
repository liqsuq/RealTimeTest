BINARY := timer_loop
OBJECT := $(BINARY:%=%.o)
SCRIPT := $(BINARY:%=%.sh)
TEMP := core core.*
CFLAGS = -O2 -march=native -Wall -I../include $(USRDEF)
USRDEF :=
LDLIBS := -lrt -lm -lncurses

RCIM := $(shell lsirq 2>&1|grep RCIM|cut -f1)
REDHAWK := $(shell if [ -f /etc/redhawk-release ]; then echo 1; else echo 0; fi)
ifeq ("$(REDHAWK)", "1")
USRDEF += -DRCIM
endif

.PHONY: all clean test

all: $(BINARY)
clean:
	$(RM) -f $(BINARY) $(OBJECT) $(TEMP)
test: $(BINARY) $(SCRIPT)
	-@./$(SCRIPT)
rcim:
	-@./$(SCRIPT) -- -r
install: all
wipeout: clean
