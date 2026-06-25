CC := gcc
CFLAGS := -Isrc -fno-common -Wall -Wextra

ALL_TARGETS := lc_3
OBJS := lc_3.o src/registers.o src/memory.o src/trap.o

all: $(ALL_TARGETS)

# link step: combine the object files into the executable
lc_3: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

# compile step: each .c becomes a .o independently
%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

.PHONY: clean
clean:
	rm -rf $(ALL_TARGETS) $(OBJS)
