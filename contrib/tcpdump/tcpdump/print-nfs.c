/*
 * Copyright (c) 1990, 1991, 1992 The Regents of the University of California.
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
 */

#ifndef lint
static char rcsid[] =
    "@(#) $Header: /a/cvs/386BSD/src/contrib/tcpdump/tcpdump/print-nfs.c,v 1.2 1993/09/15 20:27:23 jtc Exp $ (LBL)";
#endif

#include <stdio.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>

#include <sys/time.h>
#include <errno.h>
#include <rpc/rpc.h>

#include <ctype.h>

#include "interface.h"
/* These must come after interface.h for BSD. */
#if BSD >= 199006
#include <sys/ucred.h>
#include <nfs/nfsv2.h>
#endif
#include <nfs/nfs.h>

#include "addrtoname.h"
#include "extract.h"

static void nfs_printfh();
static void nfs_printfn();

#if BYTE_ORDER == LITTLE_ENDIAN
/*
 * Byte swap an array of n words.
 * Assume input is word-aligned.
 * Check that buffer is bounded by "snapend".
 */
static void
bswap(bp, n)
	register u_long *bp;
	register u_int n;
{
	register int nwords = ((char *)snapend - (char *)bp) / sizeof(*bp);

	if (nwords > n)
		nwords = n;
	for (; --nwords >= 0; ++bp)
		*bp = ntohl(*bp);
}
#endif

void
nfsreply_print(rp, length, ip)
	register struct rpc_msg *rp;
	int length;
	register struct ip *ip;
{
#if BYTE_ORDER == LITTLE_ENDIAN
	bswap((u_long *)rp, sizeof(*rp) / sizeof(u_long));
#endif
	if (!nflag)
		(void)printf("%s.nfs > %s.%x: reply %s %d",
			     ipaddr_string(&ip->ip_src),
			     ipaddr_string(&ip->ip_dst),
			     rp->rm_xid,
			     rp->rm_reply.rp_stat == MSG_ACCEPTED? "ok":"ERR",
			     length);
	else
		(void)printf("%s.%x > %s.%x: reply %s %d",
			     ipaddr_string(&ip->ip_src),
			     NFS_PORT,
			     ipaddr_string(&ip->ip_dst),
			     rp->rm_xid,
			     rp->rm_reply.rp_stat == MSG_ACCEPTED? "ok":"ERR",
			     length);
}

/*
 * Return a pointer to the first file handle in the packet.
 * If the packet was truncated, return 0.
 */
static u_long *
parsereq(rp, length)
	register struct rpc_msg *rp;
	register int length;
{
	register u_long *dp = (u_long *)&rp->rm_call.cb_cred;
	register u_long *ep = (u_long *)snapend;

	/* 
	 * find the start of the req data (if we captured it) 
	 * note that dp[1] was already byte swapped by bswap()
	 */
	if (dp < ep && dp[1] < length) {
		dp += (dp[1] + (2*sizeof(u_long) + 3)) / sizeof(u_long);
		if ((dp < ep) && (dp[1] < length)) {
			dp += (dp[1] + (2*sizeof(u_long) + 3)) /
				sizeof(u_long);
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
static u_long *
parsefh(dp)
	register u_long *dp;
{
	if (dp + 8 <= (u_long *)snapend) {
		nfs_printfh(dp);
		return (dp + 8);
	}
	return (0);
}

/*
 * Print out a file name and return pointer to longword past it.
 * If packet was truncated, return 0.
 */
static u_long *
parsefn(dp)
	register u_long *dp;
{
	register int len;
	register u_char *cp;

	/* Bail if we don't have the string length */
	if ((u_char *)dp > snapend - sizeof(*dp))
		return(0);

	/* Fetch string length; convert to host order */
	len = *dp++;
	NTOHL(len);

	cp = (u_char *)dp;
	/* Update long pointer (NFS filenames are padded to long) */
	dp += ((len + 3) & ~3) / sizeof(*dp);
	if ((u_char *)dp > snapend)
		return (0);
	nfs_printfn(cp, len);

	return (dp);
}

/*
 * Print out file handle and file name.
 * Return pointer to longword past file name.
 * If packet was truncated (or there was some other error), return 0.
 */
static u_long *
parsefhn(dp)
	register u_long *dp;
{
	dp = parsefh(dp);
	if (dp == 0)
		return (0);
	putchar(' ');
	return (parsefn(dp));
}

void
nfsreq_print(rp, length, ip)
	register struct rpc_msg *rp;
	int length;
	register struct ip *ip;
{
	register u_long *dp;
	register u_char *ep = snapend;
#define TCHECK(p, l) if ((u_char *)(p) > ep - l) break

#if BYTE_ORDER == LITTLE_ENDIAN
	bswap((u_long *)rp, sizeof(*rp) / sizeof(u_long));
#endif

	if (!nflag)
		(void)printf("%s.%x > %s.nfs: %d",
			     ipaddr_string(&ip->ip_src),
			     rp->rm_xid,
			     ipaddr_string(&ip->ip_dst),
			     length);
	else
		(void)printf("%s.%x > %s.%x: %d",
			     ipaddr_string(&ip->ip_src),
			     rp->rm_xid,
			     ipaddr_string(&ip->ip_dst),
			     NFS_PORT,
			     length);

	switch (rp->rm_call.cb_proc) {
#ifdef NFSPROC_NOOP
	case NFSPROC_NOOP:
		printf(" nop");
		return;
#else
#define NFSPROC_NOOP -1
#endif
	case RFS_NULL:
		printf(" null");
		return;

	case RFS_GETATTR:
		printf(" getattr");
		if ((dp = parsereq(rp, length)) != 0 && parsefh(dp) != 0)
			return;
		break;

	case RFS_SETATTR:
		printf(" setattr");
		if ((dp = parsereq(rp, length)) != 0 && parsefh(dp) != 0)
			return;
		break;

#if RFS_ROOT != NFSPROC_NOOP
	case RFS_ROOT:
		printf(" root");
		break;
#endif
	case RFS_LOOKUP:
		printf(" lookup");
		if ((dp = parsereq(rp, length)) != 0 && parsefhn(dp) != 0)
			return;
		break;

	case RFS_READLINK:
		printf(" readlink");
		if ((dp = parsereq(rp, length)) != 0 && parsefh(dp) != 0)
			return;
		break;

	case RFS_READ:
		printf(" read");
		if ((dp = parsereq(rp, length)) != 0 &&
		    (dp = parsefh(dp)) != 0) {
			TCHECK(dp, 3 * sizeof(*dp));
			printf(" %lu (%lu) bytes @ %lu",
			       ntohl(dp[1]), ntohl(dp[2]), ntohl(dp[0]));
			return;
		}
		break;

#if RFS_WRITECACHE != NFSPROC_NOOP
	case RFS_WRITECACHE:
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
	case RFS_WRITE:
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

	case RFS_CREATE:
		printf(" create");
		if ((dp = parsereq(rp, length)) != 0 && parsefhn(dp) != 0)
			return;
		break;

	case RFS_REMOVE:
		printf(" remove");
		if ((dp = parsereq(rp, length)) != 0 && parsefhn(dp) != 0)
			return;
		break;

	case RFS_RENAME:
		printf(" rename");
		if ((dp = parsereq(rp, length)) != 0 && 
		    (dp = parsefhn(dp)) != 0) {
			fputs(" ->", stdout);
			if (parsefhn(dp) != 0)
				return;
		}
		break;

	case RFS_LINK:
		printf(" link");
		if ((dp = parsereq(rp, length)) != 0 &&
		    (dp = parsefh(dp)) != 0) {
			fputs(" ->", stdout);
			if (parsefhn(dp) != 0)
				return;
		}
		break;

	case RFS_SYMLINK:
		printf(" symlink");
		if ((dp = parsereq(rp, length)) != 0 &&
		    (dp = parsefhn(dp)) != 0) {
			fputs(" -> ", stdout);
			if (parsefn(dp) != 0)
				return;
		}
		break;

	case RFS_MKDIR:
		printf(" mkdir");
		if ((dp = parsereq(rp, length)) != 0 && parsefhn(dp) != 0)
			return;
		break;

	case RFS_RMDIR:
		printf(" rmdir");
		if ((dp = parsereq(rp, length)) != 0 && parsefhn(dp) != 0)
			return;
		break;

	case RFS_READDIR:
		printf(" readdir");
		if ((dp = parsereq(rp, length)) != 0 &&
		    (dp = parsefh(dp)) != 0) {
			TCHECK(dp, 2 * sizeof(*dp));
			printf(" %lu bytes @ %lu", ntohl(dp[1]), ntohl(dp[0]));
			return;
		}
		break;

	case RFS_STATFS:
		printf(" statfs");
		if ((dp = parsereq(rp, length)) != 0 && parsefh(dp) != 0)
			return;
		break;

	default:
		printf(" proc-%lu", rp->rm_call.cb_proc);
		return;
	}
	fputs(" [|nfs]", stdout);
#undef TCHECK
}

/*
 * Print out an NFS file handle.
 * We assume packet was not truncated before the end of the
 * file handle pointed to by dp.
 */
static void
nfs_printfh(dp)
	register u_long *dp;
{
	/*
	 * take a wild guess at the structure of file handles.
	 * On sun 3s, there are 2 longs of fsid, a short
	 * len == 8, a long of inode & a long of generation number.
	 * On sun 4s, the len == 10 & there are 2 bytes of
	 * padding immediately following it.
	 */
	if (dp[2] == 0xa0000) {
		if (dp[1])
			(void) printf(" fh %ld.%ld.%lu", dp[0], dp[1], dp[3]);
		else
			(void) printf(" fh %ld.%ld", dp[0], dp[3]);
	} else if ((dp[2] >> 16) == 8)
		/*
		 * 'dp' is longword aligned, so we must use the extract
		 * macros below for dp+10 which cannot possibly be aligned.
		 */
		if (dp[1])
			(void) printf(" fh %ld.%ld.%lu", dp[0], dp[1],
				      EXTRACT_LONG((u_char *)dp + 10));
		else
			(void) printf(" fh %ld.%ld", dp[0],
				      EXTRACT_LONG((u_char *)dp + 10));
	/* On Ultrix pre-4.0, three longs: fsid, fno, fgen and then zeros */
	else if (dp[3] == 0) {
		(void)printf(" fh %d,%d/%ld.%ld", major(dp[0]), minor(dp[0]),
			     dp[1], dp[2]);
	}
	/*
	 * On Ultrix 4.0,
	 * five longs: fsid, fno, fgen, eno, egen and then zeros
	 */
	else if (dp[5] == 0) {
		(void)printf(" fh %d,%d/%ld.%ld", major(dp[0]), minor(dp[0]),
			     dp[1], dp[2]);
		if (vflag) {
			/* print additional info */
			(void)printf("[%ld.%ld]", dp[3], dp[4]);
		}
	}
	else
		(void) printf(" fh %lu.%lu.%lu.%lu",
		    dp[0], dp[1], dp[2], dp[3]);
}

/*
 * Print out an NFS filename.
 * Assumes that len bytes from cp are present in packet.
 */
static void
nfs_printfn(cp, len)
	register u_char *cp;
	register int len;
{
	register char c;

	/* Sanity */
	if (len >= 64) {
		fputs("[\">]", stdout);
		return;
	}
	/* Print out the filename */
	putchar('"');
	while (--len >= 0) {
		c = toascii(*cp++);
		if (!isascii(c)) {
			c = toascii(c);
			putchar('M');
			putchar('-');
		}
		if (!isprint(c)) {
			c ^= 0x40;	/* DEL to ?, others to alpha */
			putchar('^');
		}
		putchar(c);
	}
	putchar('"');
}
