/*
 * include/linux/nfsd/syscall.h
 *
 * This file holds all declarations for the knfsd syscall interface.
 *
 * Copyright (C) 1995-1997 Olaf Kirch <okir@monad.swb.de>
 */

#ifndef NFSD_SYSCALL_H
#define NFSD_SYSCALL_H

#include <asm/types.h>
#ifdef __KERNEL__
# include <linux/config.h>
# include <linux/types.h>
# include <linux/in.h>
#endif 
#include <linux/posix_types.h>
#include <linux/nfsd/const.h>
#include <linux/nfsd/export.h>
#include <linux/nfsd/nfsfh.h>
#include <linux/nfsd/auth.h>

/*
 * Version of the syscall interface
 */
#define NFSCTL_VERSION		0x0201

/*
 * These are the commands understood by nfsctl().
 */
#define NFSCTL_SVC		0	/* This is a server process. */
#define NFSCTL_ADDCLIENT	1	/* Add an NFS client. */
#define NFSCTL_DELCLIENT	2	/* Remove an NFS client. */
#define NFSCTL_EXPORT		3	/* export a file system. */
#define NFSCTL_UNEXPORT		4	/* unexport a file system. */
#define NFSCTL_UGIDUPDATE	5	/* update a client's uid/gid map. */
#define NFSCTL_GETFH		6	/* get an fh by ino (used by mountd) */
#define NFSCTL_GETFD		7	/* get an fh by path (used by mountd) */
#define	NFSCTL_GETFS		8	/* get an fh by path with max FH len */

/* SVC */
struct nfsctl_svc {
	unsigned short		svc_port;
	int			svc_nthreads;
};

/* ADDCLIENT/DELCLIENT */
struct nfsctl_client {
	char			cl_ident[NFSCLNT_IDMAX+1];
	int			cl_naddr;
	struct in_addr		cl_addrlist[NFSCLNT_ADDRMAX];
	int			cl_fhkeytype;
	int			cl_fhkeylen;
	unsigned char		cl_fhkey[NFSCLNT_KEYMAX];
};

/* EXPORT/UNEXPORT */
struct nfsctl_export {
	char			ex_client[NFSCLNT_IDMAX+1];
	char			ex_path[NFS_MAXPATHLEN+1];
	__kernel_dev_t		ex_dev;
	__kernel_ino_t		ex_ino;
	int			ex_flags;
	__kernel_uid_t		ex_anon_uid;
	__kernel_gid_t		ex_anon_gid;
};

/* UGIDUPDATE */
struct nfsctl_uidmap {
	char *			ug_ident;
	__kernel_uid_t		ug_uidbase;
	int			ug_uidlen;
	__kernel_uid_t *	ug_udimap;
	__kernel_gid_t		ug_gidbase;
	int			ug_gidlen;
	__kernel_gid_t *	ug_gdimap;
};

/* GETFH */
struct nfsctl_fhparm {
	struct sockaddr		gf_addr;
	__kernel_dev_t		gf_dev;
	__kernel_ino_t		gf_ino;
	int			gf_version;
};

/* GETFD */
struct nfsctl_fdparm {
	struct sockaddr		gd_addr;
	char			gd_path[NFS_MAXPATHLEN+1];
	int			gd_version;
};

/* GETFS - GET Filehandle with Size */
struct nfsctl_fsparm {
	struct sockaddr		gd_addr;
	char			gd_path[NFS_MAXPATHLEN+1];
	int			gd_maxlen;
};

/*
 * This is the argument union.
 */
struct nfsctl_arg {
	int			ca_version;	/* safeguard */
	union {
		struct nfsctl_svc	u_svc;
		struct nfsctl_client	u_client;
		struct nfsctl_export	u_export;
		struct nfsctl_uidmap	u_umap;
		struct nfsctl_fhparm	u_getfh;
		struct nfsctl_fdparm	u_getfd;
		struct nfsctl_fsparm	u_getfs;
	} u;
#define ca_svc		u.u_svc
#define ca_client	u.u_client
#define ca_export	u.u_export
#define ca_umap		u.u_umap
#define ca_getfh	u.u_getfh
#define ca_getfd	u.u_getfd
#define	ca_getfs	u.u_getfs
#define ca_authd	u.u_authd
};

union nfsctl_res {
	__u8			cr_getfh[NFS_FHSIZE];
	struct knfsd_fh		cr_getfs;
};

#ifdef __KERNEL__
/*
 * Kernel syscall implementation.
 */
#if defined(CONFIG_NFSD) || defined(CONFIG_NFSD_MODULE)
extern asmlinkage long	sys_nfsservctl(int, void *, void *);
#else
#define sys_nfsservctl		sys_ni_syscall
#endif
extern int		exp_addclient(struct nfsctl_client *ncp);
extern int		exp_delclient(struct nfsctl_client *ncp);
extern int		exp_export(struct nfsctl_export *nxp);
extern int		exp_unexport(struct nfsctl_export *nxp);

#endif /* __KERNEL__ */

#endif /* NFSD_SYSCALL_H */
