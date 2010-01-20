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
 * File: am-utils/libamu/xdr_func.c
 *
 */

/*
 * Complete list of all possible xdr functions which may be needed.
 */
#ifdef HAVE_CONFIG_H
# include <config.h>
#endif /* HAVE_CONFIG_H */
#include <am_defs.h>
#include <amu.h>

#ifdef __RPCSVC_MOUNT_H__
# error IRIX6 should not include rpcsvc/mount.h
#endif /* __RPCSVC_MOUNT_H__ */

/*
 * MACROS:
 */
#ifdef HAVE_FS_AUTOFS
# ifndef AUTOFS_MAXCOMPONENTLEN
#  define AUTOFS_MAXCOMPONENTLEN 255
# endif /* not AUTOFS_MAXCOMPONENTLEN */
# ifndef AUTOFS_MAXOPTSLEN
#  define AUTOFS_MAXOPTSLEN 255
# endif /* not AUTOFS_MAXOPTSLEN */
# ifndef AUTOFS_MAXPATHLEN
#  define AUTOFS_MAXPATHLEN 1024
# endif /* not AUTOFS_MAXPATHLEN */
#endif /* HAVE_FS_AUTOFS */

/* forward definitions, are they needed? */
extern bool_t xdr_exportnode(XDR *xdrs, exportnode *objp);
extern bool_t xdr_groupnode(XDR *xdrs, groupnode *objp);
extern bool_t xdr_name(XDR *xdrs, name *objp);


#ifndef HAVE_XDR_ATTRSTAT
bool_t
xdr_attrstat(XDR *xdrs, nfsattrstat *objp)
{
  if (amuDebug(D_XDRTRACE))
    plog(XLOG_DEBUG, "xdr_attrstat:");

  if (!xdr_nfsstat(xdrs, &objp->ns_status)) {
    return (FALSE);
  }
  switch (objp->ns_status) {
  case NFS_OK:
    if (!xdr_fattr(xdrs, &objp->ns_u.ns_attr_u)) {
      return (FALSE);
    }
    break;
  default:
    break;
  }
  return (TRUE);
}
#endif /* not HAVE_XDR_ATTRSTAT */


#ifndef HAVE_XDR_CREATEARGS
bool_t
xdr_createargs(XDR *xdrs, nfscreateargs *objp)
{
  if (amuDebug(D_XDRTRACE))
    plog(XLOG_DEBUG, "xdr_createargs:");

  if (!xdr_diropargs(xdrs, &objp->ca_where)) {
    return (FALSE);
  }
  if (!xdr_sattr(xdrs, &objp->ca_attributes)) {
    return (FALSE);
  }
  return (TRUE);
}
#endif /* not HAVE_XDR_CREATEARGS */


#ifndef HAVE_XDR_DIRLIST
bool_t
xdr_dirlist(XDR *xdrs, nfsdirlist *objp)
{
  if (amuDebug(D_XDRTRACE))
    plog(XLOG_DEBUG, "xdr_dirlist:");

  if (!xdr_pointer(xdrs, (char **) &objp->dl_entries, sizeof(nfsentry), (XDRPROC_T_TYPE) xdr_entry)) {
    return (FALSE);
  }
  if (!xdr_bool(xdrs, &objp->dl_eof)) {
    return (FALSE);
  }
  return (TRUE);
}
#endif /* not HAVE_XDR_DIRLIST */


#ifndef HAVE_XDR_DIROPARGS
bool_t
xdr_diropargs(XDR *xdrs, nfsdiropargs *objp)
{
  if (amuDebug(D_XDRTRACE))
    plog(XLOG_DEBUG, "xdr_diropargs:");

  if (!xdr_nfs_fh(xdrs, &objp->da_fhandle)) {
    return (FALSE);
  }
  if (!xdr_filename(xdrs, &objp->da_name)) {
    return (FALSE);
  }
  return (TRUE);
}
#endif /* not HAVE_XDR_DIROPARGS */


#ifndef HAVE_XDR_DIROPOKRES
bool_t
xdr_diropokres(XDR *xdrs, nfsdiropokres *objp)
{
  if (amuDebug(D_XDRTRACE))
    plog(XLOG_DEBUG, "xdr_diropokres:");

  if (!xdr_nfs_fh(xdrs, &objp->drok_fhandle)) {
    return (FALSE);
  }
  if (!xdr_fattr(xdrs, &objp->drok_attributes)) {
    return (FALSE);
  }
  return (TRUE);
}
#endif /* not HAVE_XDR_DIROPOKRES */


#ifndef HAVE_XDR_DIROPRES
bool_t
xdr_diropres(XDR *xdrs, nfsdiropres *objp)
{
  if (amuDebug(D_XDRTRACE))
    plog(XLOG_DEBUG, "xdr_diropres:");

  if (!xdr_nfsstat(xdrs, &objp->dr_status)) {
    return (FALSE);
  }
  switch (objp->dr_status) {
  case NFS_OK:
    if (!xdr_diropokres(xdrs, &objp->dr_u.dr_drok_u)) {
      return (FALSE);
    }
    break;
  default:
    break;
  }
  return (TRUE);
}
#endif /* not HAVE_XDR_DIROPRES */


#ifndef HAVE_XDR_DIRPATH
bool_t
xdr_dirpath(XDR *xdrs, dirpath *objp)
{
  if (amuDebug(D_XDRTRACE))
    plog(XLOG_DEBUG, "xdr_dirpath:");

  if (!xdr_string(xdrs, objp, MNTPATHLEN)) {
    return (FALSE);
  }
  return (TRUE);
}
#endif /* not HAVE_XDR_DIRPATH */


#ifndef HAVE_XDR_ENTRY
bool_t
xdr_entry(XDR *xdrs, nfsentry *objp)
{
  if (amuDebug(D_XDRTRACE))
    plog(XLOG_DEBUG, "xdr_entry:");

  if (!xdr_u_int(xdrs, &objp->ne_fileid)) {
    return (FALSE);
  }
  if (!xdr_filename(xdrs, &objp->ne_name)) {
    return (FALSE);
  }
  if (!xdr_nfscookie(xdrs, objp->ne_cookie)) {
    return (FALSE);
  }
  if (!xdr_pointer(xdrs, (char **) &objp->ne_nextentry, sizeof(nfsentry), (XDRPROC_T_TYPE) xdr_entry)) {
    return (FALSE);
  }
  return (TRUE);
}
#endif /* not HAVE_XDR_ENTRY */


#ifndef HAVE_XDR_EXPORTNODE
bool_t
xdr_exportnode(XDR *xdrs, exportnode *objp)
{
  if (amuDebug(D_XDRTRACE))
    plog(XLOG_DEBUG, "xdr_exportnode:");

  if (!xdr_dirpath(xdrs, &objp->ex_dir)) {
    return (FALSE);
  }
  if (!xdr_groups(xdrs, &objp->ex_groups)) {
    return (FALSE);
  }
  if (!xdr_exports(xdrs, &objp->ex_next)) {
    return (FALSE);
  }
  return (TRUE);
}
#endif /* not HAVE_XDR_EXPORTNODE */


#ifndef HAVE_XDR_EXPORTS
bool_t
xdr_exports(XDR *xdrs, exports *objp)
{
  if (amuDebug(D_XDRTRACE))
    plog(XLOG_DEBUG, "xdr_exports:");

  if (!xdr_pointer(xdrs, (char **) objp, sizeof(exportnode), (XDRPROC_T_TYPE) xdr_exportnode)) {
    return (FALSE);
  }
  return (TRUE);
}
#endif /* not HAVE_XDR_EXPORTS */


#ifndef HAVE_XDR_FATTR
bool_t
xdr_fattr(XDR *xdrs, nfsfattr *objp)
{
  if (amuDebug(D_XDRTRACE))
    plog(XLOG_DEBUG, "xdr_fattr:");

  if (!xdr_ftype(xdrs, &objp->na_type)) {
    return (FALSE);
  }
  if (!xdr_u_int(xdrs, &objp->na_mode)) {
    return (FALSE);
  }
  if (!xdr_u_int(xdrs, &objp->na_nlink)) {
    return (FALSE);
  }
  if (!xdr_u_int(xdrs, &objp->na_uid)) {
    return (FALSE);
  }
  if (!xdr_u_int(xdrs, &objp->na_gid)) {
    return (FALSE);
  }
  if (!xdr_u_int(xdrs, &objp->na_size)) {
    return (FALSE);
  }
  if (!xdr_u_int(xdrs, &objp->na_blocksize)) {
    return (FALSE);
  }
  if (!xdr_u_int(xdrs, &objp->na_rdev)) {
    return (FALSE);
  }
  if (!xdr_u_int(xdrs, &objp->na_blocks)) {
    return (FALSE);
  }
  if (!xdr_u_int(xdrs, &objp->na_fsid)) {
    return (FALSE);
  }
  if (!xdr_u_int(xdrs, &objp->na_fileid)) {
    return (FALSE);
  }
  if (!xdr_nfstime(xdrs, &objp->na_atime)) {
    return (FALSE);
  }
  if (!xdr_nfstime(xdrs, &objp->na_mtime)) {
    return (FALSE);
  }
  if (!xdr_nfstime(xdrs, &objp->na_ctime)) {
    return (FALSE);
  }
  return (TRUE);
}
#endif /* not HAVE_XDR_FATTR */


#ifndef HAVE_XDR_FHANDLE
bool_t
xdr_fhandle(XDR *xdrs, fhandle objp)
{
  if (amuDebug(D_XDRTRACE))
    plog(XLOG_DEBUG, "xdr_fhandle:");

  if (!xdr_opaque(xdrs, objp, NFS_FHSIZE)) {
    return (FALSE);
  }
  return (TRUE);
}
#endif /* not HAVE_XDR_FHANDLE */


#ifndef HAVE_XDR_FHSTATUS
bool_t
xdr_fhstatus(XDR *xdrs, fhstatus *objp)
{
  if (amuDebug(D_XDRTRACE))
    plog(XLOG_DEBUG, "xdr_fhstatus:");

  if (!xdr_u_int(xdrs, &objp->fhs_status)) {
    return (FALSE);
  }
  if (objp->fhs_status == 0 && !xdr_fhandle(xdrs, objp->fhs_fh)) {
    return (FALSE);
  }
  return (TRUE);
}
#endif /* not HAVE_XDR_FHSTATUS */


#ifndef HAVE_XDR_FILENAME
bool_t
xdr_filename(XDR *xdrs, filename *objp)
{
  if (amuDebug(D_XDRTRACE))
    plog(XLOG_DEBUG, "xdr_filename:");

  if (!xdr_string(xdrs, objp, NFS_MAXNAMLEN)) {
    return (FALSE);
  }
  return (TRUE);
}
#endif /* not HAVE_XDR_FILENAME */


#ifndef HAVE_XDR_FTYPE
bool_t
xdr_ftype(XDR *xdrs, nfsftype *objp)
{
  enum_t local_obj = *objp;

  if (amuDebug(D_XDRTRACE))
    plog(XLOG_DEBUG, "xdr_ftype:");

  if (!xdr_enum(xdrs, &local_obj)) {
    return (FALSE);
  }
  return (TRUE);
}
#endif /* not HAVE_XDR_FTYPE */


#ifndef HAVE_XDR_GROUPNODE
bool_t
xdr_groupnode(XDR *xdrs, groupnode *objp)
{
  if (amuDebug(D_XDRTRACE))
    plog(XLOG_DEBUG, "xdr_groupnode:");

  if (!xdr_name(xdrs, &objp->gr_name)) {
    return (FALSE);
  }
  if (!xdr_groups(xdrs, &objp->gr_next)) {
    return (FALSE);
  }
  return (TRUE);
}
#endif /* not HAVE_XDR_GROUPNODE */


#ifndef HAVE_XDR_GROUPS
bool_t
xdr_groups(XDR *xdrs, groups *objp)
{
  if (amuDebug(D_XDRTRACE))
    plog(XLOG_DEBUG, "xdr_groups:");

  if (!xdr_pointer(xdrs, (char **) objp, sizeof(groupnode), (XDRPROC_T_TYPE) xdr_groupnode)) {
    return (FALSE);
  }
  return (TRUE);
}
#endif /* not HAVE_XDR_GROUPS */


#ifndef HAVE_XDR_LINKARGS
bool_t
xdr_linkargs(XDR *xdrs, nfslinkargs *objp)
{
  if (amuDebug(D_XDRTRACE))
    plog(XLOG_DEBUG, "xdr_linkargs:");

  if (!xdr_nfs_fh(xdrs, &objp->la_fhandle)) {
    return (FALSE);
  }
  if (!xdr_diropargs(xdrs, &objp->la_to)) {
    return (FALSE);
  }
  return (TRUE);
}
#endif /* not HAVE_XDR_LINKARGS */


#ifndef HAVE_XDR_MOUNTBODY
bool_t
xdr_mountbody(XDR *xdrs, mountbody *objp)
{
  if (amuDebug(D_XDRTRACE))
    plog(XLOG_DEBUG, "xdr_mountbody:");

  if (!xdr_name(xdrs, &objp->ml_hostname)) {
    return (FALSE);
  }
  if (!xdr_dirpath(xdrs, &objp->ml_directory)) {
    return (FALSE);
  }
  if (!xdr_mountlist(xdrs, &objp->ml_next)) {
    return (FALSE);
  }
  return (TRUE);
}
#endif /* not HAVE_XDR_MOUNTBODY */


#ifndef HAVE_XDR_MOUNTLIST
bool_t
xdr_mountlist(XDR *xdrs, mountlist *objp)
{
  if (amuDebug(D_XDRTRACE))
    plog(XLOG_DEBUG, "xdr_mountlist:");

  if (!xdr_pointer(xdrs, (char **) objp, sizeof(mountbody), (XDRPROC_T_TYPE) xdr_mountbody)) {
    return (FALSE);
  }
  return (TRUE);
}
#endif /* not HAVE_XDR_MOUNTLIST */


#ifndef HAVE_XDR_NAME
bool_t
xdr_name(XDR *xdrs, name *objp)
{
  if (amuDebug(D_XDRTRACE))
    plog(XLOG_DEBUG, "xdr_name:");

  if (!xdr_string(xdrs, objp, MNTNAMLEN)) {
    return (FALSE);
  }
  return (TRUE);
}
#endif /* not HAVE_XDR_NAME */


#ifndef HAVE_XDR_NFS_FH
bool_t
xdr_nfs_fh(XDR *xdrs, am_nfs_fh *objp)
{
  if (amuDebug(D_XDRTRACE))
    plog(XLOG_DEBUG, "xdr_nfs_fh:");

  if (!xdr_opaque(xdrs, (caddr_t) objp->fh_data, NFS_FHSIZE)) {
    return (FALSE);
  }
  return (TRUE);
}
#endif /* not HAVE_XDR_NFS_FH */


#ifndef HAVE_XDR_NFSCOOKIE
bool_t
xdr_nfscookie(XDR *xdrs, nfscookie objp)
{
  if (amuDebug(D_XDRTRACE))
    plog(XLOG_DEBUG, "xdr_nfscookie:");

  if (!xdr_opaque(xdrs, objp, NFS_COOKIESIZE)) {
    return (FALSE);
  }
  return (TRUE);
}
#endif /* not HAVE_XDR_NFSCOOKIE */


#ifndef HAVE_XDR_NFSPATH
bool_t
xdr_nfspath(XDR *xdrs, nfspath *objp)
{
  if (amuDebug(D_XDRTRACE))
    plog(XLOG_DEBUG, "xdr_nfspath:");

  if (!xdr_string(xdrs, objp, NFS_MAXPATHLEN)) {
    return (FALSE);
  }
  return (TRUE);
}
#endif /* not HAVE_XDR_NFSPATH */


#ifndef HAVE_XDR_NFSSTAT
bool_t
xdr_nfsstat(XDR *xdrs, nfsstat *objp)
{
  enum_t local_obj = *objp;

  if (amuDebug(D_XDRTRACE))
    plog(XLOG_DEBUG, "xdr_nfsstat:");

  if (!xdr_enum(xdrs, &local_obj)) {
    return (FALSE);
  }
  return (TRUE);
}
#endif /* not HAVE_XDR_NFSSTAT */


#ifndef HAVE_XDR_NFSTIME
bool_t
xdr_nfstime(XDR *xdrs, nfstime *objp)
{
  if (amuDebug(D_XDRTRACE))
    plog(XLOG_DEBUG, "xdr_nfstime:");

  if (!xdr_u_int(xdrs, (u_int *) &objp->nt_seconds)) {
    return (FALSE);
  }
  if (!xdr_u_int(xdrs, (u_int *) &objp->nt_useconds)) {
    return (FALSE);
  }
  return (TRUE);
}
#endif /* not HAVE_XDR_NFSTIME */


#ifndef HAVE_XDR_POINTER
bool_t
xdr_pointer(register XDR *xdrs, char **objpp, u_int obj_size, XDRPROC_T_TYPE xdr_obj)
{
  if (amuDebug(D_XDRTRACE))
    plog(XLOG_DEBUG, "xdr_pointer:");

  bool_t more_data;

  more_data = (*objpp != NULL);
  if (!xdr_bool(xdrs, &more_data)) {
    return (FALSE);
  }
  if (!more_data) {
    *objpp = NULL;
    return (TRUE);
  }

  return (xdr_reference(xdrs, objpp, obj_size, xdr_obj));
}
#endif /* not HAVE_XDR_POINTER */


#ifndef HAVE_XDR_READARGS
bool_t
xdr_readargs(XDR *xdrs, nfsreadargs *objp)
{
  if (amuDebug(D_XDRTRACE))
    plog(XLOG_DEBUG, "xdr_readargs:");

  if (!xdr_nfs_fh(xdrs, &objp->ra_fhandle)) {
    return (FALSE);
  }
  if (!xdr_u_int(xdrs, &objp->ra_offset)) {
    return (FALSE);
  }
  if (!xdr_u_int(xdrs, &objp->ra_count)) {
    return (FALSE);
  }
  if (!xdr_u_int(xdrs, &objp->ra_totalcount)) {
    return (FALSE);
  }
  return (TRUE);
}
#endif /* not HAVE_XDR_READARGS */


#ifndef HAVE_XDR_READDIRARGS
bool_t
xdr_readdirargs(XDR *xdrs, nfsreaddirargs *objp)
{
  if (amuDebug(D_XDRTRACE))
    plog(XLOG_DEBUG, "xdr_readdirargs:");

  if (!xdr_nfs_fh(xdrs, &objp->rda_fhandle)) {
    return (FALSE);
  }
  if (!xdr_nfscookie(xdrs, objp->rda_cookie)) {
    return (FALSE);
  }
  if (!xdr_u_int(xdrs, &objp->rda_count)) {
    return (FALSE);
  }
  return (TRUE);
}
#endif /* not HAVE_XDR_READDIRARGS */


#ifndef HAVE_XDR_READDIRRES
bool_t
xdr_readdirres(XDR *xdrs, nfsreaddirres *objp)
{
  if (amuDebug(D_XDRTRACE))
    plog(XLOG_DEBUG, "xdr_readdirres:");

  if (!xdr_nfsstat(xdrs, &objp->rdr_status)) {
    return (FALSE);
  }
  switch (objp->rdr_status) {
  case NFS_OK:
    if (!xdr_dirlist(xdrs, &objp->rdr_u.rdr_reply_u)) {
      return (FALSE);
    }
    break;
  default:
    break;
  }
  return (TRUE);
}
#endif /* not HAVE_XDR_READDIRRES */


#ifndef HAVE_XDR_READLINKRES
bool_t
xdr_readlinkres(XDR *xdrs, nfsreadlinkres *objp)
{
  if (amuDebug(D_XDRTRACE))
    plog(XLOG_DEBUG, "xdr_readlinkres:");

  if (!xdr_nfsstat(xdrs, &objp->rlr_status)) {
    return (FALSE);
  }
  switch (objp->rlr_status) {
  case NFS_OK:
    if (!xdr_nfspath(xdrs, &objp->rlr_u.rlr_data_u)) {
      return (FALSE);
    }
    break;
  default:
    break;
  }
  return (TRUE);
}
#endif /* not HAVE_XDR_READLINKRES */


#ifndef HAVE_XDR_READOKRES
bool_t
xdr_readokres(XDR *xdrs, nfsreadokres *objp)
{
  if (amuDebug(D_XDRTRACE))
    plog(XLOG_DEBUG, "xdr_readokres:");

  if (!xdr_fattr(xdrs, &objp->raok_attributes)) {
    return (FALSE);
  }
  if (!xdr_bytes(xdrs,
		 (char **) & objp->raok_u.raok_val_u,
		 (u_int *) & objp->raok_u.raok_len_u,
		 NFS_MAXDATA)) {
    return (FALSE);
  }
  return (TRUE);
}
#endif /* not HAVE_XDR_READOKRES */


#ifndef HAVE_XDR_READRES
bool_t
xdr_readres(XDR *xdrs, nfsreadres *objp)
{
  if (amuDebug(D_XDRTRACE))
    plog(XLOG_DEBUG, "xdr_readres:");

  if (!xdr_nfsstat(xdrs, &objp->rr_status)) {
    return (FALSE);
  }
  switch (objp->rr_status) {
  case NFS_OK:
    if (!xdr_readokres(xdrs, &objp->rr_u.rr_reply_u)) {
      return (FALSE);
    }
    break;
  default:
    break;
  }
  return (TRUE);
}
#endif /* not HAVE_XDR_READRES */


#ifndef HAVE_XDR_RENAMEARGS
bool_t
xdr_renameargs(XDR *xdrs, nfsrenameargs *objp)
{
  if (amuDebug(D_XDRTRACE))
    plog(XLOG_DEBUG, "xdr_renameargs:");

  if (!xdr_diropargs(xdrs, &objp->rna_from)) {
    return (FALSE);
  }
  if (!xdr_diropargs(xdrs, &objp->rna_to)) {
    return (FALSE);
  }
  return (TRUE);
}
#endif /* not HAVE_XDR_RENAMEARGS */


#ifndef HAVE_XDR_SATTR
bool_t
xdr_sattr(XDR *xdrs, nfssattr *objp)
{
  if (amuDebug(D_XDRTRACE))
    plog(XLOG_DEBUG, "xdr_sattr:");

  if (!xdr_u_int(xdrs, &objp->sa_mode)) {
    return (FALSE);
  }
  if (!xdr_u_int(xdrs, &objp->sa_uid)) {
    return (FALSE);
  }
  if (!xdr_u_int(xdrs, &objp->sa_gid)) {
    return (FALSE);
  }
  if (!xdr_u_int(xdrs, &objp->sa_size)) {
    return (FALSE);
  }
  if (!xdr_nfstime(xdrs, &objp->sa_atime)) {
    return (FALSE);
  }
  if (!xdr_nfstime(xdrs, &objp->sa_mtime)) {
    return (FALSE);
  }
  return (TRUE);
}
#endif /* not HAVE_XDR_SATTR */


#ifndef HAVE_XDR_SATTRARGS
bool_t
xdr_sattrargs(XDR *xdrs, nfssattrargs *objp)
{
  if (amuDebug(D_XDRTRACE))
    plog(XLOG_DEBUG, "xdr_sattrargs:");

  if (!xdr_nfs_fh(xdrs, &objp->sag_fhandle)) {
    return (FALSE);
  }
  if (!xdr_sattr(xdrs, &objp->sag_attributes)) {
    return (FALSE);
  }
  return (TRUE);
}
#endif /* not HAVE_XDR_SATTRARGS */


#ifndef HAVE_XDR_STATFSOKRES
bool_t
xdr_statfsokres(XDR *xdrs, nfsstatfsokres *objp)
{
  if (amuDebug(D_XDRTRACE))
    plog(XLOG_DEBUG, "xdr_statfsokres:");

  if (!xdr_u_int(xdrs, &objp->sfrok_tsize)) {
    return (FALSE);
  }
  if (!xdr_u_int(xdrs, &objp->sfrok_bsize)) {
    return (FALSE);
  }
  if (!xdr_u_int(xdrs, &objp->sfrok_blocks)) {
    return (FALSE);
  }
  if (!xdr_u_int(xdrs, &objp->sfrok_bfree)) {
    return (FALSE);
  }
  if (!xdr_u_int(xdrs, &objp->sfrok_bavail)) {
    return (FALSE);
  }
  return (TRUE);
}
#endif /* not HAVE_XDR_STATFSOKRES */


#ifndef HAVE_XDR_STATFSRES
bool_t
xdr_statfsres(XDR *xdrs, nfsstatfsres *objp)
{
  if (amuDebug(D_XDRTRACE))
    plog(XLOG_DEBUG, "xdr_statfsres:");

  if (!xdr_nfsstat(xdrs, &objp->sfr_status)) {
    return (FALSE);
  }
  switch (objp->sfr_status) {
  case NFS_OK:
    if (!xdr_statfsokres(xdrs, &objp->sfr_u.sfr_reply_u)) {
      return (FALSE);
    }
    break;
  default:
    break;
  }
  return (TRUE);
}
#endif /* not HAVE_XDR_STATFSRES */


#ifndef HAVE_XDR_SYMLINKARGS
bool_t
xdr_symlinkargs(XDR *xdrs, nfssymlinkargs *objp)
{
  if (amuDebug(D_XDRTRACE))
    plog(XLOG_DEBUG, "xdr_symlinkargs:");

  if (!xdr_diropargs(xdrs, &objp->sla_from)) {
    return (FALSE);
  }
  if (!xdr_nfspath(xdrs, &objp->sla_to)) {
    return (FALSE);
  }
  if (!xdr_sattr(xdrs, &objp->sla_attributes)) {
    return (FALSE);
  }
  return (TRUE);
}
#endif /* not HAVE_XDR_SYMLINKARGS */


#ifndef HAVE_XDR_WRITEARGS
bool_t
xdr_writeargs(XDR *xdrs, nfswriteargs *objp)
{
  if (amuDebug(D_XDRTRACE))
    plog(XLOG_DEBUG, "xdr_writeargs:");

  if (!xdr_nfs_fh(xdrs, &objp->wra_fhandle)) {
    return (FALSE);
  }
  if (!xdr_u_int(xdrs, &objp->wra_beginoffset)) {
    return (FALSE);
  }
  if (!xdr_u_int(xdrs, &objp->wra_offset)) {
    return (FALSE);
  }
  if (!xdr_u_int(xdrs, &objp->wra_totalcount)) {
    return (FALSE);
  }
  if (!xdr_bytes(xdrs,
		 (char **) & objp->wra_u.wra_val_u,
		 (u_int *) & objp->wra_u.wra_len_u,
		 NFS_MAXDATA)) {
    return (FALSE);
  }
  return (TRUE);
}
#endif /* not HAVE_XDR_WRITEARGS */


/*
 * NFS V3 XDR FUNCTIONS:
 */
#ifdef HAVE_FS_NFS3
bool_t
xdr_am_fhandle3(XDR *xdrs, am_fhandle3 *objp)
{
  if (amuDebug(D_XDRTRACE))
    plog(XLOG_DEBUG, "xdr_am_fhandle3:");

  if (!xdr_bytes(xdrs,
		 (char **) &objp->fhandle3_val,
		 (u_int *) &objp->fhandle3_len,
		 AM_FHSIZE3))
    return (FALSE);
  return (TRUE);
}


bool_t
xdr_am_mountstat3(XDR *xdrs, am_mountstat3 *objp)
{
  enum_t local_obj = *objp;

  if (amuDebug(D_XDRTRACE))
    plog(XLOG_DEBUG, "xdr_am_mountstat3:");

  if (!xdr_enum(xdrs, &local_obj))
    return (FALSE);
  return (TRUE);
}


bool_t
xdr_am_mountres3_ok(XDR *xdrs, am_mountres3_ok *objp)
{
  if (amuDebug(D_XDRTRACE))
    plog(XLOG_DEBUG, "xdr_am_mountres3_ok:");

  if (!xdr_am_fhandle3(xdrs, &objp->fhandle))
    return (FALSE);
  if (!xdr_array(xdrs,
		 (char **) ((voidp) &objp->auth_flavors.auth_flavors_val),
		 (u_int *) &objp->auth_flavors.auth_flavors_len,
		 ~0,
		 sizeof(int),
		 (XDRPROC_T_TYPE) xdr_int))
    return (FALSE);
  return (TRUE);
}


bool_t
xdr_am_mountres3(XDR *xdrs, am_mountres3 *objp)
{
  if (amuDebug(D_XDRTRACE))
    plog(XLOG_DEBUG, "xdr_am_mountres3:");

  if (!xdr_am_mountstat3(xdrs, &objp->fhs_status))
    return (FALSE);

  if (objp->fhs_status == AM_MNT3_OK) {
    if (!xdr_am_mountres3_ok(xdrs, &objp->mountres3_u.mountinfo))
      return (FALSE);
  }
  return (TRUE);
}


bool_t
xdr_am_diropargs3(XDR *xdrs, am_diropargs3 *objp)
{
  if (amuDebug(D_XDRTRACE))
    plog(XLOG_DEBUG, "xdr_am_diropargs3:");

  if (!xdr_am_nfs_fh3(xdrs, &objp->dir))
    return (FALSE);
  if (!xdr_am_filename3(xdrs, &objp->name))
    return (FALSE);
  return (TRUE);
}


bool_t
xdr_am_filename3(XDR *xdrs, am_filename3 *objp)
{
  if (amuDebug(D_XDRTRACE))
    plog(XLOG_DEBUG, "xdr_am_filename3:");

  if (!xdr_string(xdrs, objp, ~0))
    return (FALSE);
  return (TRUE);
}


bool_t
xdr_am_LOOKUP3args(XDR *xdrs, am_LOOKUP3args *objp)
{
  if (amuDebug(D_XDRTRACE))
    plog(XLOG_DEBUG, "xdr_am_LOOKUP3args:");

  if (!xdr_am_diropargs3(xdrs, &objp->what))
    return (FALSE);
  return (TRUE);
}


bool_t
xdr_am_LOOKUP3res(XDR *xdrs, am_LOOKUP3res *objp)
{
  if (amuDebug(D_XDRTRACE))
    plog(XLOG_DEBUG, "xdr_am_LOOKUP3res:");

  if (!xdr_am_nfsstat3(xdrs, &objp->status))
    return (FALSE);
  switch (objp->status) {
  case AM_NFS3_OK:
    if (!xdr_am_LOOKUP3resok(xdrs, &objp->res_u.ok))
      return (FALSE);
    break;
  default:
    if (!xdr_am_LOOKUP3resfail(xdrs, &objp->res_u.fail))
      return (FALSE);
    break;
  }
  return (TRUE);
}


bool_t
xdr_am_LOOKUP3resfail(XDR *xdrs, am_LOOKUP3resfail *objp)
{
  if (amuDebug(D_XDRTRACE))
    plog(XLOG_DEBUG, "xdr_am_LOOKUP3resfail:");

  /*
   * Don't xdr post_op_attr: amd doesn't need them, but they require many
   * additional xdr functions.
   */
#if 0
  if (!xdr_post_op_attr(xdrs, &objp->dir_attributes))
    return (FALSE);
#endif
  return (TRUE);
}


bool_t
xdr_am_LOOKUP3resok(XDR *xdrs, am_LOOKUP3resok *objp)
{
  if (amuDebug(D_XDRTRACE))
    plog(XLOG_DEBUG, "xdr_am_LOOKUP3resok:");

  if (!xdr_am_nfs_fh3(xdrs, &objp->object))
    return (FALSE);
  /*
   * Don't xdr post_op_attr: amd doesn't need them, but they require many
   * additional xdr functions.
   */
#if 0
  if (!xdr_post_op_attr(xdrs, &objp->obj_attributes))
    return (FALSE);
  if (!xdr_post_op_attr(xdrs, &objp->dir_attributes))
    return (FALSE);
#endif
  return (TRUE);
}


bool_t
xdr_am_nfs_fh3(XDR *xdrs, am_nfs_fh3 *objp)
{
  if (amuDebug(D_XDRTRACE))
    plog(XLOG_DEBUG, "xdr_am_nfs_fh3:");

  if (!xdr_u_int(xdrs, &objp->am_fh3_length))
    return (FALSE);
  if (objp->am_fh3_length > AM_FHSIZE3)
    return (FALSE);
  if (!xdr_opaque(xdrs, objp->am_fh3_data, objp->am_fh3_length))
    return (FALSE);
  return (TRUE);
}


bool_t
xdr_am_nfsstat3(XDR *xdrs, am_nfsstat3 *objp)
{
  if (amuDebug(D_XDRTRACE))
    plog(XLOG_DEBUG, "xdr_am_nfsstat3:");

  if (!xdr_enum(xdrs, (enum_t *)objp))
    return (FALSE);
  return (TRUE);
}
#endif /* not HAVE_FS_NFS3 */
