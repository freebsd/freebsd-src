/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
/*
 * Copyright 2008 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*	Copyright (c) 1983, 1984, 1985, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*
 * University Copyright- Copyright (c) 1982, 1986, 1988
 * The Regents of the University of California
 * All Rights Reserved
 *
 * University Acknowledgment- Portions of this document are derived from
 * software developed by the University of California, Berkeley, and its
 * contributors.
 */


#pragma ident	"%Z%%M%	%I%	%E% SMI"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/proc.h>
#include <sys/vnode.h>

/* Extensible attribute (xva) routines. */

/*
 * Zero out the structure, set the size of the requested/returned bitmaps,
 * set AT_XVATTR in the embedded vattr_t's va_mask, and set up the pointer
 * to the returned attributes array.
 */
void
xva_init(xvattr_t *xvap)
{
	bzero(xvap, sizeof (xvattr_t));
	xvap->xva_mapsize = XVA_MAPSIZE;
	xvap->xva_magic = XVA_MAGIC;
	xvap->xva_vattr.va_mask = AT_XVATTR;
	xvap->xva_rtnattrmapp = &(xvap->xva_rtnattrmap)[0];
}

/*
 * If AT_XVATTR is set, returns a pointer to the embedded xoptattr_t
 * structure.  Otherwise, returns NULL.
 */
xoptattr_t *
xva_getxoptattr(xvattr_t *xvap)
{
	xoptattr_t *xoap = NULL;
	if (xvap->xva_vattr.va_mask & AT_XVATTR)
		xoap = &xvap->xva_xoptattrs;
	return (xoap);
}

static STAILQ_HEAD(, vnode) vn_rele_async_list;
static struct mtx vn_rele_async_lock;
static struct cv vn_rele_async_cv;
static int vn_rele_list_length;
static int vn_rele_async_thread_exit;

typedef struct  {
	struct vnode *stqe_next;
} vnode_link_t;

/*
 * Like vn_rele() except if we are going to call VOP_INACTIVE() then do it
 * asynchronously using a taskq. This can avoid deadlocks caused by re-entering
 * the file system as a result of releasing the vnode. Note, file systems
 * already have to handle the race where the vnode is incremented before the
 * inactive routine is called and does its locking.
 *
 * Warning: Excessive use of this routine can lead to performance problems.
 * This is because taskqs throttle back allocation if too many are created.
 */
void
vn_rele_async(vnode_t *vp, taskq_t *taskq /* unused */)
{
	
	KASSERT(vp != NULL, ("vrele: null vp"));
	VFS_ASSERT_GIANT(vp->v_mount);
	VI_LOCK(vp);

	if (vp->v_usecount > 1 || ((vp->v_iflag & VI_DOINGINACT) &&
	    vp->v_usecount == 1)) {
		vp->v_usecount--;
		vdropl(vp);
		return;
	}	
	if (vp->v_usecount != 1) {
#ifdef DIAGNOSTIC
		vprint("vrele: negative ref count", vp);
#endif
		VI_UNLOCK(vp);
		panic("vrele: negative ref cnt");
	}
	/*
	 * We are exiting
	 */
	if (vn_rele_async_thread_exit != 0) {
		vrele(vp);
		return;
	}
	
	mtx_lock(&vn_rele_async_lock);

	/*  STAILQ_INSERT_TAIL 			*/
	(*(vnode_link_t *)&vp->v_cstart).stqe_next = NULL;
	*vn_rele_async_list.stqh_last = vp;
	vn_rele_async_list.stqh_last =
	    &((vnode_link_t *)&vp->v_cstart)->stqe_next;

	/****************************************/
	vn_rele_list_length++;
	if ((vn_rele_list_length % 100) == 0)
		cv_signal(&vn_rele_async_cv);
	mtx_unlock(&vn_rele_async_lock);
	VI_UNLOCK(vp);
}

static void
vn_rele_async_init(void *arg)
{

	mtx_init(&vn_rele_async_lock, "valock", NULL, MTX_DEF);
	STAILQ_INIT(&vn_rele_async_list);

	/* cv_init(&vn_rele_async_cv, "vacv"); */
	vn_rele_async_cv.cv_description = "vacv";
	vn_rele_async_cv.cv_waiters = 0;
}

void
vn_rele_async_fini(void)
{

	mtx_lock(&vn_rele_async_lock);
	vn_rele_async_thread_exit = 1;
	cv_signal(&vn_rele_async_cv);
	while (vn_rele_async_thread_exit != 0)
		cv_wait(&vn_rele_async_cv, &vn_rele_async_lock);
	mtx_unlock(&vn_rele_async_lock);
	mtx_destroy(&vn_rele_async_lock);
}


static void
vn_rele_async_cleaner(void)
{
	STAILQ_HEAD(, vnode) vn_tmp_list;
	struct vnode *curvnode;

	STAILQ_INIT(&vn_tmp_list);
	mtx_lock(&vn_rele_async_lock);
	while (vn_rele_async_thread_exit == 0) {
		STAILQ_CONCAT(&vn_tmp_list, &vn_rele_async_list);
		vn_rele_list_length = 0;
		mtx_unlock(&vn_rele_async_lock);
		
		while (!STAILQ_EMPTY(&vn_tmp_list)) {
			curvnode = STAILQ_FIRST(&vn_tmp_list);

			/*   STAILQ_REMOVE_HEAD */
			STAILQ_FIRST(&vn_tmp_list) =
			    ((vnode_link_t *)&curvnode->v_cstart)->stqe_next;
			if (STAILQ_FIRST(&vn_tmp_list) == NULL)
				         vn_tmp_list.stqh_last = &STAILQ_FIRST(&vn_tmp_list);
			/***********************/
			vrele(curvnode);
		}
		mtx_lock(&vn_rele_async_lock);
		if (vn_rele_list_length == 0)
			cv_timedwait(&vn_rele_async_cv, &vn_rele_async_lock,
			    hz/10);
	}

	vn_rele_async_thread_exit = 0;
	cv_broadcast(&vn_rele_async_cv);
	mtx_unlock(&vn_rele_async_lock);
	thread_exit();
}

static struct proc *vn_rele_async_proc;
static struct kproc_desc up_kp = {
	"vaclean",
	vn_rele_async_cleaner,
	&vn_rele_async_proc
};
SYSINIT(vaclean, SI_SUB_KTHREAD_UPDATE, SI_ORDER_FIRST, kproc_start, &up_kp);
SYSINIT(vn_rele_async_setup, SI_SUB_VFS, SI_ORDER_FIRST, vn_rele_async_init, NULL);
