/*
 * Copyright 1995 Massachusetts Institute of Technology
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby
 * granted, provided that both the above copyright notice and this
 * permission notice appear in all copies, that both the above
 * copyright notice and this permission notice appear in all
 * supporting documentation, and that the name of M.I.T. not be used
 * in advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.  M.I.T. makes
 * no representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied
 * warranty.
 * 
 * THIS SOFTWARE IS PROVIDED BY M.I.T. ``AS IS''.  M.I.T. DISCLAIMS
 * ALL EXPRESS OR IMPLIED WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT
 * SHALL M.I.T. BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * devmenu - present a menu of installed devices of a specified
 * class or classes
 *
 * Garrett A. Wollman, April 1995
 */

#ifndef lint
static const char rcsid[] =
	"$Id: devmenu.c,v 1.1 1995/04/13 21:10:59 wollman Exp $";
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <sys/types.h>
#include <unistd.h>

#include <sys/devconf.h>
#include <sysexits.h>
#include <err.h>
#include <dialog.h>

#include "devmenu.h"

static enum dc_class interpret_class(char *str);
static int interpret_state(char *str);
static void dm_err_exit(int);
static int err_writefn(void *cookie, const char *val, int nchars);
static void usage(const char *);

static FILE *errfp;

int
main(int argc, char **argv)
{
	enum dc_class class = 0;
	enum mode { GENERIC, NETIF, INSTALL, DISK } mode = GENERIC;
	const char *title = 0, *hfile = 0;
	char **devnames = 0;
	int c;
	int states = 0;
	const char *fn = 0;
	const char *sel = "none";

	while ((c = getopt(argc, argv, "c:s:t:h:nidf:")) != EOF) {
		switch(c) {
		case 'c':
			class |= interpret_class(optarg);
			break;
		case 's':
			states |= interpret_state(optarg);
			break;
		case 't':
			title = optarg;
			break;
		case 'h':
			hfile = optarg;
			break;
		case 'n':
			mode = NETIF;
			break;
		case 'i':
			mode = INSTALL;
			break;
		case 'd':
			mode = DISK;
			break;
		case 'f':
			fn = optarg;
			break;
		case '?':
		default:
			usage(argv[0]);
			return EX_USAGE;
		}
	}

	if (optind < argc) {
		devnames = &argv[optind];
	}

	errfp = fwopen(0, err_writefn);
	if (!errfp) {
		err(EX_UNAVAILABLE, "fwopen");
	}

	setvbuf(errfp, (char *)0, _IOLBF, (size_t)0);

	init_dialog();
	err_set_exit(dm_err_exit);
	err_set_file(errfp);

	switch(mode) {
	case NETIF:
		sel = devmenu_netif(title, hfile, devnames, states);
		break;
	case INSTALL:
		sel = devmenu_common(title, hfile, devnames,
				     "Select an installation device",
				     "No installation devices found",
				     DC_CLS_DISK | DC_CLS_RDISK | DC_CLS_TAPE,
				     states);
		break;
	case DISK:
		sel = devmenu_common(title, hfile, devnames,
				     "Select a disk",
				     "No disks found",
				     DC_CLS_DISK,
				     states);
		break;
	case GENERIC:
		sel = devmenu_common(title, hfile, devnames,
				     "Select a device",
				     "No devices found",
				     class,
				     states);
		break;
	}
	err_set_file(0);
	fclose(errfp);
	end_dialog();
	err_set_exit(0);
	if (fn) {
		errfp = fopen(fn, "w");
		if (!errfp) {
			err(EX_OSERR, "fopen(%s)", fn);
		}
		fprintf(errfp, "%s\n", sel);
		fclose(errfp);
	} else {
		printf("%s\n", sel);
	}
	return 0;
}

static const char *classes[] = DC_CLASSNAMES;
#define NCLASSES ((sizeof classes)/(sizeof classes[0]))

static enum dc_class
interpret_class(char *str)
{
	int i;
	enum dc_class rv;

	for(i = 1; i < NCLASSES; i++) {
		if(! strcmp(classes[i], str)) {
			rv = (1 << (i - 1));
			break;
		}
	}
	if (i == NCLASSES) {
		err(EX_USAGE, "unknown class `%s'", str);
	}

	return rv;
}

static int
interpret_state(char *str)
{
	int rv = 0;
	int invert = 0;

	if (*str == '!' || *str == '~') {
		invert = 1;
		str++;
	}

	for (; *str; str++) {
		switch(*str) {
		case 'u':
		case 'U':
			rv |= 1 << DC_UNCONFIGURED;
			break;
		case '?':
			rv |= 1 << DC_UNKNOWN;
			break;
		case 'i':
		case 'I':
			rv |= 1 << DC_IDLE;
			break;
		case 'b':
		case 'B':
			rv |= 1 << DC_BUSY;
			break;
		default:
			err(EX_USAGE, "unknown state `%c'", *str);
		}
	}

	return (invert ? ~rv : rv);
}

static void
dm_err_exit(int rval)
{
	fflush(errfp);
	fclose(errfp);
	end_dialog();
	exit(rval);
}

static int
err_writefn(void *cookie, const char *val, int nchars)
{
	char buf[nchars + 1];

	strncpy(buf, val, nchars);
	buf[nchars] = '\0';

	dialog_msgbox("Error", buf, -1, -1, 1);
	return nchars;
}

static void
usage(const char *argv0)
{
	fprintf(stderr, "%s: usage:\n"
		"%s [-c class] [-s state] [-t title] [-h hfile] [-f outfile]"
		"[-n] [-i] [-d]\n", argv0, argv0);
	
}
