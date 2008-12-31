/* $FreeBSD: src/sys/nfs4client/nfs4.h,v 1.2.18.1 2008/11/25 02:59:29 kensmith Exp $ */
/* $Id: nfs4.h,v 1.25 2003/11/05 14:58:58 rees Exp $ */

/*-
 * copyright (c) 2003
 * the regents of the university of michigan
 * all rights reserved
 * 
 * permission is granted to use, copy, create derivative works and redistribute
 * this software and such derivative works for any purpose, so long as the name
 * of the university of michigan is not used in any advertising or publicity
 * pertaining to the use or distribution of this software without specific,
 * written prior authorization.  if the above copyright notice or any other
 * identification of the university of michigan is included in any copy of any
 * portion of this software, then the disclaimer below must also be included.
 * 
 * this software is provided as is, without representation from the university
 * of michigan as to its fitness for any purpose, and without warranty by the
 * university of michigan of any kind, either express or implied, including
 * without limitation the implied warranties of merchantability and fitness for
 * a particular purpose. the regents of the university of michigan shall not be
 * liable for any damages, including special, indirect, incidental, or
 * consequential damages, with respect to any claim arising out of or in
 * connection with the use of the software, even if it has been or is hereafter
 * advised of the possibility of such damages.
 */

#ifndef _NFS4CLIENT_NFS4_H
#define _NFS4CLIENT_NFS4_H

#define NFS4_USE_RPCCLNT

#define NFS4_MINOR_VERSION		0
#define NFS_PORT			2049
#define NFS4_DEF_FILE_IO_BUFFER_SIZE	4096
#define NFS4_MAX_FILE_IO_BUFFER_SIZE	32768
#define NFS4_DEF_MAXFILESIZE		0xffffffff
#define NFS4_SUPER_MAGIC		0xF00BA4

#define NFS4FS_SILLY_RENAME		1
#define NFS4FS_STRICT_LOCKING		1
#define NFS4FS_RETRY_OLD_STATEID	1
#define NFS4FS_MIN_LEASE		(1 * hz)
#define NFS4FS_DEFAULT_LEASE		(30 * hz)
#define NFS4FS_MAX_LEASE		(120 * hz)
#define NFS4FS_RETRY_MIN_DELAY		(hz >> 4)
#define NFS4FS_RETRY_MAX_DELAY		(hz << 3)
#define NFS4FS_SEMAPHORE_DELAY		(hz >> 4)
#define NFS4FS_GRACE_DELAY		(hz * 5)
#define NFS4FS_OLD_STATEID_DELAY	(hz >> 3)
#define NFS4FS_OP_MAX			10


#define NFS4_BUFSIZE			8192
#define NFS4FS_MAX_IOV			10
#define NFS4_SETCLIENTID_MAXTRIES	5
#define NFS4_READDIR_MAXTRIES		5
#define NFS4_MAXIO			4
#define NFS4_MAX_REQUEST_SOFT		192
#define NFS4_MAX_REQUEST_HARD		256
#define NFS4_MAXCOMMIT			64
#define NFS4_READ_DELAY			(2 * HZ)
#define NFS4_WRITEBACK_DELAY		(5 * HZ)
#define NFS4_WRITEBACK_LOCKDELAY	(60 * HZ)
#define NFS4_COMMIT_DELAY		(5 * HZ)
#define RPC_SLACK_SPACE			512


struct nfs4_compound {
	char	       *tag;

	int		req_nops;
	uint32_t       *req_nopsp;
	uint32_t       *req_seqidp;
	uint32_t       *req_stateidp[NFS4_MAXIO];
	uint32_t	req_nstateid;

	u_int		seqidused;

	int		rep_status;
	int		rep_nops;

	struct nfs4_fctx *fcp;

	struct vnode   *curvp;
	struct vnode   *savevp;

	struct nfsmount *nmp;
};

struct nfs4_fdata {
	struct nfsnode *fd_n;
	pid_t		fd_pid;
};

struct nfs4_oparg_putfh {
	/* filled in by caller */
/*	  struct dentry *dentry;*/

	/* filled in by setup routine */
/*	  nfs_opnum4 op;*/
	uint32_t	fh_len;
	nfsfh_t		fh_val;
	int		nlookups;
};

struct nfs4_oparg_getattr {
	struct vnode   *vp;
	nfsv4bitmap    *bm;
	struct nfsv4_fattr fa;
};

struct nfs4_oparg_getfh {
	uint32_t	fh_len;
	nfsfh_t		fh_val;
	struct vnode   *vp;
};

struct nfs4_oparg_lookup {
	const char     *name;
	uint32_t	namelen;
	struct vnode   *vp;
};

struct nfs4_oparg_setclientid {
	struct nfsmount *np;
	uint32_t	namelen;
	char	       *name;
	char	       *cb_netid;
	uint32_t	cb_netidlen;
	char	       *cb_univaddr;
	uint32_t	cb_univaddrlen;
	uint32_t	cb_prog;

	uint64_t	clientid;
	u_char		verf[NFSX_V4VERF];
};

struct nfs4_oparg_access {
	uint32_t	mode;
	uint32_t	rmode;
	uint32_t	supported;
};

struct nfs4_oparg_open {
	uint32_t	flags;
	uint32_t	rflags;

	nfsv4cltype	ctype;
	struct vattr   *vap;
	struct componentname *cnp;

	struct nfs4_fctx *fcp;

	char		stateid[NFSX_V4STATEID];
};

struct nfs4_oparg_read {
	uint64_t	off;
	uint32_t	maxcnt;
	uint32_t	eof;
	uint32_t	retlen;
	struct uio     *uiop;
	struct nfs4_fctx *fcp;
};

struct nfs4_oparg_write {
	uint64_t	off;
	uint32_t	stable;
	uint32_t	cnt;
	uint32_t	retlen;
	uint32_t	committed;
	struct uio     *uiop;
	u_char		wverf[NFSX_V4VERF];
	struct nfs4_fctx *fcp;
};

struct nfs4_oparg_commit {
	uint32_t	len;
	off_t		start;

	u_char		verf[NFSX_V4VERF];
};

struct nfs4_oparg_readdir {
	uint32_t	cnt;
	nfsv4bitmap    *bm;
	uint64_t	cookie;
	u_char		verf[NFSX_V4VERF];
};

struct nfs4_oparg_create {
	nfstype		type;
	char	       *linktext;
	char	       *name;
	uint32_t	namelen;
	struct vattr   *vap;
};

struct nfs4_oparg_rename {
	const char     *fname;
	uint32_t	fnamelen;
	const char     *tname;
	uint32_t	tnamelen;
};

struct nfs4_oparg_link {
	const char     *name;
	uint32_t	namelen;
};

/*
 * Lockowner
 */
struct nfs4_lowner {
	uint32_t	lo_cnt;
	uint32_t	lo_seqid;
	uint32_t	lo_id;
};

#define NFS4_SEQIDMUTATINGERROR(err)		\
(((err) != NFSERR_STALE_CLIENTID) &&		\
 ((err) != NFSERR_BAD_SEQID) &&			\
 ((err) != NFSERR_STALE_STATEID) &&		\
 ((err) != NFSERR_BAD_STATEID))

/* Standard bitmasks */
extern nfsv4bitmap nfsv4_fsinfobm;
extern nfsv4bitmap nfsv4_fsattrbm;
extern nfsv4bitmap nfsv4_getattrbm;
extern nfsv4bitmap nfsv4_readdirbm;

vfs_init_t nfs4_init;
vfs_uninit_t nfs4_uninit;

uint32_t nfs_v4fileid4_to_fileid(uint64_t);

int	nfs4_readrpc(struct vnode *, struct uio *, struct ucred *);
int	nfs4_writerpc(struct vnode *, struct uio *, struct ucred *, int *,
	    int *);
int	nfs4_commit(struct vnode *vp, u_quad_t offset, int cnt,
	    struct ucred *cred, struct thread *td);
int	nfs4_readdirrpc(struct vnode *, struct uio *, struct ucred *);
int	nfs4_readlinkrpc(struct vnode *, struct uio *, struct ucred *);
int	nfs4_sigintr(struct nfsmount *, struct nfsreq *, struct thread *);
int	nfs4_writebp(struct buf *, int, struct thread *);
int	nfs4_request(struct vnode *, struct mbuf *, int, struct thread *,
	    struct ucred *, struct mbuf **, struct mbuf **, caddr_t *);
int	nfs4_request_mnt(struct nfsmount *, struct mbuf *, int, struct thread *,
	    struct ucred *, struct mbuf **, struct mbuf **, caddr_t *);
int	nfs4_connect(struct nfsmount *);
void	nfs4_disconnect(struct nfsmount *);
void	nfs4_safedisconnect(struct nfsmount *);
int	nfs4_nmcancelreqs(struct nfsmount *);

void	nfs_v4initcompound(struct nfs4_compound *);
int	nfs_v4postop(struct nfs4_compound *, int);
int	nfs_v4handlestatus(int, struct nfs4_compound *);

#endif /* _NFS4CLIENT_NFS4_H */
