/*****************************************************************************/

/*
 * stlstty.c  -- stallion intelligent multiport special options.
 *
 * Copyright (c) 1996-1998 Greg Ungerer (gerg@stallion.oz.au).
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
 *	This product includes software developed by Greg Ungerer.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*****************************************************************************/

#ifndef lint
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include <machine/cdk.h>

/*****************************************************************************/

char	*version = "2.0.0";

/*
 *	Define some marker flags (to append to pflag values).
 */
#define	PFLAG_ON	0x40000000
#define	PFLAG_OFF	0x80000000

/*
 *	List of all options used. Use the option structure even though we
 *	don't use getopt!
 */
struct stloption {
	char	*name;
	int	val;
};


/*
 *	List of all options used. Use the option structure even though we
 *	don't use getopt!
 */
struct stloption longops[] = {
	{ "-V", 'V' },
	{ "--version", 'V' },
	{ "-?", 'h' },
	{ "-h", 'h' },
	{ "--help", 'h' },
	{ "maprts", (PFLAG_ON | P_MAPRTS) },
	{ "-maprts", (PFLAG_OFF | P_MAPRTS) },
	{ "mapcts", (PFLAG_ON | P_MAPCTS) },
	{ "-mapcts", (PFLAG_OFF | P_MAPCTS) },
	{ "rtslock", (PFLAG_ON | P_RTSLOCK) },
	{ "-rtslock", (PFLAG_OFF | P_RTSLOCK) },
	{ "ctslock", (PFLAG_ON | P_CTSLOCK) },
	{ "-ctslock", (PFLAG_OFF | P_CTSLOCK) },
	{ "loopback", (PFLAG_ON | P_LOOPBACK) },
	{ "-loopback", (PFLAG_OFF | P_LOOPBACK) },
	{ "fakedcd", (PFLAG_ON | P_FAKEDCD) },
	{ "-fakedcd", (PFLAG_OFF | P_FAKEDCD) },
	{ "dtrfollow", (PFLAG_ON | P_DTRFOLLOW) },
	{ "-dtrfollow", (PFLAG_OFF | P_DTRFOLLOW) },
	{ "rximin", (PFLAG_ON | P_RXIMIN) },
	{ "-rximin", (PFLAG_OFF | P_RXIMIN) },
	{ "rxitime", (PFLAG_ON | P_RXITIME) },
	{ "-rxitime", (PFLAG_OFF | P_RXITIME) },
	{ "rxthold", (PFLAG_ON | P_RXTHOLD) },
	{ "-rxthold", (PFLAG_OFF | P_RXTHOLD) },
	{ 0, 0 }
};

/*****************************************************************************/

/*
 *	Declare internal function prototypes here.
 */
static void	usage(void);

/*****************************************************************************/

static void usage()
{
	fprintf(stderr, "Usage: stlstty [OPTION] [ARGS]\n\n");
	fprintf(stderr, "  -h, --help            print this information\n");
	fprintf(stderr, "  -V, --version         show version information "
		"and exit\n");
	fprintf(stderr, "  maprts, -maprts       set RTS mapping to DTR "
		"on or off\n");
	fprintf(stderr, "  mapcts, -mapcts       set CTS mapping to DCD "
		"on or off\n");
	fprintf(stderr, "  rtslock, -rtslock     set RTS hardware flow "
		"on or off\n");
	fprintf(stderr, "  ctslock, -ctslock     set CTS hardware flow "
		"on or off\n");
	fprintf(stderr, "  fakedcd, -fakedcd     set fake DCD on or off\n");
	fprintf(stderr, "  dtrfollow, -dtrfollow set DTR data follow "
		"on or off\n");
	fprintf(stderr, "  loopback, -loopback   set port internal loop back "
		"on or off\n");
	fprintf(stderr, "  rximin, -rximin       set RX buffer minimum "
		"count on or off\n");
	fprintf(stderr, "  rxitime, -rxitime     set RX buffer minimum "
		"time on or off\n");
	fprintf(stderr, "  rxthold, -rxthold     set RX FIFO minimum "
		"count on or off\n");
	exit(0);
}

/*****************************************************************************/

void getpflags()
{
	unsigned long	pflags;

	if (ioctl(0, STL_GETPFLAG, &pflags) < 0)
		errx(1, "stdin not a Stallion serial port\n");

	if (pflags & P_MAPRTS)
		printf("maprts ");
	else
		printf("-maprts ");
	if (pflags & P_MAPCTS)
		printf("mapcts ");
	else
		printf("-mapcts ");

	if (pflags & P_RTSLOCK)
		printf("rtslock ");
	else
		printf("-rtslock ");
	if (pflags & P_CTSLOCK)
		printf("ctslock ");
	else
		printf("-ctslock ");

	if (pflags & P_FAKEDCD)
		printf("fakedcd ");
	else
		printf("-fakedcd ");
	if (pflags & P_DTRFOLLOW)
		printf("dtrfollow ");
	else
		printf("-dtrfollow ");
	if (pflags & P_LOOPBACK)
		printf("loopback ");
	else
		printf("-loopback ");
	printf("\n");

	if (pflags & P_RXIMIN)
		printf("rximin ");
	else
		printf("-rximin ");
	if (pflags & P_RXITIME)
		printf("rxitime ");
	else
		printf("-rxitime ");
	if (pflags & P_RXTHOLD)
		printf("rxthold ");
	else
		printf("-rxthold ");
	printf("\n");
}

/*****************************************************************************/

void setpflags(unsigned long pflagin, unsigned long pflagout)
{
	unsigned long	pflags;

	if (ioctl(0, STL_GETPFLAG, &pflags) < 0)
		errx(1, "stdin not a Stallion serial port\n");
	

	pflags &= ~(pflagout & ~PFLAG_OFF);
	pflags |= (pflagin & ~PFLAG_ON);

	if (ioctl(0, STL_SETPFLAG, &pflags) < 0)
		err(1, "ioctl(SET_SETPFLAGS) failed");
}

/*****************************************************************************/

int main(int argc, char *argv[])
{
	unsigned long	pflagin, pflagout;
	int		optind, optfound;
	int		i, val;

	pflagin = 0;
	pflagout = 0;

	for (optind = 1; (optind < argc); optind++) {
		optfound = 0;
		for (i = 0; (longops[i].name[0] != 0) ; i++) {
			if (strcmp(argv[optind], &(longops[i].name[0])) == 0) {
				val = longops[i].val;
				optfound++;
				break;
			}
		}
		if (optfound == 0)
			errx(1, "invalid option '%s'\n", argv[optind]);

		switch (val) {
		case 'V':
			printf("stlstats version %s\n", version);
			exit(0);
			break;
		case 'h':
			usage();
			break;
		default:
			if (val & PFLAG_ON) {
				pflagin |= val;
			} else if (val & PFLAG_OFF) {
				pflagout |= val;
			} else {
				errx(1, "unknown option found, val=%x!\n", val);
			}
		}
	}

	if (pflagin | pflagout)
		setpflags(pflagin, pflagout);
	else
		getpflags();

	exit(0);
}

/*****************************************************************************/
