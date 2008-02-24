/*-
 * Copyright (c) 1989, 1993, 1995
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Rick Macklem at The University of Guelph.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)nfs.h	8.4 (Berkeley) 5/1/95
 * $FreeBSD: src/sys/nfsclient/nfs.h,v 1.98.2.1 2007/10/12 19:18:46 mohans Exp $
 */

#ifndef _NFSCLIENT_NFS_H_
#define _NFSCLIENT_NFS_H_

#ifdef _KERNEL
#include "opt_nfs.h"
#endif

#include <nfsclient/nfsargs.h>

/*
 * Tunable constants for nfs
 */

#define NFS_TICKINTVL	10		/* Desired time for a tick (msec) */
#define NFS_HZ		(hz / nfs_ticks) /* Ticks/sec */
#define	NFS_TIMEO	(1 * NFS_HZ)	/* Default timeout = 1 second */
#define	NFS_MINTIMEO	(1 * NFS_HZ)	/* Min timeout to use */
#define	NFS_MAXTIMEO	(60 * NFS_HZ)	/* Max timeout to backoff to */
#define	NFS_MINIDEMTIMEO (5 * NFS_HZ)	/* Min timeout for non-idempotent ops*/
#define	NFS_MAXREXMIT	100		/* Stop counting after this many */
#define	NFS_RETRANS	10		/* Num of retrans for UDP soft mounts */
#define	NFS_RETRANS_TCP	2		/* Num of retrans for TCP soft mounts */
#define	NFS_MAXGRPS	16		/* Max. size of groups list */
#ifndef NFS_MINATTRTIMO
#define	NFS_MINATTRTIMO 3		/* VREG attrib cache timeout in sec */
#endif
#ifndef NFS_MAXATTRTIMO
#define	NFS_MAXATTRTIMO 60
#endif
#ifndef NFS_MINDIRATTRTIMO
#define	NFS_MINDIRATTRTIMO 30		/* VDIR attrib cache timeout in sec */
#endif
#ifndef NFS_MAXDIRATTRTIMO
#define	NFS_MAXDIRATTRTIMO 60
#endif
#define	NFS_WSIZE	8192		/* Def. write data size <= 8192 */
#define	NFS_RSIZE	8192		/* Def. read data size <= 8192 */
#define NFS_READDIRSIZE	8192		/* Def. readdir size */
#define	NFS_DEFRAHEAD	1		/* Def. read ahead # blocks */
#define	NFS_MAXRAHEAD	4		/* Max. read ahead # blocks */
#define	NFS_MAXASYNCDAEMON 	64	/* Max. number async_daemons runnable */
#define	NFS_DIRBLKSIZ	4096		/* Must be a multiple of DIRBLKSIZ */
#ifdef _KERNEL
#define	DIRBLKSIZ	512		/* XXX we used to use ufs's DIRBLKSIZ */
#endif
#define NFS_MAXDEADTHRESH	9	/* How long till we say 'server not responding' */

/*
 * Oddballs
 */
#define NFS_CMPFH(n, f, s) \
	((n)->n_fhsize == (s) && !bcmp((caddr_t)(n)->n_fhp, (caddr_t)(f), (s)))
#define NFS_ISV3(v)	(VFSTONFS((v)->v_mount)->nm_flag & NFSMNT_NFSV3)
#define NFS_ISV4(v)	(VFSTONFS((v)->v_mount)->nm_flag & NFSMNT_NFSV4)

#define NFSSTA_HASWRITEVERF	0x00040000  /* Has write verifier for V3 */
#define NFSSTA_GOTFSINFO	0x00100000  /* Got the V3 fsinfo */
#define	NFSSTA_SNDLOCK		0x01000000  /* Send socket lock */
#define	NFSSTA_WANTSND		0x02000000  /* Want above */
#define	NFSSTA_TIMEO		0x10000000  /* Experiencing a timeout */


/*
 * XXX to allow amd to include nfs.h without nfsproto.h
 */
#ifdef NFS_NPROCS
#include <nfsclient/nfsstats.h>
#endif

/*
 * vfs.nfs sysctl(3) identifiers
 */
#define NFS_NFSSTATS	1		/* struct: struct nfsstats */

/*
 * File context information for nfsv4.	Currently, there is only one
 * lockowner for the whole machine "0."
 */
struct nfs4_fctx {
	TAILQ_ENTRY(nfs4_fstate) next;
	uint32_t	refcnt;
	struct nfs4_lowner *lop;
	struct nfsnode *np;
	char		stateid[NFSX_V4STATEID];
};

#ifdef _KERNEL

#ifdef MALLOC_DECLARE
MALLOC_DECLARE(M_NFSREQ);
MALLOC_DECLARE(M_NFSDIROFF);
MALLOC_DECLARE(M_NFSBIGFH);
MALLOC_DECLARE(M_NFSHASH);
MALLOC_DECLARE(M_NFSDIRECTIO);
#endif

extern struct uma_zone *nfsmount_zone;

extern struct callout nfs_callout;
extern struct nfsstats nfsstats;
extern struct mtx nfs_iod_mtx;

extern int nfs_numasync;
extern unsigned int nfs_iodmax;
extern int nfs_pbuf_freecnt;
extern int nfs_ticks;

/* Data constants in XDR form */
extern u_int32_t nfs_true, nfs_false, nfs_xdrneg1;
extern u_int32_t rpc_reply, rpc_msgdenied, rpc_mismatch, rpc_vers;
extern u_int32_t rpc_auth_unix, rpc_msgaccepted, rpc_call, rpc_autherr;

extern int nfsv3_procid[NFS_NPROCS];

/*
 * Socket errors ignored for connectionless sockets??
 * For now, ignore them all
 */
#define	NFSIGNORE_SOERROR(s, e) \
		((e) != EINTR && (e) != EIO && \
		(e) != ERESTART && (e) != EWOULDBLOCK && \
		((s) & PR_CONNREQUIRED) == 0)

/*
 * Nfs outstanding request list element
 */
struct nfsreq {
	TAILQ_ENTRY(nfsreq) r_chain;
	struct mbuf	*r_mreq;
	struct mbuf	*r_mrep;
	struct mbuf	*r_md;
	caddr_t		r_dpos;
	struct nfsmount *r_nmp;
	struct vnode	*r_vp;
	u_int32_t	r_xid;
	int		r_flags;	/* flags on request, see below */
	int		r_retry;	/* max retransmission count */
	int		r_rexmit;	/* current retrans count */
	int		r_timer;	/* tick counter on reply */
	u_int32_t	r_procnum;	/* NFS procedure number */
	int		r_rtt;		/* RTT for rpc */
	int		r_lastmsg;	/* last tprintf */
	struct thread	*r_td;		/* Proc that did I/O system call */
	struct mtx	r_mtx;		/* Protects nfsreq fields */
};

/*
 * Queue head for nfsreq's
 */
extern TAILQ_HEAD(nfs_reqq, nfsreq) nfs_reqq;

/* Flag values for r_flags */
#define R_TIMING	0x01		/* timing request (in mntp) */
#define R_SENT		0x02		/* request has been sent */
#define	R_SOFTTERM	0x04		/* soft mnt, too many retries */
#define	R_RESENDERR	0x08		/* Resend failed */
#define	R_SOCKERR	0x10		/* Fatal error on socket */
#define	R_TPRINTFMSG	0x20		/* Did a tprintf msg. */
#define	R_MUSTRESEND	0x40		/* Must resend request */
#define	R_GETONEREP	0x80		/* Probe for one reply only */
#define	R_PIN_REQ	0x100		/* Pin request down (rexmit in prog or other) */

struct buf;
struct socket;
struct uio;
struct vattr;

/*
 * Pointers to ops that differ from v3 to v4
 */
struct nfs_rpcops {
	int	(*nr_readrpc)(struct vnode *vp, struct uio *uiop,
		    struct ucred *cred);
	int	(*nr_writerpc)(struct vnode *vp, struct uio *uiop,
		    struct ucred *cred, int *iomode, int *must_commit);
	int	(*nr_writebp)(struct buf *bp, int force, struct thread *td);
	int	(*nr_readlinkrpc)(struct vnode *vp, struct uio *uiop,
		    struct ucred *cred);
	void	(*nr_invaldir)(struct vnode *vp);
	int	(*nr_commit)(struct vnode *vp, u_quad_t offset, int cnt,
		    struct ucred *cred, struct thread *td);
};

/*
 * Defines for WebNFS
 */

#define WEBNFS_ESC_CHAR		'%'
#define WEBNFS_SPECCHAR_START	0x80

#define WEBNFS_NATIVE_CHAR	0x80
/*
 * ..
 * Possibly more here in the future.
 */

/*
 * Macro for converting escape characters in WebNFS pathnames.
 * Should really be in libkern.
 */

#define HEXTOC(c) \
	((c) >= 'a' ? ((c) - ('a' - 10)) : \
	    ((c) >= 'A' ? ((c) - ('A' - 10)) : ((c) - '0')))
#define HEXSTRTOI(p) \
	((HEXTOC(p[0]) << 4) + HEXTOC(p[1]))

/* nfs_sigintr() helper, when 'rep' has all we need */
#define NFS_SIGREP(rep)		nfs_sigintr((rep)->r_nmp, (rep), (rep)->r_td)

#ifdef NFS_DEBUG

extern int nfs_debug;
#define NFS_DEBUG_ASYNCIO	1 /* asynchronous i/o */
#define NFS_DEBUG_WG		2 /* server write gathering */
#define NFS_DEBUG_RC		4 /* server request caching */

#define NFS_DPF(cat, args)					\
	do {							\
		if (nfs_debug & NFS_DEBUG_##cat) printf args;	\
	} while (0)

#else

#define NFS_DPF(cat, args)

#endif

/*
 * On fast networks, the estimator will try to reduce the
 * timeout lower than the latency of the server's disks,
 * which results in too many timeouts, so cap the lower
 * bound.
 */
#define NFS_MINRTO	(NFS_HZ >> 2)

/*
 * Keep the RTO from increasing to unreasonably large values
 * when a server is not responding.
 */
#define NFS_MAXRTO	(20 * NFS_HZ)

enum nfs_rto_timer_t {
	NFS_DEFAULT_TIMER,
	NFS_GETATTR_TIMER,
	NFS_LOOKUP_TIMER,
	NFS_READ_TIMER,
	NFS_WRITE_TIMER,
};
#define NFS_MAX_TIMER	(NFS_WRITE_TIMER)

#define NFS_INITRTT	(NFS_HZ << 3)

vfs_init_t nfs_init;
vfs_uninit_t nfs_uninit;
int	nfs_mountroot(struct mount *mp, struct thread *td);

#ifndef NFS4_USE_RPCCLNT
int	nfs_send(struct socket *, struct sockaddr *, struct mbuf *,
	    struct nfsreq *);
int	nfs_connect_lock(struct nfsreq *);
void	nfs_connect_unlock(struct nfsreq *);
#endif /* ! NFS4_USE_RPCCLNT */

int	nfs_vinvalbuf(struct vnode *, int, struct thread *, int);
int	nfs_readrpc(struct vnode *, struct uio *, struct ucred *);
int	nfs_writerpc(struct vnode *, struct uio *, struct ucred *, int *,
	    int *);
int	nfs_commit(struct vnode *vp, u_quad_t offset, int cnt,
	    struct ucred *cred, struct thread *td);
int	nfs_readdirrpc(struct vnode *, struct uio *, struct ucred *);
int	nfs_nfsiodnew(void);
int	nfs_asyncio(struct nfsmount *, struct buf *, struct ucred *, struct thread *);
int	nfs_doio(struct vnode *, struct buf *, struct ucred *, struct thread *);
void	nfs_doio_directwrite (struct buf *);
void	nfs_up(struct nfsreq *, struct nfsmount *, struct thread *,
	    const char *, int);
void	nfs_down(struct nfsreq *, struct nfsmount *, struct thread *,
	    const char *, int, int);
int	nfs_readlinkrpc(struct vnode *, struct uio *, struct ucred *);
int	nfs_sigintr(struct nfsmount *, struct nfsreq *, struct thread *);
int	nfs_readdirplusrpc(struct vnode *, struct uio *, struct ucred *);
int	nfs_request(struct vnode *, struct mbuf *, int, struct thread *,
	    struct ucred *, struct mbuf **, struct mbuf **, caddr_t *);
int	nfs_loadattrcache(struct vnode **, struct mbuf **, caddr_t *,
	    struct vattr *, int);
int	nfsm_mbuftouio(struct mbuf **, struct uio *, int, caddr_t *);
void	nfs_nhinit(void);
void	nfs_nhuninit(void);
int	nfs_nmcancelreqs(struct nfsmount *);
void	nfs_timer(void*);

int	nfs_connect(struct nfsmount *, struct nfsreq *);
void	nfs_disconnect(struct nfsmount *);
void	nfs_safedisconnect(struct nfsmount *);
int	nfs_getattrcache(struct vnode *, struct vattr *);
int	nfs_iosize(struct nfsmount *nmp);
int	nfsm_strtmbuf(struct mbuf **, char **, const char *, long);
int	nfs_bioread(struct vnode *, struct uio *, int, struct ucred *);
int	nfsm_uiotombuf(struct uio *, struct mbuf **, int, caddr_t *);
void	nfs_clearcommit(struct mount *);
int	nfs_writebp(struct buf *, int, struct thread *);
int	nfs_fsinfo(struct nfsmount *, struct vnode *, struct ucred *,
	    struct thread *);
int	nfs_meta_setsize (struct vnode *, struct ucred *,
	    struct thread *, u_quad_t);

void	nfs_set_sigmask(struct thread *td, sigset_t *oldset);
void	nfs_restore_sigmask(struct thread *td, sigset_t *set);
int	nfs_msleep(struct thread *td, void *ident, struct mtx *mtx,
	    int priority, char *wmesg, int timo);

#endif	/* _KERNEL */

#endif
