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
    "$Id: vps_suspend.c 162 2013-06-06 18:17:55Z klaus $";

#include "opt_global.h"
#include "opt_ddb.h"
#include "opt_inet.h"
#include "opt_inet6.h"
#include "opt_compat.h"

#ifdef VPS

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
#include <sys/dirent.h>
#include <sys/jail.h>
#include <sys/stat.h>
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

#include "vps_account.h"
#include "vps_user.h"
#include "vps.h"
#include "vps2.h"
#include <machine/vps_md.h>

MALLOC_DECLARE(M_VPS_CORE);

int vps_suspend(struct vps *vps, int flags);
int vps_resume(struct vps *vps, int flags);
int vps_abort(struct vps *vps, int flags);

static int vps_syscall_fixup(struct vps *vps, struct thread *td);
static int vps_access_vmspace(struct vmspace *vm, vm_offset_t vaddr,
    size_t len, void *buf, int prot);

static int vps_resume_relinkvnodes(struct vps *vps);
static int vps_suspend_relink_delete(struct vnode *vp);

static int vps_suspend_mod_refcnt;

/*
 * This is mostly kern_linkat().
 *
 * Relinks a unlinked vnode to a temporary name so it can be migrated.
 */
static int
vps_suspend_relinkvnodes_one(struct vps *vps, struct vnode *vp,
    const char *path, int fd)
{
        struct nameidata nd;
        struct mount *mp;
	struct stat *statp;
	char *path2;
        int error;

        bwillwrite();
        if ((error = vn_start_write(vp, &mp, V_WAIT | PCATCH)) != 0) {
                return (error);
        }
        NDINIT_AT(&nd, CREATE, LOCKPARENT | SAVENAME | AUDITVNODE2,
                UIO_SYSSPACE, path, fd, curthread);
        if ((error = namei(&nd)) == 0) {
                if (nd.ni_vp != NULL) {
                        if (nd.ni_dvp == nd.ni_vp)
                                vrele(nd.ni_dvp);
                        else
                                vput(nd.ni_dvp);
                        vrele(nd.ni_vp);
                        error = EEXIST;
                } else if ((error = vn_lock(vp, LK_EXCLUSIVE | LK_RETRY))
		    == 0) {
                        error = VOP_LINK(nd.ni_dvp, vp, &nd.ni_cnd);
                        VOP_UNLOCK(vp, 0);
                        vput(nd.ni_dvp);
                }
                NDFREE(&nd, NDF_ONLY_PNBUF);
        }
        vn_finished_write(mp);

	/* Get the name into the cache for reverse lookup later. */
	statp = malloc(sizeof(*statp), M_TEMP, M_WAITOK);
	path2 = malloc(strlen(path) + 1, M_TEMP, M_WAITOK);
	strlcpy(path2, path, strlen(path) + 1);

	kern_stat(curthread, path2, UIO_SYSSPACE, statp);

	free(path2, M_TEMP);
	free(statp, M_TEMP);

        return (error);
}

static int
vps_suspend_relinkvnodes(struct vps *vps)
{
	struct filedesc *fdp;
	struct file *fp;
	struct proc *p;
	char *freebuf;
	char *retbuf;
	char *path;
	int error;
	int i;

	sx_assert(&vps->vps_lock, SA_XLOCKED);

	if (vps->vps_status != VPS_ST_SUSPENDED)
		return (EBUSY);

	error = 0;

	/* Delete all '/VPSRELINKED_*' entries first. */
	vps_resume_relinkvnodes(vps);

	sx_slock(&VPS_VPS(vps, allproc_lock));
	LIST_FOREACH(p, &VPS_VPS(vps, allproc), p_list) {
		//PROC_LOCK(p);
		fdp = p->p_fd;
		FILEDESC_SLOCK(fdp);

		for (i = 0; i < fdp->fd_nfiles; i++) {

			fp = fget_locked(fdp, i);
			if (fp == NULL || fp->f_type != DTYPE_VNODE)
				continue;

			fhold(fp);
			error = vn_fullpath(curthread, fp->f_vnode,
			    &retbuf, &freebuf);
			if (error == 0)
				free(freebuf, M_TEMP);

			if (error != 0 && error != ENOENT) {
				DBGCORE("%s: error: %d\n", __func__, error);
				fdrop(fp, curthread);
		   		FILEDESC_SUNLOCK(fdp);
		   		//PROC_SUNLOCK(p);
				sx_sunlock(&VPS_VPS(vps, allproc_lock));
				return (error);

			} else if (error == ENOENT) {
				path = malloc(MAXPATHLEN, M_TEMP, M_WAITOK);
				snprintf(path, MAXPATHLEN,
				    "%s/VPSRELINKED_%p",
				    vps->_rootpath, fp->f_vnode);
				DBGCORE("%s: proc=%p/%d/[%s] fp=%p "
				    "fd_idx=%d\n", __func__, p, p->p_pid,
				    p->p_comm, fp, i);
				DBGCORE("%s: relinking vnode %p at [%s]\n",
				    __func__, fp->f_vnode, path);
				error = vps_suspend_relinkvnodes_one(vps,
				    fp->f_vnode, path, AT_FDCWD);
				free(path, M_TEMP);
				if (error != 0 && error != EEXIST) {
					DBGCORE("%s: error: %d\n",
					    __func__, error);
					fdrop(fp, curthread);
		   			FILEDESC_SUNLOCK(fdp);
		   			//PROC_SUNLOCK(p);
					sx_sunlock(&VPS_VPS(vps,
					    allproc_lock));
					return (error);
				}
			}
			fdrop(fp, curthread);
		}
		FILEDESC_SUNLOCK(fdp);
		//PROC_UNLOCK(p);
	}
	sx_sunlock(&VPS_VPS(vps, allproc_lock));

	return (error);
}

/*
 * In case vnodes have been relinked in suspend,
 * just delete those links now.
 */
static int
vps_resume_relinkvnodes(struct vps *vps)
{
	int error = 0;

	error = vps_suspend_relink_delete(vps->_rootvnode);

	return (error);
}

/*
 * Suspend VPS instance:
 * Suspend all threads,
 * keep all sockets and other FDs from receiving data,
 * --> done for tcp !
 *
 * Come in with vps exclusively locked.
 */
int
vps_suspend(struct vps *vps, int flags)
{
	struct prison *ppr, *cpr;
	struct thread *td;
	struct proc *p;
	struct vnet *vnet;
	int running_threads;
	int descend;
	int error = 0;

	sx_assert(&vps->vps_lock, SA_XLOCKED);

	if (vps->vps_status != VPS_ST_RUNNING)
		return (EBUSY);

	vps_suspend_mod_refcnt++;

	DBGCORE("%s: suspend vps=%p BEGIN\n", __func__, vps);

	vps->vps_status = VPS_ST_SUSPENDED;

	/* ''main'' vnet of vps */
	vnet = vps->vnet;
	DBGCORE("%s: vnet=%p setting VPS_VNET_SUSPENDED\n", __func__, vnet);
	vnet->vnet_vps_flags |= VPS_VNET_SUSPENDED;

	/* vnets of jails, if any */
	sx_slock(&allprison_lock);
	ppr = VPS_VPS(vps, prison0);
	FOREACH_PRISON_DESCENDANT_LOCKED(ppr, cpr, descend) {
		if ((cpr->pr_flags & PR_VNET)==0)
			continue;
		vnet = cpr->pr_vnet;
		DBGCORE("%s: vnet=%p setting VPS_VNET_SUSPENDED\n",
		    __func__, vnet);
		vnet->vnet_vps_flags |= VPS_VNET_SUSPENDED;
	}
	sx_sunlock(&allprison_lock);

	vps->suspend_time = time_second;

	sx_xlock(&VPS_VPS(vps, allproc_lock));
	LIST_FOREACH(p, &VPS_VPS(vps, allproc), p_list) {
		DBGCORE("p=%p pid=%d comm=[%s]\n", p, p->p_pid, p->p_comm);
		PROC_LOCK(p);
		p->p_flag |= P_STOPPED_TRACE;
	
		/* Keep proc from being swapped out (kernel stacks). */
		p->p_lock++;
	
		if ((p->p_flag & P_INMEM) == 0) {
			DBGCORE("%s: p=%p NOT P_INMEM, swapping in\n",
			    __func__, p);
			faultin(p);
		}
		TAILQ_FOREACH(td, &p->p_threads, td_plist) {
			thread_lock(td);
			DBGCORE("td=%u/%p td->td_flags=%08x\n",
			    td->td_tid, td, td->td_flags);
			/*db_trace_thread(td, 16);*/
			td->td_flags |= TDF_NEEDSUSPCHK | TDF_VPSSUSPEND;
			if (TD_ON_SLEEPQ(td)) {
				if (td->td_flags & TDF_SINTR)
					sleepq_abort(td, ERESTART);
				else
					DBGCORE("%s: td=%p/%d on sleepqueue"
					    " but not TDF_SINTR!\n",
					    __func__, td, td->td_tid);
	
			} else if (TD_IS_SWAPPED(td)) {
				panic("%s: td=%p TD_IS_SWAPPED\n",
				    __func__, td);
	
			} else if (TD_IS_RUNNING(td) || TD_ON_RUNQ(td) ||
			    TD_ON_RUNQ(td)) {
				DBGCORE("%s: thread=%p is "
				    "running/runq/can_run\n", __func__, td);
				/* thread will be suspended at some
				   point ... */
	
			} else if (TD_ON_LOCK(td) || TD_AWAITING_INTR(td)) {
				DBGCORE("%s: thread=%p inhibited: "
				    "TD_ON_LOCK / TD_AWAITING_INTR\n",
				    __func__, td);
				/* thread will be suspended at some
				   point ... */
	
			} else {
				DBGCORE("%s: thread=%p UNHANDLED STATE !\n",
				    __func__, td);
			}
	
			thread_unlock(td);
		}
		PROC_UNLOCK(p);
	}
	sx_xunlock(&VPS_VPS(vps, allproc_lock));

	/*
	 * XXX If we get stuck here, keep system in a safe state and return.
	 */

	/* Block until all threads are suspended. */
	do {
		pause("vpsssp", hz / 10);
		running_threads = 0;

		sx_xlock(&VPS_VPS(vps, allproc_lock));
		LIST_FOREACH(p, &VPS_VPS(vps, allproc), p_list) {
			PROC_LOCK(p);

			/* vfork */
			if (p->p_flag & P_PPWAIT) {
				cv_broadcast(&p->p_pwait);

				DBGCORE("%s: cv_broadcast(%p) p=%p/%d\n",
				    __func__, &p->p_pwait, p, p->p_pid);
			}

			TAILQ_FOREACH(td, &p->p_threads, td_plist)
				if ( ! TD_IS_SUSPENDED(td))
					++running_threads;

			PROC_UNLOCK(p);
		}
		sx_xunlock(&VPS_VPS(vps, allproc_lock));

	} while (running_threads > 0);

	if (flags & VPS_SUSPEND_RELINKFILES)
		error = vps_suspend_relinkvnodes(vps);

	DBGCORE("%s: suspend vps=%p FINISHED\n", __func__, vps);

	vps_suspend_mod_refcnt--;

	return (error);
}

/*
 * Resume vps instance previously suspended by vps_suspend().
 *
 * Come in with vps exclusively locked.
 */
int
vps_resume(struct vps *vps, int flags)
{
	struct prison *ppr, *cpr;
	struct vnet *vnet;
	struct thread *td;
	struct proc *p;
	int wakeup_swapper;
	int descend;
#ifdef DISABLED_INVARIANTS
	struct vps_ref *ref;
#endif

	sx_assert(&vps->vps_lock, SA_XLOCKED);

	if (vps->vps_status != VPS_ST_SUSPENDED)
		return (EBUSY);

	vps_suspend_mod_refcnt++;

	vps_resume_relinkvnodes(vps);

	DBGCORE("%s: resume vps=%p BEGIN\n", __func__, vps);

#ifdef DISABLED_INVARIANTS
	DBGCORE("references: \n");
	TAILQ_FOREACH(ref, &vps->vps_ref_head, list) {
		DBGCORE("    ref=%p arg=%p ticks=%zu ",
			ref, ref->arg, (size_t)ref->ticks);
		if (ref->arg != (void*)0 && ref->arg != (void*)0xdead0010 &&
		    ref->arg != (void*)0xbeefc0de) {
			DBGCORE("cr_ref=%d\n",
			    ((struct ucred *)(ref->arg))->cr_ref);
		} else {
			DBGCORE("\n");
		}
	}
#endif

	/*
	 * XXX
	 * cannot lock here because of vps_syscall_fixup().
	 */
	sx_xlock(&VPS_VPS(vps, allproc_lock));
	LIST_FOREACH(p, &VPS_VPS(vps, allproc), p_list)
		TAILQ_FOREACH(td, &p->p_threads, td_plist) {
			//db_trace_thread(td, 16);
			if ((vps_syscall_fixup(vps, td))) {
				DBGCORE("%s: vps_syscall_fixup(vps=%p, "
				    "td=%p) failed\n", __func__, vps, td);
			}
		}

#ifdef DISABLED_INVARIANTS
	DBGCORE("references: \n");
	TAILQ_FOREACH(ref, &vps->vps_ref_head, list) {
		DBGCORE("    ref=%p arg=%p ticks=%zu ",
			ref, ref->arg, (size_t)ref->ticks);
		if (ref->arg != (void*)0 && ref->arg != (void*)0xdead0010 &&
		    ref->arg != (void*)0xbeefc0de) {
			DBGCORE("cr_ref=%d\n",
			    ((struct ucred *)(ref->arg))->cr_ref);
		} else {
			DBGCORE("\n");
		}
	}
#endif

	LIST_FOREACH(p, &VPS_VPS(vps, allproc), p_list) {
		DBGCORE("p=%p pid=%d\n", p, p->p_pid);

		PROC_LOCK(p);
		p->p_flag &= ~P_STOPPED_TRACE;

		/* Allow proc being swapped out again (kernel stacks). */
		p->p_lock--;

		PROC_UNLOCK(p);
		PROC_SLOCK(p);

		if ((p->p_flag & P_STOPPED_SIG) == 0) {
			TAILQ_FOREACH(td, &p->p_threads, td_plist) {
				DBGCORE("td=%p\n", td);
				wakeup_swapper = 0;
				thread_lock(td);
				if (TD_IS_SUSPENDED(td))
					wakeup_swapper =
					    thread_unsuspend_one(td);
				else
					DBGCORE("%s: thread %p not "
					    "suspended ---> MUST NOT "
					    "HAPPEN !\n", __func__, td);
				thread_unlock(td);
				if (wakeup_swapper) {
					DBGCORE("%s: wakeup_swapper()\n",
					    __func__);
					kick_proc0();
				}
			}
		} else {
			DBGCORE("%s: P_STOPPED_SIG, not resuming\n",
			    __func__);
		}
		PROC_SUNLOCK(p);
	}
	sx_xunlock(&VPS_VPS(vps, allproc_lock));

	vps->suspend_time = 0;

	/* vnets of jails, if any */
	sx_slock(&allprison_lock);
	ppr = VPS_VPS(vps, prison0);
	FOREACH_PRISON_DESCENDANT_LOCKED(ppr, cpr, descend) {
		if ((cpr->pr_flags & PR_VNET)==0)
			continue;
		vnet = cpr->pr_vnet;
		DBGCORE("%s: vnet=%p clearing VPS_VNET_SUSPENDED\n",
		    __func__, vnet);
		vnet->vnet_vps_flags &= ~VPS_VNET_SUSPENDED;
	}
	sx_sunlock(&allprison_lock);

	/* ''main'' vnet of vps */
	vnet = vps->vnet;
	DBGCORE("%s: vnet=%p clearing VPS_VNET_SUSPENDED\n",
	    __func__, vnet);
	vnet->vnet_vps_flags &= ~VPS_VNET_SUSPENDED;

	vps->vps_status = VPS_ST_RUNNING;

	DBGCORE("%s: resume vps=%p FINISHED\n", __func__, vps);

	vps_suspend_mod_refcnt--;

	return (0);
}

int
vps_abort(struct vps *vps, int flags)
{
	struct prison *ppr, *cpr;
	struct vnet *vnet;
	struct proc *p;
	int descend;

	sx_assert(&vps->vps_lock, SA_XLOCKED);

	if (vps->vps_status != VPS_ST_SUSPENDED)
		return (EBUSY);

	DBGCORE("%s: abort vps=%p BEGIN\n", __func__, vps);

	/* ''main'' vnet of vps */
	vnet = vps->vnet;
	DBGCORE("%s: vnet=%p setting VPS_VNET_ABORT\n", __func__, vnet);
	vnet->vnet_vps_flags |= VPS_VNET_ABORT;

	/* vnets of jails, if any */
	sx_slock(&allprison_lock);
	ppr = VPS_VPS(vps, prison0);
	FOREACH_PRISON_DESCENDANT_LOCKED(ppr, cpr, descend) {
		if ((cpr->pr_flags & PR_VNET)==0)
			continue;
		vnet = cpr->pr_vnet;
		DBGCORE("%s: vnet=%p setting VPS_VNET_ABORT\n",
		    __func__, vnet);
		vnet->vnet_vps_flags |= VPS_VNET_ABORT;
	}
	sx_sunlock(&allprison_lock);

	sx_xlock(&VPS_VPS(vps, allproc_lock));
	LIST_FOREACH(p, &VPS_VPS(vps, allproc), p_list) {
		DBGCORE("p=%p pid=%d\n", p, p->p_pid);
		PROC_LOCK(p);
		kern_psignal(p, SIGKILL);
		PROC_UNLOCK(p);
	}
	sx_xunlock(&VPS_VPS(vps, allproc_lock));

	vps_resume(vps, 0);

	return (0);
}

/*
 *  Access pages of a different vmspace.
 */

VPSFUNC
static int
vps_access_vmspace(struct vmspace *vm, vm_offset_t vaddr, size_t len,
    void *buf, int prot)
{
	vm_map_t map;
	vm_page_t m;
	vm_page_t *marr;
	vm_map_entry_t entry;
	vm_object_t object;
	vm_pindex_t pindex;
	vm_prot_t prot2;
	vm_offset_t vaddr2;
	vm_offset_t kvaddr;
	boolean_t wired;
	int npages;
	int error;

	DBGCORE("%s: vmspace=%p vaddr=%p len=%zu buf=%p prot=%x\n",
		__func__, vm, (void*)vaddr, len, buf, prot);

	prot &= (VM_PROT_READ|VM_PROT_WRITE);

	marr = malloc(sizeof(vm_page_t) * ((len >> PAGE_SHIFT) + 1),
	   M_VPS_CORE, M_WAITOK | M_ZERO);

	map = &vm->vm_map;
	npages = 0;
	error = 0;
	vaddr2 = vaddr;

	if (vm_map_wire(map, trunc_page(vaddr), trunc_page(vaddr + len) +
	    PAGE_SIZE, VM_MAP_WIRE_USER)) {
		DBGCORE("%s: vm_map_wire: error\n", __func__);
		return (EINVAL);
	}

	vm_map_lock_read(map);

	do {

		if ((error = vm_map_lookup_locked(&map, trunc_page(vaddr2),
		    prot, &entry, &object, &pindex, &prot2, &wired))) {
			DBGCORE("%s: vm_map_lookup failed: %d\n",
			    __func__, error);
			error = EFAULT;
			goto unlock;
		}

		VM_OBJECT_WLOCK(object);

		DBGCORE("%s: vaddr=%p len=%zu map=%p entry=%p object=%p "
		    "pindex=%u\n", __func__, (void*)vaddr, len, map,
		    entry, object, (unsigned int)pindex);

		if ((m = vm_page_lookup(object, pindex)) == NULL) {
			DBGCORE("%s: error vm_page_lookup\n", __func__);
			error = EINVAL;
			goto unlock;
		}

		VM_OBJECT_WUNLOCK(object);
		if (error)
			goto unwire;

		marr[npages] = m;

		++npages;
		vaddr2 += PAGE_SIZE;

	} while (trunc_page(vaddr2) < vaddr + len);

	vm_map_lock(kernel_map);

	if (vm_map_findspace(kernel_map, vm_map_min(kernel_map),
	    npages << PAGE_SHIFT, &kvaddr) != KERN_SUCCESS) {
		error = EINVAL;
		DBGCORE("%s: error findspace\n", __func__);
		goto unlock_kern;
	}

	/* actually map */
	pmap_qenter(kvaddr, marr, npages);

	switch (prot) {
	case VM_PROT_READ:
		memcpy(buf, (void *)(kvaddr + (vaddr - trunc_page(vaddr))),
		    len);
		break;
	case VM_PROT_WRITE:
		memcpy((void *)(kvaddr + (vaddr - trunc_page(vaddr))),
		    buf, len);
		break;
	default:
		break;
	}

	/* actually unmap */
	pmap_qremove(kvaddr, npages);

	free(marr, M_VPS_CORE);

  unlock_kern:
	vm_map_unlock(kernel_map);

  unlock:
	vm_map_unlock_read(map);

  unwire:
	vm_map_unwire(map, trunc_page(vaddr),
	    trunc_page(vaddr + len) + PAGE_SIZE, VM_MAP_WIRE_USER);

	return (error);
}

VPSFUNC
static void
vps_syscall_fixup_inthread(register_t code, struct trapframe *frame)
{
	/*struct syscall_args sa;*/
	struct thread *td = curthread;
	int error;

	error = 0;

	DBGCORE("%s: curthread=%p/%u code=%zu\n",
		__func__, td, td->td_tid, (size_t)code);

	KASSERT(frame == td->td_frame, ("%s: frame != td->td_frame\n",
	    __func__));

	switch(code) {
	case SYS_vfork: {
		/*
		 * XXX: assuming single-threaded parent and
		 *      only one child proc with P_PPWAIT set.
		 */
		struct proc *p2;
		struct proc *p = td->td_proc;

		LIST_FOREACH(p2, &p->p_children, p_sibling) {
			if (p2->p_flag & P_PPWAIT)
				break;
		}
		/*
		KASSERT(p2 != NULL,
		    ("%s: no child proc with p_flag & P_PPWAIT, p=%p\n",
		    __func__, p));
		*/
		/* happens when vps is aborted --> no problem */
		if (p2 == NULL) {
			DBGCORE("%s: WARNING no child proc with p_flag "
			    "& P_PPWAIT, p=%p\n", __func__, p);
			break;
		}
		PROC_LOCK(p2);
		error = 0;
		while (p2->p_flag & P_PPWAIT) {
			cv_wait(&p2->p_pwait, &p2->p_mtx);
			if (td->td_flags & TDF_VPSSUSPEND) {
				DBGCORE("%s: aborted cv_wait(), td=%p/%u\n",
					__func__, td, td->td_tid);
				error = EINTR;
				break;
			}
		}
		PROC_UNLOCK(p2);
		if (error) {
			td->td_retval[0] = 0;
			td->td_retval[1] = 0;
			td->td_errno = error;
		} else {
		   	td->td_retval[0] = p2->p_pid;
		   	td->td_retval[1] = 0;
		   	td->td_errno = 0;
		}
		break;
	}
	case SYS_nanosleep: {
		/*
		//not yet
		struct timespec ts1, ts2;

		if ((error = cpu_fetch_syscall_args(td, &sa))) {
			printf("%s: cpu_fetch_syscall_args() error: %d\n",
				__func__, error);
			error = EINTR;
			break;
		}
		KASSERT(sa.code == code,
		    ("%s: sa.code != code\n", __func__));

		if (sa.args[0] == 0)
			break;

		if ((vps_access_vmspace(td->td_proc->p_vmspace, sa.args[0],
		    sizeof(struct timespec), &ts1, VM_PROT_READ)))
			break;
		DBGCORE("%s: SYS_nanosleep: suspended at %lu\n",
		    __func__, vps->suspend_time);
		DBGCORE("%s: SYS_nanosleep: ts: sec=%lu nsec=%lu\n",
		    __func__, ts1.tv_sec, ts1.tv_nsec);
		*/
		break;
	}
	default: {
		break;
	}
	}

        if (td->td_flags & TDF_VPSSUSPEND) {
                DBGCORE("%s: td=%p suspending\n", __func__, td);
                td->td_errno = error;
                if (td->td_flags & TDF_NEEDSUSPCHK) {
                        PROC_LOCK(td->td_proc);
                        thread_suspend_check(0);
                        /*
                         * Threads created by vps_restore() never
                         * reach this point.
                         */
                        PROC_UNLOCK(td->td_proc);
                }
                td->td_flags &= ~TDF_VPSSUSPEND;
                error = td->td_errno;
        }

	cpu_set_syscall_retval(td, td->td_errno);

        userret(td, frame);

#ifdef KTRACE
        if (KTRPOINT(td, KTR_STRUCT))
                ktrstruct("VPS", "VPS RESTORED", 13);
#endif
        mtx_assert(&Giant, MA_NOTOWNED);
}

static int
vps_syscall_fixup_need_inthread(struct vps *vps, struct thread *td,
    register_t code, register_t *args, int narg)
{
	int need_inthread = 0;

	switch (code) {
	case SYS_nanosleep: {
		struct timespec ts;
		if (args[0] == 0)
			break;
		if ((vps_access_vmspace(td->td_proc->p_vmspace, args[0],
		    sizeof(struct timespec), &ts, VM_PROT_READ)))
			break;
		if (1) {
			DBGCORE("%s: SYS_nanosleep: suspended at %zu\n",
			    __func__, (size_t)vps->suspend_time);
			DBGCORE("%s: SYS_nanosleep: ts: sec=%zu nsec=%zu\n",
			    __func__, (size_t)ts.tv_sec,
			    (size_t)ts.tv_nsec);
			/*
			//not yet
			need_inthread = 1;
			*/
		}
		td->td_errno = ERESTART;
		break;
	}
	case SYS_vfork: {
		DBGCORE("%s: SYS_VFORK: td=%p tid=%u\n",
		    __func__, td, td->td_tid);
		if (td->td_errno != EINTR)
			break;
		need_inthread = 1;
		break;
	}
	default: {
		break;
	}
	}

	return (need_inthread);
}

VPSFUNC
static int
vps_syscall_fixup(struct vps *vps, struct thread *td)
{
	register_t code;
	register_t args[8];
	int narg = 8;
	int error = 0;

	error = vps_md_syscall_fixup(vps, td, &code, (register_t **)&args, &narg);
	if (error != 0)
		goto out;

	if (vps_syscall_fixup_need_inthread(vps, td, code, args, narg) != 0) {
		error = vps_md_syscall_fixup_setup_inthread(vps, td, code);
		if (error != 0)
			goto out;
	}

  out:
	return (error);
}

static int
vps_suspend_relink_delete(struct vnode *vp)
{
	struct vnode *save_cdir;
	struct dirent *dp;
	struct thread *td;
	char *dirbuf;
	char *cpos = NULL;
	off_t off;
	int dirbuflen;
	int eofflag;
	int len;
	int error = 0;

	dirbuflen = PATH_MAX;
	td = curthread;

        dirbuf = (char *)malloc(dirbuflen, M_TEMP, M_WAITOK);

	save_cdir = td->td_proc->p_fd->fd_cdir;
	td->td_proc->p_fd->fd_cdir = vp;

	VOP_LOCK(vp, LK_SHARED | LK_RETRY);

	off = 0;
	len = 0;
	do {
		error = get_next_dirent(vp, &dp, dirbuf, dirbuflen, &off,
					&cpos, &len, &eofflag, td);
		if (error) {
			printf("%s: get_next_dirent() error=%d\n",
				__func__, error);
			goto out;
		}

		/*
		DEBUG("%s: dp=%p dp->d_type=%d dp->d_name=[%s]\n",
			__func__, dp, dp->d_type, dp->d_name);
		*/

		if (dp->d_type != DT_REG)
			goto next;
		if (strcmp(dp->d_name, "VPSRELINKED_"))
			goto next;

		error = kern_unlinkat(td, AT_FDCWD, dp->d_name,
		    UIO_SYSSPACE, 0);
		printf("%s: unlinked [%s]: error=%d\n",
			__func__, dp->d_name, error);

	  next:
		;

	} while (len > 0 || !eofflag);


 out:
	VOP_UNLOCK(vp, 0);
	td->td_proc->p_fd->fd_cdir = save_cdir;
	free(dirbuf, M_TEMP);
	return (error);
}

static int
vps_suspend_modevent(module_t mod, int type, void *data)
{
        int error;

        error = 0;

        switch (type) {
        case MOD_LOAD:
		vps_suspend_mod_refcnt = 0;
		vps_func->vps_suspend = vps_suspend;
		vps_func->vps_resume = vps_resume;
		vps_func->vps_abort = vps_abort;
		vps_func->vps_access_vmspace = vps_access_vmspace;
		vps_func->vps_syscall_fixup_inthread =
		    vps_syscall_fixup_inthread;
		break;
        case MOD_UNLOAD:
		if (vps_suspend_mod_refcnt > 0)
			return (EBUSY);
		vps_func->vps_suspend = NULL;
		vps_func->vps_resume = NULL;
		vps_func->vps_abort = NULL;
		vps_func->vps_access_vmspace = NULL;
		vps_func->vps_syscall_fixup_inthread = NULL;
		break;
        default:
		error = EOPNOTSUPP;
		break;
        }

        return (error);
}

static moduledata_t vps_suspend_mod = {
        "vps_suspend",
        vps_suspend_modevent,
        0
};

DECLARE_MODULE(vps_suspend, vps_suspend_mod, SI_SUB_PSEUDO, SI_ORDER_ANY);

#endif /* VPS */

/* EOF */
