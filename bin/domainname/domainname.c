#ifndef lint
static char rcsid[] = "$Id: domainname.c,v 1.2 1993/10/25 03:12:32 rgrimes Exp $";
#endif /* not lint */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/param.h>

static void usage __P((void));

main(argc, argv)
	int argc;
	char **argv;
{
	char dom[MAXHOSTNAMELEN];

	if( argc>2 ) {
		usage ();
		/* NOTREACHED */
	}

	if( argc==2 ) {
		if( setdomainname(argv[1], strlen(argv[1])+1) == -1) {
			perror("setdomainname");
			exit(1);
		}
	} else {
		if( getdomainname(dom, sizeof(dom)) == -1) {
			perror("getdomainname");
			exit(1);
		}
		printf("%s\n", dom);
	}

	exit(0);
}

static void
usage ()
{
	(void)fprintf(stderr, "usage: domainname [name-of-domain]\n");
	exit(1);
}
