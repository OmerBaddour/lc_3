ALL_TARGETS := lc_3

all: $(ALL_TARGETS)

lc_3: lc_3.c
	gcc -o lc_3 lc_3.c

.PHONY: clean
clean:
	rm -rf $(ALL_TARGETS)
