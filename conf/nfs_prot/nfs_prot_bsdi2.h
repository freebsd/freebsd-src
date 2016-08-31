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
 * File: am-utils/conf/nfs_prot/nfs_prot_bsdi2.h
 *
 */

#ifndef _AMU_NFS_PROT_H
#define _AMU_NFS_PROT_H

#ifdef HAVE_NFS_NFSV2_H
# include <nfs/nfsv2.h>
#endif /* HAVE_NFS_NFSV2_H */
#ifdef HAVE_NFS_RPCV2_H
# include <nfs/rpcv2.h>
#endif /* HAVE_NFS_RPCV2_H */
#ifdef HAVE_NFS_NFS_H
# include <nfs/nfs.h>
#endif /* HAVE_NFS_NFS_H */
#ifdef HAVE_SYS_FS_NFS_H
# include <sys/fs/nfs.h>
#endif /* HAVE_SYS_FS_NFS_H */
#ifdef HAVE_RPCSVC_MOUNT_H
# include <rpcsvc/mount.h>
#endif /* HAVE_RPCSVC_MOUNT_H */

#ifdef HAVE_UFS_UFS_UFSMOUNT_H
# ifndef MAXQUOTAS
#  define MAXQUOTAS     2
# endif /* not MAXQUOTAS */
/* fake structure: too difficult to include other headers here */
struct netexport { int this_is_SO_wrong; };
# include <ufs/ufs/ufsmount.h>
#endif /* HAVE_UFS_UFS_UFSMOUNT_H */

/*
 * <msdosfs/msdosfsmount.h> is for kernel only on bsdi2, so do not
 * include it.
 */
#ifndef ____MSDOSFS_MSDOSFSMOUNT_H__
# define ____MSDOSFS_MSDOSFSMOUNT_H__
#endif /* ____MSDOSFS_MSDOSFSMOUNT_H__ */

/*
 * getifaddrs() on bsdi2 is broken.  Don't use it.
 */
#ifdef HAVE_GETIFADDRS
# undef HAVE_GETIFADDRS
#endif /* HAVE_GETIFADDRS */


/*
 * MACROS
 */

#define NFS_PORT 2049
#ifndef NFS_MAXDATA
# define NFS_MAXDATA 8192
#endif /* NFS_MAXDATA */
#define NFS_MAXPATHLEN 1024
#define NFS_MAXNAMLEN 255
#define NFS_FHSIZE 32
#define NFS_COOKIESIZE 4
#define	MNTPATHLEN 1024
#define	MNTNAMLEN 255

#define NFSMODE_FMT 0170000
#define NFSMODE_DIR 0040000
#define NFSMODE_CHR 0020000
#define NFSMODE_BLK 0060000
#define NFSMODE_REG 0100000
#define NFSMODE_LNK 0120000
#define NFSMODE_SOCK 0140000
#define NFSMODE_FIFO 0010000

#ifndef NFS_PROGRAM
# define NFS_PROGRAM ((u_long)100003)
#endif /* not NFS_PROGRAM */
#ifndef NFS_VERSION
# define NFS_VERSION ((u_long)2)
#endif /* not NFS_VERSION */

/*
 * BSD/OS 2.1 deprecated NFSPROC_ROOT and NFSPROC_WRITECACHE and set them
 * to NFSPROC_NOOP.
 * Since amd is its own NFS server, reinstate them.
 */
#if (NFSPROC_ROOT == NFSPROC_NOOP)
# undef NFSPROC_ROOT
# define NFSPROC_ROOT 3
#endif /* (NFSPROC_ROOT == NFSPROC_NOOP) */
#if (NFSPROC_WRITECACHE == NFSPROC_NOOP)
# undef NFSPROC_WRITECACHE
# define NFSPROC_WRITECACHE 7
#endif /* (NFSPROC_WRITECACHE == NFSPROC_NOOP) */

/* map field names */
#define ex_dir ex_name
#define gr_name g_name
#define gr_next g_next
#define ml_directory ml_path
#define ml_hostname ml_name
#define ml_next ml_nxt
#define fh_data fh_bytes

/*
 * TYPEDEFS:
 */
typedef char *dirpath;
typedef char *filename;
typedef char *name;
typedef char *nfspath;
typedef char nfscookie[NFS_COOKIESIZE];
typedef nfstype nfsftype;
typedef enum nfsstat nfsstat;
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

extern nfsattrstat *nfsproc_getattr_2_svc(nfsv2fh_t *, struct svc_req *);
extern nfsattrstat *nfsproc_setattr_2_svc(nfssattrargs *, struct svc_req *);
extern nfsattrstat *nfsproc_write_2_svc(nfswriteargs *, struct svc_req *);
extern nfsdiropres *nfsproc_create_2_svc(nfscreateargs *, struct svc_req *);
extern nfsdiropres *nfsproc_lookup_2_svc(nfsdiropargs *, struct svc_req *);
extern nfsdiropres *nfsproc_mkdir_2_svc(nfscreateargs *, struct svc_req *);
extern nfsstat *nfsproc_link_2_svc(nfslinkargs *, struct svc_req *);
extern nfsstat *nfsproc_remove_2_svc(nfsdiropargs *, struct svc_req *);
extern nfsstat *nfsproc_rename_2_svc(nfsrenameargs *, struct svc_req *);
extern nfsstat *nfsproc_rmdir_2_svc(nfsdiropargs *, struct svc_req *);
extern nfsstat *nfsproc_symlink_2_svc(nfssymlinkargs *, struct svc_req *);
extern nfsreaddirres *nfsproc_readdir_2_svc(nfsreaddirargs *, struct svc_req *);
extern nfsreadlinkres *nfsproc_readlink_2_svc(nfsv2fh_t *, struct svc_req *);
extern nfsreadres *nfsproc_read_2_svc(nfsreadargs *, struct svc_req *);
extern nfsstatfsres *nfsproc_statfs_2_svc(nfsv2fh_t *, struct svc_req *);
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
extern bool_t xdr_nfs_fh(XDR *, nfsv2fh_t*);
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
 * ENUMS:
 */

/*
 * Use AMU_ prefixes so as to not conflict with #define's in <nfs/nfsv2.h>.
 * It is vital! that the AMU_* match one-to-one with the NFS_OK and NFSERR_*
 * codes listed in <nfs/nfsv2.h>.
 */
enum nfsstat {
  AMU_NFS_OK = 0,
  AMU_NFSERR_PERM = 1,
  AMU_NFSERR_NOENT = 2,
  AMU_NFSERR_IO = 5,
  AMU_NFSERR_NXIO = 6,
  AMU_NFSERR_ACCES = 13,
  AMU_NFSERR_EXIST = 17,
  AMU_NFSERR_NODEV = 19,
  AMU_NFSERR_NOTDIR = 20,
  AMU_NFSERR_ISDIR = 21,
  AMU_NFSERR_FBIG = 27,
  AMU_NFSERR_NOSPC = 28,
  AMU_NFSERR_ROFS = 30,
  AMU_NFSERR_NAMETOOLONG = 63,
  AMU_NFSERR_NOTEMPTY = 66,
  AMU_NFSERR_DQUOT = 69,
  AMU_NFSERR_STALE = 70,
  AMU_NFSERR_WFLUSH = 99
};


/*
 * STRUCTURES:
 */

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
  nfsv2fh_t sag_fhandle;
  nfssattr sag_attributes;
};

struct diropargs {
  nfsv2fh_t da_fhandle;		/* was dir */
  filename da_name;
};

struct diropokres {
  nfsv2fh_t drok_fhandle;
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
  nfsv2fh_t ra_fhandle;
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
  nfsv2fh_t wra_fhandle;
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
  nfsv2fh_t la_fhandle;
  nfsdiropargs la_to;
};

struct symlinkargs {
  nfsdiropargs sla_from;
  nfspath sla_to;
  nfssattr sla_attributes;
};

struct readdirargs {
  nfsv2fh_t rda_fhandle;
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


/****************************************************************************/
/*** XDR ADDITIONS                                                        ***/
/****************************************************************************/

struct exports {
  dirpath ex_dir;
  groups ex_groups;
  exports ex_next;
};

typedef char fhandle[NFS_FHSIZE];

struct fhstatus {
  u_int fhs_status;
  union {
    fhandle fhs_fhandle;
  } fhstatus_u;
};

struct groups {
  name gr_name;
  groups gr_next;
};

struct mountlist {
  name ml_hostname;
  dirpath ml_directory;
  mountlist ml_next;
};


/****************************************************************************/
/*** NFS ADDITIONS                                                        ***/
/****************************************************************************/

#ifndef MOUNTPROG
# define MOUNTPROG RPCPROG_MNT
#endif /* not MOUNTPROG */

#ifndef MOUNTVERS
# define MOUNTVERS RPCMNT_VER1
#endif /* not MOUNTVERS */

#ifndef MOUNTPROC_MNT
# define MOUNTPROC_MNT RPCMNT_MOUNT
#endif /* not MOUNTPROC_MNT */

#ifndef MOUNTPROC_DUMP
# define MOUNTPROC_DUMP RPCMNT_DUMP
#endif /* not MOUNTPROC_DUMP */

#ifndef MOUNTPROC_UMNT
# define MOUNTPROC_UMNT RPCMNT_UMOUNT
#endif /* not MOUNTPROC_UMNT */

#ifndef MOUNTPROC_UMNTALL
# define MOUNTPROC_UMNTALL RPCMNT_UMNTALL
#endif /* not MOUNTPROC_UMNTALL */

#ifndef MOUNTPROC_EXPORT
# define MOUNTPROC_EXPORT RPCMNT_EXPORT
#endif /* not MOUNTPROC_EXPORT */


#endif /* not _AMU_NFS_PROT_H */
