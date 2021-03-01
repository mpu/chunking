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
	state.fp = -1;
}

#define MASK ((1u << LOG2_32(AVGBLK)) - 1)
#define UNLIKELY(x) __builtin_expect(x, 0)

void
chunk(char *buf, long sz)
{
	uint32_t fp;
	long n;
	int pos;

	fp = state.fp;
	pos = state.pos;
        for (n = 0; n + 3 < sz; n += 4) {
            fp = (fp << 1) + geartab[buf[n] & 0xff];
            if (UNLIKELY(!(fp & MASK))) {
                    chunkdone(pos + n);
                    pos = 0;
                    fp = -1;
            }
            fp = (fp << 1) + geartab[buf[n+1] & 0xff];
            if (UNLIKELY(!(fp & MASK))) {
                    chunkdone(pos + n + 1);
                    pos = 0;
                    fp = -1;
            }
            fp = (fp << 1) + geartab[buf[n+2] & 0xff];
            if (UNLIKELY(!(fp & MASK))) {
                    chunkdone(pos + n + 2);
                    pos = 0;
                    fp = -1;
            }
            fp = (fp << 1) + geartab[buf[n+3] & 0xff];
            if (UNLIKELY(!(fp & MASK))) {
                    chunkdone(pos + n + 3);
                    pos = 0;
                    fp = -1;
            }
        }
	for (; n <  sz; n++) {
		if (pos == MAXBLK && 0) {
			chunkdone(pos);
			pos = 0;
			fp = -1;
		}
		fp = (fp << 1) + geartab[buf[n] & 0xff];
		if (UNLIKELY(!(fp & MASK))) {
			chunkdone(pos);
			pos = 0;
			fp = -1;
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
    if(0)
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
