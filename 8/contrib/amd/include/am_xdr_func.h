/*
 * Copyright (c) 1997-2006 Erez Zadok
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgment:
 *      This product includes software developed by the University of
 *      California, Berkeley and its contributors.
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
 *
 * File: am-utils/include/am_xdr_func.h
 *
 */

#ifdef HAVE_FS_NFS3

#define AM_FHSIZE3 64		/* size in bytes of a file handle (v3) */
#define	AM_MOUNTVERS3 ((unsigned long)(3))

/* NFSv3 handle */
struct am_nfs_fh3 {
  u_int am_fh3_length;
  char am_fh3_data[AM_FHSIZE3];
};
typedef struct am_nfs_fh3 am_nfs_fh3;

#define AM_NFSPROC3_LOOKUP ((u_long) 3)
enum am_nfsstat3 {
	AM_NFS3_OK = 0,
	AM_NFS3ERR_PERM = 1,
	AM_NFS3ERR_NOENT = 2,
	AM_NFS3ERR_IO = 5,
	AM_NFS3ERR_NXIO = 6,
	AM_NFS3ERR_ACCES = 13,
	AM_NFS3ERR_EXIST = 17,
	AM_NFS3ERR_XDEV = 18,
	AM_NFS3ERR_NODEV = 19,
	AM_NFS3ERR_NOTDIR = 20,
	AM_NFS3ERR_ISDIR = 21,
	AM_NFS3ERR_INVAL = 22,
	AM_NFS3ERR_FBIG = 27,
	AM_NFS3ERR_NOSPC = 28,
	AM_NFS3ERR_ROFS = 30,
	AM_NFS3ERR_MLINK = 31,
	AM_NFS3ERR_NAMETOOLONG = 63,
	AM_NFS3ERR_NOTEMPTY = 66,
	AM_NFS3ERR_DQUOT = 69,
	AM_NFS3ERR_STALE = 70,
	AM_NFS3ERR_REMOTE = 71,
	AM_NFS3ERR_BADHANDLE = 10001,
	AM_NFS3ERR_NOT_SYNC = 10002,
	AM_NFS3ERR_BAD_COOKIE = 10003,
	AM_NFS3ERR_NOTSUPP = 10004,
	AM_NFS3ERR_TOOSMALL = 10005,
	AM_NFS3ERR_SERVERFAULT = 10006,
	AM_NFS3ERR_BADTYPE = 10007,
	AM_NFS3ERR_JUKEBOX = 10008
};
typedef enum am_nfsstat3 am_nfsstat3;

typedef struct {
  u_int fhandle3_len;
  char *fhandle3_val;
} am_fhandle3;

enum am_mountstat3 {
       AM_MNT3_OK = 0,
       AM_MNT3ERR_PERM = 1,
       AM_MNT3ERR_NOENT = 2,
       AM_MNT3ERR_IO = 5,
       AM_MNT3ERR_ACCES = 13,
       AM_MNT3ERR_NOTDIR = 20,
       AM_MNT3ERR_INVAL = 22,
       AM_MNT3ERR_NAMETOOLONG = 63,
       AM_MNT3ERR_NOTSUPP = 10004,
       AM_MNT3ERR_SERVERFAULT = 10006
};
typedef enum am_mountstat3 am_mountstat3;

struct am_mountres3_ok {
       am_fhandle3 fhandle;
       struct {
               u_int auth_flavors_len;
               int *auth_flavors_val;
       } auth_flavors;
};
typedef struct am_mountres3_ok am_mountres3_ok;

struct am_mountres3 {
       am_mountstat3 fhs_status;
       union {
               am_mountres3_ok mountinfo;
       } mountres3_u;
};
typedef struct am_mountres3 am_mountres3;

typedef char *am_filename3;

struct am_diropargs3 {
	am_nfs_fh3 dir;
	am_filename3 name;
};
typedef struct am_diropargs3 am_diropargs3;

struct am_LOOKUP3args {
	am_diropargs3 what;
};
typedef struct am_LOOKUP3args am_LOOKUP3args;

struct am_LOOKUP3resok {
	am_nfs_fh3 object;
#if 0
	post_op_attr obj_attributes;
	post_op_attr dir_attributes;
#endif
};
typedef struct am_LOOKUP3resok am_LOOKUP3resok;

struct am_LOOKUP3resfail {
#if 0
	post_op_attr dir_attributes;
#else
	char dummy;		/* cannot have an empty declaration */
#endif
};
typedef struct am_LOOKUP3resfail am_LOOKUP3resfail;

struct am_LOOKUP3res {
	am_nfsstat3 status;
	union {
		am_LOOKUP3resok ok;
		am_LOOKUP3resfail fail;
	} res_u;
};
typedef struct am_LOOKUP3res am_LOOKUP3res;
#endif /* HAVE_FS_NFS3 */

/*
 * Multi-protocol NFS file handle
 */
union am_nfs_handle {
				/* placeholder for V4 file handle */
#ifdef HAVE_FS_NFS3
  am_nfs_fh3		v3;	/* NFS version 3 handle */
#endif /* HAVE_FS_NFS3 */
  am_nfs_fh		v2;	/* NFS version 2 handle */
};
typedef union am_nfs_handle am_nfs_handle_t;


/*
 * Definitions of all possible xdr functions that are otherwise
 * not defined elsewhere.
 */

#ifndef _AM_XDR_FUNC_H
#define _AM_XDR_FUNC_H

#ifndef HAVE_XDR_ATTRSTAT
bool_t xdr_attrstat(XDR *xdrs, nfsattrstat *objp);
#endif /* not HAVE_XDR_ATTRSTAT */
#ifndef HAVE_XDR_CREATEARGS
bool_t xdr_createargs(XDR *xdrs, nfscreateargs *objp);
#endif /* not HAVE_XDR_CREATEARGS */
#ifndef HAVE_XDR_DIRLIST
bool_t xdr_dirlist(XDR *xdrs, nfsdirlist *objp);
#endif /* not HAVE_XDR_DIRLIST */
#ifndef HAVE_XDR_DIROPARGS
bool_t xdr_diropargs(XDR *xdrs, nfsdiropargs *objp);
#endif /* not HAVE_XDR_DIROPARGS */
#ifndef HAVE_XDR_DIROPOKRES
bool_t xdr_diropokres(XDR *xdrs, nfsdiropokres *objp);
#endif /* not HAVE_XDR_DIROPOKRES */
#ifndef HAVE_XDR_DIROPRES
bool_t xdr_diropres(XDR *xdrs, nfsdiropres *objp);
#endif /* not HAVE_XDR_DIROPRES */
#ifndef HAVE_XDR_DIRPATH
bool_t xdr_dirpath(XDR *xdrs, dirpath *objp);
#endif /* not HAVE_XDR_DIRPATH */
#ifndef HAVE_XDR_ENTRY
bool_t xdr_entry(XDR *xdrs, nfsentry *objp);
#endif /* not HAVE_XDR_ENTRY */
#ifndef HAVE_XDR_EXPORTNODE
bool_t xdr_exportnode(XDR *xdrs, exportnode *objp);
#endif /* not HAVE_XDR_EXPORTNODE */
#ifndef HAVE_XDR_EXPORTS
bool_t xdr_exports(XDR *xdrs, exports *objp);
#endif /* not HAVE_XDR_EXPORTS */
#ifndef HAVE_XDR_FATTR
bool_t xdr_fattr(XDR *xdrs, nfsfattr *objp);
#endif /* not HAVE_XDR_FATTR */
#ifndef HAVE_XDR_FHANDLE
bool_t xdr_fhandle(XDR *xdrs, fhandle objp);
#endif /* not HAVE_XDR_FHANDLE */
#ifndef HAVE_XDR_FHSTATUS
bool_t xdr_fhstatus(XDR *xdrs, fhstatus *objp);
#endif /* not HAVE_XDR_FHSTATUS */
#ifndef HAVE_XDR_FILENAME
bool_t xdr_filename(XDR *xdrs, filename *objp);
#endif /* not HAVE_XDR_FILENAME */
#ifndef HAVE_XDR_FTYPE
bool_t xdr_ftype(XDR *xdrs, nfsftype *objp);
#endif /* not HAVE_XDR_FTYPE */
#ifndef HAVE_XDR_GROUPNODE
bool_t xdr_groupnode(XDR *xdrs, groupnode *objp);
#endif /* not HAVE_XDR_GROUPNODE */
#ifndef HAVE_XDR_GROUPS
bool_t xdr_groups(XDR *xdrs, groups *objp);
#endif /* not HAVE_XDR_GROUPS */
#ifndef HAVE_XDR_LINKARGS
bool_t xdr_linkargs(XDR *xdrs, nfslinkargs *objp);
#endif /* not HAVE_XDR_LINKARGS */
#ifndef HAVE_XDR_MOUNTBODY
bool_t xdr_mountbody(XDR *xdrs, mountbody *objp);
#endif /* not HAVE_XDR_MOUNTBODY */
#ifndef HAVE_XDR_MOUNTLIST
bool_t xdr_mountlist(XDR *xdrs, mountlist *objp);
#endif /* not HAVE_XDR_MOUNTLIST */
#ifndef HAVE_XDR_NAME
bool_t xdr_name(XDR *xdrs, name *objp);
#endif /* not HAVE_XDR_NAME */
#ifndef HAVE_XDR_NFS_FH
bool_t xdr_nfs_fh(XDR *xdrs, am_nfs_fh *objp);
#endif /* not HAVE_XDR_NFS_FH */
#ifndef HAVE_XDR_NFSCOOKIE
bool_t xdr_nfscookie(XDR *xdrs, nfscookie objp);
#endif /* not HAVE_XDR_NFSCOOKIE */
#ifndef HAVE_XDR_NFSPATH
bool_t xdr_nfspath(XDR *xdrs, nfspath *objp);
#endif /* not HAVE_XDR_NFSPATH */
#ifndef HAVE_XDR_NFSSTAT
bool_t xdr_nfsstat(XDR *xdrs, nfsstat *objp);
#endif /* not HAVE_XDR_NFSSTAT */
#ifndef HAVE_XDR_NFSTIME
bool_t xdr_nfstime(XDR *xdrs, nfstime *objp);
#endif /* not HAVE_XDR_NFSTIME */
#ifndef HAVE_XDR_POINTER
bool_t xdr_pointer(register XDR *xdrs, char **objpp, u_int obj_size, XDRPROC_T_TYPE xdr_obj);
#endif /* not HAVE_XDR_POINTER */
#ifndef HAVE_XDR_READARGS
bool_t xdr_readargs(XDR *xdrs, nfsreadargs *objp);
#endif /* not HAVE_XDR_READARGS */
#ifndef HAVE_XDR_READDIRARGS
bool_t xdr_readdirargs(XDR *xdrs, nfsreaddirargs *objp);
#endif /* not HAVE_XDR_READDIRARGS */
#ifndef HAVE_XDR_READDIRRES
bool_t xdr_readdirres(XDR *xdrs, nfsreaddirres *objp);
#endif /* not HAVE_XDR_READDIRRES */
#ifndef HAVE_XDR_READLINKRES
bool_t xdr_readlinkres(XDR *xdrs, nfsreadlinkres *objp);
#endif /* not HAVE_XDR_READLINKRES */
#ifndef HAVE_XDR_READOKRES
bool_t xdr_readokres(XDR *xdrs, nfsreadokres *objp);
#endif /* not HAVE_XDR_READOKRES */
#ifndef HAVE_XDR_READRES
bool_t xdr_readres(XDR *xdrs, nfsreadres *objp);
#endif /* not HAVE_XDR_READRES */
#ifndef HAVE_XDR_RENAMEARGS
bool_t xdr_renameargs(XDR *xdrs, nfsrenameargs *objp);
#endif /* not HAVE_XDR_RENAMEARGS */
#ifndef HAVE_XDR_SATTR
bool_t xdr_sattr(XDR *xdrs, nfssattr *objp);
#endif /* not HAVE_XDR_SATTR */
#ifndef HAVE_XDR_SATTRARGS
bool_t xdr_sattrargs(XDR *xdrs, nfssattrargs *objp);
#endif /* not HAVE_XDR_SATTRARGS */
#ifndef HAVE_XDR_STATFSOKRES
bool_t xdr_statfsokres(XDR *xdrs, nfsstatfsokres *objp);
#endif /* not HAVE_XDR_STATFSOKRES */
#ifndef HAVE_XDR_STATFSRES
bool_t xdr_statfsres(XDR *xdrs, nfsstatfsres *objp);
#endif /* not HAVE_XDR_STATFSRES */
#ifndef HAVE_XDR_SYMLINKARGS
bool_t xdr_symlinkargs(XDR *xdrs, nfssymlinkargs *objp);
#endif /* not HAVE_XDR_SYMLINKARGS */
#ifndef HAVE_XDR_WRITEARGS
bool_t xdr_writeargs(XDR *xdrs, nfswriteargs *objp);
#endif /* not HAVE_XDR_WRITEARGS */

/*
 * NFS3 XDR FUNCTIONS:
 */
#ifdef HAVE_FS_NFS3
bool_t xdr_am_fhandle3(XDR *xdrs, am_fhandle3 *objp);
bool_t xdr_am_mountstat3(XDR *xdrs, am_mountstat3 *objp);
bool_t xdr_am_mountres3_ok(XDR *xdrs, am_mountres3_ok *objp);
bool_t xdr_am_mountres3(XDR *xdrs, am_mountres3 *objp);
bool_t xdr_am_diropargs3(XDR *xdrs, am_diropargs3 *objp);
bool_t xdr_am_filename3(XDR *xdrs, am_filename3 *objp);
bool_t xdr_am_LOOKUP3args(XDR *xdrs, am_LOOKUP3args *objp);
bool_t xdr_am_LOOKUP3res(XDR *xdrs, am_LOOKUP3res *objp);
bool_t xdr_am_LOOKUP3resfail(XDR *xdrs, am_LOOKUP3resfail *objp);
bool_t xdr_am_LOOKUP3resok(XDR *xdrs, am_LOOKUP3resok *objp);
bool_t xdr_am_nfsstat3(XDR *xdrs, am_nfsstat3 *objp);
bool_t xdr_am_nfs_fh3(XDR *xdrs, am_nfs_fh3 *objp);
#endif /* HAVE_FS_NFS3 */

#endif /* not _AM_XDR_FUNC_H */
