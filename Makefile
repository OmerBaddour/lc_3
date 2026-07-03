CC := gcc
# -MMD -MP make gcc emit .d files recording which headers each .o depends on
CFLAGS := -Isrc -fno-common -Wall -Wextra -MMD -MP

# --- discovery: just drop files in src/ or tests/, no edits needed ---
BIN       := lc_3
MAIN_SRC  := lc_3.c                      # the .c that holds main()
LIB_SRCS  := $(wildcard src/*.c)         # everything compiled into the VM *and* tests
TEST_SRCS := $(wildcard tests/*.c)       # one main() per file => one test binary each

# --- derived names ---
MAIN_OBJ  := $(MAIN_SRC:.c=.o)
LIB_OBJS  := $(LIB_SRCS:.c=.o)
TEST_BINS := $(TEST_SRCS:.c=)            # tests/test_registers.c -> tests/test_registers
ALL_OBJS  := $(MAIN_OBJ) $(LIB_OBJS) $(TEST_SRCS:.c=.o)
DEPS      := $(ALL_OBJS:.o=.d)

.PHONY: all test memcheck clean

all: $(BIN)

# link the VM: main + every lib object
$(BIN): $(MAIN_OBJ) $(LIB_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

# compile any .c -> .o (one rule covers src/, tests/, and lc_3.c)
%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

# each test binary = its own object + all lib objects (NOT main, to avoid two main()s)
tests/%: tests/%.o $(LIB_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

# build every test, then run them in turn; stop on first failure
test: $(TEST_BINS)
	@for t in $(TEST_BINS); do echo "=== $$t ==="; ./$$t || exit 1; done

# re-run the same test binaries under macOS 'leaks' to catch memory leaks;
# leaks exits non-zero if any are found, so this fails the same way `test` does.
# (Linux equivalent would wrap valgrind --leak-check=full here instead.)
# Plain build only: `leaks` can't inspect ASan's replacement malloc.
memcheck: $(TEST_BINS)
	@for t in $(TEST_BINS); do \
		echo "=== leaks: $$t ==="; \
		leaks --atExit -- ./$$t || exit 1; \
	done

clean:
	rm -rf $(BIN) $(ALL_OBJS) $(TEST_BINS) $(DEPS)

# pull in the auto-generated header dependencies (ignored if absent, e.g. first build)
-include $(DEPS)
