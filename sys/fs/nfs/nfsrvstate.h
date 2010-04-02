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

#ifndef _NFS_NFSRVSTATE_H_
#define	_NFS_NFSRVSTATE_H_

/*
 * Definitions for NFS V4 server state handling.
 */

/*
 * List heads for nfsclient, nfsstate and nfslockfile.
 * (Some systems seem to like to dynamically size these things, but I
 *  don't see any point in doing so for these ones.)
 */
LIST_HEAD(nfsclienthashhead, nfsclient);
LIST_HEAD(nfsstatehead, nfsstate);
LIST_HEAD(nfslockhead, nfslock);
LIST_HEAD(nfslockhashhead, nfslockfile);

/*
 * List head for nfsusrgrp.
 */
LIST_HEAD(nfsuserhashhead, nfsusrgrp);
TAILQ_HEAD(nfsuserlruhead, nfsusrgrp);

#define	NFSCLIENTHASH(id)						\
	(&nfsclienthash[(id).lval[1] % NFSCLIENTHASHSIZE])
#define	NFSSTATEHASH(clp, id)						\
	(&((clp)->lc_stateid[(id).other[2] % NFSSTATEHASHSIZE]))
#define	NFSUSERHASH(id)							\
	(&nfsuserhash[(id) % NFSUSERHASHSIZE])
#define	NFSUSERNAMEHASH(p, l)						\
	(&nfsusernamehash[((l)>=4?(*(p)+*((p)+1)+*((p)+2)+*((p)+3)):*(p)) \
		% NFSUSERHASHSIZE])
#define	NFSGROUPHASH(id)						\
	(&nfsgrouphash[(id) % NFSGROUPHASHSIZE])
#define	NFSGROUPNAMEHASH(p, l)						\
	(&nfsgroupnamehash[((l)>=4?(*(p)+*((p)+1)+*((p)+2)+*((p)+3)):*(p)) \
		% NFSGROUPHASHSIZE])

/*
 * Client server structure for V4. It is doubly linked into two lists.
 * The first is a hash table based on the clientid and the second is a
 * list of all clients maintained in LRU order.
 * The actual size malloc'd is large enough to accomodate the id string.
 */
struct nfsclient {
	LIST_ENTRY(nfsclient) lc_hash;		/* Clientid hash list */
	struct nfsstatehead lc_stateid[NFSSTATEHASHSIZE]; /* stateid hash */
	struct nfsstatehead lc_open;		/* Open owner list */
	struct nfsstatehead lc_deleg;		/* Delegations */
	struct nfsstatehead lc_olddeleg;	/* and old delegations */
	time_t		lc_expiry;		/* Expiry time (sec) */
	time_t		lc_delegtime;		/* Old deleg expiry (sec) */
	nfsquad_t	lc_clientid;		/* 64 bit clientid */
	nfsquad_t	lc_confirm;		/* 64 bit confirm value */
	u_int32_t	lc_program;		/* RPC Program # */
	u_int32_t	lc_callback;		/* Callback id */
	u_int32_t	lc_stateindex;		/* Current state index# */
	u_int32_t	lc_statemaxindex;	/* Max state index# */
	u_int32_t	lc_cbref;		/* Cnt of callbacks */
	uid_t		lc_uid;			/* User credential */
	gid_t		lc_gid;
	u_int16_t	lc_namelen;
	u_char		*lc_name;
	struct nfssockreq lc_req;		/* Callback info */
	u_short		lc_idlen;		/* Length of id string */
	u_int32_t	lc_flags;		/* LCL_ flag bits */
	u_char		lc_verf[NFSX_VERF];	 /* client verifier */
	u_char		lc_id[1];		/* Malloc'd correct size */
};

#define	CLOPS_CONFIRM		0x0001
#define	CLOPS_RENEW		0x0002
#define	CLOPS_RENEWOP		0x0004

/*
 * Nfs state structure. I couldn't resist overloading this one, since
 * it makes cleanup, etc. simpler. These structures are used in four ways:
 * - open_owner structures chained off of nfsclient
 * - open file structures chained off an open_owner structure
 * - lock_owner structures chained off an open file structure
 * - delegated file structures chained off of nfsclient and nfslockfile
 * - the ls_list field is used for the chain it is in
 * - the ls_head structure is used to chain off the sibling structure
 *   (it is a union between an nfsstate and nfslock structure head)
 *    If it is a lockowner stateid, nfslock structures hang off it.
 * For the open file and lockowner cases, it is in the hash table in
 * nfsclient for stateid.
 */
struct nfsstate {
	LIST_ENTRY(nfsstate)	ls_hash;	/* Hash list entry */
	LIST_ENTRY(nfsstate)	ls_list;	/* List of opens/delegs */
	LIST_ENTRY(nfsstate)	ls_file;	/* Opens/Delegs for a file */
	union {
		struct nfsstatehead	open; /* Opens list */
		struct nfslockhead	lock; /* Locks list */
	} ls_head;
	nfsv4stateid_t		ls_stateid;	/* The state id */
	u_int32_t		ls_seq;		/* seq id */
	uid_t			ls_uid;		/* uid of locker */
	u_int32_t		ls_flags;	/* Type of lock, etc. */
	union {
		struct nfsstate	*openowner;	/* Open only */
		u_int32_t	opentolockseq;	/* Lock call only */
		u_int32_t	noopens;	/* Openowner only */
		struct {
			u_quad_t	filerev; /* Delegations only */
			time_t		expiry;
			time_t		limit;
			u_int64_t	compref;
		} deleg;
	} ls_un;
	struct nfslockfile	*ls_lfp;	/* Back pointer */
	struct nfsrvcache	*ls_op;		/* Op cache reference */
	struct nfsclient	*ls_clp;	/* Back pointer */
	u_short			ls_ownerlen;	/* Length of ls_owner */
	u_char			ls_owner[1];	/* malloc'd the correct size */
};
#define	ls_lock			ls_head.lock
#define	ls_open			ls_head.open
#define	ls_opentolockseq	ls_un.opentolockseq
#define	ls_openowner		ls_un.openowner
#define	ls_openstp		ls_un.openowner
#define	ls_noopens		ls_un.noopens
#define	ls_filerev		ls_un.deleg.filerev
#define	ls_delegtime		ls_un.deleg.expiry
#define	ls_delegtimelimit	ls_un.deleg.limit
#define	ls_compref		ls_un.deleg.compref

/*
 * Nfs lock structure.
 * This structure is chained off of the nfsstate (the lockowner) and
 * nfslockfile (the file) structures, for the file and owner it
 * refers to. It holds flags and a byte range.
 * It also has back pointers to the associated lock_owner and lockfile.
 */
struct nfslock {
	LIST_ENTRY(nfslock)	lo_lckowner;
	LIST_ENTRY(nfslock)	lo_lckfile;
	struct nfsstate		*lo_stp;
	struct nfslockfile	*lo_lfp;
	u_int64_t		lo_first;
	u_int64_t		lo_end;
	u_int32_t		lo_flags;
};

/*
 * Structure used to return a conflicting lock. (Must be large
 * enough for the largest lock owner we can have.)
 */
struct nfslockconflict {
	nfsquad_t		cl_clientid;
	u_int64_t		cl_first;
	u_int64_t		cl_end;
	u_int32_t		cl_flags;
	u_short			cl_ownerlen;
	u_char			cl_owner[NFSV4_OPAQUELIMIT];
};

/*
 * This structure is used to keep track of local locks that might need
 * to be rolled back.
 */
struct nfsrollback {
	LIST_ENTRY(nfsrollback)	rlck_list;
	uint64_t		rlck_first;
	uint64_t		rlck_end;
	int			rlck_type;
};

/*
 * This structure refers to a file for which lock(s) and/or open(s) exist.
 * Searched via hash table on file handle or found via the back pointer from an
 * open or lock owner.
 */
struct nfslockfile {
	LIST_HEAD(, nfsstate)	lf_open;	/* Open list */
	LIST_HEAD(, nfsstate)	lf_deleg;	/* Delegation list */
	LIST_HEAD(, nfslock)	lf_lock;	/* Lock list */
	LIST_HEAD(, nfslock)	lf_locallock;	/* Local lock list */
	LIST_HEAD(, nfsrollback) lf_rollback;	/* Local lock rollback list */
	LIST_ENTRY(nfslockfile)	lf_hash;	/* Hash list entry */
	fhandle_t		lf_fh;		/* The file handle */
	struct nfsv4lock	lf_locallock_lck; /* serialize local locking */
	int			lf_usecount;	/* Ref count for locking */
};

/*
 * This structure is malloc'd an chained off hash lists for user/group
 * names.
 */
struct nfsusrgrp {
	TAILQ_ENTRY(nfsusrgrp)	lug_lru;	/* LRU list */
	LIST_ENTRY(nfsusrgrp)	lug_numhash;	/* Hash by id# */
	LIST_ENTRY(nfsusrgrp)	lug_namehash;	/* and by name */
	time_t			lug_expiry;	/* Expiry time in sec */
	union {
		uid_t		un_uid;		/* id# */
		gid_t		un_gid;
	} lug_un;
	int			lug_namelen;	/* Name length */
	u_char			lug_name[1];	/* malloc'd correct length */
};
#define	lug_uid		lug_un.un_uid
#define	lug_gid		lug_un.un_gid

/*
 * These structures are used for the stable storage restart stuff.
 */
/*
 * Record at beginning of file.
 */
struct nfsf_rec {
	u_int32_t	lease;			/* Lease duration */
	u_int32_t	numboots;		/* Number of boottimes */
};

#if defined(_KERNEL) || defined(KERNEL)
void nfsrv_cleanclient(struct nfsclient *, NFSPROC_T *);
void nfsrv_freedeleglist(struct nfsstatehead *);
#endif

#endif	/* _NFS_NFSRVSTATE_H_ */
