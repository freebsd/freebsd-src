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
 * File: am-utils/conf/nfs_prot/nfs_prot_sunos5_4.h
 *
 */

#ifndef _AMU_NFS_PROT_H
#define _AMU_NFS_PROT_H

/*
 * Do not include <nfs/nfs.h> because Solaris 2.4 really does not have NFS
 * V.3 support, despite what their header files claim.
 */

#ifdef HAVE_RPCSVC_NFS_PROT_H
# include <rpcsvc/nfs_prot.h>
#endif /* HAVE_RPCSVC_NFS_PROT_H */

#define FHSIZE 32
typedef char fhandle[FHSIZE];
#define fhandle_t fhandle

#ifdef HAVE_RPCSVC_MOUNT_H
# include <rpcsvc/mount.h>
#endif /* HAVE_RPCSVC_MOUNT_H */
#ifdef HAVE_RPC_RPC_H
#include <rpc/rpc.h>
#endif /* HAVE_RPC_RPC_H */

/* missing mntent definition for cachefs */
#ifndef MNTTYPE_CACHEFS
# define MNTTYPE_CACHEFS "cachefs"       /* Cache File System */
#endif /* not MNTTYPE_CACHEFS */

/*
 * Solaris 2.4 has header definitions for NFS V.3, but does not really
 * have NFS V.3 support in the kernel.  So I must undefine these.
 */
#ifdef MNTTYPE_NFS3
# undef MNTTYPE_NFS3
#endif /* MNTTYPE_NFS3 */


/*
 * MACROS
 */

#define	NFS_PORT 2049
#define	NFS_MAXDATA 8192
#define	NFS_MAXPATHLEN 1024
#define	NFS_MAXNAMLEN 255
#define	NFS_FHSIZE 32
#define	NFS_COOKIESIZE 4

#define	NFSMODE_FMT 0170000
#define	NFSMODE_DIR 0040000
#define	NFSMODE_CHR 0020000
#define	NFSMODE_BLK 0060000
#define	NFSMODE_REG 0100000
#define	NFSMODE_LNK 0120000
#define	NFSMODE_SOCK 0140000
#define	NFSMODE_FIFO 0010000

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
#define ex_dir		ex_name
#define gr_name		g_name
#define gr_next		g_next
#define ml_directory	ml_path
#define ml_hostname	ml_name
#define ml_next		ml_nxt


/*
 * TYPEDEFS:
 */

typedef char *dirpath;
typedef char *name;
typedef enum ftype nfsftype;
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

extern bool_t xdr_createargs(XDR *, nfscreateargs *);
extern bool_t xdr_dirlist(XDR *, nfsdirlist *);
extern bool_t xdr_diropokres(XDR *, nfsdiropokres *);
extern bool_t xdr_entry(XDR *, nfsentry *);
extern bool_t xdr_filename(XDR *, filename *);
extern bool_t xdr_ftype(XDR *, nfsftype *);
extern bool_t xdr_nfs_fh(XDR *, nfs_fh *);
extern bool_t xdr_nfscookie(XDR *, nfscookie);
extern bool_t xdr_nfspath(XDR *, nfspath *);
extern bool_t xdr_nfsstat(XDR *, nfsstat *);
extern bool_t xdr_nfstime(XDR *, nfstime *);
extern bool_t xdr_readdirargs(XDR *, nfsreaddirargs *);
extern bool_t xdr_readdirres(XDR *, nfsreaddirres *);
extern bool_t xdr_readlinkres(XDR *, nfsreadlinkres *);
extern bool_t xdr_readokres(XDR *, nfsreadokres *);
extern bool_t xdr_readres(XDR *, nfsreadres *);
extern bool_t xdr_renameargs(XDR *, nfsrenameargs *);
extern bool_t xdr_sattrargs(XDR *, nfssattrargs *);
extern bool_t xdr_statfsokres(XDR *, nfsstatfsokres *);
extern bool_t xdr_statfsres(XDR *, nfsstatfsres *);
extern bool_t xdr_symlinkargs(XDR *, nfssymlinkargs *);


/*
 * ENUMS:
 */


/*
 * STRUCTURES:
 */

#endif /* not _AMU_NFS_PROT_H */
