BINARY := aio
OBJECT := $(BINARY:%=%.o)
SCRIPT := $(BINARY:%=%.sh)
TEMP := core core.* realtime_file
CFLAGS = -O2 -march=native -Wall -I../include $(USRDEF)
USRDEF :=
LDLIBS := -lrt

.PHONY: all clean test

all: $(BINARY)
clean:
	$(RM) -f $(BINARY) $(OBJECT) $(TEMP)
test: $(BINARY) $(SCRIPT)
	-@./$(SCRIPT)
