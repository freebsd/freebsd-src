#include <stdio.h>
#include <stdlib.h>

void
main(int argc, char **argv)
{
	int i,j,n;
	char *pass[8], r;

	n = atoi(argv[1]);

	srandom(101173);
	for (i = 0; i < n; i++) {
		for (j = 0; j < 8; j++) {
			r = random() % 122;
			while (r < 48)
				r += random() % (122 - r);
			printf("%c", r);
		}
		printf("\n");
	}
}
