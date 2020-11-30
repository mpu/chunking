all:
	cc -O3 -Wall -Wextra -std=c11 gear.c -o gear
	cc -O3 -Wall -Wextra -std=c11 -mavx2 gear-simd0.c -o gear-simd0
