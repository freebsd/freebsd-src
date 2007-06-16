/*	$NetBSD: tmpfs_vnops.h,v 1.7 2005/12/03 17:34:44 christos Exp $	*/

/*
 * Copyright (c) 2005 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Julio M. Merino Vidal, developed as part of Google's Summer of Code
 * 2005 program.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _FS_TMPFS_TMPFS_VNOPS_H_
#define _FS_TMPFS_TMPFS_VNOPS_H_

#if !defined(_KERNEL)
#error not supposed to be exposed to userland.
#endif

/* --------------------------------------------------------------------- */

/*
 * Declarations for tmpfs_vnops.c.
 */

extern struct vop_vector tmpfs_vnodeop_entries;

vop_cachedlookup_t tmpfs_lookup;
vop_create_t	tmpfs_create;
vop_mknod_t	tmpfs_mknod;
vop_open_t	tmpfs_open;
vop_close_t	tmpfs_close;
vop_access_t	tmpfs_access;
vop_getattr_t	tmpfs_getattr;
vop_setattr_t	tmpfs_setattr;
vop_read_t	tmpfs_read;
vop_write_t	tmpfs_write;
vop_fsync_t	tmpfs_fsync;
vop_remove_t	tmpfs_remove;
vop_link_t	tmpfs_link;
vop_rename_t	tmpfs_rename;
vop_mkdir_t	tmpfs_mkdir;
vop_rmdir_t	tmpfs_rmdir;
vop_symlink_t	tmpfs_symlink;
vop_readdir_t	tmpfs_readdir;
vop_readlink_t	tmpfs_readlink;
vop_inactive_t	tmpfs_inactive;
vop_reclaim_t	tmpfs_reclaim;
vop_print_t	tmpfs_print;
vop_pathconf_t	tmpfs_pathconf;
vop_advlock_t	tmpfs_advlock;
vop_vptofh_t	tmpfs_vptofh;

/* --------------------------------------------------------------------- */

#endif /* _FS_TMPFS_TMPFS_VNOPS_H_ */
