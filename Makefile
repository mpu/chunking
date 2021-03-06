CFLAGS += -Wall -Wextra -std=c11

BIN = g0 g1 g2 ts sums

AVG = (1 << 20)
MAX = (3 << 20)

CFLAGS += -D"AVGBLK=$(AVG)" -D"MAXBLK=$(MAX)"

all: $(BIN)

clean:
	rm -f $(BIN)

g0: g0.c
	$(CC) $(CFLAGS) -O3 $< -o $@

g1: g1.c
	$(CC) $(CFLAGS) -O3 -mavx2 $< -o $@

g2: g2.c
	$(CC) $(CFLAGS) -O3 $< -o $@

ts: ts.c
	$(CC) $(CFLAGS) -O3 $< -o $@

sums: sums.c
	$(CC) $(CFLAGS) -O2 $< -o $@

.PHONY: all clean
