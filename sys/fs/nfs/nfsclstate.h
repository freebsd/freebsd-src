/*-
 * Copyright (c) 2009 Rick Macklem, University of Guelph
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _NFS_NFSCLSTATE_H_
#define	_NFS_NFSCLSTATE_H_

/*
 * Definitions for NFS V4 client state handling.
 */
LIST_HEAD(nfsclopenhead, nfsclopen);
LIST_HEAD(nfscllockownerhead, nfscllockowner);
LIST_HEAD(nfscllockhead, nfscllock);
LIST_HEAD(nfsclhead, nfsclclient);
LIST_HEAD(nfsclownerhead, nfsclowner);
TAILQ_HEAD(nfscldeleghead, nfscldeleg);
LIST_HEAD(nfscldeleghash, nfscldeleg);
#define	NFSCLDELEGHASHSIZE	256
#define	NFSCLDELEGHASH(c, f, l)						\
	(&((c)->nfsc_deleghash[ncl_hash((f), (l)) % NFSCLDELEGHASHSIZE]))

struct nfsclclient {
	LIST_ENTRY(nfsclclient) nfsc_list;
	struct nfsclownerhead	nfsc_owner;
	struct nfscldeleghead	nfsc_deleg;
	struct nfscldeleghash	nfsc_deleghash[NFSCLDELEGHASHSIZE];
	struct nfscllockownerhead nfsc_defunctlockowner;
	struct nfsv4lock nfsc_lock;
	struct proc	*nfsc_renewthread;
	struct nfsmount	*nfsc_nmp;
	nfsquad_t	nfsc_clientid;
	time_t		nfsc_expire;
	u_int32_t	nfsc_clientidrev;
	u_int32_t	nfsc_renew;
	u_int32_t	nfsc_cbident;
	u_int16_t	nfsc_flags;
	u_int16_t	nfsc_idlen;
	u_int8_t	nfsc_id[1];	/* Malloc'd to correct length */
};

/*
 * Bits for nfsc_flags.
 */
#define	NFSCLFLAGS_INITED	0x0001
#define	NFSCLFLAGS_HASCLIENTID	0x0002
#define	NFSCLFLAGS_RECOVER	0x0004
#define	NFSCLFLAGS_UMOUNT	0x0008
#define	NFSCLFLAGS_HASTHREAD	0x0010
#define	NFSCLFLAGS_AFINET6	0x0020
#define	NFSCLFLAGS_EXPIREIT	0x0040
#define	NFSCLFLAGS_FIRSTDELEG	0x0080
#define	NFSCLFLAGS_GOTDELEG	0x0100
#define	NFSCLFLAGS_RECVRINPROG	0x0200

struct nfsclowner {
	LIST_ENTRY(nfsclowner)	nfsow_list;
	struct nfsclopenhead	nfsow_open;
	struct nfsclclient	*nfsow_clp;
	u_int32_t		nfsow_seqid;
	u_int32_t		nfsow_defunct;
	struct nfsv4lock	nfsow_rwlock;
	u_int8_t		nfsow_owner[NFSV4CL_LOCKNAMELEN];
};

/*
 * MALLOC'd to the correct length to accommodate the file handle.
 */
struct nfscldeleg {
	TAILQ_ENTRY(nfscldeleg)	nfsdl_list;
	LIST_ENTRY(nfscldeleg)	nfsdl_hash;
	struct nfsclownerhead	nfsdl_owner;	/* locally issued state */
	struct nfscllockownerhead nfsdl_lock;
	nfsv4stateid_t		nfsdl_stateid;
	struct acl_entry	nfsdl_ace;	/* Delegation ace */
	struct nfsclclient	*nfsdl_clp;
	struct nfsv4lock	nfsdl_rwlock;	/* for active I/O ops */
	struct nfscred		nfsdl_cred;	/* Cred. used for Open */
	time_t			nfsdl_timestamp; /* used for stale cleanup */
	u_int64_t		nfsdl_sizelimit; /* Limit for file growth */
	u_int64_t		nfsdl_size;	/* saved copy of file size */
	u_int64_t		nfsdl_change;	/* and change attribute */
	struct timespec		nfsdl_modtime;	/* local modify time */
	u_int16_t		nfsdl_fhlen;
	u_int8_t		nfsdl_flags;
	u_int8_t		nfsdl_fh[1];	/* must be last */
};

/*
 * nfsdl_flags bits.
 */
#define	NFSCLDL_READ		0x01
#define	NFSCLDL_WRITE		0x02
#define	NFSCLDL_RECALL		0x04
#define	NFSCLDL_NEEDRECLAIM	0x08
#define	NFSCLDL_ZAPPED		0x10
#define	NFSCLDL_MODTIMESET	0x20

/*
 * MALLOC'd to the correct length to accommodate the file handle.
 */
struct nfsclopen {
	LIST_ENTRY(nfsclopen)	nfso_list;
	struct nfscllockownerhead nfso_lock;
	nfsv4stateid_t		nfso_stateid;
	struct nfsclowner	*nfso_own;
	struct nfscred		nfso_cred;	/* Cred. used for Open */
	u_int32_t		nfso_mode;
	u_int32_t		nfso_opencnt;
	u_int16_t		nfso_fhlen;
	u_int8_t		nfso_posixlock;	/* 1 for POSIX type locking */
	u_int8_t		nfso_fh[1];	/* must be last */
};

/*
 * Return values for nfscl_open(). NFSCLOPEN_OK must == 0.
 */
#define	NFSCLOPEN_OK		0
#define	NFSCLOPEN_DOOPEN	1
#define	NFSCLOPEN_DOOPENDOWNGRADE 2
#define	NFSCLOPEN_SETCRED	3

struct nfscllockowner {
	LIST_ENTRY(nfscllockowner) nfsl_list;
	struct nfscllockhead	nfsl_lock;
	struct nfsclopen	*nfsl_open;
	NFSPROC_T		*nfsl_inprog;
	nfsv4stateid_t		nfsl_stateid;
	u_int32_t		nfsl_seqid;
	u_int32_t		nfsl_defunct;
	struct nfsv4lock	nfsl_rwlock;
	u_int8_t		nfsl_owner[NFSV4CL_LOCKNAMELEN];
	u_int8_t		nfsl_openowner[NFSV4CL_LOCKNAMELEN];
};

/*
 * Byte range entry for the above lock owner.
 */
struct nfscllock {
	LIST_ENTRY(nfscllock)	nfslo_list;
	u_int64_t		nfslo_first;
	u_int64_t		nfslo_end;
	short			nfslo_type;
};

/*
 * Macro for incrementing the seqid#.
 */
#define	NFSCL_INCRSEQID(s, n)	do { 					\
	    if (((n)->nd_flag & ND_INCRSEQID))				\
		(s)++; 							\
	} while (0)

#endif	/* _NFS_NFSCLSTATE_H_ */
