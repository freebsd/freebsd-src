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
 * File: am-utils/conf/nfs_prot/nfs_prot_ultrix.h
 *
 */

#ifndef _AMU_NFS_PROT_H
#define _AMU_NFS_PROT_H

#ifdef HAVE_NFS_NFS_H
# include <nfs/nfs.h>
#endif /* HAVE_NFS_NFS_H */

/*
 * MACROS
 */

#if 0
/* <nfs/nfs.h> */
#define NFS_PORT 2049
#define NFS_MAXDATA 8192
#define NFS_MAXPATHLEN 1024
#define NFS_MAXNAMLEN 255
#define NFS_FHSIZE 32
#endif /* 0 */
/* sizeof(u_long) */
#define NFS_COOKIESIZE 4
#if 0
/* defaults: libamu/amu.h */
#define	MNTPATHLEN 1024
#define	MNTNAMLEN 255
#endif /* 0 */

#define NFSMODE_FMT 0170000
#define NFSMODE_DIR 0040000
#define NFSMODE_CHR 0020000
#define NFSMODE_BLK 0060000
#define NFSMODE_REG 0100000
#define NFSMODE_LNK 0120000
#define NFSMODE_SOCK 0140000
#define NFSMODE_FIFO 0010000

#if 0
/* <nfs/nfs.h> */
#ifndef NFS_PROGRAM
# define NFS_PROGRAM ((u_long)100003)
#endif /* not NFS_PROGRAM */
#ifndef NFS_VERSION
# define NFS_VERSION ((u_long)2)
#endif /* not NFS_VERSION */
#endif /* 0 */

/* could use (u_long)RFS_* with RFS_* from <nfs/nfs.h> */
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
#define ml_hostname	ml_name
#define ml_directory	ml_path
#define ml_next		ml_nxt
#define	gr_next		g_next
#define gr_name		g_name
#define ex_dir		ex_name

#define ca_where	ca_da
#define ca_attributes	ca_sa
#define drok_attributes	drok_attr
#define fh_data		fh_pad
#define na_fileid	na_nodeid
#define la_fhandle	la_from
#define nt_seconds	tv_sec
#define nt_useconds	tv_usec
#define ra_totalcount	ra_totcount
#define rlr_status	rl_status
#define rlr_u		rl_u
#define sla_to		sla_tnm
#define sla_attributes	sla_sa
#define sag_fhandle	saa_fh
#define sag_attributes	saa_sa


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
typedef fhandle_t fhandle;
typedef struct nfsattrstat nfsattrstat;
typedef struct nfscreatargs nfscreateargs;
typedef struct nfsdirlist nfsdirlist;
typedef struct nfsdiropargs nfsdiropargs;
typedef struct nfsdrok nfsdiropokres;
typedef struct nfsdiropres nfsdiropres;
typedef struct nfsentry nfsentry;
typedef struct exports *exports;
typedef struct exports exportnode;
typedef struct nfsfattr nfsfattr;
typedef struct fhstatus fhstatus;
typedef struct groups *groups;
typedef struct groups groupnode;
typedef struct nfslinkargs nfslinkargs;
typedef struct mountlist *mountlist;
typedef struct mountlist mountbody;
typedef struct timeval nfstime;
typedef struct nfsreadargs nfsreadargs;
typedef struct nfsreaddirargs nfsreaddirargs;
typedef struct nfsreaddirres nfsreaddirres;
typedef struct nfsreadlinkres nfsreadlinkres;
typedef struct nfsreadokres nfsreadokres;
typedef struct nfsreadres nfsreadres;
typedef struct nfsrnmargs nfsrenameargs;
typedef struct nfssattr nfssattr;
typedef struct nfssaargs nfssattrargs;
typedef struct nfsstatfsokres nfsstatfsokres;
typedef struct nfsstatfsres nfsstatfsres;
typedef struct nfsslargs nfssymlinkargs;
typedef struct writeargs nfswriteargs;


/*
 * EXTERNALS:
 */

extern void *nfsproc_null_2_svc(void *, struct svc_req *);
extern nfsattrstat *nfsproc_getattr_2_svc(fhandle_t *, struct svc_req *);
extern nfsattrstat *nfsproc_setattr_2_svc(nfssattrargs *, struct svc_req *);
extern void *nfsproc_root_2_svc(void *, struct svc_req *);
extern nfsdiropres *nfsproc_lookup_2_svc(nfsdiropargs *, struct svc_req *);
extern nfsreadlinkres *nfsproc_readlink_2_svc(fhandle_t *, struct svc_req *);
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
extern nfsstatfsres *nfsproc_statfs_2_svc(fhandle_t *, struct svc_req *);

extern bool_t xdr_nfsstat(XDR *, nfsstat*);
extern bool_t xdr_ftype(XDR *, nfsftype*);
extern bool_t xdr_nfs_fh(XDR *, fhandle_t*);
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

struct nfsreadlinkres {
  nfsstat rlr_status;
  union {
    nfspath rlr_data_u;
  } rlr_u;
};

struct nfsreadokres {
  nfsfattr raok_attributes;
  struct {
    u_int raok_len_u;
    char *raok_val_u;
  } raok_u;
};

struct nfsreadres {
  nfsstat rr_status;
  union {
    nfsreadokres rr_reply_u;
  } rr_u;
};

struct writeargs {
  fhandle_t wra_fhandle;
  u_int wra_beginoffset;
  u_int wra_offset;
  u_int wra_totalcount;
  struct {
    u_int wra_len_u;
    char *wra_val_u;
  } wra_u;
};

struct nfsreaddirargs {
  fhandle_t rda_fhandle;
  nfscookie rda_cookie;
  u_int rda_count;
};

struct nfsentry {
  u_int ne_fileid;
  filename ne_name;
  nfscookie ne_cookie;
  nfsentry *ne_nextentry;
};

struct nfsdirlist {
  nfsentry *dl_entries;
  bool_t dl_eof;
};

struct nfsreaddirres {
  nfsstat rdr_status;
  union {
    nfsdirlist rdr_reply_u;
  } rdr_u;
};

struct nfsstatfsokres {
  u_int sfrok_tsize;
  u_int sfrok_bsize;
  u_int sfrok_blocks;
  u_int sfrok_bfree;
  u_int sfrok_bavail;
};

struct nfsstatfsres {
  nfsstat sfr_status;
  union {
    nfsstatfsokres sfr_reply_u;
  } sfr_u;
};
#endif /* not _AMU_NFS_PROT_H */
