/*-
 * Copyright (c) 2001 Networks Associates Technology, Inc.
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
 * $Id: lomacfs_subr.c,v 1.24 2001/11/05 20:57:41 tfraser Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/namei.h>

#include <vm/vm.h>
#include <vm/vm_extern.h>

#include "lomacfs.h"
#include "kernel_plm.h"

int
lomacfs_node_alloc(struct mount *mp, struct componentname *cnp,
    struct vnode *dvp, struct vnode *lowervp, struct vnode **vpp) {
	lomac_object_t lobj;
	struct thread *td = curthread;
	struct vnode *vp;
	struct lomac_node *lp;
	lattr_t subjlattr, objlattr;
	int error;

	KASSERT((cnp == NULL) == (dvp == NULL),
	    ("lomacfs_node_alloc: dvp and cnp do not match"));
	lp = malloc(sizeof(*lp), M_LOMACFS, M_WAITOK);
	if (dvp != NULL) {
		error = cache_lookup(dvp, vpp, cnp);
		if (error == -1) { /* lost the race; return EEXIST and the vp */
			vput(lowervp);
			error = vget(*vpp, LK_EXCLUSIVE, td);
			free(lp, M_LOMACFS);
			if (error) {
				*vpp = NULL;
				return (error);
			} else
				return (EEXIST);
		}
	}
	error = getnewvnode(VT_NULL, mp, lomacfs_vnodeop_p, vpp);
	if (error) {
		vput(lowervp);
		free(lp, M_LOMACFS);
		return (error);
	}
	vp = *vpp;

	vp->v_type = lowervp != NULL ? lowervp->v_type : VBAD;
	if (vp->v_type == VCHR)
		vp->v_rdev = lowervp->v_rdev;
	vp->v_data = lp;
	lp->ln_vp = vp;
	lp->ln_lowervp = lowervp;
	if (lowervp != NULL)
		vhold(lowervp);
	get_subject_lattr(curthread->td_proc, &subjlattr);
	lp->ln_flags = 0;
	lomac_plm_init_lomacfs_vnode(dvp, vp, cnp, &subjlattr);
	/* retrieve the just-initialized attributes */
	lobj.lo_type = LO_TYPE_LVNODE;
	lobj.lo_object.vnode = vp;
	get_object_lattr(&lobj, &objlattr);
	/* propogate the lattr to the underlying vnode */
	lobj.lo_type = LO_TYPE_UVNODE;
	lobj.lo_object.vnode = lowervp;
	set_object_lattr(&lobj, objlattr);
#if defined(LOMAC_DEBUG_INCNAME)
	if (cnp == NULL)
		strncpy(lp->ln_name, "/", sizeof(lp->ln_name));
	else {
		strncpy(lp->ln_name, cnp->cn_nameptr, cnp->cn_namelen);
		lp->ln_name[cnp->cn_namelen] = '\0';
	}
#endif
	error = vn_lock(vp, LK_EXCLUSIVE | LK_RETRY | LK_THISLAYER, td);
	if (error)
		panic("lomacfs_node_alloc: can't lock new vnode\n");
	if (cnp == NULL)
		vp->v_flag |= VROOT;
	else if (cnp->cn_flags & MAKEENTRY)
		cache_enter(dvp, vp, cnp);

#if defined(LOMAC_DEBUG_NODE_ALLOC)
	printf("lomacfs: made vp %p for lvp %p \"%.*s\" in dvp %p from %s\n",
	    vp, lowervp, cnp ? (int)cnp->cn_namelen : 0,
	    cnp ? cnp->cn_nameptr : "", dvp,
	    lowervp != NULL ? lowervp->v_mount->mnt_stat.f_mntonname : "");
#endif

	return (0);
}
