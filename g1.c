#include <stdint.h>
#include <stdio.h>
#include <string.h>
// #include "main.c"
#include "geartab.c"

enum {
	Maxblk = 2 << 20,
	Avgblk = 1 << 20,
};

static struct {
	int pos;
	uint32_t fp[8];
	char buf[Maxblk];
} state;

void chunkcb(char *, long);

static void
chunkdone(int len)
{
	chunkcb(state.buf, len);
	state.pos = 0;
	memset(state.fp, 0, sizeof(state.fp));
}

#include <immintrin.h>

#define TWENTYONES 0xfffff000
// #define GEARMUL 0xc7810b1b

static int
chunkseq(char *buf, long *n, long count)
{
	uint32_t *fp, cmp;
	int idx;

	fp = state.fp;
	idx = (state.pos + *n) & 7;
	for (; count > 0; count--) {
		fp[idx] <<= 1;
		fp[idx] += geartab[buf[(*n)++] & 0xff];
		cmp = (fp[idx] & TWENTYONES) == TWENTYONES;
		if (__builtin_expect(cmp, 0)) {
			chunkdone(state.pos + *n);
			return 1;
		}
		idx = (idx + 1) & 7;
	}
	return 0;
}

inline __attribute__((always_inline))
static int
checkboundary(__m256i fp, long *n)
{
	__m256i tmp, match;
	int mask;

	match = _mm256_set1_epi32(TWENTYONES);
	tmp = _mm256_and_si256(fp, match);
	tmp = _mm256_cmpeq_epi32(tmp, match);
	mask = _mm256_movemask_epi8(tmp);
	if (__builtin_expect(mask, 0)) {
		*n += 1 + __builtin_clz(mask) / 4;
		chunkdone(state.pos + *n);
		return 1;
	}
	*n += 8;
	return 0;
}

void
chunk(char *buf, long sz)
{
	__m256i b0, b1, b2, b3;
	__m256i fp, mask8;
	long n;
	int x;

	mask8 = _mm256_set1_epi32(0xff);
	n = 0;

startchunk:
	buf += n;
	sz -= n;
	n = 0;

	x = (long long)buf & 31;
	if (x) {
		if (chunkseq(buf, &n, 32 - x))
			goto startchunk;
	}

	// FIXME not right, we might need to rotate
	// the lanes
	fp = _mm256_loadu_si256((__m256i*)state.fp);

	for (; n + 32 <= sz; n += 32) {
                b0 = _mm256_load_si256((__m256i*)&buf[n]);

#define SHUFBYTES(a,b,c,d,e,f,g,h) \
        _mm256_shuffle_epi8(b0, \
                _mm256_set_epi8(a,a,a,a,b,b,b,b,c,c,c,c,d,d,d,d, \
                                e,e,e,e,f,f,f,f,g,g,g,g,h,h,h,h))
                b3 = SHUFBYTES(24,25,26,27,28,29,30,31);
                b2 = SHUFBYTES(16,17,18,19,20,21,22,23);
                b1 = SHUFBYTES(8,9,10,11,12,13,14,15);
                b0 = SHUFBYTES(0,1,2,3,4,5,6,7);
#undef SHUFBYTES
 
		b3 = _mm256_and_si256(b3, mask8);
		b2 = _mm256_and_si256(b2, mask8);
		b1 = _mm256_and_si256(b1, mask8);
		b0 = _mm256_and_si256(b0, mask8);

#ifndef GEARMUL

		b3 = _mm256_i32gather_epi32((int*)geartab, b3, 4);
		b2 = _mm256_i32gather_epi32((int*)geartab, b2, 4);
		b1 = _mm256_i32gather_epi32((int*)geartab, b1, 4);
		b0 = _mm256_i32gather_epi32((int*)geartab, b0, 4);
#else
		__m256i mul = _mm256_set1_epi32(GEARMUL);
		b3 = _mm256_mul_epi32(b3, mul);
		b2 = _mm256_mul_epi32(b2, mul);
		b1 = _mm256_mul_epi32(b1, mul);
		b0 = _mm256_mul_epi32(b0, mul);
#endif

		fp = _mm256_slli_epi32(fp, 1);
		fp = _mm256_add_epi32(fp, b0);
		if (checkboundary(fp, &n))
			goto startchunk;

		fp = _mm256_slli_epi32(fp, 1);
		fp = _mm256_add_epi32(fp, b1);
		if (checkboundary(fp, &n))
			goto startchunk;

		fp = _mm256_slli_epi32(fp, 1);
		fp = _mm256_add_epi32(fp, b2);
		if (checkboundary(fp, &n))
			goto startchunk;

		fp = _mm256_slli_epi32(fp, 1);
		fp = _mm256_add_epi32(fp, b3);
		if (checkboundary(fp, &n))
			goto startchunk;

	}

	_mm256_storeu_si256((__m256i*)state.fp, fp);
	state.pos += n;
}

void
chunk1(char *buf, long sz)
{
	long n;
	__m256i fp, dat, mask8;
	// __m256i tmp, match;

	mask8 = _mm256_set1_epi32(0xff);
	// match = _mm256_set1_epi32(TWENTYONES);
	fp = _mm256_loadu_si256((__m256i*)state.fp);

	for (n = 0; n + 8 <= sz; n += 8) {
		dat = _mm256_i32gather_epi32((int*)&buf[n],
			_mm256_set_epi32(0, 1, 2, 3, 4, 5, 6, 7), 1);
		dat = _mm256_and_si256(dat, mask8);
		dat = _mm256_i32gather_epi32((int*)geartab, dat, 4);
		fp = _mm256_slli_epi32(fp, 1);
		fp = _mm256_add_epi32(fp, dat);
	}

	_mm256_storeu_si256((__m256i*)state.fp, fp);
}


#define CHUNK chunk


#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

int
main(int argc, char *argv[])
{
	long tot;
	int fd;

	if (argc < 2)
		return 1;

	if ((fd = open(argv[1], O_RDONLY)) == -1)
		return 1;
	#if 0
	struct stat sb;
	char *mmbuf;

	fstat(fd, &sb);
	mmbuf = mmap(0, (sb.st_size + 4095) & -4096, PROT_READ, MAP_PRIVATE, fd, 0);
	tot = sb.st_size;
	CHUNK(mmbuf, tot);
	#else
	static char iobuf[1 << 17];
	long rd;

	tot = 0;
	for (;;) {
		rd = read(fd, iobuf, sizeof(iobuf));
		tot += rd;
		if (!rd)
			break;
		CHUNK(iobuf+1, rd-1);
	}
	#endif

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

void
chunkcb(char *buf, long len)
{
	(void)buf;
	printf("%ld\n", len);
}
