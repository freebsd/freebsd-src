/*
 * Copyright (C) 1992-1993 by Joerg Wunsch, Dresden
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <strings.h>
#include <ctype.h>

#include <errno.h>
#include <machine/ioctl_fd.h>
#include <../i386/isa/fdreg.h>	/* XXX should be in <machine> dir */

static void
format_track(int fd, int cyl, int secs, int head, int rate,
	     int gaplen, int secsize, int fill)
{
	struct fd_formb f;
	register int i;

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
		f.fd_formb_secno(i) = i + 1;
		f.fd_formb_secsize(i) = secsize;
	}
	if(ioctl(fd, FD_FORM, (caddr_t)&f) < 0) {
		perror("\nfdformat: ioctl(FD_FORM)");
		exit(1);
	}
}

static int
verify_track(int fd, int track, int tracksize)
{
	static char *buf = 0;
	static int bufsz = 0;

	if (bufsz < tracksize) {
		if (buf)
			free (buf);
		bufsz = tracksize;
		buf = 0;
	}
	if (! buf)
		buf = malloc (bufsz);
	if (! buf) {
		fprintf (stderr, "\nfdformat: out of memory\n");
		exit (2);
	}
	if (lseek (fd, (long) track*tracksize, 0) < 0)
		return (-1);
	if (read (fd, buf, tracksize) != tracksize)
		return (-1);
	return (0);
}

static const char *
makename(const char *arg, const char *suffix)
{
	static char namebuff[20];	/* big enough for "/dev/rfd0a"... */

	memset(namebuff, 0, 20);
	if(*arg == '\0') /* ??? */
		return arg;
	if(*arg == '/')  /* do not convert absolute pathnames */
		return arg;
	strcpy(namebuff, "/dev/r");
	strncat(namebuff, arg, 3);
	strcat(namebuff, suffix);
	return namebuff;
}

static void
usage ()
{
	printf("Usage:\n\tfdformat [-q] [-n | -v] [-f #] [-c #] [-s #] [-h #]\n");
	printf("\t\t [-r #] [-g #] [-i #] [-S #] [-F #] [-t #] devname\n");
	printf("Options:\n");
	printf("\t-q\tsupress any normal output, don't ask for confirmation\n");
	printf("\t-n\tdon't verify floppy after formatting\n");
	printf("\t-v\tdon't format, verify only\n");
	printf("\t-f #\tspecify desired floppy capacity, in kilobytes;\n");
	printf("\t\tvalid choices are 360, 720, 800, 820, 1200, 1440, 1480, 1720\n");
	printf("\tdevname\tthe full name of floppy device or in short form fd0, fd1\n");
	printf("Obscure options:\n");
	printf("\t-c #\tspecify number of cylinders, 40 or 80\n");
	printf("\t-s #\tspecify number of sectors per track, 9, 10, 15 or 18\n");
	printf("\t-h #\tspecify number of floppy heads, 1 or 2\n");
	printf("\t-r #\tspecify data rate, 250, 300 or 500 kbps\n");
	printf("\t-g #\tspecify gap length\n");
	printf("\t-i #\tspecify interleave factor\n");
	printf("\t-S #\tspecify sector size, 0=128, 1=256, 2=512 bytes\n");
	printf("\t-F #\tspecify fill byte\n");
	printf("\t-t #\tnumber of steps per track\n");
	exit(2);
}

static int
yes ()
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
	int fill = 0xf6, quiet = 0, verify = 1, verify_only = 0;
	int fd, c, track, error, tracks_per_dot, bytes_per_track, errs;
	const char *devname, *suffix;
	struct fd_type fdt;

	while((c = getopt(argc, argv, "f:c:s:h:r:g:S:F:t:i:qvn")) != -1)
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
		fprintf(stderr, "fdformat: bad floppy size: %dK\n", format);
		exit(2);
	case -1:   suffix = "";      break;
	case 360:  suffix = ".360";  break;
	case 720:  suffix = ".720";  break;
	case 800:  suffix = ".800";  break;
	case 820:  suffix = ".820";  break;
	case 1200: suffix = ".1200"; break;
	case 1440: suffix = ".1440"; break;
	case 1480: suffix = ".1480"; break;
	case 1720: suffix = ".1720"; break;
	}

	devname = makename(argv[optind], suffix);

	if((fd = open(devname, O_RDWR)) < 0) {
		perror(devname);
		exit(1);
	}

	if(ioctl(fd, FD_GTYPE, &fdt) < 0) {
		fprintf(stderr, "fdformat: not a floppy disk: %s\n", devname);
		exit(1);
	}

	switch(rate) {
	case -1:  break;
	case 250: fdt.trans = FDC_250KBPS; break;
	case 300: fdt.trans = FDC_300KBPS; break;
	case 500: fdt.trans = FDC_500KBPS; break;
	default:
		fprintf(stderr, "fdformat: invalid transfer rate: %d\n", rate);
		exit(2);
	}

	if (cyls >= 0)    fdt.tracks = cyls;
	if (secs >= 0)    fdt.sectrac = secs;
	if (heads >= 0)   fdt.heads = heads;
	if (gaplen >= 0)  fdt.f_gap = gaplen;
	if (secsize >= 0) fdt.secsize = secsize;
	if (steps >= 0)   fdt.steptrac = steps;
	if (intleave >= 0) fdt.f_inter = intleave;
	if (fdt.f_inter != 1) {
		fprintf(stderr, "fdformat: can't format with interleave != 1 yet\n");
		exit(2);
	}

	bytes_per_track = fdt.sectrac * (1<<fdt.secsize) * 128;
	tracks_per_dot = fdt.tracks * fdt.heads / 40;

	if (verify_only) {
		if(!quiet)
			printf("Verify %dK floppy `%s'.\n",
				fdt.tracks * fdt.heads * bytes_per_track / 1024,
				devname);
	}
	else if(!quiet) {
		printf("Format %dK floppy `%s'? (y/n): ",
			fdt.tracks * fdt.heads * bytes_per_track / 1024,
			devname);
		if(! yes ()) {
			printf("Not confirmed.\n");
			return 0;
		}
	}

	/*
	 * Formatting.
	 */
	if(!quiet) {
		printf("Processing ----------------------------------------\r");
		printf("Processing ");
		fflush(stdout);
	}

	error = errs = 0;

	for (track = 0; track < fdt.tracks * fdt.heads; track++) {
		if (!verify_only) {
			format_track(fd, track / 2, fdt.sectrac, track & 1,
				fdt.trans, fdt.f_gap, fdt.secsize, fill);
			if(!quiet && !((track + 1) % tracks_per_dot)) {
				putchar('F');
				fflush(stdout);
			}
		}
		if (verify) {
			if (verify_track(fd, track, bytes_per_track) < 0)
				error = errs = 1;
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

	return errs;
}
