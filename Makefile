CC := gcc
CFLAGS := -Isrc -fno-common -Wall -Wextra

ALL_TARGETS := lc_3
OBJS := lc_3.o src/registers.o src/memory.o src/trap.o

# src targets
all: $(ALL_TARGETS)

## link step: combine the object files into the executable
lc_3: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

## compile step: each .c becomes a .o independently
%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

# test targets
test_registers: tests/test_registers.o src/registers.o
	$(CC) $(CFLAGS) -o $@ $^

# phony targets
.PHONY: clean test

clean:
	rm -rf $(ALL_TARGETS) $(OBJS)

test: test_registers
	./test_registers
