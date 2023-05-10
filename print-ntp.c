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
 *	By Jeffrey Mogul/DECWRL
 *	loosely based on print-bootp.c
 */

/* \summary: Network Time Protocol (NTP) printer */

/*
 * specification:
 *
 * RFC 1119 - NTPv2
 * RFC 1305 - NTPv3
 * RFC 5905 - NTPv4
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "netdissect-stdinc.h"

#include <time.h>

#include "netdissect.h"
#include "addrtoname.h"
#include "extract.h"

#include "ntp.h"

/*
 * Based on ntp.h from the U of MD implementation
 *	This file is based on Version 2 of the NTP spec (RFC1119).
 */

/* rfc2030
 *                      1                   2                   3
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |LI | VN  |Mode |    Stratum    |     Poll      |   Precision   |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                          Root Delay                           |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                       Root Dispersion                         |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                     Reference Identifier                      |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                                                               |
 * |                   Reference Timestamp (64)                    |
 * |                                                               |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                                                               |
 * |                   Originate Timestamp (64)                    |
 * |                                                               |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                                                               |
 * |                    Receive Timestamp (64)                     |
 * |                                                               |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                                                               |
 * |                    Transmit Timestamp (64)                    |
 * |                                                               |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                 Key Identifier (optional) (32)                |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                                                               |
 * |                                                               |
 * |                 Message Digest (optional) (128)               |
 * |                                                               |
 * |                                                               |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */

/* Length of the NTP data message with the mandatory fields ("the header")
 * and without any optional fields (extension, Key Identifier,
 * Message Digest).
 */
#define NTP_TIMEMSG_MINLEN 48U

struct ntp_time_data {
	nd_uint8_t status;		/* status of local clock and leap info */
	nd_uint8_t stratum;		/* Stratum level */
	nd_int8_t ppoll;		/* poll value */
	nd_int8_t precision;
	struct s_fixedpt root_delay;
	struct s_fixedpt root_dispersion;
	nd_uint32_t refid;
	struct l_fixedpt ref_timestamp;
	struct l_fixedpt org_timestamp;
	struct l_fixedpt rec_timestamp;
	struct l_fixedpt xmt_timestamp;
	nd_uint32_t key_id;
	nd_uint8_t  message_digest[20];
};
/*
 *	Leap Second Codes (high order two bits)
 */
#define	NO_WARNING	0x00	/* no warning */
#define	PLUS_SEC	0x40	/* add a second (61 seconds) */
#define	MINUS_SEC	0x80	/* minus a second (59 seconds) */
#define	ALARM		0xc0	/* alarm condition (clock unsynchronized) */

/*
 *	Clock Status Bits that Encode Version
 */
#define	NTPVERSION_1	0x08
#define	VERSIONMASK	0x38
#define	VERSIONSHIFT	3
#define LEAPMASK	0xc0
#define LEAPSHIFT	6
#ifdef MODEMASK
#undef MODEMASK					/* Solaris sucks */
#endif
#define	MODEMASK	0x07
#define	MODESHIFT	0

/*
 *	Code values
 */
#define	MODE_UNSPEC	0	/* unspecified */
#define	MODE_SYM_ACT	1	/* symmetric active */
#define	MODE_SYM_PAS	2	/* symmetric passive */
#define	MODE_CLIENT	3	/* client */
#define	MODE_SERVER	4	/* server */
#define	MODE_BROADCAST	5	/* broadcast */
#define	MODE_CONTROL	6	/* control message */
#define	MODE_RES2	7	/* reserved */

/*
 *	Stratum Definitions
 */
#define	UNSPECIFIED	0
#define	PRIM_REF	1	/* radio clock */
#define	INFO_QUERY	62	/* **** THIS implementation dependent **** */
#define	INFO_REPLY	63	/* **** THIS implementation dependent **** */

static void p_sfix(netdissect_options *ndo, const struct s_fixedpt *);
static void p_ntp_delta(netdissect_options *, const struct l_fixedpt *, const struct l_fixedpt *);
static void p_poll(netdissect_options *, const int);

static const struct tok ntp_mode_values[] = {
    { MODE_UNSPEC,    "unspecified" },
    { MODE_SYM_ACT,   "symmetric active" },
    { MODE_SYM_PAS,   "symmetric passive" },
    { MODE_CLIENT,    "Client" },
    { MODE_SERVER,    "Server" },
    { MODE_BROADCAST, "Broadcast" },
    { MODE_CONTROL,   "Control Message" },
    { MODE_RES2,      "Reserved" },
    { 0, NULL }
};

static const struct tok ntp_leapind_values[] = {
    { NO_WARNING,     "" },
    { PLUS_SEC,       "+1s" },
    { MINUS_SEC,      "-1s" },
    { ALARM,          "clock unsynchronized" },
    { 0, NULL }
};

static const struct tok ntp_stratum_values[] = {
	{ UNSPECIFIED,	"unspecified" },
	{ PRIM_REF,	"primary reference" },
	{ 0, NULL }
};

/* draft-ietf-ntp-mode-6-cmds-02
 *  0                   1                   2                   3
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |LI |  VN |Mode |R|E|M| OpCode  |       Sequence Number         |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |            Status             |       Association ID          |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |            Offset             |            Count              |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                                                               |
 * /                    Data (up to 468 bytes)                     /
 * |                                                               |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                    Padding (optional)                         |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                                                               |
 * /              Authenticator (optional, 96 bytes)               /
 * |                                                               |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 *               Figure 1: NTP Control Message Header
 */

/* Length of the NTP control message with the mandatory fields ("the header")
 * and without any optional fields (Data, Padding, Authenticator).
 */
#define NTP_CTRLMSG_MINLEN 12U

struct ntp_control_data {
	nd_uint8_t	magic;		/* LI, VN, Mode */
	nd_uint8_t	control;	/* R, E, M, OpCode */
	nd_uint16_t	sequence;	/* Sequence Number */
	nd_uint16_t	status;		/* Status */
	nd_uint16_t	assoc;		/* Association ID */
	nd_uint16_t	offset;		/* Offset */
	nd_uint16_t	count;		/* Count */
	nd_uint8_t	data[564];	/* Data, [Padding, [Authenticator]] */
};

/*
 * Print NTP time requests and responses
 */
static void
ntp_time_print(netdissect_options *ndo,
	       const struct ntp_time_data *bp, u_int length)
{
	uint8_t stratum;

	if (length < NTP_TIMEMSG_MINLEN)
		goto invalid;

	stratum = GET_U_1(bp->stratum);
	ND_PRINT(", Stratum %u (%s)",
		stratum,
		tok2str(ntp_stratum_values, (stratum >=2 && stratum<=15) ? "secondary reference" : "reserved", stratum));

	ND_PRINT(", poll %d", GET_S_1(bp->ppoll));
	p_poll(ndo, GET_S_1(bp->ppoll));

	ND_PRINT(", precision %d", GET_S_1(bp->precision));

	ND_TCHECK_SIZE(&bp->root_delay);
	ND_PRINT("\n\tRoot Delay: ");
	p_sfix(ndo, &bp->root_delay);

	ND_TCHECK_SIZE(&bp->root_dispersion);
	ND_PRINT(", Root dispersion: ");
	p_sfix(ndo, &bp->root_dispersion);

	ND_TCHECK_4(bp->refid);
	ND_PRINT(", Reference-ID: ");
	/* Interpretation depends on stratum */
	switch (stratum) {

	case UNSPECIFIED:
		ND_PRINT("(unspec)");
		break;

	case PRIM_REF:
		if (nd_printn(ndo, (const u_char *)&(bp->refid), 4, ndo->ndo_snapend))
			goto trunc;
		break;

	case INFO_QUERY:
		ND_PRINT("%s INFO_QUERY", GET_IPADDR_STRING(bp->refid));
		/* this doesn't have more content */
		return;

	case INFO_REPLY:
		ND_PRINT("%s INFO_REPLY", GET_IPADDR_STRING(bp->refid));
		/* this is too complex to be worth printing */
		return;

	default:
		/* In NTPv4 (RFC 5905) refid is an IPv4 address or first 32 bits of
		   MD5 sum of IPv6 address */
		ND_PRINT("0x%08x", GET_BE_U_4(bp->refid));
		break;
	}

	ND_TCHECK_SIZE(&bp->ref_timestamp);
	ND_PRINT("\n\t  Reference Timestamp:  ");
	p_ntp_time(ndo, &(bp->ref_timestamp));

	ND_TCHECK_SIZE(&bp->org_timestamp);
	ND_PRINT("\n\t  Originator Timestamp: ");
	p_ntp_time(ndo, &(bp->org_timestamp));

	ND_TCHECK_SIZE(&bp->rec_timestamp);
	ND_PRINT("\n\t  Receive Timestamp:    ");
	p_ntp_time(ndo, &(bp->rec_timestamp));

	ND_TCHECK_SIZE(&bp->xmt_timestamp);
	ND_PRINT("\n\t  Transmit Timestamp:   ");
	p_ntp_time(ndo, &(bp->xmt_timestamp));

	ND_PRINT("\n\t    Originator - Receive Timestamp:  ");
	p_ntp_delta(ndo, &(bp->org_timestamp), &(bp->rec_timestamp));

	ND_PRINT("\n\t    Originator - Transmit Timestamp: ");
	p_ntp_delta(ndo, &(bp->org_timestamp), &(bp->xmt_timestamp));

	/* FIXME: this code is not aware of any extension fields */
	if (length == NTP_TIMEMSG_MINLEN + 4) {	/* Optional: key-id (crypto-NAK) */
		ND_PRINT("\n\tKey id: %u", GET_BE_U_4(bp->key_id));
	} else if (length == NTP_TIMEMSG_MINLEN + 4 + 16) {	/* Optional: key-id + 128-bit digest */
		ND_PRINT("\n\tKey id: %u", GET_BE_U_4(bp->key_id));
		ND_TCHECK_LEN(bp->message_digest, 16);
		ND_PRINT("\n\tAuthentication: %08x%08x%08x%08x",
			 GET_BE_U_4(bp->message_digest),
			 GET_BE_U_4(bp->message_digest + 4),
			 GET_BE_U_4(bp->message_digest + 8),
			 GET_BE_U_4(bp->message_digest + 12));
	} else if (length == NTP_TIMEMSG_MINLEN + 4 + 20) {	/* Optional: key-id + 160-bit digest */
		ND_PRINT("\n\tKey id: %u", GET_BE_U_4(bp->key_id));
		ND_TCHECK_LEN(bp->message_digest, 20);
		ND_PRINT("\n\tAuthentication: %08x%08x%08x%08x%08x",
			 GET_BE_U_4(bp->message_digest),
			 GET_BE_U_4(bp->message_digest + 4),
			 GET_BE_U_4(bp->message_digest + 8),
			 GET_BE_U_4(bp->message_digest + 12),
			 GET_BE_U_4(bp->message_digest + 16));
	} else if (length > NTP_TIMEMSG_MINLEN) {
		ND_PRINT("\n\t(%u more bytes after the header)", length - NTP_TIMEMSG_MINLEN);
	}
	return;

invalid:
	nd_print_invalid(ndo);
	ND_TCHECK_LEN(bp, length);
	return;

trunc:
	nd_print_trunc(ndo);
}

/*
 * Print NTP control message requests and responses
 */
static void
ntp_control_print(netdissect_options *ndo,
		  const struct ntp_control_data *cd, u_int length)
{
	uint8_t control, R, E, M, opcode;
	uint16_t sequence, status, assoc, offset, count;

	if (length < NTP_CTRLMSG_MINLEN)
		goto invalid;

	control = GET_U_1(cd->control);
	R = (control & 0x80) != 0;
	E = (control & 0x40) != 0;
	M = (control & 0x20) != 0;
	opcode = control & 0x1f;
	ND_PRINT(", %s, %s, %s, OpCode=%u\n",
		  R ? "Response" : "Request", E ? "Error" : "OK",
		  M ? "More" : "Last", opcode);

	sequence = GET_BE_U_2(cd->sequence);
	ND_PRINT("\tSequence=%hu", sequence);

	status = GET_BE_U_2(cd->status);
	ND_PRINT(", Status=%#hx", status);

	assoc = GET_BE_U_2(cd->assoc);
	ND_PRINT(", Assoc.=%hu", assoc);

	offset = GET_BE_U_2(cd->offset);
	ND_PRINT(", Offset=%hu", offset);

	count = GET_BE_U_2(cd->count);
	ND_PRINT(", Count=%hu", count);

	if (NTP_CTRLMSG_MINLEN + count > length)
		goto invalid;
	if (count != 0) {
		ND_TCHECK_LEN(cd->data, count);
		ND_PRINT("\n\tTO-BE-DONE: data not interpreted");
	}
	return;

invalid:
	nd_print_invalid(ndo);
	ND_TCHECK_LEN(cd, length);
	return;

trunc:
	nd_print_trunc(ndo);
}

union ntpdata {
	struct ntp_time_data	td;
	struct ntp_control_data	cd;
};

/*
 * Print NTP requests, handling the common VN, LI, and Mode
 */
void
ntp_print(netdissect_options *ndo,
	  const u_char *cp, u_int length)
{
	const union ntpdata *bp = (const union ntpdata *)cp;
	u_int mode, version, leapind;
	uint8_t status;

	ndo->ndo_protocol = "ntp";
	status = GET_U_1(bp->td.status);

	version = (status & VERSIONMASK) >> VERSIONSHIFT;
	ND_PRINT("NTPv%u", version);

	mode = (status & MODEMASK) >> MODESHIFT;
	if (!ndo->ndo_vflag) {
		ND_PRINT(", %s, length %u",
			 tok2str(ntp_mode_values, "Unknown mode", mode),
			 length);
		return;
	}

	ND_PRINT(", %s, length %u\n",
		  tok2str(ntp_mode_values, "Unknown mode", mode), length);

	/* leapind = (status & LEAPMASK) >> LEAPSHIFT; */
	leapind = (status & LEAPMASK);
	ND_PRINT("\tLeap indicator: %s (%u)",
		 tok2str(ntp_leapind_values, "Unknown", leapind),
		 leapind);

	switch (mode) {

	case MODE_UNSPEC:
	case MODE_SYM_ACT:
	case MODE_SYM_PAS:
	case MODE_CLIENT:
	case MODE_SERVER:
	case MODE_BROADCAST:
		ntp_time_print(ndo, &bp->td, length);
		break;

	case MODE_CONTROL:
		ntp_control_print(ndo, &bp->cd, length);
		break;

	default:
		break;			/* XXX: not implemented! */
	}
}

static void
p_sfix(netdissect_options *ndo,
       const struct s_fixedpt *sfp)
{
	int i;
	int f;
	double ff;

	i = GET_BE_U_2(sfp->int_part);
	f = GET_BE_U_2(sfp->fraction);
	ff = f / 65536.0;		/* shift radix point by 16 bits */
	f = (int)(ff * 1000000.0);	/* Treat fraction as parts per million */
	ND_PRINT("%d.%06d", i, f);
}

/* Prints time difference between *lfp and *olfp */
static void
p_ntp_delta(netdissect_options *ndo,
	    const struct l_fixedpt *olfp,
	    const struct l_fixedpt *lfp)
{
	uint32_t u, uf;
	uint32_t ou, ouf;
	uint32_t i;
	uint32_t f;
	double ff;
	int signbit;

	u = GET_BE_U_4(lfp->int_part);
	ou = GET_BE_U_4(olfp->int_part);
	uf = GET_BE_U_4(lfp->fraction);
	ouf = GET_BE_U_4(olfp->fraction);
	if (ou == 0 && ouf == 0) {
		p_ntp_time(ndo, lfp);
		return;
	}

	if (u > ou) {		/* new is definitely greater than old */
		signbit = 0;
		i = u - ou;
		f = uf - ouf;
		if (ouf > uf)	/* must borrow from high-order bits */
			i -= 1;
	} else if (u < ou) {	/* new is definitely less than old */
		signbit = 1;
		i = ou - u;
		f = ouf - uf;
		if (uf > ouf)	/* must borrow from the high-order bits */
			i -= 1;
	} else {		/* int_part is zero */
		i = 0;
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
	ff = ff / FMAXINT;			/* shift radix point by 32 bits */
	f = (uint32_t)(ff * 1000000000.0);	/* treat fraction as parts per billion */
	ND_PRINT("%s%u.%09u", signbit ? "-" : "+", i, f);
}

/* Prints polling interval in log2 as seconds or fraction of second */
static void
p_poll(netdissect_options *ndo,
       const int poll_interval)
{
	if (poll_interval <= -32 || poll_interval >= 32)
		return;

	if (poll_interval >= 0)
		ND_PRINT(" (%us)", 1U << poll_interval);
	else
		ND_PRINT(" (1/%us)", 1U << -poll_interval);
}

