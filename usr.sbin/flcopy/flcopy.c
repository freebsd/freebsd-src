/*
 * Copyright (c) 1989 The Regents of the University of California.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
char copyright[] =
"@(#) Copyright (c) 1989 The Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)flcopy.c	5.4 (Berkeley) 1/20/91";
#endif /* not lint */

#include <sys/file.h>
#include <stdio.h>
#include "pathnames.h"

int	floppydes;
char	*flopname = _PATH_FLOPPY;
long	dsize = 77 * 26 * 128;
int	hflag;
int	rflag;

main(argc, argv)
	register char **argv;
{
	extern char *optarg;
	extern int optind;
	static char buff[512];
	register long count;
	register startad = -26 * 128;
	register int n, file;
	register char *cp;
	int ch;

	while ((ch = getopt(argc, argv, "f:hrt:")) != EOF)
		switch(ch) {
		case 'f':
			flopname = optarg;
			break;
		case 'h':
			hflag = 1;
			printf("Halftime!\n");
			if ((file = open("floppy", 0)) < 0) {
				printf("can't open \"floppy\"\n");
				exit(1);
			}
			break;
		case 'r':
			rflag = 1;
			break;
		case 't':
			dsize = atoi(optarg);
			if (dsize <= 0 || dsize > 77) {
				(void)fprintf(stderr,
				    "flcopy: bad number of tracks (0 - 77).\n");
				exit(2);
			}
			dsize *= 26 * 128;
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (!hflag) {
		file = open("floppy", O_RDWR|O_CREAT|O_TRUNC, 0666);
		if (file < 0) {
			printf("can't open \"floppy\"\n");
			exit(1);
		}
		for (count = dsize; count > 0 ; count -= 512) {
			n = count > 512 ? 512 : count;
			lread(startad, n, buff);
			write(file, buff, n);
			startad += 512;
		}
	}
	if (rflag)
		exit(0);
	printf("Change Floppy, Hit return when done.\n");
	gets(buff);
	lseek(file, 0, 0);
	count = dsize;
	startad = -26 * 128;
	for ( ; count > 0 ; count -= 512) {
		n = count > 512 ? 512 : count;
		read(file, buff, n);
		lwrite(startad, n, buff);
		startad += 512;
	}
	exit(0);
}

rt_init()
{
	static initized = 0;
	int mode = 2;

	if (initized)
		return;
	if (rflag)
		mode = 0;
	initized = 1;
	if ((floppydes = open(flopname, mode)) < 0) {
		printf("Floppy open failed\n");
		exit(1);
	}
}

/*
 * Logical to physical adress translation
 */
long
trans(logical)
	register int logical;
{
	register int sector, bytes, track;

	logical += 26 * 128;
	bytes = (logical & 127);
	logical >>= 7;
	sector = logical % 26;
	if (sector >= 13)
		sector = sector*2 +1;
	else
		sector *= 2;
	sector += 26 + ((track = (logical / 26)) - 1) * 6;
	sector %= 26;
	return ((((track *26) + sector) << 7) + bytes);
}

lread(startad, count, obuff)
	register startad, count;
	register char *obuff;
{
	long trans();
	extern floppydes;

	rt_init();
	while ((count -= 128) >= 0) {
		lseek(floppydes, trans(startad), 0);
		read(floppydes, obuff, 128);
		obuff += 128;
		startad += 128;
	}
}

lwrite(startad, count, obuff)
	register startad, count;
	register char *obuff;
{
	long trans();
	extern floppydes;

	rt_init();
	while ((count -= 128) >= 0) {
		lseek(floppydes, trans(startad), 0);
		write(floppydes, obuff, 128);
		obuff += 128;
		startad += 128;
	}
}

usage()
{
	(void)fprintf(stderr, "usage: flcopy [-hr] [-f file] [-t ntracks]\n");
	exit(1);
}
