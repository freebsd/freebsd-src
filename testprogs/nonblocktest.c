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

/*
 * Tests for pcap_set_nonblock / pcap_get_nonblock:
 * - idempotency
 * - set/get are symmetric
 * - get returns the same before/after activate
 * - pcap_breakloop works after setting nonblock on and then off
 *
 * Really this is meant to
 * be run manually under strace, to check for extra
 * calls to eventfd or close.
 */
#include <pcap.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

static pcap_t *pd;
static char *program_name = "nonblocktest";
/* Forwards */
static void PCAP_NORETURN usage(void);
static void PCAP_NORETURN error(const char *, ...) PCAP_PRINTFLIKE(1, 2);
static void warning(const char *, ...) PCAP_PRINTFLIKE(1, 2);

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

static void
usage(void)
{
	(void)fprintf(stderr, "Usage: %s [ -i interface ]\n",
	    program_name);
	exit(1);
}

static void
breakme(u_char *user _U_, const struct pcap_pkthdr *h _U_, const u_char *sp _U_)
{
	warning("using pcap_breakloop()");
	pcap_breakloop(pd);
}

int
main(int argc, char **argv)
{
	int status, op, i, ret;
	char *device;
	pcap_if_t *devlist;
	char ebuf[PCAP_ERRBUF_SIZE];

	device = NULL;
	while ((op = getopt(argc, argv, "i:sptnq")) != -1) {
		switch (op) {

		case 'i':
			device = optarg;
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
		warning("listening on %s", device);
		pcap_freealldevs(devlist);
	}
	*ebuf = '\0';
	pd = pcap_create(device, ebuf);
	if (pd == NULL)
		error("%s", ebuf);
	else if (*ebuf)
		warning("%s", ebuf);
	/* set nonblock before activate */
	if (pcap_setnonblock(pd, 1, ebuf) < 0)
		error("pcap_setnonblock failed: %s", ebuf);
	/* getnonblock just returns "not activated yet" */
	ret = pcap_getnonblock(pd, ebuf);
	if (ret != PCAP_ERROR_NOT_ACTIVATED)
		error("pcap_getnonblock unexpectedly succeeded");
	if ((status = pcap_activate(pd)) < 0)
		error("pcap_activate failed");
	ret = pcap_getnonblock(pd, ebuf);
	if (ret != 1)
		error( "pcap_getnonblock did not return nonblocking" );

	/* Set nonblock multiple times, ensure with strace that it's a noop */
	for (i=0; i<10; i++) {
		if (pcap_setnonblock(pd, 1, ebuf) < 0)
			error("pcap_setnonblock failed: %s", ebuf);
		ret = pcap_getnonblock(pd, ebuf);
		if (ret != 1)
			error( "pcap_getnonblock did not return nonblocking" );
	}
	/* Set block multiple times, ensure with strace that it's a noop */
	for (i=0; i<10; i++) {
		if (pcap_setnonblock(pd, 0, ebuf) < 0)
			error("pcap_setnonblock failed: %s", ebuf);
		ret = pcap_getnonblock(pd, ebuf);
		if (ret != 0)
			error( "pcap_getnonblock did not return blocking" );
	}

	/* Now pcap_loop forever, with a callback that
	 * uses pcap_breakloop to get out of forever */
	pcap_loop(pd, -1, breakme, NULL);

        /* Now test that pcap_setnonblock fails if we can't open the
         * eventfd. */
        if (pcap_setnonblock(pd, 1, ebuf) < 0)
                error("pcap_setnonblock failed: %s", ebuf);
        while (1) {
                ret = open("/dev/null", O_RDONLY);
                if (ret < 0)
                        break;
        }
        ret = pcap_setnonblock(pd, 0, ebuf);
        if (ret == 0)
                error("pcap_setnonblock succeeded even though file table is full");
        else
                warning("pcap_setnonblock failed as expected: %s", ebuf);
}
