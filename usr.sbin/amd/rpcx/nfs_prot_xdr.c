/*
 * Copyright (c) 1989 Jan-Simon Pendry
 * Copyright (c) 1989 Imperial College of Science, Technology & Medicine
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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
 *	@(#)nfs_prot_xdr.c	8.1 (Berkeley) 6/6/93
 *
 * $FreeBSD$
 *
 */

#include "am.h"


#ifndef xdr_nfsstat
bool_t
xdr_nfsstat(xdrs, objp)
	XDR *xdrs;
	nfsstat *objp;
{
	if (!xdr_enum(xdrs, (enum_t *)objp)) {
		return (FALSE);
	}
	return (TRUE);
}
#endif /* xdr_nfsstat */



#ifndef xdr_ftype
static bool_t
xdr_ftype(xdrs, objp)
	XDR *xdrs;
	ftype *objp;
{
	if (!xdr_enum(xdrs, (enum_t *)objp)) {
		return (FALSE);
	}
	return (TRUE);
}
#endif /* xdr_ftype */



bool_t
xdr_nfs_fh(xdrs, objp)
	XDR *xdrs;
	nfs_fh *objp;
{
	if (!xdr_opaque(xdrs, objp->data, NFS_FHSIZE)) {
		return (FALSE);
	}
	return (TRUE);
}




static bool_t
xdr_nfstime(xdrs, objp)
	XDR *xdrs;
	nfstime *objp;
{
	if (!xdr_u_int(xdrs, &objp->seconds)) {
		return (FALSE);
	}
	if (!xdr_u_int(xdrs, &objp->useconds)) {
		return (FALSE);
	}
	return (TRUE);
}




static bool_t
xdr_fattr(xdrs, objp)
	XDR *xdrs;
	fattr *objp;
{
	if (!xdr_ftype(xdrs, &objp->type)) {
		return (FALSE);
	}
	if (!xdr_u_int(xdrs, &objp->mode)) {
		return (FALSE);
	}
	if (!xdr_u_int(xdrs, &objp->nlink)) {
		return (FALSE);
	}
	if (!xdr_u_int(xdrs, &objp->uid)) {
		return (FALSE);
	}
	if (!xdr_u_int(xdrs, &objp->gid)) {
		return (FALSE);
	}
	if (!xdr_u_int(xdrs, &objp->size)) {
		return (FALSE);
	}
	if (!xdr_u_int(xdrs, &objp->blocksize)) {
		return (FALSE);
	}
	if (!xdr_u_int(xdrs, &objp->rdev)) {
		return (FALSE);
	}
	if (!xdr_u_int(xdrs, &objp->blocks)) {
		return (FALSE);
	}
	if (!xdr_u_int(xdrs, &objp->fsid)) {
		return (FALSE);
	}
	if (!xdr_u_int(xdrs, &objp->fileid)) {
		return (FALSE);
	}
	if (!xdr_nfstime(xdrs, &objp->atime)) {
		return (FALSE);
	}
	if (!xdr_nfstime(xdrs, &objp->mtime)) {
		return (FALSE);
	}
	if (!xdr_nfstime(xdrs, &objp->ctime)) {
		return (FALSE);
	}
	return (TRUE);
}




static bool_t
xdr_sattr(xdrs, objp)
	XDR *xdrs;
	sattr *objp;
{
	if (!xdr_u_int(xdrs, &objp->mode)) {
		return (FALSE);
	}
	if (!xdr_u_int(xdrs, &objp->uid)) {
		return (FALSE);
	}
	if (!xdr_u_int(xdrs, &objp->gid)) {
		return (FALSE);
	}
	if (!xdr_u_int(xdrs, &objp->size)) {
		return (FALSE);
	}
	if (!xdr_nfstime(xdrs, &objp->atime)) {
		return (FALSE);
	}
	if (!xdr_nfstime(xdrs, &objp->mtime)) {
		return (FALSE);
	}
	return (TRUE);
}




static bool_t
xdr_filename(xdrs, objp)
	XDR *xdrs;
	filename *objp;
{
	if (!xdr_string(xdrs, objp, NFS_MAXNAMLEN)) {
		return (FALSE);
	}
	return (TRUE);
}




bool_t
xdr_nfspath(xdrs, objp)
	XDR *xdrs;
	nfspath *objp;
{
	if (!xdr_string(xdrs, objp, NFS_MAXPATHLEN)) {
		return (FALSE);
	}
	return (TRUE);
}




bool_t
xdr_attrstat(xdrs, objp)
	XDR *xdrs;
	attrstat *objp;
{
	if (!xdr_nfsstat(xdrs, &objp->status)) {
		return (FALSE);
	}
	switch (objp->status) {
	case NFS_OK:
		if (!xdr_fattr(xdrs, &objp->attrstat_u.attributes)) {
			return (FALSE);
		}
		break;
	}
	return (TRUE);
}




bool_t
xdr_sattrargs(xdrs, objp)
	XDR *xdrs;
	sattrargs *objp;
{
	if (!xdr_nfs_fh(xdrs, &objp->file)) {
		return (FALSE);
	}
	if (!xdr_sattr(xdrs, &objp->attributes)) {
		return (FALSE);
	}
	return (TRUE);
}




bool_t
xdr_diropargs(xdrs, objp)
	XDR *xdrs;
	diropargs *objp;
{
	if (!xdr_nfs_fh(xdrs, &objp->dir)) {
		return (FALSE);
	}
	if (!xdr_filename(xdrs, &objp->name)) {
		return (FALSE);
	}
	return (TRUE);
}




bool_t
xdr_diropokres(xdrs, objp)
	XDR *xdrs;
	diropokres *objp;
{
	if (!xdr_nfs_fh(xdrs, &objp->file)) {
		return (FALSE);
	}
	if (!xdr_fattr(xdrs, &objp->attributes)) {
		return (FALSE);
	}
	return (TRUE);
}




bool_t
xdr_diropres(xdrs, objp)
	XDR *xdrs;
	diropres *objp;
{
	if (!xdr_nfsstat(xdrs, &objp->status)) {
		return (FALSE);
	}
	switch (objp->status) {
	case NFS_OK:
		if (!xdr_diropokres(xdrs, &objp->diropres_u.diropres)) {
			return (FALSE);
		}
		break;
	}
	return (TRUE);
}




bool_t
xdr_readlinkres(xdrs, objp)
	XDR *xdrs;
	readlinkres *objp;
{
	if (!xdr_nfsstat(xdrs, &objp->status)) {
		return (FALSE);
	}
	switch (objp->status) {
	case NFS_OK:
		if (!xdr_nfspath(xdrs, &objp->readlinkres_u.data)) {
			return (FALSE);
		}
		break;
	}
	return (TRUE);
}




bool_t
xdr_readargs(xdrs, objp)
	XDR *xdrs;
	readargs *objp;
{
	if (!xdr_nfs_fh(xdrs, &objp->file)) {
		return (FALSE);
	}
	if (!xdr_u_int(xdrs, &objp->offset)) {
		return (FALSE);
	}
	if (!xdr_u_int(xdrs, &objp->count)) {
		return (FALSE);
	}
	if (!xdr_u_int(xdrs, &objp->totalcount)) {
		return (FALSE);
	}
	return (TRUE);
}




bool_t
xdr_readokres(xdrs, objp)
	XDR *xdrs;
	readokres *objp;
{
	if (!xdr_fattr(xdrs, &objp->attributes)) {
		return (FALSE);
	}
	if (!xdr_bytes(xdrs, (char **)&objp->data.data_val, (u_int *)&objp->data.data_len, NFS_MAXDATA)) {
		return (FALSE);
	}
	return (TRUE);
}




bool_t
xdr_readres(xdrs, objp)
	XDR *xdrs;
	readres *objp;
{
	if (!xdr_nfsstat(xdrs, &objp->status)) {
		return (FALSE);
	}
	switch (objp->status) {
	case NFS_OK:
		if (!xdr_readokres(xdrs, &objp->readres_u.reply)) {
			return (FALSE);
		}
		break;
	}
	return (TRUE);
}




bool_t
xdr_writeargs(xdrs, objp)
	XDR *xdrs;
	writeargs *objp;
{
	if (!xdr_nfs_fh(xdrs, &objp->file)) {
		return (FALSE);
	}
	if (!xdr_u_int(xdrs, &objp->beginoffset)) {
		return (FALSE);
	}
	if (!xdr_u_int(xdrs, &objp->offset)) {
		return (FALSE);
	}
	if (!xdr_u_int(xdrs, &objp->totalcount)) {
		return (FALSE);
	}
	if (!xdr_bytes(xdrs, (char **)&objp->data.data_val, (u_int *)&objp->data.data_len, NFS_MAXDATA)) {
		return (FALSE);
	}
	return (TRUE);
}




bool_t
xdr_createargs(xdrs, objp)
	XDR *xdrs;
	createargs *objp;
{
	if (!xdr_diropargs(xdrs, &objp->where)) {
		return (FALSE);
	}
	if (!xdr_sattr(xdrs, &objp->attributes)) {
		return (FALSE);
	}
	return (TRUE);
}




bool_t
xdr_renameargs(xdrs, objp)
	XDR *xdrs;
	renameargs *objp;
{
	if (!xdr_diropargs(xdrs, &objp->from)) {
		return (FALSE);
	}
	if (!xdr_diropargs(xdrs, &objp->to)) {
		return (FALSE);
	}
	return (TRUE);
}




bool_t
xdr_linkargs(xdrs, objp)
	XDR *xdrs;
	linkargs *objp;
{
	if (!xdr_nfs_fh(xdrs, &objp->from)) {
		return (FALSE);
	}
	if (!xdr_diropargs(xdrs, &objp->to)) {
		return (FALSE);
	}
	return (TRUE);
}




bool_t
xdr_symlinkargs(xdrs, objp)
	XDR *xdrs;
	symlinkargs *objp;
{
	if (!xdr_diropargs(xdrs, &objp->from)) {
		return (FALSE);
	}
	if (!xdr_nfspath(xdrs, &objp->to)) {
		return (FALSE);
	}
	if (!xdr_sattr(xdrs, &objp->attributes)) {
		return (FALSE);
	}
	return (TRUE);
}




static bool_t
xdr_nfscookie(xdrs, objp)
	XDR *xdrs;
	nfscookie objp;
{
	if (!xdr_opaque(xdrs, objp, NFS_COOKIESIZE)) {
		return (FALSE);
	}
	return (TRUE);
}




bool_t
xdr_readdirargs(xdrs, objp)
	XDR *xdrs;
	readdirargs *objp;
{
	if (!xdr_nfs_fh(xdrs, &objp->dir)) {
		return (FALSE);
	}
	if (!xdr_nfscookie(xdrs, objp->cookie)) {
		return (FALSE);
	}
	if (!xdr_u_int(xdrs, &objp->count)) {
		return (FALSE);
	}
	return (TRUE);
}




static bool_t
xdr_entry(xdrs, objp)
	XDR *xdrs;
	entry *objp;
{
	if (!xdr_u_int(xdrs, &objp->fileid)) {
		return (FALSE);
	}
	if (!xdr_filename(xdrs, &objp->name)) {
		return (FALSE);
	}
	if (!xdr_nfscookie(xdrs, objp->cookie)) {
		return (FALSE);
	}
	if (!xdr_pointer(xdrs, (char **)&objp->nextentry, sizeof(entry), xdr_entry)) {
		return (FALSE);
	}
	return (TRUE);
}




static bool_t
xdr_dirlist(xdrs, objp)
	XDR *xdrs;
	dirlist *objp;
{
	if (!xdr_pointer(xdrs, (char **)&objp->entries, sizeof(entry), xdr_entry)) {
		return (FALSE);
	}
	if (!xdr_bool(xdrs, &objp->eof)) {
		return (FALSE);
	}
	return (TRUE);
}




bool_t
xdr_readdirres(xdrs, objp)
	XDR *xdrs;
	readdirres *objp;
{
	if (!xdr_nfsstat(xdrs, &objp->status)) {
		return (FALSE);
	}
	switch (objp->status) {
	case NFS_OK:
		if (!xdr_dirlist(xdrs, &objp->readdirres_u.reply)) {
			return (FALSE);
		}
		break;
	}
	return (TRUE);
}




bool_t
xdr_statfsokres(xdrs, objp)
	XDR *xdrs;
	statfsokres *objp;
{
	if (!xdr_u_int(xdrs, &objp->tsize)) {
		return (FALSE);
	}
	if (!xdr_u_int(xdrs, &objp->bsize)) {
		return (FALSE);
	}
	if (!xdr_u_int(xdrs, &objp->blocks)) {
		return (FALSE);
	}
	if (!xdr_u_int(xdrs, &objp->bfree)) {
		return (FALSE);
	}
	if (!xdr_u_int(xdrs, &objp->bavail)) {
		return (FALSE);
	}
	return (TRUE);
}




bool_t
xdr_statfsres(xdrs, objp)
	XDR *xdrs;
	statfsres *objp;
{
	if (!xdr_nfsstat(xdrs, &objp->status)) {
		return (FALSE);
	}
	switch (objp->status) {
	case NFS_OK:
		if (!xdr_statfsokres(xdrs, &objp->statfsres_u.reply)) {
			return (FALSE);
		}
		break;
	}
	return (TRUE);
}
