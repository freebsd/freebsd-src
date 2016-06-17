/*
 * linux/include/linux/nfsd/nfsd.h
 *
 * Hodge-podge collection of knfsd-related stuff.
 * I will sort this out later.
 *
 * Copyright (C) 1995-1997 Olaf Kirch <okir@monad.swb.de>
 */

#ifndef LINUX_NFSD_NFSD_H
#define LINUX_NFSD_NFSD_H

#include <linux/config.h>
#include <linux/types.h>
#include <linux/unistd.h>
#include <linux/dirent.h>
#include <linux/fs.h>

#include <linux/nfsd/debug.h>
#include <linux/nfsd/nfsfh.h>
#include <linux/nfsd/export.h>
#include <linux/nfsd/auth.h>
#include <linux/nfsd/stats.h>
#include <linux/nfsd/interface.h>
/*
 * nfsd version
 */
#define NFSD_VERSION		"0.5"

#ifdef __KERNEL__
/*
 * Special flags for nfsd_permission. These must be different from MAY_READ,
 * MAY_WRITE, and MAY_EXEC.
 */
#define MAY_NOP			0
#define MAY_SATTR		8
#define MAY_TRUNC		16
#define MAY_LOCK		32
#define MAY_OWNER_OVERRIDE	64
#define _NFSD_IRIX_BOGOSITY	128
#if (MAY_SATTR | MAY_TRUNC | MAY_LOCK | MAY_OWNER_OVERRIDE | _NFSD_IRIX_BOGOSITY) & (MAY_READ | MAY_WRITE | MAY_EXEC)
# error "please use a different value for MAY_SATTR or MAY_TRUNC or MAY_LOCK or MAY_OWNER_OVERRIDE."
#endif
#define MAY_CREATE		(MAY_EXEC|MAY_WRITE)
#define MAY_REMOVE		(MAY_EXEC|MAY_WRITE|MAY_TRUNC)

/*
 * Callback function for readdir
 */
struct readdir_cd {
	struct svc_rqst *	rqstp;
	struct svc_fh *		dirfh;
	u32 *			buffer;
	int			buflen;
	u32 *			offset;		/* previous dirent->d_next */
	char			plus;		/* readdirplus */
	char			eob;		/* end of buffer */
	char			dotonly;
};
typedef int		(*encode_dent_fn)(struct readdir_cd *, const char *,
						int, loff_t, ino_t, unsigned int);
typedef int (*nfsd_dirop_t)(struct inode *, struct dentry *, int, int);

/*
 * Procedure table for NFSv2
 */
extern struct svc_procedure	nfsd_procedures2[];
#ifdef CONFIG_NFSD_V3
extern struct svc_procedure	nfsd_procedures3[];
#endif /* CONFIG_NFSD_V3 */
extern struct svc_program	nfsd_program;

/*
 * Function prototypes.
 */
int		nfsd_svc(unsigned short port, int nrservs);

/* nfsd/vfs.c */
int		fh_lock_parent(struct svc_fh *, struct dentry *);
int		nfsd_racache_init(int);
void		nfsd_racache_shutdown(void);
int		nfsd_lookup(struct svc_rqst *, struct svc_fh *,
				const char *, int, struct svc_fh *);
int		nfsd_setattr(struct svc_rqst *, struct svc_fh *,
				struct iattr *, int, time_t);
int		nfsd_create(struct svc_rqst *, struct svc_fh *,
				char *name, int len, struct iattr *attrs,
				int type, dev_t rdev, struct svc_fh *res);
#ifdef CONFIG_NFSD_V3
int		nfsd_access(struct svc_rqst *, struct svc_fh *, u32 *);
int		nfsd_create_v3(struct svc_rqst *, struct svc_fh *,
				char *name, int len, struct iattr *attrs,
				struct svc_fh *res, int createmode,
				u32 *verifier);
int		nfsd_commit(struct svc_rqst *, struct svc_fh *,
				off_t, unsigned long);
#endif /* CONFIG_NFSD_V3 */
int		nfsd_open(struct svc_rqst *, struct svc_fh *, int,
				int, struct file *);
void		nfsd_close(struct file *);
int		nfsd_read(struct svc_rqst *, struct svc_fh *,
				loff_t, char *, unsigned long *);
int		nfsd_write(struct svc_rqst *, struct svc_fh *,
				loff_t, char *, unsigned long, int *);
int		nfsd_readlink(struct svc_rqst *, struct svc_fh *,
				char *, int *);
int		nfsd_symlink(struct svc_rqst *, struct svc_fh *,
				char *name, int len, char *path, int plen,
				struct svc_fh *res, struct iattr *);
int		nfsd_link(struct svc_rqst *, struct svc_fh *,
				char *, int, struct svc_fh *);
int		nfsd_rename(struct svc_rqst *,
				struct svc_fh *, char *, int,
				struct svc_fh *, char *, int);
int		nfsd_remove(struct svc_rqst *,
				struct svc_fh *, char *, int);
int		nfsd_unlink(struct svc_rqst *, struct svc_fh *, int type,
				char *name, int len);
int		nfsd_truncate(struct svc_rqst *, struct svc_fh *,
				unsigned long size);
int		nfsd_readdir(struct svc_rqst *, struct svc_fh *,
				loff_t, encode_dent_fn,
				u32 *buffer, int *countp, u32 *verf);
int		nfsd_statfs(struct svc_rqst *, struct svc_fh *,
				struct statfs *);

int		nfsd_notify_change(struct inode *, struct iattr *);
int		nfsd_permission(struct svc_export *, struct dentry *, int);


/*
 * lockd binding
 */
void		nfsd_lockd_init(void);
void		nfsd_lockd_shutdown(void);
void		nfsd_lockd_unexport(struct svc_client *);


/*
 * These macros provide pre-xdr'ed values for faster operation.
 */
#define	nfs_ok			__constant_htonl(NFS_OK)
#define	nfserr_perm		__constant_htonl(NFSERR_PERM)
#define	nfserr_noent		__constant_htonl(NFSERR_NOENT)
#define	nfserr_io		__constant_htonl(NFSERR_IO)
#define	nfserr_nxio		__constant_htonl(NFSERR_NXIO)
#define	nfserr_eagain		__constant_htonl(NFSERR_EAGAIN)
#define	nfserr_acces		__constant_htonl(NFSERR_ACCES)
#define	nfserr_exist		__constant_htonl(NFSERR_EXIST)
#define	nfserr_xdev		__constant_htonl(NFSERR_XDEV)
#define	nfserr_nodev		__constant_htonl(NFSERR_NODEV)
#define	nfserr_notdir		__constant_htonl(NFSERR_NOTDIR)
#define	nfserr_isdir		__constant_htonl(NFSERR_ISDIR)
#define	nfserr_inval		__constant_htonl(NFSERR_INVAL)
#define	nfserr_fbig		__constant_htonl(NFSERR_FBIG)
#define	nfserr_nospc		__constant_htonl(NFSERR_NOSPC)
#define	nfserr_rofs		__constant_htonl(NFSERR_ROFS)
#define	nfserr_mlink		__constant_htonl(NFSERR_MLINK)
#define	nfserr_opnotsupp	__constant_htonl(NFSERR_OPNOTSUPP)
#define	nfserr_nametoolong	__constant_htonl(NFSERR_NAMETOOLONG)
#define	nfserr_notempty		__constant_htonl(NFSERR_NOTEMPTY)
#define	nfserr_dquot		__constant_htonl(NFSERR_DQUOT)
#define	nfserr_stale		__constant_htonl(NFSERR_STALE)
#define	nfserr_remote		__constant_htonl(NFSERR_REMOTE)
#define	nfserr_wflush		__constant_htonl(NFSERR_WFLUSH)
#define	nfserr_badhandle	__constant_htonl(NFSERR_BADHANDLE)
#define	nfserr_notsync		__constant_htonl(NFSERR_NOT_SYNC)
#define	nfserr_badcookie	__constant_htonl(NFSERR_BAD_COOKIE)
#define	nfserr_notsupp		__constant_htonl(NFSERR_NOTSUPP)
#define	nfserr_toosmall		__constant_htonl(NFSERR_TOOSMALL)
#define	nfserr_serverfault	__constant_htonl(NFSERR_SERVERFAULT)
#define	nfserr_badtype		__constant_htonl(NFSERR_BADTYPE)
#define	nfserr_jukebox		__constant_htonl(NFSERR_JUKEBOX)

/* error code for internal use - if a request fails due to
 * kmalloc failure, it gets dropped.  Client should resend eventually
 */
#define	nfserr_dropit		__constant_htonl(30000)

/* Check for dir entries '.' and '..' */
#define isdotent(n, l)	(l < 3 && n[0] == '.' && (l == 1 || n[1] == '.'))

/*
 * Time of server startup
 */
extern struct timeval	nfssvc_boot;

#endif /* __KERNEL__ */

#endif /* LINUX_NFSD_NFSD_H */
