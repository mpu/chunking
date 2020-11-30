all:
	cc -O3 -Wall -Wextra -std=c11 g0.c -o g0
	cc -O3 -Wall -Wextra -std=c11 -mavx2 g1.c -o g1
