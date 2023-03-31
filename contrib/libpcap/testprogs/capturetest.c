/*
 * Copyright (c) 1988, 1989, 1990, 1991, 1992, 1993, 1994, 1995, 1996, 1997, 2000
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: (1) source code distributions
 * retain the above copyright notice and this paragraph in its entirety, (2)
 * distributions including binary code include the above copyright notice and
 * this paragraph in its entirety in the documentation or other materials
 * provided with the distribution, and (3) all advertising materials mentioning
 * features or use of this software display the following acknowledgement:
 * ``This product includes software developed by the University of California,
 * Lawrence Berkeley Laboratory and its contributors.'' Neither the name of
 * the University nor the names of its contributors may be used to endorse
 * or promote products derived from this software without specific prior
 * written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#include "varattrs.h"

#ifndef lint
static const char copyright[] _U_ =
    "@(#) Copyright (c) 1988, 1989, 1990, 1991, 1992, 1993, 1994, 1995, 1996, 1997, 2000\n\
The Regents of the University of California.  All rights reserved.\n";
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <limits.h>
#ifdef _WIN32
  #include "getopt.h"
#else
  #include <unistd.h>
#endif
#include <errno.h>
#ifndef _WIN32
  #include <signal.h>
#endif
#include <sys/types.h>

#include <pcap.h>

#include "pcap/funcattrs.h"

#ifdef _WIN32
  #include "portability.h"
#endif

static char *program_name;

/* Forwards */
static void countme(u_char *, const struct pcap_pkthdr *, const u_char *);
static void PCAP_NORETURN usage(void);
static void PCAP_NORETURN error(const char *, ...) PCAP_PRINTFLIKE(1, 2);
static void warning(const char *, ...) PCAP_PRINTFLIKE(1, 2);
static char *copy_argv(char **);

static pcap_t *pd;
#ifndef _WIN32
static int breaksigint = 0;
#endif

#ifndef _WIN32
static void
sigint_handler(int signum _U_)
{
	if (breaksigint)
		pcap_breakloop(pd);
}
#endif

#ifdef _WIN32
/*
 * We don't have UN*X-style signals, so we don't have anything to test.
 */
#define B_OPTION	""
#define R_OPTION	""
#define S_OPTION	""
#else
/*
 * We do have UN*X-style signals (we assume that "not Windows" means "UN*X").
 */
#define B_OPTION	"b"
#define R_OPTION	"r"
#define S_OPTION	"s"
#endif

#define COMMAND_OPTIONS	B_OPTION "i:mn" R_OPTION S_OPTION "t:"
#define USAGE_OPTIONS	"-" B_OPTION "mn" R_OPTION S_OPTION

int
main(int argc, char **argv)
{
	register int op;
	register char *cp, *cmdbuf, *device;
	long longarg;
	char *p;
	int timeout = 1000;
	int immediate = 0;
	int nonblock = 0;
#ifndef _WIN32
	int sigrestart = 0;
	int catchsigint = 0;
#endif
	pcap_if_t *devlist;
	bpf_u_int32 localnet, netmask;
	struct bpf_program fcode;
	char ebuf[PCAP_ERRBUF_SIZE];
	int status;
	int packet_count;

	device = NULL;
	if ((cp = strrchr(argv[0], '/')) != NULL)
		program_name = cp + 1;
	else
		program_name = argv[0];

	opterr = 0;
	while ((op = getopt(argc, argv, COMMAND_OPTIONS)) != -1) {
		switch (op) {

#ifndef _WIN32
		case 'b':
			breaksigint = 1;
			break;
#endif

		case 'i':
			device = optarg;
			break;

		case 'm':
			immediate = 1;
			break;

		case 'n':
			nonblock = 1;
			break;

#ifndef _WIN32
		case 'r':
			sigrestart = 1;
			break;

		case 's':
			catchsigint = 1;
			break;
#endif

		case 't':
			longarg = strtol(optarg, &p, 10);
			if (p == optarg || *p != '\0') {
				error("Timeout value \"%s\" is not a number",
				    optarg);
				/* NOTREACHED */
			}
			if (longarg < 0) {
				error("Timeout value %ld is negative", longarg);
				/* NOTREACHED */
			}
			if (longarg > INT_MAX) {
				error("Timeout value %ld is too large (> %d)",
				    longarg, INT_MAX);
				/* NOTREACHED */
			}
			timeout = (int)longarg;
			break;

		default:
			usage();
			/* NOTREACHED */
		}
	}

	if (device == NULL) {
		if (pcap_findalldevs(&devlist, ebuf) == -1)
			error("%s", ebuf);
		if (devlist == NULL)
			error("no interfaces available for capture");
		device = strdup(devlist->name);
		pcap_freealldevs(devlist);
	}
	*ebuf = '\0';

#ifndef _WIN32
	/*
	 * If we were told to catch SIGINT, do so.
	 */
	if (catchsigint) {
		struct sigaction action;

		action.sa_handler = sigint_handler;
		sigemptyset(&action.sa_mask);

		/*
		 * Should SIGINT interrupt, or restart, system calls?
		 */
		action.sa_flags = sigrestart ? SA_RESTART : 0;

		if (sigaction(SIGINT, &action, NULL) == -1)
			error("Can't catch SIGINT: %s\n",
			    strerror(errno));
	}
#endif

	pd = pcap_create(device, ebuf);
	if (pd == NULL)
		error("%s", ebuf);
	status = pcap_set_snaplen(pd, 65535);
	if (status != 0)
		error("%s: pcap_set_snaplen failed: %s",
			    device, pcap_statustostr(status));
	if (immediate) {
		status = pcap_set_immediate_mode(pd, 1);
		if (status != 0)
			error("%s: pcap_set_immediate_mode failed: %s",
			    device, pcap_statustostr(status));
	}
	status = pcap_set_timeout(pd, timeout);
	if (status != 0)
		error("%s: pcap_set_timeout failed: %s",
		    device, pcap_statustostr(status));
	status = pcap_activate(pd);
	if (status < 0) {
		/*
		 * pcap_activate() failed.
		 */
		error("%s: %s\n(%s)", device,
		    pcap_statustostr(status), pcap_geterr(pd));
	} else if (status > 0) {
		/*
		 * pcap_activate() succeeded, but it's warning us
		 * of a problem it had.
		 */
		warning("%s: %s\n(%s)", device,
		    pcap_statustostr(status), pcap_geterr(pd));
	}
	if (pcap_lookupnet(device, &localnet, &netmask, ebuf) < 0) {
		localnet = 0;
		netmask = 0;
		warning("%s", ebuf);
	}
	cmdbuf = copy_argv(&argv[optind]);

	if (pcap_compile(pd, &fcode, cmdbuf, 1, netmask) < 0)
		error("%s", pcap_geterr(pd));

	if (pcap_setfilter(pd, &fcode) < 0)
		error("%s", pcap_geterr(pd));
	if (pcap_setnonblock(pd, nonblock, ebuf) == -1)
		error("pcap_setnonblock failed: %s", ebuf);
	printf("Listening on %s\n", device);
	for (;;) {
		packet_count = 0;
		status = pcap_dispatch(pd, -1, countme,
		    (u_char *)&packet_count);
		if (status < 0)
			break;
		if (status != 0) {
			printf("%d packets seen, %d packets counted after pcap_dispatch returns\n",
			    status, packet_count);
			struct pcap_stat ps;
			pcap_stats(pd, &ps);
			printf("%d ps_recv, %d ps_drop, %d ps_ifdrop\n",
			    ps.ps_recv, ps.ps_drop, ps.ps_ifdrop);
		}
	}
	if (status == -2) {
		/*
		 * We got interrupted, so perhaps we didn't
		 * manage to finish a line we were printing.
		 * Print an extra newline, just in case.
		 */
		putchar('\n');
		printf("Broken out of loop from SIGINT handler\n");
	}
	(void)fflush(stdout);
	if (status == -1) {
		/*
		 * Error.  Report it.
		 */
		(void)fprintf(stderr, "%s: pcap_dispatch: %s\n",
		    program_name, pcap_geterr(pd));
	}
	pcap_close(pd);
	pcap_freecode(&fcode);
	free(cmdbuf);
	exit(status == -1 ? 1 : 0);
}

static void
countme(u_char *user, const struct pcap_pkthdr *h _U_, const u_char *sp _U_)
{
	int *counterp = (int *)user;

	(*counterp)++;
}

static void
usage(void)
{
	(void)fprintf(stderr, "Usage: %s [ " USAGE_OPTIONS " ] [ -i interface ] [ -t timeout] [expression]\n",
	    program_name);
	exit(1);
}

/* VARARGS */
static void
error(const char *fmt, ...)
{
	va_list ap;

	(void)fprintf(stderr, "%s: ", program_name);
	va_start(ap, fmt);
	(void)vfprintf(stderr, fmt, ap);
	va_end(ap);
	if (*fmt) {
		fmt += strlen(fmt);
		if (fmt[-1] != '\n')
			(void)fputc('\n', stderr);
	}
	exit(1);
	/* NOTREACHED */
}

/* VARARGS */
static void
warning(const char *fmt, ...)
{
	va_list ap;

	(void)fprintf(stderr, "%s: WARNING: ", program_name);
	va_start(ap, fmt);
	(void)vfprintf(stderr, fmt, ap);
	va_end(ap);
	if (*fmt) {
		fmt += strlen(fmt);
		if (fmt[-1] != '\n')
			(void)fputc('\n', stderr);
	}
}

/*
 * Copy arg vector into a new buffer, concatenating arguments with spaces.
 */
static char *
copy_argv(register char **argv)
{
	register char **p;
	register size_t len = 0;
	char *buf;
	char *src, *dst;

	p = argv;
	if (*p == 0)
		return 0;

	while (*p)
		len += strlen(*p++) + 1;

	buf = (char *)malloc(len);
	if (buf == NULL)
		error("copy_argv: malloc");

	p = argv;
	dst = buf;
	while ((src = *p++) != NULL) {
		while ((*dst++ = *src++) != '\0')
			;
		dst[-1] = ' ';
	}
	dst[-1] = '\0';

	return buf;
}
