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
 *	@(#)nfs_prot_svc.c	8.1 (Berkeley) 6/6/93
 *
 * $FreeBSD$
 *
 */

#include "am.h"

void nfs_program_2(rqstp, transp)
struct svc_req *rqstp;
SVCXPRT *transp;
{
	union {
		nfs_fh nfsproc_getattr_2_arg;
		sattrargs nfsproc_setattr_2_arg;
		diropargs nfsproc_lookup_2_arg;
		nfs_fh nfsproc_readlink_2_arg;
		readargs nfsproc_read_2_arg;
		writeargs nfsproc_write_2_arg;
		createargs nfsproc_create_2_arg;
		diropargs nfsproc_remove_2_arg;
		renameargs nfsproc_rename_2_arg;
		linkargs nfsproc_link_2_arg;
		symlinkargs nfsproc_symlink_2_arg;
		createargs nfsproc_mkdir_2_arg;
		diropargs nfsproc_rmdir_2_arg;
		readdirargs nfsproc_readdir_2_arg;
		nfs_fh nfsproc_statfs_2_arg;
	} argument;
	char *result;
	bool_t (*xdr_argument)(), (*xdr_result)();
	char *(*local)();

	switch (rqstp->rq_proc) {
	case NFSPROC_NULL:
		xdr_argument = xdr_void;
		xdr_result = xdr_void;
		local = (char *(*)()) nfsproc_null_2;
		break;

	case NFSPROC_GETATTR:
		xdr_argument = xdr_nfs_fh;
		xdr_result = xdr_attrstat;
		local = (char *(*)()) nfsproc_getattr_2;
		break;

	case NFSPROC_SETATTR:
		xdr_argument = xdr_sattrargs;
		xdr_result = xdr_attrstat;
		local = (char *(*)()) nfsproc_setattr_2;
		break;

	case NFSPROC_ROOT:
		xdr_argument = xdr_void;
		xdr_result = xdr_void;
		local = (char *(*)()) nfsproc_root_2;
		break;

	case NFSPROC_LOOKUP:
		xdr_argument = xdr_diropargs;
		xdr_result = xdr_diropres;
		local = (char *(*)()) nfsproc_lookup_2;
		break;

	case NFSPROC_READLINK:
		xdr_argument = xdr_nfs_fh;
		xdr_result = xdr_readlinkres;
		local = (char *(*)()) nfsproc_readlink_2;
		break;

	case NFSPROC_READ:
		xdr_argument = xdr_readargs;
		xdr_result = xdr_readres;
		local = (char *(*)()) nfsproc_read_2;
		break;

	case NFSPROC_WRITECACHE:
		xdr_argument = xdr_void;
		xdr_result = xdr_void;
		local = (char *(*)()) nfsproc_writecache_2;
		break;

	case NFSPROC_WRITE:
		xdr_argument = xdr_writeargs;
		xdr_result = xdr_attrstat;
		local = (char *(*)()) nfsproc_write_2;
		break;

	case NFSPROC_CREATE:
		xdr_argument = xdr_createargs;
		xdr_result = xdr_diropres;
		local = (char *(*)()) nfsproc_create_2;
		break;

	case NFSPROC_REMOVE:
		xdr_argument = xdr_diropargs;
		xdr_result = xdr_nfsstat;
		local = (char *(*)()) nfsproc_remove_2;
		break;

	case NFSPROC_RENAME:
		xdr_argument = xdr_renameargs;
		xdr_result = xdr_nfsstat;
		local = (char *(*)()) nfsproc_rename_2;
		break;

	case NFSPROC_LINK:
		xdr_argument = xdr_linkargs;
		xdr_result = xdr_nfsstat;
		local = (char *(*)()) nfsproc_link_2;
		break;

	case NFSPROC_SYMLINK:
		xdr_argument = xdr_symlinkargs;
		xdr_result = xdr_nfsstat;
		local = (char *(*)()) nfsproc_symlink_2;
		break;

	case NFSPROC_MKDIR:
		xdr_argument = xdr_createargs;
		xdr_result = xdr_diropres;
		local = (char *(*)()) nfsproc_mkdir_2;
		break;

	case NFSPROC_RMDIR:
		xdr_argument = xdr_diropargs;
		xdr_result = xdr_nfsstat;
		local = (char *(*)()) nfsproc_rmdir_2;
		break;

	case NFSPROC_READDIR:
		xdr_argument = xdr_readdirargs;
		xdr_result = xdr_readdirres;
		local = (char *(*)()) nfsproc_readdir_2;
		break;

	case NFSPROC_STATFS:
		xdr_argument = xdr_nfs_fh;
		xdr_result = xdr_statfsres;
		local = (char *(*)()) nfsproc_statfs_2;
		break;

	default:
		svcerr_noproc(transp);
		return;
	}
	bzero((char *)&argument, sizeof(argument));
	if (!svc_getargs(transp, xdr_argument, &argument)) {
		svcerr_decode(transp);
		return;
	}
	result = (*local)(&argument, rqstp);
	if (result != NULL && !svc_sendreply(transp, xdr_result, result)) {
		svcerr_systemerr(transp);
	}
	if (!svc_freeargs(transp, xdr_argument, &argument)) {
		plog(XLOG_FATAL, "unable to free rpc arguments in nfs_program_1");
		going_down(1);
	}
}

