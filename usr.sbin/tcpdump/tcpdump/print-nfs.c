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
    "@(#) $Header: /home/ncvs/src/usr.sbin/tcpdump/tcpdump/print-nfs.c,v 1.4 1995/08/23 05:18:54 pst Exp $ (LBL)";
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
#include <rpc/rpc.h>
#include <rpc/pmap_prot.h>

#include <ctype.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include "interface.h"
#include "addrtoname.h"
#include "extract.h"			/* must come after interface.h */

#include "nfs.h"
#include "nfsfh.h"

static void nfs_printfh(const u_int32 *, const int);
static void xid_map_enter(const struct rpc_msg *, const struct ip *);
static int32 xid_map_find(const struct rpc_msg *, const struct ip *, u_int32 *,
			  u_int32 *);
static void interp_reply(const struct rpc_msg *, u_int32, u_int32, int);
static const u_int32 *parse_post_op_attr(const u_int32 *, int);

/*
 * Mapping of old NFS Version 2 RPC numbers to generic numbers.
 */
u_int32 nfsv3_procid[NFS_NPROCS] = {
	NFSPROC_NULL,
	NFSPROC_GETATTR,
	NFSPROC_SETATTR,
	NFSPROC_NOOP,
	NFSPROC_LOOKUP,
	NFSPROC_READLINK,
	NFSPROC_READ,
	NFSPROC_NOOP,
	NFSPROC_WRITE,
	NFSPROC_CREATE,
	NFSPROC_REMOVE,
	NFSPROC_RENAME,
	NFSPROC_LINK,
	NFSPROC_SYMLINK,
	NFSPROC_MKDIR,
	NFSPROC_RMDIR,
	NFSPROC_READDIR,
	NFSPROC_FSSTAT,
	NFSPROC_NOOP,
	NFSPROC_NOOP,
	NFSPROC_NOOP,
	NFSPROC_NOOP,
	NFSPROC_NOOP,
	NFSPROC_NOOP,
	NFSPROC_NOOP,
	NFSPROC_NOOP
};

const char *nfsv3_writemodes[NFSV3WRITE_NMODES] = {
	"unstable",
	"datasync",
	"filesync"
};

static struct token type2str[] = {
	{ NFNON,	"NON" },
	{ NFREG,	"REG" },
	{ NFDIR,	"DIR" },
	{ NFBLK,	"BLK" },
	{ NFCHR,	"CHR" },
	{ NFLNK,	"LNK" },
	{ NFFIFO,	"FIFO" },
	{ 0,		NULL }
};

/*
 * Print out a 64-bit integer. This appears to be different on each system,
 * try to make the best of it. The integer stored as 2 consecutive XDR
 * encoded 32-bit integers, to which a pointer is passed.
 *
 * Assume that a system that has INT64_FORMAT defined, has a 64-bit
 * integer datatype and can print it.
 */ 

#define UNSIGNED 0
#define SIGNED   1
#define HEX      2

int print_int64(const u_int32 *dp, int how)
{
	static char buf[32];
#ifdef INT64_FORMAT
	u_int64 res;

	res = ((u_int64)ntohl(dp[0]) << 32) | (u_int64)ntohl(dp[1]);
	switch (how) {
	case SIGNED:
		printf(INT64_FORMAT, res);
		break;
	case UNSIGNED:
		printf(U_INT64_FORMAT, res);
		break;
	case HEX:
		printf(HEX_INT64_FORMAT, res);
		break;
	default:
		return (0);
	}
#else
	/*
	 * XXX - throw upper 32 bits away.
	 * Could also go for hex: printf("0x%x%x", dp[0], dp[1]);
	 */
	if (sign)
		printf("%ld", (int)dp[1]);
	else
		printf("%lu", (unsigned int)dp[1]);
#endif
	return 1;
}

static const u_int32 *
parse_sattr3(const u_int32 *dp, struct nfsv3_sattr *sa3)
{
	register const u_int32 *ep = (u_int32 *)snapend;

	if (dp + 1 > ep)
		return (0);
	if ((sa3->sa_modeset = ntohl(*dp++))) {
		if (dp + 1 > ep)
			return (0);
		sa3->sa_mode = ntohl(*dp++);
	}

	if (dp + 1 > ep)
		return (0);
	if ((sa3->sa_uidset = ntohl(*dp++))) {
		if (dp + 1 > ep)
			return (0);
		sa3->sa_uid = ntohl(*dp++);
	}

	if (dp + 1 > ep)
		return (0);
	if ((sa3->sa_gidset = ntohl(*dp++))) {
		if (dp + 1 > ep)
			return (0);
		sa3->sa_gid = ntohl(*dp++);
	}

	if (dp + 1 > ep)
		return (0);
	if ((sa3->sa_sizeset = ntohl(*dp++))) {
		if (dp + 1 > ep)
			return (0);
		sa3->sa_size = ntohl(*dp++);
	}

	if (dp + 1 > ep)
		return (0);
	if ((sa3->sa_atimetype = ntohl(*dp++)) == NFSV3SATTRTIME_TOCLIENT) {
		if (dp + 2 > ep)
			return (0);
		sa3->sa_atime.nfsv3_sec = ntohl(*dp++);
		sa3->sa_atime.nfsv3_nsec = ntohl(*dp++);
	}

	if (dp + 1 > ep)
		return (0);
	if ((sa3->sa_mtimetype = ntohl(*dp++)) == NFSV3SATTRTIME_TOCLIENT) {
		if (dp + 2 > ep)
			return (0);
		sa3->sa_mtime.nfsv3_sec = ntohl(*dp++);
		sa3->sa_mtime.nfsv3_nsec = ntohl(*dp++);
	}

	return dp;
}

void
print_sattr3(const struct nfsv3_sattr *sa3, int verbose)
{
	if (sa3->sa_modeset)
		printf(" mode %o", sa3->sa_mode);
	if (sa3->sa_uidset)
		printf(" uid %u", sa3->sa_uid);
	if (sa3->sa_gidset)
		printf(" gid %u", sa3->sa_gid);
	if (verbose > 1) {
		if (sa3->sa_atimetype == NFSV3SATTRTIME_TOCLIENT)
			printf(" atime %u.%06u", sa3->sa_atime.nfsv3_sec,
			       sa3->sa_atime.nfsv3_nsec);
		if (sa3->sa_mtimetype == NFSV3SATTRTIME_TOCLIENT)
			printf(" mtime %u.%06u", sa3->sa_mtime.nfsv3_sec,
			       sa3->sa_mtime.nfsv3_nsec);
	}
}

void
nfsreply_print(register const u_char *bp, int length,
	       register const u_char *bp2)
{
	register const struct rpc_msg *rp;
	register const struct ip *ip;
	u_int32 proc, vers;

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

	if (xid_map_find(rp, ip, &proc, &vers) >= 0)
		interp_reply(rp, proc, vers, length);
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
parsefh(register const u_int32 *dp, int v3)
{
	int len;

	if (v3) {
		if (dp + 1 > (u_int32 *)snapend)
			return (0);
		len = (int)ntohl(*dp) / 4;
		dp++;
	} else
		len = NFSX_V2FH / 4;

	if (dp + len <= (u_int32 *)snapend) {
		nfs_printfh(dp, len);
		return (dp + len);
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
parsefhn(register const u_int32 *dp, int v3)
{
	dp = parsefh(dp, v3);
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
	nfstype type;
	int proc, v3;
	struct nfsv3_sattr sa3;

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

	v3 = (ntohl(rp->rm_call.cb_vers) == NFS_VER3);
	proc = ntohl(rp->rm_call.cb_proc);

	if (!v3 && proc < NFS_NPROCS)
		proc =  nfsv3_procid[proc];

	switch (proc) {
	case NFSPROC_NOOP:
		printf(" nop");
		return;
	case NFSPROC_NULL:
		printf(" null");
		return;

	case NFSPROC_GETATTR:
		printf(" getattr");
		if ((dp = parsereq(rp, length)) != 0 && parsefh(dp, v3) != 0)
			return;
		break;

	case NFSPROC_SETATTR:
		printf(" setattr");
		if ((dp = parsereq(rp, length)) != 0 && parsefh(dp, v3) != 0)
			return;
		break;

	case NFSPROC_LOOKUP:
		printf(" lookup");
		if ((dp = parsereq(rp, length)) != 0 && parsefhn(dp, v3) != 0)
			return;
		break;

	case NFSPROC_ACCESS:
		printf(" access");
		if ((dp = parsereq(rp, length)) != 0 &&
		    (dp = parsefh(dp, v3)) != 0) {
			TCHECK(dp, 4);
			printf(" %04x", ntohl(dp[0]));
			return;
		}
		break;

	case NFSPROC_READLINK:
		printf(" readlink");
		if ((dp = parsereq(rp, length)) != 0 && parsefh(dp, v3) != 0)
			return;
		break;

	case NFSPROC_READ:
		printf(" read");
		if ((dp = parsereq(rp, length)) != 0 &&
		    (dp = parsefh(dp, v3)) != 0) {
			if (v3) {
				TCHECK(dp, 3 * sizeof(*dp));
				printf(" %lu bytes @ ", ntohl(dp[2]));
				print_int64(dp, UNSIGNED);
			} else {
				TCHECK(dp, 2 * sizeof(*dp));
				printf(" %lu bytes @ %lu",
				       ntohl(dp[1]), ntohl(dp[0]));
			}
			return;
		}
		break;

	case NFSPROC_WRITE:
		printf(" write");
		if ((dp = parsereq(rp, length)) != 0 &&
		    (dp = parsefh(dp, v3)) != 0) {
			if (v3) {
				TCHECK(dp, 3 * sizeof(*dp));
				printf(" %lu bytes @ ", ntohl(dp[4]));
				print_int64(dp, UNSIGNED);
				if (vflag) {
					dp += 3;
					TCHECK(dp, sizeof(*dp));
					printf(" <%s>",
					       nfsv3_writemodes[ntohl(*dp)]);
				}
			} else {
				TCHECK(dp, 4 * sizeof(*dp));
				printf(" %lu (%lu) bytes @ %lu (%lu)",
				       ntohl(dp[3]), ntohl(dp[2]),
				       ntohl(dp[1]), ntohl(dp[0]));
			}
			return;
		}
		break;

	case NFSPROC_CREATE:
		printf(" create");
		if ((dp = parsereq(rp, length)) != 0 && parsefhn(dp, v3) != 0)
			return;
		break;

	case NFSPROC_MKDIR:
		printf(" mkdir");
		if ((dp = parsereq(rp, length)) != 0 && parsefhn(dp, v3) != 0)
			return;
		break;

	case NFSPROC_SYMLINK:
		printf(" symlink");
		if ((dp = parsereq(rp, length)) != 0 &&
		    (dp = parsefhn(dp, v3)) != 0) {
			fputs(" -> ", stdout);
			if (v3 && (dp = parse_sattr3(dp, &sa3)) == 0)
				break;
			if (parsefn(dp) == 0)
				break;
			if (v3 && vflag)
				print_sattr3(&sa3, vflag);
			return;
		}
		break;

	case NFSPROC_MKNOD:
		printf(" mknod");
		if ((dp = parsereq(rp, length)) != 0 &&
		    (dp = parsefhn(dp, v3)) != 0) {
			if (dp + 1 > (u_int32 *)snapend)
				break;
			type = (nfstype)ntohl(*dp++);
			if ((dp = parse_sattr3(dp, &sa3)) == 0)
				break;
			printf(" %s", tok2str(type2str, "unk-ft %d", type));
			if (vflag && (type == NFCHR || type == NFBLK)) {
				if (dp + 2 > (u_int32 *)snapend)
					break;
				printf(" %u/%u", ntohl(dp[0]), ntohl(dp[1]));
				dp += 2;
			}
			if (vflag)
				print_sattr3(&sa3, vflag);
			return;
		}
		break;

	case NFSPROC_REMOVE:
		printf(" remove");
		if ((dp = parsereq(rp, length)) != 0 && parsefhn(dp, v3) != 0)
			return;
		break;

	case NFSPROC_RMDIR:
		printf(" rmdir");
		if ((dp = parsereq(rp, length)) != 0 && parsefhn(dp, v3) != 0)
			return;
		break;

	case NFSPROC_RENAME:
		printf(" rename");
		if ((dp = parsereq(rp, length)) != 0 &&
		    (dp = parsefhn(dp, v3)) != 0) {
			fputs(" ->", stdout);
			if (parsefhn(dp, v3) != 0)
				return;
		}
		break;

	case NFSPROC_LINK:
		printf(" link");
		if ((dp = parsereq(rp, length)) != 0 &&
		    (dp = parsefh(dp, v3)) != 0) {
			fputs(" ->", stdout);
			if (parsefhn(dp, v3) != 0)
				return;
		}
		break;

	case NFSPROC_READDIR:
		printf(" readdir");
		if ((dp = parsereq(rp, length)) != 0 &&
		    (dp = parsefh(dp, v3)) != 0) {
			if (v3) {
				TCHECK(dp, 20);
				/*
				 * We shouldn't really try to interpret the
				 * offset cookie here.
				 */
				printf(" %lu bytes @ ", ntohl(dp[4]));
				print_int64(dp, SIGNED);
				if (vflag)
					printf(" verf %08lx%08lx", dp[2],
					       dp[3]);
			} else {
				TCHECK(dp, 2 * sizeof(*dp));
				/*
				 * Print the offset as signed, since -1 is
				 * common, but offsets > 2^31 aren't.
				 */
				printf(" %lu bytes @ %ld", ntohl(dp[1]),
				       ntohl(dp[0]));
			}
			return;
		}
		break;

	case NFSPROC_READDIRPLUS:
		printf(" readdirplus");
		if ((dp = parsereq(rp, length)) != 0 &&
		    (dp = parsefh(dp, v3)) != 0) {
			TCHECK(dp, 20);
			/*
			 * We don't try to interpret the offset
			 * cookie here.
			 */
			printf(" %lu bytes @ ", ntohl(dp[4]));
			print_int64(dp, SIGNED);
			if (vflag)
				printf(" max %lu verf %08lx%08lx",
				       ntohl(dp[5]), dp[2], dp[3]);
			return;
		}
		break;

	case NFSPROC_FSSTAT:
		printf(" fsstat");
		if ((dp = parsereq(rp, length)) != 0 && parsefh(dp, v3) != 0)
			return;
		break;

	case NFSPROC_FSINFO:
		printf(" fsinfo");
		break;

	case NFSPROC_PATHCONF:
		printf(" pathconf");
		break;

	case NFSPROC_COMMIT:
		printf(" commit");
		if ((dp = parsereq(rp, length)) != 0 &&
		    (dp = parsefh(dp, v3)) != 0) {
			printf(" %lu bytes @ ", ntohl(dp[2]));
			print_int64(dp, UNSIGNED);
			return;
		}
		break;

	default:
		printf(" proc-%lu", ntohl(rp->rm_call.cb_proc));
		return;
	}
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
nfs_printfh(register const u_int32 *dp, const int len)
{
	my_fsid fsid;
	ino_t ino;
	char *sfsname = NULL;

	Parse_fh((caddr_t*)dp, len, &fsid, &ino, NULL, &sfsname, 0);

	if (sfsname) {
	    /* file system ID is ASCII, not numeric, for this server OS */
	    static char temp[NFSX_V3FHMAX+1];

	    /* Make sure string is null-terminated */
	    strncpy(temp, sfsname, NFSX_V3FHMAX);
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
	u_int32		vers;		/* program version (host order) */
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
	xmep->vers = ntohl(rp->rm_call.cb_vers);
}

/* Returns NFSPROC_xxx or -1 on failure */
static int
xid_map_find(const struct rpc_msg *rp, const struct ip *ip, u_int32 *proc,
	     u_int32 *vers)
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
			*proc = xmep->proc;
			*vers = xmep->vers;
			return 0;
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

/*
 * Not all systems have strerror().
 */
static char *
strerr(int errno)
{

	return (strerror(errno));
}

static const u_int32 *
parsestatus(const u_int32 *dp, int *er)
{
	int errno;
	T2CHECK(dp, 4);

	errno = ntohl(dp[0]);
	if (er)
		*er = errno;
	if (errno != 0 && !qflag) {
		char *errmsg;

		errmsg = strerr(errno);
		if (errmsg)
			printf(" ERROR: '%s'", errmsg);
		else
			printf(" ERROR: %d", errno);
	}
	return (dp + 1);
}

static const u_int32 *
parsefattr(const u_int32 *dp, int verbose, int v3)
{
	const struct nfs_fattr *fap;

	T2CHECK(dp,  5 * sizeof(*dp));

	fap = (const struct nfs_fattr *)dp;
	if (verbose) {
		printf(" %s %o ids %d/%d",
		    tok2str(type2str, "unk-ft %d ", ntohl(fap->fa_type)),
		       ntohl(fap->fa_mode), ntohl(fap->fa_uid),
		       ntohl(fap->fa_gid));
		if (v3) {
			T2CHECK(dp,  7 * sizeof(*dp));
			printf(" sz ");
			print_int64((u_int32 *)&fap->fa3_size, UNSIGNED);
			putchar(' ');
		}
		else {
			T2CHECK(dp,  6 * sizeof(*dp));
			printf(" sz %d ", ntohl(fap->fa2_size));
		}
	}
	/* print lots more stuff */
	if (verbose > 1) {
		if (v3) {
			T2CHECK(dp, 64);
			printf("nlink %d rdev %d/%d ",
			       ntohl(fap->fa_nlink),
			       ntohl(fap->fa3_rdev.specdata1),
			       ntohl(fap->fa3_rdev.specdata2));
			printf("fsid ");
			print_int64((u_int32 *)&fap->fa2_fsid, HEX);
			printf(" nodeid ");
			print_int64((u_int32 *)&fap->fa2_fileid, HEX);
			printf(" a/m/ctime %u.%06u ",
			       ntohl(fap->fa3_atime.nfsv3_sec),
			       ntohl(fap->fa3_atime.nfsv3_nsec));
			printf("%u.%06u ",
			       ntohl(fap->fa3_mtime.nfsv3_sec),
			       ntohl(fap->fa3_mtime.nfsv3_nsec));
			printf("%u.%06u ",
			       ntohl(fap->fa3_ctime.nfsv3_sec),
			       ntohl(fap->fa3_ctime.nfsv3_nsec));
		} else {
			T2CHECK(dp, 48);
			printf("nlink %d rdev %x fsid %x nodeid %x a/m/ctime ",
			       ntohl(fap->fa_nlink), ntohl(fap->fa2_rdev),
			       ntohl(fap->fa2_fsid), ntohl(fap->fa2_fileid));
			printf("%u.%06u ",
			       ntohl(fap->fa2_atime.nfsv2_sec),
			       ntohl(fap->fa2_atime.nfsv2_usec));
			printf("%u.%06u ",
			       ntohl(fap->fa2_mtime.nfsv2_sec),
			       ntohl(fap->fa2_mtime.nfsv2_usec));
			printf("%u.%06u ",
			       ntohl(fap->fa2_ctime.nfsv2_sec),
			       ntohl(fap->fa2_ctime.nfsv2_usec));
		}
	}
	return ((const u_int32 *)((unsigned char *)dp +
		(v3 ? NFSX_V3FATTR : NFSX_V2FATTR)));
}

static int
parseattrstat(const u_int32 *dp, int verbose, int v3)
{
	int er;

	dp = parsestatus(dp, &er);
	if (dp == NULL || er)
		return (0);

	return ((long)parsefattr(dp, verbose, v3));
}

static int
parsediropres(const u_int32 *dp)
{
	int er;

	if (!(dp = parsestatus(dp, &er)) || er)
		return (0);

	dp = parsefh(dp, 0);
	if (dp == NULL)
		return (0);

	return (parsefattr(dp, vflag, 0) != NULL);
}

static int
parselinkres(const u_int32 *dp, int v3)
{
	int er;

	dp = parsestatus(dp, &er);
	if (dp == NULL || er)
		return(0);
	if (v3 && !(dp = parse_post_op_attr(dp, vflag)))
		return (0);
	putchar(' ');
	return (parsefn(dp) != NULL);
}

static int
parsestatfs(const u_int32 *dp, int v3)
{
	const struct nfs_statfs *sfsp;
	int er;

	dp = parsestatus(dp, &er);
	if (dp == NULL || (!v3 && er))
		return(0);

	if (qflag)
		return(1);

	if (v3) {
		if (vflag)
			printf(" POST:");
		if (!(dp = parse_post_op_attr(dp, vflag)))
			return (0);
	}

	T2CHECK(dp, (v3 ? NFSX_V3STATFS : NFSX_V2STATFS));

	sfsp = (const struct nfs_statfs *)dp;

	if (v3) {
		printf(" tbytes ");
		print_int64((u_int32 *)&sfsp->sf_tbytes, UNSIGNED);
		printf(" fbytes ");
		print_int64((u_int32 *)&sfsp->sf_fbytes, UNSIGNED);
		printf(" abytes ");
		print_int64((u_int32 *)&sfsp->sf_abytes, UNSIGNED);
		if (vflag) {
			printf(" tfiles ");
			print_int64((u_int32 *)&sfsp->sf_tfiles, UNSIGNED);
			printf(" ffiles ");
			print_int64((u_int32 *)&sfsp->sf_ffiles, UNSIGNED);
			printf(" afiles ");
			print_int64((u_int32 *)&sfsp->sf_afiles, UNSIGNED);
			printf(" invar %lu", ntohl(sfsp->sf_invarsec));
		}
	} else {
		printf(" tsize %d bsize %d blocks %d bfree %d bavail %d",
		       ntohl(sfsp->sf_tsize), ntohl(sfsp->sf_bsize),
		       ntohl(sfsp->sf_blocks), ntohl(sfsp->sf_bfree),
		       ntohl(sfsp->sf_bavail));
	}

	return (1);
}

static int
parserddires(const u_int32 *dp)
{
	int er;

	dp = parsestatus(dp, &er);
	if (dp == 0 || er)
		return (0);
	if (qflag)
		return (1);

	T2CHECK(dp, 12);
	printf(" offset %x size %d ", ntohl(dp[0]), ntohl(dp[1]));
	if (dp[2] != 0)
		printf("eof");

	return (1);
}

static const u_int32 *
parse_wcc_attr(const u_int32 *dp)
{
	printf(" sz ");
	print_int64(dp, UNSIGNED);
	printf(" mtime %u.%06u ctime %u.%06u", ntohl(dp[2]), ntohl(dp[3]),
	       ntohl(dp[4]), ntohl(dp[5]));
	return (dp + 6);
}

/*
 * Pre operation attributes. Print only if vflag > 1.
 */
static const u_int32 *
parse_pre_op_attr(const u_int32 *dp, int verbose)
{
	T2CHECK(dp, 4);
	if (!ntohl(dp[0]))
		return (dp + 1);
	dp++;
	T2CHECK(dp, 24);
	if (verbose > 1) {
		return parse_wcc_attr(dp);
	} else {
		/* If not verbose enough, just skip over wcc_attr */
		return (dp + 6);
	}
}

/*
 * Post operation attributes are printed if vflag >= 1
 */
static const u_int32 *
parse_post_op_attr(const u_int32 *dp, int verbose)
{
	T2CHECK(dp, 4);
	if (!ntohl(dp[0]))
		return (dp + 1);
	dp++;
	if (verbose) {
		return parsefattr(dp, verbose, 1);
	} else
		return (dp + (NFSX_V3FATTR / sizeof (u_int32)));
}

static const u_int32 *
parse_wcc_data(const u_int32 *dp, int verbose)
{
	if (verbose > 1)
		printf(" PRE:");
	if (!(dp = parse_pre_op_attr(dp, verbose)))
		return (0);

	if (verbose)
		printf(" POST:");
	return parse_post_op_attr(dp, verbose);
}

static const u_int32 *
parsecreateopres(const u_int32 *dp, int verbose)
{
	int er;

	if (!(dp = parsestatus(dp, &er)))
		return (0);
	if (er)
		dp = parse_wcc_data(dp, verbose);
	else {
		T2CHECK(dp, 4);
		if (!ntohl(dp[0]))
			return (dp + 1);
		dp++;
		if (!(dp = parsefh(dp, 1)))
			return (0);
		if (verbose) {
			if (!(dp = parse_post_op_attr(dp, verbose)))
				return (0);
			if (vflag > 1) {
				printf("dir attr:");
				dp = parse_wcc_data(dp, verbose);
			}
		}
	}
	return (dp);
}

static int
parsewccres(const u_int32 *dp, int verbose)
{
	int er;

	if (!(dp = parsestatus(dp, &er)))
		return (0);
	return parse_wcc_data(dp, verbose) != 0;
}

static const u_int32 *
parsev3rddirres(const u_int32 *dp, int verbose)
{
	int er;

	if (!(dp = parsestatus(dp, &er)))
		return (0);
	if (vflag)
		printf(" POST:");
	if (!(dp = parse_post_op_attr(dp, verbose)))
		return (0);
	if (er)
		return dp;
	if (vflag) {
		T2CHECK(dp, 8);
		printf(" verf %08lx%08lx", dp[0], dp[1]);
		dp += 2;
	}
	return dp;
}

static int
parsefsinfo(const u_int32 *dp)
{
	struct nfsv3_fsinfo *sfp;
	int er;

	if (!(dp = parsestatus(dp, &er)))
		return (0);
	if (vflag)
		printf(" POST:");
	if (!(dp = parse_post_op_attr(dp, vflag)))
		return (0);
	if (er)
		return (1);

	T2CHECK(dp, sizeof (struct nfsv3_fsinfo));

	sfp = (struct nfsv3_fsinfo *)dp;
	printf(" rtmax %lu rtpref %lu wtmax %lu wtpref %lu dtpref %lu",
	       ntohl(sfp->fs_rtmax), ntohl(sfp->fs_rtpref),
	       ntohl(sfp->fs_wtmax), ntohl(sfp->fs_wtpref),
	       ntohl(sfp->fs_dtpref));
	if (vflag) {
		printf(" rtmult %lu wtmult %lu maxfsz ",
		       ntohl(sfp->fs_rtmult), ntohl(sfp->fs_wtmult));
		print_int64((u_int32 *)&sfp->fs_maxfilesize, UNSIGNED);
		printf(" delta %u.%06u ", ntohl(sfp->fs_timedelta.nfsv3_sec),
		       ntohl(sfp->fs_timedelta.nfsv3_nsec));
	}
	return (1);
}

static int
parsepathconf(const u_int32 *dp)
{
	int er;
	struct nfsv3_pathconf *spp;

	if (!(dp = parsestatus(dp, &er)))
		return (0);
	if (vflag)
		printf(" POST:");
	if (!(dp = parse_post_op_attr(dp, vflag)))
		return (0);
	if (er)
		return (1);

	T2CHECK(dp, sizeof (struct nfsv3_pathconf));

	spp = (struct nfsv3_pathconf *)dp;

	printf(" linkmax %lu namemax %lu %s %s %s %s",
	       ntohl(spp->pc_linkmax),
	       ntohl(spp->pc_namemax),
	       ntohl(spp->pc_notrunc) ? "notrunc" : "",
	       ntohl(spp->pc_chownrestricted) ? "chownres" : "",
	       ntohl(spp->pc_caseinsensitive) ? "igncase" : "",
	       ntohl(spp->pc_casepreserving) ? "keepcase" : "");
	return (0);
}
				
static void
interp_reply(const struct rpc_msg *rp, u_int32 proc, u_int32 vers, int length)
{
	register const u_int32 *dp;
	register int v3;
	register const u_char *ep = snapend;
	int er;

	v3 = (vers == NFS_VER3);

	if (!v3 && proc < NFS_NPROCS)
		proc = nfsv3_procid[proc];

	switch (proc) {

	case NFSPROC_NOOP:
		printf(" nop");
		return;

	case NFSPROC_NULL:
		printf(" null");
		return;

	case NFSPROC_GETATTR:
		printf(" getattr");
		dp = parserep(rp, length);
		if (dp != 0 && parseattrstat(dp, !qflag, v3) != 0)
			return;
		break;

	case NFSPROC_SETATTR:
		printf(" setattr");
		if (!(dp = parserep(rp, length)))
			return;
		if (v3) {
			if (parsewccres(dp, vflag))
				return;
		} else {
			if (parseattrstat(dp, !qflag, 0) != 0)
				return;
		}
		break;

	case NFSPROC_LOOKUP:
		printf(" lookup");
		if (!(dp = parserep(rp, length)))
			break;
		if (v3) {
			if (!(dp = parsestatus(dp, &er)))
				break;
			if (er) {
				if (vflag > 1) {
					printf(" post dattr:");
					dp = parse_post_op_attr(dp, vflag);
				}
			} else {
				if (!(dp = parsefh(dp, v3)))
					break;
				if ((dp = parse_post_op_attr(dp, vflag)) &&
				    vflag > 1) {
					printf(" post dattr:");
					dp = parse_post_op_attr(dp, vflag);
				}
			}
			if (dp)
				return;
		} else {
			if (parsediropres(dp) != 0)
				return;
		}
		break;

	case NFSPROC_ACCESS:
		printf(" access");
		dp = parserep(rp, length);
		if (!(dp = parsestatus(dp, &er)))
			break;
		if (vflag)
			printf(" attr:");
		if (!(dp = parse_post_op_attr(dp, vflag)))
			break;
		if (!er)
			printf(" c %04x", ntohl(dp[0]));
		return;

	case NFSPROC_READLINK:
		printf(" readlink");
		dp = parserep(rp, length);
		if (dp != 0 && parselinkres(dp, v3) != 0)
			return;
		break;

	case NFSPROC_READ:
		printf(" read");
		if (!(dp = parserep(rp, length)))
			break;
		if (v3) {
			if (!(dp = parsestatus(dp, &er)))
				break;
			if (!(dp = parse_post_op_attr(dp, vflag)))
				break;
			if (er)
				return;
			if (vflag) {
				TCHECK(dp, 8);
				printf("%lu bytes", ntohl(dp[0]));
				if (ntohl(dp[1]))
					printf(" EOF");
			}
			return;
		} else {
			if (parseattrstat(dp, vflag, 0) != 0)
				return;
		}
		break;

	case NFSPROC_WRITE:
		printf(" write");
		if (!(dp = parserep(rp, length)))
			break;
		if (v3) {
			if (!(dp = parsestatus(dp, &er)))
				break;
			if (!(dp = parse_wcc_data(dp, vflag)))
				break;
			if (er)
				return;
			if (vflag) {
				TCHECK(dp, 4);
				printf("%lu bytes", ntohl(dp[0]));
				if (vflag > 1) {
					TCHECK(dp, 4);
					printf(" <%s>",
					       nfsv3_writemodes[ntohl(dp[1])]);
				}
				return;
			}
		} else {
			if (parseattrstat(dp, vflag, v3) != 0)
				return;
		}
		break;

	case NFSPROC_CREATE:
		printf(" create");
		if (!(dp = parserep(rp, length)))
			break;
		if (v3) {
			if (parsecreateopres(dp, vflag) != 0)
				return;
		} else {
			if (parsediropres(dp) != 0)
				return;
		}
		break;

	case NFSPROC_MKDIR:
		printf(" mkdir");
		if (!(dp = parserep(rp, length)))
			break;
		if (v3) {
			if (parsecreateopres(dp, vflag) != 0)
				return;
		} else {
			if (parsediropres(dp) != 0)
				return;
		}
		break;

	case NFSPROC_SYMLINK:
		printf(" symlink");
		if (!(dp = parserep(rp, length)))
			break;
		if (v3) {
			if (parsecreateopres(dp, vflag) != 0)
				return;
		} else {
			if (parsestatus(dp, &er) != 0)
				return;
		}
		break;

	case NFSPROC_MKNOD:
		printf(" mknod");
		if (!(dp = parserep(rp, length)))
			break;
		if (parsecreateopres(dp, vflag) != 0)
			return;
		break;

	case NFSPROC_REMOVE:
		printf(" remove");
		if (!(dp = parserep(rp, length)))
			break;
		if (v3) {
			if (parsewccres(dp, vflag))
				return;
		} else {
			if (parsestatus(dp, &er) != 0)
				return;
		}
		break;

	case NFSPROC_RMDIR:
		printf(" rmdir");
		if (!(dp = parserep(rp, length)))
			break;
		if (v3) {
			if (parsewccres(dp, vflag))
				return;
		} else {
			if (parsestatus(dp, &er) != 0)
				return;
		}
		break;

	case NFSPROC_RENAME:
		printf(" rename");
		if (!(dp = parserep(rp, length)))
			break;
		if (v3) {
			if (!(dp = parsestatus(dp, &er)))
				break;
			if (vflag) {
				printf(" from:");
				if (!(dp = parse_wcc_data(dp, vflag)))
					break;
				printf(" to:");
				if (!(dp = parse_wcc_data(dp, vflag)))
					break;
			}
			return;
		} else {
			if (parsestatus(dp, &er) != 0)
				return;
		}
		break;

	case NFSPROC_LINK:
		printf(" link");
		if (!(dp = parserep(rp, length)))
			break;
		if (v3) {
			if (!(dp = parsestatus(dp, &er)))
				break;
			if (vflag) {
				printf(" file POST:");
				if (!(dp = parse_post_op_attr(dp, vflag)))
					break;
				printf(" dir:");
				if (!(dp = parse_wcc_data(dp, vflag)))
					break;
				return;
			}
		} else {
			if (parsestatus(dp, &er) != 0)
				return;
		}
		break;

	case NFSPROC_READDIR:
		printf(" readdir");
		if (!(dp = parserep(rp, length)))
			break;
		if (v3) {
			if (parsev3rddirres(dp, vflag))
				return;
		} else {
			if (parserddires(dp) != 0)
				return;
		}
		break;

	case NFSPROC_READDIRPLUS:
		printf(" readdirplus");
		if (!(dp = parserep(rp, length)))
			break;
		if (parsev3rddirres(dp, vflag))
			return;
		break;

	case NFSPROC_FSSTAT:
		printf(" fsstat");
		dp = parserep(rp, length);
		if (dp != 0 && parsestatfs(dp, v3) != 0)
			return;
		break;

	case NFSPROC_FSINFO:
		printf(" fsinfo");
		dp = parserep(rp, length);
		if (dp != 0 && parsefsinfo(dp) != 0)
			return;
		break;

	case NFSPROC_PATHCONF:
		printf(" pathconf");
		dp = parserep(rp, length);
		if (dp != 0 && parsepathconf(dp) != 0)
			return;
		break;

	case NFSPROC_COMMIT:
		printf(" commit");
		dp = parserep(rp, length);
		if (dp != 0 && parsewccres(dp, vflag) != 0)
			return;
		break;

	default:
		printf(" proc-%lu", proc);
		return;
	}
	fputs(" [|nfs]", stdout);
}
