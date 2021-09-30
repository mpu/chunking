#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "geartab.h"
#include "log2.h"

static struct {
	int pos;
	int fp[8];
	char buf[MAXBLK];
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

#define MASK ((1u << LOG2_32(AVGBLK)) - 1)

static int
chunkseq(char *buf, long *n, long count)
{
	int *fp, cmp, idx;

	fp = state.fp;
	idx = (state.pos + *n) & 7;
	for (; count > 0; count--) {
		fp[idx] <<= 1;
		fp[idx] += geartab[buf[(*n)++] & 0xff];
		cmp = (fp[idx] & MASK) == MASK;
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

	match = _mm256_set1_epi32(MASK);
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
	int perm[8], max;
	long n;
	int i, x;

	mask8 = _mm256_set1_epi32(0xff);
	n = 0;

startchunk:
	buf += n;
	sz -= n;
	n = 0;

	max = sz;
	if (state.pos + max > MAXBLK)
		max = MAXBLK - state.pos;

	x = (long long)buf & 31;
	if (max >= 31 && x > 0) {
		if (chunkseq(buf, &n, 32 - x))
			goto startchunk;
	}

	for (i = 0; i < 8; i++)
		perm[i] = state.fp[(state.pos + n + i) & 7];
	fp = _mm256_loadu_si256((__m256i*)perm);

	while (n + 32 <= max) {
                b0 = _mm256_load_si256((__m256i*)&buf[n]);

		/* permute the bytes so that shuffle_epi8 only
		 * moves data within 128bit lanes
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

		/* move each of the 32 bytes into a 32 bit
		 * word of b0,b1,b2,b3
		 * 
		 * after:
		 *  b0: |0___|1___|2___|3___||4___|5___|6___|7___|
		 *  b1: |8___|9___|a___|b___||c___|d___|e___|f___|
		 *  b2: |g___|h___|i___|j___||k___|l___|m___|n___|
		 *  b3: |o___|p___|q___|r___||s___|t___|u___|v___|
		 */
		b3 = EXTRACT(12);
		b2 = EXTRACT(8);
		b1 = EXTRACT(4);
		b0 = EXTRACT(0);

#undef EXTRACT
 
		b3 = _mm256_and_si256(b3, mask8);
		b2 = _mm256_and_si256(b2, mask8);
		b1 = _mm256_and_si256(b1, mask8);
		b0 = _mm256_and_si256(b0, mask8);

		b3 = _mm256_i32gather_epi32(geartab, b3, 4);
		b2 = _mm256_i32gather_epi32(geartab, b2, 4);
		b1 = _mm256_i32gather_epi32(geartab, b1, 4);
		b0 = _mm256_i32gather_epi32(geartab, b0, 4);

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

	if (n < max) {
		if (chunkseq(buf, &n, max - n))
			goto startchunk;
	}

	assert(n == max);
	state.pos += n;

	if (state.pos == MAXBLK) {
		chunkdone(state.pos);
		goto startchunk;
	}
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
	chunk(test+1, 32);
	for (i = 0; i < 8; i++)
		printf("fp[%d] = %08x\n", i, state.fp[i]);
	puts("");

/* -=- */
	l = 0;
	memset(&state, 0, sizeof(state));
	chunkseq(test+1, &l, 32);
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

void
chunkcb(char *buf, long len)
{
	(void)buf;
	printf("%ld\n", len);
}

void
finish()
{
	if (state.pos)
		printf("%d\n", state.pos);
}

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
	static char iobuf[65536];
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
