/*
 * Copyright (c) 1989, 1993
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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
 *	@(#)nfs.h	8.1 (Berkeley) 6/10/93
 */

/*
 * Tunable constants for nfs
 */

#define	NFS_MAXIOVEC	34
#define NFS_HZ		25		/* Ticks per second for NFS timeouts */
#define	NFS_TIMEO	(1*NFS_HZ)	/* Default timeout = 1 second */
#define	NFS_MINTIMEO	(1*NFS_HZ)	/* Min timeout to use */
#define	NFS_MAXTIMEO	(60*NFS_HZ)	/* Max timeout to backoff to */
#define	NFS_MINIDEMTIMEO (5*NFS_HZ)	/* Min timeout for non-idempotent ops*/
#define	NFS_MAXREXMIT	100		/* Stop counting after this many */
#define	NFS_MAXWINDOW	1024		/* Max number of outstanding requests */
#define	NFS_RETRANS	10		/* Num of retrans for soft mounts */
#define	NFS_MAXGRPS	16		/* Max. size of groups list */
#define	NFS_MINATTRTIMO 5		/* Attribute cache timeout in sec */
#define	NFS_MAXATTRTIMO 60
#define	NFS_WSIZE	8192		/* Def. write data size <= 8192 */
#define	NFS_RSIZE	8192		/* Def. read data size <= 8192 */
#define	NFS_DEFRAHEAD	1		/* Def. read ahead # blocks */
#define	NFS_MAXRAHEAD	4		/* Max. read ahead # blocks */
#define	NFS_MAXREADDIR	NFS_MAXDATA	/* Max. size of directory read */
#define	NFS_MAXUIDHASH	64		/* Max. # of hashed uid entries/mp */
#define	NFS_MAXASYNCDAEMON 20	/* Max. number async_daemons runable */
#define	NFS_DIRBLKSIZ	1024		/* Size of an NFS directory block */
#define	NMOD(a)		((a) % nfs_asyncdaemons)

/*
 * Set the attribute timeout based on how recently the file has been modified.
 */
#define	NFS_ATTRTIMEO(np) \
	((((np)->n_flag & NMODIFIED) || \
	 (time.tv_sec - (np)->n_mtime) / 10 < NFS_MINATTRTIMO) ? NFS_MINATTRTIMO : \
	 ((time.tv_sec - (np)->n_mtime) / 10 > NFS_MAXATTRTIMO ? NFS_MAXATTRTIMO : \
	  (time.tv_sec - (np)->n_mtime) / 10))

/*
 * Structures for the nfssvc(2) syscall. Not that anyone but nfsd and mount_nfs
 * should ever try and use it.
 */
struct nfsd_args {
	int	sock;		/* Socket to serve */
	caddr_t	name;		/* Client address for connection based sockets */
	int	namelen;	/* Length of name */
};

struct nfsd_srvargs {
	struct nfsd	*nsd_nfsd;	/* Pointer to in kernel nfsd struct */
	uid_t		nsd_uid;	/* Effective uid mapped to cred */
	u_long		nsd_haddr;	/* Ip address of client */
	struct ucred	nsd_cr;		/* Cred. uid maps to */
	int		nsd_authlen;	/* Length of auth string (ret) */
	char		*nsd_authstr;	/* Auth string (ret) */
};

struct nfsd_cargs {
	char		*ncd_dirp;	/* Mount dir path */
	uid_t		ncd_authuid;	/* Effective uid */
	int		ncd_authtype;	/* Type of authenticator */
	int		ncd_authlen;	/* Length of authenticator string */
	char		*ncd_authstr;	/* Authenticator string */
};

/*
 * Stats structure
 */
struct nfsstats {
	int	attrcache_hits;
	int	attrcache_misses;
	int	lookupcache_hits;
	int	lookupcache_misses;
	int	direofcache_hits;
	int	direofcache_misses;
	int	biocache_reads;
	int	read_bios;
	int	read_physios;
	int	biocache_writes;
	int	write_bios;
	int	write_physios;
	int	biocache_readlinks;
	int	readlink_bios;
	int	biocache_readdirs;
	int	readdir_bios;
	int	rpccnt[NFS_NPROCS];
	int	rpcretries;
	int	srvrpccnt[NFS_NPROCS];
	int	srvrpc_errs;
	int	srv_errs;
	int	rpcrequests;
	int	rpctimeouts;
	int	rpcunexpected;
	int	rpcinvalid;
	int	srvcache_inproghits;
	int	srvcache_idemdonehits;
	int	srvcache_nonidemdonehits;
	int	srvcache_misses;
	int	srvnqnfs_leases;
	int	srvnqnfs_maxleases;
	int	srvnqnfs_getleases;
};

/*
 * Flags for nfssvc() system call.
 */
#define	NFSSVC_BIOD	0x002
#define	NFSSVC_NFSD	0x004
#define	NFSSVC_ADDSOCK	0x008
#define	NFSSVC_AUTHIN	0x010
#define	NFSSVC_GOTAUTH	0x040
#define	NFSSVC_AUTHINFAIL 0x080
#define	NFSSVC_MNTD	0x100

/*
 * The set of signals the interrupt an I/O in progress for NFSMNT_INT mounts.
 * What should be in this set is open to debate, but I believe that since
 * I/O system calls on ufs are never interrupted by signals the set should
 * be minimal. My reasoning is that many current programs that use signals
 * such as SIGALRM will not expect file I/O system calls to be interrupted
 * by them and break.
 */
#ifdef KERNEL
#define	NFSINT_SIGMASK	(sigmask(SIGINT)|sigmask(SIGTERM)|sigmask(SIGKILL)| \
			 sigmask(SIGHUP)|sigmask(SIGQUIT))

/*
 * Socket errors ignored for connectionless sockets??
 * For now, ignore them all
 */
#define	NFSIGNORE_SOERROR(s, e) \
		((e) != EINTR && (e) != ERESTART && (e) != EWOULDBLOCK && \
		((s) & PR_CONNREQUIRED) == 0)

/*
 * Nfs outstanding request list element
 */
struct nfsreq {
	struct nfsreq	*r_next;
	struct nfsreq	*r_prev;
	struct mbuf	*r_mreq;
	struct mbuf	*r_mrep;
	struct mbuf	*r_md;
	caddr_t		r_dpos;
	struct nfsmount *r_nmp;
	struct vnode	*r_vp;
	u_long		r_xid;
	int		r_flags;	/* flags on request, see below */
	int		r_retry;	/* max retransmission count */
	int		r_rexmit;	/* current retrans count */
	int		r_timer;	/* tick counter on reply */
	int		r_procnum;	/* NFS procedure number */
	int		r_rtt;		/* RTT for rpc */
	struct proc	*r_procp;	/* Proc that did I/O system call */
};

/* Flag values for r_flags */
#define R_TIMING	0x01		/* timing request (in mntp) */
#define R_SENT		0x02		/* request has been sent */
#define	R_SOFTTERM	0x04		/* soft mnt, too many retries */
#define	R_INTR		0x08		/* intr mnt, signal pending */
#define	R_SOCKERR	0x10		/* Fatal error on socket */
#define	R_TPRINTFMSG	0x20		/* Did a tprintf msg. */
#define	R_MUSTRESEND	0x40		/* Must resend request */
#define	R_GETONEREP	0x80		/* Probe for one reply only */

struct nfsstats nfsstats;

/*
 * A list of nfssvc_sock structures is maintained with all the sockets
 * that require service by the nfsd.
 * The nfsuid structs hang off of the nfssvc_sock structs in both lru
 * and uid hash lists.
 */
#define	NUIDHASHSIZ	32
#define	NUIDHASH(uid)	((uid) & (NUIDHASHSIZ - 1))

/*
 * Network address hash list element
 */
union nethostaddr {
	u_long had_inetaddr;
	struct mbuf *had_nam;
};

struct nfsuid {
	struct nfsuid	*nu_lrunext;	/* MUST be first */
	struct nfsuid	*nu_lruprev;
	struct nfsuid	*nu_hnext;
	struct nfsuid	*nu_hprev;
	int		nu_flag;	/* Flags */
	uid_t		nu_uid;		/* Uid mapped by this entry */
	union nethostaddr nu_haddr;	/* Host addr. for dgram sockets */
	struct ucred	nu_cr;		/* Cred uid mapped to */
};

#define	nu_inetaddr	nu_haddr.had_inetaddr
#define	nu_nam		nu_haddr.had_nam
/* Bits for nu_flag */
#define	NU_INETADDR	0x1

struct nfssvc_sock {
	struct nfsuid	*ns_lrunext;	/* MUST be first */
	struct nfsuid	*ns_lruprev;
	struct nfssvc_sock *ns_next;
	struct nfssvc_sock *ns_prev;
	int		ns_flag;
	u_long		ns_sref;
	struct file	*ns_fp;
	struct socket	*ns_so;
	int		ns_solock;
	struct mbuf	*ns_nam;
	int		ns_cc;
	struct mbuf	*ns_raw;
	struct mbuf	*ns_rawend;
	int		ns_reclen;
	struct mbuf	*ns_rec;
	struct mbuf	*ns_recend;
	int		ns_numuids;
	struct nfsuid	*ns_uidh[NUIDHASHSIZ];
};

/* Bits for "ns_flag" */
#define	SLP_VALID	0x01
#define	SLP_DOREC	0x02
#define	SLP_NEEDQ	0x04
#define	SLP_DISCONN	0x08
#define	SLP_GETSTREAM	0x10
#define	SLP_INIT	0x20
#define	SLP_WANTINIT	0x40

#define SLP_ALLFLAGS	0xff

/*
 * One of these structures is allocated for each nfsd.
 */
struct nfsd {
	struct nfsd	*nd_next;	/* Must be first */
	struct nfsd	*nd_prev;
	int		nd_flag;	/* NFSD_ flags */
	struct nfssvc_sock *nd_slp;	/* Current socket */
	struct mbuf	*nd_nam;	/* Client addr for datagram req. */
	struct mbuf	*nd_mrep;	/* Req. mbuf list */
	struct mbuf	*nd_md;
	caddr_t		nd_dpos;	/* Position in list */
	int		nd_procnum;	/* RPC procedure number */
	u_long		nd_retxid;	/* RPC xid */
	int		nd_repstat;	/* Reply status value */
	struct ucred	nd_cr;		/* Credentials for req. */
	int		nd_nqlflag;	/* Leasing flag */
	int		nd_duration;	/* Lease duration */
	int		nd_authlen;	/* Authenticator len */
	u_char		nd_authstr[RPCAUTH_MAXSIZ]; /* Authenticator data */
	struct proc	*nd_procp;	/* Proc ptr */
};

#define	NFSD_WAITING	0x01
#define	NFSD_CHECKSLP	0x02
#define	NFSD_REQINPROG	0x04
#define	NFSD_NEEDAUTH	0x08
#define	NFSD_AUTHFAIL	0x10
#endif	/* KERNEL */
