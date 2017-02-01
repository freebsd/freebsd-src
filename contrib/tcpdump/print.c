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
 *
 * Support for splitting captures into multiple files with a maximum
 * file size:
 *
 * Copyright (c) 2001
 *	Seth Webster <swebster@sst.ll.mit.edu>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include <netdissect-stdinc.h>

#include "netdissect.h"
#include "addrtoname.h"
#include "print.h"

static void	ndo_default_print(netdissect_options *ndo, const u_char *bp,
		    u_int length);

static void	ndo_error(netdissect_options *ndo, const char *fmt, ...)
		    __attribute__((noreturn))
#ifdef __ATTRIBUTE___FORMAT_OK
		    __attribute__((format (printf, 2, 3)))
#endif /* __ATTRIBUTE___FORMAT_OK */
		    ;
static void	ndo_warning(netdissect_options *ndo, const char *fmt, ...)
#ifdef __ATTRIBUTE___FORMAT_OK
		    __attribute__((format (printf, 2, 3)))
#endif /* __ATTRIBUTE___FORMAT_OK */
		    ;

static int	ndo_printf(netdissect_options *ndo, const char *fmt, ...)
#ifdef __ATTRIBUTE___FORMAT_OK
		     __attribute ((format (printf, 2, 3)))
#endif /* __ATTRIBUTE___FORMAT_OK */
		     ;

void
init_print(netdissect_options *ndo, uint32_t localnet, uint32_t mask,
    uint32_t timezone_offset)
{

	thiszone = timezone_offset;
	init_addrtoname(ndo, localnet, mask);
	init_checksum();
}

void
pretty_print_packet(netdissect_options *ndo, const struct pcap_pkthdr *h,
    const u_char * __capability sp, u_int packets_captured)
{
	u_int hdrlen;

	if(ndo->ndo_packet_number)
		ND_PRINT((ndo, "%5u  ", packets_captured));

	ts_print(ndo, &h->ts);

	/*
	 * Some printers want to check that they're not walking off the
	 * end of the packet.
	 * Rather than pass it all the way down, we set this member
	 * of the netdissect_options structure.
	 */
	ndo->ndo_snapend = sp + h->caplen;

        hdrlen = (ndo->ndo_if_printer)(ndo, h, sp);

	/*
	 * Restore the original snapend, as a printer might have
	 * changed it.
	 */
	ndo->ndo_snapend = sp + h->caplen;

	if (ndo->ndo_Xflag) {
		/*
		 * Print the raw packet data in hex and ASCII.
		 */
		if (ndo->ndo_Xflag > 1) {
			/*
			 * Include the link-layer header.
			 */
			hex_and_ascii_print(ndo, "\n\t", sp, h->caplen);
		} else {
			/*
			 * Don't include the link-layer header - and if
			 * we have nothing past the link-layer header,
			 * print nothing.
			 */
			if (h->caplen > hdrlen)
				hex_and_ascii_print(ndo, "\n\t", sp + hdrlen,
				    h->caplen - hdrlen);
		}
	} else if (ndo->ndo_xflag) {
		/*
		 * Print the raw packet data in hex.
		 */
		if (ndo->ndo_xflag > 1) {
			/*
			 * Include the link-layer header.
			 */
                        hex_print(ndo, "\n\t", sp, h->caplen);
		} else {
			/*
			 * Don't include the link-layer header - and if
			 * we have nothing past the link-layer header,
			 * print nothing.
			 */
			if (h->caplen > hdrlen)
				hex_print(ndo, "\n\t", sp + hdrlen,
                                          h->caplen - hdrlen);
		}
	} else if (ndo->ndo_Aflag) {
		/*
		 * Print the raw packet data in ASCII.
		 */
		if (ndo->ndo_Aflag > 1) {
			/*
			 * Include the link-layer header.
			 */
			ascii_print(ndo, sp, h->caplen);
		} else {
			/*
			 * Don't include the link-layer header - and if
			 * we have nothing past the link-layer header,
			 * print nothing.
			 */
			if (h->caplen > hdrlen)
				ascii_print(ndo, sp + hdrlen, h->caplen - hdrlen);
		}
	}

	ND_PRINT((ndo, "\n"));
}

/*
 * By default, print the specified data out in hex and ASCII.
 */
static void
ndo_default_print(netdissect_options *ndo, const u_char *bp, u_int length)
{
	hex_and_ascii_print(ndo, "\n\t", bp, length); /* pass on lf and indentation string */
}

/* VARARGS */
static void
ndo_error(netdissect_options *ndo, const char *fmt, ...)
{
	va_list ap;

	if(ndo->program_name)
		(void)fprintf(stderr, "%s: ", ndo->program_name);
	va_start(ap, fmt);
	(void)vfprintf(stderr, fmt, ap);
	va_end(ap);
	if (*fmt) {
		fmt += strlen(fmt);
		if (fmt[-1] != '\n')
			(void)fputc('\n', stderr);
	}
	nd_cleanup();
	exit(1);
	/* NOTREACHED */
}

/* VARARGS */
static void
ndo_warning(netdissect_options *ndo, const char *fmt, ...)
{
	va_list ap;

	if(ndo->program_name)
		(void)fprintf(stderr, "%s: ", ndo->program_name);
	(void)fprintf(stderr, "WARNING: ");
	va_start(ap, fmt);
	(void)vfprintf(stderr, fmt, ap);
	va_end(ap);
	if (*fmt) {
		fmt += strlen(fmt);
		if (fmt[-1] != '\n')
			(void)fputc('\n', stderr);
	}
}

static int
ndo_printf(netdissect_options *ndo, const char *fmt, ...)
{
	va_list args;
	int ret;

	va_start(args, fmt);
	ret = vfprintf(stdout, fmt, args);
	va_end(args);

	if (ret < 0)
		ndo_error(ndo, "Unable to write output: %s", strerror(errno));
	return (ret);
}

void
ndo_set_function_pointers(netdissect_options *ndo)
{
	ndo->ndo_default_print=ndo_default_print;
	ndo->ndo_printf=ndo_printf;
	ndo->ndo_error=ndo_error;
	ndo->ndo_warning=ndo_warning;
}
/*
 * Local Variables:
 * c-style: whitesmith
 * c-basic-offset: 8
 * End:
 */
