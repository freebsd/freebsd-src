/*
 * Copyright (c) 1997-1999 Erez Zadok
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
 * $Id: am_xdr_func.h,v 1.2 1999/01/10 21:54:35 ezk Exp $
 *
 */

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
bool_t xdr_groups(XDR *xdrs, groups objp);
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

/*
 * NFS3 XDR FUNCTIONS:
 */
#if defined(HAVE_FS_NFS3) && !defined(HAVE_XDR_MOUNTRES3)
bool_t xdr_fhandle3(XDR *xdrs, fhandle3 *objp);
bool_t xdr_mountstat3(XDR *xdrs, mountstat3 *objp);
bool_t xdr_mountres3_ok(XDR *xdrs, mountres3_ok *objp);
bool_t xdr_mountres3(XDR *xdrs, mountres3 *objp);
#endif /* defined(HAVE_FS_NFS3) && !defined(HAVE_XDR_MOUNTRES3) */

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
 * AUTOFS XDR FUNCTIONS:
 */
#ifdef HAVE_FS_AUTOFS
# ifndef HAVE_XDR_MNTREQUEST
bool_t xdr_mntrequest(XDR *xdrs, mntrequest *objp);
# endif /* not HAVE_XDR_MNTREQUEST */
# ifndef HAVE_XDR_MNTRES
bool_t xdr_mntres(XDR *xdrs, mntres *objp);
# endif /* not HAVE_XDR_MNTRES */
# ifndef HAVE_XDR_UMNTREQUEST
bool_t xdr_umntrequest(XDR *xdrs, umntrequest *objp);
# endif /* not HAVE_XDR_UMNTREQUEST */
# ifndef HAVE_XDR_UMNTRES
bool_t xdr_umntres(XDR *xdrs, umntres *objp);
# endif /* not HAVE_XDR_UMNTRES */
#endif /* HAVE_FS_AUTOFS */

#endif /* not _AM_XDR_FUNC_H */
