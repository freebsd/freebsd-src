/*
 * Copyright (c) 1990, 1991, 1992, 1993, 1994, 1995, 1996, 1997
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
 * Format and print ntp packets.
 *	By Jeffrey Mogul/DECWRL
 *	loosely based on print-bootp.c
 *
 * $FreeBSD$
 */

#ifndef lint
static const char rcsid[] _U_ =
    "@(#) $Header: /tcpdump/master/tcpdump/print-ntp.c,v 1.37.2.2 2003/11/16 08:51:36 guy Exp $ (LBL)";
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <tcpdump-stdinc.h>

#include <stdio.h>
#include <string.h>
#ifdef HAVE_STRFTIME
#include <time.h>
#endif

#include "interface.h"
#include "addrtoname.h"
#include "extract.h"
#ifdef MODEMASK
#undef MODEMASK					/* Solaris sucks */
#endif
#include "ntp.h"

static void p_sfix(const struct s_fixedpt *);
static void p_ntp_time(const struct l_fixedpt *);
static void p_ntp_delta(const struct l_fixedpt *, const struct l_fixedpt *);

/*
 * Print ntp requests
 */
void
ntp_print(register const u_char *cp, u_int length)
{
	register const struct ntpdata *bp;
	int mode, version, leapind;

	bp = (struct ntpdata *)cp;
	/* Note funny sized packets */
	if (length != sizeof(struct ntpdata))
		(void)printf(" [len=%d]", length);

	TCHECK(bp->status);

	version = (int)(bp->status & VERSIONMASK) >> 3;
	printf("NTPv%d", version);

	leapind = bp->status & LEAPMASK;
	switch (leapind) {

	case NO_WARNING:
		break;

	case PLUS_SEC:
		fputs(" +1s", stdout);
		break;

	case MINUS_SEC:
		fputs(" -1s", stdout);
		break;
	}

	mode = bp->status & MODEMASK;
	switch (mode) {

	case MODE_UNSPEC:	/* unspecified */
		fputs(" unspec", stdout);
		break;

	case MODE_SYM_ACT:	/* symmetric active */
		fputs(" sym_act", stdout);
		break;

	case MODE_SYM_PAS:	/* symmetric passive */
		fputs(" sym_pas", stdout);
		break;

	case MODE_CLIENT:	/* client */
		fputs(" client", stdout);
		break;

	case MODE_SERVER:	/* server */
		fputs(" server", stdout);
		break;

	case MODE_BROADCAST:	/* broadcast */
		fputs(" bcast", stdout);
		break;

	case MODE_RES1:		/* reserved */
		fputs(" res1", stdout);
		break;

	case MODE_RES2:		/* reserved */
		fputs(" res2", stdout);
		break;

	}

	TCHECK(bp->stratum);
	printf(", strat %d", bp->stratum);

	TCHECK(bp->ppoll);
	printf(", poll %d", bp->ppoll);

	/* Can't TCHECK bp->precision bitfield so bp->distance + 0 instead */
	TCHECK2(bp->distance, 0);
	printf(", prec %d", bp->precision);

	if (!vflag)
		return;

	TCHECK(bp->distance);
	fputs(" dist ", stdout);
	p_sfix(&bp->distance);

	TCHECK(bp->dispersion);
	fputs(", disp ", stdout);
	p_sfix(&bp->dispersion);

	TCHECK(bp->refid);
	fputs(", ref ", stdout);
	/* Interpretation depends on stratum */
	switch (bp->stratum) {

	case UNSPECIFIED:
		printf("(unspec)");
		break;

	case PRIM_REF:
		fn_printn((u_char *)&(bp->refid), 4, NULL);
		break;

	case INFO_QUERY:
		printf("%s INFO_QUERY", ipaddr_string(&(bp->refid)));
		/* this doesn't have more content */
		return;

	case INFO_REPLY:
		printf("%s INFO_REPLY", ipaddr_string(&(bp->refid)));
		/* this is too complex to be worth printing */
		return;

	default:
		printf("%s", ipaddr_string(&(bp->refid)));
		break;
	}

	TCHECK(bp->reftime);
	putchar('@');
	p_ntp_time(&(bp->reftime));

	TCHECK(bp->org);
	fputs(" orig ", stdout);
	p_ntp_time(&(bp->org));

	TCHECK(bp->rec);
	fputs(" rec ", stdout);
	p_ntp_delta(&(bp->org), &(bp->rec));

	TCHECK(bp->xmt);
	fputs(" xmt ", stdout);
	p_ntp_delta(&(bp->org), &(bp->xmt));

	return;

trunc:
	fputs(" [|ntp]", stdout);
}

static void
p_sfix(register const struct s_fixedpt *sfp)
{
	register int i;
	register int f;
	register float ff;

	i = EXTRACT_16BITS(&sfp->int_part);
	f = EXTRACT_16BITS(&sfp->fraction);
	ff = f / 65536.0;	/* shift radix point by 16 bits */
	f = ff * 1000000.0;	/* Treat fraction as parts per million */
	printf("%d.%06d", i, f);
}

#define	FMAXINT	(4294967296.0)	/* floating point rep. of MAXINT */

static void
p_ntp_time(register const struct l_fixedpt *lfp)
{
	register int32_t i;
	register u_int32_t uf;
	register u_int32_t f;
	register float ff;

	i = EXTRACT_32BITS(&lfp->int_part);
	uf = EXTRACT_32BITS(&lfp->fraction);
	ff = uf;
	if (ff < 0.0)		/* some compilers are buggy */
		ff += FMAXINT;
	ff = ff / FMAXINT;	/* shift radix point by 32 bits */
	f = ff * 1000000000.0;	/* treat fraction as parts per billion */
	printf("%u.%09d", i, f);

#ifdef HAVE_STRFTIME
	/*
	 * For extra verbosity, print the time in human-readable format.
	 */
	if (vflag > 1 && i) {
	    time_t seconds = i - JAN_1970;
	    struct tm *tm;
	    char time_buf[128];

	    tm = localtime(&seconds);
	    strftime(time_buf, sizeof (time_buf), "%Y/%m/%d %H:%M:%S", tm);
	    printf (" (%s)", time_buf);
	}
#endif
}

/* Prints time difference between *lfp and *olfp */
static void
p_ntp_delta(register const struct l_fixedpt *olfp,
	    register const struct l_fixedpt *lfp)
{
	register int32_t i;
	register u_int32_t u, uf;
	register u_int32_t ou, ouf;
	register u_int32_t f;
	register float ff;
	int signbit;

	u = EXTRACT_32BITS(&lfp->int_part);
	ou = EXTRACT_32BITS(&olfp->int_part);
	uf = EXTRACT_32BITS(&lfp->fraction);
	ouf = EXTRACT_32BITS(&olfp->fraction);
	if (ou == 0 && ouf == 0) {
		p_ntp_time(lfp);
		return;
	}

	i = u - ou;

	if (i > 0) {		/* new is definitely greater than old */
		signbit = 0;
		f = uf - ouf;
		if (ouf > uf)	/* must borrow from high-order bits */
			i -= 1;
	} else if (i < 0) {	/* new is definitely less than old */
		signbit = 1;
		f = ouf - uf;
		if (uf > ouf)	/* must carry into the high-order bits */
			i += 1;
		i = -i;
	} else {		/* int_part is zero */
		if (uf > ouf) {
			signbit = 0;
			f = uf - ouf;
		} else {
			signbit = 1;
			f = ouf - uf;
		}
	}

	ff = f;
	if (ff < 0.0)		/* some compilers are buggy */
		ff += FMAXINT;
	ff = ff / FMAXINT;	/* shift radix point by 32 bits */
	f = ff * 1000000000.0;	/* treat fraction as parts per billion */
	if (signbit)
		putchar('-');
	else
		putchar('+');
	printf("%d.%09d", i, f);
}

