#include <stdio.h>

int incr = 201;
char *cp;
int i;

main()
{
	extern char *sbrk();
	int sz;

	sz = getpagesize();
	printf("pagesize is 0x%08x (%d)\n", sz, sz);
	for(i = 0; i < 1000; i++) {
		if ((cp = sbrk(incr)) == (char *) -1) {
			fprintf(stderr, "Cannot sbrk further\n");
			exit(-1);
		}
		printf("segment starts at 0x%08x, ends at 0x%08x\n", (int) cp,
			(int) (cp + incr - 1));
	}
}
