/*
 * Copyright (C) 1992-1994,2001 by Joerg Wunsch, Dresden
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

/*
 * FreeBSD:
 * format a floppy disk
 *
 * Added FD_GTYPE ioctl, verifying, proportional indicators.
 * Serge Vakulenko, vak@zebub.msk.su
 * Sat Dec 18 17:45:47 MSK 1993
 *
 * Final adaptation, change format/verify logic, add separate
 * format gap/interleave values
 * Andrew A. Chernov, ache@astral.msk.su
 * Thu Jan 27 00:47:24 MSK 1994
 */

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>

#include <sys/fdcio.h>

#include "fdutil.h"

static void
format_track(int fd, int cyl, int secs, int head, int rate,
	     int gaplen, int secsize, int fill,int interleave)
{
	struct fd_formb f;
	register int i,j;
	int il[FD_MAX_NSEC + 1];

	memset(il,0,sizeof il);
	for(j = 0, i = 1; i <= secs; i++) {
	    while(il[(j%secs)+1]) j++;
	    il[(j%secs)+1] = i;
	    j += interleave;
	}

	f.format_version = FD_FORMAT_VERSION;
	f.head = head;
	f.cyl = cyl;
	f.transfer_rate = rate;

	f.fd_formb_secshift = secsize;
	f.fd_formb_nsecs = secs;
	f.fd_formb_gaplen = gaplen;
	f.fd_formb_fillbyte = fill;
	for(i = 0; i < secs; i++) {
		f.fd_formb_cylno(i) = cyl;
		f.fd_formb_headno(i) = head;
		f.fd_formb_secno(i) = il[i+1];
		f.fd_formb_secsize(i) = secsize;
	}
	if(ioctl(fd, FD_FORM, (caddr_t)&f) < 0)
		err(1, "ioctl(FD_FORM)");
}

static int
verify_track(int fd, int track, int tracksize)
{
	static char *buf = 0;
	static int bufsz = 0;
	int fdopts = -1, ofdopts, rv = 0;

	if (ioctl(fd, FD_GOPTS, &fdopts) < 0)
		warn("warning: ioctl(FD_GOPTS)");
	else {
		ofdopts = fdopts;
		fdopts |= FDOPT_NORETRY;
		(void)ioctl(fd, FD_SOPTS, &fdopts);
	}

	if (bufsz < tracksize) {
		if (buf)
			free (buf);
		bufsz = tracksize;
		buf = 0;
	}
	if (! buf)
		buf = malloc (bufsz);
	if (! buf)
		errx(2, "out of memory");
	if (lseek (fd, (long) track*tracksize, 0) < 0)
		rv = -1;
	/* try twice reading it, without using the normal retrier */
	else if (read (fd, buf, tracksize) != tracksize
		 && read (fd, buf, tracksize) != tracksize)
		rv = -1;
	if(fdopts != -1)
		(void)ioctl(fd, FD_SOPTS, &ofdopts);
	return (rv);
}

static const char *
makename(const char *arg, const char *suffix)
{
	static char namebuff[20];	/* big enough for "/dev/fd0a"... */

	memset(namebuff, 0, 20);
	if(*arg == '\0') /* ??? */
		return arg;
	if(*arg == '/')  /* do not convert absolute pathnames */
		return arg;
	strcpy(namebuff, _PATH_DEV);
	strncat(namebuff, arg, 3);
	strcat(namebuff, suffix);
	return namebuff;
}

static void
usage (void)
{
	fprintf(stderr, "%s\n%s\n",
	"usage: fdformat [-y] [-q] [-n | -v] [-f #] [-c #] [-s #] [-h #]",
	"                [-r #] [-g #] [-i #] [-S #] [-F #] [-t #] device_name");
	exit(2);
}

static int
yes (void)
{
	char reply [256], *p;

	reply[sizeof(reply)-1] = 0;
	for (;;) {
		fflush(stdout);
		if (! fgets (reply, sizeof(reply)-1, stdin))
			return (0);
		for (p=reply; *p==' ' || *p=='\t'; ++p)
			continue;
		if (*p=='y' || *p=='Y')
			return (1);
		if (*p=='n' || *p=='N' || *p=='\n' || *p=='\r')
			return (0);
		printf("Answer `yes' or `no': ");
	}
}

int
main(int argc, char **argv)
{
	int format = -1, cyls = -1, secs = -1, heads = -1, intleave = -1;
	int rate = -1, gaplen = -1, secsize = -1, steps = -1;
	int fill = 0xf6, quiet = 0, verify = 1, verify_only = 0, confirm = 0;
	int fd, c, i, track, error, tracks_per_dot, bytes_per_track, errs;
	int fdopts;
	const char *device, *suffix;
	struct fd_type fdt;
#define MAXPRINTERRS 10
	struct fdc_status fdcs[MAXPRINTERRS];

	while((c = getopt(argc, argv, "f:c:s:h:r:g:S:F:t:i:qyvn")) != -1)
		switch(c) {
		case 'f':	/* format in kilobytes */
			format = atoi(optarg);
			break;

		case 'c':	/* # of cyls */
			cyls = atoi(optarg);
			break;

		case 's':	/* # of secs per track */
			secs = atoi(optarg);
			break;

		case 'h':	/* # of heads */
			heads = atoi(optarg);
			break;

		case 'r':	/* transfer rate, kilobyte/sec */
			rate = atoi(optarg);
			break;

		case 'g':	/* length of GAP3 to format with */
			gaplen = atoi(optarg);
			break;

		case 'S':	/* sector size shift factor (1 << S)*128 */
			secsize = atoi(optarg);
			break;

		case 'F':	/* fill byte, C-like notation allowed */
			fill = (int)strtol(optarg, (char **)0, 0);
			break;

		case 't':	/* steps per track */
			steps = atoi(optarg);
			break;

		case 'i':       /* interleave factor */
			intleave = atoi(optarg);
			break;

		case 'q':
			quiet = 1;
			break;

		case 'y':
			confirm = 1;
			break;

		case 'n':
			verify = 0;
			break;

		case 'v':
			verify = 1;
			verify_only = 1;
			break;

		case '?': default:
			usage();
		}

	if(optind != argc - 1)
		usage();

	switch(format) {
	default:
		errx(2, "bad floppy size: %dK", format);
	case -1:   suffix = "";      break;
	case 360:  suffix = ".360";  break;
	case 640:  suffix = ".640";  break;
	case 720:  suffix = ".720";  break;
	case 800:  suffix = ".800";  break;
	case 820:  suffix = ".820";  break;
	case 1200: suffix = ".1200"; break;
	case 1232: suffix = ".1232"; break;
	case 1440: suffix = ".1440"; break;
	case 1480: suffix = ".1480"; break;
	case 1720: suffix = ".1720"; break;
	}

	device = makename(argv[optind], suffix);

	if((fd = open(device, O_RDWR)) < 0)
		err(1, "%s", device);

	if(ioctl(fd, FD_GTYPE, &fdt) < 0)
		errx(1, "not a floppy disk: %s", device);
	fdopts = FDOPT_NOERRLOG;
	if (ioctl(fd, FD_SOPTS, &fdopts) == -1)
		err(1, "ioctl(FD_SOPTS, FDOPT_NOERRLOG)");

	switch(rate) {
	case -1:  break;
	case 250: fdt.trans = FDC_250KBPS; break;
	case 300: fdt.trans = FDC_300KBPS; break;
	case 500: fdt.trans = FDC_500KBPS; break;
	default:
		errx(2, "invalid transfer rate: %d", rate);
	}

	if (cyls >= 0)    fdt.tracks = cyls;
	if (secs >= 0)    fdt.sectrac = secs;
	if (fdt.sectrac > FD_MAX_NSEC)
		errx(2, "too many sectors per track, max value is %d", FD_MAX_NSEC);
	if (heads >= 0)   fdt.heads = heads;
	if (gaplen >= 0)  fdt.f_gap = gaplen;
	if (secsize >= 0) fdt.secsize = secsize;
	if (steps >= 0)   fdt.steptrac = steps;
	if (intleave >= 0) fdt.f_inter = intleave;

	bytes_per_track = fdt.sectrac * (1<<fdt.secsize) * 128;

	/* XXX  20/40 = 0.5 */
	tracks_per_dot = (fdt.tracks * fdt.heads + 20) / 40;

	if (verify_only) {
		if(!quiet)
			printf("Verify %dK floppy `%s'.\n",
				fdt.tracks * fdt.heads * bytes_per_track / 1024,
				device);
	}
	else if(!quiet && !confirm) {
		printf("Format %dK floppy `%s'? (y/n): ",
			fdt.tracks * fdt.heads * bytes_per_track / 1024,
			device);
		if(! yes ()) {
			printf("Not confirmed.\n");
			return 3;
		}
	}

	/*
	 * Formatting.
	 */
	if(!quiet) {
		printf("Processing ");
		for (i = 0; i < (fdt.tracks * fdt.heads) / tracks_per_dot; i++)
			putchar('-');
		printf("\rProcessing ");
		fflush(stdout);
	}

	error = errs = 0;

	for (track = 0; track < fdt.tracks * fdt.heads; track++) {
		if (!verify_only) {
			format_track(fd, track / fdt.heads, fdt.sectrac,
				track % fdt.heads, fdt.trans, fdt.f_gap,
				fdt.secsize, fill, fdt.f_inter);
			if(!quiet && !((track + 1) % tracks_per_dot)) {
				putchar('F');
				fflush(stdout);
			}
		}
		if (verify) {
			if (verify_track(fd, track, bytes_per_track) < 0) {
				error = 1;
				if (errs < MAXPRINTERRS && errno == EIO) {
					if (ioctl(fd, FD_GSTAT, fdcs + errs) ==
					    -1)
						errx(1,
					"floppy IO error, but no FDC status");
					errs++;
				}
			}
			if(!quiet && !((track + 1) % tracks_per_dot)) {
				if (!verify_only)
					putchar('\b');
				if (error) {
					putchar('E');
					error = 0;
				}
				else
					putchar('V');
				fflush(stdout);
			}
		}
	}
	if(!quiet)
		printf(" done.\n");

	if (!quiet && errs) {
		fflush(stdout);
		fprintf(stderr, "Errors encountered:\nCyl Head Sect   Error\n");
		for (i = 0; i < errs && i < MAXPRINTERRS; i++) {
			fprintf(stderr, " %2d   %2d   %2d   ",
				fdcs[i].status[3], fdcs[i].status[4],
				fdcs[i].status[5]);
			printstatus(fdcs + i, 1);
			putc('\n', stderr);
		}
		if (errs >= MAXPRINTERRS)
			fprintf(stderr, "(Further errors not printed.)\n");
	}

	return errs != 0;
}
