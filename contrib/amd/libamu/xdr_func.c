/*
 * Copyright (c) 1997-2004 Erez Zadok
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
 *      %W% (Berkeley) %G%
 *
 * $Id: xdr_func.c,v 1.4.2.8 2004/01/06 03:15:24 ezk Exp $
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


/*
 * MACROS:
 */

/* forward definitions, are they needed? */
extern bool_t xdr_exportnode(XDR *xdrs, exportnode *objp);
extern bool_t xdr_groupnode(XDR *xdrs, groupnode *objp);
extern bool_t xdr_name(XDR *xdrs, name *objp);


#ifndef HAVE_XDR_ATTRSTAT
bool_t
xdr_attrstat(XDR *xdrs, nfsattrstat *objp)
{
#ifdef DEBUG
  amuDebug(D_XDRTRACE)
    plog(XLOG_DEBUG, "xdr_attrstat:");
#endif /* DEBUG */

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
#ifdef DEBUG
  amuDebug(D_XDRTRACE)
    plog(XLOG_DEBUG, "xdr_createargs:");
#endif /* DEBUG */

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
#ifdef DEBUG
  amuDebug(D_XDRTRACE)
    plog(XLOG_DEBUG, "xdr_dirlist:");
#endif /* DEBUG */

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
#ifdef DEBUG
  amuDebug(D_XDRTRACE)
    plog(XLOG_DEBUG, "xdr_diropargs:");
#endif /* DEBUG */

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
#ifdef DEBUG
  amuDebug(D_XDRTRACE)
    plog(XLOG_DEBUG, "xdr_diropokres:");
#endif /* DEBUG */

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
#ifdef DEBUG
  amuDebug(D_XDRTRACE)
    plog(XLOG_DEBUG, "xdr_diropres:");
#endif /* DEBUG */

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
#ifdef DEBUG
  amuDebug(D_XDRTRACE)
    plog(XLOG_DEBUG, "xdr_dirpath:");
#endif /* DEBUG */

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
#ifdef DEBUG
  amuDebug(D_XDRTRACE)
    plog(XLOG_DEBUG, "xdr_entry:");
#endif /* DEBUG */

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
#ifdef DEBUG
  amuDebug(D_XDRTRACE)
    plog(XLOG_DEBUG, "xdr_exportnode:");
#endif /* DEBUG */

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
#ifdef DEBUG
  amuDebug(D_XDRTRACE)
    plog(XLOG_DEBUG, "xdr_exports:");
#endif /* DEBUG */

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
#ifdef DEBUG
  amuDebug(D_XDRTRACE)
    plog(XLOG_DEBUG, "xdr_fattr:");
#endif /* DEBUG */

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
#ifdef DEBUG
  amuDebug(D_XDRTRACE)
    plog(XLOG_DEBUG, "xdr_fhandle:");
#endif /* DEBUG */

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
#ifdef DEBUG
  amuDebug(D_XDRTRACE)
    plog(XLOG_DEBUG, "xdr_fhstatus:");
#endif /* DEBUG */

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
#ifdef DEBUG
  amuDebug(D_XDRTRACE)
    plog(XLOG_DEBUG, "xdr_filename:");
#endif /* DEBUG */

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
#ifdef DEBUG
  amuDebug(D_XDRTRACE)
    plog(XLOG_DEBUG, "xdr_ftype:");
#endif /* DEBUG */

  if (!xdr_enum(xdrs, (enum_t *) objp)) {
    return (FALSE);
  }
  return (TRUE);
}
#endif /* not HAVE_XDR_FTYPE */


#ifndef HAVE_XDR_GROUPNODE
bool_t
xdr_groupnode(XDR *xdrs, groupnode *objp)
{
#ifdef DEBUG
  amuDebug(D_XDRTRACE)
    plog(XLOG_DEBUG, "xdr_groupnode:");
#endif /* DEBUG */

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
#ifdef DEBUG
  amuDebug(D_XDRTRACE)
    plog(XLOG_DEBUG, "xdr_groups:");
#endif /* DEBUG */

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
#ifdef DEBUG
  amuDebug(D_XDRTRACE)
    plog(XLOG_DEBUG, "xdr_linkargs:");
#endif /* DEBUG */

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
#ifdef DEBUG
  amuDebug(D_XDRTRACE)
    plog(XLOG_DEBUG, "xdr_mountbody:");
#endif /* DEBUG */

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
#ifdef DEBUG
  amuDebug(D_XDRTRACE)
    plog(XLOG_DEBUG, "xdr_mountlist:");
#endif /* DEBUG */

  if (!xdr_pointer(xdrs, (char **) objp, sizeof(mountbody), (XDRPROC_T_TYPE) xdr_mountbody)) {
    return (FALSE);
  }
  return (TRUE);
}
#endif /* not HAVE_XDR_MOUNTLIST */


#if defined(HAVE_FS_NFS3) && !defined(HAVE_XDR_MOUNTRES3)
/*
 * This ifdef is a hack: this whole file needs to be compiled
 * only if the system has NFS V3 and does not have the xdr_mountres3
 * function.  Autoconf should pick this source file to compile only
 * if these two conditions apply.
 */

bool_t
xdr_fhandle3(XDR *xdrs, fhandle3 *objp)
{
#ifdef DEBUG
  amuDebug(D_XDRTRACE)
    plog(XLOG_DEBUG, "xdr_fhandle3:");
#endif /* DEBUG */

  if (!xdr_bytes(xdrs,
		 (char **) &objp->fhandle3_val,
		 (u_int *) &objp->fhandle3_len,
		 FHSIZE3))
    return (FALSE);
  return (TRUE);
}


bool_t
xdr_mountstat3(XDR *xdrs, mountstat3 *objp)
{
#ifdef DEBUG
  amuDebug(D_XDRTRACE)
    plog(XLOG_DEBUG, "xdr_mountstat3:");
#endif /* DEBUG */

  if (!xdr_enum(xdrs, (enum_t *)objp))
    return (FALSE);
  return (TRUE);
}


bool_t
xdr_mountres3_ok(XDR *xdrs, mountres3_ok *objp)
{
#ifdef DEBUG
  amuDebug(D_XDRTRACE)
    plog(XLOG_DEBUG, "xdr_mountres3_ok:");
#endif /* DEBUG */

  if (!xdr_fhandle3(xdrs, &objp->fhandle))
    return (FALSE);
  if (!xdr_array(xdrs,
		 (char **)&objp->auth_flavors.auth_flavors_val,
		 (u_int *) &objp->auth_flavors.auth_flavors_len,
		 ~0,
		 sizeof (int),
		 (xdrproc_t) xdr_int))
    return (FALSE);
  return (TRUE);
}


bool_t
xdr_mountres3(XDR *xdrs, mountres3 *objp)
{
#ifdef DEBUG
  amuDebug(D_XDRTRACE)
    plog(XLOG_DEBUG, "xdr_mountres3:");
#endif /* DEBUG */

  if (!xdr_mountstat3(xdrs, &objp->fhs_status))
    return (FALSE);

  if (objp->fhs_status == 0) {	/* 0 == MNT_OK or MNT3_OK */
    if (!xdr_mountres3_ok(xdrs, &objp->mountres3_u.mountinfo))
      return (FALSE);
  }
  return (TRUE);
}
#endif /* defined(HAVE_FS_NFS3) && !defined(HAVE_XDR_MOUNTRES3) */


#ifndef HAVE_XDR_NAME
bool_t
xdr_name(XDR *xdrs, name *objp)
{
#ifdef DEBUG
  amuDebug(D_XDRTRACE)
    plog(XLOG_DEBUG, "xdr_name:");
#endif /* DEBUG */

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
#ifdef DEBUG
  amuDebug(D_XDRTRACE)
    plog(XLOG_DEBUG, "xdr_nfs_fh:");
#endif /* DEBUG */

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
#ifdef DEBUG
  amuDebug(D_XDRTRACE)
    plog(XLOG_DEBUG, "xdr_nfscookie:");
#endif /* DEBUG */

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
#ifdef DEBUG
  amuDebug(D_XDRTRACE)
    plog(XLOG_DEBUG, "xdr_nfspath:");
#endif /* DEBUG */

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
#ifdef DEBUG
  amuDebug(D_XDRTRACE)
    plog(XLOG_DEBUG, "xdr_nfsstat:");
#endif /* DEBUG */

  if (!xdr_enum(xdrs, (enum_t *) objp)) {
    return (FALSE);
  }
  return (TRUE);
}
#endif /* not HAVE_XDR_NFSSTAT */


#ifndef HAVE_XDR_NFSTIME
bool_t
xdr_nfstime(XDR *xdrs, nfstime *objp)
{
#ifdef DEBUG
  amuDebug(D_XDRTRACE)
    plog(XLOG_DEBUG, "xdr_nfstime:");
#endif /* DEBUG */

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
#ifdef DEBUG
  amuDebug(D_XDRTRACE)
    plog(XLOG_DEBUG, "xdr_pointer:");
#endif /* DEBUG */


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
#ifdef DEBUG
  amuDebug(D_XDRTRACE)
    plog(XLOG_DEBUG, "xdr_readargs:");
#endif /* DEBUG */

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
#ifdef DEBUG
  amuDebug(D_XDRTRACE)
    plog(XLOG_DEBUG, "xdr_readdirargs:");
#endif /* DEBUG */

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
#ifdef DEBUG
  amuDebug(D_XDRTRACE)
    plog(XLOG_DEBUG, "xdr_readdirres:");
#endif /* DEBUG */

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
#ifdef DEBUG
  amuDebug(D_XDRTRACE)
    plog(XLOG_DEBUG, "xdr_readlinkres:");
#endif /* DEBUG */

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
#ifdef DEBUG
  amuDebug(D_XDRTRACE)
    plog(XLOG_DEBUG, "xdr_readokres:");
#endif /* DEBUG */

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
#ifdef DEBUG
  amuDebug(D_XDRTRACE)
    plog(XLOG_DEBUG, "xdr_readres:");
#endif /* DEBUG */

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
#ifdef DEBUG
  amuDebug(D_XDRTRACE)
    plog(XLOG_DEBUG, "xdr_renameargs:");
#endif /* DEBUG */

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
#ifdef DEBUG
  amuDebug(D_XDRTRACE)
    plog(XLOG_DEBUG, "xdr_sattr:");
#endif /* DEBUG */

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
#ifdef DEBUG
  amuDebug(D_XDRTRACE)
    plog(XLOG_DEBUG, "xdr_sattrargs:");
#endif /* DEBUG */

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
#ifdef DEBUG
  amuDebug(D_XDRTRACE)
    plog(XLOG_DEBUG, "xdr_statfsokres:");
#endif /* DEBUG */

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
#ifdef DEBUG
  amuDebug(D_XDRTRACE)
    plog(XLOG_DEBUG, "xdr_statfsres:");
#endif /* DEBUG */

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
#ifdef DEBUG
  amuDebug(D_XDRTRACE)
    plog(XLOG_DEBUG, "xdr_symlinkargs:");
#endif /* DEBUG */

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
#ifdef DEBUG
  amuDebug(D_XDRTRACE)
    plog(XLOG_DEBUG, "xdr_writeargs:");
#endif /* DEBUG */

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
