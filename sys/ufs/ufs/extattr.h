/*-
 * Copyright (c) 1999-2001 Robert N. M. Watson
 * All rights reserved.
 *
 * This software was developed by Robert Watson for the TrustedBSD Project.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */
/*
 * Developed by the TrustedBSD Project.
 * Support for extended file system attributes.
 */

#ifndef _UFS_UFS_EXTATTR_H_
#define	_UFS_UFS_EXTATTR_H_

#define	UFS_EXTATTR_MAGIC		0x00b5d5ec
#define	UFS_EXTATTR_VERSION		0x00000003
#define	UFS_EXTATTR_FSROOTSUBDIR	".attribute"
#define	UFS_EXTATTR_SUBDIR_SYSTEM	"system"
#define	UFS_EXTATTR_SUBDIR_USER		"user"
#define	UFS_EXTATTR_MAXEXTATTRNAME	65	/* including null */

#define	UFS_EXTATTR_ATTR_FLAG_INUSE	0x00000001	/* attr has been set */
#define	UFS_EXTATTR_PERM_KERNEL		0x00000000
#define	UFS_EXTATTR_PERM_ROOT		0x00000001
#define	UFS_EXTATTR_PERM_OWNER		0x00000002
#define	UFS_EXTATTR_PERM_ANYONE		0x00000003

#define	UFS_EXTATTR_UEPM_INITIALIZED	0x00000001
#define	UFS_EXTATTR_UEPM_STARTED	0x00000002

#define	UFS_EXTATTR_CMD_START		0x00000001
#define	UFS_EXTATTR_CMD_STOP		0x00000002
#define	UFS_EXTATTR_CMD_ENABLE		0x00000003
#define	UFS_EXTATTR_CMD_DISABLE		0x00000004

struct ufs_extattr_fileheader {
	u_int	uef_magic;	/* magic number for sanity checking */
	u_int	uef_version;	/* version of attribute file */
	u_int	uef_size;	/* size of attributes, w/o header */
};

struct ufs_extattr_header {
	u_int	ueh_flags;	/* flags for attribute */
	u_int	ueh_len;	/* local defined length; <= uef_size */
	u_int32_t	ueh_i_gen;	/* generation number for sanity */
	/* data follows the header */
};

#ifdef _KERNEL

#ifdef MALLOC_DECLARE
MALLOC_DECLARE(M_EXTATTR);
#endif

struct vnode;
LIST_HEAD(ufs_extattr_list_head, ufs_extattr_list_entry);
struct ufs_extattr_list_entry {
	LIST_ENTRY(ufs_extattr_list_entry)	uele_entries;
	struct ufs_extattr_fileheader		uele_fileheader;
	int	uele_attrnamespace;
	char	uele_attrname[UFS_EXTATTR_MAXEXTATTRNAME];
	struct vnode	*uele_backing_vnode;
};

struct lock;
struct ucred;
struct ufs_extattr_per_mount {
	struct lock	uepm_lock;
	struct ufs_extattr_list_head	uepm_list;
	struct ucred	*uepm_ucred;
	int	uepm_flags;
};

void	ufs_extattr_uepm_init(struct ufs_extattr_per_mount *uepm);
void	ufs_extattr_uepm_destroy(struct ufs_extattr_per_mount *uepm);
int	ufs_extattr_start(struct mount *mp, struct thread *td);
int	ufs_extattr_autostart(struct mount *mp, struct thread *td);
int	ufs_extattr_stop(struct mount *mp, struct thread *td);
int	ufs_extattrctl(struct mount *mp, int cmd, struct vnode *filename,
	    int attrnamespace, const char *attrname, struct thread *td);
int	ufs_vop_getextattr(struct vop_getextattr_args *ap);
int	ufs_vop_setextattr(struct vop_setextattr_args *ap);
void	ufs_extattr_vnode_inactive(struct vnode *vp, struct thread *td);

#endif /* !_KERNEL */

#endif /* !_UFS_UFS_EXTATTR_H_ */
