#include <stdint.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "geartab.h"
#include "log2.h"

void chunkcb(long sz);

enum {
	GearM = 0xc7810b1b,
	GearA = 0x4e39afed,
};

static struct {
	int pos;
	uint32_t fp;
} state;

static void
chunkdone(int len)
{
	chunkcb(len);
	state.pos = 0;
	state.fp = 0;
}

#define MASK ((1u << LOG2_32(AVGBLK)) - 1)

void
chunk(char *buf, long sz)
{
	uint32_t fp;
	long n;
	int pos;

	fp = state.fp;
	pos = state.pos;
	for (n = 0; n <  sz; n++) {
		if (pos == MAXBLK) {
			chunkdone(pos);
			pos = 0;
			fp = 0;
		}
		fp = (fp << 1) + geartab[buf[n] & 0xff];
		// fp = (fp << 1) + GearA + GearM * (buf[n] & 0xff);
		pos++;
		if ((fp & MASK) == MASK) {
			chunkdone(pos);
			pos = 0;
			fp = 0;
		}
	}
	state.pos = pos;
	state.fp = fp;
}

void finish(void)
{
	if (state.pos)
		chunkdone(state.pos);
}

void
chunkcb(long sz)
{
	printf("%ld\n", sz);
}

int
main(int argc, char *argv[])
{
	static char iobuf[65536];
	long rd;
	int fd;

	if (argc < 2)
		return 1;

	if ((fd = open(argv[1], O_RDONLY)) == -1)
		return 1;
	for (;;) {
		rd = read(fd, iobuf, sizeof(iobuf));
		if (!rd)
			break;
		chunk(iobuf, rd);
	}
	finish();
	return 0;
}
