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
 * File: am-utils/conf/nfs_prot/nfs_prot_irix6.h
 *
 */

#ifndef _AMU_NFS_PROT_H
#define _AMU_NFS_PROT_H

#ifdef HAVE_NFS_NFSV2_H
# include <nfs/nfsv2.h>
#endif /* HAVE_NFS_NFSV2_H */
#ifdef HAVE_RPC_RPC_H
# include <rpc/rpc.h>
#endif /* HAVE_RPC_RPC_H */
#ifdef HAVE_NFS_RPCV2_H
# include <nfs/rpcv2.h>
#endif /* HAVE_NFS_RPCV2_H */
#ifdef HAVE_NFS_NFS_H
# include <nfs/nfs.h>
#endif /* HAVE_NFS_NFS_H */
#ifdef HAVE_SYS_FS_NFS_H
# include <sys/fs/nfs.h>
#endif /* HAVE_SYS_FS_NFS_H */

#ifdef HAVE_RPCSVC_MOUNT_H_off
# include <rpcsvc/mount.h>
#endif /* HAVE_RPCSVC_MOUNT_H */

/* evil: don't include */
#undef HAVE_RPCSVC_MOUNT_H

/*
 * MACROS
 */

#define NFS_PORT 2049
#define NFS_MAXDATA 8192
#define NFS_MAXPATHLEN 1024
#define NFS_MAXNAMLEN 255
#define NFS_FHSIZE 32
#define NFS_COOKIESIZE 4
#define	MNTPATHLEN 1024
#define	MNTNAMLEN 255
#define FHSIZE 32

#define MOUNTPROG 100005
#define MOUNTPROC_MNT 1
#define MOUNTPROC_DUMP 2
#define MOUNTPROC_UMNT 3
#define MOUNTPROC_UMNTALL 4
#define MOUNTPROC_EXPORT 5
#define MOUNTPROC_EXPORTALL 6
#define MOUNTVERS_ORIG 1
#define MOUNTVERS 1
#define MOUNTVERS3      3

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

#define NFSPROC_NULL ((u_long)0)
#define NFSPROC_GETATTR ((u_long)1)
#define NFSPROC_SETATTR ((u_long)2)
#define NFSPROC_ROOT ((u_long)3)
#define NFSPROC_LOOKUP ((u_long)4)
#define NFSPROC_READLINK ((u_long)5)
#define NFSPROC_READ ((u_long)6)
#define NFSPROC_WRITECACHE ((u_long)7)
#define NFSPROC_WRITE ((u_long)8)
#define NFSPROC_CREATE ((u_long)9)
#define NFSPROC_REMOVE ((u_long)10)
#define NFSPROC_RENAME ((u_long)11)
#define NFSPROC_LINK ((u_long)12)
#define NFSPROC_SYMLINK ((u_long)13)
#define NFSPROC_MKDIR ((u_long)14)
#define NFSPROC_RMDIR ((u_long)15)
#define NFSPROC_READDIR ((u_long)16)
#define NFSPROC_STATFS ((u_long)17)

/* map field names */
#define ml_hostname ml_name
#define ml_directory ml_path
#define ml_next ml_nxt
#define gr_next g_next
#define gr_name g_name
#define ex_dir ex_name

/*
 * Irix 6.2 does not define the proto/dev options, although they
 * do support them for NFS V3.
 */
#ifdef MNTTYPE_NFS3
# ifndef MNTOPT_PROTO
#  define MNTOPT_PROTO "proto"
# endif /* not MNTOPT_PROTO */
# ifndef MNTOPT_VERS
#  define MNTOPT_VERS "vers"
# endif /* not MNTOPT_VERS */
#endif /* MNTTYPE_NFS3 */

#define AUTOFS_CONFTYPE "udp"

/*
 * TYPEDEFS:
 */
typedef char *dirpath;
typedef char *filename;
typedef char *name;
typedef char *nfspath;
typedef char nfscookie[NFS_COOKIESIZE];
typedef enum nfsftype nfsftype;
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

extern void *nfsproc_null_2_svc(void *, struct svc_req *);
extern nfsattrstat *nfsproc_getattr_2_svc(nfs_fh *, struct svc_req *);
extern nfsattrstat *nfsproc_setattr_2_svc(nfssattrargs *, struct svc_req *);
extern void *nfsproc_root_2_svc(void *, struct svc_req *);
extern nfsdiropres *nfsproc_lookup_2_svc(nfsdiropargs *, struct svc_req *);
extern nfsreadlinkres *nfsproc_readlink_2_svc(nfs_fh *, struct svc_req *);
extern nfsreadres *nfsproc_read_2_svc(nfsreadargs *, struct svc_req *);
extern void *nfsproc_writecache_2_svc(void *, struct svc_req *);
extern nfsattrstat *nfsproc_write_2_svc(nfswriteargs *, struct svc_req *);
extern nfsdiropres *nfsproc_create_2_svc(nfscreateargs *, struct svc_req *);
extern nfsstat *nfsproc_remove_2_svc(nfsdiropargs *, struct svc_req *);
extern nfsstat *nfsproc_rename_2_svc(nfsrenameargs *, struct svc_req *);
extern nfsstat *nfsproc_link_2_svc(nfslinkargs *, struct svc_req *);
extern nfsstat *nfsproc_symlink_2_svc(nfssymlinkargs *, struct svc_req *);
extern nfsdiropres *nfsproc_mkdir_2_svc(nfscreateargs *, struct svc_req *);
extern nfsstat *nfsproc_rmdir_2_svc(nfsdiropargs *, struct svc_req *);
extern nfsreaddirres *nfsproc_readdir_2_svc(nfsreaddirargs *, struct svc_req *);
extern nfsstatfsres *nfsproc_statfs_2_svc(nfs_fh *, struct svc_req *);

extern bool_t xdr_nfsstat(XDR *, nfsstat*);
extern bool_t xdr_ftype(XDR *, nfsftype*);
extern bool_t xdr_nfs_fh(XDR *, nfs_fh*);
extern bool_t xdr_nfstime(XDR *, nfstime*);
extern bool_t xdr_fattr(XDR *, nfsfattr*);
extern bool_t xdr_sattr(XDR *, nfssattr*);
extern bool_t xdr_filename(XDR *, filename*);
extern bool_t xdr_nfspath(XDR *, nfspath*);
extern bool_t xdr_attrstat(XDR *, nfsattrstat*);
extern bool_t xdr_sattrargs(XDR *, nfssattrargs*);
extern bool_t xdr_diropargs(XDR *, nfsdiropargs*);
extern bool_t xdr_diropokres(XDR *, nfsdiropokres*);
extern bool_t xdr_diropres(XDR *, nfsdiropres*);
extern bool_t xdr_readlinkres(XDR *, nfsreadlinkres*);
extern bool_t xdr_readargs(XDR *, nfsreadargs*);
extern bool_t xdr_readokres(XDR *, nfsreadokres*);
extern bool_t xdr_readres(XDR *, nfsreadres*);
extern bool_t xdr_writeargs(XDR *, nfswriteargs*);
extern bool_t xdr_createargs(XDR *, nfscreateargs*);
extern bool_t xdr_renameargs(XDR *, nfsrenameargs*);
extern bool_t xdr_linkargs(XDR *, nfslinkargs*);
extern bool_t xdr_symlinkargs(XDR *, nfssymlinkargs*);
extern bool_t xdr_nfscookie(XDR *, nfscookie);
extern bool_t xdr_readdirargs(XDR *, nfsreaddirargs*);
extern bool_t xdr_entry(XDR *, nfsentry*);
extern bool_t xdr_dirlist(XDR *, nfsdirlist*);
extern bool_t xdr_readdirres(XDR *, nfsreaddirres*);
extern bool_t xdr_statfsokres(XDR *, nfsstatfsokres*);
extern bool_t xdr_statfsres(XDR *, nfsstatfsres*);


/*
 * STRUCTURES:
 */

struct nfs_fh {
  char fh_data[NFS_FHSIZE];
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


/*
 * Partial definitions from rpcsvc/mount.h (can't use that header
 * because it includes other "bad" stuff wrt xdr_groups.
 */

struct mountlist {
  char *ml_name;
  char *ml_path;
  struct mountlist *ml_nxt;
};

struct fhstatus {
  int fhs_status;
  fhandle_t fhs_fh;
};

typedef char fhandle[FHSIZE];

typedef struct {
  u_int fhandle3_len;
  char *fhandle3_val;
} fhandle3;

enum mountstat3 {
  MNT_OK = 0,
  MNT3ERR_PERM = 1,
  MNT3ERR_NOENT = 2,
  MNT3ERR_IO = 5,
  MNT3ERR_ACCES = 13,
  MNT3ERR_NOTDIR = 20,
  MNT3ERR_INVAL = 22,
  MNT3ERR_NAMETOOLONG = 63,
  MNT3ERR_NOTSUPP = 10004,
  MNT3ERR_SERVERFAULT = 10006
};
typedef enum mountstat3 mountstat3;

struct mountres3_ok {
  fhandle3 fhandle;
  struct {
    u_int auth_flavors_len;
    int *auth_flavors_val;
  } auth_flavors;
};
typedef struct mountres3_ok mountres3_ok;

struct mountres3 {
  mountstat3 fhs_status;
  union {
    mountres3_ok mountinfo;
  } mountres3_u;
};
typedef struct mountres3 mountres3;

/*
 * List of exported directories
 * An export entry with ex_groups
 * NULL indicates an entry which is exported to the world.
 */
struct exports {
  dev_t ex_dev;			/* dev of directory */
  char *ex_name;		/* name of directory */
  struct groups *ex_groups;	/* groups allowed to mount this entry */
  struct exports *ex_next;
  short ex_rootmap;		/* id to map root requests to */
  short ex_flags;		/* bits to mask off file mode */
};

struct groups {
  char *g_name;
  struct groups	*g_next;
};

extern bool_t xdr_groups(XDR *, groups *);
extern bool_t xdr_exports(XDR *, struct exports **);
extern bool_t xdr_mountres3_ok(XDR *, mountres3_ok *);
extern bool_t xdr_mountres3(XDR *, mountres3 *);
extern bool_t xdr_fhstatus(XDR *, struct fhstatus *);
extern bool_t xdr_mountlist(XDR *, struct mountlist **);

#endif /* not _AMU_NFS_PROT_H */
