/*
 * Copyright (c) 1992, 1993, 1994, 1995, 1996
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

#ifndef lint
static char rcsid[] =
    "@(#) $Header: print-decnet.c,v 1.22 96/07/23 14:17:22 leres Exp $ (LBL)";
#endif

#include <sys/param.h>
#include <sys/time.h>
#include <sys/socket.h>

#if __STDC__
struct mbuf;
struct rtentry;
#endif
#include <net/if.h>

#ifdef	HAVE_LIBDNET
#include <netdnet/dnetdb.h>
#endif

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "decnet.h"
#include "extract.h"
#include "interface.h"
#include "addrtoname.h"

/* Forwards */
static void print_decnet_ctlmsg(const union routehdr *, u_int);
static void print_t_info(int);
static void print_l1_routes(const char *, u_int);
static void print_l2_routes(const char *, u_int);
static void print_i_info(int);
static void print_elist(const char *, u_int);
static void print_nsp(const u_char *, u_int);
static void print_reason(int);
#ifdef	PRINT_NSPDATA
static void pdata(u_char *, int);
#endif

#ifdef	HAVE_LIBDNET
extern char *dnet_htoa(struct dn_naddr *);
#endif

void
decnet_print(register const u_char *ap, register u_int length,
	     register u_int caplen)
{
	static union routehdr rhcopy;
	register union routehdr *rhp = &rhcopy;
	register int mflags;
	int dst, src, hops;
	u_int rhlen, nsplen, pktlen;
	const u_char *nspp;

	if (length < sizeof(struct shorthdr)) {
		(void)printf("[|decnet]");
		return;
	}

	pktlen = EXTRACT_LE_16BITS(ap);

	rhlen = min(length, caplen);
	rhlen = min(rhlen, sizeof(*rhp));
	memcpy((char *)rhp, (char *)&(ap[sizeof(short)]), rhlen);

	mflags = EXTRACT_LE_8BITS(rhp->rh_short.sh_flags);

	if (mflags & RMF_PAD) {
	    /* pad bytes of some sort in front of message */
	    u_int padlen = mflags & RMF_PADMASK;
	    if (vflag)
		(void) printf("[pad:%d] ", padlen);
	    ap += padlen;
	    length -= padlen;
	    caplen -= padlen;
	    rhlen = min(length, caplen);
	    rhlen = min(rhlen, sizeof(*rhp));
	    memcpy((char *)rhp, (char *)&(ap[sizeof(short)]), rhlen);
	    mflags = EXTRACT_LE_8BITS(rhp->rh_short.sh_flags);
	}

	if (mflags & RMF_FVER) {
		(void) printf("future-version-decnet");
		default_print(ap, length);
		return;
	}

	/* is it a control message? */
	if (mflags & RMF_CTLMSG) {
		print_decnet_ctlmsg(rhp, min(length, caplen));
		return;
	}

	switch (mflags & RMF_MASK) {
	case RMF_LONG:
	    dst =
		EXTRACT_LE_16BITS(rhp->rh_long.lg_dst.dne_remote.dne_nodeaddr);
	    src =
		EXTRACT_LE_16BITS(rhp->rh_long.lg_src.dne_remote.dne_nodeaddr);
	    hops = EXTRACT_LE_8BITS(rhp->rh_long.lg_visits);
	    nspp = &(ap[sizeof(short) + sizeof(struct longhdr)]);
	    nsplen = min((length - sizeof(struct longhdr)),
			 (caplen - sizeof(struct longhdr)));
	    break;
	case RMF_SHORT:
	    dst = EXTRACT_LE_16BITS(rhp->rh_short.sh_dst);
	    src = EXTRACT_LE_16BITS(rhp->rh_short.sh_src);
	    hops = (EXTRACT_LE_8BITS(rhp->rh_short.sh_visits) & VIS_MASK)+1;
	    nspp = &(ap[sizeof(short) + sizeof(struct shorthdr)]);
	    nsplen = min((length - sizeof(struct shorthdr)),
			 (caplen - sizeof(struct shorthdr)));
	    break;
	default:
	    (void) printf("unknown message flags under mask");
	    default_print((u_char *)ap, length);
	    return;
	}

	(void)printf("%s > %s %d ",
			dnaddr_string(src), dnaddr_string(dst), pktlen);
	if (vflag) {
	    if (mflags & RMF_RQR)
		(void)printf("RQR ");
	    if (mflags & RMF_RTS)
		(void)printf("RTS ");
	    if (mflags & RMF_IE)
		(void)printf("IE ");
	    (void)printf("%d hops ", hops);
	}

	print_nsp(nspp, nsplen);
}

static void
print_decnet_ctlmsg(register const union routehdr *rhp, u_int length)
{
	int mflags = EXTRACT_LE_8BITS(rhp->rh_short.sh_flags);
	register union controlmsg *cmp = (union controlmsg *)rhp;
	int src, dst, info, blksize, eco, ueco, hello, other, vers;
	etheraddr srcea, rtea;
	int priority;
	char *rhpx = (char *)rhp;

	switch (mflags & RMF_CTLMASK) {
	case RMF_INIT:
	    (void)printf("init ");
	    src = EXTRACT_LE_16BITS(cmp->cm_init.in_src);
	    info = EXTRACT_LE_8BITS(cmp->cm_init.in_info);
	    blksize = EXTRACT_LE_16BITS(cmp->cm_init.in_blksize);
	    vers = EXTRACT_LE_8BITS(cmp->cm_init.in_vers);
	    eco = EXTRACT_LE_8BITS(cmp->cm_init.in_eco);
	    ueco = EXTRACT_LE_8BITS(cmp->cm_init.in_ueco);
	    hello = EXTRACT_LE_16BITS(cmp->cm_init.in_hello);
	    print_t_info(info);
	    (void)printf(
		"src %sblksize %d vers %d eco %d ueco %d hello %d",
			dnaddr_string(src), blksize, vers, eco, ueco,
			hello);
	    break;
	case RMF_VER:
	    (void)printf("verification ");
	    src = EXTRACT_LE_16BITS(cmp->cm_ver.ve_src);
	    other = EXTRACT_LE_8BITS(cmp->cm_ver.ve_fcnval);
	    (void)printf("src %s fcnval %o", dnaddr_string(src), other);
	    break;
	case RMF_TEST:
	    (void)printf("test ");
	    src = EXTRACT_LE_16BITS(cmp->cm_test.te_src);
	    other = EXTRACT_LE_8BITS(cmp->cm_test.te_data);
	    (void)printf("src %s data %o", dnaddr_string(src), other);
	    break;
	case RMF_L1ROUT:
	    (void)printf("lev-1-routing ");
	    src = EXTRACT_LE_16BITS(cmp->cm_l1rou.r1_src);
	    (void)printf("src %s ", dnaddr_string(src));
	    print_l1_routes(&(rhpx[sizeof(struct l1rout)]),
				length - sizeof(struct l1rout));
	    break;
	case RMF_L2ROUT:
	    (void)printf("lev-2-routing ");
	    src = EXTRACT_LE_16BITS(cmp->cm_l2rout.r2_src);
	    (void)printf("src %s ", dnaddr_string(src));
	    print_l2_routes(&(rhpx[sizeof(struct l2rout)]),
				length - sizeof(struct l2rout));
	    break;
	case RMF_RHELLO:
	    (void)printf("router-hello ");
	    vers = EXTRACT_LE_8BITS(cmp->cm_rhello.rh_vers);
	    eco = EXTRACT_LE_8BITS(cmp->cm_rhello.rh_eco);
	    ueco = EXTRACT_LE_8BITS(cmp->cm_rhello.rh_ueco);
	    memcpy((char *)&srcea, (char *)&(cmp->cm_rhello.rh_src),
		sizeof(srcea));
	    src = EXTRACT_LE_16BITS(srcea.dne_remote.dne_nodeaddr);
	    info = EXTRACT_LE_8BITS(cmp->cm_rhello.rh_info);
	    blksize = EXTRACT_LE_16BITS(cmp->cm_rhello.rh_blksize);
	    priority = EXTRACT_LE_8BITS(cmp->cm_rhello.rh_priority);
	    hello = EXTRACT_LE_16BITS(cmp->cm_rhello.rh_hello);
	    print_i_info(info);
	    (void)printf(
	    "vers %d eco %d ueco %d src %s blksize %d pri %d hello %d",
			vers, eco, ueco, dnaddr_string(src),
			blksize, priority, hello);
	    print_elist(&(rhpx[sizeof(struct rhellomsg)]),
				length - sizeof(struct rhellomsg));
	    break;
	case RMF_EHELLO:
	    (void)printf("endnode-hello ");
	    vers = EXTRACT_LE_8BITS(cmp->cm_ehello.eh_vers);
	    eco = EXTRACT_LE_8BITS(cmp->cm_ehello.eh_eco);
	    ueco = EXTRACT_LE_8BITS(cmp->cm_ehello.eh_ueco);
	    memcpy((char *)&srcea, (char *)&(cmp->cm_ehello.eh_src),
		sizeof(srcea));
	    src = EXTRACT_LE_16BITS(srcea.dne_remote.dne_nodeaddr);
	    info = EXTRACT_LE_8BITS(cmp->cm_ehello.eh_info);
	    blksize = EXTRACT_LE_16BITS(cmp->cm_ehello.eh_blksize);
	    /*seed*/
	    memcpy((char *)&rtea, (char *)&(cmp->cm_ehello.eh_router),
		sizeof(rtea));
	    dst = EXTRACT_LE_16BITS(rtea.dne_remote.dne_nodeaddr);
	    hello = EXTRACT_LE_16BITS(cmp->cm_ehello.eh_hello);
	    other = EXTRACT_LE_8BITS(cmp->cm_ehello.eh_data);
	    print_i_info(info);
	    (void)printf(
	"vers %d eco %d ueco %d src %s blksize %d rtr %s hello %d data %o",
			vers, eco, ueco, dnaddr_string(src),
			blksize, dnaddr_string(dst), hello, other);
	    break;

	default:
	    (void)printf("unknown control message");
	    default_print((u_char *)rhp, length);
	    break;
	}
}

static void
print_t_info(int info)
{
	int ntype = info & 3;
	switch (ntype) {
	case 0: (void)printf("reserved-ntype? "); break;
	case TI_L2ROUT: (void)printf("l2rout "); break;
	case TI_L1ROUT: (void)printf("l1rout "); break;
	case TI_ENDNODE: (void)printf("endnode "); break;
	}
	if (info & TI_VERIF)
	    (void)printf("verif ");
	if (info & TI_BLOCK)
	    (void)printf("blo ");
}

static void
print_l1_routes(const char *rp, u_int len)
{
	int count;
	int id;
	int info;

	/* The last short is a checksum */
	while (len > (3 * sizeof(short))) {
	    count = EXTRACT_LE_16BITS(rp);
	    if (count > 1024)
		return;	/* seems to be bogus from here on */
	    rp += sizeof(short);
	    len -= sizeof(short);
	    id = EXTRACT_LE_16BITS(rp);
	    rp += sizeof(short);
	    len -= sizeof(short);
	    info = EXTRACT_LE_16BITS(rp);
	    rp += sizeof(short);
	    len -= sizeof(short);
	    (void)printf("{ids %d-%d cost %d hops %d} ", id, id + count,
			    RI_COST(info), RI_HOPS(info));
	}
}

static void
print_l2_routes(const char *rp, u_int len)
{
	int count;
	int area;
	int info;

	/* The last short is a checksum */
	while (len > (3 * sizeof(short))) {
	    count = EXTRACT_LE_16BITS(rp);
	    if (count > 1024)
		return;	/* seems to be bogus from here on */
	    rp += sizeof(short);
	    len -= sizeof(short);
	    area = EXTRACT_LE_16BITS(rp);
	    rp += sizeof(short);
	    len -= sizeof(short);
	    info = EXTRACT_LE_16BITS(rp);
	    rp += sizeof(short);
	    len -= sizeof(short);
	    (void)printf("{areas %d-%d cost %d hops %d} ", area, area + count,
			    RI_COST(info), RI_HOPS(info));
	}
}

static void
print_i_info(int info)
{
	int ntype = info & II_TYPEMASK;
	switch (ntype) {
	case 0: (void)printf("reserved-ntype? "); break;
	case II_L2ROUT: (void)printf("l2rout "); break;
	case II_L1ROUT: (void)printf("l1rout "); break;
	case II_ENDNODE: (void)printf("endnode "); break;
	}
	if (info & II_VERIF)
	    (void)printf("verif ");
	if (info & II_NOMCAST)
	    (void)printf("nomcast ");
	if (info & II_BLOCK)
	    (void)printf("blo ");
}

static void
print_elist(const char *elp, u_int len)
{
	/* Not enough examples available for me to debug this */
}

static void
print_nsp(const u_char *nspp, u_int nsplen)
{
	const struct nsphdr *nsphp = (struct nsphdr *)nspp;
	int dst, src, flags;

	flags = EXTRACT_LE_8BITS(nsphp->nh_flags);
	dst = EXTRACT_LE_16BITS(nsphp->nh_dst);
	src = EXTRACT_LE_16BITS(nsphp->nh_src);

	switch (flags & NSP_TYPEMASK) {
	case MFT_DATA:
	    switch (flags & NSP_SUBMASK) {
	    case MFS_BOM:
	    case MFS_MOM:
	    case MFS_EOM:
	    case MFS_BOM+MFS_EOM:
		printf("data %d>%d ", src, dst);
		{
		    struct seghdr *shp = (struct seghdr *)nspp;
		    int ack;
#ifdef	PRINT_NSPDATA
		    u_char *dp;
#endif
		    u_int data_off = sizeof(struct minseghdr);

		    ack = EXTRACT_LE_16BITS(shp->sh_seq[0]);
		    if (ack & SGQ_ACK) {	/* acknum field */
			if ((ack & SGQ_NAK) == SGQ_NAK)
			    (void)printf("nak %d ", ack & SGQ_MASK);
			else
			    (void)printf("ack %d ", ack & SGQ_MASK);
		        ack = EXTRACT_LE_16BITS(shp->sh_seq[1]);
			data_off += sizeof(short);
			if (ack & SGQ_OACK) {	/* ackoth field */
			    if ((ack & SGQ_ONAK) == SGQ_ONAK)
				(void)printf("onak %d ", ack & SGQ_MASK);
			    else
				(void)printf("oack %d ", ack & SGQ_MASK);
			    ack = EXTRACT_LE_16BITS(shp->sh_seq[2]);
			    data_off += sizeof(short);
			}
		    }
		    (void)printf("seg %d ", ack & SGQ_MASK);
#ifdef	PRINT_NSPDATA
		    dp = &(nspp[data_off]);
		    pdata(dp, 10);
#endif
		}
		break;
	    case MFS_ILS+MFS_INT:
		printf("intr ");
		{
		    struct seghdr *shp = (struct seghdr *)nspp;
		    int ack;
#ifdef	PRINT_NSPDATA
		    u_char *dp;
#endif
		    u_int data_off = sizeof(struct minseghdr);

		    ack = EXTRACT_LE_16BITS(shp->sh_seq[0]);
		    if (ack & SGQ_ACK) {	/* acknum field */
			if ((ack & SGQ_NAK) == SGQ_NAK)
			    (void)printf("nak %d ", ack & SGQ_MASK);
			else
			    (void)printf("ack %d ", ack & SGQ_MASK);
		        ack = EXTRACT_LE_16BITS(shp->sh_seq[1]);
			data_off += sizeof(short);
			if (ack & SGQ_OACK) {	/* ackdat field */
			    if ((ack & SGQ_ONAK) == SGQ_ONAK)
				(void)printf("nakdat %d ", ack & SGQ_MASK);
			    else
				(void)printf("ackdat %d ", ack & SGQ_MASK);
			    ack = EXTRACT_LE_16BITS(shp->sh_seq[2]);
			    data_off += sizeof(short);
			}
		    }
		    (void)printf("seg %d ", ack & SGQ_MASK);
#ifdef	PRINT_NSPDATA
		    dp = &(nspp[data_off]);
		    pdata(dp, 10);
#endif
		}
		break;
	    case MFS_ILS:
		(void)printf("link-service %d>%d ", src, dst);
		{
		    struct seghdr *shp = (struct seghdr *)nspp;
		    struct lsmsg *lsmp =
			(struct lsmsg *)&(nspp[sizeof(struct seghdr)]);
		    int ack;
		    int lsflags, fcval;

		    ack = EXTRACT_LE_16BITS(shp->sh_seq[0]);
		    if (ack & SGQ_ACK) {	/* acknum field */
			if ((ack & SGQ_NAK) == SGQ_NAK)
			    (void)printf("nak %d ", ack & SGQ_MASK);
			else
			    (void)printf("ack %d ", ack & SGQ_MASK);
		        ack = EXTRACT_LE_16BITS(shp->sh_seq[1]);
			if (ack & SGQ_OACK) {	/* ackdat field */
			    if ((ack & SGQ_ONAK) == SGQ_ONAK)
				(void)printf("nakdat %d ", ack & SGQ_MASK);
			    else
				(void)printf("ackdat %d ", ack & SGQ_MASK);
			    ack = EXTRACT_LE_16BITS(shp->sh_seq[2]);
			}
		    }
		    (void)printf("seg %d ", ack & SGQ_MASK);
		    lsflags = EXTRACT_LE_8BITS(lsmp->ls_lsflags);
		    fcval = EXTRACT_LE_8BITS(lsmp->ls_fcval);
		    switch (lsflags & LSI_MASK) {
		    case LSI_DATA:
			(void)printf("dat seg count %d ", fcval);
			switch (lsflags & LSM_MASK) {
			case LSM_NOCHANGE:
			    break;
			case LSM_DONOTSEND:
			    (void)printf("donotsend-data ");
			    break;
			case LSM_SEND:
			    (void)printf("send-data ");
			    break;
			default:
			    (void)printf("reserved-fcmod? %x", lsflags);
			    break;
			}
			break;
		    case LSI_INTR:
			(void)printf("intr req count %d ", fcval);
			break;
		    default:
			(void)printf("reserved-fcval-int? %x", lsflags);
			break;
		    }
		}
		break;
	    default:
		(void)printf("reserved-subtype? %x %d > %d", flags, src, dst);
		break;
	    }
	    break;
	case MFT_ACK:
	    switch (flags & NSP_SUBMASK) {
	    case MFS_DACK:
		(void)printf("data-ack %d>%d ", src, dst);
		{
		    struct ackmsg *amp = (struct ackmsg *)nspp;
		    int ack;

		    ack = EXTRACT_LE_16BITS(amp->ak_acknum[0]);
		    if (ack & SGQ_ACK) {	/* acknum field */
			if ((ack & SGQ_NAK) == SGQ_NAK)
			    (void)printf("nak %d ", ack & SGQ_MASK);
			else
			    (void)printf("ack %d ", ack & SGQ_MASK);
		        ack = EXTRACT_LE_16BITS(amp->ak_acknum[1]);
			if (ack & SGQ_OACK) {	/* ackoth field */
			    if ((ack & SGQ_ONAK) == SGQ_ONAK)
				(void)printf("onak %d ", ack & SGQ_MASK);
			    else
				(void)printf("oack %d ", ack & SGQ_MASK);
			}
		    }
		}
		break;
	    case MFS_IACK:
		(void)printf("ils-ack %d>%d ", src, dst);
		{
		    struct ackmsg *amp = (struct ackmsg *)nspp;
		    int ack;

		    ack = EXTRACT_LE_16BITS(amp->ak_acknum[0]);
		    if (ack & SGQ_ACK) {	/* acknum field */
			if ((ack & SGQ_NAK) == SGQ_NAK)
			    (void)printf("nak %d ", ack & SGQ_MASK);
			else
			    (void)printf("ack %d ", ack & SGQ_MASK);
		        ack = EXTRACT_LE_16BITS(amp->ak_acknum[1]);
			if (ack & SGQ_OACK) {	/* ackdat field */
			    if ((ack & SGQ_ONAK) == SGQ_ONAK)
				(void)printf("nakdat %d ", ack & SGQ_MASK);
			    else
				(void)printf("ackdat %d ", ack & SGQ_MASK);
			}
		    }
		}
		break;
	    case MFS_CACK:
		(void)printf("conn-ack %d", dst);
		break;
	    default:
		(void)printf("reserved-acktype? %x %d > %d", flags, src, dst);
		break;
	    }
	    break;
	case MFT_CTL:
	    switch (flags & NSP_SUBMASK) {
	    case MFS_CI:
	    case MFS_RCI:
		if ((flags & NSP_SUBMASK) == MFS_CI)
		    (void)printf("conn-initiate ");
		else
		    (void)printf("retrans-conn-initiate ");
		(void)printf("%d>%d ", src, dst);
		{
		    struct cimsg *cimp = (struct cimsg *)nspp;
		    int services, info, segsize;
#ifdef	PRINT_NSPDATA
		    u_char *dp;
#endif

		    services = EXTRACT_LE_8BITS(cimp->ci_services);
		    info = EXTRACT_LE_8BITS(cimp->ci_info);
		    segsize = EXTRACT_LE_16BITS(cimp->ci_segsize);

		    switch (services & COS_MASK) {
		    case COS_NONE:
			break;
		    case COS_SEGMENT:
			(void)printf("seg ");
			break;
		    case COS_MESSAGE:
			(void)printf("msg ");
			break;
		    case COS_CRYPTSER:
			(void)printf("crypt ");
			break;
		    }
		    switch (info & COI_MASK) {
		    case COI_32:
			(void)printf("ver 3.2 ");
			break;
		    case COI_31:
			(void)printf("ver 3.1 ");
			break;
		    case COI_40:
			(void)printf("ver 4.0 ");
			break;
		    case COI_41:
			(void)printf("ver 4.1 ");
			break;
		    }
		    (void)printf("segsize %d ", segsize);
#ifdef	PRINT_NSPDATA
		    dp = &(nspp[sizeof(struct cimsg)]);
		    pdata(dp, nsplen - sizeof(struct cimsg));
#endif
		}
		break;
	    case MFS_CC:
		(void)printf("conn-confirm %d>%d ", src, dst);
		{
		    struct ccmsg *ccmp = (struct ccmsg *)nspp;
		    int services, info;
		    u_int segsize, optlen;
#ifdef	PRINT_NSPDATA
		    u_char *dp;
#endif

		    services = EXTRACT_LE_8BITS(ccmp->cc_services);
		    info = EXTRACT_LE_8BITS(ccmp->cc_info);
		    segsize = EXTRACT_LE_16BITS(ccmp->cc_segsize);
		    optlen = EXTRACT_LE_8BITS(ccmp->cc_optlen);

		    switch (services & COS_MASK) {
		    case COS_NONE:
			break;
		    case COS_SEGMENT:
			(void)printf("seg ");
			break;
		    case COS_MESSAGE:
			(void)printf("msg ");
			break;
		    case COS_CRYPTSER:
			(void)printf("crypt ");
			break;
		    }
		    switch (info & COI_MASK) {
		    case COI_32:
			(void)printf("ver 3.2 ");
			break;
		    case COI_31:
			(void)printf("ver 3.1 ");
			break;
		    case COI_40:
			(void)printf("ver 4.0 ");
			break;
		    case COI_41:
			(void)printf("ver 4.1 ");
			break;
		    }
		    (void)printf("segsize %d ", segsize);
		    if (optlen) {
			(void)printf("optlen %d ", optlen);
#ifdef	PRINT_NSPDATA
			optlen = min(optlen, nsplen - sizeof(struct ccmsg));
			dp = &(nspp[sizeof(struct ccmsg)]);
			pdata(dp, optlen);
#endif
		    }
		}
		break;
	    case MFS_DI:
		(void)printf("disconn-initiate %d>%d ", src, dst);
		{
		    struct dimsg *dimp = (struct dimsg *)nspp;
		    int reason;
		    u_int optlen;
#ifdef	PRINT_NSPDATA
		    u_char *dp;
#endif

		    reason = EXTRACT_LE_16BITS(dimp->di_reason);
		    optlen = EXTRACT_LE_8BITS(dimp->di_optlen);

		    print_reason(reason);
		    if (optlen) {
			(void)printf("optlen %d ", optlen);
#ifdef	PRINT_NSPDATA
			optlen = min(optlen, nsplen - sizeof(struct dimsg));
			dp = &(nspp[sizeof(struct dimsg)]);
			pdata(dp, optlen);
#endif
		    }
		}
		break;
	    case MFS_DC:
		(void)printf("disconn-confirm %d>%d ", src, dst);
		{
		    struct dcmsg *dcmp = (struct dcmsg *)nspp;
		    int reason;

		    reason = EXTRACT_LE_16BITS(dcmp->dc_reason);

		    print_reason(reason);
		}
		break;
	    default:
		(void)printf("reserved-ctltype? %x %d > %d", flags, src, dst);
		break;
	    }
	    break;
	default:
	    (void)printf("reserved-type? %x %d > %d", flags, src, dst);
	    break;
	}
}

static struct tok reason2str[] = {
	{ UC_OBJREJECT,		"object rejected connect" },
	{ UC_RESOURCES,		"insufficient resources" },
	{ UC_NOSUCHNODE,	"unrecognized node name" },
	{ DI_SHUT,		"node is shutting down" },
	{ UC_NOSUCHOBJ,		"unrecognized object" },
	{ UC_INVOBJFORMAT,	"invalid object name format" },
	{ UC_OBJTOOBUSY,	"object too busy" },
	{ DI_PROTOCOL,		"protocol error discovered" },
	{ DI_TPA,		"third party abort" },
	{ UC_USERABORT,		"user abort" },
	{ UC_INVNODEFORMAT,	"invalid node name format" },
	{ UC_LOCALSHUT,		"local node shutting down" },
	{ DI_LOCALRESRC,	"insufficient local resources" },
	{ DI_REMUSERRESRC,	"insufficient remote user resources" },
	{ UC_ACCESSREJECT,	"invalid access control information" },
	{ DI_BADACCNT,		"bad ACCOUNT information" },
	{ UC_NORESPONSE,	"no response from object" },
	{ UC_UNREACHABLE,	"node unreachable" },
	{ DC_NOLINK,		"no link terminate" },
	{ DC_COMPLETE,		"disconnect complete" },
	{ DI_BADIMAGE,		"bad image data in connect" },
	{ DI_SERVMISMATCH,	"cryptographic service mismatch" },
	{ 0,			NULL }
};

static void
print_reason(register int reason)
{
	printf("%s ", tok2str(reason2str, "reason-%d", reason));
}

char *
dnnum_string(u_short dnaddr)
{
	char *str;
	int area = (dnaddr & AREAMASK) >> AREASHIFT;
	int node = dnaddr & NODEMASK;

	str = (char *)malloc(sizeof("00.0000"));
	if (str == NULL)
		error("dnnum_string: malloc");
	sprintf(str, "%d.%d", area, node);
	return(str);
}

char *
dnname_string(u_short dnaddr)
{
#ifdef	HAVE_LIBDNET
	struct dn_naddr dna;

	dna.a_len = sizeof(short);
	memcpy((char *)dna.a_addr, (char *)&dnaddr, sizeof(short));
	return (savestr(dnet_htoa(&dna)));
#else
	return(dnnum_string(dnaddr));	/* punt */
#endif
}

#ifdef	PRINT_NSPDATA
static void
pdata(u_char *dp, u_int maxlen)
{
	char c;
	u_int x = maxlen;

	while (x-- > 0) {
	    c = *dp++;
	    if (isprint(c))
		putchar(c);
	    else
		printf("\\%o", c & 0xFF);
	}
}
#endif
