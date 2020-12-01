#include <stdio.h>
#include "sha1.c"
#include "g0.c"

static char *
strhash(unsigned char *h)
{
  static char s[41];
  int i;

  for (i = 0; i < 40; i += 2)
    sprintf(&s[i], "%02x", *h++);
  return s;
}

void
chunkcb(char *buf, long sz)
{
  static unsigned char hash[20];
  struct sha1_context ctx;
  sha1_init(&ctx);
  sha1_update(&ctx, buf, sz);
  sha1_final(&ctx, hash);
  printf("%s %ld\n", strhash(hash), sz);
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
  finish();
  return 0;
}
