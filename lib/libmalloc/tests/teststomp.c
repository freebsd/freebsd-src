#if defined(STDHEADERS)
# include <stddef.h>
# include <string.h>
# include <stdlib.h>
# include <unistd.h>
#else
# define u_int unsigned int
extern char *memset();
/* ignore some complaints about declarations.  get ANSI headers */
#endif

#include <stdio.h>
#include "malloc.h"

int
main(argc, argv)
char **argv;
int argc;
{
	char *cp;
	int nbytes;

	if (argc != 2) {
		(void) fprintf(stderr, "Usage: %s nbytes\n", argv[0]);
		exit(1);
	}

	nbytes = atoi(argv[1]);
	cp = (char *) malloc(nbytes);
	cp[nbytes] = 'a';
	mal_verify(1);
	/* We aren't going to get here, y'know... */
	return 0;
}
