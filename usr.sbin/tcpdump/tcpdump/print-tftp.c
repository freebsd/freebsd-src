/*
 * Copyright (c) 1988-1990 The Regents of the University of California.
 * All rights reserved.
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
    "@(#) $Header: print-tftp.c,v 1.13 91/04/19 10:46:57 mccanne Exp $ (LBL)";
#endif

#include <stdio.h>
#include <sys/param.h>
#include <sys/types.h>
#include <arpa/tftp.h>

#include "interface.h"
#include <strings.h>
#include <ctype.h>

struct int2str {
	int code;
	char *str;
};

/* op code to string mapping */
static struct int2str op2str[] = {
	RRQ, "RRQ",			/* read request */
	WRQ, "WRQ",			/* write request */
	DATA, "DATA",			/* data packet */
	ACK, "ACK",			/* acknowledgement */
	ERROR, "ERROR",			/* error code */
	0, 0
};

/* error code to string mapping */
static struct int2str err2str[] = {
	EUNDEF, "EUNDEF",		/* not defined */
	ENOTFOUND, "ENOTFOUND",		/* file not found */
	EACCESS, "EACCESS",		/* access violation */
	ENOSPACE, "ENOSPACE",		/* disk full or allocation exceeded *?
	EBADOP, "EBADOP",		/* illegal TFTP operation */
	EBADID, "EBADID",		/* unknown transfer ID */
	EEXISTS, "EEXISTS",		/* file already exists */
	ENOUSER, "ENOUSER",		/* no such user */
	0, 0
};

/*
 * Print trivial file transfer program requests
 */
void
tftp_print(tp, length)
	register struct tftphdr *tp;
	int length;
{
	register struct int2str *ts;
	register u_char *ep;
#define TCHECK(var, l) if ((u_char *)&(var) > ep - l) goto trunc
	static char tstr[] = " [|tftp]";

	/* 'ep' points to the end of avaible data. */
	ep = (u_char *)snapend;

	/* Print length */
	printf(" %d", length);

	/* Print tftp request type */
	TCHECK(tp->th_opcode, sizeof(tp->th_opcode));
	NTOHS(tp->th_opcode);
	putchar(' ');
	for (ts = op2str; ts->str; ++ts)
		if (ts->code == tp->th_opcode) {
			fputs(ts->str, stdout);
			break;
		}
	if (ts->str == 0) {
		/* Bail if bogus opcode */
		printf("tftp-#%d", tp->th_opcode);
		return;
	}

	switch (tp->th_opcode) {

	case RRQ:
	case WRQ:
		putchar(' ');
		if (printfn((u_char *)tp->th_stuff, ep)) {
			fputs(&tstr[1], stdout);
			return;
		}
		break;

	case DATA:
		TCHECK(tp->th_block, sizeof(tp->th_block));
		NTOHS(tp->th_block);
		printf(" block %d", tp->th_block);
		break;

	case ACK:
		break;

	case ERROR:
		/* Print error code string */
		TCHECK(tp->th_code, sizeof(tp->th_code));
		NTOHS(tp->th_code);
		putchar(' ');
		for (ts = err2str; ts->str; ++ts)
			if (ts->code == tp->th_code) {
				fputs(ts->str, stdout);
				break;
			}
		if (ts->str == 0)
			printf("tftp-err-#%d", tp->th_code);

		/* Print error message string */
		putchar(' ');
		if (printfn((u_char *)tp->th_data, ep)) {
			fputs(&tstr[1], stdout);
			return;
		}
		break;

	default:
		/* We shouldn't get here */
		printf("(unknown #%d)", tp->th_opcode);
		break;
	}
	return;
trunc:
	fputs(tstr, stdout);
#undef TCHECK
}
