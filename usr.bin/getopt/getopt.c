#include <stdio.h>

main(argc, argv)
int argc;
char *argv[];
{
	extern int optind;
	extern char *optarg;
	int c;
	int status = 0;

	optind = 2;	/* Past the program name and the option letters. */
	while ((c = getopt(argc, argv, argv[1])) !=  -1)
		switch (c) {
		case '?':
			status = 1;	/* getopt routine gave message */
			break;
		default:
			if (optarg != NULL)
				printf(" -%c %s", c, optarg);
			else
				printf(" -%c", c);
			break;
		}
	printf(" --");
	for (; optind < argc; optind++)
		printf(" %s", argv[optind]);
	printf("\n");
	exit(status);
}
