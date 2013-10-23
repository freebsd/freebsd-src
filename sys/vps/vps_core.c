/*-
 * Copyright (c) 2009-2013 Klaus P. Ohrhallinger <k@7he.at>
 * All rights reserved.
 *
 * Development of this software was partly funded by:
 *    TransIP.nl <http://www.transip.nl/>
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
 */

static const char vpsid[] =
    "$Id: vps_core.c 174 2013-06-12 15:39:22Z klaus $";

#include <sys/cdefs.h>

#include <sys/param.h>
#include <sys/types.h>
#include <sys/limits.h>
#include <sys/kernel.h>
#include <sys/linker.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/conf.h>
#include <sys/libkern.h>
#include <sys/module.h>
#include <sys/sysctl.h>
#include <sys/file.h>
#include <sys/proc.h>
#include <sys/ucred.h>
#include <sys/ioccom.h>
#include <sys/socket.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <sys/fcntl.h>
#include <sys/namei.h>
#include <sys/syscallsubr.h>
#include <sys/resourcevar.h>
#include <sys/sysproto.h>
#include <sys/reboot.h>
#include <sys/sysent.h>
#include <sys/sleepqueue.h>
#include <sys/syscall.h>
#include <sys/systm.h>
#include <sys/filedesc.h>
#include <sys/buf.h>
#include <sys/imgact.h>
#include <sys/vmmeter.h>
#include <sys/jail.h>
#include <sys/loginclass.h>
#include <sys/rwlock.h>

#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pager.h>
#include <vm/vm_kern.h>

#include <machine/pcb.h>

#include <net/if.h>
#include <net/if_clone.h>
#include <netinet/in.h>

#include <security/mac/mac_framework.h>

#ifdef DDB
#include <ddb/ddb.h>
#else
#define db_trace_thread(x,y)
#endif

extern struct prison prison0;

#include "vps_user.h"
#include "vps.h"
#include "vps2.h"
#include "vps_int.h"
#include "vps_account.h"
#include <machine/vps_md.h>

#ifdef DIAGNOSTIC
int vps_debug_core = 1;

SYSCTL_INT(_debug, OID_AUTO, vps_core_debug, CTLFLAG_RW,
    &vps_debug_core, 0, "");
#endif

VPS_DECLARE(struct unrhdr *, pts_pool);

static int vps_sysctl_reclaim(SYSCTL_HANDLER_ARGS);
SYSCTL_PROC(_debug, OID_AUTO, vps_reclaim, CTLTYPE_INT | CTLFLAG_RW,
    NULL, 0, vps_sysctl_reclaim, "I", "set to call vps_sysctl_reclaim()");

/*
 * Function pointers filled by the vps_* modules.
 */

struct vps_functions _vps_func;
struct vps_functions *vps_func = &_vps_func;

MALLOC_DEFINE(M_VPS_CORE, "vps_core",
    "Virtual Private Systems core memory");

static int last_vps_id;
struct vps_list_head vps_head;

struct vps *vps0;

struct sx vps_all_lock;

void vps_proc_release_timeout(void *);
void vps_proc_release_taskq(void *, int);

static int vps_prison_alloc(struct vps *, struct vps *);
static void vps_prison_destroy(struct vps *);

/* vps/vps_pager.c */
void vps_pager_lowmem(void *arg);

/* net/route.c */
void vnet_route_init(const void *);
void vnet_route_uninit(const void *);

static void
vps_init(void *unused)
{

	LIST_INIT(&vps_head);

	/* init all the global locks here */
	sx_init(&vps_all_lock, "lock of all vps instances");

	sx_xlock(&vps_all_lock);
	vps0 = vps_alloc(NULL, NULL, "", NULL);
	sx_xunlock(&vps_all_lock);

	curthread->td_vps = vps0;

	vps_console_init();

	DBGCORE("WARNING: VPS VIRTUAL PRIVATE SYSTEMS ENABLED. "
	    "HIGHLY EXPERIMENTAL!\n");
}

SYSINIT(vps, SI_SUB_VPS, SI_ORDER_FIRST, vps_init, NULL);

/*
 * Shutdown all running instances except vps0.
 * Called from reboot() on global shutdown.
 */
int
vps_shutdown_all(struct thread *td)
{
	struct vps *vps;

	KASSERT(td->td_vps == vps0,
	 	("%s: calling vps != vps0 (td->td_vps=%p)\n",
		__func__, td->td_vps));

	printf("%s: VPS GLOBAL SHUTDOWN\n", __func__);

	sx_xlock(&vps_all_lock);
        LIST_FOREACH(vps, &vps_head, vps_all)
		if (vps != vps0 &&
		    vps->vps_status != VPS_ST_DEAD) {
			sx_xlock(&vps->vps_lock);
			vps_free_locked(vps);
			sx_xunlock(&vps->vps_lock);
		}
	sx_xunlock(&vps_all_lock);

	/*
	 * At this point all processes are gone.
	 * Due to TIME_WAIT sockets or similar things
	 * some VPS instances might be still alive,
	 * but that doesn't matter for global reboot.
	 */

        return (0);
}

/*
 * We are called with allvps lock held exclusively.
 */
struct vps *
vps_alloc(struct vps *vps_parent, struct vps_param *vps_pr,
    char *vps_name, int *errorval)
{
	struct vps *vps;
	struct vps *vps_save;
	struct nameidata nd;
	char *path;
	int error;
	char *tmpstr = NULL;
	struct thread *td = curthread;

	if (errorval)
		*errorval = 0;

	/* Only the first call is valid with vps_parent == NULL. */
	if (vps_parent == NULL && ! LIST_EMPTY(&vps_head)) {
		if (errorval)
			*errorval = EINVAL;
		return (NULL);
	}

	/* Allocating a vps instance in a jail (other than prison0)
	   is forbidden. */
	if (td->td_ucred != NULL && jailed(td->td_ucred)) {
		*errorval = EPERM;
		return (NULL);
	}

	if (vm_page_count_min()) {
		printf("%s: low on memory: v_free_min=%u > "
		    "(v_free_count=%u + v_cache_count=%u)\n",
		    __func__, cnt.v_free_min, cnt.v_free_count,
		    cnt.v_cache_count);
		if (errorval)
			*errorval = ENOMEM;
		return (NULL);
	}
	DBGCORE("%s: v_free_min=%u v_free_count=%u + v_cache_count=%u\n",
	    __func__, cnt.v_free_min, cnt.v_free_count, cnt.v_cache_count);

	vps = malloc(sizeof(*vps), M_VPS_CORE, M_WAITOK | M_ZERO);

	vps->vps_id = last_vps_id++;
	LIST_INIT(&vps->vps_child_head);
	snprintf(vps->vps_name, sizeof(vps->vps_name), "%s", vps_name);
	vps->vps_parent = vps_parent;

	mtx_init(&vps->vps_refcnt_lock, "vps_refcnt_lock", NULL, MTX_SPIN);
	refcount_init(&vps->vps_refcnt, 1);
#ifdef INVARIANTS
	TAILQ_INIT(&vps->vps_ref_head);
#endif

	/* is freed in vps_destroy() */
	tmpstr = malloc(0x100, M_VPS_CORE, M_WAITOK);
	snprintf(tmpstr, 0x100, "vps instance %p lock", vps);
	sx_init(&vps->vps_lock, tmpstr);
	vps->vps_lock_name = tmpstr;
	sx_xlock(&vps->vps_lock);

	if (vps_parent) {

		/* Alloc vnet. Apparently always succeeds. */
		vps->vnet = vnet_alloc();

	   	vps->vps_ucred = crget();
		crcopy(vps->vps_ucred, td->td_ucred);
		vps_ref(vps, vps->vps_ucred);
		vps_deref(vps->vps_ucred->cr_vps, vps->vps_ucred);
		vps->vps_ucred->cr_vps = vps;

		getbintime(&VPS_VPS(vps, boottimebin));
		bintime2timeval(&VPS_VPS(vps, boottimebin),
		    &VPS_VPS(vps, boottime));

		memset(VPS_VPS(vps, hostname), 0, sizeof(VPS_VPS(vps,
		    hostname)));
		memset(VPS_VPS(vps, domainname), 0, sizeof(VPS_VPS(vps,
		    domainname)));
		memset(VPS_VPS(vps, hostuuid), 0, sizeof(VPS_VPS(vps,
		    hostuuid)));

		/* Default is nprocs=1 for vps0, so set to 0 here. */
		VPS_VPS(vps, nprocs) = 0;
		VPS_VPS(vps, nprocs_zomb) = 0;

		VPS_VPS(vps, vmaxproc) = VPS_VPS(vps0, vmaxproc);
		VPS_VPS(vps, vmaxprocperuid) = VPS_VPS(vps0,
		    vmaxprocperuid);

		vps_save = td->td_vps;
		td->td_vps = vps;

		procinit();

		KASSERT(VPS_VPS(vps, initpgrp) == NULL,
		    ("%s: initpgrp != NULL", __func__));
		KASSERT(VPS_VPS(vps, initproc) == NULL,
		    ("%s: initproc != NULL", __func__));

		td->td_vps = vps_save;

		vps_priv_setdefault(vps, vps_pr);

		(void)vps_devfs_ruleset_create(vps);

		/* Filesystem root. */
		if (vps_pr && vps_pr->fsroot[0])
			path = vps_pr->fsroot;
		else
			path = "/";

		NDINIT(&nd, LOOKUP, FOLLOW | LOCKLEAF, UIO_SYSSPACE,
		    path, td);
		if ((error = namei(&nd))) {
	   		DBGCORE("%s: namei(path=[%s]): %d\n",
			    __func__, path, error);
			if (errorval)
				*errorval = error;
	   		goto fail;
		}
		vps->_rootvnode = nd.ni_vp;
		strncpy(vps->_rootpath, path, sizeof(vps->_rootpath));
		VOP_UNLOCK(nd.ni_vp, 0);
		NDFREE(&nd, NDF_ONLY_PNBUF);

		vps_prison_alloc(vps_parent, vps);
		prison_hold(VPS_VPS(vps, prison0));
		prison_free(vps->vps_ucred->cr_prison);
		vps->vps_ucred->cr_prison = VPS_VPS(vps, prison0);

		sx_xlock(&vps_parent->vps_lock);
		LIST_INSERT_HEAD(&vps_parent->vps_child_head, vps,
		    vps_sibling);
		sx_xunlock(&vps_parent->vps_lock);

	} else {

		vps->vnet = vnet0;

	   	vps->vps_ucred = crget();
		vps->vps_ucred->cr_ngroups = 1;
		vps->vps_ucred->cr_prison = &prison0;
		prison_hold(&prison0);
		/*
		vps->vps_ucred->cr_uidinfo = uifind(0);
		vps->vps_ucred->cr_ruidinfo = uifind(0);
		vps->vps_ucred->cr_loginclass = loginclass_find("default");
		*/
		vps->vps_ucred->cr_uidinfo = NULL;
		vps->vps_ucred->cr_ruidinfo = NULL;
		vps->vps_ucred->cr_loginclass = NULL;
	
		vps_ref(vps, vps->vps_ucred);
		vps->vps_ucred->cr_vps = vps;
	
		//vps->_maxproc = G_maxproc;
		VPS_VPS(vps, vmaxproc) = maxproc;
		VPS_VPS(vps, vmaxprocperuid) = maxprocperuid;

		/* We grant every privilege for vps0. */
		memset(vps->priv_allow_set, -1,
		    sizeof(vps->priv_allow_set));
		memset(vps->priv_impl_set, -1,
		    sizeof(vps->priv_impl_set));
	
		/* Same goes for network. */
		vps->vps_ip4_cnt = 1;
		vps->vps_ip4 = malloc(sizeof(*vps->vps_ip4) * 1,
		    M_VPS_CORE, M_WAITOK);
		vps->vps_ip4->addr.s_addr = 0;
		vps->vps_ip4->mask.s_addr = 0;
	
		vps->vps_ip6_cnt = 1;
		vps->vps_ip6 = malloc(sizeof(*vps->vps_ip6) * 1,
		    M_VPS_CORE, M_WAITOK);
		memset(&vps->vps_ip6->addr, 0,
		    sizeof(vps->vps_ip6->addr));
		vps->vps_ip6->plen = 0;
	
		VPS_VPS(vps, prison0) = &prison0;
	
		DBGCORE("%s: vps=%p vmaxproc=%d\n", __func__,
		    vps, VPS_VPS(vps, vmaxproc));

	}

	LIST_INSERT_HEAD(&vps_head, vps, vps_all);

	vps->vps_acc = malloc(sizeof(*vps->vps_acc),
	    M_VPS_CORE, M_WAITOK | M_ZERO);
	//mtx_init(&vps->vps_acc->lock, "vps accounting", NULL, MTX_DEF);
	mtx_init(&vps->vps_acc->lock, "vps accounting", NULL, MTX_SPIN);
	vps->vps_acc->vps = vps;

	VPS_VPS(vps, pts_pool) = new_unrhdr(0, INT_MAX, NULL);

	/* Needs V_pts_pool initialized. */
	if (vps_parent)
		vps_console_alloc(vps, curthread);

	vps_save = td->td_vps;
	td->td_vps = vps;

	EVENTHANDLER_INVOKE(vps_alloc, vps);

	td->td_vps = vps_save;

	vps->vps_status = VPS_ST_RUNNING;

	sx_xunlock(&vps->vps_lock);

	/* XXX stuff that should be moved somewhere appropriate */
	TAILQ_INIT(&VPS_VPS(vps, allprison));

	if (errorval)
		*errorval = 0;
	return (vps);

  fail:

	if (vps->vps_ucred)
		crfree(vps->vps_ucred);

	if (vps && VPS_VPS(vps, initpgrp)) {
		free(VPS_VPS(vps, initpgrp)->pg_session, M_SESSION);
		free(VPS_VPS(vps, initpgrp), M_PGRP);
	}

	if (vps && VPS_VPS(vps, proc_lock_names)) {
		vps_save = td->td_vps;
		td->td_vps = vps;
		procuninit();
		td->td_vps = vps_save;
	}

	if (vps->vps_acc) {
		mtx_destroy(&vps->vps_acc->lock);
		free(vps->vps_acc, M_VPS_CORE);
	}

	if (vps->vnet) {
		/* see vps_free() */
		VPS_VPS(vps, loif) = NULL;
		vnet_destroy(vps->vnet);
	}

	if (tmpstr)
		free(tmpstr, M_VPS_CORE);

	if (vps) {
		sx_xunlock(&vps->vps_lock);
		sx_destroy(&vps->vps_lock);
		mtx_destroy(&vps->vps_refcnt_lock);
		free(vps, M_VPS_CORE);
	}

	return (NULL);
}


/*
 * Unmount filesystems that were mounted by the vps instance.
 */
int
vps_unmount_all(struct vps *vps)
{
	struct mount *mp;
	int error;
	struct thread *td = curthread;
	struct vps *save_vps = td->td_vps;
	struct ucred *save_ucred = td->td_ucred;

	td->td_vps = vps;
	td->td_ucred = vps->vps_ucred;

	DBGCORE("%s: td->td_ucred=%p vps->vps_ucred=%p\n",
	    __func__, td->td_ucred, vps->vps_ucred);

  unmount_restart:
	mtx_lock(&mountlist_mtx);
	TAILQ_FOREACH(mp, &mountlist, mnt_list) {
		if (mp->mnt_vps != vps)
			continue;
		mtx_lock(&Giant);
		mtx_unlock(&mountlist_mtx);
		DBGCORE("%s: attempting unmount of mp=%p\n", __func__, mp);
		if ((error = dounmount(mp, MNT_FORCE, curthread)))
			DBGCORE("%s: dounmount() failed: %d\n",
			    __func__, error);
		mtx_unlock(&Giant);
		goto unmount_restart;
	}
	mtx_unlock(&mountlist_mtx);

	td->td_vps = save_vps;
	td->td_ucred = save_ucred;

	return (0);
}

int
vps_free(struct vps *vps)
{
	int error;

	sx_xlock(&vps->vps_lock);
	error = vps_free_locked(vps);
	sx_xunlock(&vps->vps_lock);

	return (error);
}

/*
 * We are called with vps locked exclusively.
 */
int
vps_free_locked(struct vps *vps)
{
	struct ifnet *ifp, *ifp2;
	struct vps *vps_save;
	struct proc *p, *p2;
	char *tmpstr;
	int nonzombprocs;
	int systemprocs;
	int error;
	int i;

	sx_assert(&vps->vps_lock, SA_XLOCKED);

	KASSERT(vps->vps_status != VPS_ST_DEAD,
	    ("%s: vps=%p vps->vps_status == VPS_ST_DEAD\n", __func__, vps));

	if (vps->vps_status == VPS_ST_SUSPENDED)
		return (EBUSY);

	/*
	 * Status can be VPS_ST_INITISDYING here,
	 * just treat is like _RUNNING
	 */

	vps_save = curthread->td_vps;
	curthread->td_vps = vps;

  again:
	error = 0;
	nonzombprocs = 0;
	systemprocs = 0;

	sx_xlock(&V_proctree_lock);
	sx_xlock(&V_allproc_lock);

	FOREACH_PROC_IN_SYSTEM(p) {
		nonzombprocs++;
		if (p->p_flag & P_SYSTEM)
			systemprocs++;
	}

	if (systemprocs > 0) {
		/*
		 * XXX e.g. kernel procs created via mdconfig.
		 */
		DBGCORE("%s: vps %p [%s] has %d procs with P_SYSTEM "
		    "flag.\n", __func__, vps, vps->vps_name, systemprocs);
		sx_xunlock(&V_allproc_lock);
		sx_xunlock(&V_proctree_lock);

		error = EBUSY;
		goto out;
	}

	if (nonzombprocs > 0) {
		/*
		 * In this case we have to do a broadcast kill
		 * and sleep until there are no more active processes.
		 */
		sx_xunlock(&V_allproc_lock);
		sx_xunlock(&V_proctree_lock);

		sx_downgrade(&vps_all_lock);

		vps_proc_signal(vps, -1, SIGKILL);

		/* Sleep. */
		do {
	   		pause("vpskll", hz / 10);
		} while (sx_try_upgrade(&vps_all_lock) == 0);

		/*
		 * XXX If we get stuck here, just clean up as much we can,
		 *     keep system in a safe state and continue.
		 */

		goto again;
	}

	/*
	 * At this point there MUST NOT be any non-zombie processes,
	 * as we could release a parent of a still alive child !
	 */
	LIST_FOREACH_SAFE(p, &V_zombproc, p_list, p2)
		vps_proc_release(vps, p);

	i = 0;
	do {
		pause("vpskll", hz / 10);
	} while (++i < 100 && VPS_VPS(vps, nprocs) > 0);

	sx_xunlock(&V_allproc_lock);
	sx_xunlock(&V_proctree_lock);

	if (VPS_VPS(vps, nprocs)) {
		DBGCORE("%s: nprocs still > 0: %d\n",
		    __func__, VPS_VPS(vps, nprocs));
		error = EBUSY;
		goto out;
	}

	EVENTHANDLER_INVOKE(vps_free, vps);

	vps->vps_status = VPS_ST_DEAD;
	tmpstr = malloc(MAXHOSTNAMELEN, M_TEMP, M_WAITOK | M_ZERO);
	snprintf(tmpstr, MAXHOSTNAMELEN-1, "dead_%s", vps->vps_name);
	memcpy(vps->vps_name, tmpstr, MAXHOSTNAMELEN);
	free(tmpstr, M_TEMP);

	/* Point of no return. */

	/*
	 * Set all network interfaces to down; in order to avoid
	 * receiving any more packets.
	 */
	CURVNET_SET_QUIET(vps->vnet);
	//IFNET_RLOCK();
	TAILQ_FOREACH_SAFE(ifp, &V_ifnet, if_link, ifp2) {
		DBGCORE("%s: if_down() <-- ifnet=%p dname=[%s] "
		    "xname=[%s]\n", __func__, ifp, ifp->if_dname,
		    ifp->if_xname);
		if_down(ifp);
	}
	//IFNET_RUNLOCK();
	/* --> rt_dispatch() *grrr* */
	V_loif = NULL;
	CURVNET_RESTORE();

	if (vps->_rootvnode) {
		vrele(vps->_rootvnode);
		vps->_rootvnode = NULL;
		vps->_rootpath[0] = '\0';
	}
	vrele(VPS_VPS(vps, prison0)->pr_root);
	VPS_VPS(vps, prison0)->pr_root = NULL;

	vps_unmount_all(vps);

	vps_console_free(vps, curthread);

	crfree(vps->vps_ucred);

	/* This is the reference acquired by refcount_init() in
	   vps_alloc(). */
	vps_deref(vps, (void*)0xdeadbeef);

  out:
	curthread->td_vps = vps_save;

	return (error);
}

/*
 * We are called with vps locked exclusively.
 */
int
vps_destroy(struct vps *vps)
{
	struct ifnet *ifp, *ifp2;
	struct vps *vps_save;

	KASSERT(vps != vps0,
	    ("%s: attempting to destroy vps0 !\n", __func__));

	KASSERT(vps->vps_status == VPS_ST_DEAD,
	    ("%s: vps->vps_status != VPS_ST_DEAD", __func__));

	vps_save = curthread->td_vps;
	curthread->td_vps = vps;

	sx_assert(&vps->vps_lock, SA_XLOCKED);

	if ( ! LIST_EMPTY(&vps->vps_child_head)) {
		DBGCORE("%s: vps_child_head not empty !\n", __func__);
		sx_xunlock(&vps->vps_lock);
		curthread->td_vps = vps_save;
		return (EBUSY);
	}

	/*
	 * Reclaim the network interfaces to parent vps now;
	 * vpsctl needs to handle them !
	 */
	CURVNET_SET_QUIET(vps->vnet);
	//IFNET_RLOCK();
	TAILQ_FOREACH_SAFE(ifp, &V_ifnet, if_link, ifp2) {
		DBGCORE("%s: ifnet=%p dname=[%s] xname=[%s]\n",
		    __func__, ifp, ifp->if_dname, ifp->if_xname);
		if (ifp->if_vnet == ifp->if_home_vnet) {
			DBGCORE("%s: non-inherited interface left: [%s]\n",
			    __func__, ifp->if_xname);
			continue;
		} else if (strncmp(ifp->if_dname, "vps", 3) == 0) {
			DBGCORE("%s: interface ''if_vps'' [%s] --> "
			    "destroy.\n", __func__, ifp->if_xname);
			if_clone_destroy(ifp->if_xname);
		} else {
			DBGCORE("%s: interface ifp=%p xname=[%s] reclaim "
			    "to parent.\n", __func__, ifp, ifp->if_xname);
			if_vmove_vps(curthread, ifp->if_xname, 0, vps_save,
			    NULL);
	   }
	}
	//IFNET_RUNLOCK();
	CURVNET_RESTORE();

	sx_xlock(&vps_all_lock);
	LIST_REMOVE(vps, vps_all);
	sx_xlock(&vps->vps_parent->vps_lock);
	LIST_REMOVE(vps, vps_sibling);
	sx_xunlock(&vps->vps_parent->vps_lock);
	sx_xunlock(&vps_all_lock);

	(void)vps_devfs_ruleset_destroy(vps);

	vps_prison_destroy(vps);

	procuninit();

	delete_unrhdr(VPS_VPS(vps, pts_pool));

	/* After this point V_* globals are invalid ! */

	/* Destroy vnet. Apparently always succeeds. */
	KASSERT(vps->vnet->vnet_sockcnt == 0, ("%s: vnet->vnet_sockcnt "
	   "!= 0, vps=%p vnet=%p vnet->vnet_sockcnt=%d\n",
	   __func__, vps, vps->vnet, vps->vnet->vnet_sockcnt));
	DBGCORE("%s: right before vnet_destroy(%p)\n", __func__, vps->vnet);
	vnet_destroy(vps->vnet);
	curthread->td_vps = vps_save;
	DBGCORE("%s: right after  vnet_destroy(%p)\n", __func__, vps->vnet);

	vps_account_stats(vps);

	mtx_destroy(&vps->vps_acc->lock);

	sx_xunlock(&vps->vps_lock);
	sx_destroy(&vps->vps_lock);
	mtx_destroy(&vps->vps_refcnt_lock);
	free(vps->vps_lock_name, M_VPS_CORE);

	free(vps->vps_acc, M_VPS_CORE);
	free(vps, M_VPS_CORE);

	/* reclaim memory */
	proc_zone_reclaim();
	thread_zone_reclaim();
	vmspace_zone_reclaim();
	uma_reclaim();

	return (0);
}

void
vps_ref(struct vps *vps, struct ucred *ucred)
{
#ifdef INVARIANTS
	struct vps_ref *ref;
#endif

#ifdef INVARIANTS
	ref = malloc(sizeof(*ref), M_VPS_CORE, M_NOWAIT);
#endif
	mtx_lock_spin(&vps->vps_refcnt_lock);
	refcount_acquire(&vps->vps_refcnt);
#ifdef INVARIANTS
	if (ref != NULL) {
		ref->arg = ucred;
		ref->ticks = ticks;
		TAILQ_INSERT_TAIL(&vps->vps_ref_head, ref, list);
	} else {
		printf("%s: WARNING: could not allocate ref\n", __func__);
	}
#endif
	mtx_unlock_spin(&vps->vps_refcnt_lock);

	/*
	DBGCORE("%s: adding ref, vps=%p refcnt=%d->%d ucred=%p\n",
	   __func__, vps, vps->vps_refcnt-1, vps->vps_refcnt, ucred);
	*/
}

static void
vps_destroy_task(void *context, int pending)
{
	struct vps *vps;

	vps = (struct vps *)context;

	sx_xlock(&vps->vps_lock);

	vps_destroy(vps);
}

void
vps_deref(struct vps *vps, struct ucred *ucred)
{
	int last;
#ifdef INVARIANTS
	struct vps_ref *ref;
#endif

	/*
	DBGCORE("%s: deleting ref, vps=%p refcnt=%d->%d ucred=%p\n",
	   __func__, vps, vps->vps_refcnt, vps->vps_refcnt-1, ucred);
	*/

	mtx_lock_spin(&vps->vps_refcnt_lock);
	last = refcount_release(&vps->vps_refcnt);
#ifdef INVARIANTS
	TAILQ_FOREACH(ref, &vps->vps_ref_head, list) {
		if (ref->arg == ucred) {
			TAILQ_REMOVE(&vps->vps_ref_head, ref, list);
			break;
		}
	}
#endif
	mtx_unlock_spin(&vps->vps_refcnt_lock);

#ifdef INVARIANTS
	if (ref != NULL)
		free(ref, M_VPS_CORE);
#endif

	if (last) {
		sx_xlock(&vps->vps_lock);
		KASSERT(vps->vps_status == VPS_ST_DEAD,
		    ("%s: vps=%p; released last reference but "
		    "vps_status = %d\n", __func__, vps, vps->vps_status));

		DBGCORE("%s: vps=%p is DEAD and last reference gone"
		    " --> scheduling destroy\n", __func__, vps);

		KASSERT(vps->vps_task.q == NULL,
		    ("%s: task is already initialized ! vps=%p "
		    "timeout_task=%p\n", __func__, vps, &vps->vps_task));

		/*
		 * Not calling vps_destroy() directly because there
		 * might be locks held we need to hold exclusively.
		 */
		/*
		TASK_INIT(&vps->vps_task, 0, vps_destroy_task, vps);
		taskqueue_enqueue(taskqueue_thread, &vps->vps_task);
		*/
		/*
		 * Defer actual destroy routine by 10 seconds in case
		 * e.g. network packets are still queued in netisr stuff.
		 */
		TIMEOUT_TASK_INIT(taskqueue_thread, &vps->vps_task, 0,
		    vps_destroy_task, vps);
		taskqueue_enqueue_timeout(taskqueue_thread, &vps->vps_task,
		    1 * hz /* ticks */);
		sx_xunlock(&vps->vps_lock);
	}
}

int
vps_proc_signal(struct vps *vps, pid_t pid, int sig)
{
	struct vps *vps_save;
	struct proc *p, *p2;

	vps_save = curthread->td_vps;
	curthread->td_vps = vps;

	sx_xlock(&V_allproc_lock);

	if (pid == -1) {
		/* Broadcast. */
		LIST_FOREACH_SAFE(p, &V_allproc, p_list, p2) {
			PROC_LOCK(p);
			kern_psignal(p, sig);
			PROC_UNLOCK(p);
		}
	} else {
		if ((p = pfind(pid))) {
			PROC_LOCK(p);
			kern_psignal(p, sig);
			PROC_UNLOCK(p);
		}
	}

	sx_xunlock(&V_allproc_lock);

	curthread->td_vps = vps_save;

	return (0);
}

int
vps_proc_exit(struct thread *td, struct proc *p)
{

	return (0);
}

#if 0
/*
 * That doesn't work properly; there's no way to recognize
 * if the zombproc hasn't already been reaped in case
 * of 'vpsctl stop' ...
 */
int
vps_proc_exit(struct thread *td, struct proc *p)
{
	struct vps *vps;

	vps = td->td_vps;

	KASSERT(VPS_VPS(vps, nprocs) > 0, ("%s: vps=%p nprocs=%d\n",
		__func__, vps, VPS_VPS(vps, nprocs)));

	if (VPS_VPS(vps, nprocs) > 1)
		return (0);

	/*
	 * This process is the last in the vps,
	 * so schedule the reaping of our zombie.
	 */
	vps_ref(vps, (void *)0x3478242);
	timeout(vps_proc_release_timeout, (void *)p, hz);

	return (0);
}

void
vps_proc_release_timeout(void *arg)
{
	struct vps *vps;

	vps = ((struct proc *)arg)->p_ucred->cr_vps;

	TASK_INIT(&vps->vps_task, 0, vps_proc_release_taskq, arg);
	taskqueue_enqueue(taskqueue_thread, &vps->vps_task);
}

void
vps_proc_release_taskq(void *arg, int pending)
{
	struct vps *vps;
	struct proc *p;

	p = (struct proc *)arg;
	vps = p->p_ucred->cr_vps;

	sx_xlock(&VPS_VPS(vps, proctree_lock));
	sx_xlock(&VPS_VPS(vps, allproc_lock));

	/* Make sure zombie wasn't reaped by vps_free() already. */
	if (VPS_VPS(vps, nprocs) == 0) {
		vps_deref(vps, (void *)0x3478242);
		return;
	}

	(void)vps_proc_release(vps, p);

	sx_xunlock(&VPS_VPS(vps, allproc_lock));
	sx_xunlock(&VPS_VPS(vps, proctree_lock));

	vps_deref(vps, (void *)0x3478242);
}
#endif /* 0 */

/*
 * Used instead of kern_wait() to release a zombie process.
 *
 * Enter with exclusive allproc and proctree locks held.
 *
 * XXX Try to do this with proc_reap() instead duplicating code here.
 */
int
vps_proc_release(struct vps *vps2, struct proc *p)
{
	struct vps *vps1;
	int error;

	vps1 = curthread->td_vps;
	curthread->td_vps = vps2;

	PROC_LOCK(p);
	if (p->p_state != PRS_ZOMBIE) {
		PROC_UNLOCK(p);
		error = EBUSY;
		goto fail;
	}
	sigqueue_take(p->p_ksi);
	LIST_REMOVE(p, p_list);
	if (p->p_pptr)
		LIST_REMOVE(p, p_sibling);
	PROC_UNLOCK(p);
	leavepgrp(p);

	/* XXX process/accounting stats */

	vps_account(p->p_ucred->cr_vps, VPS_ACC_PROCS, VPS_ACC_FREE, 1);

	chgproccnt(p->p_ucred->cr_uidinfo, -1, 0);
	crfree(p->p_ucred);
	p->p_ucred = NULL;
	pargs_drop(p->p_args);
	sigacts_free(p->p_sigacts);
	p->p_sigacts = NULL;
	thread_wait(p);
	vm_waitproc(p);
#ifdef MAC
	mac_proc_destroy(p);
#endif

	KASSERT(FIRST_THREAD_IN_PROC(p),
	   ("%s: p=%p no residual thread!", __func__, p));

	uma_zfree(proc_zone, p);
	V_nprocs--;
	V_nprocs_zomb--;

	error = 0;

  fail:
	curthread->td_vps = vps1;

	return (error);
}

static struct pgrp *
vps_alloc_pgrp(struct vps *vps)
{
	struct pgrp *pg;
	struct session *sess;

	sess = NULL;

	pg = malloc(sizeof(struct pgrp), M_PGRP, M_WAITOK | M_ZERO);

	mtx_init(&pg->pg_mtx, "process group", NULL, MTX_DEF | MTX_DUPOK);

	sess = malloc(sizeof(struct session), M_SESSION, M_WAITOK | M_ZERO);

	mtx_init(&sess->s_mtx, "session", NULL, MTX_DEF | MTX_DUPOK);
	refcount_init(&sess->s_count, 1);

	PGRP_LOCK(pg);
	pg->pg_session = sess;
	pg->pg_id = 1;
	LIST_INIT(&pg->pg_members);
	LIST_INSERT_HEAD(&VPS_VPS(vps, pgrphashtbl)[pg->pg_id &
	    VPS_VPS(vps, pgrphash)], pg, pg_hash);
	pg->pg_jobc = 0;
	SLIST_INIT(&pg->pg_sigiolst);
	PGRP_UNLOCK(pg);

	return (pg);
}

static int
vps_switch_rootvnode(struct thread *td, struct vps *vps)
{
	int error;

	if (vps->_rootvnode == NULL)
		return (0);

	vn_lock(vps->_rootvnode, LK_EXCLUSIVE | LK_RETRY);
	if ((error = change_dir(vps->_rootvnode, td)) != 0) {
		VOP_UNLOCK(vps->_rootvnode, 0);
		return (error);
	}
#ifdef MAC
	if ((error = mac_vnode_check_chroot(td->td_ucred,
	    vps->_rootvnode))) {
		VOP_UNLOCK(vps->_rootvnode, 0);
		return (error);
	}
#endif
	VOP_UNLOCK(vps->_rootvnode, 0);
	change_root(vps->_rootvnode, td);

	kern_chdir(td, "/", UIO_SYSSPACE);

	return (0);
}

/*
 * Unshare vmspace.
 */
static int
vps_switch_vmspace(struct thread *td, struct vps *vps1, struct vps *vps2,
	struct ucred *ucr2, struct vmspace *vm)
{
	struct vm_map_entry *e, *e2;
	struct ucred *ucr3;
	vm_object_t obj;
	char do_charge;
	int ref_cnt;
	int i, i2;

#ifdef DIAGNOSTIC
	ucr3 = crget();
	crcopy(ucr3, ucr2);
	DBGCORE("%s: ucr2=%p ref=%d ucr3=%p ref=%d\n",
	    __func__, ucr2, ucr2->cr_ref, ucr3, ucr3->cr_ref);
#else
	ucr3 = ucr2;
#endif

	for (i = 0, e = vm->vm_map.header.next;
		e != &vm->vm_map.header;
		i++, e = e->next) {

		/*
		DBGCORE("%s: e=%p e->eflags=%08x e->inheritance=%d "
		    "e->object.vm_object=%p\n", __func__, e, e->eflags,
		    e->inheritance, e->object.vm_object);
		*/

		if (e->cred != NULL) {

			crfree(e->cred);

			/*
			crhold(ucr3);
			e->cred = ucr3;
			*/

			/*
			 * XXX Don't know why but otherwise swap charge
			 *     is released one extra time in vm_map_lookup
			 *     on fault-in.
			 */
			e->cred = NULL;
		}

		if (e->eflags & MAP_ENTRY_IS_SUB_MAP) {
			DBGCORE("%s: WARNING: skipping submap\n", __func__);
			continue;
		}

		obj = e->object.vm_object;

		/* Check if we own this object. */
		if (obj != NULL && obj->cred != NULL && obj->ref_count == 1)
			do_charge = 1;
		else if (obj != NULL && obj->cred != NULL &&
		    obj->ref_count > 1) {
			ref_cnt = 0;
			for (i2 = 0, e2 = vm->vm_map.header.next;
			     e2 != &vm->vm_map.header;
			     i2++, e2 = e2->next) {
				if ((e2->eflags & MAP_ENTRY_IS_SUB_MAP)
				    == 0 && e2->object.vm_object == obj)
					ref_cnt += 1;
				if (ref_cnt == obj->ref_count)
					break;
			}

			if (ref_cnt == obj->ref_count) {
				/* We own it. */
				do_charge = 1;
			} else {
				/* Is shared with other vmspaces. */
				do_charge = 0;
			}
		} else {
			do_charge = 0;
		}

#if 1
		if (e->inheritance == VM_INHERIT_SHARE &&
			obj != NULL && obj->cred != NULL) {
			/* Don't charge, don't change credentials. */
			DBGCORE("%s: e=%p e->object=%p obj->cred=%p "
			    "VM_INHERIT_SHARE\n", __func__, e,
			    obj,obj->cred);
			/*
			 * XXX Maybe it would be better to deny having
			 * shared map entries (with credentials).
			 */
			do_charge = 0;
		}
#endif

		if (do_charge != 0) {
			vps_account(vps1, VPS_ACC_PHYS, VPS_ACC_FREE,
				obj->resident_page_count << PAGE_SHIFT);
			vps_account(vps2, VPS_ACC_PHYS, VPS_ACC_ALLOC,
				obj->resident_page_count << PAGE_SHIFT);
			DBGCORE("%s: obj=%p cred=%p refcnt=%d virt=%zx "
			    "phys=%zx\n", __func__, obj, obj->cred,
			    obj->ref_count, (size_t)obj->charge,
			    (size_t)obj->resident_page_count << PAGE_SHIFT);

			swap_release_by_cred(obj->charge, obj->cred);
			if (swap_reserve_by_cred(obj->charge, ucr3) == 0) {
				DBGCORE("%s: swap_reserve_by_cred() "
				    "error\n", __func__);
			}

			crfree(obj->cred);
			crhold(ucr3);
			obj->cred = ucr3;
		}

	}
#ifdef DIAGNOSTIC
	crfree(ucr3);
	DBGCORE("%s: ucr2=%p ref=%d ucr3=%p ref=%d\n",
		__func__, ucr2, ucr2->cr_ref, ucr3, ucr3->cr_ref);
#endif

	return (0);
}

int
vps_switch_proc(struct thread *td, struct vps *vps2, int flag)
{
	struct proc *ppold, *p2, *p;
	struct ucred *ucr1, *ucr2;
	struct thread *td2;
	struct vps *vps1;
	char save_s_login[sizeof(td->td_proc->p_session->s_login)];

	ppold = NULL;
	p = td->td_proc;
	vps1 = td->td_vps;

	/*
	 * XXX Add more checks for resources we don't
	 *     switch from one vps instance to another.
	 */
	if (p->p_numthreads != 1) {
		DBGCORE("%s: p->p_numthreads=%d\n",
			__func__, p->p_numthreads);
		return (EINVAL);
	}

	/*
	 * XXX
	 *
	 * In the ideal case we should not allow any resources
	 * referring to the vps instance moved from.
	 *
	 * But vpsctl requires at least a pts device
	 * and the executable vnode.
	 *
	 * Calling process is responsible for closing the
	 * /dev/vps handle.
	 */
	{
		struct filedesc *fdp;
		struct file *fp;
		struct vnode *vp;
		int i;

		fdp = p->p_fd;
		FILEDESC_XLOCK(fdp);

		for (i = 0; i < fdp->fd_nfiles; i++) {

			fp = fget_locked(fdp, i);
			if (fp == NULL)
				continue;

			switch (fp->f_type) {
			case DTYPE_PTS:
				break;
			case DTYPE_VNODE:
				vp = fp->f_vnode;
				if (vp->v_type != VCHR &&
				    (strcmp(vp->v_rdev->si_name, "vps") == 0 ||
				    strncmp(vp->v_rdev->si_name, "pts/", 4) == 0)) {
					DBGCORE("%s: vnode=%p v_type=%d\n",
					   __func__, vp, vp->v_type);
					FILEDESC_XUNLOCK(fdp);
					return (EINVAL);
				}
				break;
			case DTYPE_PIPE:
			case DTYPE_SOCKET:
			case DTYPE_KQUEUE:
			default:
				DBGCORE("%s: file type=%d\n",
				    __func__, fp->f_type);
				FILEDESC_XUNLOCK(fdp);
				return (EINVAL);
				break;
			}
		}

		FILEDESC_XUNLOCK(fdp);
	}

	sx_xlock(&vps2->vps_lock);
	sx_xlock(&vps1->vps_lock);

	if (vps2->vps_status != VPS_ST_RUNNING) {
		sx_xunlock(&vps2->vps_lock);
		sx_xunlock(&vps1->vps_lock);
		return (EBUSY);
	}

	/*
	 * Unshare things to ease accounting.
	 */
	if (vmspace_unshare(p)) {
		sx_xunlock(&vps2->vps_lock);
		sx_xunlock(&vps1->vps_lock);
		return (ENOMEM);
	}
	fdunshare(p, td);

        /*
         * + copy ucred and set new vps
	 * + set new vnet
         * + remove from parents childlist
         * + leave process group
         * + remove from pidhash list
         * + remove from allproc list
         * +
         * + insert into new allproc list
         * + insert into new pidhash list
         * + enter new process group
         * + adjust session
         * + reparent to initproc if available
         * + ... much much more i guess
         */

	bcopy(p->p_session->s_login, save_s_login, sizeof(save_s_login));

	sx_xlock(&VPS_VPS(vps1, proctree_lock));
	sx_xlock(&VPS_VPS(vps2, proctree_lock));

	leavepgrp(p);

	ucr2 = crget();

	/*
	 * If no processes left (only one zombie typically) reap zombies
	 * so initpgrp will be freed.
	 */
	if (LIST_EMPTY(&VPS_VPS(vps2, allproc)) && VPS_VPS(vps2, initpgrp)
	    != NULL) {
		struct proc *p3;

		/*
		 * Reap zombies.
		 */
		LIST_FOREACH_SAFE(p2, &VPS_VPS(vps2, zombproc), p_list, p3)
			vps_proc_release(vps2, p2);

		VPS_VPS(vps2, initpgrp) = NULL;
	}

	if (VPS_VPS(vps2, initpgrp) == NULL)
		VPS_VPS(vps2, initpgrp) = vps_alloc_pgrp(vps2);

	/*
	 * Same for the initproc reference.
	 */
	if (VPS_VPS(vps2, initproc)) {
		int found;

		found = 0;
		LIST_FOREACH(p2, &VPS_VPS(vps2, allproc), p_list)
			if (p2 == VPS_VPS(vps2, initproc))
				++found;
		if (found == 0)
			VPS_VPS(vps2, initproc) = NULL;
	}

	sx_xlock(&VPS_VPS(vps1, allproc_lock));
	sx_xlock(&VPS_VPS(vps2, allproc_lock));

	PROC_LOCK(p);

	ppold = p->p_pptr;
	PROC_LOCK(p->p_pptr);
	sigqueue_take(p->p_ksi);
	LIST_REMOVE(p, p_sibling);
	PROC_UNLOCK(p->p_pptr);
	p->p_pptr = NULL;

	ucr1 = p->p_ucred;
	setsugid(p); /* ? */
	crcopy(ucr2, ucr1);
	vps_deref(ucr2->cr_vps, ucr2);
	/* crcopy() did prison_hold() */
	prison_free(ucr2->cr_prison);
	prison_proc_free(ucr2->cr_prison);
	uifree(ucr2->cr_uidinfo);
	uifree(ucr2->cr_ruidinfo);
	ucr2->cr_vps = vps2;
	ucr2->cr_prison = VPS_VPS(vps2, prison0);
	vps_ref(ucr2->cr_vps, ucr2);
	td->td_vps = vps2;
	prison_hold(ucr2->cr_prison);
	prison_proc_hold(ucr2->cr_prison);
	ucr2->cr_uidinfo = uifind(ucr2->cr_uid);
	ucr2->cr_ruidinfo = uifind(ucr2->cr_ruid);
	td->td_vps = vps1;
	p->p_ucred = ucr2;
	FOREACH_THREAD_IN_PROC(p, td2) {
		if (td2->td_ucred != ucr1)
			DBGCORE("%s: WARNING: td2->td_ucred != ucr1\n",
			    __func__);
		crfree(td2->td_ucred);
		td2->td_ucred = ucr2;
		crhold(td2->td_ucred);
		td2->td_vps_acc = vps2->vps_acc;
		td2->td_vps = vps2;
		//td->td_vps = vps2;
		vps_account(vps1, VPS_ACC_THREADS, VPS_ACC_FREE, 1);
	}
	crfree(ucr1);

	VPS_VPS(vps1, nprocs)--;
	chgproccnt(ucr1->cr_uidinfo, -1, 0);
	chgproccnt(ucr2->cr_uidinfo, 1, 0);
	vps_account(vps1, VPS_ACC_PROCS, VPS_ACC_FREE, 1);

	LIST_REMOVE(p, p_hash);
	LIST_REMOVE(p, p_list);

	if (VPS_VPS(vps2, initproc) == NULL) {
		KASSERT(VPS_VPS(vps2, nprocs) == 0,
		    ("%s: vps2 initproc == NULL && vps2 nprocs == %d",
		    __func__, VPS_VPS(vps2, nprocs)));
		p->p_pid = 1;
		VPS_VPS(vps2, pidchecked) = 0;
	}

	PROC_UNLOCK(p);
	vps_switch_vmspace(td, vps1, vps2, ucr2, p->p_vmspace);
	PROC_LOCK(p);

	LIST_INSERT_HEAD(&VPS_VPS(vps2, allproc), p, p_list);
	LIST_INSERT_HEAD(&VPS_VPS(vps2, pidhashtbl)[p->p_pid &
	    VPS_VPS(vps2, pidhash)], p, p_hash);

	VPS_VPS(vps2, nprocs)++;
	/* Intentionally not checking for limit here. */
	vps_account(vps2, VPS_ACC_PROCS, VPS_ACC_ALLOC, 1);
	vps_account(vps2, VPS_ACC_THREADS, VPS_ACC_ALLOC, 1);

	sx_xunlock(&VPS_VPS(vps2, allproc_lock));
	sx_xunlock(&VPS_VPS(vps1, allproc_lock));

	if (VPS_VPS(vps2, initproc) == NULL) {
		struct session *sess;

		sess = VPS_VPS(vps2, initpgrp)->pg_session;
		SESS_LOCK(sess);
		sess->s_leader = p;
		sess->s_sid = p->p_pid;
		bcopy(save_s_login, sess->s_login, sizeof(sess->s_login));
		SESS_UNLOCK(sess);

		/* First proc in vps, so it becomes initproc. */
		VPS_VPS(vps2, initproc) = p;

	} else {
		p->p_pptr = VPS_VPS(vps2, initproc);
		PROC_LOCK(p->p_pptr);
		LIST_INSERT_HEAD(&p->p_pptr->p_children, p, p_sibling);
		PROC_UNLOCK(p->p_pptr);
	}

	p->p_pgrp = VPS_VPS(vps2, initpgrp);
	PROC_UNLOCK(p);

	PGRP_LOCK(p->p_pgrp);
	LIST_INSERT_HEAD(&p->p_pgrp->pg_members, p, p_pglist);
	PGRP_UNLOCK(p->p_pgrp);
	fixjobc(p, p->p_pgrp, 1);

	sx_xunlock(&VPS_VPS(vps2, proctree_lock));
	sx_xunlock(&VPS_VPS(vps1, proctree_lock));

	if (vps_switch_rootvnode(td, vps2))
		goto fail;

	if (ppold)
		wakeup(ppold);

	sx_xunlock(&vps2->vps_lock);
	sx_xunlock(&vps1->vps_lock);

	DBGCORE("%s: ucr1=%p ref=%d; ucr2=%p ref=%d\n",
	    __func__, ucr1, ucr1->cr_ref, ucr2, ucr2->cr_ref);

	return (0);

  fail:
	PROC_LOCK(p);
	killproc(p, "vps_switchproc: unrecoverable error !\n");
	PROC_UNLOCK(p);
	if (ppold)
		wakeup(ppold);

	sx_xunlock(&vps2->vps_lock);
	sx_xunlock(&vps1->vps_lock);

	return (EINVAL);
}

/*
 * Called by reboot().
 */
int
vps_reboot(struct thread *td, int howto)
{
	struct vps *vps;
	struct proc *p, *p2;
	struct pargs *pargs;
	struct execve_args args;
	struct ifnet *ifp;
	char *comm;
	/*
	struct image_args imgargs;
	int arglen;
	*/
	int arg;
	int reboot;
	int error;

	DBGCORE("%s: howto=%d\n", __func__, howto);

	error = 0;

	if ( ! ((howto & RB_HALT) || (howto & RB_POWEROFF)) ) {
		reboot = 1;
	} else
		reboot = 0;

	/*
	 * Get rid of every process/thread in the vps instance.
	 *
	 * In case of reboot, we keep the current proc an reuse it.
	 *
	 * Otherwise we call vps_destroy() afterwards.
	 *
	 */

	vps = td->td_vps;

	KASSERT(vps != vps0, ("%s: attempt to reboot vps0 !\n", __func__));

	if (reboot) {
		KASSERT(VPS_VPS(vps, initproc) != NULL,
		    ("%s: vps=%p initproc == NULL\n", __func__, vps));

		p = VPS_VPS(vps, initproc);
		PROC_SLOCK(p);
		comm = NULL;
		pargs = NULL;
#if 0
//notyet
		/* Just ignore that for now. We always exec /sbin/init. */
		if (p->p_args == NULL || p->p_args->ar_length == 0) {
			if (p->p_comm[0] == '/') {
				/* Hope it is null-terminated :-o. */
				comm = strdup(p->p_comm, M_VPS_CORE);
			} else {
				/* Don't know what to execve() */
				DBGCORE("%s: don't know what to "
				    "execve() !\n", __func__);
				reboot = 0;
			}
		} else {
			pargs_hold(p->p_args);
			pargs = p->p_args;
		}
#endif
		PROC_SUNLOCK(p);
	}

	sx_slock(&V_allproc_lock);
	FOREACH_PROC_IN_SYSTEM(p) {
		PROC_LOCK(p);
		if (reboot && p == td->td_proc ) {
			PROC_UNLOCK(p);
			continue;
		}
		if (p_cansignal(td, p, SIGKILL) == 0) {
			kern_psignal(p, SIGKILL);
		}
		PROC_UNLOCK(p);
	}
	sx_sunlock(&V_allproc_lock);

	while ((V_nprocs - V_nprocs_zomb) > 1) {
		/* Sleep. */
		pause("vpsbot", hz / 10);
	}

	/*
	 * Reap zombies.
	 */
	sx_xlock(&V_proctree_lock);
	sx_xlock(&V_allproc_lock);
	LIST_FOREACH_SAFE(p, &V_zombproc, p_list, p2)
		vps_proc_release(vps, p);
	sx_xunlock (&V_allproc_lock);
	sx_xunlock (&V_proctree_lock);

	if ( ! reboot ) {
		/*
		 * When the last proc has exited,
		 * exit1() schedules vps_destroy().
		 * XXX not yet !
		 */
		return (error);
	}

	/*
	 * Get rid of everything unwanted and reset
	 * the process to a fresh state.
	 */
	arg = 0;
	sys_closefrom(td, (struct closefrom_args *)&arg);

	/* This proc is now pid 1. */
	p = td->td_proc;
	sx_xlock(&V_proctree_lock);
	sx_xlock(&V_allproc_lock);
	PROC_LOCK(p);
	LIST_REMOVE(p, p_hash);
	p->p_pid = 1;
	LIST_INSERT_HEAD(PIDHASH(p->p_pid), p, p_hash);
	p->p_pptr = NULL;
	VPS_VPS(vps, initproc) = p;
	VPS_VPS(vps, initpgrp) = p->p_pgrp;
	PROC_UNLOCK(p);
	sx_xunlock(&V_allproc_lock);
	sx_xunlock(&V_proctree_lock);

	/* Reset vps uptime. */
	curthread->td_vps = vps0;
	memset(&VPS_VPS(vps, boottimebin), 0, sizeof(struct bintime));
	memset(&VPS_VPS(vps, boottime), 0, sizeof(struct timeval));
	getbintime(&VPS_VPS(vps, boottimebin));
	bintime2timeval(&VPS_VPS(vps, boottimebin),
	    &VPS_VPS(vps, boottime));
	curthread->td_vps = vps;

	/* Reset hostname, domainname, hostuuid. */
	memset(VPS_VPS(vps, hostname), 0, sizeof(VPS_VPS(vps, hostname)));
	memset(VPS_VPS(vps, domainname), 0,
	    sizeof(VPS_VPS(vps, domainname)));
	memset(VPS_VPS(vps, hostuuid), 0, sizeof(VPS_VPS(vps, hostuuid)));

	/* XXX Clean network interfaces, routing tables, mounts, etc. ... */

	CURVNET_SET_QUIET(vps->vnet);

	TAILQ_FOREACH(ifp, &V_ifnet, if_link) {
		if_purgeaddrs(ifp);
		/* declared static in net/if.c ...
		if_purgemaddrs(ifp);
		*/
	}
	/* XXX We lose memory this way ...
	vnet_route_uninit(NULL);
	vnet_route_init(NULL);
	*/

	CURVNET_RESTORE();

	vps_unmount_all(vps);

	/*
	 * XXX Clean this up ! Also fetch the arguments from
	 *     the proc cmdline of the initproc.
	 */

#if 1

	error = vps_md_reboot_copyout(td, &args);
	if (error != 0)
		goto fail;

	DBGCORE("%s: args.fname=[%s] args.argv=%p \n"
	    "argv[0]=%p/[%s] argv[1]=%p/[%s]\n",
	    __func__, args.fname, args.argv,
	    args.argv[0], args.argv[0],
	    args.argv[1], args.argv[1]);

	error = sys_execve(td, &args);

#else
//notyet

	pargs_drop(pargs);

	DBGCORE("%s: imgargs.fname=[%s] imgargs.begin_argv=%p/[%s] "
	    "imgargs.begin_envv=%p\n", __func__, imgargs.fname,
	    imgargs.begin_argv, imgargs.begin_argv, imgargs.begin_envv);

	error = kern_execve(td, &imgargs, NULL);
#endif

	if (error == 0)
		return (0);

	DBGCORE("%s: execve error %d\n", __func__, error);

  fail:
	PROC_LOCK(p);
	killproc(p, "vps reboot failed");
	PROC_UNLOCK(p);

	return (error);
}

static int
vps_sysctl_reclaim(SYSCTL_HANDLER_ARGS)
{

	if (curthread->td_pinned != 0) {
		printf("%s: thread is pinned\n", __func__);
		return (1);
	}

	/* XXX M_WAITOK not safe
	zone_drain_wait(proc_zone, M_WAITOK);
	zone_drain_wait(thread_zone, M_WAITOK);
	zone_drain_wait(vmspace_zone, M_WAITOK);
	uma_reclaim();
	uma_reclaim();
	*/
	proc_zone_reclaim();
	thread_zone_reclaim();
	vmspace_zone_reclaim();
	uma_zone_reclaim(NULL);
	vps_pager_lowmem(NULL);

        return (0);
}

static int
vps_prison_alloc(struct vps *vps_parent, struct vps *vps)
{
	struct prison *pp, *np;

	const char *name = "0";
	const char *path = "/";
	const char *hostuuid = "00000000-0000-0000-0000-000000000000";

	/* Like prison0 */

	VPS_VPS(vps, prison0) = malloc(sizeof(struct prison),
	    M_VPS_CORE, M_WAITOK|M_ZERO);
	np = VPS_VPS(vps, prison0);
	pp = VPS_VPS(vps_parent, prison0);

	np->pr_id = 0;
	strncpy(np->pr_name, name, strlen(name));
	/* Keep an extra reference so this fake prison isn't
	   freed by the last proc. */
	/* XXX debug this; somethings wrong with references here */
	np->pr_ref = 3;
	np->pr_uref = 3;
	strncpy(np->pr_path, path, strlen(name));
	np->pr_securelevel = pp->pr_securelevel;
	np->pr_childmax = pp->pr_childmax;
	strncpy(np->pr_hostuuid, hostuuid, strlen(hostuuid));
	LIST_INIT(&np->pr_children);
	np->pr_flags = pp->pr_flags;
	np->pr_allow = pp->pr_allow;
	mtx_init(&np->pr_mtx, "jail mutex", NULL, MTX_DEF);

	/* do all the extra stuff */
	np->pr_vnet = vps->vnet;
	VREF(vps->_rootvnode);
	np->pr_root = vps->_rootvnode;
	np->pr_cpuset = cpuset_ref(pp->pr_cpuset);

	return (0);
}

static void
vps_prison_destroy(struct vps *vps)
{
	struct prison *pr;

	pr = VPS_VPS(vps, prison0);

	cpuset_rel(pr->pr_cpuset);
	//vrele(pr->pr_root);

	mtx_destroy(&pr->pr_mtx);

	KASSERT(LIST_EMPTY(&pr->pr_children),
		("%s: pr->pr_children list not empty\n", __func__));

	free(pr, M_VPS_CORE);
	VPS_VPS(vps, prison0) = NULL;
}

/*
 * taken from VIMAGE
 *
 * ''name'' must be null-terminated !
 *
 */
struct vps *
vps_by_name(struct vps *top, char *name)
{
        struct vps *vps;
        char *next_name;
        int namelen;

	sx_assert(&vps_all_lock, SA_LOCKED);

        next_name = strchr(name, '.');
        if (next_name != NULL) {
                namelen = next_name - name;
                next_name++;
                if (namelen == 0) {
                        if (strlen(next_name) == 0)
                                return (top);    /* '.' == this vps */
                        else
                                return (NULL);
                }
        } else
                namelen = strlen(name);
        if (namelen == 0)
                return (NULL);
        LIST_FOREACH(vps, &top->vps_child_head, vps_sibling)
                if (strlen(vps->vps_name) == namelen &&
                    strncmp(name, vps->vps_name, namelen) == 0) {
                        if (next_name != NULL)
                                return (vps_by_name(vps, next_name));
                        else
                                return (vps);
                }
        return (NULL);
}

/* EOF */
