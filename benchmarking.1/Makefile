CC      := gcc
CCFLAGS := -I
LDFLAGS :=

TARGETS:= process_vm_benchmarking
MAINS  := $(addsuffix .o, $(TARGETS) )
OBJ    := #kbd.o command.o display.o $(MAINS)
DEPS   := #defs.h command.h

BUILDPATH	:= ./build/

.PHONY: all clean

all: $(TARGETS)

clean:
	rm -f $(TARGETS) $(OBJ)

$(OBJ): %.o : %.c $(DEPS)
	$(CC) -c -o $@ $< $(CCFLAGS)

$(TARGETS): % : $(filter-out $(MAINS), $(OBJ)) %.o
	$(CC) -o $@ $(LIBS) $^ $(CCFLAGS) $(BUILDPATH) $(LDFLAGS)
	echo Build Done