all:
	cc -O2 -Wall -Wextra -std=c11 sums.c -o sums
	cc -O3 -Wall -Wextra -std=c11 main.c -o g0
	cc -O3 -Wall -Wextra -std=c11 main-sums.c -o g0-sums
	cc -O3 -Wall -Wextra -std=c11 -mavx2 g1.c -o g1
