/*% cc -O3 -Wall -mavx2 % -o simd-tests
 */
#include <stdio.h>
#include <stdint.h>
#include <immintrin.h>

void
p256(__m256i x)
{
	int i;
	uint32_t it[8];

	it[0] = _mm256_extract_epi32(x, 0);
	it[1] = _mm256_extract_epi32(x, 1);
	it[2] = _mm256_extract_epi32(x, 2);
	it[3] = _mm256_extract_epi32(x, 3);
	it[4] = _mm256_extract_epi32(x, 4);
	it[5] = _mm256_extract_epi32(x, 5);
	it[6] = _mm256_extract_epi32(x, 6);
	it[7] = _mm256_extract_epi32(x, 7);

	for (i = 0; i < 8; i++)
		printf("%08x%s", it[i], (i == 7) ? "" : " ");
}

int
main()
{
	char bytes[32] = "abcdefghijklmnopqrstuvwx01234567";
	__m256i x, y;

	x = _mm256_loadu_si256((__m256i*)bytes);
	printf("x: ");
	p256(x); puts("");
	printf("x[0]: %02x\n", _mm256_extract_epi8(x, 0));

        y = _mm256_shuffle_epi8(x,
		                // high 128 bits lane
                _mm256_set_epi8(0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,
				// low 128 bits lane
                                16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31));
	printf("y: ");
	p256(y); puts("");

	// cross lanes, slow :'( !! latency is 3
	// there does not seem to be much one can do
        y = _mm256_permutevar8x32_epi32(x, _mm256_set_epi32(7,3,5,1,6,2,4,0));
	printf("y(perm): ");
	p256(y); puts("");

        y = _mm256_shuffle_epi8(y, _mm256_set_epi32(3,2,1,0,3,2,1,0));
	y = _mm256_and_si256(y, _mm256_set1_epi32(0xff));
	printf("y: ");
	p256(y); puts("");

        y = _mm256_cmpeq_epi32(y, _mm256_set1_epi32(0x62));
	printf("y: ");
	p256(y); puts("");
	unsigned mask = _mm256_movemask_epi8(y);
	printf("movemask: %08x\n", mask);
	printf("ctz: %d\n", __builtin_ctz(mask));

}
