#include <stdint.h>
#include <stdio.h>
#include <string.h>

void chunkcb(long);

struct {
	uint32_t fp[4];
	long pos;
} state;

enum {
	GearM = 0xc7810b1b,
	GearA = 0x4e39afed,
};

inline __attribute__((always_inline))
void
gear(uint32_t *fp, int b)
{
	*fp = (*fp << 1) + GearA + (uint32_t)b * GearM;
}

#define TWENTYONES 0xfffff000

inline __attribute__((always_inline))
int
check(uint32_t fp, long n)
{
	if (__builtin_expect((fp & TWENTYONES) == TWENTYONES, 0)) {
		chunkcb(state.pos + n);
		memset(&state, 0, sizeof(state));
		return 1;
	}
	return 0;
}

void
chunk(char *buf, long sz)
{
	uint32_t *fp;
	long n;

	fp = state.fp;
	n = 0;

startblock:
	buf += n;
	sz -= n;
	n = 0;

	switch ((state.pos + n) & 3) {
	while (n + 8 < sz) {
	case 0:
		gear(fp+0, buf[n++] & 0xff);
		if (check(fp[0], n))
			goto startblock;
		/* fallthrough */
	case 1:
		gear(fp+1, buf[n++] & 0xff);
		if (check(fp[1], n))
			goto startblock;
		/* fallthrough */
	case 2:
		gear(fp+2, buf[n++] & 0xff);
		if (check(fp[2], n))
			goto startblock;
		/* fallthrough */
	case 3:
		gear(fp+3, buf[n++] & 0xff);
		if (check(fp[3], n))
			goto startblock;
		/* fallthrough */
	}
	}
}

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

void
chunkcb(long len)
{
	printf("%ld\n", len);
}

void
finish()
{
	if (state.pos)
		chunkcb(state.pos);
}

int
main(int argc, char *argv[])
{
	static char iobuf[65536];
	long rd, tot;
	int fd;


	if (argc < 2)
		return 1;

	if ((fd = open(argv[1], O_RDONLY)) == -1)
		return 1;


	tot = 0;
	for (;;) {
		rd = read(fd, iobuf, sizeof(iobuf));
		tot += rd;
		if (!rd)
			break;
		chunk(iobuf, rd);
	}
	finish();

	fprintf(stderr, "munched ");
	if (tot / 1000000000)
		fprintf(stderr, "%.2fGb\n", (float)tot / 1000000000);
	else if (tot / 1000000)
		fprintf(stderr, "%.2fMb\n", (float)tot / 1000000);
	else if (tot / 1000)
		fprintf(stderr, "%.2fKb\n", (float)tot / 1000);
	else
		fprintf(stderr, "%ldb\n", tot);
}
