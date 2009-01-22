/*-
 * Copyright (c) 2007 Pawel Jakub Dawidek <pjd@FreeBSD.org>
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 $ $FreeBSD$
 */

#ifndef _OPENSOLARIS_SYS_POLICY_H_
#define	_OPENSOLARIS_SYS_POLICY_H_

#include <sys/param.h>

#ifdef _KERNEL

#include <sys/vnode.h>

struct mount;
struct ucred;
struct vattr;
struct vnode;

int	secpolicy_nfs(struct ucred *cred);
int	secpolicy_zfs(struct ucred *cred);
int	secpolicy_sys_config(struct ucred *cred, int checkonly);
int	secpolicy_zinject(struct ucred *cred);
int	secpolicy_fs_unmount(struct ucred *cred, struct mount *vfsp);
int	secpolicy_basic_link(struct vnode *vp, struct ucred *cred);
int	secpolicy_vnode_owner(struct vnode *vp, cred_t *cred, uid_t owner);
int	secpolicy_vnode_chown(struct vnode *vp, cred_t *cred,
	    boolean_t check_self);
int	secpolicy_vnode_stky_modify(struct ucred *cred);
int	secpolicy_vnode_remove(struct vnode *vp, struct ucred *cred);
int	secpolicy_vnode_access(struct ucred *cred, struct vnode *vp,
	    uint64_t owner, accmode_t accmode);
int	secpolicy_vnode_setdac(struct vnode *vp, struct ucred *cred,
	    uid_t owner);
int	secpolicy_vnode_setattr(struct ucred *cred, struct vnode *vp,
	    struct vattr *vap, const struct vattr *ovap, int flags,
	    int unlocked_access(void *, int, struct ucred *), void *node);
int	secpolicy_vnode_create_gid(struct ucred *cred);
int	secpolicy_vnode_setids_setgids(struct vnode *vp, struct ucred *cred,
	    gid_t gid);
int	secpolicy_vnode_setid_retain(struct vnode *vp, struct ucred *cred,
	    boolean_t issuidroot);
void	secpolicy_setid_clear(struct vattr *vap, struct vnode *vp,
	    struct ucred *cred);
int	secpolicy_setid_setsticky_clear(struct vnode *vp, struct vattr *vap,
	    const struct vattr *ovap, struct ucred *cred);
int	secpolicy_fs_owner(struct mount *vfsp, struct ucred *cred);
int	secpolicy_fs_mount(cred_t *cr, vnode_t *mvp, struct mount *vfsp);
void	secpolicy_fs_mount_clearopts(cred_t *cr, struct mount *vfsp);
int	secpolicy_xvattr(xvattr_t *xvap, uid_t owner, cred_t *cr, vtype_t vtype);

#endif	/* _KERNEL */

#endif	/* _OPENSOLARIS_SYS_POLICY_H_ */
