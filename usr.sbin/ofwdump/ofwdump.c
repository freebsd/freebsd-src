/*-
 * Copyright (c) 2002 by Thomas Moestl <tmm@FreeBSD.org>.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "ofw_util.h"

void	usage(void);

void
usage(void)
{

	fprintf(stderr,
	    "usage: ofwdump -a [-p | -P property] [-R | -S]\n"
	    "       ofwdump [-p | -P property] [-r] [-R | -S] [--] nodes\n");
	exit(1);
}

int
main(int argc, char *argv[])
{
	int opt, i, fd;
	int aflag, pflag, rflag, Rflag, Sflag;
	char *Parg;

	aflag = pflag = rflag = Rflag = Sflag = 0;
	Parg = NULL;
	while ((opt = getopt(argc, argv, "-aprP:RS")) != -1) {
		if (opt == '-')
			break;
		switch (opt) {
		case 'a':
			aflag = 1;
			rflag = 1;
			break;
		case 'p':
			if (Parg != NULL)
				usage();
			pflag = 1;
			break;
		case 'r':
			rflag = 1;
			break;
		case 'P':
			if (pflag)
				usage();
			pflag = 1;
			Parg = optarg;
			break;
		case 'R':
			if (Sflag)
				usage();
			Rflag = 1;
			break;
		case 'S':
			if (Rflag)
				usage();
			Sflag = 1;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	fd = ofw_open();
	if (aflag) {
		if (argc != 0)
			usage();
		ofw_dump(fd, NULL, rflag, pflag, Parg, Rflag, Sflag);
	} else {
		for (i = 0; i < argc; i++)
			ofw_dump(fd, argv[i], rflag, pflag, Parg, Rflag, Sflag);
	}
	ofw_close(fd);
	return (0);
}
