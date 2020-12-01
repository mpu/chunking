#include <stdint.h>
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

#if 0
static void
chunkdone(int len)
{
	chunkcb(state.buf, len);
	state.pos = 0;
	state.fp[0] = 0;
	state.fp[1] = 0;
	state.fp[2] = 0;
	state.fp[3] = 0;
	state.fp[4] = 0;
	state.fp[5] = 0;
	state.fp[6] = 0;
	state.fp[7] = 0;
}
#endif

#include <immintrin.h>

#define TWENTYONES 0xfffff000

void
chunk(char *buf, long sz)
{
	long n;
	__m256i b0, b1, b2, b3;
	__m256i fp, mask8;
	// __m256i tmp, match;

	mask8 = _mm256_set1_epi32(0xff);
	// match = _mm256_set1_epi32(TWENTYONES);
	fp = _mm256_loadu_si256((__m256i*)state.fp);

	for (n = 0; n + 32 <= sz; n += 32) {
                b0 = _mm256_load_si256((__m256i*)&buf[n]);

#define BYTES(a,b,c,d,e,f,g,h) \
        _mm256_shuffle_epi8(b0, \
                _mm256_set_epi8(a,a,a,a,b,b,b,b,c,c,c,c,d,d,d,d, \
                                e,e,e,e,f,f,f,f,g,g,g,g,h,h,h,h))
                b3 = BYTES(24,25,26,27,28,29,30,31);
                b2 = BYTES(16,17,18,19,20,21,22,23);
                b1 = BYTES(8,9,10,11,12,13,14,15);
                b0 = BYTES(0,1,2,3,4,5,6,7);
#undef BYTES
 
		b3 = _mm256_and_si256(b3, mask8);
		b2 = _mm256_and_si256(b2, mask8);
		b1 = _mm256_and_si256(b1, mask8);
		b0 = _mm256_and_si256(b0, mask8);

		b3 = _mm256_i32gather_epi32((int*)geartab, b3, 4);
		b2 = _mm256_i32gather_epi32((int*)geartab, b2, 4);
		b1 = _mm256_i32gather_epi32((int*)geartab, b1, 4);
		b0 = _mm256_i32gather_epi32((int*)geartab, b0, 4);

		fp = _mm256_slli_epi32(fp, 1);
		fp = _mm256_add_epi32(fp, b0);

		// tmp = _mm256_and_si256(fp, match);

		fp = _mm256_slli_epi32(fp, 1);
		fp = _mm256_add_epi32(fp, b1);
		fp = _mm256_slli_epi32(fp, 1);
		fp = _mm256_add_epi32(fp, b2);
		fp = _mm256_slli_epi32(fp, 1);
		fp = _mm256_add_epi32(fp, b3);

	}

	_mm256_storeu_si256((__m256i*)state.fp, fp);
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


#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

int
main(int argc, char *argv[])
{
	long tot;
	int fd, i;

	if (argc < 2)
		return 1;

	if ((fd = open(argv[1], O_RDONLY)) == -1)
		return 1;
	#if 1
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
	for (i = 0; i < 8; i++)
		printf("%x\n", state.fp[i]);

	printf("munched ");
	if (tot / 1000000000)
		printf("%.2fGb\n", (float)tot / 1000000000);
	else if (tot / 1000000)
		printf("%.2fMb\n", (float)tot / 1000000);
	else if (tot / 1000)
		printf("%.2fKb\n", (float)tot / 1000);
	else
		printf("%ldb\n", tot);
}
