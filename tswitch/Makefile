BINARY := tswitch
OBJECT := $(BINARY:%=%.o)
SCRIPT := $(BINARY:%=%.sh)
TEMP := core core.*
CFLAGS = -O2 -march=native -Wall -I../include $(USRDEF)
USRDEF :=
LDLIBS := -lpthread -lrt -lm

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
rcim: $(BINARY) $(SCRIPT)
	-@./$(SCRIPT) -- -r
install: all
wipeout: clean
