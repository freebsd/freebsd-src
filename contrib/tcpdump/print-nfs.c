/*
 * Copyright (c) 1988, 1989, 1990, 1991, 1992, 1993, 1994, 1995, 1996
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
    "@(#) $Header: print-nfs.c,v 1.56 96/07/23 14:17:25 leres Exp $ (LBL)";
#endif

#include <sys/param.h>
#include <sys/time.h>
#include <sys/socket.h>

#if __STDC__
struct mbuf;
struct rtentry;
#endif
#include <net/if.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>

#include <rpc/rpc.h>

#include <ctype.h>
#include <pcap.h>
#include <stdio.h>
#include <string.h>

#include "interface.h"
#include "addrtoname.h"

#include "nfsv2.h"
#include "nfsfh.h"

static void nfs_printfh(const u_int32_t *);
static void xid_map_enter(const struct rpc_msg *, const struct ip *);
static int32_t xid_map_find(const struct rpc_msg *, const struct ip *);
static void interp_reply(const struct rpc_msg *, u_int32_t, u_int);

void
nfsreply_print(register const u_char *bp, u_int length,
	       register const u_char *bp2)
{
	register const struct rpc_msg *rp;
	register const struct ip *ip;
	int32_t proc;

	rp = (const struct rpc_msg *)bp;
	ip = (const struct ip *)bp2;

	if (!nflag)
		(void)printf("%s.nfs > %s.%x: reply %s %d",
			     ipaddr_string(&ip->ip_src),
			     ipaddr_string(&ip->ip_dst),
			     (u_int32_t)ntohl(rp->rm_xid),
			     ntohl(rp->rm_reply.rp_stat) == MSG_ACCEPTED?
				     "ok":"ERR",
			     length);
	else
		(void)printf("%s.%x > %s.%x: reply %s %d",
			     ipaddr_string(&ip->ip_src),
			     NFS_PORT,
			     ipaddr_string(&ip->ip_dst),
			     (u_int32_t)ntohl(rp->rm_xid),
			     ntohl(rp->rm_reply.rp_stat) == MSG_ACCEPTED?
			     	"ok":"ERR",
			     length);

	proc = xid_map_find(rp, ip);
	if (proc >= 0)
		interp_reply(rp, (u_int32_t)proc, length);
}

/*
 * Return a pointer to the first file handle in the packet.
 * If the packet was truncated, return 0.
 */
static const u_int32_t *
parsereq(register const struct rpc_msg *rp, register u_int length)
{
	register const u_int32_t *dp;
	register u_int len;

	/*
	 * find the start of the req data (if we captured it)
	 */
	dp = (u_int32_t *)&rp->rm_call.cb_cred;
	TCHECK(dp[1]);
	len = ntohl(dp[1]);
	if (len < length) {
		dp += (len + (2 * sizeof(*dp) + 3)) / sizeof(*dp);
		TCHECK(dp[1]);
		len = ntohl(dp[1]);
		if (len < length) {
			dp += (len + (2 * sizeof(*dp) + 3)) / sizeof(*dp);
			TCHECK2(dp[0], 0);
			return (dp);
		}
	}
trunc:
	return (0);
}

/*
 * Print out an NFS file handle and return a pointer to following word.
 * If packet was truncated, return 0.
 */
static const u_int32_t *
parsefh(register const u_int32_t *dp)
{
	if (dp + 8 <= (u_int32_t *)snapend) {
		nfs_printfh(dp);
		return (dp + 8);
	}
	return (0);
}

/*
 * Print out a file name and return pointer to 32-bit word past it.
 * If packet was truncated, return 0.
 */
static const u_int32_t *
parsefn(register const u_int32_t *dp)
{
	register u_int32_t len;
	register const u_char *cp;

	/* Bail if we don't have the string length */
	if ((u_char *)dp > snapend - sizeof(*dp))
		return(0);

	/* Fetch string length; convert to host order */
	len = *dp++;
	NTOHL(len);

	cp = (u_char *)dp;
	/* Update 32-bit pointer (NFS filenames padded to 32-bit boundaries) */
	dp += ((len + 3) & ~3) / sizeof(*dp);
	if ((u_char *)dp > snapend)
		return (0);
	/* XXX seems like we should be checking the length */
	putchar('"');
	(void) fn_printn(cp, len, NULL);
	putchar('"');

	return (dp);
}

/*
 * Print out file handle and file name.
 * Return pointer to 32-bit word past file name.
 * If packet was truncated (or there was some other error), return 0.
 */
static const u_int32_t *
parsefhn(register const u_int32_t *dp)
{
	dp = parsefh(dp);
	if (dp == 0)
		return (0);
	putchar(' ');
	return (parsefn(dp));
}

void
nfsreq_print(register const u_char *bp, u_int length,
    register const u_char *bp2)
{
	register const struct rpc_msg *rp;
	register const struct ip *ip;
	register const u_int32_t *dp;

	rp = (const struct rpc_msg *)bp;
	ip = (const struct ip *)bp2;
	if (!nflag)
		(void)printf("%s.%x > %s.nfs: %d",
			     ipaddr_string(&ip->ip_src),
			     (u_int32_t)ntohl(rp->rm_xid),
			     ipaddr_string(&ip->ip_dst),
			     length);
	else
		(void)printf("%s.%x > %s.%x: %d",
			     ipaddr_string(&ip->ip_src),
			     (u_int32_t)ntohl(rp->rm_xid),
			     ipaddr_string(&ip->ip_dst),
			     NFS_PORT,
			     length);

	xid_map_enter(rp, ip);	/* record proc number for later on */

	switch (ntohl(rp->rm_call.cb_proc)) {
#ifdef NFSPROC_NOOP
	case NFSPROC_NOOP:
		printf(" nop");
		return;
#else
#define NFSPROC_NOOP -1
#endif
	case NFSPROC_NULL:
		printf(" null");
		return;

	case NFSPROC_GETATTR:
		printf(" getattr");
		if ((dp = parsereq(rp, length)) != 0 && parsefh(dp) != 0)
			return;
		break;

	case NFSPROC_SETATTR:
		printf(" setattr");
		if ((dp = parsereq(rp, length)) != 0 && parsefh(dp) != 0)
			return;
		break;

#if NFSPROC_ROOT != NFSPROC_NOOP
	case NFSPROC_ROOT:
		printf(" root");
		break;
#endif
	case NFSPROC_LOOKUP:
		printf(" lookup");
		if ((dp = parsereq(rp, length)) != 0 && parsefhn(dp) != 0)
			return;
		break;

	case NFSPROC_READLINK:
		printf(" readlink");
		if ((dp = parsereq(rp, length)) != 0 && parsefh(dp) != 0)
			return;
		break;

	case NFSPROC_READ:
		printf(" read");
		if ((dp = parsereq(rp, length)) != 0 &&
		    (dp = parsefh(dp)) != 0) {
			TCHECK2(dp[0], 3 * sizeof(*dp));
			printf(" %u bytes @ %u",
			    (u_int32_t)ntohl(dp[1]),
			    (u_int32_t)ntohl(dp[0]));
			return;
		}
		break;

#if NFSPROC_WRITECACHE != NFSPROC_NOOP
	case NFSPROC_WRITECACHE:
		printf(" writecache");
		if ((dp = parsereq(rp, length)) != 0 &&
		    (dp = parsefh(dp)) != 0) {
			TCHECK2(dp[0], 4 * sizeof(*dp));
			printf(" %u (%u) bytes @ %u (%u)",
			    (u_int32_t)ntohl(dp[3]),
			    (u_int32_t)ntohl(dp[2]),
			    (u_int32_t)ntohl(dp[1]),
			    (u_int32_t)ntohl(dp[0]));
			return;
		}
		break;
#endif
	case NFSPROC_WRITE:
		printf(" write");
		if ((dp = parsereq(rp, length)) != 0 &&
		    (dp = parsefh(dp)) != 0) {
			TCHECK2(dp[0], 4 * sizeof(*dp));
			printf(" %u (%u) bytes @ %u (%u)",
			    (u_int32_t)ntohl(dp[3]),
			    (u_int32_t)ntohl(dp[2]),
			    (u_int32_t)ntohl(dp[1]),
			    (u_int32_t)ntohl(dp[0]));
			return;
		}
		break;

	case NFSPROC_CREATE:
		printf(" create");
		if ((dp = parsereq(rp, length)) != 0 && parsefhn(dp) != 0)
			return;
		break;

	case NFSPROC_REMOVE:
		printf(" remove");
		if ((dp = parsereq(rp, length)) != 0 && parsefhn(dp) != 0)
			return;
		break;

	case NFSPROC_RENAME:
		printf(" rename");
		if ((dp = parsereq(rp, length)) != 0 &&
		    (dp = parsefhn(dp)) != 0) {
			fputs(" ->", stdout);
			if (parsefhn(dp) != 0)
				return;
		}
		break;

	case NFSPROC_LINK:
		printf(" link");
		if ((dp = parsereq(rp, length)) != 0 &&
		    (dp = parsefh(dp)) != 0) {
			fputs(" ->", stdout);
			if (parsefhn(dp) != 0)
				return;
		}
		break;

	case NFSPROC_SYMLINK:
		printf(" symlink");
		if ((dp = parsereq(rp, length)) != 0 &&
		    (dp = parsefhn(dp)) != 0) {
			fputs(" -> ", stdout);
			if (parsefn(dp) != 0)
				return;
		}
		break;

	case NFSPROC_MKDIR:
		printf(" mkdir");
		if ((dp = parsereq(rp, length)) != 0 && parsefhn(dp) != 0)
			return;
		break;

	case NFSPROC_RMDIR:
		printf(" rmdir");
		if ((dp = parsereq(rp, length)) != 0 && parsefhn(dp) != 0)
			return;
		break;

	case NFSPROC_READDIR:
		printf(" readdir");
		if ((dp = parsereq(rp, length)) != 0 &&
		    (dp = parsefh(dp)) != 0) {
			TCHECK2(dp[0], 2 * sizeof(*dp));
			/*
			 * Print the offset as signed, since -1 is common,
			 * but offsets > 2^31 aren't.
			 */
			printf(" %u bytes @ %d",
			    (u_int32_t)ntohl(dp[1]),
			    (u_int32_t)ntohl(dp[0]));
			return;
		}
		break;

	case NFSPROC_STATFS:
		printf(" statfs");
		if ((dp = parsereq(rp, length)) != 0 && parsefh(dp) != 0)
			return;
		break;

	default:
		printf(" proc-%u", (u_int32_t)ntohl(rp->rm_call.cb_proc));
		return;
	}
trunc:
	fputs(" [|nfs]", stdout);
}

/*
 * Print out an NFS file handle.
 * We assume packet was not truncated before the end of the
 * file handle pointed to by dp.
 *
 * Note: new version (using portable file-handle parser) doesn't produce
 * generation number.  It probably could be made to do that, with some
 * additional hacking on the parser code.
 */
static void
nfs_printfh(register const u_int32_t *dp)
{
	my_fsid fsid;
	ino_t ino;
	char *sfsname = NULL;

	Parse_fh((caddr_t*)dp, &fsid, &ino, NULL, &sfsname, 0);

	if (sfsname) {
	    /* file system ID is ASCII, not numeric, for this server OS */
	    static char temp[NFS_FHSIZE+1];

	    /* Make sure string is null-terminated */
	    strncpy(temp, sfsname, NFS_FHSIZE);
	    /* Remove trailing spaces */
	    sfsname = strchr(temp, ' ');
	    if (sfsname)
		*sfsname = 0;

	    (void)printf(" fh %s/%u", temp, (u_int32_t)ino);
	}
	else {
	    (void)printf(" fh %u,%u/%u",
		fsid.fsid_dev.Major,
		fsid.fsid_dev.Minor,
		(u_int32_t)ino);
	}
}

/*
 * Maintain a small cache of recent client.XID.server/proc pairs, to allow
 * us to match up replies with requests and thus to know how to parse
 * the reply.
 */

struct xid_map_entry {
	u_int32_t		xid;		/* transaction ID (net order) */
	struct in_addr	client;		/* client IP address (net order) */
	struct in_addr	server;		/* server IP address (net order) */
	u_int32_t		proc;		/* call proc number (host order) */
};

/*
 * Map entries are kept in an array that we manage as a ring;
 * new entries are always added at the tail of the ring.  Initially,
 * all the entries are zero and hence don't match anything.
 */

#define	XIDMAPSIZE	64

struct xid_map_entry xid_map[XIDMAPSIZE];

int	xid_map_next = 0;
int	xid_map_hint = 0;

static void
xid_map_enter(const struct rpc_msg *rp, const struct ip *ip)
{
	struct xid_map_entry *xmep;

	xmep = &xid_map[xid_map_next];

	if (++xid_map_next >= XIDMAPSIZE)
		xid_map_next = 0;

	xmep->xid = rp->rm_xid;
	xmep->client = ip->ip_src;
	xmep->server = ip->ip_dst;
	xmep->proc = ntohl(rp->rm_call.cb_proc);
}

/* Returns NFSPROC_xxx or -1 on failure */
static int32_t
xid_map_find(const struct rpc_msg *rp, const struct ip *ip)
{
	int i;
	struct xid_map_entry *xmep;
	u_int32_t xid = rp->rm_xid;
	u_int32_t clip = ip->ip_dst.s_addr;
	u_int32_t sip = ip->ip_src.s_addr;

	/* Start searching from where we last left off */
	i = xid_map_hint;
	do {
		xmep = &xid_map[i];
		if (xmep->xid == xid && xmep->client.s_addr == clip &&
		    xmep->server.s_addr == sip) {
			/* match */
			xid_map_hint = i;
			return ((int32_t)xmep->proc);
		}
		if (++i >= XIDMAPSIZE)
			i = 0;
	} while (i != xid_map_hint);

	/* search failed */
	return(-1);
}

/*
 * Routines for parsing reply packets
 */

/*
 * Return a pointer to the beginning of the actual results.
 * If the packet was truncated, return 0.
 */
static const u_int32_t *
parserep(register const struct rpc_msg *rp, register u_int length)
{
	register const u_int32_t *dp;
	u_int len;
	enum accept_stat astat;

	/*
	 * Portability note:
	 * Here we find the address of the ar_verf credentials.
	 * Originally, this calculation was
	 *	dp = (u_int32_t *)&rp->rm_reply.rp_acpt.ar_verf
	 * On the wire, the rp_acpt field starts immediately after
	 * the (32 bit) rp_stat field.  However, rp_acpt (which is a
	 * "struct accepted_reply") contains a "struct opaque_auth",
	 * whose internal representation contains a pointer, so on a
	 * 64-bit machine the compiler inserts 32 bits of padding
	 * before rp->rm_reply.rp_acpt.ar_verf.  So, we cannot use
	 * the internal representation to parse the on-the-wire
	 * representation.  Instead, we skip past the rp_stat field,
	 * which is an "enum" and so occupies one 32-bit word.
	 */
	dp = ((const u_int32_t *)&rp->rm_reply) + 1;
	TCHECK2(dp[0], 1);
		return(0);
	len = ntohl(dp[1]);
	if (len >= length)
		return(0);
	/*
	 * skip past the ar_verf credentials.
	 */
	dp += (len + (2*sizeof(u_int32_t) + 3)) / sizeof(u_int32_t);
	TCHECK2(dp[0], 0);

	/*
	 * now we can check the ar_stat field
	 */
	astat = ntohl(*(enum accept_stat *)dp);
	switch (astat) {

	case SUCCESS:
		break;

	case PROG_UNAVAIL:
		printf(" PROG_UNAVAIL");
		return(0);

	case PROG_MISMATCH:
		printf(" PROG_MISMATCH");
		return(0);

	case PROC_UNAVAIL:
		printf(" PROC_UNAVAIL");
		return(0);

	case GARBAGE_ARGS:
		printf(" GARBAGE_ARGS");
		return(0);

	case SYSTEM_ERR:
		printf(" SYSTEM_ERR");
		return(0);

	default:
		printf(" ar_stat %d", astat);
		return(0);
	}
	/* successful return */
	if ((sizeof(astat) + ((u_char *)dp)) < snapend)
		return((u_int32_t *) (sizeof(astat) + ((char *)dp)));

trunc:
	return (0);
}

static const u_int32_t *
parsestatus(const u_int32_t *dp)
{
	int errnum;

	TCHECK(dp[0]);
	errnum = ntohl(dp[0]);
	if (errnum != 0) {
		char *errmsg;

		if (qflag)
			return(0);

		errmsg = pcap_strerror(errnum);
		printf(" ERROR: %s", errmsg);
		return(0);
	}
	return (dp + 1);
trunc:
	return (0);
}

static struct tok type2str[] = {
	{ NFNON,	"NON" },
	{ NFREG,	"REG" },
	{ NFDIR,	"DIR" },
	{ NFBLK,	"BLK" },
	{ NFCHR,	"CHR" },
	{ NFLNK,	"LNK" },
	{ 0,		NULL }
};

static const u_int32_t *
parsefattr(const u_int32_t *dp, int verbose)
{
	const struct nfsv2_fattr *fap;

	fap = (const struct nfsv2_fattr *)dp;
	if (verbose) {
		TCHECK(fap->fa_nfssize);
		printf(" %s %o ids %u/%u sz %u ",
		    tok2str(type2str, "unk-ft %d ",
		    (u_int32_t)ntohl(fap->fa_type)),
		    (u_int32_t)ntohl(fap->fa_mode),
		    (u_int32_t)ntohl(fap->fa_uid),
		    (u_int32_t)ntohl(fap->fa_gid),
		    (u_int32_t)ntohl(fap->fa_nfssize));
	}
	/* print lots more stuff */
	if (verbose > 1) {
		TCHECK(fap->fa_nfsfileid);
		printf("nlink %u rdev %x fsid %x nodeid %x a/m/ctime ",
		    (u_int32_t)ntohl(fap->fa_nlink),
		    (u_int32_t)ntohl(fap->fa_nfsrdev),
		    (u_int32_t)ntohl(fap->fa_nfsfsid),
		    (u_int32_t)ntohl(fap->fa_nfsfileid));
		TCHECK(fap->fa_nfsatime);
		printf("%u.%06u ",
		    (u_int32_t)ntohl(fap->fa_nfsatime.nfs_sec),
		    (u_int32_t)ntohl(fap->fa_nfsatime.nfs_usec));
		TCHECK(fap->fa_nfsmtime);
		printf("%u.%06u ",
		    (u_int32_t)ntohl(fap->fa_nfsmtime.nfs_sec),
		    (u_int32_t)ntohl(fap->fa_nfsmtime.nfs_usec));
		TCHECK(fap->fa_nfsctime);
		printf("%u.%06u ",
		    (u_int32_t)ntohl(fap->fa_nfsctime.nfs_sec),
		    (u_int32_t)ntohl(fap->fa_nfsctime.nfs_usec));
	}
	return ((const u_int32_t *)&fap[1]);
trunc:
	return (NULL);
}

static int
parseattrstat(const u_int32_t *dp, int verbose)
{
	dp = parsestatus(dp);
	if (dp == NULL)
		return (0);

	return (parsefattr(dp, verbose) != NULL);
}

static int
parsediropres(const u_int32_t *dp)
{
	dp = parsestatus(dp);
	if (dp == NULL)
		return (0);

	dp = parsefh(dp);
	if (dp == NULL)
		return (0);

	return (parsefattr(dp, vflag) != NULL);
}

static int
parselinkres(const u_int32_t *dp)
{
	dp = parsestatus(dp);
	if (dp == NULL)
		return(0);

	putchar(' ');
	return (parsefn(dp) != NULL);
}

static int
parsestatfs(const u_int32_t *dp)
{
	const struct nfsv2_statfs *sfsp;

	dp = parsestatus(dp);
	if (dp == NULL)
		return(0);

	if (!qflag) {
		sfsp = (const struct nfsv2_statfs *)dp;
		TCHECK(sfsp->sf_bavail);
		printf(" tsize %u bsize %u blocks %u bfree %u bavail %u",
		    (u_int32_t)ntohl(sfsp->sf_tsize),
		    (u_int32_t)ntohl(sfsp->sf_bsize),
		    (u_int32_t)ntohl(sfsp->sf_blocks),
		    (u_int32_t)ntohl(sfsp->sf_bfree),
		    (u_int32_t)ntohl(sfsp->sf_bavail));
	}

	return (1);
trunc:
	return (0);
}

static int
parserddires(const u_int32_t *dp)
{
	dp = parsestatus(dp);
	if (dp == 0)
		return (0);
	if (!qflag) {
		TCHECK(dp[0]);
		printf(" offset %x", (u_int32_t)ntohl(dp[0]));
		TCHECK(dp[1]);
		printf(" size %u", (u_int32_t)ntohl(dp[1]));
		TCHECK(dp[2]);
		if (dp[2] != 0)
			printf(" eof");
	}

	return (1);
trunc:
	return (0);
}

static void
interp_reply(const struct rpc_msg *rp, u_int32_t proc, u_int length)
{
	register const u_int32_t *dp;

	switch (proc) {

#ifdef NFSPROC_NOOP
	case NFSPROC_NOOP:
		printf(" nop");
		return;
#else
#define NFSPROC_NOOP -1
#endif
	case NFSPROC_NULL:
		printf(" null");
		return;

	case NFSPROC_GETATTR:
		printf(" getattr");
		dp = parserep(rp, length);
		if (dp != 0 && parseattrstat(dp, !qflag) != 0)
			return;
		break;

	case NFSPROC_SETATTR:
		printf(" setattr");
		dp = parserep(rp, length);
		if (dp != 0 && parseattrstat(dp, !qflag) != 0)
			return;
		break;

#if NFSPROC_ROOT != NFSPROC_NOOP
	case NFSPROC_ROOT:
		printf(" root");
		break;
#endif
	case NFSPROC_LOOKUP:
		printf(" lookup");
		dp = parserep(rp, length);
		if (dp != 0 && parsediropres(dp) != 0)
			return;
		break;

	case NFSPROC_READLINK:
		printf(" readlink");
		dp = parserep(rp, length);
		if (dp != 0 && parselinkres(dp) != 0)
			return;
		break;

	case NFSPROC_READ:
		printf(" read");
		dp = parserep(rp, length);
		if (dp != 0 && parseattrstat(dp, vflag) != 0)
			return;
		break;

#if NFSPROC_WRITECACHE != NFSPROC_NOOP
	case NFSPROC_WRITECACHE:
		printf(" writecache");
		break;
#endif
	case NFSPROC_WRITE:
		printf(" write");
		dp = parserep(rp, length);
		if (dp != 0 && parseattrstat(dp, vflag) != 0)
			return;
		break;

	case NFSPROC_CREATE:
		printf(" create");
		dp = parserep(rp, length);
		if (dp != 0 && parsediropres(dp) != 0)
			return;
		break;

	case NFSPROC_REMOVE:
		printf(" remove");
		dp = parserep(rp, length);
		if (dp != 0 && parsestatus(dp) != 0)
			return;
		break;

	case NFSPROC_RENAME:
		printf(" rename");
		dp = parserep(rp, length);
		if (dp != 0 && parsestatus(dp) != 0)
			return;
		break;

	case NFSPROC_LINK:
		printf(" link");
		dp = parserep(rp, length);
		if (dp != 0 && parsestatus(dp) != 0)
			return;
		break;

	case NFSPROC_SYMLINK:
		printf(" symlink");
		dp = parserep(rp, length);
		if (dp != 0 && parsestatus(dp) != 0)
			return;
		break;

	case NFSPROC_MKDIR:
		printf(" mkdir");
		dp = parserep(rp, length);
		if (dp != 0 && parsediropres(dp) != 0)
			return;
		break;

	case NFSPROC_RMDIR:
		printf(" rmdir");
		dp = parserep(rp, length);
		if (dp != 0 && parsestatus(dp) != 0)
			return;
		break;

	case NFSPROC_READDIR:
		printf(" readdir");
		dp = parserep(rp, length);
		if (dp != 0 && parserddires(dp) != 0)
			return;
		break;

	case NFSPROC_STATFS:
		printf(" statfs");
		dp = parserep(rp, length);
		if (dp != 0 && parsestatfs(dp) != 0)
			return;
		break;

	default:
		printf(" proc-%u", proc);
		return;
	}
	fputs(" [|nfs]", stdout);
}
