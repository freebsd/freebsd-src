/*
 * Copyright (c) 1988, 1989, 1990, 1991, 1992, 1993, 1994
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
    "@(#) $Header: /pub/FreeBSD/FreeBSD-CVS/src/usr.sbin/tcpdump/tcpdump/print-nfs.c,v 1.3.4.1 1995/10/06 11:53:46 davidg Exp $ (LBL)";
#endif

#include <sys/param.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <net/if.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>

#ifdef SOLARIS
#include <tiuser.h>
#endif
#include <rpc/types.h>
#include <rpc/xdr.h>
#include <rpc/auth.h>
#include <rpc/auth_unix.h>
#include <rpc/svc.h>
#include <rpc/rpc_msg.h>

#include <ctype.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include "interface.h"
#include "addrtoname.h"
#include "extract.h"			/* must come after interface.h */

#include "nfsv2.h"
#include "nfsfh.h"

static void nfs_printfh(const u_int32 *);
static void xid_map_enter(const struct rpc_msg *, const struct ip *);
static int32 xid_map_find(const struct rpc_msg *, const struct ip *);
static void interp_reply(const struct rpc_msg *, u_int32, int);

void
nfsreply_print(register const u_char *bp, int length,
	       register const u_char *bp2)
{
	register const struct rpc_msg *rp;
	register const struct ip *ip;
	int32 proc;

	rp = (const struct rpc_msg *)bp;
	ip = (const struct ip *)bp2;

	if (!nflag)
		(void)printf("%s.nfs > %s.%x: reply %s %d",
			     ipaddr_string(&ip->ip_src),
			     ipaddr_string(&ip->ip_dst),
			     ntohl(rp->rm_xid),
			     ntohl(rp->rm_reply.rp_stat) == MSG_ACCEPTED?
				     "ok":"ERR",
			     length);
	else
		(void)printf("%s.%x > %s.%x: reply %s %d",
			     ipaddr_string(&ip->ip_src),
			     NFS_PORT,
			     ipaddr_string(&ip->ip_dst),
			     ntohl(rp->rm_xid),
			     ntohl(rp->rm_reply.rp_stat) == MSG_ACCEPTED?
			     	"ok":"ERR",
			     length);

	proc = xid_map_find(rp, ip);
	if (proc >= 0)
		interp_reply(rp, (u_int32)proc, length);
}

/*
 * Return a pointer to the first file handle in the packet.
 * If the packet was truncated, return 0.
 */
static const u_int32 *
parsereq(register const struct rpc_msg *rp, register int length)
{
	register const u_int32 *dp = (u_int32 *)&rp->rm_call.cb_cred;
	register const u_int32 *ep = (u_int32 *)snapend;
	register u_int len;

	if (&dp[2] >= ep)
		return (0);
	/*
	 * find the start of the req data (if we captured it)
	 */
	len =  ntohl(dp[1]);
	if (dp < ep && len < length) {
		dp += (len + (2 * sizeof(u_int32) + 3)) / sizeof(u_int32);
		len = ntohl(dp[1]);
		if ((dp < ep) && (len < length)) {
			dp += (len + (2 * sizeof(u_int32) + 3)) /
				sizeof(u_int32);
			if (dp < ep)
				return (dp);
		}
	}
	return (0);
}

/*
 * Print out an NFS file handle and return a pointer to following word.
 * If packet was truncated, return 0.
 */
static const u_int32 *
parsefh(register const u_int32 *dp)
{
	if (dp + 8 <= (u_int32 *)snapend) {
		nfs_printfh(dp);
		return (dp + 8);
	}
	return (0);
}

/*
 * Print out a file name and return pointer to 32-bit word past it.
 * If packet was truncated, return 0.
 */
static const u_int32 *
parsefn(register const u_int32 *dp)
{
	register u_int32 len;
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
	(void) fn_printn(cp, len, NULL);

	return (dp);
}

/*
 * Print out file handle and file name.
 * Return pointer to 32-bit word past file name.
 * If packet was truncated (or there was some other error), return 0.
 */
static const u_int32 *
parsefhn(register const u_int32 *dp)
{
	dp = parsefh(dp);
	if (dp == 0)
		return (0);
	putchar(' ');
	return (parsefn(dp));
}

void
nfsreq_print(register const u_char *bp, int length, register const u_char *bp2)
{
	register const struct rpc_msg *rp;
	register const struct ip *ip;
	register const u_int32 *dp;
	register const u_char *ep;

#define TCHECK(p, l) if ((u_char *)(p) > ep - l) break

	rp = (const struct rpc_msg *)bp;
	ip = (const struct ip *)bp2;
	ep = snapend;
	if (!nflag)
		(void)printf("%s.%x > %s.nfs: %d",
			     ipaddr_string(&ip->ip_src),
			     ntohl(rp->rm_xid),
			     ipaddr_string(&ip->ip_dst),
			     length);
	else
		(void)printf("%s.%x > %s.%x: %d",
			     ipaddr_string(&ip->ip_src),
			     ntohl(rp->rm_xid),
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
			TCHECK(dp, 3 * sizeof(*dp));
			printf(" %lu bytes @ %lu",
			       ntohl(dp[1]), ntohl(dp[0]));
			return;
		}
		break;

#if NFSPROC_WRITECACHE != NFSPROC_NOOP
	case NFSPROC_WRITECACHE:
		printf(" writecache");
		if ((dp = parsereq(rp, length)) != 0 &&
		    (dp = parsefh(dp)) != 0) {
			TCHECK(dp, 4 * sizeof(*dp));
			printf(" %lu (%lu) bytes @ %lu (%lu)",
			       ntohl(dp[3]), ntohl(dp[2]),
			       ntohl(dp[1]), ntohl(dp[0]));
			return;
		}
		break;
#endif
	case NFSPROC_WRITE:
		printf(" write");
		if ((dp = parsereq(rp, length)) != 0 &&
		    (dp = parsefh(dp)) != 0) {
			TCHECK(dp, 4 * sizeof(*dp));
			printf(" %lu (%lu) bytes @ %lu (%lu)",
			       ntohl(dp[3]), ntohl(dp[2]),
			       ntohl(dp[1]), ntohl(dp[0]));
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
			TCHECK(dp, 2 * sizeof(*dp));
			/*
			 * Print the offset as signed, since -1 is common,
			 * but offsets > 2^31 aren't.
			 */
			printf(" %lu bytes @ %ld", ntohl(dp[1]), ntohl(dp[0]));
			return;
		}
		break;

	case NFSPROC_STATFS:
		printf(" statfs");
		if ((dp = parsereq(rp, length)) != 0 && parsefh(dp) != 0)
			return;
		break;

	default:
		printf(" proc-%lu", ntohl(rp->rm_call.cb_proc));
		return;
	}
	fputs(" [|nfs]", stdout);
#undef TCHECK
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
nfs_printfh(register const u_int32 *dp)
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

	    (void)printf(" fh %s/%ld", temp, ino);
	}
	else {
	    (void)printf(" fh %d,%d/%ld",
			fsid.fsid_dev.Major, fsid.fsid_dev.Minor,
			ino);
	}
}

/*
 * Maintain a small cache of recent client.XID.server/proc pairs, to allow
 * us to match up replies with requests and thus to know how to parse
 * the reply.
 */

struct xid_map_entry {
	u_int32		xid;		/* transaction ID (net order) */
	struct in_addr	client;		/* client IP address (net order) */
	struct in_addr	server;		/* server IP address (net order) */
	u_int32		proc;		/* call proc number (host order) */
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
static int32
xid_map_find(const struct rpc_msg *rp, const struct ip *ip)
{
	int i;
	struct xid_map_entry *xmep;
	u_int32 xid = rp->rm_xid;
	u_int32 clip = ip->ip_dst.s_addr;
	u_int32 sip = ip->ip_src.s_addr;

	/* Start searching from where we last left off */
	i = xid_map_hint;
	do {
		xmep = &xid_map[i];
		if (xmep->xid == xid && xmep->client.s_addr == clip &&
		    xmep->server.s_addr == sip) {
			/* match */
			xid_map_hint = i;
			return ((int32)xmep->proc);
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
static const u_int32 *
parserep(register const struct rpc_msg *rp, register int length)
{
	register const u_int32 *dp;
	register const u_int32 *ep = (const u_int32 *)snapend;
	int len;
	enum accept_stat astat;

	/*
	 * Portability note:
	 * Here we find the address of the ar_verf credentials.
	 * Originally, this calculation was
	 *	dp = (u_int32 *)&rp->rm_reply.rp_acpt.ar_verf
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
	dp = ((const u_int32 *)&rp->rm_reply) + 1;
	if (&dp[1] >= ep)
		return(0);
	len = ntohl(dp[1]);
	if (len >= length)
		return(0);
	/*
	 * skip past the ar_verf credentials.
	 */
	dp += (len + (2*sizeof(u_int32) + 3)) / sizeof(u_int32);
	if (dp >= ep)
		return(0);

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
	if ((sizeof(astat) + ((char *)dp)) < (char *)ep)
		return((u_int32 *) (sizeof(astat) + ((char *)dp)));

	return (0);
}

#define T2CHECK(p, l) if ((u_char *)(p) > ((u_char *)snapend) - l) return(0)

#if defined(BSD) && (BSD >= 199103)
#define	strerr strerror
#else
/*
 * Not all systems have strerror().
 */
static char *
strerr(int errno)
{
	extern int sys_nerr;
	/* Conflicts with declaration in <stdio.h> */
	extern char *sys_errlist[];

	if (errno < sys_nerr)
		return (sys_errlist[errno]);
	return (0);
}
#endif

static const u_int32 *
parsestatus(const u_int32 *dp)
{
	int errno;
	T2CHECK(dp, 4);

	errno = ntohl(dp[0]);
	if (errno != 0) {
		char *errmsg;

		if (qflag)
			return(0);

		errmsg = strerr(errno);
		if (errmsg)
			printf(" ERROR: %s", errmsg);
		else
			printf(" ERROR: %d", errno);
		return(0);
	}
	return (dp + 1);
}

static struct token type2str[] = {
	{ NFNON,	"NON" },
	{ NFREG,	"REG" },
	{ NFDIR,	"DIR" },
	{ NFBLK,	"BLK" },
	{ NFCHR,	"CHR" },
	{ NFLNK,	"LNK" },
	{ 0,		NULL }
};

static const u_int32 *
parsefattr(const u_int32 *dp, int verbose)
{
	const struct nfsv2_fattr *fap;

	T2CHECK(dp, 4);

	fap = (const struct nfsv2_fattr *)dp;
	if (verbose)
		printf(" %s %o ids %d/%d sz %d ",
		    tok2str(type2str, "unk-ft %d ", ntohl(fap->fa_type)),
		       ntohl(fap->fa_mode), ntohl(fap->fa_uid),
		       ntohl(fap->fa_gid), ntohl(fap->fa_nfssize));
	/* print lots more stuff */
	if (verbose > 1) {
		printf("nlink %d rdev %x fsid %x nodeid %x a/m/ctime ",
		       ntohl(fap->fa_nlink), ntohl(fap->fa_nfsrdev),
		       ntohl(fap->fa_nfsfsid), ntohl(fap->fa_nfsfileid));
		printf("%d.%06d ",
		       ntohl(fap->fa_nfsatime.nfs_sec),
		       ntohl(fap->fa_nfsatime.nfs_usec));
		printf("%d.%06d ",
		       ntohl(fap->fa_nfsmtime.nfs_sec),
		       ntohl(fap->fa_nfsmtime.nfs_usec));
		printf("%d.%06d ",
		       ntohl(fap->fa_nfsctime.nfs_sec),
		       ntohl(fap->fa_nfsctime.nfs_usec));
	}
	return ((const u_int32 *)&fap[1]);
}

static int
parseattrstat(const u_int32 *dp, int verbose)
{
	dp = parsestatus(dp);
	if (dp == NULL)
		return (0);

	return ((int)parsefattr(dp, verbose));
}

static int
parsediropres(const u_int32 *dp)
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
parselinkres(const u_int32 *dp)
{
	dp = parsestatus(dp);
	if (dp == NULL)
		return(0);

	putchar(' ');
	return (parsefn(dp) != NULL);
}

static int
parsestatfs(const u_int32 *dp)
{
	const struct nfsv2_statfs *sfsp;

	dp = parsestatus(dp);
	if (dp == NULL)
		return(0);

	if (qflag)
		return(1);

	T2CHECK(dp, 20);

	sfsp = (const struct nfsv2_statfs *)dp;
	printf(" tsize %d bsize %d blocks %d bfree %d bavail %d",
	       ntohl(sfsp->sf_tsize), ntohl(sfsp->sf_bsize),
	       ntohl(sfsp->sf_blocks), ntohl(sfsp->sf_bfree),
	       ntohl(sfsp->sf_bavail));

	return (1);
}

static int
parserddires(const u_int32 *dp)
{
	dp = parsestatus(dp);
	if (dp == 0)
		return (0);
	if (qflag)
		return (1);

	T2CHECK(dp, 12);
	printf(" offset %x size %d ", ntohl(dp[0]), ntohl(dp[1]));
	if (dp[2] != 0)
		printf("eof");

	return (1);
}

static void
interp_reply(const struct rpc_msg *rp, u_int32 proc, int length)
{
	register const u_int32 *dp;

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
		printf(" proc-%lu", proc);
		return;
	}
	fputs(" [|nfs]", stdout);
}
