/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Rick Macklem at The University of Guelph.
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
 */

#include <sys/cdefs.h>
/*
 * These functions support the macros and help fiddle mbuf chains for
 * the nfs op functions. They do things like create the rpc header and
 * copy data between mbuf chains and uio lists.
 */
#include <fs/nfs/nfsport.h>

extern u_int32_t newnfs_true, newnfs_false;
extern int nfs_pubfhset;
extern int nfsrv_clienthashsize;
extern int nfsrv_lockhashsize;
extern int nfsrv_sessionhashsize;
extern int nfsrv_useacl;
extern uid_t nfsrv_defaultuid;
extern gid_t nfsrv_defaultgid;

NFSD_VNET_DECLARE(struct nfsclienthashhead *, nfsclienthash);
NFSD_VNET_DECLARE(struct nfslockhashhead *, nfslockhash);
NFSD_VNET_DECLARE(struct nfssessionhash *, nfssessionhash);
NFSD_VNET_DECLARE(int, nfs_rootfhset);
NFSD_VNET_DECLARE(uid_t, nfsrv_defaultuid);
NFSD_VNET_DECLARE(gid_t, nfsrv_defaultgid);

char nfs_v2pubfh[NFSX_V2FH];
struct nfsdontlisthead nfsrv_dontlisthead;
struct nfslayouthead nfsrv_recalllisthead;
static nfstype newnfsv2_type[9] = { NFNON, NFREG, NFDIR, NFBLK, NFCHR, NFLNK,
    NFNON, NFCHR, NFNON };
extern nfstype nfsv34_type[9];

static u_int32_t nfsrv_isannfserr(u_int32_t);

SYSCTL_DECL(_vfs_nfsd);

static int	enable_checkutf8 = 1;
SYSCTL_INT(_vfs_nfsd, OID_AUTO, enable_checkutf8, CTLFLAG_RW,
    &enable_checkutf8, 0,
    "Enable the NFSv4 check for the UTF8 compliant name required by rfc3530");

static int    enable_nobodycheck = 1;
SYSCTL_INT(_vfs_nfsd, OID_AUTO, enable_nobodycheck, CTLFLAG_RW,
    &enable_nobodycheck, 0,
    "Enable the NFSv4 check when setting user nobody as owner");

static int    enable_nogroupcheck = 1;
SYSCTL_INT(_vfs_nfsd, OID_AUTO, enable_nogroupcheck, CTLFLAG_RW,
    &enable_nogroupcheck, 0,
    "Enable the NFSv4 check when setting group nogroup as owner");

static char nfsrv_hexdigit(char, int *);

/*
 * Maps errno values to nfs error numbers.
 * Use NFSERR_IO as the catch all for ones not specifically defined in
 * RFC 1094. (It now includes the errors added for NFSv3.)
 */
static u_char nfsrv_v2errmap[NFSERR_REMOTE] = {
  NFSERR_PERM,	NFSERR_NOENT,	NFSERR_IO,	NFSERR_IO,	NFSERR_IO,
  NFSERR_NXIO,	NFSERR_IO,	NFSERR_IO,	NFSERR_IO,	NFSERR_IO,
  NFSERR_IO,	NFSERR_IO,	NFSERR_ACCES,	NFSERR_IO,	NFSERR_IO,
  NFSERR_IO,	NFSERR_EXIST,	NFSERR_XDEV,	NFSERR_NODEV,	NFSERR_NOTDIR,
  NFSERR_ISDIR,	NFSERR_INVAL,	NFSERR_IO,	NFSERR_IO,	NFSERR_IO,
  NFSERR_IO,	NFSERR_FBIG,	NFSERR_NOSPC,	NFSERR_IO,	NFSERR_ROFS,
  NFSERR_MLINK,	NFSERR_IO,	NFSERR_IO,	NFSERR_IO,	NFSERR_IO,
  NFSERR_IO,	NFSERR_IO,	NFSERR_IO,	NFSERR_IO,	NFSERR_IO,
  NFSERR_IO,	NFSERR_IO,	NFSERR_IO,	NFSERR_IO,	NFSERR_IO,
  NFSERR_IO,	NFSERR_IO,	NFSERR_IO,	NFSERR_IO,	NFSERR_IO,
  NFSERR_IO,	NFSERR_IO,	NFSERR_IO,	NFSERR_IO,	NFSERR_IO,
  NFSERR_IO,	NFSERR_IO,	NFSERR_IO,	NFSERR_IO,	NFSERR_IO,
  NFSERR_IO,	NFSERR_IO,	NFSERR_NAMETOL,	NFSERR_IO,	NFSERR_IO,
  NFSERR_NOTEMPTY, NFSERR_IO,	NFSERR_IO,	NFSERR_DQUOT,	NFSERR_STALE,
  NFSERR_REMOTE,
};

/*
 * Maps errno values to nfs error numbers.
 * Although it is not obvious whether or not NFS clients really care if
 * a returned error value is in the specified list for the procedure, the
 * safest thing to do is filter them appropriately. For Version 2, the
 * X/Open XNFS document is the only specification that defines error values
 * for each RPC (The RFC simply lists all possible error values for all RPCs),
 * so I have decided to not do this for Version 2.
 * The first entry is the default error return and the rest are the valid
 * errors for that RPC in increasing numeric order.
 */
static short nfsv3err_null[] = {
	0,
	0,
};

static short nfsv3err_getattr[] = {
	NFSERR_IO,
	NFSERR_IO,
	NFSERR_STALE,
	NFSERR_BADHANDLE,
	NFSERR_SERVERFAULT,
	NFSERR_DELAY,
	0,
};

static short nfsv3err_setattr[] = {
	NFSERR_IO,
	NFSERR_ACCES,
	NFSERR_PERM,
	NFSERR_IO,
	NFSERR_INVAL,
	NFSERR_NOSPC,
	NFSERR_ROFS,
	NFSERR_DQUOT,
	NFSERR_STALE,
	NFSERR_BADHANDLE,
	NFSERR_NOT_SYNC,
	NFSERR_SERVERFAULT,
	NFSERR_DELAY,
	0,
};

static short nfsv3err_lookup[] = {
	NFSERR_IO,
	NFSERR_NOENT,
	NFSERR_ACCES,
	NFSERR_NAMETOL,
	NFSERR_IO,
	NFSERR_NOTDIR,
	NFSERR_STALE,
	NFSERR_BADHANDLE,
	NFSERR_SERVERFAULT,
	NFSERR_DELAY,
	0,
};

static short nfsv3err_access[] = {
	NFSERR_IO,
	NFSERR_IO,
	NFSERR_STALE,
	NFSERR_BADHANDLE,
	NFSERR_SERVERFAULT,
	NFSERR_DELAY,
	0,
};

static short nfsv3err_readlink[] = {
	NFSERR_IO,
	NFSERR_IO,
	NFSERR_ACCES,
	NFSERR_INVAL,
	NFSERR_STALE,
	NFSERR_BADHANDLE,
	NFSERR_NOTSUPP,
	NFSERR_SERVERFAULT,
	NFSERR_DELAY,
	0,
};

static short nfsv3err_read[] = {
	NFSERR_IO,
	NFSERR_IO,
	NFSERR_NXIO,
	NFSERR_ACCES,
	NFSERR_INVAL,
	NFSERR_STALE,
	NFSERR_BADHANDLE,
	NFSERR_SERVERFAULT,
	NFSERR_DELAY,
	0,
};

static short nfsv3err_write[] = {
	NFSERR_IO,
	NFSERR_IO,
	NFSERR_ACCES,
	NFSERR_NOSPC,
	NFSERR_INVAL,
	NFSERR_FBIG,
	NFSERR_ROFS,
	NFSERR_DQUOT,
	NFSERR_STALE,
	NFSERR_BADHANDLE,
	NFSERR_SERVERFAULT,
	NFSERR_DELAY,
	0,
};

static short nfsv3err_create[] = {
	NFSERR_IO,
	NFSERR_EXIST,
	NFSERR_NAMETOL,
	NFSERR_ACCES,
	NFSERR_IO,
	NFSERR_NOTDIR,
	NFSERR_NOSPC,
	NFSERR_ROFS,
	NFSERR_DQUOT,
	NFSERR_STALE,
	NFSERR_BADHANDLE,
	NFSERR_NOTSUPP,
	NFSERR_SERVERFAULT,
	NFSERR_DELAY,
	0,
};

static short nfsv3err_mkdir[] = {
	NFSERR_IO,
	NFSERR_EXIST,
	NFSERR_ACCES,
	NFSERR_NAMETOL,
	NFSERR_IO,
	NFSERR_NOTDIR,
	NFSERR_NOSPC,
	NFSERR_ROFS,
	NFSERR_DQUOT,
	NFSERR_STALE,
	NFSERR_BADHANDLE,
	NFSERR_NOTSUPP,
	NFSERR_SERVERFAULT,
	NFSERR_DELAY,
	0,
};

static short nfsv3err_symlink[] = {
	NFSERR_IO,
	NFSERR_ACCES,
	NFSERR_EXIST,
	NFSERR_NAMETOL,
	NFSERR_NOSPC,
	NFSERR_IO,
	NFSERR_NOTDIR,
	NFSERR_ROFS,
	NFSERR_DQUOT,
	NFSERR_STALE,
	NFSERR_BADHANDLE,
	NFSERR_NOTSUPP,
	NFSERR_SERVERFAULT,
	NFSERR_DELAY,
	0,
};

static short nfsv3err_mknod[] = {
	NFSERR_IO,
	NFSERR_ACCES,
	NFSERR_EXIST,
	NFSERR_NAMETOL,
	NFSERR_NOSPC,
	NFSERR_IO,
	NFSERR_NOTDIR,
	NFSERR_ROFS,
	NFSERR_DQUOT,
	NFSERR_STALE,
	NFSERR_BADHANDLE,
	NFSERR_NOTSUPP,
	NFSERR_SERVERFAULT,
	NFSERR_DELAY,
	NFSERR_BADTYPE,
	0,
};

static short nfsv3err_remove[] = {
	NFSERR_IO,
	NFSERR_NOENT,
	NFSERR_ACCES,
	NFSERR_NAMETOL,
	NFSERR_IO,
	NFSERR_NOTDIR,
	NFSERR_ROFS,
	NFSERR_STALE,
	NFSERR_BADHANDLE,
	NFSERR_SERVERFAULT,
	NFSERR_DELAY,
	0,
};

static short nfsv3err_rmdir[] = {
	NFSERR_IO,
	NFSERR_NOENT,
	NFSERR_ACCES,
	NFSERR_NOTDIR,
	NFSERR_NAMETOL,
	NFSERR_IO,
	NFSERR_EXIST,
	NFSERR_INVAL,
	NFSERR_ROFS,
	NFSERR_NOTEMPTY,
	NFSERR_STALE,
	NFSERR_BADHANDLE,
	NFSERR_NOTSUPP,
	NFSERR_SERVERFAULT,
	NFSERR_DELAY,
	0,
};

static short nfsv3err_rename[] = {
	NFSERR_IO,
	NFSERR_NOENT,
	NFSERR_ACCES,
	NFSERR_EXIST,
	NFSERR_NAMETOL,
	NFSERR_XDEV,
	NFSERR_IO,
	NFSERR_NOTDIR,
	NFSERR_ISDIR,
	NFSERR_INVAL,
	NFSERR_NOSPC,
	NFSERR_ROFS,
	NFSERR_MLINK,
	NFSERR_NOTEMPTY,
	NFSERR_DQUOT,
	NFSERR_STALE,
	NFSERR_BADHANDLE,
	NFSERR_NOTSUPP,
	NFSERR_SERVERFAULT,
	NFSERR_DELAY,
	0,
};

static short nfsv3err_link[] = {
	NFSERR_IO,
	NFSERR_ACCES,
	NFSERR_EXIST,
	NFSERR_NAMETOL,
	NFSERR_IO,
	NFSERR_XDEV,
	NFSERR_NOTDIR,
	NFSERR_INVAL,
	NFSERR_NOSPC,
	NFSERR_ROFS,
	NFSERR_MLINK,
	NFSERR_DQUOT,
	NFSERR_STALE,
	NFSERR_BADHANDLE,
	NFSERR_NOTSUPP,
	NFSERR_SERVERFAULT,
	NFSERR_DELAY,
	0,
};

static short nfsv3err_readdir[] = {
	NFSERR_IO,
	NFSERR_ACCES,
	NFSERR_NOTDIR,
	NFSERR_IO,
	NFSERR_STALE,
	NFSERR_BADHANDLE,
	NFSERR_BAD_COOKIE,
	NFSERR_TOOSMALL,
	NFSERR_SERVERFAULT,
	NFSERR_DELAY,
	0,
};

static short nfsv3err_readdirplus[] = {
	NFSERR_IO,
	NFSERR_ACCES,
	NFSERR_NOTDIR,
	NFSERR_IO,
	NFSERR_STALE,
	NFSERR_BADHANDLE,
	NFSERR_BAD_COOKIE,
	NFSERR_NOTSUPP,
	NFSERR_TOOSMALL,
	NFSERR_SERVERFAULT,
	NFSERR_DELAY,
	0,
};

static short nfsv3err_fsstat[] = {
	NFSERR_IO,
	NFSERR_IO,
	NFSERR_STALE,
	NFSERR_BADHANDLE,
	NFSERR_SERVERFAULT,
	NFSERR_DELAY,
	0,
};

static short nfsv3err_fsinfo[] = {
	NFSERR_STALE,
	NFSERR_STALE,
	NFSERR_BADHANDLE,
	NFSERR_SERVERFAULT,
	NFSERR_DELAY,
	0,
};

static short nfsv3err_pathconf[] = {
	NFSERR_STALE,
	NFSERR_STALE,
	NFSERR_BADHANDLE,
	NFSERR_SERVERFAULT,
	NFSERR_DELAY,
	0,
};

static short nfsv3err_commit[] = {
	NFSERR_IO,
	NFSERR_IO,
	NFSERR_STALE,
	NFSERR_BADHANDLE,
	NFSERR_SERVERFAULT,
	NFSERR_DELAY,
	0,
};

static short *nfsrv_v3errmap[] = {
	nfsv3err_null,
	nfsv3err_getattr,
	nfsv3err_setattr,
	nfsv3err_lookup,
	nfsv3err_access,
	nfsv3err_readlink,
	nfsv3err_read,
	nfsv3err_write,
	nfsv3err_create,
	nfsv3err_mkdir,
	nfsv3err_symlink,
	nfsv3err_mknod,
	nfsv3err_remove,
	nfsv3err_rmdir,
	nfsv3err_rename,
	nfsv3err_link,
	nfsv3err_readdir,
	nfsv3err_readdirplus,
	nfsv3err_fsstat,
	nfsv3err_fsinfo,
	nfsv3err_pathconf,
	nfsv3err_commit,
};

/*
 * And the same for V4.
 */
static short nfsv4err_null[] = {
	0,
	0,
};

static short nfsv4err_access[] = {
	NFSERR_IO,
	NFSERR_ACCES,
	NFSERR_BADHANDLE,
	NFSERR_BADXDR,
	NFSERR_DELAY,
	NFSERR_FHEXPIRED,
	NFSERR_INVAL,
	NFSERR_IO,
	NFSERR_MOVED,
	NFSERR_NOFILEHANDLE,
	NFSERR_RESOURCE,
	NFSERR_SERVERFAULT,
	NFSERR_STALE,
	0,
};

static short nfsv4err_close[] = {
	NFSERR_EXPIRED,
	NFSERR_ADMINREVOKED,
	NFSERR_BADHANDLE,
	NFSERR_BADSEQID,
	NFSERR_BADSTATEID,
	NFSERR_BADXDR,
	NFSERR_DELAY,
	NFSERR_EXPIRED,
	NFSERR_FHEXPIRED,
	NFSERR_INVAL,
	NFSERR_ISDIR,
	NFSERR_LEASEMOVED,
	NFSERR_LOCKSHELD,
	NFSERR_MOVED,
	NFSERR_NOFILEHANDLE,
	NFSERR_OLDSTATEID,
	NFSERR_RESOURCE,
	NFSERR_SERVERFAULT,
	NFSERR_STALE,
	NFSERR_STALESTATEID,
	0,
};

static short nfsv4err_commit[] = {
	NFSERR_IO,
	NFSERR_ACCES,
	NFSERR_BADHANDLE,
	NFSERR_BADXDR,
	NFSERR_FHEXPIRED,
	NFSERR_INVAL,
	NFSERR_IO,
	NFSERR_ISDIR,
	NFSERR_MOVED,
	NFSERR_NOFILEHANDLE,
	NFSERR_RESOURCE,
	NFSERR_ROFS,
	NFSERR_SERVERFAULT,
	NFSERR_STALE,
	0,
};

static short nfsv4err_create[] = {
	NFSERR_IO,
	NFSERR_ACCES,
	NFSERR_ATTRNOTSUPP,
	NFSERR_BADCHAR,
	NFSERR_BADHANDLE,
	NFSERR_BADNAME,
	NFSERR_BADOWNER,
	NFSERR_BADTYPE,
	NFSERR_BADXDR,
	NFSERR_DELAY,
	NFSERR_DQUOT,
	NFSERR_EXIST,
	NFSERR_FHEXPIRED,
	NFSERR_INVAL,
	NFSERR_IO,
	NFSERR_MOVED,
	NFSERR_NAMETOL,
	NFSERR_NOFILEHANDLE,
	NFSERR_NOSPC,
	NFSERR_NOTDIR,
	NFSERR_PERM,
	NFSERR_RESOURCE,
	NFSERR_ROFS,
	NFSERR_SERVERFAULT,
	NFSERR_STALE,
	0,
};

static short nfsv4err_delegpurge[] = {
	NFSERR_SERVERFAULT,
	NFSERR_BADXDR,
	NFSERR_NOTSUPP,
	NFSERR_LEASEMOVED,
	NFSERR_MOVED,
	NFSERR_RESOURCE,
	NFSERR_SERVERFAULT,
	NFSERR_STALECLIENTID,
	0,
};

static short nfsv4err_delegreturn[] = {
	NFSERR_SERVERFAULT,
	NFSERR_ADMINREVOKED,
	NFSERR_BADSTATEID,
	NFSERR_BADXDR,
	NFSERR_EXPIRED,
	NFSERR_INVAL,
	NFSERR_LEASEMOVED,
	NFSERR_MOVED,
	NFSERR_NOFILEHANDLE,
	NFSERR_NOTSUPP,
	NFSERR_OLDSTATEID,
	NFSERR_RESOURCE,
	NFSERR_SERVERFAULT,
	NFSERR_STALE,
	NFSERR_STALESTATEID,
	0,
};

static short nfsv4err_getattr[] = {
	NFSERR_IO,
	NFSERR_ACCES,
	NFSERR_BADHANDLE,
	NFSERR_BADXDR,
	NFSERR_DELAY,
	NFSERR_FHEXPIRED,
	NFSERR_INVAL,
	NFSERR_IO,
	NFSERR_MOVED,
	NFSERR_NOFILEHANDLE,
	NFSERR_RESOURCE,
	NFSERR_SERVERFAULT,
	NFSERR_STALE,
	0,
};

static short nfsv4err_getfh[] = {
	NFSERR_BADHANDLE,
	NFSERR_BADHANDLE,
	NFSERR_FHEXPIRED,
	NFSERR_MOVED,
	NFSERR_NOFILEHANDLE,
	NFSERR_RESOURCE,
	NFSERR_SERVERFAULT,
	NFSERR_STALE,
	0,
};

static short nfsv4err_link[] = {
	NFSERR_IO,
	NFSERR_ACCES,
	NFSERR_BADCHAR,
	NFSERR_BADHANDLE,
	NFSERR_BADNAME,
	NFSERR_BADXDR,
	NFSERR_DELAY,
	NFSERR_DQUOT,
	NFSERR_EXIST,
	NFSERR_FHEXPIRED,
	NFSERR_FILEOPEN,
	NFSERR_INVAL,
	NFSERR_IO,
	NFSERR_ISDIR,
	NFSERR_MLINK,
	NFSERR_MOVED,
	NFSERR_NAMETOL,
	NFSERR_NOENT,
	NFSERR_NOFILEHANDLE,
	NFSERR_NOSPC,
	NFSERR_NOTDIR,
	NFSERR_NOTSUPP,
	NFSERR_RESOURCE,
	NFSERR_ROFS,
	NFSERR_SERVERFAULT,
	NFSERR_STALE,
	NFSERR_WRONGSEC,
	NFSERR_XDEV,
	0,
};

static short nfsv4err_lock[] = {
	NFSERR_SERVERFAULT,
	NFSERR_ACCES,
	NFSERR_ADMINREVOKED,
	NFSERR_BADHANDLE,
	NFSERR_BADRANGE,
	NFSERR_BADSEQID,
	NFSERR_BADSTATEID,
	NFSERR_BADXDR,
	NFSERR_DEADLOCK,
	NFSERR_DELAY,
	NFSERR_DENIED,
	NFSERR_EXPIRED,
	NFSERR_FHEXPIRED,
	NFSERR_GRACE,
	NFSERR_INVAL,
	NFSERR_ISDIR,
	NFSERR_LEASEMOVED,
	NFSERR_LOCKNOTSUPP,
	NFSERR_LOCKRANGE,
	NFSERR_MOVED,
	NFSERR_NOFILEHANDLE,
	NFSERR_NOGRACE,
	NFSERR_OLDSTATEID,
	NFSERR_OPENMODE,
	NFSERR_RECLAIMBAD,
	NFSERR_RECLAIMCONFLICT,
	NFSERR_RESOURCE,
	NFSERR_SERVERFAULT,
	NFSERR_STALE,
	NFSERR_STALECLIENTID,
	NFSERR_STALESTATEID,
	0,
};

static short nfsv4err_lockt[] = {
	NFSERR_SERVERFAULT,
	NFSERR_ACCES,
	NFSERR_BADHANDLE,
	NFSERR_BADRANGE,
	NFSERR_BADXDR,
	NFSERR_DELAY,
	NFSERR_DENIED,
	NFSERR_FHEXPIRED,
	NFSERR_GRACE,
	NFSERR_INVAL,
	NFSERR_ISDIR,
	NFSERR_LEASEMOVED,
	NFSERR_LOCKRANGE,
	NFSERR_MOVED,
	NFSERR_NOFILEHANDLE,
	NFSERR_RESOURCE,
	NFSERR_SERVERFAULT,
	NFSERR_STALE,
	NFSERR_STALECLIENTID,
	0,
};

static short nfsv4err_locku[] = {
	NFSERR_SERVERFAULT,
	NFSERR_ACCES,
	NFSERR_ADMINREVOKED,
	NFSERR_BADHANDLE,
	NFSERR_BADRANGE,
	NFSERR_BADSEQID,
	NFSERR_BADSTATEID,
	NFSERR_BADXDR,
	NFSERR_EXPIRED,
	NFSERR_FHEXPIRED,
	NFSERR_GRACE,
	NFSERR_INVAL,
	NFSERR_ISDIR,
	NFSERR_LEASEMOVED,
	NFSERR_LOCKRANGE,
	NFSERR_MOVED,
	NFSERR_NOFILEHANDLE,
	NFSERR_OLDSTATEID,
	NFSERR_RESOURCE,
	NFSERR_SERVERFAULT,
	NFSERR_STALE,
	NFSERR_STALESTATEID,
	0,
};

static short nfsv4err_lookup[] = {
	NFSERR_IO,
	NFSERR_ACCES,
	NFSERR_BADCHAR,
	NFSERR_BADHANDLE,
	NFSERR_BADNAME,
	NFSERR_BADXDR,
	NFSERR_FHEXPIRED,
	NFSERR_INVAL,
	NFSERR_IO,
	NFSERR_MOVED,
	NFSERR_NAMETOL,
	NFSERR_NOENT,
	NFSERR_NOFILEHANDLE,
	NFSERR_NOTDIR,
	NFSERR_RESOURCE,
	NFSERR_SERVERFAULT,
	NFSERR_STALE,
	NFSERR_SYMLINK,
	NFSERR_WRONGSEC,
	0,
};

static short nfsv4err_lookupp[] = {
	NFSERR_IO,
	NFSERR_ACCES,
	NFSERR_BADHANDLE,
	NFSERR_FHEXPIRED,
	NFSERR_IO,
	NFSERR_MOVED,
	NFSERR_NOENT,
	NFSERR_NOFILEHANDLE,
	NFSERR_NOTDIR,
	NFSERR_RESOURCE,
	NFSERR_SERVERFAULT,
	NFSERR_STALE,
	0,
};

static short nfsv4err_nverify[] = {
	NFSERR_IO,
	NFSERR_ACCES,
	NFSERR_ATTRNOTSUPP,
	NFSERR_BADCHAR,
	NFSERR_BADHANDLE,
	NFSERR_BADXDR,
	NFSERR_DELAY,
	NFSERR_FHEXPIRED,
	NFSERR_INVAL,
	NFSERR_IO,
	NFSERR_MOVED,
	NFSERR_NOFILEHANDLE,
	NFSERR_RESOURCE,
	NFSERR_SAME,
	NFSERR_SERVERFAULT,
	NFSERR_STALE,
	0,
};

static short nfsv4err_open[] = {
	NFSERR_IO,
	NFSERR_ACCES,
	NFSERR_ADMINREVOKED,
	NFSERR_ATTRNOTSUPP,
	NFSERR_BADCHAR,
	NFSERR_BADHANDLE,
	NFSERR_BADNAME,
	NFSERR_BADOWNER,
	NFSERR_BADSEQID,
	NFSERR_BADXDR,
	NFSERR_DELAY,
	NFSERR_DQUOT,
	NFSERR_EXIST,
	NFSERR_EXPIRED,
	NFSERR_FHEXPIRED,
	NFSERR_GRACE,
	NFSERR_IO,
	NFSERR_INVAL,
	NFSERR_ISDIR,
	NFSERR_LEASEMOVED,
	NFSERR_MOVED,
	NFSERR_NAMETOL,
	NFSERR_NOENT,
	NFSERR_NOFILEHANDLE,
	NFSERR_NOGRACE,
	NFSERR_NOSPC,
	NFSERR_NOTDIR,
	NFSERR_NOTSUPP,
	NFSERR_PERM,
	NFSERR_RECLAIMBAD,
	NFSERR_RECLAIMCONFLICT,
	NFSERR_RESOURCE,
	NFSERR_ROFS,
	NFSERR_SERVERFAULT,
	NFSERR_SHAREDENIED,
	NFSERR_STALE,
	NFSERR_STALECLIENTID,
	NFSERR_SYMLINK,
	NFSERR_WRONGSEC,
	0,
};

static short nfsv4err_openattr[] = {
	NFSERR_IO,
	NFSERR_ACCES,
	NFSERR_BADHANDLE,
	NFSERR_BADXDR,
	NFSERR_DELAY,
	NFSERR_DQUOT,
	NFSERR_FHEXPIRED,
	NFSERR_IO,
	NFSERR_MOVED,
	NFSERR_NOENT,
	NFSERR_NOFILEHANDLE,
	NFSERR_NOSPC,
	NFSERR_NOTSUPP,
	NFSERR_RESOURCE,
	NFSERR_ROFS,
	NFSERR_SERVERFAULT,
	NFSERR_STALE,
	0,
};

static short nfsv4err_openconfirm[] = {
	NFSERR_SERVERFAULT,
	NFSERR_ADMINREVOKED,
	NFSERR_BADHANDLE,
	NFSERR_BADSEQID,
	NFSERR_BADSTATEID,
	NFSERR_BADXDR,
	NFSERR_EXPIRED,
	NFSERR_FHEXPIRED,
	NFSERR_INVAL,
	NFSERR_ISDIR,
	NFSERR_MOVED,
	NFSERR_NOFILEHANDLE,
	NFSERR_OLDSTATEID,
	NFSERR_RESOURCE,
	NFSERR_SERVERFAULT,
	NFSERR_STALE,
	NFSERR_STALESTATEID,
	0,
};

static short nfsv4err_opendowngrade[] = {
	NFSERR_SERVERFAULT,
	NFSERR_ADMINREVOKED,
	NFSERR_BADHANDLE,
	NFSERR_BADSEQID,
	NFSERR_BADSTATEID,
	NFSERR_BADXDR,
	NFSERR_EXPIRED,
	NFSERR_FHEXPIRED,
	NFSERR_INVAL,
	NFSERR_MOVED,
	NFSERR_NOFILEHANDLE,
	NFSERR_OLDSTATEID,
	NFSERR_RESOURCE,
	NFSERR_SERVERFAULT,
	NFSERR_STALE,
	NFSERR_STALESTATEID,
	0,
};

static short nfsv4err_putfh[] = {
	NFSERR_SERVERFAULT,
	NFSERR_BADHANDLE,
	NFSERR_BADXDR,
	NFSERR_FHEXPIRED,
	NFSERR_MOVED,
	NFSERR_RESOURCE,
	NFSERR_SERVERFAULT,
	NFSERR_STALE,
	NFSERR_WRONGSEC,
	0,
};

static short nfsv4err_putpubfh[] = {
	NFSERR_SERVERFAULT,
	NFSERR_RESOURCE,
	NFSERR_SERVERFAULT,
	NFSERR_WRONGSEC,
	0,
};

static short nfsv4err_putrootfh[] = {
	NFSERR_SERVERFAULT,
	NFSERR_RESOURCE,
	NFSERR_SERVERFAULT,
	NFSERR_WRONGSEC,
	0,
};

static short nfsv4err_read[] = {
	NFSERR_IO,
	NFSERR_ACCES,
	NFSERR_ADMINREVOKED,
	NFSERR_BADHANDLE,
	NFSERR_BADSTATEID,
	NFSERR_BADXDR,
	NFSERR_DELAY,
	NFSERR_EXPIRED,
	NFSERR_FHEXPIRED,
	NFSERR_GRACE,
	NFSERR_IO,
	NFSERR_INVAL,
	NFSERR_ISDIR,
	NFSERR_LEASEMOVED,
	NFSERR_LOCKED,
	NFSERR_MOVED,
	NFSERR_NOFILEHANDLE,
	NFSERR_NXIO,
	NFSERR_OLDSTATEID,
	NFSERR_OPENMODE,
	NFSERR_RESOURCE,
	NFSERR_SERVERFAULT,
	NFSERR_STALE,
	NFSERR_STALESTATEID,
	0,
};

static short nfsv4err_readdir[] = {
	NFSERR_IO,
	NFSERR_ACCES,
	NFSERR_BADHANDLE,
	NFSERR_BAD_COOKIE,
	NFSERR_BADXDR,
	NFSERR_DELAY,
	NFSERR_FHEXPIRED,
	NFSERR_INVAL,
	NFSERR_IO,
	NFSERR_MOVED,
	NFSERR_NOFILEHANDLE,
	NFSERR_NOTDIR,
	NFSERR_NOTSAME,
	NFSERR_RESOURCE,
	NFSERR_SERVERFAULT,
	NFSERR_STALE,
	NFSERR_TOOSMALL,
	0,
};

static short nfsv4err_readlink[] = {
	NFSERR_IO,
	NFSERR_ACCES,
	NFSERR_BADHANDLE,
	NFSERR_DELAY,
	NFSERR_FHEXPIRED,
	NFSERR_INVAL,
	NFSERR_IO,
	NFSERR_ISDIR,
	NFSERR_MOVED,
	NFSERR_NOFILEHANDLE,
	NFSERR_NOTSUPP,
	NFSERR_RESOURCE,
	NFSERR_SERVERFAULT,
	NFSERR_STALE,
	0,
};

static short nfsv4err_remove[] = {
	NFSERR_IO,
	NFSERR_ACCES,
	NFSERR_BADCHAR,
	NFSERR_BADHANDLE,
	NFSERR_BADNAME,
	NFSERR_BADXDR,
	NFSERR_DELAY,
	NFSERR_FHEXPIRED,
	NFSERR_FILEOPEN,
	NFSERR_INVAL,
	NFSERR_IO,
	NFSERR_MOVED,
	NFSERR_NAMETOL,
	NFSERR_NOENT,
	NFSERR_NOFILEHANDLE,
	NFSERR_NOTDIR,
	NFSERR_NOTEMPTY,
	NFSERR_RESOURCE,
	NFSERR_ROFS,
	NFSERR_SERVERFAULT,
	NFSERR_STALE,
	0,
};

static short nfsv4err_rename[] = {
	NFSERR_IO,
	NFSERR_ACCES,
	NFSERR_BADCHAR,
	NFSERR_BADHANDLE,
	NFSERR_BADNAME,
	NFSERR_BADXDR,
	NFSERR_DELAY,
	NFSERR_DQUOT,
	NFSERR_EXIST,
	NFSERR_FHEXPIRED,
	NFSERR_FILEOPEN,
	NFSERR_INVAL,
	NFSERR_IO,
	NFSERR_MOVED,
	NFSERR_NAMETOL,
	NFSERR_NOENT,
	NFSERR_NOFILEHANDLE,
	NFSERR_NOSPC,
	NFSERR_NOTDIR,
	NFSERR_NOTEMPTY,
	NFSERR_RESOURCE,
	NFSERR_ROFS,
	NFSERR_SERVERFAULT,
	NFSERR_STALE,
	NFSERR_WRONGSEC,
	NFSERR_XDEV,
	0,
};

static short nfsv4err_renew[] = {
	NFSERR_SERVERFAULT,
	NFSERR_ACCES,
	NFSERR_ADMINREVOKED,
	NFSERR_BADXDR,
	NFSERR_CBPATHDOWN,
	NFSERR_EXPIRED,
	NFSERR_LEASEMOVED,
	NFSERR_RESOURCE,
	NFSERR_SERVERFAULT,
	NFSERR_STALECLIENTID,
	0,
};

static short nfsv4err_restorefh[] = {
	NFSERR_SERVERFAULT,
	NFSERR_BADHANDLE,
	NFSERR_FHEXPIRED,
	NFSERR_MOVED,
	NFSERR_RESOURCE,
	NFSERR_RESTOREFH,
	NFSERR_SERVERFAULT,
	NFSERR_STALE,
	NFSERR_WRONGSEC,
	0,
};

static short nfsv4err_savefh[] = {
	NFSERR_SERVERFAULT,
	NFSERR_BADHANDLE,
	NFSERR_FHEXPIRED,
	NFSERR_MOVED,
	NFSERR_NOFILEHANDLE,
	NFSERR_RESOURCE,
	NFSERR_SERVERFAULT,
	NFSERR_STALE,
	0,
};

static short nfsv4err_secinfo[] = {
	NFSERR_SERVERFAULT,
	NFSERR_ACCES,
	NFSERR_BADCHAR,
	NFSERR_BADHANDLE,
	NFSERR_BADNAME,
	NFSERR_BADXDR,
	NFSERR_FHEXPIRED,
	NFSERR_INVAL,
	NFSERR_MOVED,
	NFSERR_NAMETOL,
	NFSERR_NOENT,
	NFSERR_NOFILEHANDLE,
	NFSERR_NOTDIR,
	NFSERR_RESOURCE,
	NFSERR_SERVERFAULT,
	NFSERR_STALE,
	0,
};

static short nfsv4err_setattr[] = {
	NFSERR_IO,
	NFSERR_ACCES,
	NFSERR_ADMINREVOKED,
	NFSERR_ATTRNOTSUPP,
	NFSERR_BADCHAR,
	NFSERR_BADHANDLE,
	NFSERR_BADOWNER,
	NFSERR_BADSTATEID,
	NFSERR_BADXDR,
	NFSERR_DELAY,
	NFSERR_DQUOT,
	NFSERR_EXPIRED,
	NFSERR_FBIG,
	NFSERR_FHEXPIRED,
	NFSERR_GRACE,
	NFSERR_INVAL,
	NFSERR_IO,
	NFSERR_ISDIR,
	NFSERR_LOCKED,
	NFSERR_MOVED,
	NFSERR_NOFILEHANDLE,
	NFSERR_NOSPC,
	NFSERR_OLDSTATEID,
	NFSERR_OPENMODE,
	NFSERR_PERM,
	NFSERR_RESOURCE,
	NFSERR_ROFS,
	NFSERR_SERVERFAULT,
	NFSERR_STALE,
	NFSERR_STALESTATEID,
	0,
};

static short nfsv4err_setclientid[] = {
	NFSERR_SERVERFAULT,
	NFSERR_BADXDR,
	NFSERR_CLIDINUSE,
	NFSERR_INVAL,
	NFSERR_RESOURCE,
	NFSERR_SERVERFAULT,
	NFSERR_WRONGSEC,
	0,
};

static short nfsv4err_setclientidconfirm[] = {
	NFSERR_SERVERFAULT,
	NFSERR_BADXDR,
	NFSERR_CLIDINUSE,
	NFSERR_RESOURCE,
	NFSERR_SERVERFAULT,
	NFSERR_STALECLIENTID,
	0,
};

static short nfsv4err_verify[] = {
	NFSERR_SERVERFAULT,
	NFSERR_ACCES,
	NFSERR_ATTRNOTSUPP,
	NFSERR_BADCHAR,
	NFSERR_BADHANDLE,
	NFSERR_BADXDR,
	NFSERR_DELAY,
	NFSERR_FHEXPIRED,
	NFSERR_INVAL,
	NFSERR_MOVED,
	NFSERR_NOFILEHANDLE,
	NFSERR_NOTSAME,
	NFSERR_RESOURCE,
	NFSERR_SERVERFAULT,
	NFSERR_STALE,
	0,
};

static short nfsv4err_write[] = {
	NFSERR_IO,
	NFSERR_ACCES,
	NFSERR_ADMINREVOKED,
	NFSERR_BADHANDLE,
	NFSERR_BADSTATEID,
	NFSERR_BADXDR,
	NFSERR_DELAY,
	NFSERR_DQUOT,
	NFSERR_EXPIRED,
	NFSERR_FBIG,
	NFSERR_FHEXPIRED,
	NFSERR_GRACE,
	NFSERR_INVAL,
	NFSERR_IO,
	NFSERR_ISDIR,
	NFSERR_LEASEMOVED,
	NFSERR_LOCKED,
	NFSERR_MOVED,
	NFSERR_NOFILEHANDLE,
	NFSERR_NOSPC,
	NFSERR_NXIO,
	NFSERR_OLDSTATEID,
	NFSERR_OPENMODE,
	NFSERR_RESOURCE,
	NFSERR_ROFS,
	NFSERR_SERVERFAULT,
	NFSERR_STALE,
	NFSERR_STALESTATEID,
	0,
};

static short nfsv4err_releaselockowner[] = {
	NFSERR_SERVERFAULT,
	NFSERR_ADMINREVOKED,
	NFSERR_BADXDR,
	NFSERR_EXPIRED,
	NFSERR_LEASEMOVED,
	NFSERR_LOCKSHELD,
	NFSERR_RESOURCE,
	NFSERR_SERVERFAULT,
	NFSERR_STALECLIENTID,
	0,
};

static short *nfsrv_v4errmap[] = {
	nfsv4err_null,
	nfsv4err_null,
	nfsv4err_null,
	nfsv4err_access,
	nfsv4err_close,
	nfsv4err_commit,
	nfsv4err_create,
	nfsv4err_delegpurge,
	nfsv4err_delegreturn,
	nfsv4err_getattr,
	nfsv4err_getfh,
	nfsv4err_link,
	nfsv4err_lock,
	nfsv4err_lockt,
	nfsv4err_locku,
	nfsv4err_lookup,
	nfsv4err_lookupp,
	nfsv4err_nverify,
	nfsv4err_open,
	nfsv4err_openattr,
	nfsv4err_openconfirm,
	nfsv4err_opendowngrade,
	nfsv4err_putfh,
	nfsv4err_putpubfh,
	nfsv4err_putrootfh,
	nfsv4err_read,
	nfsv4err_readdir,
	nfsv4err_readlink,
	nfsv4err_remove,
	nfsv4err_rename,
	nfsv4err_renew,
	nfsv4err_restorefh,
	nfsv4err_savefh,
	nfsv4err_secinfo,
	nfsv4err_setattr,
	nfsv4err_setclientid,
	nfsv4err_setclientidconfirm,
	nfsv4err_verify,
	nfsv4err_write,
	nfsv4err_releaselockowner,
};

/*
 * Trim tlen bytes off the end of the mbuf list and then ensure
 * the end of the last mbuf is nul filled to a long boundary,
 * as indicated by the value of "nul".
 * Return the last mbuf in the updated list and free and mbufs
 * that follow it in the original list.
 * This is somewhat different than the old nfsrv_adj() with
 * support for ext_pgs mbufs.  It frees the remaining mbufs
 * instead of setting them 0 length, since lists of ext_pgs
 * mbufs are all expected to be non-empty.
 */
struct mbuf *
nfsrv_adj(struct mbuf *mp, int len, int nul)
{
	struct mbuf *m, *m2;
	vm_page_t pg;
	int i, lastlen, pgno, plen, tlen, trim;
	uint16_t off;
	char *cp;

	/*
	 * Find the last mbuf after adjustment and
	 * how much it needs to be adjusted by.
	 */
	tlen = 0;
	m = mp;
	for (;;) {
		tlen += m->m_len;
		if (m->m_next == NULL)
			break;
		m = m->m_next;
	}
	/* m is now the last mbuf and tlen the total length. */

	if (len >= m->m_len) {
		/* Need to trim away the last mbuf(s). */
		i = tlen - len;
		m = mp;
		for (;;) {
			if (m->m_len >= i)
				break;
			i -= m->m_len;
			m = m->m_next;
		}
		lastlen = i;
	} else
		lastlen = m->m_len - len;

	/*
	 * m is now the last mbuf after trimming and its length needs to
	 * be lastlen.
	 * Adjust the last mbuf and set cp to point to where nuls must be
	 * written.
	 */
	if ((m->m_flags & M_EXTPG) != 0) {
		pgno = m->m_epg_npgs - 1;
		off = (pgno == 0) ? m->m_epg_1st_off : 0;
		plen = m_epg_pagelen(m, pgno, off);
		if (m->m_len > lastlen) {
			/* Trim this mbuf. */
			trim = m->m_len - lastlen;
			while (trim >= plen) {
				KASSERT(pgno > 0,
				    ("nfsrv_adj: freeing page 0"));
				/* Free page. */
				pg = PHYS_TO_VM_PAGE(m->m_epg_pa[pgno]);
				vm_page_unwire_noq(pg);
				vm_page_free(pg);
				trim -= plen;
				m->m_epg_npgs--;
				pgno--;
				off = (pgno == 0) ? m->m_epg_1st_off : 0;
				plen = m_epg_pagelen(m, pgno, off);
			}
			plen -= trim;
			m->m_epg_last_len = plen;
			m->m_len = lastlen;
		}
		cp = (char *)(void *)PHYS_TO_DMAP(m->m_epg_pa[pgno]);
		cp += off + plen - nul;
	} else {
		m->m_len = lastlen;
		cp = mtod(m, char *) + m->m_len - nul;
	}

	/* Write the nul bytes. */
	for (i = 0; i < nul; i++)
		*cp++ = '\0';

	/* Free up any mbufs past "m". */
	m2 = m->m_next;
	m->m_next = NULL;
	if (m2 != NULL)
		m_freem(m2);
	return (m);
}

/*
 * Make these functions instead of macros, so that the kernel text size
 * doesn't get too big...
 */
void
nfsrv_wcc(struct nfsrv_descript *nd, int before_ret,
    struct nfsvattr *before_nvap, int after_ret, struct nfsvattr *after_nvap)
{
	u_int32_t *tl;

	if (before_ret) {
		NFSM_BUILD(tl, u_int32_t *, NFSX_UNSIGNED);
		*tl = newnfs_false;
	} else {
		NFSM_BUILD(tl, u_int32_t *, 7 * NFSX_UNSIGNED);
		*tl++ = newnfs_true;
		txdr_hyper(before_nvap->na_size, tl);
		tl += 2;
		txdr_nfsv3time(&(before_nvap->na_mtime), tl);
		tl += 2;
		txdr_nfsv3time(&(before_nvap->na_ctime), tl);
	}
	nfsrv_postopattr(nd, after_ret, after_nvap);
}

void
nfsrv_postopattr(struct nfsrv_descript *nd, int after_ret,
    struct nfsvattr *after_nvap)
{
	u_int32_t *tl;

	NFSM_BUILD(tl, u_int32_t *, NFSX_UNSIGNED);
	if (after_ret)
		*tl = newnfs_false;
	else {
		*tl = newnfs_true;
		nfsrv_fillattr(nd, after_nvap);
	}
}

/*
 * Fill in file attributes for V2 and 3. For V4, call a separate
 * routine that sifts through all the attribute bits.
 */
void
nfsrv_fillattr(struct nfsrv_descript *nd, struct nfsvattr *nvap)
{
	struct nfs_fattr *fp;
	int fattr_size;

	/*
	 * Build space for the attribute structure.
	 */
	if (nd->nd_flag & ND_NFSV3)
		fattr_size = NFSX_V3FATTR;
	else
		fattr_size = NFSX_V2FATTR;
	NFSM_BUILD(fp, struct nfs_fattr *, fattr_size);

	/*
	 * Now just fill it all in.
	 */
	fp->fa_nlink = txdr_unsigned(nvap->na_nlink);
	fp->fa_uid = txdr_unsigned(nvap->na_uid);
	fp->fa_gid = txdr_unsigned(nvap->na_gid);
	if (nd->nd_flag & ND_NFSV3) {
		fp->fa_type = vtonfsv34_type(nvap->na_type);
		fp->fa_mode = vtonfsv34_mode(nvap->na_mode);
		txdr_hyper(nvap->na_size, (uint32_t*)&fp->fa3_size);
		txdr_hyper(nvap->na_bytes, (uint32_t*)&fp->fa3_used);
		fp->fa3_rdev.specdata1 = txdr_unsigned(NFSMAJOR(nvap->na_rdev));
		fp->fa3_rdev.specdata2 = txdr_unsigned(NFSMINOR(nvap->na_rdev));
		fp->fa3_fsid.nfsuquad[0] = 0;
		fp->fa3_fsid.nfsuquad[1] = txdr_unsigned(nvap->na_fsid);
		txdr_hyper(nvap->na_fileid, (uint32_t*)&fp->fa3_fileid);
		txdr_nfsv3time(&nvap->na_atime, &fp->fa3_atime);
		txdr_nfsv3time(&nvap->na_mtime, &fp->fa3_mtime);
		txdr_nfsv3time(&nvap->na_ctime, &fp->fa3_ctime);
	} else {
		fp->fa_type = vtonfsv2_type(nvap->na_type);
		fp->fa_mode = vtonfsv2_mode(nvap->na_type, nvap->na_mode);
		fp->fa2_size = txdr_unsigned(nvap->na_size);
		fp->fa2_blocksize = txdr_unsigned(nvap->na_blocksize);
		if (nvap->na_type == VFIFO)
			fp->fa2_rdev = 0xffffffff;
		else
			fp->fa2_rdev = txdr_unsigned(nvap->na_rdev);
		fp->fa2_blocks = txdr_unsigned(nvap->na_bytes / NFS_FABLKSIZE);
		fp->fa2_fsid = txdr_unsigned(nvap->na_fsid);
		fp->fa2_fileid = txdr_unsigned(nvap->na_fileid);
		txdr_nfsv2time(&nvap->na_atime, &fp->fa2_atime);
		txdr_nfsv2time(&nvap->na_mtime, &fp->fa2_mtime);
		txdr_nfsv2time(&nvap->na_ctime, &fp->fa2_ctime);
	}
}

/*
 * This function gets a file handle out of an mbuf list.
 * It returns 0 for success, EBADRPC otherwise.
 * If sets the third flagp argument to 1 if the file handle is
 * the public file handle.
 * For NFSv4, if the length is incorrect, set nd_repstat == NFSERR_BADHANDLE
 */
int
nfsrv_mtofh(struct nfsrv_descript *nd, struct nfsrvfh *fhp)
{
	u_int32_t *tl;
	int error = 0, len, copylen;

	if (nd->nd_flag & (ND_NFSV3 | ND_NFSV4)) {
		NFSM_DISSECT(tl, u_int32_t *, NFSX_UNSIGNED);
		len = fxdr_unsigned(int, *tl);
		if (len == 0 && nfs_pubfhset && (nd->nd_flag & ND_NFSV3) &&
		    nd->nd_procnum == NFSPROC_LOOKUP) {
			nd->nd_flag |= ND_PUBLOOKUP;
			goto nfsmout;
		}
		copylen = len;

		/* If len == NFSX_V4PNFSFH the RPC is a pNFS DS one. */
		if (len == NFSX_V4PNFSFH && (nd->nd_flag & ND_NFSV41) != 0) {
			copylen = NFSX_MYFH;
			len = NFSM_RNDUP(len);
			nd->nd_flag |= ND_DSSERVER;
		} else if (len < NFSRV_MINFH || len > NFSRV_MAXFH) {
			if (nd->nd_flag & ND_NFSV4) {
			    if (len > 0 && len <= NFSX_V4FHMAX) {
				error = nfsm_advance(nd, NFSM_RNDUP(len), -1);
				if (error)
					goto nfsmout;
				nd->nd_repstat = NFSERR_BADHANDLE;
				goto nfsmout;
			    } else {
				    error = EBADRPC;
				    goto nfsmout;
			    }
			} else {
				error = EBADRPC;
				goto nfsmout;
			}
		}
	} else {
		/*
		 * For NFSv2, the file handle is always 32 bytes on the
		 * wire, but this server only cares about the first
		 * NFSRV_MAXFH bytes.
		 */
		len = NFSX_V2FH;
		copylen = NFSRV_MAXFH;
	}
	NFSM_DISSECT(tl, u_int32_t *, len);
	if ((nd->nd_flag & ND_NFSV2) && nfs_pubfhset &&
	    nd->nd_procnum == NFSPROC_LOOKUP &&
	    !NFSBCMP((caddr_t)tl, nfs_v2pubfh, NFSX_V2FH)) {
		nd->nd_flag |= ND_PUBLOOKUP;
		goto nfsmout;
	}
	NFSBCOPY(tl, (caddr_t)fhp->nfsrvfh_data, copylen);
	fhp->nfsrvfh_len = copylen;
nfsmout:
	NFSEXITCODE2(error, nd);
	return (error);
}

/*
 * Map errnos to NFS error numbers. For Version 3 and 4 also filter out error
 * numbers not specified for the associated procedure.
 * NFSPROC_NOOP is a special case, where the high order bits of nd_repstat
 * should be cleared. NFSPROC_NOOP is used to return errors when a valid
 * RPC procedure is not involved.
 * Returns the error number in XDR.
 */
int
nfsd_errmap(struct nfsrv_descript *nd)
{
	short *defaulterrp, *errp;

	if (!nd->nd_repstat)
		return (0);
	if ((nd->nd_repstat & NFSERR_AUTHERR) != 0)
		return (txdr_unsigned(NFSERR_ACCES));
	if (nd->nd_flag & (ND_NFSV3 | ND_NFSV4)) {
		if (nd->nd_procnum == NFSPROC_NOOP)
			return (txdr_unsigned(nd->nd_repstat & 0xffff));
		if (nd->nd_flag & ND_NFSV3)
		    errp = defaulterrp = nfsrv_v3errmap[nd->nd_procnum];
		else if (nd->nd_repstat == EBADRPC)
			return (txdr_unsigned(NFSERR_BADXDR));
		else if (nd->nd_repstat == NFSERR_MINORVERMISMATCH ||
			 nd->nd_repstat == NFSERR_OPILLEGAL)
			return (txdr_unsigned(nd->nd_repstat));
		else if (nd->nd_repstat == NFSERR_REPLYFROMCACHE)
			return (txdr_unsigned(NFSERR_IO));
		else if ((nd->nd_flag & ND_NFSV41) != 0) {
			if (nd->nd_repstat == EOPNOTSUPP)
				nd->nd_repstat = NFSERR_NOTSUPP;
			nd->nd_repstat = nfsrv_isannfserr(nd->nd_repstat);
			return (txdr_unsigned(nd->nd_repstat));
		} else
		    errp = defaulterrp = nfsrv_v4errmap[nd->nd_procnum];
		while (*++errp)
			if (*errp == nd->nd_repstat)
				return (txdr_unsigned(nd->nd_repstat));
		return (txdr_unsigned(*defaulterrp));
	}
	if (nd->nd_repstat <= NFSERR_REMOTE)
		return (txdr_unsigned(nfsrv_v2errmap[nd->nd_repstat - 1]));
	return (txdr_unsigned(NFSERR_IO));
}

/*
 * Check to see if the error is a valid NFS one. If not, replace it with
 * NFSERR_IO.
 */
static u_int32_t
nfsrv_isannfserr(u_int32_t errval)
{

	if (errval == NFSERR_OK)
		return (errval);
	if (errval >= NFSERR_BADHANDLE && errval <= NFSERR_MAXERRVAL)
		return (errval);
	if (errval > 0 && errval <= NFSERR_REMOTE)
		return (nfsrv_v2errmap[errval - 1]);
	return (NFSERR_IO);
}

/*
 * Check to see if setting a uid/gid is permitted when creating a new
 * file object. (Called when uid and/or gid is specified in the
 * settable attributes for V4.
 */
int
nfsrv_checkuidgid(struct nfsrv_descript *nd, struct nfsvattr *nvap)
{
	int error = 0;

	/*
	 * If not setting either uid nor gid, it's OK.
	 */
	if (NFSVNO_NOTSETUID(nvap) && NFSVNO_NOTSETGID(nvap))
		goto out;
	if ((NFSVNO_ISSETUID(nvap) &&
	     nvap->na_uid == NFSD_VNET(nfsrv_defaultuid) &&
             enable_nobodycheck == 1) ||
	    (NFSVNO_ISSETGID(nvap) &&
	     nvap->na_gid == NFSD_VNET(nfsrv_defaultgid) &&
             enable_nogroupcheck == 1)) {
		error = NFSERR_BADOWNER;
		goto out;
	}
	if (nd->nd_cred->cr_uid == 0)
		goto out;
	if ((NFSVNO_ISSETUID(nvap) && nvap->na_uid != nd->nd_cred->cr_uid) ||
	    (NFSVNO_ISSETGID(nvap) &&
	    !groupmember(nvap->na_gid, nd->nd_cred)))
		error = NFSERR_PERM;

out:
	NFSEXITCODE2(error, nd);
	return (error);
}

/*
 * and this routine fixes up the settable attributes for V4 if allowed
 * by nfsrv_checkuidgid().
 */
void
nfsrv_fixattr(struct nfsrv_descript *nd, vnode_t vp,
    struct nfsvattr *nvap, NFSACL_T *aclp, NFSPROC_T *p, nfsattrbit_t *attrbitp,
    struct nfsexstuff *exp)
{
	int change = 0;
	struct nfsvattr nva;
	uid_t tuid;
	int error;
	nfsattrbit_t nattrbits;

	/*
	 * Maybe this should be done for V2 and 3 but it never has been
	 * and nobody seems to be upset, so I think it's best not to change
	 * the V2 and 3 semantics.
	 */
	if ((nd->nd_flag & ND_NFSV4) == 0)
		goto out;
	NFSVNO_ATTRINIT(&nva);
	NFSZERO_ATTRBIT(&nattrbits);
	tuid = nd->nd_cred->cr_uid;
	if (NFSISSET_ATTRBIT(attrbitp, NFSATTRBIT_OWNER) &&
	    NFSVNO_ISSETUID(nvap) &&
	    nvap->na_uid != nd->nd_cred->cr_uid) {
		if (nd->nd_cred->cr_uid == 0) {
			nva.na_uid = nvap->na_uid;
			change++;
			NFSSETBIT_ATTRBIT(&nattrbits, NFSATTRBIT_OWNER);
		} else {
			NFSCLRBIT_ATTRBIT(attrbitp, NFSATTRBIT_OWNER);
		}
	}
	if (NFSISSET_ATTRBIT(attrbitp, NFSATTRBIT_TIMEACCESSSET) &&
	    NFSVNO_ISSETATIME(nvap)) {
		nva.na_atime = nvap->na_atime;
		change++;
		NFSSETBIT_ATTRBIT(&nattrbits, NFSATTRBIT_TIMEACCESSSET);
	}
	if (NFSISSET_ATTRBIT(attrbitp, NFSATTRBIT_TIMEMODIFYSET) &&
	    NFSVNO_ISSETMTIME(nvap)) {
		nva.na_mtime = nvap->na_mtime;
		change++;
		NFSSETBIT_ATTRBIT(&nattrbits, NFSATTRBIT_TIMEMODIFYSET);
	}
	if (NFSISSET_ATTRBIT(attrbitp, NFSATTRBIT_OWNERGROUP) &&
	    NFSVNO_ISSETGID(nvap)) {
		if (groupmember(nvap->na_gid, nd->nd_cred)) {
			nd->nd_cred->cr_uid = 0;
			nva.na_gid = nvap->na_gid;
			change++;
			NFSSETBIT_ATTRBIT(&nattrbits, NFSATTRBIT_OWNERGROUP);
		} else {
			NFSCLRBIT_ATTRBIT(attrbitp, NFSATTRBIT_OWNERGROUP);
		}
	}
	if (change) {
		error = nfsvno_setattr(vp, &nva, nd->nd_cred, p, exp);
		if (error) {
			NFSCLRALL_ATTRBIT(attrbitp, &nattrbits);
		}
	}
	if (NFSISSET_ATTRBIT(attrbitp, NFSATTRBIT_SIZE) &&
	    NFSVNO_ISSETSIZE(nvap) && nvap->na_size != (u_quad_t)0) {
		NFSCLRBIT_ATTRBIT(attrbitp, NFSATTRBIT_SIZE);
	}
#ifdef NFS4_ACL_EXTATTR_NAME
	if (NFSISSET_ATTRBIT(attrbitp, NFSATTRBIT_ACL) &&
	    nfsrv_useacl != 0 && aclp != NULL) {
		if (aclp->acl_cnt > 0) {
			error = nfsrv_setacl(vp, aclp, nd->nd_cred, p);
			if (error) {
				NFSCLRBIT_ATTRBIT(attrbitp, NFSATTRBIT_ACL);
			}
		}
	} else
#endif
	NFSCLRBIT_ATTRBIT(attrbitp, NFSATTRBIT_ACL);
	nd->nd_cred->cr_uid = tuid;

out:
	NFSEXITCODE2(0, nd);
}

/*
 * Translate an ASCII hex digit to it's binary value. Return -1 if the
 * char isn't a hex digit.
 */
static char
nfsrv_hexdigit(char c, int *err)
{

	*err = 0;
	if (c >= '0' && c <= '9')
		return (c - '0');
	if (c >= 'a' && c <= 'f')
		return (c - 'a' + ((char)10));
	if (c >= 'A' && c <= 'F')
		return (c - 'A' + ((char)10));
	/* Not valid ! */
	*err = 1;
	return (1);	/* BOGUS */
}

/*
 * Check to see if NFSERR_MOVED can be returned for this op. Return 1 iff
 * it can be.
 */
int
nfsrv_errmoved(int op)
{
	short *errp;

	errp = nfsrv_v4errmap[op];
	while (*errp != 0) {
		if (*errp == NFSERR_MOVED)
			return (1);
		errp++;
	}
	return (0);
}

/*
 * Fill in attributes for a Referral.
 * (Return the number of bytes of XDR created.)
 */
int
nfsrv_putreferralattr(struct nfsrv_descript *nd, nfsattrbit_t *retbitp,
    struct nfsreferral *refp, int getattr, int *reterrp)
{
	u_int32_t *tl, *retnump;
	u_char *cp, *cp2;
	int prefixnum, retnum = 0, i, len, bitpos, rderrbit = 0, nonrefbit = 0;
	int fslocationsbit = 0;
	nfsattrbit_t tmpbits, refbits;

	NFSREFERRAL_ATTRBIT(&refbits);
	if (getattr)
		NFSCLRBIT_ATTRBIT(&refbits, NFSATTRBIT_RDATTRERROR);
	else if (NFSISSET_ATTRBIT(retbitp, NFSATTRBIT_RDATTRERROR))
		rderrbit = 1;
	if (NFSISSET_ATTRBIT(retbitp, NFSATTRBIT_FSLOCATIONS))
		fslocationsbit = 1;

	/*
	 * Check for the case where unsupported referral attributes are
	 * requested.
	 */
	NFSSET_ATTRBIT(&tmpbits, retbitp);
	NFSCLRALL_ATTRBIT(&tmpbits, &refbits);
	if (NFSNONZERO_ATTRBIT(&tmpbits))
		nonrefbit = 1;

	if (nonrefbit && !fslocationsbit && (getattr || !rderrbit)) {
		*reterrp = NFSERR_MOVED;
		return (0);
	}

	/*
	 * Now we can fill in the attributes.
	 */
	NFSSET_ATTRBIT(&tmpbits, retbitp);
	NFSCLRNOT_ATTRBIT(&tmpbits, &refbits);

	/*
	 * Put out the attribute bitmap for the ones being filled in
	 * and get the field for the number of attributes returned.
	 */
	prefixnum = nfsrv_putattrbit(nd, &tmpbits);
	NFSM_BUILD(retnump, u_int32_t *, NFSX_UNSIGNED);
	prefixnum += NFSX_UNSIGNED;

	/*
	 * Now, loop around filling in the attributes for each bit set.
	 */
	for (bitpos = 0; bitpos < NFSATTRBIT_MAX; bitpos++) {
	    if (NFSISSET_ATTRBIT(&tmpbits, bitpos)) {
		switch (bitpos) {
		case NFSATTRBIT_TYPE:
			NFSM_BUILD(tl, u_int32_t *, NFSX_UNSIGNED);
			*tl = txdr_unsigned(NFDIR);
			retnum += NFSX_UNSIGNED;
			break;
		case NFSATTRBIT_FSID:
			NFSM_BUILD(tl, u_int32_t *, NFSX_V4FSID);
			*tl++ = 0;
			*tl++ = txdr_unsigned(NFSV4ROOT_FSID0);
			*tl++ = 0;
			*tl = txdr_unsigned(NFSV4ROOT_REFERRAL);
			retnum += NFSX_V4FSID;
			break;
		case NFSATTRBIT_RDATTRERROR:
			NFSM_BUILD(tl, u_int32_t *, NFSX_UNSIGNED);
			if (nonrefbit)
				*tl = txdr_unsigned(NFSERR_MOVED);
			else
				*tl = 0;
			retnum += NFSX_UNSIGNED;
			break;
		case NFSATTRBIT_FSLOCATIONS:
			retnum += nfsm_strtom(nd, "/", 1);
			NFSM_BUILD(tl, u_int32_t *, NFSX_UNSIGNED);
			*tl = txdr_unsigned(refp->nfr_srvcnt);
			retnum += NFSX_UNSIGNED;
			cp = refp->nfr_srvlist;
			for (i = 0; i < refp->nfr_srvcnt; i++) {
				NFSM_BUILD(tl, u_int32_t *, NFSX_UNSIGNED);
				*tl = txdr_unsigned(1);
				retnum += NFSX_UNSIGNED;
				cp2 = STRCHR(cp, ':');
				if (cp2 != NULL)
					len = cp2 - cp;
				else
					len = 1;
				retnum += nfsm_strtom(nd, cp, len);
				if (cp2 != NULL)
					cp = cp2 + 1;
				cp2 = STRCHR(cp, ',');
				if (cp2 != NULL)
					len = cp2 - cp;
				else
					len = strlen(cp);
				retnum += nfsm_strtom(nd, cp, len);
				if (cp2 != NULL)
					cp = cp2 + 1;
			}
			break;
		case NFSATTRBIT_MOUNTEDONFILEID:
			NFSM_BUILD(tl, u_int32_t *, NFSX_HYPER);
			txdr_hyper(refp->nfr_dfileno, tl);
			retnum += NFSX_HYPER;
			break;
		default:
			printf("EEK! Bad V4 refattr bitpos=%d\n", bitpos);
		}
	    }
	}
	*retnump = txdr_unsigned(retnum);
	return (retnum + prefixnum);
}

/*
 * Parse a file name out of a request.
 */
int
nfsrv_parsename(struct nfsrv_descript *nd, char *bufp, u_long *hashp,
    NFSPATHLEN_T *outlenp)
{
	char *fromcp, *tocp, val = '\0';
	struct mbuf *md;
	int i;
	int rem, len, error = 0, pubtype = 0, outlen = 0, percent = 0;
	char digit;
	u_int32_t *tl;
	u_long hash = 0;

	if (hashp != NULL)
		*hashp = 0;
	tocp = bufp;
	/*
	 * For V4, check for lookup parent.
	 * Otherwise, get the component name.
	 */
	if ((nd->nd_flag & ND_NFSV4) && (nd->nd_procnum == NFSV4OP_LOOKUPP ||
	    nd->nd_procnum == NFSV4OP_SECINFONONAME)) {
	    *tocp++ = '.';
	    hash += ((u_char)'.');
	    *tocp++ = '.';
	    hash += ((u_char)'.');
	    outlen = 2;
	} else {
	    /*
	     * First, get the name length.
	     */
	    NFSM_DISSECT(tl, u_int32_t *, NFSX_UNSIGNED);
	    len = fxdr_unsigned(int, *tl);
	    if (len > NFS_MAXNAMLEN) {
		nd->nd_repstat = NFSERR_NAMETOL;
		error = 0;
		goto nfsmout;
	    } else if (len <= 0) {
		nd->nd_repstat = NFSERR_INVAL;
		error = 0;
		goto nfsmout;
	    }

	    /*
	     * Now, copy the component name into the buffer.
	     */
	    fromcp = nd->nd_dpos;
	    md = nd->nd_md;
	    rem = mtod(md, caddr_t) + md->m_len - fromcp;
	    for (i = 0; i < len; i++) {
		while (rem == 0) {
			md = md->m_next;
			if (md == NULL) {
				error = EBADRPC;
				goto nfsmout;
			}
			fromcp = mtod(md, caddr_t);
			rem = md->m_len;
		}
		if (*fromcp == '\0') {
			nd->nd_repstat = EACCES;
			error = 0;
			goto nfsmout;
		}
		/*
		 * For lookups on the public filehandle, do some special
		 * processing on the name. (The public file handle is the
		 * root of the public file system for this server.)
		 */
		if (nd->nd_flag & ND_PUBLOOKUP) {
			/*
			 * If the first char is ASCII, it is a canonical
			 * path, otherwise it is a native path. (RFC2054
			 * doesn't actually state what it is if the first
			 * char isn't ASCII or 0x80, so I assume native.)
			 * pubtype == 1 -> native path
			 * pubtype == 2 -> canonical path
			 */
			if (i == 0) {
				if (*fromcp & 0x80) {
					/*
					 * Since RFC2054 doesn't indicate
					 * that a native path of just 0x80
					 * isn't allowed, I'll replace the
					 * 0x80 with '/' instead of just
					 * throwing it away.
					 */
					*fromcp = '/';
					pubtype = 1;
				} else {
					pubtype = 2;
				}
			}
			/*
			 * '/' only allowed in a native path
			 */
			if (*fromcp == '/' && pubtype != 1) {
				nd->nd_repstat = EACCES;
				error = 0;
				goto nfsmout;
			}

			/*
			 * For the special case of 2 hex digits after a
			 * '%' in an absolute path, calculate the value.
			 * percent == 1 -> indicates "get first hex digit"
			 * percent == 2 -> indicates "get second hex digit"
			 */
			if (percent > 0) {
				digit = nfsrv_hexdigit(*fromcp, &error);
				if (error) {
					nd->nd_repstat = EACCES;
					error = 0;
					goto nfsmout;
				}
				if (percent == 1) {
					val = (digit << 4);
					percent = 2;
				} else {
					val += digit;
					percent = 0;
					*tocp++ = val;
					hash += ((u_char)val);
					outlen++;
				}
			} else {
				if (*fromcp == '%' && pubtype == 2) {
					/*
					 * Must be followed by 2 hex digits
					 */
					if ((len - i) < 3) {
						nd->nd_repstat = EACCES;
						error = 0;
						goto nfsmout;
					}
					percent = 1;
				} else {
					*tocp++ = *fromcp;
					hash += ((u_char)*fromcp);
					outlen++;
				}
			}
		} else {
			/*
			 * Normal, non lookup on public, name.
			 */
			if (*fromcp == '/') {
				if (nd->nd_flag & ND_NFSV4)
					nd->nd_repstat = NFSERR_BADNAME;
				else
					nd->nd_repstat = EACCES;
				error = 0;
				goto nfsmout;
			}
			hash += ((u_char)*fromcp);
			*tocp++ = *fromcp;
			outlen++;
		}
		fromcp++;
		rem--;
	    }
	    nd->nd_md = md;
	    nd->nd_dpos = fromcp;
	    i = NFSM_RNDUP(len) - len;
	    if (i > 0) {
		if (rem >= i) {
			nd->nd_dpos += i;
		} else {
			error = nfsm_advance(nd, i, rem);
			if (error)
				goto nfsmout;
		}
	    }

	    /*
	     * For v4, don't allow lookups of '.' or '..' and
	     * also check for non-utf8 strings.
	     */
	    if (nd->nd_flag & ND_NFSV4) {
		if ((outlen == 1 && bufp[0] == '.') ||
		    (outlen == 2 && bufp[0] == '.' &&
		     bufp[1] == '.')) {
		    nd->nd_repstat = NFSERR_BADNAME;
		    error = 0;
		    goto nfsmout;
		}
		if (enable_checkutf8 == 1 &&
		    nfsrv_checkutf8((u_int8_t *)bufp, outlen)) {
		    nd->nd_repstat = NFSERR_INVAL;
		    error = 0;
		    goto nfsmout;
		}
	    }
	}
	*tocp = '\0';
	*outlenp = (size_t)outlen + 1;
	if (hashp != NULL)
		*hashp = hash;
nfsmout:
	NFSEXITCODE2(error, nd);
	return (error);
}

void
nfsd_init(void)
{
	int i;


	/*
	 * Initialize client queues. Don't free/reinitialize
	 * them when nfsds are restarted.
	 */
	NFSD_VNET(nfsclienthash) = malloc(sizeof(struct nfsclienthashhead) *
	    nfsrv_clienthashsize, M_NFSDCLIENT, M_WAITOK | M_ZERO);
	for (i = 0; i < nfsrv_clienthashsize; i++)
		LIST_INIT(&NFSD_VNET(nfsclienthash)[i]);
	NFSD_VNET(nfslockhash) = malloc(sizeof(struct nfslockhashhead) *
	    nfsrv_lockhashsize, M_NFSDLOCKFILE, M_WAITOK | M_ZERO);
	for (i = 0; i < nfsrv_lockhashsize; i++)
		LIST_INIT(&NFSD_VNET(nfslockhash)[i]);
	NFSD_VNET(nfssessionhash) = malloc(sizeof(struct nfssessionhash) *
	    nfsrv_sessionhashsize, M_NFSDSESSION, M_WAITOK | M_ZERO);
	for (i = 0; i < nfsrv_sessionhashsize; i++) {
		mtx_init(&NFSD_VNET(nfssessionhash)[i].mtx, "nfssm", NULL,
		    MTX_DEF);
		LIST_INIT(&NFSD_VNET(nfssessionhash)[i].list);
	}
	LIST_INIT(&nfsrv_dontlisthead);
	TAILQ_INIT(&nfsrv_recalllisthead);

	/* and the v2 pubfh should be all zeros */
	NFSBZERO(nfs_v2pubfh, NFSX_V2FH);
}

/*
 * Check the v4 root exports.
 * Return 0 if ok, 1 otherwise.
 */
int
nfsd_checkrootexp(struct nfsrv_descript *nd)
{

	if (NFSD_VNET(nfs_rootfhset) == 0)
		return (NFSERR_AUTHERR | AUTH_FAILED);
	/*
	 * For NFSv4.1/4.2, if the client specifies SP4_NONE, then these
	 * operations are allowed regardless of the value of the "sec=XXX"
	 * field in the V4: exports line.
	 * As such, these Kerberos checks only apply to NFSv4.0 mounts.
	 */
	if ((nd->nd_flag & ND_NFSV41) != 0)
		goto checktls;
	if ((nd->nd_flag & (ND_GSS | ND_EXAUTHSYS)) == ND_EXAUTHSYS)
		goto checktls;
	if ((nd->nd_flag & (ND_GSSINTEGRITY | ND_EXGSSINTEGRITY)) ==
	    (ND_GSSINTEGRITY | ND_EXGSSINTEGRITY))
		goto checktls;
	if ((nd->nd_flag & (ND_GSSPRIVACY | ND_EXGSSPRIVACY)) ==
	    (ND_GSSPRIVACY | ND_EXGSSPRIVACY))
		goto checktls;
	if ((nd->nd_flag & (ND_GSS | ND_GSSINTEGRITY | ND_GSSPRIVACY |
	     ND_EXGSS)) == (ND_GSS | ND_EXGSS))
		goto checktls;
	return (NFSERR_AUTHERR | AUTH_TOOWEAK);
checktls:
	if ((nd->nd_flag & ND_EXTLS) == 0)
		return (0);
	if ((nd->nd_flag & (ND_TLSCERTUSER | ND_EXTLSCERTUSER)) ==
	    (ND_TLSCERTUSER | ND_EXTLSCERTUSER))
		return (0);
	if ((nd->nd_flag & (ND_TLSCERT | ND_EXTLSCERT | ND_EXTLSCERTUSER)) ==
	    (ND_TLSCERT | ND_EXTLSCERT))
		return (0);
	if ((nd->nd_flag & (ND_TLS | ND_EXTLSCERTUSER | ND_EXTLSCERT)) ==
	    ND_TLS)
		return (0);
#ifdef notnow
	/* There is currently no auth_stat for this. */
	if ((nd->nd_flag & ND_TLS) == 0)
		return (NFSERR_AUTHERR | AUTH_NEEDS_TLS);
	return (NFSERR_AUTHERR | AUTH_NEEDS_TLS_MUTUAL_HOST);
#endif
	return (NFSERR_AUTHERR | AUTH_TOOWEAK);
}

/*
 * Parse the first part of an NFSv4 compound to find out what the minor
 * version# is.
 */
void
nfsd_getminorvers(struct nfsrv_descript *nd, u_char *tag, u_char **tagstrp,
    int *taglenp, u_int32_t *minversp)
{
	uint32_t *tl;
	int error = 0, taglen = -1;
	u_char *tagstr = NULL;

	NFSM_DISSECT(tl, uint32_t *, NFSX_UNSIGNED);
	taglen = fxdr_unsigned(int, *tl);
	if (taglen < 0 || taglen > NFSV4_OPAQUELIMIT) {
		error = EBADRPC;
		goto nfsmout;
	}
	if (taglen <= NFSV4_SMALLSTR)
		tagstr = tag;
	else
		tagstr = malloc(taglen + 1, M_TEMP, M_WAITOK);
	error = nfsrv_mtostr(nd, tagstr, taglen);
	if (error != 0)
		goto nfsmout;
	NFSM_DISSECT(tl, uint32_t *, NFSX_UNSIGNED);
	*minversp = fxdr_unsigned(u_int32_t, *tl);
	*tagstrp = tagstr;
	if (*minversp == NFSV41_MINORVERSION)
		nd->nd_flag |= ND_NFSV41;
	else if (*minversp == NFSV42_MINORVERSION)
		nd->nd_flag |= (ND_NFSV41 | ND_NFSV42);
nfsmout:
	if (error != 0) {
		if (tagstr != NULL && taglen > NFSV4_SMALLSTR)
			free(tagstr, M_TEMP);
		taglen = -1;
	}
	*taglenp = taglen;
}
