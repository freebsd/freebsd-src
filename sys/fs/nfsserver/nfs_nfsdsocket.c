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
 * Socket operations for use by the nfs server.
 */

#include <fs/nfs/nfsport.h>

#include <security/mac/mac_framework.h>

extern struct nfsrvfh nfs_pubfh;
extern int nfs_pubfhset;
extern struct nfsv4lock nfsv4rootfs_lock;
extern int nfsrv_clienthashsize;
extern int nfsd_debuglevel;
extern int nfsrv_layouthighwater;
extern volatile int nfsrv_layoutcnt;
NFSV4ROOTLOCKMUTEX;
NFSSTATESPINLOCK;

NFSD_VNET_DECLARE(struct nfsrv_stablefirst, nfsrv_stablefirst);
NFSD_VNET_DECLARE(struct nfsclienthashhead *, nfsclienthash);
NFSD_VNET_DECLARE(int, nfsrc_floodlevel);
NFSD_VNET_DECLARE(int, nfsrc_tcpsavedreplies);
NFSD_VNET_DECLARE(struct nfsrvfh, nfs_rootfh);
NFSD_VNET_DECLARE(int, nfs_rootfhset);
NFSD_VNET_DECLARE(struct nfsstatsv1 *, nfsstatsv1_p);

int (*nfsrv3_procs0[NFS_V3NPROCS])(struct nfsrv_descript *,
    int, vnode_t , struct nfsexstuff *) = {
	(int (*)(struct nfsrv_descript *, int, vnode_t , struct nfsexstuff *))0,
	nfsrvd_getattr,
	nfsrvd_setattr,
	(int (*)(struct nfsrv_descript *, int, vnode_t , struct nfsexstuff *))0,
	nfsrvd_access,
	nfsrvd_readlink,
	nfsrvd_read,
	nfsrvd_write,
	nfsrvd_create,
	(int (*)(struct nfsrv_descript *, int, vnode_t , struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , struct nfsexstuff *))0,
	nfsrvd_remove,
	nfsrvd_remove,
	(int (*)(struct nfsrv_descript *, int, vnode_t , struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , struct nfsexstuff *))0,
	nfsrvd_readdir,
	nfsrvd_readdirplus,
	nfsrvd_statfs,
	nfsrvd_fsinfo,
	nfsrvd_pathconf,
	nfsrvd_commit,
};

int (*nfsrv3_procs1[NFS_V3NPROCS])(struct nfsrv_descript *,
    int, vnode_t , vnode_t *, fhandle_t *, struct nfsexstuff *) = {
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t *, fhandle_t *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t *, fhandle_t *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t *, fhandle_t *, struct nfsexstuff *))0,
	nfsrvd_lookup,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t *, fhandle_t *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t *, fhandle_t *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t *, fhandle_t *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t *, fhandle_t *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t *, fhandle_t *, struct nfsexstuff *))0,
	nfsrvd_mkdir,
	nfsrvd_symlink,
	nfsrvd_mknod,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t *, fhandle_t *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t *, fhandle_t *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t *, fhandle_t *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t *, fhandle_t *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t *, fhandle_t *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t *, fhandle_t *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t *, fhandle_t *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t *, fhandle_t *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t *, fhandle_t *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t *, fhandle_t *, struct nfsexstuff *))0,
};

int (*nfsrv3_procs2[NFS_V3NPROCS])(struct nfsrv_descript *,
    int, vnode_t , vnode_t , struct nfsexstuff *, struct nfsexstuff *) = {
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t , struct nfsexstuff *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t , struct nfsexstuff *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t , struct nfsexstuff *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t , struct nfsexstuff *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t , struct nfsexstuff *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t , struct nfsexstuff *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t , struct nfsexstuff *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t , struct nfsexstuff *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t , struct nfsexstuff *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t , struct nfsexstuff *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t , struct nfsexstuff *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t , struct nfsexstuff *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t , struct nfsexstuff *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t , struct nfsexstuff *, struct nfsexstuff *))0,
	nfsrvd_rename,
	nfsrvd_link,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t , struct nfsexstuff *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t , struct nfsexstuff *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t , struct nfsexstuff *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t , struct nfsexstuff *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t , struct nfsexstuff *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t , struct nfsexstuff *, struct nfsexstuff *))0,
};

int (*nfsrv4_ops0[NFSV42_NOPS])(struct nfsrv_descript *,
    int, vnode_t , struct nfsexstuff *) = {
	(int (*)(struct nfsrv_descript *, int, vnode_t , struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , struct nfsexstuff *))0,
	nfsrvd_access,
	nfsrvd_close,
	nfsrvd_commit,
	(int (*)(struct nfsrv_descript *, int, vnode_t , struct nfsexstuff *))0,
	nfsrvd_delegpurge,
	nfsrvd_delegreturn,
	nfsrvd_getattr,
	nfsrvd_getfh,
	(int (*)(struct nfsrv_descript *, int, vnode_t , struct nfsexstuff *))0,
	nfsrvd_lock,
	nfsrvd_lockt,
	nfsrvd_locku,
	(int (*)(struct nfsrv_descript *, int, vnode_t , struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , struct nfsexstuff *))0,
	nfsrvd_verify,
	(int (*)(struct nfsrv_descript *, int, vnode_t , struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , struct nfsexstuff *))0,
	nfsrvd_openconfirm,
	nfsrvd_opendowngrade,
	(int (*)(struct nfsrv_descript *, int, vnode_t , struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , struct nfsexstuff *))0,
	nfsrvd_read,
	nfsrvd_readdirplus,
	nfsrvd_readlink,
	nfsrvd_remove,
	(int (*)(struct nfsrv_descript *, int, vnode_t , struct nfsexstuff *))0,
	nfsrvd_renew,
	(int (*)(struct nfsrv_descript *, int, vnode_t , struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , struct nfsexstuff *))0,
	nfsrvd_secinfo,
	nfsrvd_setattr,
	nfsrvd_setclientid,
	nfsrvd_setclientidcfrm,
	nfsrvd_verify,
	nfsrvd_write,
	nfsrvd_releaselckown,
	nfsrvd_notsupp,
	nfsrvd_bindconnsess,
	nfsrvd_exchangeid,
	nfsrvd_createsession,
	nfsrvd_destroysession,
	nfsrvd_freestateid,
	nfsrvd_notsupp,
	nfsrvd_getdevinfo,
	nfsrvd_notsupp,
	nfsrvd_layoutcommit,
	nfsrvd_layoutget,
	nfsrvd_layoutreturn,
	nfsrvd_secinfononame,
	nfsrvd_sequence,
	nfsrvd_notsupp,
	nfsrvd_teststateid,
	nfsrvd_notsupp,
	nfsrvd_destroyclientid,
	nfsrvd_reclaimcomplete,
	nfsrvd_allocate,
	(int (*)(struct nfsrv_descript *, int, vnode_t , struct nfsexstuff *))0,
	nfsrvd_notsupp,
	nfsrvd_deallocate,
	nfsrvd_ioadvise,
	nfsrvd_layouterror,
	nfsrvd_layoutstats,
	nfsrvd_notsupp,
	nfsrvd_notsupp,
	nfsrvd_notsupp,
	nfsrvd_seek,
	nfsrvd_notsupp,
	nfsrvd_notsupp,
	nfsrvd_getxattr,
	nfsrvd_setxattr,
	nfsrvd_listxattr,
	nfsrvd_rmxattr,
};

int (*nfsrv4_ops1[NFSV42_NOPS])(struct nfsrv_descript *,
    int, vnode_t , vnode_t *, fhandle_t *, struct nfsexstuff *) = {
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t *, fhandle_t *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t *, fhandle_t *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t *, fhandle_t *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t *, fhandle_t *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t *, fhandle_t *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t *, fhandle_t *, struct nfsexstuff *))0,
	nfsrvd_mknod,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t *, fhandle_t *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t *, fhandle_t *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t *, fhandle_t *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t *, fhandle_t *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t *, fhandle_t *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t *, fhandle_t *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t *, fhandle_t *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t *, fhandle_t *, struct nfsexstuff *))0,
	nfsrvd_lookup,
	nfsrvd_lookup,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t *, fhandle_t *, struct nfsexstuff *))0,
	nfsrvd_open,
	nfsrvd_openattr,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t *, fhandle_t *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t *, fhandle_t *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t *, fhandle_t *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t *, fhandle_t *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t *, fhandle_t *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t *, fhandle_t *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t *, fhandle_t *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t *, fhandle_t *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t *, fhandle_t *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t *, fhandle_t *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t *, fhandle_t *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t *, fhandle_t *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t *, fhandle_t *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t *, fhandle_t *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t *, fhandle_t *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t *, fhandle_t *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t *, fhandle_t *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t *, fhandle_t *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t *, fhandle_t *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t *, fhandle_t *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t *, fhandle_t *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t *, fhandle_t *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t *, fhandle_t *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t *, fhandle_t *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t *, fhandle_t *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t *, fhandle_t *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t *, fhandle_t *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t *, fhandle_t *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t *, fhandle_t *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t *, fhandle_t *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t *, fhandle_t *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t *, fhandle_t *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t *, fhandle_t *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t *, fhandle_t *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t *, fhandle_t *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t *, fhandle_t *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t *, fhandle_t *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t *, fhandle_t *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t *, fhandle_t *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t *, fhandle_t *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t *, fhandle_t *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t *, fhandle_t *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t *, fhandle_t *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t *, fhandle_t *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t *, fhandle_t *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t *, fhandle_t *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t *, fhandle_t *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t *, fhandle_t *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t *, fhandle_t *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t *, fhandle_t *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t *, fhandle_t *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t *, fhandle_t *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t *, fhandle_t *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t *, fhandle_t *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t *, fhandle_t *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t *, fhandle_t *, struct nfsexstuff *))0,
};

int (*nfsrv4_ops2[NFSV42_NOPS])(struct nfsrv_descript *,
    int, vnode_t , vnode_t , struct nfsexstuff *, struct nfsexstuff *) = {
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t , struct nfsexstuff *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t , struct nfsexstuff *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t , struct nfsexstuff *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t , struct nfsexstuff *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t , struct nfsexstuff *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t , struct nfsexstuff *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t , struct nfsexstuff *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t , struct nfsexstuff *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t , struct nfsexstuff *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t , struct nfsexstuff *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t , struct nfsexstuff *, struct nfsexstuff *))0,
	nfsrvd_link,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t , struct nfsexstuff *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t , struct nfsexstuff *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t , struct nfsexstuff *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t , struct nfsexstuff *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t , struct nfsexstuff *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t , struct nfsexstuff *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t , struct nfsexstuff *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t , struct nfsexstuff *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t , struct nfsexstuff *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t , struct nfsexstuff *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t , struct nfsexstuff *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t , struct nfsexstuff *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t , struct nfsexstuff *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t , struct nfsexstuff *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t , struct nfsexstuff *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t , struct nfsexstuff *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t , struct nfsexstuff *, struct nfsexstuff *))0,
	nfsrvd_rename,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t , struct nfsexstuff *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t , struct nfsexstuff *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t , struct nfsexstuff *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t , struct nfsexstuff *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t , struct nfsexstuff *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t , struct nfsexstuff *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t , struct nfsexstuff *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t , struct nfsexstuff *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t , struct nfsexstuff *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t , struct nfsexstuff *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t , struct nfsexstuff *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t , struct nfsexstuff *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t , struct nfsexstuff *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t , struct nfsexstuff *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t , struct nfsexstuff *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t , struct nfsexstuff *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t , struct nfsexstuff *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t , struct nfsexstuff *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t , struct nfsexstuff *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t , struct nfsexstuff *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t , struct nfsexstuff *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t , struct nfsexstuff *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t , struct nfsexstuff *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t , struct nfsexstuff *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t , struct nfsexstuff *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t , struct nfsexstuff *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t , struct nfsexstuff *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t , struct nfsexstuff *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t , struct nfsexstuff *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t , struct nfsexstuff *, struct nfsexstuff *))0,
	nfsrvd_copy_file_range,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t , struct nfsexstuff *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t , struct nfsexstuff *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t , struct nfsexstuff *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t , struct nfsexstuff *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t , struct nfsexstuff *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t , struct nfsexstuff *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t , struct nfsexstuff *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t , struct nfsexstuff *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t , struct nfsexstuff *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t , struct nfsexstuff *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t , struct nfsexstuff *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t , struct nfsexstuff *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t , struct nfsexstuff *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t , struct nfsexstuff *, struct nfsexstuff *))0,
	(int (*)(struct nfsrv_descript *, int, vnode_t , vnode_t , struct nfsexstuff *, struct nfsexstuff *))0,
};

/*
 * Static array that defines which nfs rpc's are nonidempotent
 */
static int nfsrv_nonidempotent[NFS_V3NPROCS] = {
	FALSE,
	FALSE,
	TRUE,
	FALSE,
	FALSE,
	FALSE,
	FALSE,
	TRUE,
	TRUE,
	TRUE,
	TRUE,
	TRUE,
	TRUE,
	TRUE,
	TRUE,
	TRUE,
	FALSE,
	FALSE,
	FALSE,
	FALSE,
	FALSE,
	FALSE,
};

/*
 * This static array indicates whether or not the RPC modifies the
 * file system.
 */
int nfsrv_writerpc[NFS_NPROCS] = { 0, 0, 1, 0, 0, 0, 0,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 };

SYSCTL_DECL(_vfs_nfsd);
static int	nfs_minminorv4 = NFSV4_MINORVERSION;
SYSCTL_INT(_vfs_nfsd, OID_AUTO, server_min_minorversion4, CTLFLAG_RWTUN,
    &nfs_minminorv4, 0,
    "The lowest minor version of NFSv4 handled by the server");

static int	nfs_maxminorv4 = NFSV42_MINORVERSION;
SYSCTL_INT(_vfs_nfsd, OID_AUTO, server_max_minorversion4, CTLFLAG_RWTUN,
    &nfs_maxminorv4, 0,
    "The highest minor version of NFSv4 handled by the server");

/* local functions */
static void nfsrvd_compound(struct nfsrv_descript *nd, int isdgram,
    u_char *tag, int taglen, u_int32_t minorvers);

/*
 * This static array indicates which server procedures require the extra
 * arguments to return the current file handle for V2, 3.
 */
static int nfs_retfh[NFS_V3NPROCS] = { 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 1,
	1, 0, 0, 2, 2, 0, 0, 0, 0, 0, 0 };

extern struct nfsv4_opflag nfsv4_opflag[NFSV42_NOPS];

static int nfsv3to4op[NFS_V3NPROCS] = {
	NFSPROC_NULL,
	NFSV4OP_GETATTR,
	NFSV4OP_SETATTR,
	NFSV4OP_LOOKUP,
	NFSV4OP_ACCESS,
	NFSV4OP_READLINK,
	NFSV4OP_READ,
	NFSV4OP_WRITE,
	NFSV4OP_V3CREATE,
	NFSV4OP_MKDIR,
	NFSV4OP_SYMLINK,
	NFSV4OP_MKNOD,
	NFSV4OP_REMOVE,
	NFSV4OP_RMDIR,
	NFSV4OP_RENAME,
	NFSV4OP_LINK,
	NFSV4OP_READDIR,
	NFSV4OP_READDIRPLUS,
	NFSV4OP_FSSTAT,
	NFSV4OP_FSINFO,
	NFSV4OP_PATHCONF,
	NFSV4OP_COMMIT,
};

static struct mtx nfsrvd_statmtx;
MTX_SYSINIT(nfsst, &nfsrvd_statmtx, "NFSstat", MTX_DEF);

static struct ucred *nfsrv_createrootcred(void);

static void
nfsrvd_statstart(int op, struct bintime *now)
{
	if (op > (NFSV42_NOPS + NFSV4OP_FAKENOPS)) {
		printf("%s: op %d invalid\n", __func__, op);
		return;
	}

	mtx_lock(&nfsrvd_statmtx);
	if (NFSD_VNET(nfsstatsv1_p)->srvstartcnt ==
	    NFSD_VNET(nfsstatsv1_p)->srvdonecnt) {
		if (now != NULL)
			NFSD_VNET(nfsstatsv1_p)->busyfrom = *now;
		else
			binuptime(&NFSD_VNET(nfsstatsv1_p)->busyfrom);
		
	}
	NFSD_VNET(nfsstatsv1_p)->srvrpccnt[op]++;
	NFSD_VNET(nfsstatsv1_p)->srvstartcnt++;
	mtx_unlock(&nfsrvd_statmtx);

}

static void
nfsrvd_statend(int op, uint64_t bytes, struct bintime *now,
    struct bintime *then)
{
	struct bintime dt, lnow;

	if (op > (NFSV42_NOPS + NFSV4OP_FAKENOPS)) {
		printf("%s: op %d invalid\n", __func__, op);
		return;
	}

	if (now == NULL) {
		now = &lnow;
		binuptime(now);
	}

	mtx_lock(&nfsrvd_statmtx);

	NFSD_VNET(nfsstatsv1_p)->srvbytes[op] += bytes;
	NFSD_VNET(nfsstatsv1_p)->srvops[op]++;

	if (then != NULL) {
		dt = *now;
		bintime_sub(&dt, then);
		bintime_add(&NFSD_VNET(nfsstatsv1_p)->srvduration[op], &dt);
	}

	dt = *now;
	bintime_sub(&dt, &NFSD_VNET(nfsstatsv1_p)->busyfrom);
	bintime_add(&NFSD_VNET(nfsstatsv1_p)->busytime, &dt);
	NFSD_VNET(nfsstatsv1_p)->busyfrom = *now;

	NFSD_VNET(nfsstatsv1_p)->srvdonecnt++;

	mtx_unlock(&nfsrvd_statmtx);
}

/*
 * Do an RPC. Basically, get the file handles translated to vnode pointers
 * and then call the appropriate server routine. The server routines are
 * split into groups, based on whether they use a file handle or file
 * handle plus name or ...
 * The NFS V4 Compound RPC is performed separately by nfsrvd_compound().
 */
void
nfsrvd_dorpc(struct nfsrv_descript *nd, int isdgram, u_char *tag, int taglen,
    u_int32_t minorvers)
{
	int error = 0, lktype;
	vnode_t vp;
	mount_t mp;
	struct nfsrvfh fh;
	struct nfsexstuff nes;
	struct mbuf *md;
	char *dpos;

	/*
	 * Save the current position in the request mbuf list so
	 * that a rollback to this location can be done upon an
	 * ERELOOKUP error return from an RPC function.
	 */
	md = nd->nd_md;
	dpos = nd->nd_dpos;
tryagain:
	mp = NULL;

	/*
	 * Get a locked vnode for the first file handle
	 */
	if (!(nd->nd_flag & ND_NFSV4)) {
		KASSERT(nd->nd_repstat == 0, ("nfsrvd_dorpc"));
		/*
		 * For NFSv3, if the malloc/mget allocation is near limits,
		 * return NFSERR_DELAY.
		 */
		if ((nd->nd_flag & ND_NFSV3) && nfsrv_mallocmget_limit()) {
			nd->nd_repstat = NFSERR_DELAY;
			vp = NULL;
		} else {
			error = nfsrv_mtofh(nd, &fh);
			if (error) {
				if (error != EBADRPC)
					printf("nfs dorpc err1=%d\n", error);
				nd->nd_repstat = NFSERR_GARBAGE;
				goto out;
			}
			if (nd->nd_procnum == NFSPROC_READ ||
			    nd->nd_procnum == NFSPROC_WRITE ||
			    nd->nd_procnum == NFSPROC_READDIR ||
			    nd->nd_procnum == NFSPROC_READDIRPLUS ||
			    nd->nd_procnum == NFSPROC_READLINK ||
			    nd->nd_procnum == NFSPROC_GETATTR ||
			    nd->nd_procnum == NFSPROC_ACCESS ||
			    nd->nd_procnum == NFSPROC_FSSTAT ||
			    nd->nd_procnum == NFSPROC_FSINFO)
				lktype = LK_SHARED;
			else
				lktype = LK_EXCLUSIVE;
			if (nd->nd_flag & ND_PUBLOOKUP)
				nfsd_fhtovp(nd, &nfs_pubfh, lktype, &vp, &nes,
				    &mp, nfsrv_writerpc[nd->nd_procnum], -1);
			else
				nfsd_fhtovp(nd, &fh, lktype, &vp, &nes,
				    &mp, nfsrv_writerpc[nd->nd_procnum], -1);
			if (nd->nd_repstat == NFSERR_PROGNOTV4)
				goto out;
		}
	}

	/*
	 * For V2 and 3, set the ND_SAVEREPLY flag for the recent request
	 * cache, as required.
	 * For V4, nfsrvd_compound() does this.
	 */
	if (!(nd->nd_flag & ND_NFSV4) && nfsrv_nonidempotent[nd->nd_procnum])
		nd->nd_flag |= ND_SAVEREPLY;

	nfsrvd_rephead(nd);
	/*
	 * If nd_repstat is non-zero, just fill in the reply status
	 * to complete the RPC reply for V2. Otherwise, you must do
	 * the RPC.
	 */
	if (nd->nd_repstat && (nd->nd_flag & ND_NFSV2)) {
		*nd->nd_errp = nfsd_errmap(nd);
		nfsrvd_statstart(nfsv3to4op[nd->nd_procnum], /*now*/ NULL);
		nfsrvd_statend(nfsv3to4op[nd->nd_procnum], /*bytes*/ 0,
		   /*now*/ NULL, /*then*/ NULL);
		vn_finished_write(mp);
		goto out;
	}

	/*
	 * Now the procedure can be performed. For V4, nfsrvd_compound()
	 * works through the sub-rpcs, otherwise just call the procedure.
	 * The procedures are in three groups with different arguments.
	 * The group is indicated by the value in nfs_retfh[].
	 */
	if (nd->nd_flag & ND_NFSV4) {
		nfsrvd_compound(nd, isdgram, tag, taglen, minorvers);
	} else {
		struct bintime start_time;

		binuptime(&start_time);
		nfsrvd_statstart(nfsv3to4op[nd->nd_procnum], &start_time);

		if (nfs_retfh[nd->nd_procnum] == 1) {
			if (vp)
				NFSVOPUNLOCK(vp);
			error = (*(nfsrv3_procs1[nd->nd_procnum]))(nd, isdgram,
			    vp, NULL, (fhandle_t *)fh.nfsrvfh_data, &nes);
		} else if (nfs_retfh[nd->nd_procnum] == 2) {
			error = (*(nfsrv3_procs2[nd->nd_procnum]))(nd, isdgram,
			    vp, NULL, &nes, NULL);
		} else {
			error = (*(nfsrv3_procs0[nd->nd_procnum]))(nd, isdgram,
			    vp, &nes);
		}
		vn_finished_write(mp);

		if (error == 0 && nd->nd_repstat == ERELOOKUP) {
			/*
			 * Roll back to the beginning of the RPC request
			 * arguments.
			 */
			nd->nd_md = md;
			nd->nd_dpos = dpos;

			/* Free the junk RPC reply and redo the RPC. */
			m_freem(nd->nd_mreq);
			nd->nd_mreq = nd->nd_mb = NULL;
			nd->nd_repstat = 0;
			goto tryagain;
		}

		nfsrvd_statend(nfsv3to4op[nd->nd_procnum], /*bytes*/ 0,
		    /*now*/ NULL, /*then*/ &start_time);
	}
	if (error) {
		if (error != EBADRPC)
			printf("nfs dorpc err2=%d\n", error);
		nd->nd_repstat = NFSERR_GARBAGE;
	}
	*nd->nd_errp = nfsd_errmap(nd);

	/*
	 * Don't cache certain reply status values.
	 */
	if (nd->nd_repstat && (nd->nd_flag & ND_SAVEREPLY) &&
	    (nd->nd_repstat == NFSERR_GARBAGE ||
	     nd->nd_repstat == NFSERR_BADXDR ||
	     nd->nd_repstat == NFSERR_MOVED ||
	     nd->nd_repstat == NFSERR_DELAY ||
	     nd->nd_repstat == NFSERR_BADSEQID ||
	     nd->nd_repstat == NFSERR_RESOURCE ||
	     nd->nd_repstat == NFSERR_SERVERFAULT ||
	     nd->nd_repstat == NFSERR_STALECLIENTID ||
	     nd->nd_repstat == NFSERR_STALESTATEID ||
	     nd->nd_repstat == NFSERR_OLDSTATEID ||
	     nd->nd_repstat == NFSERR_BADSTATEID ||
	     nd->nd_repstat == NFSERR_GRACE ||
	     nd->nd_repstat == NFSERR_NOGRACE))
		nd->nd_flag &= ~ND_SAVEREPLY;

out:
	NFSEXITCODE2(0, nd);
}

/*
 * Breaks down a compound RPC request and calls the server routines for
 * the subprocedures.
 * Some suboperations are performed directly here to simplify file handle<-->
 * vnode pointer handling.
 */
static void
nfsrvd_compound(struct nfsrv_descript *nd, int isdgram, u_char *tag,
    int taglen, u_int32_t minorvers)
{
	int i, lktype, op, op0 = 0, rstat, statsinprog = 0;
	u_int32_t *tl;
	struct nfsclient *clp, *nclp;
	int error = 0, igotlock, nextop, numops, savefhcnt;
	u_int32_t retops = 0, *retopsp = NULL, *repp;
	vnode_t vp, nvp, savevp;
	struct nfsrvfh fh;
	mount_t new_mp, temp_mp = NULL;
	struct ucred *credanon, *rootcred, *savecred;
	struct nfsexstuff nes, vpnes, savevpnes;
	fsid_t cur_fsid, save_fsid;
	static u_int64_t compref = 0;
	struct bintime start_time;
	struct thread *p;
	struct mbuf *mb, *md;
	char *bpos, *dpos;
	int bextpg, bextpgsiz;

	p = curthread;
	rootcred = savecred = NULL;

	/* Check for and optionally clear the no space flags for DSs. */
	nfsrv_checknospc();

	NFSVNO_EXINIT(&vpnes);
	NFSVNO_EXINIT(&savevpnes);
	/*
	 * Put the seq# of the current compound RPC in nfsrv_descript.
	 * (This is used by nfsrv_checkgetattr(), to see if the write
	 *  delegation was created by the same compound RPC as the one
	 *  with that Getattr in it.)
	 * Don't worry about the 64bit number wrapping around. It ain't
	 * gonna happen before this server gets shut down/rebooted.
	 */
	nd->nd_compref = compref++;

	/*
	 * Check for and optionally get a lock on the root. This lock means that
	 * no nfsd will be fiddling with the V4 file system and state stuff. It
	 * is required when the V4 root is being changed, the stable storage
	 * restart file is being updated, or callbacks are being done.
	 * When any of the nfsd are processing an NFSv4 compound RPC, they must
	 * either hold a reference count (nfs_usecnt) or the lock. When
	 * nfsrv_unlock() is called to release the lock, it can optionally
	 * also get a reference count, which saves the need for a call to
	 * nfsrv_getref() after nfsrv_unlock().
	 */
	/*
	 * First, check to see if we need to wait for an update lock.
	 */
	igotlock = 0;
	NFSLOCKV4ROOTMUTEX();
	if (NFSD_VNET(nfsrv_stablefirst).nsf_flags & NFSNSF_NEEDLOCK)
		igotlock = nfsv4_lock(&nfsv4rootfs_lock, 1, NULL,
		    NFSV4ROOTLOCKMUTEXPTR, NULL);
	else
		igotlock = nfsv4_lock(&nfsv4rootfs_lock, 0, NULL,
		    NFSV4ROOTLOCKMUTEXPTR, NULL);
	NFSUNLOCKV4ROOTMUTEX();
	if (igotlock) {
		/*
		 * If I got the lock, I can update the stable storage file.
		 * Done when the grace period is over or a client has long
		 * since expired.
		 */
		NFSD_VNET(nfsrv_stablefirst).nsf_flags &= ~NFSNSF_NEEDLOCK;
		if ((NFSD_VNET(nfsrv_stablefirst).nsf_flags &
		    (NFSNSF_GRACEOVER | NFSNSF_UPDATEDONE)) == NFSNSF_GRACEOVER)
			nfsrv_updatestable(p);

		/*
		 * If at least one client has long since expired, search
		 * the client list for them, write a REVOKE record on the
		 * stable storage file and then remove them from the client
		 * list.
		 */
		if (NFSD_VNET(nfsrv_stablefirst).nsf_flags &
		    NFSNSF_EXPIREDCLIENT) {
			NFSD_VNET(nfsrv_stablefirst).nsf_flags &=
			    ~NFSNSF_EXPIREDCLIENT;
			for (i = 0; i < nfsrv_clienthashsize; i++) {
			    LIST_FOREACH_SAFE(clp, &NFSD_VNET(nfsclienthash)[i],
				lc_hash, nclp) {
				if (clp->lc_flags & LCL_EXPIREIT) {
				    if (!LIST_EMPTY(&clp->lc_open) ||
					!LIST_EMPTY(&clp->lc_deleg))
					nfsrv_writestable(clp->lc_id,
					    clp->lc_idlen, NFSNST_REVOKE, p);
				    nfsrv_cleanclient(clp, p, false, NULL);
				    nfsrv_freedeleglist(&clp->lc_deleg);
				    nfsrv_freedeleglist(&clp->lc_olddeleg);
				    LIST_REMOVE(clp, lc_hash);
				    nfsrv_zapclient(clp, p);
				}
			    }
			}
		}
		NFSLOCKV4ROOTMUTEX();
		nfsv4_unlock(&nfsv4rootfs_lock, 1);
		NFSUNLOCKV4ROOTMUTEX();
	} else {
		/*
		 * If we didn't get the lock, we need to get a refcnt,
		 * which also checks for and waits for the lock.
		 */
		NFSLOCKV4ROOTMUTEX();
		nfsv4_getref(&nfsv4rootfs_lock, NULL,
		    NFSV4ROOTLOCKMUTEXPTR, NULL);
		NFSUNLOCKV4ROOTMUTEX();
	}

	/*
	 * If flagged, search for open owners that haven't had any opens
	 * for a long time.
	 */
	if (NFSD_VNET(nfsrv_stablefirst).nsf_flags & NFSNSF_NOOPENS) {
		nfsrv_throwawayopens(p);
	}

	/* Do a CBLAYOUTRECALL callback if over the high water mark. */
	if (nfsrv_layoutcnt > nfsrv_layouthighwater)
		nfsrv_recalloldlayout(p);

	savevp = vp = NULL;
	save_fsid.val[0] = save_fsid.val[1] = 0;
	cur_fsid.val[0] = cur_fsid.val[1] = 0;
	nextop = -1;
	savefhcnt = 0;

	/* If taglen < 0, there was a parsing error in nfsd_getminorvers(). */
	if (taglen < 0) {
		error = EBADRPC;
		goto nfsmout;
	}

	(void) nfsm_strtom(nd, tag, taglen);
	NFSM_BUILD(retopsp, u_int32_t *, NFSX_UNSIGNED);
	NFSM_DISSECT(tl, u_int32_t *, NFSX_UNSIGNED);
	if ((minorvers != NFSV4_MINORVERSION &&
	    minorvers != NFSV41_MINORVERSION &&
	    minorvers != NFSV42_MINORVERSION) ||
	    minorvers < nfs_minminorv4 || minorvers > nfs_maxminorv4)
		nd->nd_repstat = NFSERR_MINORVERMISMATCH;
	if (nd->nd_repstat)
		numops = 0;
	else
		numops = fxdr_unsigned(int, *tl);
	/*
	 * Loop around doing the sub ops.
	 * vp - is an unlocked vnode pointer for the CFH
	 * savevp - is an unlocked vnode pointer for the SAVEDFH
	 * (at some future date, it might turn out to be more appropriate
	 *  to keep the file handles instead of vnode pointers?)
	 * savevpnes and vpnes - are the export flags for the above.
	 */
	for (i = 0; i < numops; i++) {
		NFSM_BUILD(repp, u_int32_t *, 2 * NFSX_UNSIGNED);
		if (savefhcnt > 0) {
			op = NFSV4OP_SAVEFH;
			*repp = txdr_unsigned(op);
			savefhcnt--;
		} else if (nextop == -1) {
			NFSM_DISSECT(tl, u_int32_t *, NFSX_UNSIGNED);
			*repp = *tl;
			op = fxdr_unsigned(int, *tl);
		} else {
			op = nextop;
			*repp = txdr_unsigned(op);
			nextop = -1;
		}
		NFSD_DEBUG(4, "op=%d\n", op);
		if (op < NFSV4OP_ACCESS || op >= NFSV42_NOPS ||
		    (op >= NFSV4OP_NOPS && (nd->nd_flag & ND_NFSV41) == 0) ||
		    (op >= NFSV41_NOPS && (nd->nd_flag & ND_NFSV42) == 0)) {
			nd->nd_repstat = NFSERR_OPILLEGAL;
			*repp++ = txdr_unsigned(NFSV4OP_OPILLEGAL);
			*repp = nfsd_errmap(nd);
			retops++;
			break;
		} else {
			repp++;
		}

		binuptime(&start_time);
		nfsrvd_statstart(op, &start_time);
		statsinprog = 1;

		if (i == 0)
			op0 = op;
		if (i == numops - 1)
			nd->nd_flag |= ND_LASTOP;

		/*
		 * Check for a referral on the current FH and, if so, return
		 * NFSERR_MOVED for all ops that allow it, except Getattr.
		 */
		if (vp != NULL && op != NFSV4OP_GETATTR &&
		    nfsv4root_getreferral(vp, NULL, 0) != NULL &&
		    nfsrv_errmoved(op)) {
			nd->nd_repstat = NFSERR_MOVED;
			*repp = nfsd_errmap(nd);
			retops++;
			break;
		}

		/*
		 * For NFSv4.1, check for a Sequence Operation being first
		 * or one of the other allowed operations by itself.
		 */
		if ((nd->nd_flag & ND_NFSV41) != 0) {
			if (i != 0 && op == NFSV4OP_SEQUENCE)
				nd->nd_repstat = NFSERR_SEQUENCEPOS;
			else if (i == 0 && op != NFSV4OP_SEQUENCE &&
			    op != NFSV4OP_EXCHANGEID &&
			    op != NFSV4OP_CREATESESSION &&
			    op != NFSV4OP_BINDCONNTOSESS &&
			    op != NFSV4OP_DESTROYCLIENTID &&
			    op != NFSV4OP_DESTROYSESSION)
				nd->nd_repstat = NFSERR_OPNOTINSESS;
			else if (i != 0 && op0 != NFSV4OP_SEQUENCE)
				nd->nd_repstat = NFSERR_NOTONLYOP;
			if (nd->nd_repstat != 0) {
				*repp = nfsd_errmap(nd);
				retops++;
				break;
			}
		}

		nd->nd_procnum = op;
		/*
		 * If over flood level, reply NFSERR_RESOURCE, if at the first
		 * Op. (Since a client recovery from NFSERR_RESOURCE can get
		 * really nasty for certain Op sequences, I'll play it safe
		 * and only return the error at the beginning.) The cache
		 * will still function over flood level, but uses lots of
		 * mbufs.)
		 * If nfsrv_mallocmget_limit() returns True, the system is near
		 * to its limit for memory that malloc()/mget() can allocate.
		 */
		if (i == 0 && (nd->nd_rp == NULL ||
		    nd->nd_rp->rc_refcnt == 0) &&
		    (nfsrv_mallocmget_limit() ||
		     NFSD_VNET(nfsrc_tcpsavedreplies) >
		     NFSD_VNET(nfsrc_floodlevel))) {
			if (NFSD_VNET(nfsrc_tcpsavedreplies) >
			    NFSD_VNET(nfsrc_floodlevel))
				printf("nfsd server cache flooded, try "
				    "increasing vfs.nfsd.tcphighwater\n");
			nd->nd_repstat = NFSERR_RESOURCE;
			*repp = nfsd_errmap(nd);
			if (op == NFSV4OP_SETATTR) {
				/*
				 * Setattr replies require a bitmap.
				 * even for errors like these.
				 */
				NFSM_BUILD(tl, u_int32_t *, NFSX_UNSIGNED);
				*tl = 0;
			}
			retops++;
			break;
		}

		/*
		 * Check for the case of SP4_MACH_CRED and an operation in
		 * the allow set.  For these operations, replace nd_cred with
		 * root credentials so that the operation will not fail due
		 * to credentials.
		 * NB: ND_MACHCRED is set by Sequence when the ClientID
		 * specifies LCL_MACHCRED and the RPC is being performed
		 * via krb5i or krb5p using the machine principal.
		 */
		if ((nd->nd_flag & ND_MACHCRED) != 0) {
			if (NFSISSET_OPBIT(&nd->nd_allowops, op)) {
				/* Replace nd_cred with root creds. */
				if (rootcred == NULL)
					rootcred = nfsrv_createrootcred();
				if (savecred == NULL)
					savecred = nd->nd_cred;
				nd->nd_cred = rootcred;
			} else if (savecred != NULL) {
				nd->nd_cred = savecred;
				savecred = NULL;
			}
		}

		if (nfsv4_opflag[op].savereply)
			nd->nd_flag |= ND_SAVEREPLY;
		switch (op) {
		case NFSV4OP_PUTFH:
			error = nfsrv_mtofh(nd, &fh);
			if (error)
				goto nfsmout;
			if ((nd->nd_flag & ND_LASTOP) == 0) {
				/*
				 * Pre-parse the next op#.  If it is
				 * SaveFH, count it and skip to the
				 * next op#, if not the last op#.
				 * nextop is used to determine if
				 * NFSERR_WRONGSEC can be returned,
				 * per RFC5661 Sec. 2.6.
				 */
				do {
					NFSM_DISSECT(tl, uint32_t *,
					    NFSX_UNSIGNED);
					nextop = fxdr_unsigned(int, *tl);
					if (nextop == NFSV4OP_SAVEFH &&
					    i < numops - 1)
						savefhcnt++;
				} while (nextop == NFSV4OP_SAVEFH &&
				    i < numops - 1);
			}
			if (!nd->nd_repstat)
				nfsd_fhtovp(nd, &fh, LK_SHARED, &nvp, &nes,
				    NULL, 0, nextop);
			/* For now, allow this for non-export FHs */
			if (!nd->nd_repstat) {
				if (vp)
					vrele(vp);
				vp = nvp;
				cur_fsid = vp->v_mount->mnt_stat.f_fsid;
				NFSVOPUNLOCK(vp);
				vpnes = nes;
			}
			break;
		case NFSV4OP_PUTPUBFH:
			if (nfs_pubfhset) {
				if ((nd->nd_flag & ND_LASTOP) == 0) {
					/*
					 * Pre-parse the next op#.  If it is
					 * SaveFH, count it and skip to the
					 * next op#, if not the last op#.
					 * nextop is used to determine if
					 * NFSERR_WRONGSEC can be returned,
					 * per RFC5661 Sec. 2.6.
					 */
					do {
						NFSM_DISSECT(tl, uint32_t *,
						    NFSX_UNSIGNED);
						nextop = fxdr_unsigned(int,
						    *tl);
						if (nextop == NFSV4OP_SAVEFH &&
						    i < numops - 1)
							savefhcnt++;
					} while (nextop == NFSV4OP_SAVEFH &&
					    i < numops - 1);
				}
				nfsd_fhtovp(nd, &nfs_pubfh, LK_SHARED, &nvp,
				    &nes, NULL, 0, nextop);
			} else
				nd->nd_repstat = NFSERR_NOFILEHANDLE;
			if (!nd->nd_repstat) {
				if (vp)
					vrele(vp);
				vp = nvp;
				cur_fsid = vp->v_mount->mnt_stat.f_fsid;
				NFSVOPUNLOCK(vp);
				vpnes = nes;
			}
			break;
		case NFSV4OP_PUTROOTFH:
			if (NFSD_VNET(nfs_rootfhset)) {
				if ((nd->nd_flag & ND_LASTOP) == 0) {
					/*
					 * Pre-parse the next op#.  If it is
					 * SaveFH, count it and skip to the
					 * next op#, if not the last op#.
					 * nextop is used to determine if
					 * NFSERR_WRONGSEC can be returned,
					 * per RFC5661 Sec. 2.6.
					 */
					do {
						NFSM_DISSECT(tl, uint32_t *,
						    NFSX_UNSIGNED);
						nextop = fxdr_unsigned(int,
						    *tl);
						if (nextop == NFSV4OP_SAVEFH &&
						    i < numops - 1)
							savefhcnt++;
					} while (nextop == NFSV4OP_SAVEFH &&
					    i < numops - 1);
				}
				nfsd_fhtovp(nd, &NFSD_VNET(nfs_rootfh),
				    LK_SHARED, &nvp, &nes, NULL, 0, nextop);
				if (!nd->nd_repstat) {
					if (vp)
						vrele(vp);
					vp = nvp;
					cur_fsid = vp->v_mount->mnt_stat.f_fsid;
					NFSVOPUNLOCK(vp);
					vpnes = nes;
				}
			} else
				nd->nd_repstat = NFSERR_NOFILEHANDLE;
			break;
		case NFSV4OP_SAVEFH:
			if (vp && NFSVNO_EXPORTED(&vpnes)) {
				nd->nd_repstat = 0;
				/* If vp == savevp, a no-op */
				if (vp != savevp) {
					if (savevp)
						vrele(savevp);
					VREF(vp);
					savevp = vp;
					savevpnes = vpnes;
					save_fsid = cur_fsid;
				}
				if ((nd->nd_flag & ND_CURSTATEID) != 0) {
					nd->nd_savedcurstateid =
					    nd->nd_curstateid;
					nd->nd_flag |= ND_SAVEDCURSTATEID;
				}
			} else {
				nd->nd_repstat = NFSERR_NOFILEHANDLE;
			}
			break;
		case NFSV4OP_RESTOREFH:
			if (savevp) {
				if ((nd->nd_flag & ND_LASTOP) == 0) {
					/*
					 * Pre-parse the next op#.  If it is
					 * SaveFH, count it and skip to the
					 * next op#, if not the last op#.
					 * nextop is used to determine if
					 * NFSERR_WRONGSEC can be returned,
					 * per RFC5661 Sec. 2.6.
					 */
					do {
						NFSM_DISSECT(tl, uint32_t *,
						    NFSX_UNSIGNED);
						nextop = fxdr_unsigned(int,
						    *tl);
						if (nextop == NFSV4OP_SAVEFH &&
						    i < numops - 1)
							savefhcnt++;
					} while (nextop == NFSV4OP_SAVEFH &&
					    i < numops - 1);
				}
				nd->nd_repstat = 0;
				/* If vp == savevp, a no-op */
				if (vp != savevp) {
					if (nfsrv_checkwrongsec(nd, nextop,
					    savevp->v_type))
						nd->nd_repstat =
						    nfsvno_testexp(nd,
						    &savevpnes);
					if (nd->nd_repstat == 0) {
						VREF(savevp);
						vrele(vp);
						vp = savevp;
						vpnes = savevpnes;
						cur_fsid = save_fsid;
					}
				}
				if (nd->nd_repstat == 0 &&
				     (nd->nd_flag & ND_SAVEDCURSTATEID) != 0) {
					nd->nd_curstateid =
					    nd->nd_savedcurstateid;
					nd->nd_flag |= ND_CURSTATEID;
				}
			} else {
				nd->nd_repstat = NFSERR_RESTOREFH;
			}
			break;
		default:
		    /*
		     * Allow a Lookup, Getattr, GetFH, Secinfo on an
		     * non-exported directory if
		     * nfs_rootfhset. Do I need to allow any other Ops?
		     * (You can only have a non-exported vpnes if
		     *  nfs_rootfhset is true. See nfsd_fhtovp())
		     * Allow AUTH_SYS to be used for file systems
		     * exported GSS only for certain Ops, to allow
		     * clients to do mounts more easily.
		     */
		    if (nfsv4_opflag[op].needscfh && vp) {
			if (!NFSVNO_EXPORTED(&vpnes) &&
			    op != NFSV4OP_LOOKUP &&
			    op != NFSV4OP_GETATTR &&
			    op != NFSV4OP_GETFH &&
			    op != NFSV4OP_ACCESS &&
			    op != NFSV4OP_READLINK &&
			    op != NFSV4OP_SECINFO &&
			    op != NFSV4OP_SECINFONONAME)
				nd->nd_repstat = NFSERR_NOFILEHANDLE;
			if (nd->nd_repstat) {
				if (op == NFSV4OP_SETATTR) {
				    /*
				     * Setattr reply requires a bitmap
				     * even for errors like these.
				     */
				    NFSM_BUILD(tl, u_int32_t *,
					NFSX_UNSIGNED);
				    *tl = 0;
				}
				break;
			}
		    }

		    /*
		     * Save the current positions in the mbuf lists so
		     * that a rollback to this location can be done upon a
		     * redo due to a ERELOOKUP return for a operation.
		     */
		    mb = nd->nd_mb;
		    bpos = nd->nd_bpos;
		    bextpg = nd->nd_bextpg;
		    bextpgsiz = nd->nd_bextpgsiz;
		    md = nd->nd_md;
		    dpos = nd->nd_dpos;
tryagain:

		    if (nfsv4_opflag[op].retfh == 1) {
			if (!vp) {
				nd->nd_repstat = NFSERR_NOFILEHANDLE;
				break;
			}
			if (NFSVNO_EXPORTED(&vpnes) && (op == NFSV4OP_LOOKUP ||
			    op == NFSV4OP_LOOKUPP || (op == NFSV4OP_OPEN &&
			    vp->v_type == VDIR))) {
				/* Check for wrong security. */
				rstat = nfsvno_testexp(nd, &vpnes);
				if (rstat != 0) {
					nd->nd_repstat = rstat;
					break;
				}
			}
			VREF(vp);
			if (nfsv4_opflag[op].modifyfs)
				vn_start_write(vp, &temp_mp, V_WAIT);
			error = (*(nfsrv4_ops1[op]))(nd, isdgram, vp,
			    &nvp, (fhandle_t *)fh.nfsrvfh_data, &vpnes);
			if (!error && !nd->nd_repstat) {
			    if (op == NFSV4OP_LOOKUP || op == NFSV4OP_LOOKUPP) {
				new_mp = nvp->v_mount;
				if (fsidcmp(&cur_fsid, &new_mp->mnt_stat.f_fsid) != 0) {
				    /* crossed a server mount point */
				    nd->nd_repstat = nfsvno_checkexp(new_mp,
					nd->nd_nam, &nes, &credanon);
				    if (!nd->nd_repstat)
					nd->nd_repstat = nfsd_excred(nd,
					    &nes, credanon, true);
				    if (credanon != NULL)
					crfree(credanon);
				    if (!nd->nd_repstat) {
					vpnes = nes;
					cur_fsid = new_mp->mnt_stat.f_fsid;
				    }
				}
				/* Lookup ops return a locked vnode */
				NFSVOPUNLOCK(nvp);
			    }
			    if (!nd->nd_repstat) {
				    vrele(vp);
				    vp = nvp;
			    } else
				    vrele(nvp);
			}
			if (nfsv4_opflag[op].modifyfs)
				vn_finished_write(temp_mp);
		    } else if (nfsv4_opflag[op].retfh == 2) {
			if (vp == NULL || savevp == NULL) {
				nd->nd_repstat = NFSERR_NOFILEHANDLE;
				break;
			} else if (fsidcmp(&cur_fsid, &save_fsid) != 0) {
				nd->nd_repstat = NFSERR_XDEV;
				break;
			}
			if (nfsv4_opflag[op].modifyfs)
				vn_start_write(savevp, &temp_mp, V_WAIT);
			if (NFSVOPLOCK(savevp, LK_EXCLUSIVE) == 0) {
				VREF(vp);
				VREF(savevp);
				error = (*(nfsrv4_ops2[op]))(nd, isdgram,
				    savevp, vp, &savevpnes, &vpnes);
			} else
				nd->nd_repstat = NFSERR_PERM;
			if (nfsv4_opflag[op].modifyfs)
				vn_finished_write(temp_mp);
		    } else {
			if (nfsv4_opflag[op].retfh != 0)
				panic("nfsrvd_compound");
			if (nfsv4_opflag[op].needscfh) {
				if (vp != NULL) {
					lktype = nfsv4_opflag[op].lktype;
					if (nfsv4_opflag[op].modifyfs) {
						vn_start_write(vp, &temp_mp,
						    V_WAIT);
						if (op == NFSV4OP_WRITE &&
						    MNT_SHARED_WRITES(temp_mp))
							lktype = LK_SHARED;
					}
					if (NFSVOPLOCK(vp, lktype) == 0)
						VREF(vp);
					else
						nd->nd_repstat = NFSERR_PERM;
				} else {
					nd->nd_repstat = NFSERR_NOFILEHANDLE;
					if (op == NFSV4OP_SETATTR) {
						/*
						 * Setattr reply requires a
						 * bitmap even for errors like
						 * these.
						 */
						NFSM_BUILD(tl, u_int32_t *,
						    NFSX_UNSIGNED);
						*tl = 0;
					}
					break;
				}
				if (nd->nd_repstat == 0) {
					error = (*(nfsrv4_ops0[op]))(nd,
					    isdgram, vp, &vpnes);
					if ((op == NFSV4OP_SECINFO ||
					     op == NFSV4OP_SECINFONONAME) &&
					    error == 0 && nd->nd_repstat == 0) {
						/*
						 * Secinfo and Secinfo_no_name
						 * consume the current FH.
						 */
						vrele(vp);
						vp = NULL;
					}
				}
				if (nfsv4_opflag[op].modifyfs)
					vn_finished_write(temp_mp);
			} else {
				error = (*(nfsrv4_ops0[op]))(nd, isdgram,
				    NULL, &vpnes);
			}
		    }
		}
		if (error) {
			if (error == EBADRPC || error == NFSERR_BADXDR) {
				nd->nd_repstat = NFSERR_BADXDR;
			} else {
				nd->nd_repstat = error;
				printf("nfsv4 comperr0=%d\n", error);
			}
			error = 0;
		}

		if (nd->nd_repstat == ERELOOKUP) {
			/*
			 * Roll back to the beginning of the operation
			 * arguments.
			 */
			nd->nd_md = md;
			nd->nd_dpos = dpos;

			/*
			 * Trim off the bogus reply for this operation
			 * and redo the operation.
			 */
			nfsm_trimtrailing(nd, mb, bpos, bextpg, bextpgsiz);
			nd->nd_repstat = 0;
			nd->nd_flag |= ND_ERELOOKUP;
			goto tryagain;
		}
		nd->nd_flag &= ~ND_ERELOOKUP;

		if (statsinprog != 0) {
			nfsrvd_statend(op, /*bytes*/ 0, /*now*/ NULL,
			    /*then*/ &start_time);
			statsinprog = 0;
		}

		retops++;
		if (nd->nd_repstat) {
			*repp = nfsd_errmap(nd);
			break;
		} else {
			*repp = 0;	/* NFS4_OK */
		}
	}
nfsmout:
	if (statsinprog != 0) {
		nfsrvd_statend(op, /*bytes*/ 0, /*now*/ NULL,
		    /*then*/ &start_time);
		statsinprog = 0;
	}
	if (error) {
		if (error == EBADRPC || error == NFSERR_BADXDR)
			nd->nd_repstat = NFSERR_BADXDR;
		else
			printf("nfsv4 comperr1=%d\n", error);
	}
	if (taglen == -1) {
		NFSM_BUILD(tl, u_int32_t *, 2 * NFSX_UNSIGNED);
		*tl++ = 0;
		*tl = 0;
	} else {
		*retopsp = txdr_unsigned(retops);
	}
	if (vp)
		vrele(vp);
	if (savevp)
		vrele(savevp);
	if (savecred != NULL)
		nd->nd_cred = savecred;
	if (rootcred != NULL)
		crfree(rootcred);
	NFSLOCKV4ROOTMUTEX();
	nfsv4_relref(&nfsv4rootfs_lock);
	NFSUNLOCKV4ROOTMUTEX();

	NFSEXITCODE2(0, nd);
}

/* Create a credential for "root". */
static struct ucred *
nfsrv_createrootcred(void)
{
	struct ucred *cr;

	cr = crget();
	cr->cr_uid = cr->cr_ruid = cr->cr_svuid = UID_ROOT;
	crsetgroups_fallback(cr, 0, NULL, GID_WHEEL);
	cr->cr_rgid = cr->cr_svgid = cr->cr_gid;
	cr->cr_prison = curthread->td_ucred->cr_prison;
	prison_hold(cr->cr_prison);
#ifdef MAC
	mac_cred_associate_nfsd(cr);
#endif
	return (cr);
}
