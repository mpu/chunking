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
uint32_t
gear(uint32_t fp, int b)
{
	return (fp << 1) + GearA + (uint32_t)b * GearM;
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
	uint32_t fp0, fp1, fp2, fp3;
	long n;

startblock:
	fp0 = state.fp[0];
	fp1 = state.fp[1];
	fp2 = state.fp[2];
	fp3 = state.fp[3];
	n = 0;

	switch (state.pos & 3) {
	for (; n + 4 <= sz; n += 4) {
	case 0:
		fp0 = gear(fp0, buf[n+0] & 0xff);
		/* fallthrough */
	case 1:
		fp1 = gear(fp1, buf[n+1] & 0xff);
		/* fallthrough */
	case 2:
		fp2 = gear(fp2, buf[n+2] & 0xff);
		/* fallthrough */
	case 3:
		fp3 = gear(fp3, buf[n+3] & 0xff);

		if (check(fp0, n+1)) {
			buf += n+1;
			sz -= n+1;
			goto startblock;
		}
		if (check(fp1, n+2)) {
			buf += n+2;
			sz -= n+2;
			goto startblock;
		}
		if (check(fp2, n+3)) {
			buf += n+3;
			sz -= n+3;
			goto startblock;
		}
		if (check(fp3, n+4)) {
			buf += n+4;
			sz -= n+4;
			goto startblock;
		}
		/* fallthrough */
	}
	}

	state.pos += n;
	state.fp[0] = fp0;
	state.fp[1] = fp1;
	state.fp[2] = fp2;
	state.fp[3] = fp3;
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