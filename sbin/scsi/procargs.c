/*
 * Written By Julian ELischer
 * Copyright julian Elischer 1993.
 * Permission is granted to use or redistribute this file in any way as long
 * as this notice remains. Julian Elischer does not guarantee that this file 
 * is totally correct for any given task and users of this file must 
 * accept responsibility for any damage that occurs from the application of this
 * file.
 * 
 * (julian@tfs.com julian@dialix.oz.au)
 *
 *	$Id: procargs.c,v 1.1 1993/11/18 05:05:26 rgrimes Exp $
 */

#include <stdio.h>
#include <sys/file.h>

extern int	fd;
extern int	debuglevel;
extern int	inqflag;
extern int	reprobe;
extern int	dflag;
extern int	bus;
extern int	targ;
extern int	lun;
char *myname;

void procargs(int argc, char **argv, char **envp)
{
	extern char        *optarg;
	extern int          optind;
	int		    fflag,
	                    ch;

	myname = *argv;
	fflag = 0;
	inqflag = 0;
	dflag = 0;
	while ((ch = getopt(argc, argv, "irf:d:b:t:l:")) != EOF) {
		switch (ch) {
		case 'r':
			reprobe = 1;
			break;
		case 'i':
			inqflag = 1;
			break;
		case 'f':
			if ((fd = open(optarg, O_RDWR, 0)) < 0) {
				(void) fprintf(stderr,
					  "%s: unable to open device %s.\n",
					       myname, optarg);
				exit(1);
			}
			fflag = 1;
			break;
		case 'd':
			debuglevel = atoi(optarg);
			dflag = 1;
			break;
		case 'b':
			bus = atoi(optarg);
			break;
		case 't':
			targ = atoi(optarg);
			break;
		case 'l':
			lun = atoi(optarg);
			break;
		case '?':
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;
	if (!fflag) usage();
}

usage()
{
	printf("usage: %s -f devicename [-d level] [-i] [-r [-b bus] "
		"[-t targ] [-l lun]]\n",myname);
	printf("where r = reprobe, i = inquire, d = debug\n");
	printf("If debugging is not compiled in the kernel, 'd' will have no effect\n");
	exit (1);
}
