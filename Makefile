CFLAGS += -Wall -Wextra -std=c11

BIN = g0 g1 sums

all: $(BIN)

clean:
	rm -f $(BIN)

g0: g0.c
	$(CC) $(CFLAGS) -O3 $< -o $@

g1: g1.c
	$(CC) $(CFLAGS) -O3 -mavx2 $< -o $@

sums: sums.c
	$(CC) $(CFLAGS) -O2 $< -o $@

.PHONY: all clean
