/*
 * Copyright (c) 1990, 1991, 1993, 1994
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
 * Format and print trivial file transfer protocol packets.
 */

#ifndef lint
static char rcsid[] =
    "@(#) $Header: /pub/FreeBSD/FreeBSD-CVS/src/usr.sbin/tcpdump/tcpdump/print-tftp.c,v 1.2 1995/03/08 12:52:45 olah Exp $ (LBL)";
#endif

#include <sys/param.h>
#include <sys/time.h>
#include <sys/types.h>

#include <netinet/in.h>

#include <arpa/tftp.h>

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "interface.h"
#include "addrtoname.h"

/* op code to string mapping */
static struct token op2str[] = {
	{ RRQ,		"RRQ" },	/* read request */
	{ WRQ,		"WRQ" },	/* write request */
	{ DATA,		"DATA" },	/* data packet */
	{ ACK,		"ACK" },	/* acknowledgement */
	{ ERROR,	"ERROR" },	/* error code */
	{ 0,		NULL }
};

/* error code to string mapping */
static struct token err2str[] = {
	{ EUNDEF,	"EUNDEF" },	/* not defined */
	{ ENOTFOUND,	"ENOTFOUND" },	/* file not found */
	{ EACCESS,	"EACCESS" },	/* access violation */
	{ ENOSPACE,	"ENOSPACE" },	/* disk full or allocation exceeded */
	{ EBADOP,	"EBADOP" },	/* illegal TFTP operation */
	{ EBADID,	"EBADID" },	/* unknown transfer ID */
	{ EEXISTS,	"EEXISTS" },	/* file already exists */
	{ ENOUSER,	"ENOUSER" },	/* no such user */
	{ 0,		NULL }
};

/*
 * Print trivial file transfer program requests
 */
void
tftp_print(register const u_char *bp, int length)
{
	register const struct tftphdr *tp;
	register const char *cp;
	register const u_char *ep, *p;
	register int opcode;
#define TCHECK(var, l) if ((u_char *)&(var) > ep - l) goto trunc
	static char tstr[] = " [|tftp]";

	tp = (const struct tftphdr *)bp;
	/* 'ep' points to the end of avaible data. */
	ep = snapend;

	/* Print length */
	printf(" %d", length);

	/* Print tftp request type */
	TCHECK(tp->th_opcode, sizeof(tp->th_opcode));
	opcode = ntohs(tp->th_opcode);
	cp = tok2str(op2str, "tftp-#%d", opcode);
	printf(" %s", cp);
	/* Bail if bogus opcode */
	if (*cp == 't')
		return;

	switch (opcode) {

	case RRQ:
	case WRQ:
		putchar(' ');
		/*
		 * XXX Not all arpa/tftp.h's specify th_stuff as any
		 * array; use address of th_block instead
		 */
#ifdef notdef
		p = (u_char *)tp->th_stuff;
#else
		p = (u_char *)&tp->th_block;
#endif
		if (fn_print(p, ep)) {
			fputs(&tstr[1], stdout);
			return;
		}
		break;

	case DATA:
		TCHECK(tp->th_block, sizeof(tp->th_block));
		printf(" block %d", ntohs(tp->th_block));
		break;

	case ACK:
		break;

	case ERROR:
		/* Print error code string */
		TCHECK(tp->th_code, sizeof(tp->th_code));
		printf(" %s ", tok2str(err2str, "tftp-err-#%d",
				       ntohs(tp->th_code)));
		/* Print error message string */
		if (fn_print((const u_char *)tp->th_data, ep)) {
			fputs(&tstr[1], stdout);
			return;
		}
		break;

	default:
		/* We shouldn't get here */
		printf("(unknown #%d)", opcode);
		break;
	}
	return;
trunc:
	fputs(tstr, stdout);
#undef TCHECK
}
