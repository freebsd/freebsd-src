/*
 * include/linux/nfsd/export.h
 * 
 * Public declarations for NFS exports. The definitions for the
 * syscall interface are in nfsctl.h
 *
 * Copyright (C) 1995-1997 Olaf Kirch <okir@monad.swb.de>
 */

#ifndef NFSD_EXPORT_H
#define NFSD_EXPORT_H

#include <asm/types.h>
#ifdef __KERNEL__
# include <linux/types.h>
# include <linux/in.h>
#endif

/*
 * Important limits for the exports stuff.
 */
#define NFSCLNT_IDMAX		1024
#define NFSCLNT_ADDRMAX		16
#define NFSCLNT_KEYMAX		32

/*
 * Export flags.
 */
#define NFSEXP_READONLY		0x0001
#define NFSEXP_INSECURE_PORT	0x0002
#define NFSEXP_ROOTSQUASH	0x0004
#define NFSEXP_ALLSQUASH	0x0008
#define NFSEXP_ASYNC		0x0010
#define NFSEXP_GATHERED_WRITES	0x0020
#define NFSEXP_UIDMAP		0x0040
#define NFSEXP_KERBEROS		0x0080		/* not available */
#define NFSEXP_SUNSECURE	0x0100
#define NFSEXP_NOHIDE		0x0200
#define NFSEXP_NOSUBTREECHECK	0x0400
#define	NFSEXP_NOAUTHNLM	0x0800		/* Don't authenticate NLM requests - just trust */
#define NFSEXP_MSNFS		0x1000	/* do silly things that MS clients expect */
#define NFSEXP_FSID		0x2000
#define NFSEXP_ALLFLAGS		0x3FFF


#ifdef __KERNEL__

/* The following are hashtable sizes and must be powers of 2 */
#define NFSCLNT_EXPMAX		16

struct svc_client {
	struct svc_client *	cl_next;
	char			cl_ident[NFSCLNT_IDMAX];
	int			cl_idlen;
	int			cl_naddr;
	struct in_addr		cl_addr[NFSCLNT_ADDRMAX];
	struct svc_uidmap *	cl_umap;
	struct list_head	cl_export[NFSCLNT_EXPMAX];
	struct list_head	cl_expfsid[NFSCLNT_EXPMAX];
	struct list_head	cl_list;
};

struct svc_export {
	struct list_head	ex_hash;
	struct list_head	ex_fsid_hash;
	struct list_head	ex_list;
	char			ex_path[NFS_MAXPATHLEN+1];
	struct svc_export *	ex_parent;
	struct svc_client *	ex_client;
	int			ex_flags;
	struct vfsmount *	ex_mnt;
	struct dentry *		ex_dentry;
	kdev_t			ex_dev;
	ino_t			ex_ino;
	uid_t			ex_anon_uid;
	gid_t			ex_anon_gid;
	int			ex_fsid;
};

#define EX_SECURE(exp)		(!((exp)->ex_flags & NFSEXP_INSECURE_PORT))
#define EX_ISSYNC(exp)		(!((exp)->ex_flags & NFSEXP_ASYNC))
#define EX_RDONLY(exp)		((exp)->ex_flags & NFSEXP_READONLY)
#define EX_NOHIDE(exp)		((exp)->ex_flags & NFSEXP_NOHIDE)
#define EX_SUNSECURE(exp)	((exp)->ex_flags & NFSEXP_SUNSECURE)
#define EX_WGATHER(exp)		((exp)->ex_flags & NFSEXP_GATHERED_WRITES)


/*
 * Function declarations
 */
void			nfsd_export_init(void);
void			nfsd_export_shutdown(void);
void			exp_readlock(void);
int			exp_writelock(void);
void			exp_unlock(void);
struct svc_client *	exp_getclient(struct sockaddr_in *sin);
void			exp_putclient(struct svc_client *clp);
struct svc_export *	exp_get(struct svc_client *clp, kdev_t dev, ino_t ino);
struct svc_export *	exp_get_fsid(struct svc_client *clp, int fsid);
int			exp_rootfh(struct svc_client *, kdev_t, ino_t,
					char *path, struct knfsd_fh *, int maxsize);
int			nfserrno(int errno);
void			exp_nlmdetach(void);


#endif /* __KERNEL__ */

#endif /* NFSD_EXPORT_H */

