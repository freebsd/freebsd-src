/*
 * trylook.c - test program for lookup.c
 */

#include <sys/types.h>
#include <netinet/in.h>
#include <stdio.h>

#include "report.h"
#include "lookup.h"

extern char *ether_ntoa();
extern char *inet_ntoa();

int debug = 0;
char *progname;

main(argc, argv)
	char **argv;
{
	int i;
	struct in_addr in;
	char *a;
	u_char *hwa;

	progname = argv[0];			/* for report */

	for (i = 1; i < argc; i++) {

		/* Host name */
		printf("%s:", argv[i]);

		/* IP addr */
		if (lookup_ipa(argv[i], &in.s_addr))
			a = "?";
		else
			a = inet_ntoa(in);
		printf(" ipa=%s", a);

		/* Ether addr */
		hwa = lookup_hwa(argv[i], 1);
		if (!hwa)
			a = "?";
		else
			a = ether_ntoa(hwa);
		printf(" hwa=%s\n", a);

	}
	exit(0);
}
