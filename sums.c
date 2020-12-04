#include <stdio.h>
#include <stdlib.h>
#include "sha1.c"

char *
hashstr(char *h)
{
	static char str[41];
	int i;

	for (i = 0; i < 20; i++)
		sprintf(&str[2*i], "%02x", h[i] & 0xff);
	return str;
}

int
main(int argc, char *argv[])
{
	char hash[20], line[32], *buf;
	long blen, l;
	FILE *f, *fdat;
	struct sha1_context ctx;

	if (argc < 3)
		return 1;

	if (!(f = fopen(argv[1], "r")))
		return 1;

	if (!(fdat = fopen(argv[2], "r")))
		return 1;

	buf = 0;
	blen = 0;
	while (fgets(line, sizeof(line), f)) {
		l = strtol(line, 0, 10);
		if (l > blen) {
			buf = realloc(buf, 2*l);
			if (!buf)
				return 1;
		}
		if (fread(buf, l, 1, fdat) != 1)
			return 1;
		sha1_init(&ctx);
		sha1_update(&ctx, buf, l);
		sha1_final(&ctx, (unsigned char*)hash);
		printf("%s %ld\n", hashstr(hash), l);
	}

	/* we must be at eof */
	return (fread(buf, 1, 1, fdat) == 0) ? 0 : 1;
}
