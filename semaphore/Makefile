BINARY := semaphore
OBJECT := $(BINARY:%=%.o)
SCRIPT := $(BINARY:%=%.sh)
TEMP := core core.*
CFLAGS = -O2 -march=native -Wall -I../include $(USRDEF)
USRDEF :=
LDLIBS := -lrt -lpthread

.PHONY: all clean test

all: $(BINARY)
clean:
	$(RM) -f $(BINARY) $(OBJECT) $(TEMP)
test: $(BINARY) $(SCRIPT)
	-@./$(SCRIPT)
install: all
wipeout: clean
