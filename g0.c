#include <stdint.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "geartab.h"

void chunkcb(char *buf, long sz);

enum {
	Maxblk = 3 << 20,
	Avgblk = 1 << 20,
};

static struct {
	int pos;
	uint32_t fp;
	char buf[Maxblk];
} state;

static void
chunkdone(int len)
{
	chunkcb(state.buf, len);
	state.pos = 0;
	state.fp = 0;
}

/* relates to Avgblk */
#define TWENTYONES 0xfffff000

void
chunk(char *buf, long sz)
{
	uint32_t fp;
	long n;
	int pos;

	fp = state.fp;
	pos = state.pos;
	for (n = 0; n <  sz; n++) {
		if (pos == Maxblk) {
			chunkdone(pos);
			pos = 0;
			fp = 0;
		}
		fp = (fp << 1) + geartab[buf[n] & 0xff];
		// state.buf[pos] = buf[n];
		pos++;
		if ((fp & TWENTYONES) == TWENTYONES) {
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
chunkcb(char *buf, long sz)
{
	(void)buf;
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
