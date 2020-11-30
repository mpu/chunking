#include <stdio.h>

static void chunk(char *, long);

void
chunkcb(char *buf, long sz)
{
	(void)buf;
	printf("%ld\n", sz);
}

int
main(int argc, char *argv[])
{
	static char iobuf[65536];
	long rd;
	FILE *f;

	if (argc < 2)
		return 1;

	if (!(f = fopen(argv[1], "r")))
		return 1;
	for (;;) {
		rd = fread(iobuf, 1, sizeof(iobuf), f);
		if (!rd)
			break;
		chunk(iobuf, rd);
	}
}
