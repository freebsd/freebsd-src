/*
 * Copyright (c) 1997-2014 Erez Zadok
 * Copyright (c) 1990 Jan-Simon Pendry
 * Copyright (c) 1990 Imperial College of Science, Technology & Medicine
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Jan-Simon Pendry at Imperial College, London.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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
 *
 * File: am-utils/conf/nfs_prot/nfs_prot_nextstep.h
 *
 */

#ifndef _AMU_NFS_PROT_H
#define _AMU_NFS_PROT_H

#ifdef HAVE_RPCSVC_NFS_PROT_H
# include <rpcsvc/nfs_prot.h>
#endif /* HAVE_RPCSVC_NFS_PROT_H */
#ifdef HAVE_BSD_RPC_RPC_H
# include <bsd/rpc/rpc.h>
#endif /* HAVE_BSD_RPC_RPC_H */
#ifdef HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif /* HAVE_SYS_STAT_H */

/*
 * odd problems happen during configuration and nextstep's /bin/cc
 * gets strange errors from /bin/sh.  I suspect it may be a conflict
 * over some memory, a bad /bin/cc, or even kernel bug.
 * this is what I get, as a result of running cc inside configure:
 *  /bin/cc -o conftest -g -O2 -D_POSIX_SOURCE conftest.c -lrpcsvc
 *  configure:2294: illegal external declaration, found `#'
 *  configure:2388: illegal file name 'AMU_NFS_PROTOCOL_HEADER'
 *  configure:2416: illegal external declaration, found `{'
 * I have no explanation for it... -Erez
 * Solution: don't use /bin/cc, but gcc 2.7.2.1 or better!
 */

#if 0
/* cannot include this file b/c it refers to non-existing headers */
# include <bsd/nfs/nfs_mount.h>
#endif /* 0 */


/*
 * MACROS:
 */

/* ugly: fix up nextstep's nfs mount type */
#ifdef MOUNT_TYPE_NFS___off_no_longer_needed
# undef MOUNT_TYPE_UFS
# define MOUNT_TYPE_UFS		MOUNT_UFS
# undef MOUNT_TYPE_NFS
# define MOUNT_TYPE_NFS		MOUNT_NFS
# undef MOUNT_TYPE_PCFS
# define MOUNT_TYPE_PCFS	MOUNT_PC
# undef MOUNT_TYPE_LOFS
# define MOUNT_TYPE_LOFS	MOUNT_LO
# undef MOUNT_TYPE_SPECFS
# define MOUNT_TYPE_SPECFS	MOUNT_SPECFS
# undef MOUNT_TYPE_CFS
# define MOUNT_TYPE_CFS		MOUNT_CFS
#endif /* MOUNT_TYPE_NFS */

#define	dr_drok_u	diropres
#define ca_attributes	attributes
#define ca_where	where
#define da_fhandle	dir
#define da_name		name
#define dl_entries	entries
#define dl_eof		eof
#define dr_status	status
#define dr_u		diropres_u
#define drok_attributes	attributes
#define drok_fhandle	file
#define fh_data		data
#define la_fhandle	from
#define la_to		to
#define na_atime	atime
#define na_blocks	blocks
#define na_blocksize	blocksize
#define na_ctime	ctime
#define na_fileid	fileid
#define na_fsid		fsid
#define na_gid		gid
#define na_mode		mode
#define na_mtime	mtime
#define na_nlink	nlink
#define na_rdev		rdev
#define na_size		size
#define na_type		type
#define na_uid		uid
#define ne_cookie	cookie
#define ne_fileid	fileid
#define ne_name		name
#define ne_nextentry	nextentry
#define ns_attr_u	attributes
#define ns_status	status
#define ns_u		attrstat_u
#define nt_seconds	seconds
#define nt_useconds	useconds
#define ra_count	count
#define ra_fhandle	file
#define ra_offset	offset
#define ra_totalcount	totalcount
#define raok_attributes	attributes
#define raok_len_u	data_len
#define raok_u		data
#define raok_val_u	data_val
#define rda_cookie	cookie
#define rda_count	count
#define rda_fhandle	dir
#define rdr_reply_u	reply
#define rdr_status	status
#define rdr_u		readdirres_u
#define rlr_data_u	data
#define rlr_status	status
#define rlr_u		readlinkres_u
#define rna_from	from
#define rna_to		to
#define rr_reply_u	reply
#define rr_status	status
#define rr_u		readres_u
#define sa_atime	atime
#define sa_gid		gid
#define sa_mode		mode
#define sa_mtime	mtime
#define sa_size		size
#define sa_uid		uid
#define sag_attributes	attributes
#define sag_fhandle	file
#define sfr_reply_u	reply
#define sfr_status	status
#define sfr_u		statfsres_u
#define sfrok_bavail	bavail
#define sfrok_bfree	bfree
#define sfrok_blocks	blocks
#define sfrok_bsize	bsize
#define sfrok_tsize	tsize
#define sla_attributes	attributes
#define sla_from	from
#define sla_to		to
#define wra_beginoffset	beginoffset
#define wra_fhandle	file
#define wra_len_u	data_len
#define wra_offset	offset
#define wra_totalcount	totalcount
#define wra_u		data
#define wra_val_u	data_val

/* map field names */
#define ex_dir ex_name
#define gr_name g_name
#define gr_next g_next
#define ml_directory ml_path
#define ml_hostname ml_name
#define ml_next ml_nxt

/*
 * NFS mount option flags
 */
#define	NFSMNT_SOFT	0x001	/* soft mount (hard is default) */
#define	NFSMNT_WSIZE	0x002	/* set write size */
#define	NFSMNT_RSIZE	0x004	/* set read size */
#define	NFSMNT_TIMEO	0x008	/* set initial timeout */
#define	NFSMNT_RETRANS	0x010	/* set number of request retries */
#define	NFSMNT_HOSTNAME	0x020	/* set hostname for error printf */
#define	NFSMNT_INT	0x040	/* allow interrupts on hard mount */
#define	NFSMNT_NOAC	0x080	/* don't cache attributes */
#define	NFSMNT_ACREGMIN	0x0100	/* set min secs for file attr cache */
#define	NFSMNT_ACREGMAX	0x0200	/* set max secs for file attr cache */
#define	NFSMNT_ACDIRMIN	0x0400	/* set min secs for dir attr cache */
#define	NFSMNT_ACDIRMAX	0x0800	/* set max secs for dir attr cache */
#define NFSMNT_SECURE	0x1000	/* secure mount */
#define NFSMNT_NOCTO	0x2000	/* no close-to-open consistency */

#define NFS_PORT	2049
#ifndef NFS_MAXDATA
# define NFS_MAXDATA	8192
#endif /* NFS_MAXDATA */
#define NFS_MAXPATHLEN	1024
#define NFS_MAXNAMLEN	255
#define NFS_FHSIZE	32
#define NFS_COOKIESIZE	4
#define	MNTPATHLEN	1024
#define	MNTNAMLEN	255

#define NFSMODE_FMT	0170000
#define NFSMODE_DIR	0040000
#define NFSMODE_CHR	0020000
#define NFSMODE_BLK	0060000
#define NFSMODE_REG	0100000
#define NFSMODE_LNK	0120000
#define NFSMODE_SOCK	0140000
#define NFSMODE_FIFO	0010000

#ifndef NFS_PROGRAM
# define NFS_PROGRAM	((u_long)100003)
#endif /* not NFS_PROGRAM */
#ifndef NFS_VERSION
# define NFS_VERSION	((u_long)2)
#endif /* not NFS_VERSION */

#define NFSPROC_NULL		((u_long)0)
#define NFSPROC_GETATTR		((u_long)1)
#define NFSPROC_SETATTR		((u_long)2)
#define NFSPROC_ROOT		((u_long)3)
#define NFSPROC_LOOKUP		((u_long)4)
#define NFSPROC_READLINK	((u_long)5)
#define NFSPROC_READ		((u_long)6)
#define NFSPROC_WRITECACHE	((u_long)7)
#define NFSPROC_WRITE		((u_long)8)
#define NFSPROC_CREATE		((u_long)9)
#define NFSPROC_REMOVE		((u_long)10)
#define NFSPROC_RENAME		((u_long)11)
#define NFSPROC_LINK		((u_long)12)
#define NFSPROC_SYMLINK		((u_long)13)
#define NFSPROC_MKDIR		((u_long)14)
#define NFSPROC_RMDIR		((u_long)15)
#define NFSPROC_READDIR		((u_long)16)
#define NFSPROC_STATFS		((u_long)17)

/*
 * fix up or complete other some definitions...
 */
#define WNOHANG         01
#define WUNTRACED       02
#define WIFSTOPPED(status)      ((status) & 0100)
#define WSTOPSIG(status)        (int)(WIFSTOPPED(status) ? \
				      (((status) >> 8) & 0177) : -1)
#define WIFSIGNALED(status)     ( !WIFEXITED(status) && \
				  !WIFSTOPPED(status) )
#define WTERMSIG(status)        (int)(WIFSIGNALED(status) ? \
                                      ((status) & 0177) : -1)
/* define missing macros */
#ifndef S_ISDIR
# ifdef S_IFMT
#  define S_ISDIR(mode)   (((mode) & (S_IFMT)) == (S_IFDIR))
# else /* not S_IFMT */
#  define S_ISDIR(mode)   (((mode) & (_S_IFMT)) == (_S_IFDIR))
# endif /* not S_IFMT */
#endif /* not S_ISDIR */

/*
 * ENUMS:
 */

enum nfstype {
  NFNON = 0,
  NFREG = 1,
  NFDIR = 2,
  NFBLK = 3,
  NFCHR = 4,
  NFLNK = 5,
  NFSOCK = 6,
  NFBAD = 7,
  NFFIFO = 8,
};

enum nfsstat {
  NFS_OK = 0,                     /* no error */
  NFSERR_PERM=EPERM,              /* Not owner */
  NFSERR_NOENT=ENOENT,            /* No such file or directory */
  NFSERR_IO=EIO,                  /* I/O error */
  NFSERR_NXIO=ENXIO,              /* No such device or address */
  NFSERR_ACCES=EACCES,            /* Permission denied */
  NFSERR_EXIST=EEXIST,            /* File exists */
  NFSERR_NODEV=ENODEV,            /* No such device */
  NFSERR_NOTDIR=ENOTDIR,          /* Not a directory */
  NFSERR_ISDIR=EISDIR,            /* Is a directory */
  NFSERR_FBIG=EFBIG,              /* File too large */
  NFSERR_NOSPC=ENOSPC,            /* No space left on device */
  NFSERR_ROFS=EROFS,              /* Read-only file system */
  NFSERR_NAMETOOLONG=ENAMETOOLONG,/* File name too long */
  NFSERR_NOTEMPTY=ENOTEMPTY,      /* Directory not empty */
  NFSERR_DQUOT=EDQUOT,            /* Disc quota exceeded */
  NFSERR_STALE=ESTALE,            /* Stale NFS file handle */
  NFSERR_WFLUSH                   /* write cache flushed */
};


/*
 * TYPEDEFS:
 */
typedef char *dirpath;
typedef char *filename;
typedef char *name;
typedef char *nfspath;
typedef char fhandle[NFS_FHSIZE];
typedef char nfscookie[NFS_COOKIESIZE];
typedef enum ftype ftype;
typedef enum nfsstat nfsstat;
typedef enum nfstype nfsftype;
typedef fhandle fhandle_t;
typedef struct attrstat nfsattrstat;
typedef struct createargs nfscreateargs;
typedef struct dirlist nfsdirlist;
typedef struct diropargs nfsdiropargs;
typedef struct diropokres nfsdiropokres;
typedef struct diropres nfsdiropres;
typedef struct entry nfsentry;
typedef struct exports *exports;
typedef struct exports exportnode;
typedef struct fattr nfsfattr;
typedef struct fhstatus fhstatus;
typedef struct groups *groups;
typedef struct groups groupnode;
typedef struct linkargs nfslinkargs;
typedef struct mountlist *mountlist;
typedef struct mountlist mountbody;
typedef struct nfs_fh nfs_fh;
typedef struct nfstime nfstime;
typedef struct readargs nfsreadargs;
typedef struct readdirargs nfsreaddirargs;
typedef struct readdirres nfsreaddirres;
typedef struct readlinkres nfsreadlinkres;
typedef struct readokres nfsreadokres;
typedef struct readres nfsreadres;
typedef struct renameargs nfsrenameargs;
typedef struct sattr nfssattr;
typedef struct sattrargs nfssattrargs;
typedef struct statfsokres nfsstatfsokres;
typedef struct statfsres nfsstatfsres;
typedef struct symlinkargs nfssymlinkargs;
typedef struct writeargs nfswriteargs;


/*
 * EXTERNALS:
 */

extern nfsattrstat *nfsproc_getattr_2_svc(nfs_fh *, struct svc_req *);
extern nfsattrstat *nfsproc_setattr_2_svc(nfssattrargs *, struct svc_req *);
extern nfsattrstat *nfsproc_write_2_svc(nfswriteargs *, struct svc_req *);
extern nfsdiropres *nfsproc_create_2_svc(nfscreateargs *, struct svc_req *);
extern nfsdiropres *nfsproc_lookup_2_svc(nfsdiropargs *, struct svc_req *);
extern nfsdiropres *nfsproc_mkdir_2_svc(nfscreateargs *, struct svc_req *);
extern nfsreaddirres *nfsproc_readdir_2_svc(nfsreaddirargs *, struct svc_req *);
extern nfsreadlinkres *nfsproc_readlink_2_svc(nfs_fh *, struct svc_req *);
extern nfsreadres *nfsproc_read_2_svc(nfsreadargs *, struct svc_req *);
extern nfsstat *nfsproc_link_2_svc(nfslinkargs *, struct svc_req *);
extern nfsstat *nfsproc_remove_2_svc(nfsdiropargs *, struct svc_req *);
extern nfsstat *nfsproc_rename_2_svc(nfsrenameargs *, struct svc_req *);
extern nfsstat *nfsproc_rmdir_2_svc(nfsdiropargs *, struct svc_req *);
extern nfsstat *nfsproc_symlink_2_svc(nfssymlinkargs *, struct svc_req *);
extern nfsstatfsres *nfsproc_statfs_2_svc(nfs_fh *, struct svc_req *);
extern void *nfsproc_null_2_svc(void *, struct svc_req *);
extern void *nfsproc_root_2_svc(void *, struct svc_req *);
extern void *nfsproc_writecache_2_svc(void *, struct svc_req *);

extern bool_t xdr_attrstat(XDR *, nfsattrstat*);
extern bool_t xdr_createargs(XDR *, nfscreateargs*);
extern bool_t xdr_dirlist(XDR *, nfsdirlist*);
extern bool_t xdr_diropargs(XDR *, nfsdiropargs*);
extern bool_t xdr_diropokres(XDR *, nfsdiropokres*);
extern bool_t xdr_diropres(XDR *, nfsdiropres*);
extern bool_t xdr_entry(XDR *, nfsentry*);
extern bool_t xdr_fattr(XDR *, nfsfattr*);
extern bool_t xdr_filename(XDR *, filename*);
extern bool_t xdr_ftype(XDR *, nfsftype*);
extern bool_t xdr_linkargs(XDR *, nfslinkargs*);
extern bool_t xdr_mountlist(XDR *xdrs, mountlist *objp);
extern bool_t xdr_nfs_fh(XDR *, nfs_fh*);
extern bool_t xdr_nfscookie(XDR *, nfscookie);
extern bool_t xdr_nfspath(XDR *, nfspath*);
extern bool_t xdr_nfsstat(XDR *, nfsstat*);
extern bool_t xdr_nfstime(XDR *, nfstime*);
extern bool_t xdr_readargs(XDR *, nfsreadargs*);
extern bool_t xdr_readdirargs(XDR *, nfsreaddirargs*);
extern bool_t xdr_readdirres(XDR *, nfsreaddirres*);
extern bool_t xdr_readlinkres(XDR *, nfsreadlinkres*);
extern bool_t xdr_readokres(XDR *, nfsreadokres*);
extern bool_t xdr_readres(XDR *, nfsreadres*);
extern bool_t xdr_renameargs(XDR *, nfsrenameargs*);
extern bool_t xdr_sattr(XDR *, nfssattr*);
extern bool_t xdr_sattrargs(XDR *, nfssattrargs*);
extern bool_t xdr_statfsokres(XDR *, nfsstatfsokres*);
extern bool_t xdr_statfsres(XDR *, nfsstatfsres*);
extern bool_t xdr_symlinkargs(XDR *, nfssymlinkargs*);
extern bool_t xdr_writeargs(XDR *, nfswriteargs*);


/*
 * STRUCTURES:
 */

struct nfs_args {
  struct sockaddr_in	*addr;		/* file server address */
  caddr_t		fh;		/* File handle to be mounted */
  int			flags;		/* flags */
  int			wsize;		/* write size in bytes */
  int			rsize;		/* read size in bytes */
  int			timeo;		/* initial timeout in .1 secs */
  int			retrans;	/* times to retry send */
  char			*hostname;	/* server's hostname */
  int			acregmin;	/* attr cache file min secs */
  int			acregmax;	/* attr cache file max secs */
  int			acdirmin;	/* attr cache dir min secs */
  int			acdirmax;	/* attr cache dir max secs */
  char			*netname;	/* server's netname */
};

struct nfs_fh {
  char data[NFS_FHSIZE];
};

struct nfstime {
  u_int nt_seconds;
  u_int nt_useconds;
};

struct fattr {
  nfsftype na_type;
  u_int na_mode;
  u_int na_nlink;
  u_int na_uid;
  u_int na_gid;
  u_int na_size;
  u_int na_blocksize;
  u_int na_rdev;
  u_int na_blocks;
  u_int na_fsid;
  u_int na_fileid;
  nfstime na_atime;
  nfstime na_mtime;
  nfstime na_ctime;
};

struct sattr {
  u_int sa_mode;
  u_int sa_uid;
  u_int sa_gid;
  u_int sa_size;
  nfstime sa_atime;
  nfstime sa_mtime;
};

struct attrstat {
  nfsstat ns_status;
  union {
    nfsfattr ns_attr_u;
  } ns_u;
};

struct sattrargs {
  nfs_fh sag_fhandle;
  nfssattr sag_attributes;
};

struct diropargs {
  nfs_fh da_fhandle;		/* was dir */
  filename da_name;
};

struct diropokres {
  nfs_fh drok_fhandle;
  nfsfattr drok_attributes;
};

struct diropres {
  nfsstat dr_status;		/* was status */
  union {
    nfsdiropokres dr_drok_u;	/* was diropres */
  } dr_u;			/* was diropres_u */
};

struct readlinkres {
  nfsstat rlr_status;
  union {
    nfspath rlr_data_u;
  } rlr_u;
};

struct readargs {
  nfs_fh ra_fhandle;
  u_int ra_offset;
  u_int ra_count;
  u_int ra_totalcount;
};

struct readokres {
  nfsfattr raok_attributes;
  struct {
    u_int raok_len_u;
    char *raok_val_u;
  } raok_u;
};

struct readres {
  nfsstat rr_status;
  union {
    nfsreadokres rr_reply_u;
  } rr_u;
};

struct writeargs {
  nfs_fh wra_fhandle;
  u_int wra_beginoffset;
  u_int wra_offset;
  u_int wra_totalcount;
  struct {
    u_int wra_len_u;
    char *wra_val_u;
  } wra_u;
};

struct createargs {
  nfsdiropargs ca_where;
  nfssattr ca_attributes;
};

struct renameargs {
  nfsdiropargs rna_from;
  nfsdiropargs rna_to;
};

struct linkargs {
  nfs_fh la_fhandle;
  nfsdiropargs la_to;
};

struct symlinkargs {
  nfsdiropargs sla_from;
  nfspath sla_to;
  nfssattr sla_attributes;
};

struct readdirargs {
  nfs_fh rda_fhandle;
  nfscookie rda_cookie;
  u_int rda_count;
};

struct entry {
  u_int ne_fileid;
  filename ne_name;
  nfscookie ne_cookie;
  nfsentry *ne_nextentry;
};

struct dirlist {
  nfsentry *dl_entries;
  bool_t dl_eof;
};

struct readdirres {
  nfsstat rdr_status;
  union {
    nfsdirlist rdr_reply_u;
  } rdr_u;
};

struct statfsokres {
  u_int sfrok_tsize;
  u_int sfrok_bsize;
  u_int sfrok_blocks;
  u_int sfrok_bfree;
  u_int sfrok_bavail;
};

struct statfsres {
  nfsstat sfr_status;
  union {
    nfsstatfsokres sfr_reply_u;
  } sfr_u;
};

#endif /* not _AMU_NFS_PROT_H */
