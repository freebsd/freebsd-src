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

/* $FreeBSD$ */

if_printer	lookup_printer(int type);
if_ndo_printer	lookup_ndo_printer(int type);

/* XXX: Could be opaque to tcpdump.c */
struct print_info {
        netdissect_options *ndo;
        union {
                if_printer     printer;
                if_ndo_printer ndo_printer;
        } p;
        int ndo_type;
};


/* XXX: should be hidden */
int	tcpdump_printf(netdissect_options *ndo _U_, const char *fmt, ...);

/* XXX: should return a malloced pointer */
struct print_info	get_print_info(int type);

int	pretty_print_packet(struct print_info *print_info,
	    const struct pcap_pkthdr *h, const u_char *sp);

void	ndo_default_print(netdissect_options *ndo _U_, const u_char *bp,
	    u_int length);
void	default_print(const u_char *bp, u_int length);

void	ndo_error(netdissect_options *ndo _U_, const char *fmt, ...)
	    __attribute__ ((noreturn, format (printf, 2, 3)));
void	ndo_warning(netdissect_options *ndo _U_, const char *fmt, ...);
