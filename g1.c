#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// #include "main.c"
#include "geartab.c"

enum {
	Maxblk = 2 << 20,
	Avgblk = 1 << 20,
};

static struct {
	int pos;
	int fp[8];
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
	int *fp, cmp, idx;

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
		*n += 1 + __builtin_ctz(mask) / 4;
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
	int perm[8];
	long n;
	int i, x;

	mask8 = _mm256_set1_epi32(0xff);
	n = 0;

startchunk:
	buf += n;
	sz -= n;
	n = 0;

	x = (long long)buf & 31;
	if (x > 0) {
		if (chunkseq(buf, &n, 32 - x))
			goto startchunk;
	}

	for (i = 0; i < 8; i++)
		perm[i] = state.fp[(state.pos + n + i) & 7];
	fp = _mm256_loadu_si256((__m256i*)perm);

	while (n + 32 <= sz) {
                b0 = _mm256_load_si256((__m256i*)&buf[n]);

		/* permute the bytes so that shuffle_epi8 only
		 * moves data within 128bit lanes; that's
		 * unfortunately a bit costly (Intel documents
		 * 3 cycles of latency)
		 *
		 * before:
		 *  |0123|4567|89ab|cdef||ghij|klmn|opqr|stuv|
		 * after the permutation:
		 *  |0123|89ab|ghij|opqr||4567|cdef|klmn|stuv|
		 */
		b0 = _mm256_permutevar8x32_epi32(b0,
			_mm256_set_epi32(7,5,3,1,6,4,2,0));

#define EXTRACT(i) \
	_mm256_shuffle_epi8(b0, \
		_mm256_set_epi32(i+3,i+2,i+1,i+0,i+3,i+2,i+1,i+0))

		b3 = EXTRACT(12);
		b2 = EXTRACT(8);
		b1 = EXTRACT(4);
		b0 = EXTRACT(0);

#undef EXTRACT
 
		b3 = _mm256_and_si256(b3, mask8);
		b2 = _mm256_and_si256(b2, mask8);
		b1 = _mm256_and_si256(b1, mask8);
		b0 = _mm256_and_si256(b0, mask8);

#ifndef GEARMUL

		b3 = _mm256_i32gather_epi32(geartab, b3, 4);
		b2 = _mm256_i32gather_epi32(geartab, b2, 4);
		b1 = _mm256_i32gather_epi32(geartab, b1, 4);
		b0 = _mm256_i32gather_epi32(geartab, b0, 4);
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

	_mm256_storeu_si256((__m256i*)perm, fp);
	for (i = 0; i < 8; i++)
		state.fp[(state.pos + n + i) & 7] = perm[i];

	if (n < sz) {
		if (chunkseq(buf, &n, sz - n))
			goto startchunk;
	}

	assert(n == sz);
	state.pos += n;
}

void
test()
{
	struct {
		__m256i align_;
		char buf[64];
	} aligned;
	char *test = aligned.buf;
	long l;
	int i;

	srand(0);
	for (i = 0; i < 64; i++)
		test[i] = rand();

/* -=- */
	memset(&state, 0, sizeof(state));
	chunk(test+2, 30+32+4);
	for (i = 0; i < 8; i++)
		printf("fp[%d] = %08x\n", i, state.fp[i]);
	puts("");

/* -=- */
	l = 0;
	memset(&state, 0, sizeof(state));
	chunkseq(test+2, &l, 30+32+4);
	for (i = 0; i < 8; i++)
		printf("fp[%d] = %08x\n", i, state.fp[i]);
	puts("");

/* -=- */
	memset(&state, 0, sizeof(state));
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


	if (argc < 2) {
		test();
		return 1;
	}

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
		CHUNK(iobuf, rd);
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
