#ifndef lint
static char rcsid[] = "$Header: /a/cvs/386BSD/src/sbin/mount_isofs/mount_isofs.c,v 1.3 1993/10/24 04:30:08 rgrimes Exp $";
#endif

#include <stdio.h>
#include <sys/types.h>
#include <sys/mount.h>

void
usage ()
{
	fprintf (stderr, "usage: mount_iso bdev dir\n");
	exit (1);
}
		
int
main (argc, argv)
int argc;
char **argv;
{
	char *dev;
	char *dir;
	struct ufs_args args;
	int c;
	int opts;

	opts = MNT_RDONLY;

	argc--;
	argv++;
	while (argc > 2) {
		if (!strcmp("-F", argv[0])) {
			argc--; argv++;
			opts |= atoi(argv[0]);
			argc--; argv++;
		} else if (!strcmp(argv[0], "-norrip")) {
			opts |= MNT_NORRIP;
			argc--; argv++;
		} else
			usage();
	}

	dev = argv[0];
	dir = argv[1];

	args.fspec = dev;
	args.exflags = MNT_EXRDONLY | opts;
	args.exroot = 0;

	if (mount (MOUNT_ISOFS, dir, MNT_RDONLY, &args) < 0) {
		perror ("mount");
		exit (1);
	}

	exit (0);
}

