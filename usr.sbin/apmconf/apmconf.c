/*
 * LP (Laptop Package)
 *
 * Copyright (C) 1994 by HOSOKAWA Tatasumi <hosokawa@mt.cs.keio.ac.jp>
 *
 * This software may be used, modified, copied, distributed, and sold,
 * in both source and binary form provided that the above copyright and
 * these terms are retained. Under no circumstances is the author
 * responsible for the proper functioning of this software, nor does
 * the author assume any responsibility for damages incurred with its
 * use.
 *
 * Sep., 1994	Implemented on FreeBSD 1.1.5.1R (Toshiba AVS001WD)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <machine/apm_bios.h>

#define APMDEV		"/dev/apm"

static int		enable = 0, disable = 0;
static int		haltcpu = 0, nothaltcpu = 0;
static int		main_argc;
static char		**main_argv;

static void
parse_option(void)
{
	int	i, option;
	char	*optarg;
	enum {OPT_NONE, OPT_ENABLE, OPT_DISABLE, OPT_HALTCPU, OPT_NOTHALTCPU} mode;

	for (i = 1; i < main_argc; i++) {
		option = 0;
		mode = OPT_NONE;
		if (main_argv[i][0] == '-') {
			switch (main_argv[i][1]) {
			case 'e':
				mode = OPT_ENABLE;
				option = 0;
				break;
			case 'd':
				mode = OPT_DISABLE;
				option = 0;
				break;
			case 'h':
				mode = OPT_HALTCPU;
				option = 0;
				break;
			case 't':
				mode = OPT_NOTHALTCPU;
				option = 0;
				break;
			default:
				fprintf(stderr, "%s: Unknown option '%s.'\n", main_argv[0], main_argv[i]);
				exit(1);
			}
		}
		if (option) {
			if (i == main_argc - 1) {
				fprintf(stderr, "%s: Option '%s' needs arguments.\n", main_argv[0], main_argv[i]);
				exit(1);
			}
			optarg = main_argv[++i];
		}

		switch (mode) {
		case OPT_ENABLE:
			enable = 1;
			break;
		case OPT_DISABLE:
			disable = 1;
			break;
		case OPT_HALTCPU:
			haltcpu = 1;
			break;
		case OPT_NOTHALTCPU:
			nothaltcpu = 1;
			break;
		}
	}
}

static void
enable_apm(int dh)
{
	if (ioctl(dh, APMIO_ENABLE, NULL) == -1) {
		fprintf(stderr, "%s: Can't ioctl APMIO_ENABLE.\n", main_argv[0]);
		exit(1);
	}
}

static void
disable_apm(int dh)
{
	if (ioctl(dh, APMIO_DISABLE, NULL) == -1) {
		fprintf(stderr, "%s: Can't ioctl APMIO_DISABLE.\n", main_argv[0]);
		exit(1);
	}
}

static void
haltcpu_apm(int dh)
{
	if (ioctl(dh, APMIO_HALTCPU, NULL) == -1) {
		fprintf(stderr, "%s: Can't ioctl APMIO_HALTCPU.\n", main_argv[0]);
		exit(1);
	}
}

static void
nothaltcpu_apm(int dh)
{
	if (ioctl(dh, APMIO_NOTHALTCPU, NULL) == -1) {
		fprintf(stderr, "%s: Can't ioctl APMIO_NOTHALTCPU.\n", main_argv[0]);
		exit(1);
	}
}
int
main(int argc, char *argv[])
{
	int		i, dh;
	FILE		*fp;

	main_argc = argc;
	main_argv = argv;
	if ((dh = open(APMDEV, O_RDWR)) == -1) {
		fprintf(stderr, "%s: Can't open '%s'\n", argv[0], APMDEV);
		exit(1);
	}
	parse_option();

	/* disable operation is executed first */
	if (disable) {
		disable_apm(dh);
	}
	if (haltcpu) {
		haltcpu_apm(dh);
	}
	if (nothaltcpu) {
		nothaltcpu_apm(dh);
	}
	/* enable operation is executed last */
	if (enable) {
		enable_apm(dh);
	}
	return 0;
}
