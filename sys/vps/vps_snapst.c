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
    "$Id: vps_snapst.c 180 2013-06-14 16:52:13Z klaus $";

#include <sys/cdefs.h>

#include "opt_ddb.h"
#include "opt_global.h"
#include "opt_compat.h"

#ifdef VPS

#include <sys/param.h>
#include <sys/types.h>
#include <sys/kernel.h>
#include <sys/linker.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/refcount.h>
#include <sys/sched.h>
#include <sys/sockio.h>
#include <sys/sx.h>
#include <sys/sysctl.h>
#include <sys/ttycom.h>
#include <sys/file.h>
#include <sys/fcntl.h>
#include <sys/uio.h>
#include <sys/resourcevar.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <sys/namei.h>
#include <sys/sysent.h>
#include <sys/sysproto.h>
#include <sys/reboot.h>
#include <sys/conf.h>
#include <sys/syscallsubr.h>
#include <sys/mman.h>
#include <sys/sleepqueue.h>
#include <sys/filedesc.h>
#include <sys/mount.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/pipe.h>
#include <sys/tty.h>
#include <sys/syscall.h>
#include <sys/un.h>
#include <sys/unpcb.h>
#include <sys/sem.h>
#include <sys/ktrace.h>
#include <sys/buf.h>
#include <sys/jail.h>
#include <sys/event.h>
#include <sys/eventvar.h>
#include <sys/umtx.h>

#include <machine/pcb.h>

#include <net/if.h>
#include <netinet/in.h>

#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pager.h>
#include <vm/vm_kern.h>
#include <vm/vm_pageout.h>

#include <machine/pcb.h>

#ifdef DDB
#include <ddb/ddb.h>
#endif

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_clone.h>
#include <net/if_types.h>
#include <net/if_var.h>
#include <net/ethernet.h>
#include <net/radix.h>
#include <net/route.h>
#include <net/vnet.h>
#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/in_pcb.h>
#include <netinet/ip_var.h>
#include <netinet/tcp_var.h>
#include <netinet/tcp_fsm.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>

#include "vps_account.h"
#include "vps_user.h"
#include "vps.h"
#include "vps2.h"
#include <machine/vps_md.h>

#define _VPS_SNAPST_H_ALL
#include "vps_snapst.h"

#include "vps_libdump.h"

#define ERRMSG vps_snapst_pusherrormsg

#ifdef DIAGNOSTIC

#define DBGS if (debug_snapst) printf

static int debug_snapst = 1;
SYSCTL_INT(_debug, OID_AUTO, vps_snapst_debug, CTLFLAG_RW,
    &debug_snapst, 0, "");

#else

#define DBGS(x, ...)

#endif /* DIAGNOSTIC */

MALLOC_DEFINE(M_VPS_SNAPST, "vps_snapst",
    "Virtual Private Systems Snapshot memory");

static int vps_snapshot_sysinfo(struct vps_snapst_ctx *ctx,
    struct vps *vps);
static int vps_snapshot_vps(struct vps_snapst_ctx *ctx,
    struct vps *vps);
static int vps_snapshot_arg(struct vps_snapst_ctx *ctx,
    struct vps *vps);
static int vps_snapshot_mounts(struct vps_snapst_ctx *ctx,
    struct vps *vps);
static int vps_snapshot_prison(struct vps_snapst_ctx *ctx,
    struct vps *vps);
static int vps_snapshot_proc(struct vps_snapst_ctx *ctx,
    struct vps *vps);
static int vps_snapshot_proc_one(struct vps_snapst_ctx *ctx,
    struct vps *vps, struct proc *p);
static int vps_snapshot_vnet(struct vps_snapst_ctx *ctx,
    struct vps *vps, struct vnet *vnet);
static int vps_snapshot_vnet_route_table(struct vps_snapst_ctx *ctx,
    struct vps *vps, struct vnet *vnet, int fibnum, int af);
static int vps_snapshot_ucred(struct vps_snapst_ctx *ctx,
    struct vps *vps, struct ucred *cr, int how);

static int vps_snapst_mod_refcnt;

/*
 * * * * * Support functions. * * * *
 */

/*
 * * * * * Snapshot memory functions. * * * *
 *
 * Since the required amount of memory for storing the snapshot is unkown
 * in the beginning and can't be determined in advance (as soon as locks
 * are released objects may change) memory has to be allocated dynamically.
 * Working with memory chunks or reallocating and copying everything,
 * including change of memory adresses, lots of times, is slow and tedious.
 * So we reserve a virtual adress space big enough to hold the whole kernel
 * part of the snapshot. On 32bit architectures it *could* be a problem
 * because in default configuration the kernel address space is only 1 GB,
 * but is still the best solution, IMHO. On 64bit architectures there is
 * enough address space to feed pigs with.
 * The base of the snapshot memory (ctx->data) never changes. Before copying
 * data vps_ctx_extend() must be called. This function allocates and wires
 * physical pages, if called with M_NOWAIT only what is absolutely
 * necessary, if called with M_WAITOK some extra pages get allocated,
 * so the next couple of runs don't need to actually allocate memory.
 */
VPSFUNC
static int
vps_ctx_alloc(struct vps_snapst_ctx *ctx, struct vps *vps)
{
	vm_offset_t kvaddr;
	int npages;
	int error;

	/*
	 * This space must be big enough to hold the whole kernel part
	 * of the snapshot.
	 * On 64bit systems we can set it to something like 1 GB.
	 *
	 * npages --> 10240 --> 40 mbyte (assuming PAGE_SIZE == 4K)
	 */
	npages = VPS_MD_SNAPCTX_NPAGES;

	ctx->maxsize = npages << PAGE_SHIFT;
	ctx->dsize2 = 0;

	ctx->vmobj = vm_object_allocate(OBJT_DEFAULT, npages);
	if (ctx->vmobj == NULL) {
		error = ENOMEM;
		ERRMSG(ctx, "%s: vm_object_allocate failed\n", __func__);
		return (error);
	}

	vm_map_lock(kernel_map);
	/* Find free space in kernel virtual address space. */
	if (vm_map_findspace(kernel_map, vm_map_min (kernel_map),
		npages << PAGE_SHIFT, &kvaddr) != KERN_SUCCESS) {
		error = ENOMEM;
		vm_map_unlock(kernel_map);
		vm_object_deallocate(ctx->vmobj);
		ERRMSG(ctx, "%s: vm_map_findspace failed\n", __func__);
		return (error);
	}

	/*
	 * Reserve it by mapping the object.
	 * Note that if an address in this space gets accessed
	 * prior to calling vps_ctx_extend() a page fault gets
	 * triggered and kernel panics !
	 */
	if (vm_map_insert(kernel_map, ctx->vmobj,
	    kvaddr - VM_MIN_KERNEL_ADDRESS, kvaddr,
	    kvaddr + (npages << PAGE_SHIFT),
	    VM_PROT_ALL, VM_PROT_ALL, 0) != KERN_SUCCESS) {
		error = ENOMEM;
		vm_map_unlock(kernel_map);
		vm_object_deallocate(ctx->vmobj);
		ERRMSG(ctx, "%s: vm_map_insert failed\n", __func__);
		return (error);
	}

	vm_map_unlock(kernel_map);

	ctx->data = ctx->cpos = (void *)kvaddr;

	ctx->syspagelist = malloc(npages * sizeof(struct vm_page *),
		M_VPS_SNAPST, M_WAITOK | M_ZERO);

	vps_snapst_mod_refcnt++;

	return (0);
}

VPSFUNC
static int
vps_ctx_free(struct vps_snapst_ctx *ctx, struct vps *vps)
{
	vm_offset_t kvaddr;
	vm_page_t m;
	int npages, maxpages;

	kvaddr = (vm_offset_t)ctx->data;
	maxpages = ctx->maxsize >> PAGE_SHIFT;
	npages = ctx->dsize2 >> PAGE_SHIFT;

	pmap_qremove(kvaddr, npages);

	VM_OBJECT_WLOCK(ctx->vmobj);

	while (npages) {
		npages--;
		m = TAILQ_LAST(&ctx->vmobj->memq, pglist);
		vm_page_lock(m);
		vm_page_unwire(m, 0);
		vm_page_free(m);
		vm_page_unlock(m);
	}

	VM_OBJECT_WUNLOCK(ctx->vmobj);

	(void)vm_map_unwire(kernel_map, kvaddr, kvaddr +
	    (maxpages << PAGE_SHIFT), 0);

	/* This also destroys the vm object. */
	(void)vm_map_remove(kernel_map, kvaddr, kvaddr +
	    (maxpages << PAGE_SHIFT));

	free(ctx->syspagelist, M_VPS_SNAPST);

	vps_snapst_mod_refcnt--;

	return (0);
}

/*
 * If called with M_NOWAIT the return value must be checked !
 */
/*
 * Inline the code that gets executed on every single call.
 */

/*
 * We actually have to allocate.
 */
VPSFUNC
static int
vps_ctx_extend_hard(struct vps_snapst_ctx *ctx, struct vps *vps,
    size_t size, int how)
{
	int npages;
	int pagenum;
	int i;
	int flags;
	vm_offset_t new;
	vm_page_t m;

#if 0
	/* XXX Can't sleep here because of vmobject lock :-( */
	KASSERT(how == M_WAITOK, ("%s: how != M_WAITOK\n", __func__));
#endif

	if (how == M_WAITOK)
		WITNESS_WARN(WARN_GIANTOK | WARN_SLEEPOK, NULL,
				"%s: size=%d", __func__, size);

	npages = round_page(size) >> PAGE_SHIFT;
	if (how == M_WAITOK) {
		/* Allocate more here since we can sleep now. */
		npages += 10;
	}

	flags = VM_ALLOC_NORMAL|VM_ALLOC_WIRED;

	VM_OBJECT_WLOCK(ctx->vmobj);
	new = (vm_offset_t)((caddr_t)ctx->data + ctx->dsize2);
	m = TAILQ_LAST(&ctx->vmobj->memq, pglist);
	pagenum = (m != NULL) ? m->pindex + 1 : 0;
	for (i = 0; i < npages; i++) {
		do {
			m = vm_page_alloc(ctx->vmobj, pagenum + i, flags);
			if (m == NULL) {
				/* Assume that object will not be
				   modified. */
				VM_OBJECT_WUNLOCK(ctx->vmobj);
				vm_waitpfault();
				VM_OBJECT_WLOCK(ctx->vmobj);
			}
		} while (m == NULL && how == M_WAITOK);

		if (m == NULL) {
			ERRMSG(ctx, "%s: ENOMEM (size=%zu)\n",
			    __func__, size);
			/* XXX undo what we've done so far */
			return (ENOMEM);
		}
		pmap_qenter(new + (i << PAGE_SHIFT), &m, 1);
		bzero((void *)(new + (i << PAGE_SHIFT)), PAGE_SIZE);
		ctx->syspagelist[pagenum + i] = m;
	}
	VM_OBJECT_WUNLOCK(ctx->vmobj);

	ctx->dsize2 += npages << PAGE_SHIFT;

	DBGS("%s: extended by %d pages, total now %d\n",
	    __func__, npages, pagenum + npages);

	return (0);
}

/*
 * * * * * Snapshot functions. * * * *
 */

/*
 * This function does the actual snapshot and prepares the dump.
 */

VPSFUNC
int
vps_snapshot(struct vps_dev_ctx *dev_ctx, struct vps *vps,
    struct vps_arg_snapst *va)
{
	struct vps_snapst_ctx *ctx;
	struct vps_dumpheader *dumphdr;
	time_t starttime;
	int error = 0;

#ifdef DIAGNOSTIC
	vps_snapst_print_errormsgs = debug_snapst;
#endif

	starttime = time_second;

	sx_assert(&vps->vps_lock, SX_XLOCKED);

	dev_ctx->snapst = NULL;

	ctx = malloc(sizeof (*ctx), M_VPS_SNAPST, M_WAITOK | M_ZERO);
	LIST_INIT(&ctx->errormsgs);
	dev_ctx->snapst = ctx;

	if (vps_func->vps_dumpobj_create == NULL) {
		ERRMSG(ctx, "%s: vps_libdump module not loaded\n",
		   __func__);
		error = EOPNOTSUPP;
		goto out;
	}

	if (vps->vps_status != VPS_ST_SUSPENDED) {
		ERRMSG(ctx, "%s: vps is not VPS_ST_SUSPENDED\n",
		    __func__);
		error = EBUSY;
		goto out;
	}

	dev_ctx->cmd = VPS_IOC_SNAPST;
	ctx->vps = vps;
	ctx->pagesread = 0;
	vps->vps_status = VPS_ST_SNAPSHOOTING;

	if (vps_ctx_alloc(ctx, vps)) {
		error = ENOMEM;
		goto out;
	}

	vps_ctx_extend(ctx, vps, 10 << PAGE_SHIFT, M_WAITOK);

	ctx->dumpobj_list = NULL;
	dumphdr = (struct vps_dumpheader *)ctx->data;
	memset(dumphdr, 0, sizeof(*dumphdr));
	ctx->cpos = (caddr_t)ctx->data + sizeof(*dumphdr);
	ctx->dsize = sizeof(*dumphdr);

	ctx->nuserpages = 0;
	/* Gets extended dynamically on demand. */
	ctx->userpagelistlength = 1024; /* 1024 items */
	ctx->userpagelist = malloc(ctx->userpagelistlength *
	    sizeof(void *), M_VPS_SNAPST, M_WAITOK|M_ZERO);

	ctx->nuserpages = 0;
	/* Gets extended dynamically on demand. */
	ctx->page_ref_size = 1024; /* 1024 items */
	ctx->page_ref = malloc(ctx->page_ref_size *
	    sizeof(struct vps_page_ref), M_VPS_SNAPST, M_WAITOK|M_ZERO);

	SLIST_INIT(&ctx->obj_list);

	ctx->rootobj = vdo_create(ctx, VPS_DUMPOBJT_ROOT, M_WAITOK);

	if ((error = vps_snapshot_sysinfo(ctx, vps)))
		goto out;

	/* Leaves the vps dump object open */
	if ((error = vps_snapshot_vps(ctx, vps)))
		goto out;

	if ((error = vps_snapshot_mounts(ctx, vps)))
		goto out;

	if ((error = vps_snapshot_arg(ctx, vps)))
		goto out;

	if ((error = vps_snapshot_vnet(ctx, vps, vps->vnet)))
		goto out;

	if ((error = vps_snapshot_prison(ctx, vps)))
		goto out;

	if (vps_func->sem_snapshot_vps &&
	    (error = vps_func->sem_snapshot_vps(ctx, vps)))
		goto out;

	if (vps_func->shm_snapshot_vps &&
	    (error = vps_func->shm_snapshot_vps(ctx, vps)))
		goto out;

	if (vps_func->msg_snapshot_vps &&
	    (error = vps_func->msg_snapshot_vps(ctx, vps)))
		goto out;

	if ((error = vps_snapshot_proc(ctx, vps)))
		goto out;

	/* Close the vps object. */
	vdo_close(ctx);

	/* Close the root object. */
	vdo_close(ctx);

	ctx->nsyspages = (((caddr_t)ctx->cpos - (caddr_t)ctx->data)
	    >> PAGE_SHIFT);
	/* If not exactly a multiple of PAGE_SIZE, add one page. */
	if (ctx->nsyspages << PAGE_SHIFT != (caddr_t)ctx->cpos -
	    (caddr_t)ctx->data)
		ctx->nsyspages += 1;
	va->datalen = (ctx->nsyspages + ctx->nuserpages) << PAGE_SHIFT;
	va->database = NULL;

	dumphdr->byteorder = VPS_MD_DUMPHDR_BYTEORDER;
	dumphdr->ptrsize = VPS_MD_DUMPHDR_PTRSIZE;
	dumphdr->pageshift = PAGE_SHIFT;
	dumphdr->version = VPS_DUMPH_VERSION;
	dumphdr->magic = VPS_DUMPH_MAGIC;
	dumphdr->time = time_second;

	dumphdr->nsyspages = ctx->nsyspages;
	dumphdr->nuserpages = ctx->nuserpages;

	/* The userspace utility relies on this value. */
	dumphdr->size = va->datalen;

	if (vdo_checktree(ctx)) {
		ERRMSG(ctx, "%s: dump tree is invalid !\n", __func__);
		/*
		vdo_printtree(ctx);
		*/
		return (EINVAL);
	}
	vdo_makerelative(ctx);
	DBGS("%s: dump tree is valid and contains %d elements\n",
		__func__, ctx->elements);

	/* This has to be the last write access to syspages
	   dump *grr* !!! */
	dumphdr->checksum = 0;
	dumphdr->checksum = vps_cksum((char *)ctx->data,
	    dumphdr->nsyspages << PAGE_SHIFT);

	DBGS("%s: snapshot dev_ctx=%p dev_ctx->snapst=%p vps=%p "
	    "syspages=%d cksum=%08llx userpages=%d PREPARED\n",
		__func__, dev_ctx, dev_ctx->snapst, vps,
		dumphdr->nsyspages,
		(long long unsigned int)dumphdr->checksum,
		dumphdr->nuserpages);

	/* allocate userspace pages vm object and map it */
	if ((ctx->vps_vmobject = vps_pager_ops.pgo_alloc(ctx,
			ctx->nsyspages + ctx->nuserpages,
			VM_PROT_READ, 0,
			curthread->td_proc->p_ucred)) == NULL) {
		ERRMSG(ctx, "%s: vm_object_allocate: %d\n",
		    __func__, error);
		/* XXX */
		return (error);
	}

	ctx->user_map = &curthread->td_proc->p_vmspace->vm_map;
	vm_map_lock(ctx->user_map);
	if (vm_map_findspace(ctx->user_map, vm_map_min(ctx->user_map),
	                     (ctx->nsyspages + ctx->nuserpages) <<
			     PAGE_SHIFT,
	                     &ctx->user_map_start) != KERN_SUCCESS) {
		error = ENOMEM;
		vm_map_unlock(ctx->user_map);
		ERRMSG(ctx, "%s: vm_map_findspace: %d\n", __func__, error);
		/* XXX */
		return (error);
	}
	error = vm_map_insert(ctx->user_map, ctx->vps_vmobject, 0,
		ctx->user_map_start,
		ctx->user_map_start +
		((ctx->nsyspages + ctx->nuserpages) << PAGE_SHIFT),
		VM_PROT_READ, VM_PROT_READ, 0);
	if (error != KERN_SUCCESS)
		panic("%s: vm_map_insert: error=%d\n", __func__, error);
	vm_map_unlock(ctx->user_map);

	DBGS("%s: snapshot at %zx - %zx\n", __func__,
	    (size_t)ctx->user_map_start,
	    (size_t)(ctx->user_map_start +
	    ((ctx->nsyspages + ctx->nuserpages) << PAGE_SHIFT)));

	va->database = (void *)ctx->user_map_start;

	/*
 	 * Resources are freed in vps_snapshot_finish().
	 */

  out:
	if (error) {
		ERRMSG(ctx, "%s: error=%d\n", __func__, error);
	}

	{
		struct vps_snapst_errmsg *msg;
		char *buf, *bufpos;
		size_t buflen;
		int error2;

		buflen = 0;

		LIST_FOREACH(msg, &ctx->errormsgs, list) {
			printf("%s\n", msg->str);
			buflen += strlen(msg->str);
		}

		buf = bufpos = malloc(buflen + 1, M_TEMP, M_WAITOK);
		LIST_FOREACH(msg, &ctx->errormsgs, list) {
			memcpy(bufpos, msg->str, strlen(msg->str));
			bufpos += strlen(msg->str);
		}
		*(bufpos) = '\0';
		bufpos += 1;

		DBGS("%s: va->msgbase=%p va->msglen=%zu\n",
			__func__, va->msgbase, va->msglen);
		if (va->msgbase != NULL && va->msglen > 0) {
			if (va->msglen < (bufpos - buf))
				printf("%s: warning: user-supplied buffer "
				    "too small for error messages\n",
				    __func__);
			if ((error2 = copyout(buf, va->msgbase,
			    min(va->msglen, (bufpos - buf)))))
				printf("%s: error messages copyout=%d\n",
				    __func__, error2);
		}
		free(buf, M_TEMP);

		while (!LIST_EMPTY(&ctx->errormsgs)) {
			msg = LIST_FIRST(&ctx->errormsgs);
			LIST_REMOVE(msg, list);
			free(msg, M_TEMP);
		}
	}

	DBGS("%s: total time: %lld seconds\n",
		__func__, (long long int)(time_second - starttime));

	return (error);
}

/*
 * Create FILE_PATH dump object for vnode vp at ctx->cpos.
 */
VPSFUNC
static int
vps_snapshot_vnodepath(struct vps_snapst_ctx *ctx, struct vps *vps,
    struct vnode *vp, int howalloc)
{
	struct vps_dumpobj *o1;
	struct vps_dump_filepath *vdfp;
	/*
	struct ucred *save_ucred;
	struct vps *save_vps;
	struct thread *td;
	*/
	char *retbuf, *buf;
	int error;

	if (vp == NULL)
		return (0);

	howalloc &= (M_WAITOK|M_NOWAIT);
	if (howalloc == 0)
		return (EINVAL);

	vref(vp);
	buf = malloc(MAXPATHLEN, M_TEMP, howalloc|M_ZERO);
	if (buf == NULL) {
		vrele(vp);
		return (ENOMEM);
	}

#if 0
// not yet ... for devfs lookups
	td = curthread;
	save_ucred = td->td_ucred;
	save_vps = td->td_vps;
	td->td_ucred = crdup(save_ucred);
	vps_deref(td->td_ucred->cr_vps, td->td_ucred);
	td->td_ucred->cr_vps = vps;
	vps_ref(td->td_ucred->cr_vps, td->td_ucred);
	td->td_vps = vps;
#endif
	
	retbuf = "-";
	error = vn_fullpath1_failsafe(curthread, vp, vps->_rootvnode,
				buf, &retbuf, MAXPATHLEN);

#if 0
	crfree(td->td_ucred);
	td->td_ucred = save_ucred;
	td->td_vps = save_vps;
#endif

	if (error != 0) {
		free(buf, M_TEMP);
		vrele(vp);
		ERRMSG(ctx, "%s: vn_fullpath1() failed for vp=%p\n",
		    __func__, vp);
		return (error);
	}

	DBGS("%s: vnode=%p path=[%s] error=%d\n",
	    __func__, vp, retbuf, error);

	o1 = vdo_create(ctx, VPS_DUMPOBJT_FILE_PATH, howalloc);
	if (o1 == NULL) {
		free(buf, M_TEMP);
		vrele(vp);
		return (ENOMEM);
	}
	if ((vdfp = vdo_space(ctx, sizeof(*vdfp) +
	    roundup(strlen(retbuf) + 1, 8), howalloc)) == NULL) {
		vdo_discard(ctx, o1);
		free(buf, M_TEMP);
		vrele(vp);
		return (ENOMEM);
	}

	vdfp->fp_size = strlen(retbuf);
	strlcpy(vdfp->fp_path, retbuf, vdfp->fp_size + 1);

	vdo_close(ctx);
	free(buf, M_TEMP);
	vrele(vp);

	return (0);
}

VPSFUNC
static int
vps_snapshot_vnodeinodenum(struct vps_snapst_ctx *ctx, struct vps *vps,
    struct vnode *vp, int howalloc)
{
	struct vps_dump_fileinodenum *vdfi;
	struct vps_dumpobj *o1;
	struct vattr vattr;
	int error;

	if (vp == NULL)
		return (0);

	howalloc &= (M_WAITOK|M_NOWAIT);
	if (howalloc == 0)
		return (EINVAL);

	vref(vp);
	vn_lock(vp, LK_SHARED | LK_RETRY);
	error = VOP_GETATTR(vp, &vattr, curthread->td_ucred);
	VOP_UNLOCK(vp, 0);

	if (error != 0) {
		DBGS("%s: vnode=%p VOP_GETATTR(): error=%d\n",
		    __func__, vp, error);
		vrele(vp);
		return (error);
	}

	DBGS("%s: vnode=%p fsid=%u fileid=%ld error=%d\n",
	    __func__, vp, vattr.va_fsid, vattr.va_fileid, error);

	o1 = vdo_create(ctx, VPS_DUMPOBJT_FILE_INODENUM, howalloc);
	if (o1 == NULL) {
		vrele(vp);
		return (ENOMEM);
	}
	if ((vdfi = vdo_space(ctx, sizeof(*vdfi), howalloc)) == NULL) {
		vdo_discard(ctx, o1);
		vrele(vp);
		return (ENOMEM);
	}

	vdfi->fsid = vattr.va_fsid;
	vdfi->fileid = vattr.va_fileid;

	vdo_close(ctx);
	vrele(vp);

	return (0);
}

/*
 * EXPERIMENTAL:
 * Depending of kind of filesystem choose whether we snapshot
 * a path to the vnode (unreliable and sometimes slow) or
 * the inode number (aka file id).
 */
VPSFUNC
static int
vps_snapshot_vnode(struct vps_snapst_ctx *ctx, struct vps *vps,
    struct vnode *vp, int howalloc)
{
	int error;

	if (vp == NULL)
		return (0);

	vref(vp);

	/*
	nfs doesn't support vfs_vget()
	if (!strcmp(vp->v_tag, "newnfs")) {
	*/
	if (0) {
		error = vps_snapshot_vnodeinodenum(ctx, vps, vp, howalloc);
	} else {
		error = vps_snapshot_vnodepath(ctx, vps, vp, howalloc);
	}

	vrele(vp);

	return (error);
}

/*
 * Pseudo teletype device.
 */
VPSFUNC
static int
vps_snapshot_pts(struct vps_snapst_ctx *ctx, struct vps *vps,
		struct tty *tp)
{
	struct vps_dump_pts *vdp;
	struct vps_dump_filepath *vdf;
	struct pts_softc *psc;
	int i;

	if (tp == NULL)
		return (0);

	vdo_create(ctx, VPS_DUMPOBJT_PTS, M_WAITOK);

	vdp = vdo_space(ctx, sizeof(*vdp), M_WAITOK);

	psc = tty_softc(tp);
	vdp->pt_index = psc->pts_unit;
	if (tp->t_pgrp != NULL)
		vdp->pt_pgrp_id = tp->t_pgrp->pg_id;
	else
		vdp->pt_pgrp_id = 0;
	vdp->pt_flags = psc->pts_flags;
	vdp->pt_cred = psc->pts_cred;
	vdp->pt_termios.c_iflag = tp->t_termios.c_iflag;
	vdp->pt_termios.c_oflag = tp->t_termios.c_oflag;
	vdp->pt_termios.c_cflag = tp->t_termios.c_cflag;
	vdp->pt_termios.c_lflag = tp->t_termios.c_lflag;
	vdp->pt_termios.c_ispeed = tp->t_termios.c_ispeed;
	vdp->pt_termios.c_ospeed = tp->t_termios.c_ospeed;
	KASSERT(NCCS <= (sizeof(vdp->pt_termios.c_cc) /
	    sizeof(vdp->pt_termios.c_cc[0])),
	    ("%s: vdp->pt_termios.c_cc smaller than NCCS\n", __func__));
	memset(vdp->pt_termios.c_cc, 0, sizeof(vdp->pt_termios.c_cc));
	for (i = 0; i < NCCS; i++)
		vdp->pt_termios.c_cc[i] = tp->t_termios.c_cc[i];

	vps_snapshot_ucred(ctx, vps, psc->pts_cred, M_WAITOK);

	vdo_create(ctx, VPS_DUMPOBJT_FILE_PATH, M_WAITOK);

	vdf = vdo_space(ctx, sizeof(*vdf), M_WAITOK);
	vdf->fp_size = strlen(tty_devname(tp));
	vdo_space(ctx, roundup(vdf->fp_size + 1, 8), M_WAITOK);
	memcpy(vdf->fp_path, tty_devname(tp), vdf->fp_size);

	DBGS("%s: data=%p path=[%s]\n", __func__, tp, vdf->fp_path);

	vdo_close(ctx);		/* VPS_DUMPOBJT_FILE_PATH */

	vdo_close(ctx);		/* VPS_DUMPOBJT_PTS */

	return (0);
}

VPSFUNC
static int
vps_snapshot_sysinfo(struct vps_snapst_ctx *ctx, struct vps *vps)
{
	struct vps_dump_sysinfo *vds;

	vdo_create(ctx, VPS_DUMPOBJT_SYSINFO, M_WAITOK);

	vds = vdo_space(ctx, sizeof(*vds), M_WAITOK);

	strncpy(vds->kernel, version, MAXHOSTNAMELEN);
	strncpy(vds->hostname, VPS_VPS(vps, hostname), MAXHOSTNAMELEN);

	vds->shared_page_obj = shared_page_obj;

	DBGS("%s: shared_page_obj = %p\n", __func__, shared_page_obj);

	vdo_close(ctx);

	return (0);
}

VPSFUNC
static int
vps_snapshot_vps(struct vps_snapst_ctx *ctx, struct vps *vps)
{
	struct vps_dump_vps *vdi;

	/* Dump vps specific data. */
	vdo_create(ctx, VPS_DUMPOBJT_VPS, M_WAITOK);

	vdi = vdo_space(ctx, sizeof(*vdi), M_WAITOK);
	strlcpy(vdi->hostname, VPS_VPS(vps, hostname),
	    sizeof(vdi->hostname));
	vdi->boottime.tv_sec = VPS_VPS(vps, boottime).tv_sec;
	vdi->boottime.tv_usec = VPS_VPS(vps, boottime).tv_usec;
	vdi->lastpid = VPS_VPS(vps, lastpid);

	vdi->restore_count = vps->restore_count;
	vdi->initpgrp_id = (VPS_VPS(vps, initpgrp) != NULL) ?
	    VPS_VPS(vps, initpgrp)->pg_id : 0;
	vdi->initproc_id = (VPS_VPS(vps, initproc) != NULL) ?
	    VPS_VPS(vps, initproc)->p_pid : 0;
	strlcpy(vdi->vps_name, vps->vps_name, sizeof(vdi->vps_name));
	strlcpy(vdi->rootpath, vps->_rootpath, sizeof(vdi->rootpath));

	/* Object is closed in calling function. */

	return (0);
}

VPSFUNC
static int
vps_snapshot_arg(struct vps_snapst_ctx *ctx, struct vps *vps)
{
	struct vps_dump_arg *vda;
	struct vps_dump_arg_ip4 *vda4;
	struct vps_dump_arg_ip6 *vda6;
	struct vps_dump_accounting *vdacc;
	size_t privsetsize_round;
	caddr_t cpos;
	int error = 0;
	int i;

	/* XXX locking */

	/* If we ran out of memory previously, we try again.
 again:
 */
	vdo_create(ctx, VPS_DUMPOBJT_ARG, M_WAITOK);

	vda = vdo_space(ctx, sizeof(*vda), M_WAITOK);

	vda->privset_size = PRIV_SET_SIZE;
	vda->ip4net_cnt = vps->vps_ip4_cnt;
	vda->ip6net_cnt = vps->vps_ip6_cnt;
	vda->have_accounting = (vps->vps_acc != NULL) ? 1 : 0;

	privsetsize_round = roundup(vda->privset_size, 8);

	cpos = vdo_space(ctx, privsetsize_round, M_WAITOK);
	memcpy(cpos, vps->priv_allow_set, vda->privset_size);

	cpos = vdo_space(ctx, privsetsize_round, M_WAITOK);
	memcpy(cpos, vps->priv_impl_set, vda->privset_size);

	if (vps->vps_ip4_cnt > 0) {
		cpos = vdo_space(ctx, vps->vps_ip4_cnt *
			sizeof(struct vps_dump_arg_ip4), M_WAITOK);
		for (i = 0; i < vps->vps_ip4_cnt; i++) {
			vda4 = (struct vps_dump_arg_ip4 *)cpos;
			memcpy(vda4->a4_addr, &vps->vps_ip4[i].addr, 0x4);
			memcpy(vda4->a4_mask, &vps->vps_ip4[i].mask, 0x4);
			cpos = (caddr_t)(vda4 + 1);
		}
	}

	if (vps->vps_ip6_cnt > 0) {
		cpos = vdo_space(ctx, vps->vps_ip6_cnt *
			sizeof(struct vps_dump_arg_ip6), M_WAITOK);
		for (i = 0; i < vps->vps_ip6_cnt; i++) {
			vda6 = (struct vps_dump_arg_ip6 *)cpos;
			memcpy(vda6->a6_addr, &vps->vps_ip6[i].addr, 0x10);
			vda6->a6_plen = vps->vps_ip6[i].plen;
			cpos = (caddr_t)(vda6 + 1);
		}
	}

	if (vda->have_accounting) {
		vdacc = vdo_space(ctx, sizeof(*vdacc), M_WAITOK);

#define FILL_ACCVAL(x)						\
	vdacc->x.cur = vps->vps_acc->x.cur;			\
	vdacc->x.soft = vps->vps_acc->x.soft;			\
	vdacc->x.hard = vps->vps_acc->x.hard;			\
	vdacc->x.hits_soft = vps->vps_acc->x.hits_soft;		\
	vdacc->x.hits_hard = vps->vps_acc->x.hits_hard

		FILL_ACCVAL(virt);
		FILL_ACCVAL(phys);
		FILL_ACCVAL(kmem);
		FILL_ACCVAL(kernel);
		FILL_ACCVAL(buffer);
		FILL_ACCVAL(pctcpu);
		FILL_ACCVAL(blockio);
		FILL_ACCVAL(threads);
		FILL_ACCVAL(procs);

#undef FILL_ACCVAL
	}

	vdo_close(ctx);

	return (error);
}

VPSFUNC
static int
vps_snapshot_mounts(struct vps_snapst_ctx *ctx, struct vps *vps)
{
	int error = 0;
	struct vps_dumpobj *o1, *o2;
	struct vps_dump_mount *vdm;
	struct vps_dump_mount_opt *vdmopt;
	struct vfsopt *opt;
	struct mount *mp;
	char *mntfrom, *mnton, *fstype, *vpsroot;
	int len;

	/*
	 * Find all mountpoints that are in the vps's root directory
	 * (this includes the root directory itself, if a mountpoint).
	 *
	 * Assume that the mountlist is in correct order regarding
	 * dependencies.
	 *
	 * XXX If vps's rootdir is '/', things will be odd.
	 */

	DBGS("%s: vps's rootpath=[%s] vnode=%p\n",
	    __func__, vps->_rootpath, vps->_rootvnode);

	vpsroot = strdup(vps->_rootpath, M_TEMP);
	if (vpsroot[strlen(vpsroot) - 1] == '/')
		vpsroot[strlen(vpsroot) - 1] = '\0';
	len = strlen(vpsroot);

	/* If we ran out of memory previously, we try again. */
  again:
	o1 = NULL;

	mtx_lock(&mountlist_mtx);
	TAILQ_FOREACH(mp, &mountlist, mnt_list) {
		mntfrom = mp->mnt_stat.f_mntfromname;
		mnton = mp->mnt_stat.f_mntonname;
		fstype = mp->mnt_stat.f_fstypename;
		DBGS("%s: mp=%p: flag=%zx type=[%s] from=[%s] on=[%s]\n",
		    __func__, mp, (size_t)mp->mnt_flag, fstype, mntfrom,
		    mnton);

		if (!(strncmp(vpsroot, mnton, len) == 0 &&
		    (mnton[len] == '\0' || mnton[len] == '/')))
			continue;

#if 0
		/* Mounts performed by jails are not yet supported. */
		if (mp->mnt_cred->cr_prison != VPS_VPS(vps, prison0)) {
			ERRMSG(ctx, "%s: unsupported mount by jail: "
			    "mp=%p\n", __func__, mp);
			mtx_unlock(&mountlist_mtx);
			ctx->cpos = (caddr_t)o1;
			free(vpsroot, M_TEMP);
			return (EINVAL);
		}
#endif
		if ((o2 = vdo_create(ctx, VPS_DUMPOBJT_MOUNT, M_NOWAIT))
		    == NULL) {
			mtx_unlock(&mountlist_mtx);
			if (o1 != NULL)
				vdo_discard(ctx, o1);
			goto again;
		}
		/* Remember the dump object of the first mount. */
		if (o1 == NULL)
			o1 = o2;

		vdm = vdo_space(ctx, sizeof(*vdm), M_NOWAIT);
		if (vdm == NULL) {
			mtx_unlock(&mountlist_mtx);
			vdo_discard(ctx, o1);
			goto again;
		}
		strlcpy(vdm->mntfrom, mntfrom, sizeof(vdm->mntfrom));
		strlcpy(vdm->mnton, mnton, sizeof(vdm->mnton));
		strlcpy(vdm->fstype, fstype, sizeof(vdm->fstype));
		vdm->flags = mp->mnt_flag;
		vdm->optcnt = 0;
		/* Mounted from inside vps ? */
		if (mp->mnt_cred->cr_vps == vps)
			vdm->vpsmount = 1;
		else
			vdm->vpsmount = 0;

		TAILQ_FOREACH(opt, mp->mnt_opt, link) {
			vdmopt = vdo_space(ctx, sizeof(*vdmopt), M_NOWAIT);
			if (vdmopt == NULL) {
				mtx_unlock(&mountlist_mtx);
				vdo_discard(ctx, o1);
				goto again;
			}
			if (opt->len >= sizeof(vdmopt->value)) {
				ERRMSG(ctx, "%s: opt->len=%d (name=[%s]) too big\n",
				    __func__, opt->len, opt->name);
				mtx_unlock(&mountlist_mtx);
				free(vpsroot, M_TEMP);
				return (EINVAL);
			}
			strlcpy(vdmopt->name, opt->name, sizeof(vdmopt->name));
			memcpy(vdmopt->value, opt->value, opt->len);
			vdmopt->len = opt->len;
			vdm->optcnt += 1;
			DBGS("%s: opt name=[%s] value=%p len=%d\n",
			   __func__, opt->name, opt->value, opt->len);
		}

		if (vdm->vpsmount) {
			vdm->mnt_cred = mp->mnt_cred;

			if (vps_snapshot_ucred(ctx, vps, mp->mnt_cred,
			    M_NOWAIT)) {
				mtx_unlock(&mountlist_mtx);
				vdo_discard(ctx, o1);
				goto again;
			}
		} else {
			vdm->mnt_cred = NULL;
		}

		DBGS("%s: [%s] is in [%s]; vpsmount=%d vdm->mnt_cred=%p\n",
		    __func__, mnton, vpsroot, vdm->vpsmount, vdm->mnt_cred);

		/* Next. */
		vdo_close(ctx);
	}
	mtx_unlock(&mountlist_mtx);

	free(vpsroot, M_TEMP);

	return (error);
}

VPSFUNC
static int
vps_snapshot_vnet_route(struct vps_snapst_ctx *ctx, struct vps *vps,
    struct vnet *vnet)
{
	int error = 0;

	vps_snapshot_vnet_route_table(ctx, vps, vnet, 0, AF_INET);
	vps_snapshot_vnet_route_table(ctx, vps, vnet, 0, AF_INET6);

	return (error);
}

VPSFUNC
static int
vps_snapshot_vnet_route_one(struct vps_snapst_ctx *ctx, struct vps *vps,
    struct vnet *vnet, struct radix_node *rn)
{
	struct vps_dumpobj *o1;
	struct vps_dump_route *vdr;
	struct vps_dump_vnet_sockaddr *vds;
	struct rtentry *rt;

	if ((o1 = vdo_create(ctx, VPS_DUMPOBJT_VNET_ROUTE, M_NOWAIT)) ==
	    NULL)
		return (ENOMEM);

	if ((vdr = vdo_space(ctx, sizeof(*vdr), M_NOWAIT)) == NULL) {
		vdo_discard(ctx, o1);
		return (ENOMEM);
	}

	rt = (struct rtentry *)rn;

	vdr->rt_have_mask = 0;
	vdr->rt_have_gateway = 0;
	vdr->rt_have_ifp = 0;
	vdr->rt_have_ifa = 0;
	vdr->rt_flags = rt->rt_flags;
	vdr->rt_fibnum = rt->rt_fibnum;
	vdr->rt_rmx.rmx_mtu = rt->rt_rmx.rmx_mtu;
	vdr->rt_rmx.rmx_expire = rt->rt_rmx.rmx_expire;
	vdr->rt_rmx.rmx_pksent = rt->rt_rmx.rmx_pksent;
	vdr->rt_rmx.rmx_weight = rt->rt_rmx.rmx_weight;

	if (rt_key(rt) != NULL) {
		if ((vds = vdo_space(ctx, sizeof(*vds), M_NOWAIT)) ==
		    NULL) {
			vdo_discard(ctx, o1);
			return (ENOMEM);
		}
		vds->sa_len = rt_key(rt)->sa_len;
		vds->sa_family = rt_key(rt)->sa_family;
		memcpy(vds->sa_data, rt_key(rt)->sa_data, vds->sa_len);
	} else {
		vdo_discard(ctx, o1);
		return (0);
	}

	if (rt_mask(rt) != NULL) {
		if ((vds = vdo_space(ctx, sizeof(*vds), M_NOWAIT)) ==
		    NULL) {
			vdo_discard(ctx, o1);
			return (ENOMEM);
		}
		vds->sa_len = rt_mask(rt)->sa_len;
		vds->sa_family = rt_mask(rt)->sa_family;
		memcpy(vds->sa_data, rt_mask(rt)->sa_data, vds->sa_len);
		vdr->rt_have_mask = 1;
	}

	if (rt->rt_gateway != NULL) {
		if ((vds = vdo_space(ctx, sizeof(*vds), M_NOWAIT)) ==
		    NULL) {
			vdo_discard(ctx, o1);
			return (ENOMEM);
		}
		vds->sa_len = rt->rt_gateway->sa_len;
		vds->sa_family = rt->rt_gateway->sa_family;
		memcpy(vds->sa_data, rt->rt_gateway->sa_data, vds->sa_len);
		vdr->rt_have_gateway = 1;
	}

	if (rt->rt_ifa != NULL ) { //&& rt->rt_ifa->ifa_addr->sa_len > 0) {
		if ((vds = vdo_space(ctx, sizeof(*vds), M_NOWAIT)) ==
		    NULL) {
			vdo_discard(ctx, o1);
			return (ENOMEM);
		}
		vds->sa_len = rt->rt_ifa->ifa_addr->sa_len;
		vds->sa_family = rt->rt_ifa->ifa_addr->sa_family;
		memcpy(vds->sa_data, rt->rt_ifa->ifa_addr->sa_data,
		    vds->sa_len);
		vdr->rt_have_ifa = 1;
	}

	DBGS("%s: rt=%p rt_flags=%p rt_refcnt=%d rt_fibnum=%d\n",
	    __func__, rt, (void*)(intptr_t)rt->rt_flags,
	    rt->rt_refcnt, rt->rt_fibnum);
	DBGS("%s: rt_have_mask=%d rt_have_gateway=%d rt_have_ifa=%d\n",
	    __func__, vdr->rt_have_mask, vdr->rt_have_gateway,
	    vdr->rt_have_ifa);

	vdo_close(ctx);

	return (0);
}

VPSFUNC
static int
vps_snapshot_vnet_route_table(struct vps_snapst_ctx *ctx, struct vps *vps,
	struct vnet *vnet, int fibnum, int af)
{
	struct radix_node_head *rnh;
	struct radix_node **stack;
	struct radix_node **sp;
	struct radix_node *rn;
	struct vps_dumpobj *o1;
	int error = 0;

	/*
	 * Because of the small kernel stack we can't just use a recursive
	 * function for dumping the routing tree.
	 */
	/*
	 * I don't remember what I did but now I'm wondering why I
	 * didn't use rn_walktree(), but there propably was a problem
	 * involved.
	 */

	/* XXX determine required size or extend stack dynamically */
	stack = malloc(sizeof(struct radix_node) * 0x100, M_TEMP, M_WAITOK);

	/* If we couldn't allocate memory we start over from here. */
 again:
	memset(stack, 0, sizeof(struct radix_node) * 0x100);

	o1 = vdo_create(ctx, VPS_DUMPOBJT_VNET_ROUTETABLE, M_WAITOK);

	vdo_append(ctx, &fibnum, sizeof(fibnum), M_WAITOK);
	vdo_append(ctx, &af, sizeof(af), M_WAITOK);

	CURVNET_SET_QUIET(vnet);

	rnh = rt_tables_get_rnh(fibnum, af);

	DBGS("%s: fibnum=%d af=%d rnh=%p\n", __func__, fibnum, af, rnh);

	RADIX_NODE_HEAD_RLOCK(rnh);

	sp = &stack[0xff];
	*sp = rnh->rnh_treetop;

	while (sp <= &stack[0xff] && *sp) {

		if (sp < &stack[0x02]) {
			/* Stack is full. */
			ERRMSG(ctx, "%s: stack is full, skipping remaining "
			    "routing table entries !\n", __func__);
			/* XXX raise error or better restart with a
			   bigger stack ! */
			break;
		}

		/* pop */
		rn = *sp;
		*sp = NULL;
		sp++;

		if (!(rn->rn_flags & RNF_ACTIVE))
			continue;

		if (rn->rn_bit < 0) {

			if ((rn->rn_flags & RNF_ROOT) == 0) {
				/* leaf */
				error = vps_snapshot_vnet_route_one(ctx,
				    vps, vnet, rn);
				if (error == ENOMEM) {
					vdo_discard(ctx, o1);
					RADIX_NODE_HEAD_RUNLOCK(rnh);
					goto again;
				} else if (error != 0) {
					goto out;
				}
			}
			if ((rn = rn->rn_dupedkey)) {
				/* push */
				sp--;
				*sp = rn;
			}

		} else {
			/* tree forks */

			/* push */
			sp--;
			*sp = rn->rn_right;

			/* push */
			sp--;
			*sp = rn->rn_left;
		}

		/*
		DBGS("stack: %p %p %p %p %p %p\n",
		    stack[0xff], stack[0xfe], stack[0xfd],
		    stack[0xfc], stack[0xfb], stack[0xfa]);
		DBGS("stack position: %d\n", &stack[0xff] - sp);
		*/
	}

 out:
	RADIX_NODE_HEAD_RUNLOCK(rnh);

	free(stack, M_TEMP);

	CURVNET_RESTORE();

	if (error)
		vdo_discard(ctx, o1);
	else
		vdo_close(ctx);

	return (error);
}

VPSFUNC
static int
vps_snapshot_vnet_iface_ifaddr(struct vps_snapst_ctx *ctx, struct vps *vps,
    struct ifaddr *ifa)
{
	struct vps_dump_vnet_ifaddr *vdifaddr;
	struct vps_dump_vnet_sockaddr *vdsaddr;
	struct vps_dump_vnet_inet6_lifetime *vdia6lt;
	struct in6_ifaddr *ifaddr6p;

	DBGS("%s: ifa=%p: \n", __func__, ifa);

	vdo_create(ctx, VPS_DUMPOBJT_VNET_ADDR, M_WAITOK);

	vdifaddr = vdo_space(ctx, sizeof(*vdifaddr), M_WAITOK);
	memset(vdifaddr, 0, sizeof(*vdifaddr));

	if (ifa->ifa_addr && ifa->ifa_addr->sa_len != 0) {

		vdifaddr->have_addr = 1;
		vdsaddr = vdo_space(ctx, sizeof(*vdsaddr), M_WAITOK);
		vdsaddr->sa_len = ifa->ifa_addr->sa_len;
		vdsaddr->sa_family = ifa->ifa_addr->sa_family;
		KASSERT(vdsaddr->sa_len <= sizeof(vdsaddr->sa_data),
		    ("%s: sa_len too big\n", __func__));
		memcpy(vdsaddr->sa_data, ifa->ifa_addr->sa_data,
		    vdsaddr->sa_len);
	}
	if (ifa->ifa_dstaddr && ifa->ifa_dstaddr->sa_len != 0) {

		vdifaddr->have_dstaddr = 1;
		vdsaddr = vdo_space(ctx, sizeof(*vdsaddr), M_WAITOK);
		vdsaddr->sa_len = ifa->ifa_dstaddr->sa_len;
		vdsaddr->sa_family = ifa->ifa_dstaddr->sa_family;
		KASSERT(vdsaddr->sa_len <= sizeof(vdsaddr->sa_data),
		    ("%s: sa_len too big\n", __func__));
		memcpy(vdsaddr->sa_data, ifa->ifa_dstaddr->sa_data,
		    vdsaddr->sa_len);
	}
	if (ifa->ifa_netmask && ifa->ifa_netmask->sa_len != 0) {

		vdifaddr->have_netmask = 1;
		vdsaddr = vdo_space(ctx, sizeof(*vdsaddr), M_WAITOK);
		vdsaddr->sa_len = ifa->ifa_netmask->sa_len;
		vdsaddr->sa_family = ifa->ifa_netmask->sa_family;
		KASSERT(vdsaddr->sa_len <= sizeof(vdsaddr->sa_data),
		    ("%s: sa_len too big\n", __func__));
		memcpy(vdsaddr->sa_data, ifa->ifa_netmask->sa_data,
		    vdsaddr->sa_len);
	}

	if (ifa->ifa_addr->sa_family == AF_INET6) {
		ifaddr6p = ifatoia6(ifa);

		vdia6lt = vdo_space(ctx, sizeof(*vdia6lt), M_WAITOK);
		vdia6lt->ia6t_expire = ifaddr6p->ia6_lifetime.ia6t_expire;
		vdia6lt->ia6t_preferred =
		    ifaddr6p->ia6_lifetime.ia6t_preferred;
		vdia6lt->ia6t_vltime = ifaddr6p->ia6_lifetime.ia6t_vltime;
		vdia6lt->ia6t_pltime = ifaddr6p->ia6_lifetime.ia6t_pltime;
	}

	/* XXX - metric, ifa_flags, ... */

	DBGS("%s: ifa: have_addr=%d have_dstaddr=%d have_netmask=%d \n",
	    __func__, vdifaddr->have_addr, vdifaddr->have_dstaddr,
	    vdifaddr->have_netmask);

	vdo_close(ctx);

	return (0);
}

VPSFUNC
static int
vps_snapshot_vnet_iface(struct vps_snapst_ctx *ctx, struct vps *vps,
    struct vnet *vnet)
{
	struct ifnet *ifnetp;
	struct ifaddr *ifaddrp;
	struct vps_dump_vnet_ifnet *vdifnet;
	int last_was_epair = 0;
	int error = 0;

	/* XXX locking */

	CURVNET_SET_QUIET(vnet);
	DBGS("%s: curvnet=%p\n", __func__, curvnet);

	TAILQ_FOREACH(ifnetp, &V_ifnet, if_link) {

  again:
		vdo_create(ctx, VPS_DUMPOBJT_VNET_IFACE, M_WAITOK);

		DBGS("%s: ifnetp=%p: if_xname=[%s]\n",
				__func__, ifnetp, ifnetp->if_xname);

		vdifnet = vdo_space(ctx, sizeof(*vdifnet), M_WAITOK);

		strlcpy(vdifnet->if_dname, ifnetp->if_dname,
		    sizeof(vdifnet->if_dname));
		strlcpy(vdifnet->if_xname, ifnetp->if_xname,
		    sizeof(vdifnet->if_xname));
		vdifnet->if_dunit = ifnetp->if_dunit;
		vdifnet->if_flags = ifnetp->if_flags;

		TAILQ_FOREACH(ifaddrp, &ifnetp->if_addrhead, ifa_link) {
			error = vps_snapshot_vnet_iface_ifaddr(ctx, vps,
			    ifaddrp);
			if (error != 0)
				goto out;
		}

		if (last_was_epair == 0 &&
		    strcmp(ifnetp->if_dname, "epair") == 0) {
			/* The outside interface of the epair
			   needs to be dumped too. */
			ifnetp = ((struct epair_softc *)
			    (ifnetp->if_softc))->oifp;
			last_was_epair = 1;
			vdo_close(ctx);
			goto again;
		} else
			last_was_epair = 0;

		vdo_close(ctx);
	}

  out:
	CURVNET_RESTORE();

	return (error);
}

VPSFUNC
static int
vps_snapshot_vnet(struct vps_snapst_ctx *ctx, struct vps *vps,
    struct vnet *vnet)
{
	struct vps_dump_vnet *vdvnet;
	struct vps_dumpobj *o1;
	int error = 0;

	o1 = vdo_create(ctx, VPS_DUMPOBJT_VNET, M_WAITOK);

	vdvnet = vdo_space(ctx, sizeof(*vdvnet), M_WAITOK);

	vdvnet->orig_ptr = vnet;

	if ((error = vps_snapshot_vnet_iface(ctx, vps, vnet)))
		goto out;

	if ((error = vps_snapshot_vnet_route(ctx, vps, vnet)))
		goto out;

  out:
	if (error)
		vdo_discard(ctx, o1);
	else
		vdo_close(ctx);

	return (error);
}

VPSFUNC
static int
vps_snapshot_ucred(struct vps_snapst_ctx *ctx, struct vps *vps,
    struct ucred *cr, int how)
{
	struct vps_restore_obj *vbo;
	struct vps_dumpobj *o1;
	struct vps_dump_ucred *vdcr;
	int size;
	int i;

	DBGS("%s: cr=%p\n", __func__, cr);

	if (cr == NULL)
		return (0);

	SLIST_FOREACH(vbo, &ctx->obj_list, list)
		if (vbo->type == VPS_DUMPOBJT_UCRED &&
		    vbo->orig_ptr == cr)
			return (0);

	o1 = vdo_create(ctx, VPS_DUMPOBJT_UCRED, how);
	if (o1 == NULL)
		return (ENOMEM);

	size = sizeof(*vdcr) + sizeof(vdcr->cr_groups[0]) * cr->cr_ngroups;
	if ((vdcr = vdo_space(ctx, size, how)) == NULL) {
		vdo_discard(ctx, o1);
		return (ENOMEM);
	}

	vdcr->cr_uid = cr->cr_uid;
	vdcr->cr_ruid = cr->cr_ruid;
	vdcr->cr_svuid = cr->cr_svuid;
	vdcr->cr_rgid = cr->cr_rgid;
	vdcr->cr_svgid = cr->cr_svgid;
	vdcr->cr_flags = cr->cr_flags;
	vdcr->cr_ngroups = cr->cr_ngroups;
	vdcr->cr_origptr = cr;
	/*
	if (cr->cr_prison == VPS_VPS(vps, prison0))
		vdcr->cr_prison = NULL;
	else
	*/
		vdcr->cr_prison = cr->cr_prison;
	KASSERT(cr->cr_prison != VPS_VPS(vps0, prison0),
		("%s: ucred=%p cr_prison == VPS_VPS(vps0, prison0)\n",
		__func__, cr));
	vdcr->cr_vps = cr->cr_vps;
	vdcr->cr_ref = cr->cr_ref;

	for (i = 0; i < cr->cr_ngroups; i++)
		vdcr->cr_groups[i] = cr->cr_groups[i];

	vdo_close(ctx);

	/* Insert into list of dumped objects. */
	vbo = malloc(sizeof(*vbo), M_TEMP, how);
	if (vbo == NULL) {
		vdo_discard(ctx, o1);
		return (ENOMEM);
	}
	vbo->type = VPS_DUMPOBJT_UCRED;
	vbo->orig_ptr = crhold(cr);
	vbo->new_ptr = NULL;
	SLIST_INSERT_HEAD(&ctx->obj_list, vbo, list);

	DBGS("%s: cr=%p not seen before --> dumped\n", __func__, cr);

	return (0);
}

VPSFUNC
static int
vps_snapshot_prison_one(struct vps_snapst_ctx *ctx, struct vps *vps,
    struct prison *pr)
{
	struct vps_dump_prison *vdpr;
	struct vps_dumpobj *o1;
	struct vnode *rootvp;
	caddr_t cpos;
	int error = 0;
	int i;

	DBGS("%s: prison=%p pr_id=%d pr_name=[%s]\n",
		__func__, pr, pr->pr_id, pr->pr_name);

	mtx_assert(&pr->pr_mtx, MA_OWNED);

	pr->pr_ref++;
	prison_unlock(pr);

	o1 = vdo_create(ctx, VPS_DUMPOBJT_PRISON, M_WAITOK);
	vdpr = vdo_space(ctx, sizeof(*vdpr), M_WAITOK);

	prison_lock(pr);
	pr->pr_ref--;

	vdpr->pr_origptr = pr;
	if (pr == VPS_VPS(vps, prison0))
		vdpr->pr_parent = NULL;
	else
		vdpr->pr_parent = pr->pr_parent;

	vdpr->pr_id = pr->pr_id;
	vdpr->pr_securelevel = pr->pr_securelevel;
	vdpr->pr_enforce_statfs = pr->pr_enforce_statfs;
	vdpr->pr_childmax = pr->pr_childmax;
	vdpr->pr_ip4s = pr->pr_ip4s;
	vdpr->pr_ip6s = pr->pr_ip6s;
	vdpr->pr_flags = pr->pr_flags;
	vdpr->pr_allow = pr->pr_allow;
	strlcpy(vdpr->pr_name, pr->pr_name, sizeof(vdpr->pr_name));
	strlcpy(vdpr->pr_path, pr->pr_path, sizeof(vdpr->pr_path));
	vdpr->pr_root = NULL;

	vdo_space(ctx, roundup(vdpr->pr_ip4s * 0x4, 8) +
	    vdpr->pr_ip6s * 0x10, M_WAITOK);

	cpos = vdpr->pr_ipdata;

	for (i = 0; i < vdpr->pr_ip4s; i++) {
		memcpy(cpos, &pr->pr_ip4[i], 0x4);
		cpos += 0x4;
	}
	/*
	if (((size_t)cpos % 8) != 0)
		cpos += 4;
	*/
	cpos = (caddr_t)roundup((size_t)cpos, 8);

	for (i = 0; i < vdpr->pr_ip6s; i++) {
		memcpy(cpos, &pr->pr_ip6[i], 0x10);
		cpos += 0x10;
	}

	/*
	vps_print_ascii(vdpr->pr_ipdata, roundup(vdpr->pr_ip4s * 0x4, 8)
		+ vdpr->pr_ip6s * 0x10);
	*/

	rootvp = pr->pr_root;
	vref(rootvp);

	pr->pr_ref++;
	prison_unlock(pr);

	if ((error = vps_snapshot_vnode(ctx, vps, rootvp, M_WAITOK))) {
		vrele(rootvp);
		DBGS("%s: vps_snapshot_vnode: %d\n", __func__, error);
		goto out;
	}
	vrele(rootvp);

	if (pr != VPS_VPS(vps, prison0) && pr->pr_flags & PR_VNET) {
		KASSERT(pr->pr_vnet != vps->vnet,
		    ("%s: prison=%p has PR_VNET but pr->pr_vnet == "
		    "vps->vnet\n", __func__, pr));
		if ((error = vps_snapshot_vnet(ctx, vps, pr->pr_vnet)))
			goto out;
	}

 out:
	prison_lock(pr);
	pr->pr_ref--;
 //out_locked:
	if (error)
		vdo_discard(ctx, o1);
	else
		vdo_close(ctx);

	return (error);
}

VPSFUNC
static int
vps_snapshot_prison(struct vps_snapst_ctx *ctx, struct vps *vps)
{
	struct prison *ppr, *cpr;
	int level, descend;
	int error = 0;

	sx_slock(&allprison_lock);

	ppr = VPS_VPS(vps, prison0);

	prison_lock(ppr);
	error = vps_snapshot_prison_one(ctx, vps, ppr);
	prison_unlock(ppr);

	FOREACH_PRISON_DESCENDANT_LOCKED_LEVEL(ppr, cpr, descend, level) {
		DBGS("%s: ppr=%p cpr=%p descend=%d level=%d\n",
			__func__, ppr, cpr, descend, level);

		/*
		 * In case of error continue looping in order
		 * to unlock prisons,
		 * but don't dump anything anymore.
		 */
		if (error == 0)
			error = vps_snapshot_prison_one(ctx, vps, cpr);
	}

	sx_sunlock(&allprison_lock);

	return (error);
}

VPSFUNC
static int
vps_snapshot_proc(struct vps_snapst_ctx *ctx, struct vps *vps)
{
	struct vps_restore_obj *vbo;
	struct vps_dumpobj *o1;
	struct vps_dump_pgrp *vdpg;
	struct vps_dump_session *vdsess;
	struct session *sess;
	struct pgrp *pg;
	struct proc *p, *p2;
	int found;
	int i;
	int error = 0;

	sx_slock(&VPS_VPS(vps, proctree_lock));

	/* Dump all process groups (pgrp), and all sessions. */
	for (i = 0; i <= VPS_VPS(vps, pgrphash); i++)
		LIST_FOREACH(pg, &VPS_VPS(vps, pgrphashtbl)[i], pg_hash) {
			DBGS("%s: pgrp=%p pg_id=%d pg_session=%p\n",
			    __func__, pg, pg->pg_id, pg->pg_session);

			o1 = vdo_create(ctx, VPS_DUMPOBJT_PGRP, M_WAITOK);

			vdpg = vdo_space(ctx, sizeof(*vdpg), M_WAITOK);
			vdpg->pg_id = pg->pg_id;
			vdpg->pg_jobc = pg->pg_jobc;
			vdpg->pg_session_id = pg->pg_session->s_sid;

			/* XXX necessary ?
			SLIST_INSERT_HEAD(ctx->dumpobj_list, o1, list);
			*/

			found = 0;
			SLIST_FOREACH(vbo, &ctx->obj_list, list)
				if (vbo->type == VPS_DUMPOBJT_SESSION &&
				    vbo->orig_ptr == pg->pg_session)
					found = 1;

			if (found == 1) {
				vdo_close(ctx); /* pgrp */
				continue;
			}

			/* Dump session. */
			vdo_create(ctx, VPS_DUMPOBJT_SESSION, M_WAITOK);

			sess = pg->pg_session;
			vdsess = vdo_space(ctx, sizeof(*vdsess), M_WAITOK);
			vdsess->s_sid = sess->s_sid;
			vdsess->s_count = sess->s_count;
			vdsess->s_leader_id = (sess->s_leader != NULL) ?
				sess->s_leader->p_pid : 0;
			vdsess->s_have_ttyvp = (sess->s_ttyvp != NULL) ?
			    1 : 0;
			KASSERT(sizeof(sess->s_login) <= 
			    sizeof(vdsess->s_login),
			    ("%s: sess->s_login too big\n", __func__));
			memcpy(vdsess->s_login, sess->s_login,
			    sizeof(sess->s_login));

			if (sess->s_ttyvp)
				vps_snapshot_vnode(ctx, vps,
				    sess->s_ttyvp, M_WAITOK);

			vdo_close(ctx);

			/* Insert into list of dumped objects. */
			vbo = malloc(sizeof(*vbo), M_TEMP, M_WAITOK);
			vbo->type = VPS_DUMPOBJT_SESSION;
			vbo->orig_ptr = pg->pg_session;
			vbo->new_ptr = NULL;
			SLIST_INSERT_HEAD(&ctx->obj_list, vbo, list);

			DBGS("%s: dump session %p/%d: s_ttyvp=%p s_ttyp=%p"
			    "\n", __func__, pg->pg_session, sess->s_sid,
			    sess->s_ttyvp, sess->s_ttyp);

			vdo_close(ctx); /* pgrp */
		}

	sx_sunlock(&VPS_VPS(vps, proctree_lock));

	sx_slock(&VPS_VPS(vps, allproc_lock));

	LIST_FOREACH_SAFE(p, &VPS_VPS(vps, allproc), p_list, p2) {
		if ((error = vps_snapshot_proc_one(ctx, vps, p))) {
			ERRMSG(ctx, "%s: vps_snapshot_proc_one(p=%p) "
			    "returned error\n", __func__, p);
			goto out;
		}
	}

 out:
	sx_sunlock(&VPS_VPS(vps, allproc_lock));

	return (error);
}

VPSFUNC
static int
vps_snapshot_sysentvec(struct vps_snapst_ctx *ctx, struct vps *vps,
		struct proc *p)
{
	struct vps_dump_sysentvec *vds;
	struct sysentvec *sv;
	long svtype;

	sv = p->p_sysent;

        if (vps_md_snapshot_sysentvec(sv, &svtype) != 0) {
		ERRMSG(ctx, "%s: proc=%p/%u unknown sysentvec %p\n",
		    __func__, p, p->p_pid, sv);
		return (EINVAL);
        }

	vdo_create(ctx, VPS_DUMPOBJT_SYSENTVEC, M_WAITOK);

	vds = vdo_space(ctx, sizeof(*vds), M_WAITOK);
	vds->sv_type = svtype;

	vdo_close(ctx);

	return (0);
}

VPSFUNC
static int
vps_snapshot_pipe(struct vps_snapst_ctx *ctx, struct vps *vps,
		struct pipe *pi)
{
	struct vps_restore_obj *vbo;
	struct vps_dump_pipe *vdp;
	struct pipepair *pp;
	int error = 0;
	char f_dump;

	vdo_create(ctx, VPS_DUMPOBJT_PIPE, M_WAITOK);

	pp = pi->pipe_pair;

	DBGS("%s: pipe=%p pipepair=%p\n", __func__, pi, pp);

	/* Check in list of dumped objects, if already dumped. */
	f_dump = 1;
	SLIST_FOREACH(vbo, &ctx->obj_list, list)
		if (vbo->type == VPS_DUMPOBJT_PIPE &&
		    vbo->spare[0] == pp)
			f_dump = 0;

#if 0
//delete
	/* Save the pointers. */
	vdp.have_dumped_pipe = f_dump;
	vdp.thisend = pi;
	vdp.pair = pp;
	vdp.rpipe = &pp->pp_rpipe;
	vdp.wpipe = &pp->pp_wpipe;

	vdo_append(ctx, &vdp, sizeof(vdp), M_WAITOK);
#endif

	vdp = vdo_space(ctx, sizeof(*vdp), M_WAITOK);

	vdp->pi_have_dumped_pipe = f_dump;
	vdp->pi_localend = pi;
	vdp->pi_pair = pp;
	vdp->pi_rpipe = &pp->pp_rpipe;
	vdp->pi_wpipe = &pp->pp_wpipe;

	if (f_dump) {
		DBGS("%s: dumping ... \n", __func__);

		/* not used by restore function
		vdo_append(ctx, pp, sizeof(*pp), M_WAITOK);
		*/

		/* XXX dump buffered data if any */

		if (pp->pp_rpipe.pipe_buffer.cnt > 0)
			DBGS("%s: has data: rpipe->pipe_buffer.cnt=%u\n",
			    __func__, pp->pp_rpipe.pipe_buffer.cnt);
		if (pp->pp_rpipe.pipe_map.cnt > 0)
			DBGS("%s: has data: rpipe->pipe_map.cnt=%zu\n",
			    __func__, pp->pp_rpipe.pipe_map.cnt);
		if (pp->pp_wpipe.pipe_buffer.cnt > 0)
			DBGS("%s: has data: wpipe->pipe_buffer.cnt=%u\n",
			    __func__, pp->pp_wpipe.pipe_buffer.cnt);
		if (pp->pp_wpipe.pipe_map.cnt > 0)
			DBGS("%s: has data: wpipe->pipe_map.cnt=%zu\n",
			    __func__, pp->pp_wpipe.pipe_map.cnt);

		/* Insert into list of dumped objects. */
		vbo = malloc(sizeof(*vbo), M_TEMP, M_WAITOK);
		vbo->type = VPS_DUMPOBJT_PIPE;
		vbo->orig_ptr = pi;
		vbo->new_ptr = NULL;
		vbo->spare[0] = pp;
		SLIST_INSERT_HEAD(&ctx->obj_list, vbo, list);
	}

	vdo_close(ctx);

	return (error);
}

VPSFUNC
static int
vps_snapshot_mbufchain(struct vps_snapst_ctx *ctx, struct vps *vps,
		struct mbuf *m)
{
	struct vps_dump_mbufchain *vdmc;
	struct vps_dump_mbuf *vdmb;
	struct vps_dumpobj *o1;
	struct mbuf *m2;
	int error = 0;
	int i;

	if ((o1 = vdo_create(ctx, VPS_DUMPOBJT_MBUFCHAIN,
	    M_NOWAIT)) == NULL)
		return (ENOMEM);

	if ((vdmc = vdo_space(ctx, sizeof(*vdmc), M_NOWAIT)) == NULL) {
		vdo_discard(ctx, o1);
		return (ENOMEM);
	}

	vdmc->mc_mbcount = 0;

	m2 = m;

	/* Count the chain length. */
	while (m2) {
		m2 = m2->m_next;
		vdmc->mc_mbcount++;
	}

	/* Actually dump a chain. */
	m2 = m;
	for (i = 0; i < vdmc->mc_mbcount; i++) {

		if ((vdmb = vdo_space(ctx, sizeof(*vdmb), M_NOWAIT)) == 
		    NULL) {
			vdo_discard(ctx, o1);
			return (ENOMEM);
		}
		vdmb->mb_orig_ptr = m2;
		vdmb->mb_type = m2->m_type;
		vdmb->mb_len = m2->m_len;
		vdmb->mb_flags = m2->m_flags;
		vdmb->mb_have_dat = 0;
		vdmb->mb_have_ext = 0;
		vdmb->mb_have_data = 0;
		vdmb->mb_payload_size = 0;

		DBGS("%s: m=%p type=%d flags=%d len=%d next=%p "
		    "nextpkt=%p\n", __func__, m2, m2->m_type,
		    m2->m_flags, m2->m_len, m2->m_next, m2->m_nextpkt);

		/* Force EXT_PACKET --> EXT_CLUSTER */
		if (m2->m_flags & M_EXT && m2->m_ext.ext_type == EXT_PACKET)
			//dm2->m_ext.ext_type = EXT_CLUSTER;
			;

		if ((m2->m_flags & M_EXT) == 0) {

			//vps_print_ascii(m2->m_dat, MLEN);

			vdmb->mb_have_dat = 1;
			vdmb->mb_payload_size = MLEN;
			if (vdo_append(ctx, m2->m_dat, roundup(MLEN, 8),
			    M_NOWAIT)) {
				vdo_discard(ctx, o1);
				return (ENOMEM);
			}
			if (m2->m_data != NULL) {
				vdmb->mb_have_data = 1;
				vdmb->mb_data_off = m2->m_data - m2->m_dat;
			}

		} else if (m2->m_flags & M_EXT) {

			DBGS("%s: M_EXT ext_type=%d ext_size=%u\n",
			    __func__, m2->m_ext.ext_type,
			    m2->m_ext.ext_size);

			if (m2->m_ext.ext_type != EXT_CLUSTER &&
				m2->m_ext.ext_type != EXT_JUMBOP &&
				m2->m_ext.ext_type != EXT_JUMBO9 &&
				m2->m_ext.ext_type != EXT_JUMBO16 &&
				m2->m_ext.ext_type != EXT_PACKET) {
				ERRMSG(ctx, "%s: DON'T KNOW HOW TO HANDLE "
				    "MBUF M_EXT TYPE!\n", __func__);
				return (EINVAL);
			}

			/* checksum */
			{
				u_int32_t sum = 0, i;

				for (i = 0; i < m2->m_ext.ext_size; i++)
					sum += (u_char)m2->m_ext.ext_buf[i];
				DBGS("%s: checksum=%08x\n", __func__, sum);

				vdmb->mb_checksum = sum;
			}

			if (vdo_append(ctx, m2->m_ext.ext_buf,
			    roundup(m2->m_ext.ext_size, 8), M_NOWAIT)) {
				vdo_discard(ctx, o1);
				return (ENOMEM);
			}
			vdmb->mb_have_ext = 1;
			vdmb->mb_payload_size = m2->m_ext.ext_size;
			if (m2->m_data != NULL) {
				vdmb->mb_have_data = 1;
				vdmb->mb_data_off = m2->m_data -
				    m2->m_ext.ext_buf;
			}

			/*
			vps_print_ascii(m2->m_ext.ext_buf,
			    m2->m_ext.ext_size);
			*/

		} else {
			ERRMSG(ctx, "%s: DON'T KNOW HOW TO HANDLE MBUF!\n",
			    __func__);
			return (EINVAL);
		}
		if (m2->m_flags & M_PKTHDR) {
			DBGS("%s: m2=%p header=%p rcvif=%p \n",
			    __func__, m2, m2->m_pkthdr.header,
			    m2->m_pkthdr.rcvif);
		}

		DBGS("%s: vdmb: mb_have_dat=%d mb_have_ext=%d "
		    "mb_payload_size=%u\n", __func__, vdmb->mb_have_dat,
		    vdmb->mb_have_ext, vdmb->mb_payload_size);

		m2 = m2->m_next;
	}

	vdo_close(ctx);

	DBGS("%s: %d mbufs dumped\n", __func__, vdmc->mc_mbcount);

	return (error);
}

VPSFUNC
static int
vps_snapshot_sockbuf(struct vps_snapst_ctx *ctx, struct vps *vps,
		struct sockbuf *sb)
{
	struct vps_dumpobj *o1;
	struct vps_dump_sockbuf *vdsb;
	int error = 0;

	if (sb->sb_upcall != NULL) {
		ERRMSG(ctx, "%s: sb->sb_upcall != NULL\n", __func__);
		return (EINVAL);
	}

	if ((o1 = vdo_create(ctx, VPS_DUMPOBJT_SOCKBUF, M_NOWAIT)) == NULL)
		return (ENOMEM);

	if ((vdsb = vdo_space(ctx, sizeof(*vdsb), M_NOWAIT)) == NULL) {
		vdo_discard(ctx, o1);
		return (ENOMEM);
	}

	vdsb->sb_mb = sb->sb_mb;
	vdsb->sb_mbtail = sb->sb_mbtail;
	vdsb->sb_lastrecord = sb->sb_lastrecord;
	vdsb->sb_sndptr = sb->sb_sndptr;
	vdsb->sb_state = sb->sb_state;
	vdsb->sb_flags = sb->sb_flags;
	vdsb->sb_sndptroff = sb->sb_sndptroff;
	vdsb->sb_cc = sb->sb_cc;
	vdsb->sb_hiwat = sb->sb_hiwat;
	vdsb->sb_mbcnt = sb->sb_mbcnt;
	vdsb->sb_mcnt = sb->sb_mcnt;
	vdsb->sb_ccnt = sb->sb_ccnt;
	vdsb->sb_mbmax = sb->sb_mbmax;
	vdsb->sb_ctl = sb->sb_ctl;
	vdsb->sb_lowat = sb->sb_lowat;
	vdsb->sb_timeo = sb->sb_timeo;

	if (sb->sb_mb) {
		if ((error = vps_snapshot_mbufchain(ctx, vps, sb->sb_mb))) {
			vdo_discard(ctx, o1);
			return (error);
		}
	}

	DBGS("%s: sb=%p sb_mb=%p sb_mbtail=%p sb_lastrecord=%p "
	    "sb_sndptr=%p\n", __func__, sb, sb->sb_mb, sb->sb_mbtail,
	    sb->sb_lastrecord, sb->sb_sndptr);
	DBGS("%s: sb=%p sb->sb_cc=%u sb->sb_mb=%p sb->sb_sndptroff=%u\n",
	    __func__, sb, sb->sb_cc, sb->sb_mb, sb->sb_sndptroff);

	vdo_close(ctx);

	return (error);
}

VPSFUNC
static int
vps_snapshot_socket_unix(struct vps_snapst_ctx *ctx, struct vps *vps,
    struct socket *so)
{
	struct vps_dump_vnet_sockaddr *vdsaddr;
	struct vps_dump_unixpcb *vdunpcb;
	struct unpcb *un_pcb;
	int error;
	int i;

	error = 0;
	un_pcb = (struct unpcb *)so->so_pcb;

	if ((vdunpcb = vdo_space(ctx, sizeof(*vdunpcb),
	    M_NOWAIT)) == NULL) {
		error = ENOMEM;
		goto drop;
	}

	vdunpcb->unp_have_conn = 0;
	vdunpcb->unp_have_addr = 0;
	vdunpcb->unp_have_vnode = 0;
	vdunpcb->unp_conn_socket = NULL;

	vdunpcb->unp_socket = un_pcb->unp_socket;
	vdunpcb->unp_flags = un_pcb->unp_flags;
	vdunpcb->unp_cc = un_pcb->unp_cc;
	vdunpcb->unp_mbcnt = un_pcb->unp_mbcnt;

	vdunpcb->unp_peercred.cr_uid = un_pcb->unp_peercred.cr_uid;
	vdunpcb->unp_peercred.cr_ngroups =
	    min(un_pcb->unp_peercred.cr_ngroups, 16);
	for (i = 0; i < vdunpcb->unp_peercred.cr_ngroups; i++)
		vdunpcb->unp_peercred.cr_groups[i] =
			un_pcb->unp_peercred.cr_groups[i];

	if (un_pcb->unp_conn != NULL) {
		vdunpcb->unp_have_conn = 1;
		vdunpcb->unp_conn_socket = un_pcb->unp_conn->unp_socket;
	}

	if (un_pcb->unp_addr != NULL) {
		vdunpcb->unp_have_addr = 1;
		if ((vdsaddr = vdo_space(ctx, sizeof(*vdsaddr),
		    M_NOWAIT)) == NULL) {
			error = ENOMEM;
			goto drop;
		}
		vdsaddr->sa_len = un_pcb->unp_addr->sun_len;
		vdsaddr->sa_family = un_pcb->unp_addr->sun_family;
		memcpy(vdsaddr->sa_data, un_pcb->unp_addr->sun_path,
		    vdsaddr->sa_len);
	}

	if (un_pcb->unp_vnode != NULL) {
		vdunpcb->unp_have_vnode = 1;
		/*
		if ((error = vps_snapshot_vnode(ctx, vps,
		    un_pcb->unp_vnode, M_NOWAIT)))
			goto drop;
		*/
	}

  drop:
	return (error);
}

VPSFUNC
static int
vps_snapshot_socket_inet(struct vps_snapst_ctx *ctx, struct vps *vps,
    struct socket *so)
{
	struct vps_dump_inetpcb *vdinpcb;
	struct vps_dump_udppcb *vdudpcb;
	struct vps_dump_tcppcb *vdtcpcb;
	struct inpcb *inpcb;
	struct udpcb *udp_pcb;
	struct tcpcb *tcp_pcb;
	int error;

	error = 0;
	inpcb = (struct inpcb *)so->so_pcb;

	if ((vdinpcb = vdo_space(ctx, sizeof(*vdinpcb),
	    M_NOWAIT)) == NULL) {
		error = ENOMEM;
		goto drop;
	}

	vdinpcb->inp_vflag = inpcb->inp_vflag;
	vdinpcb->inp_flags = inpcb->inp_flags;
	vdinpcb->inp_flags2 = inpcb->inp_flags2;
	vdinpcb->inp_ip_p = inpcb->inp_ip_p;
	vdinpcb->inp_have_ppcb = 0;

	vdinpcb->inp_inc.inc_flags = inpcb->inp_inc.inc_flags;
	vdinpcb->inp_inc.inc_len = inpcb->inp_inc.inc_len;
	vdinpcb->inp_inc.inc_fibnum = inpcb->inp_inc.inc_fibnum;
	vdinpcb->inp_inc.ie_fport = inpcb->inp_inc.inc_ie.ie_fport;
	vdinpcb->inp_inc.ie_lport = inpcb->inp_inc.inc_ie.ie_lport;

	if (vdinpcb->inp_vflag & INP_IPV6) {
		memcpy(vdinpcb->inp_inc.ie_ufaddr,
		    &inpcb->inp_inc.inc6_faddr, 0x10);
		memcpy(vdinpcb->inp_inc.ie_uladdr,
		    &inpcb->inp_inc.inc6_laddr, 0x10);
	} else {
		memcpy(vdinpcb->inp_inc.ie_ufaddr,
		    &inpcb->inp_inc.inc_faddr, 0x4);
		memcpy(vdinpcb->inp_inc.ie_uladdr,
		    &inpcb->inp_inc.inc_laddr, 0x4);
	}

	if (inpcb->inp_ppcb == NULL)
		return (0);

	vdinpcb->inp_have_ppcb = 1;

	INP_INFO_RLOCK(inpcb->inp_pcbinfo);
	INP_RLOCK(inpcb);

	/* inpcb->inp_ip_p seems to be 0 and only used for raw ip
	   and divert sockets! */
	switch (so->so_proto->pr_protocol) {
	case IPPROTO_TCP:
		DBGS("%s: IPPROTO_TCP\n", __func__);
		tcp_pcb = (struct tcpcb *)inpcb->inp_ppcb;
		if ((vdtcpcb = vdo_space(ctx, sizeof(*vdtcpcb),
		    M_NOWAIT)) == NULL) {
			INP_RUNLOCK(inpcb);
			INP_INFO_RUNLOCK(inpcb->inp_pcbinfo);
			error = ENOMEM;
			goto drop;
		}
		vdtcpcb->t_state = tcp_pcb->t_state;
		vdtcpcb->t_flags = tcp_pcb->t_flags;
		vdtcpcb->snd_una = tcp_pcb->snd_una;
		vdtcpcb->snd_max = tcp_pcb->snd_max;
		vdtcpcb->snd_nxt = tcp_pcb->snd_nxt;
		vdtcpcb->snd_up = tcp_pcb->snd_up;
		vdtcpcb->snd_wl1 = tcp_pcb->snd_wl1;
		vdtcpcb->snd_wl2 = tcp_pcb->snd_wl2;
		vdtcpcb->iss = tcp_pcb->iss;
		vdtcpcb->irs = tcp_pcb->irs;
		vdtcpcb->rcv_nxt = tcp_pcb->rcv_nxt;
		vdtcpcb->rcv_adv = tcp_pcb->rcv_adv;
		vdtcpcb->rcv_wnd = tcp_pcb->rcv_wnd;
		vdtcpcb->rcv_up = tcp_pcb->rcv_up;
		vdtcpcb->snd_wnd = tcp_pcb->snd_wnd;
		vdtcpcb->snd_cwnd = tcp_pcb->snd_cwnd;
		vdtcpcb->snd_ssthresh = tcp_pcb->snd_ssthresh;
		break;
	case IPPROTO_UDP:
		DBGS("%s: IPPROTO_UDP\n", __func__);
		udp_pcb = (struct udpcb *)inpcb->inp_ppcb;
		if (udp_pcb->u_tun_func != NULL) {
			ERRMSG(ctx, "%s: udp socket with tunneling "
			    "function set, skipping !\n", __func__);
			INP_RUNLOCK(inpcb);
			INP_INFO_RUNLOCK(inpcb->inp_pcbinfo);
			error = EINVAL;
			goto drop;
		}
		if ((vdudpcb = vdo_space(ctx, sizeof(*vdudpcb),
		    M_NOWAIT)) == NULL) {
			INP_RUNLOCK(inpcb);
			INP_INFO_RUNLOCK(inpcb->inp_pcbinfo);
			error = ENOMEM;
			goto drop;
		}
		vdudpcb->u_have_tun_func = 0;
		vdudpcb->u_flags = udp_pcb->u_flags;
		break;
	case IPPROTO_ICMP:
	case IPPROTO_RAW:
		/* Nothing to do. */
		break;
	default:
		ERRMSG(ctx, "%s: unhandled IPPROTO %d\n",
			__func__, inpcb->inp_ip_p);
		error = EINVAL;
		INP_RUNLOCK(inpcb);
		INP_INFO_RUNLOCK(inpcb->inp_pcbinfo);
		goto drop;
		break;
	}
	INP_INFO_RUNLOCK(inpcb->inp_pcbinfo);
	INP_RUNLOCK(inpcb);

  drop:
	return (error);
}

VPSFUNC
static int
vps_snapshot_socket(struct vps_snapst_ctx *ctx, struct vps *vps,
    struct socket *so)
{
	struct vps_restore_obj *vbo;
	struct vps_dumpobj *o1;
	struct vps_dump_socket *vds;
	struct socket *so2;
	int error = 0;

	SLIST_FOREACH(vbo, &ctx->obj_list, list)
		if (vbo->type == VPS_DUMPOBJT_SOCKET &&
		    vbo->orig_ptr == so) {
			DBGS("%s: socket=%p already dumped\n",
			    __func__, so);
			return (0);
	}

	/* If we couldn't allocate memory we try again. */
  again:

	sblock(&so->so_snd, SBL_WAIT | SBL_NOINTR);
	sblock(&so->so_rcv, SBL_WAIT | SBL_NOINTR);
	SOCKBUF_LOCK(&so->so_snd);
	SOCKBUF_LOCK(&so->so_rcv);

	if ((o1 = vdo_create(ctx, VPS_DUMPOBJT_SOCKET, M_NOWAIT)) == NULL) {
		error = ENOMEM;
		goto drop;
	}

	if ((vds = vdo_space(ctx, sizeof(*vds), M_NOWAIT)) == NULL) {
		error = ENOMEM;
		goto drop;
	}

	vds->so_family = so->so_proto->pr_domain->dom_family;
	vds->so_protocol = so->so_proto->pr_protocol;
	vds->so_type = so->so_proto->pr_type;

	vds->so_options = so->so_options;
	vds->so_state = so->so_state;
	vds->so_qlimit = so->so_qlimit;
	vds->so_qstate = so->so_qstate;
	vds->so_qlen = so->so_qlen;
	vds->so_incqlen = so->so_incqlen;
	vds->so_cred = so->so_cred;
	vds->so_orig_ptr = so;

	DBGS("%s: socket protocol family=%d protocol=%d type=%d\n",
	    __func__, vds->so_family, vds->so_protocol, vds->so_type);

	switch (vds->so_family) {
		case PF_UNIX:
			error = vps_snapshot_socket_unix(ctx, vps, so);
			if (error != 0)
				goto drop;
			break;
		case PF_INET:
		case PF_INET6:
			error = vps_snapshot_socket_inet(ctx, vps, so);
			if (error != 0)
				goto drop;
			break;

		default:
			ERRMSG(ctx, "%s: unhandled protocol family %d\n",
			    __func__, vds->so_family);
			error = EINVAL;
			goto drop;
			break;
	}

	if ((error = vps_snapshot_ucred(ctx, vps, so->so_cred, M_NOWAIT)))
		goto drop;
	if ((error = vps_snapshot_sockbuf(ctx, vps, &so->so_rcv)))
		goto drop;
	if ((error = vps_snapshot_sockbuf(ctx, vps, &so->so_snd)))
		goto drop;


	SOCKBUF_UNLOCK(&so->so_rcv);
	SOCKBUF_UNLOCK(&so->so_snd);
	sbunlock(&so->so_rcv);
	sbunlock(&so->so_snd);

	vdo_close(ctx);

	/* Sockets that are on the accept queue of this socket. */
	if (so->so_qlen > 0 || so->so_incqlen > 0) {
		DBGS("%s: so=%p so_qlen=%d so_incqlen=%d\n",
			__func__, so, so->so_qlen, so->so_incqlen);
		TAILQ_FOREACH(so2, &so->so_comp, so_list) {
			DBGS("%s: so_comp: so2=%p \n", __func__, so2);
			if ((error = vps_snapshot_socket(ctx, vps, so2)))
				goto drop;
		}
		TAILQ_FOREACH(so2, &so->so_incomp, so_list) {
			DBGS("%s: so_incomp: so2=%p \n", __func__, so2);
			if ((error = vps_snapshot_socket(ctx, vps, so2)))
				goto drop;
		}
	}

	/* Insert into list of dumped objects. */
	vbo = malloc(sizeof(*vbo), M_TEMP, M_WAITOK);
	vbo->type = VPS_DUMPOBJT_SOCKET;
	vbo->orig_ptr = so;
	vbo->new_ptr = NULL;
	SLIST_INSERT_HEAD(&ctx->obj_list, vbo, list);

	DBGS("%s: socket=%p not seen before --> dumped\n", __func__, so);

	return (0);

  drop:
	SOCKBUF_UNLOCK(&so->so_rcv);
	SOCKBUF_UNLOCK(&so->so_snd);
	sbunlock(&so->so_rcv);
	sbunlock(&so->so_snd);
	/* SOCK_UNLOCK(so); */

	DBGS("%s: error = %d\n", __func__, error);
	if (o1 != NULL)
		vdo_discard(ctx, o1);

	if (error == ENOMEM)
		goto again;

	return (error);
}

/* XXX */
int kqueue_register(struct kqueue *kq, struct kevent *kev,
    struct thread *td, int waitok);
int kqueue_acquire(struct file *fp, struct kqueue **kqp);
void kqueue_release(struct kqueue *kq, int locked);

VPSFUNC
__attribute__((unused))
static int
vps_snapshot_kqueue(struct vps_snapst_ctx *ctx, struct vps *vps,
    struct file *fp)
{
	struct vps_dump_knote *vdkn;
	struct kevent *kev;
	struct kqueue *kq;
	struct knote *kn;
	int error;
	int i;

	error = 0;

	vdo_create(ctx, VPS_DUMPOBJT_KQUEUE, M_WAITOK);

	kq = NULL;
        if ((error = kqueue_acquire(fp, &kq)) != 0) {
		ERRMSG(ctx, "%s: kqueue_acquire(): error=%d\n",
		    __func__, error);
		goto out;
	}

	for (i = 0; i < kq->kq_knlistsize; i++) {
		/*DBGS("%s: kqueue=%p i=%d\n", __func__, kq, i);*/
		SLIST_FOREACH(kn, &kq->kq_knlist[i], kn_link) {
			DBGS("%s: knote=%p kn_status=%08x\n",
			    __func__, kn, kn->kn_status);
			if (kn != NULL && (kn->kn_status & KN_INFLUX) ==
			    KN_INFLUX) {
				DBGS("%s: kn->kn_status & KN_INFLUX\n",
				    __func__);
				/* XXX have to sleep here */
			}
			kev = &kn->kn_kevent;
			DBGS("kevent: ident  = 0x%016zx\n",
			    (size_t)kev->ident);
			DBGS("kevent: filter = 0x%04hx\n",
			    kev->filter);
			DBGS("kevent: flags  = 0x%04hx\n",
			    kev->flags);
			DBGS("kevent: fflags = 0x%08x\n",
			    kev->fflags);
			DBGS("kevent: data   = 0x%016zx\n",
			    (size_t)kev->data);
			DBGS("kevent: udata  = 0x%016lx\n",
			    (long unsigned int)kev->udata);

			vdo_create(ctx, VPS_DUMPOBJT_KNOTE, M_WAITOK);

			vdkn = vdo_space(ctx, sizeof(*vdkn), M_WAITOK);
			vdkn->kn_status = kn->kn_status;
			vdkn->ke_ident = kev->ident;
			vdkn->ke_filter = kev->filter;
			vdkn->ke_flags = kev->flags;
			vdkn->ke_fflags = kev->fflags;
			vdkn->ke_data = kev->data;
			vdkn->ke_udata = kev->udata;

			vdo_close(ctx);
		}
	}

  out:
	if (kq != NULL)
		kqueue_release(kq, 0);

	vdo_close(ctx);

	return (error);
}

VPSFUNC
static int
vps_snapshot_fdset(struct vps_snapst_ctx *ctx, struct vps *vps,
    struct proc *p)
{
	struct vps_restore_obj *vbo;
	struct vps_dump_filedesc *vdfd;
	struct vps_dump_file *vdf;
	struct vps_dumpobj *o1;
	struct filedesc *fdp;
	struct file *fp;
	int error = 0;
	int found;
	int i;

	fdp = p->p_fd;

	SLIST_FOREACH(vbo, &ctx->obj_list, list)
		if (vbo->type == VPS_DUMPOBJT_FDSET &&
		    vbo->orig_ptr == fdp)
			break;

	if (vbo != NULL) {
		DBGS("%s: fdset %p already dumped\n", __func__, fdp);
		return (0);
	}

	DBGS("%s: dumping fdset %p\n", __func__, fdp);

	FILEDESC_XLOCK(fdp);

	vdo_create(ctx, VPS_DUMPOBJT_FDSET, M_WAITOK);

	vdfd = vdo_space(ctx, sizeof(*vdfd), M_WAITOK);

	vdfd->fd_orig_ptr = fdp;
	vdfd->fd_nfiles = fdp->fd_nfiles;
	vdfd->fd_have_cdir = (fdp->fd_cdir != NULL) ? 1 : 0;
	vdfd->fd_have_rdir = (fdp->fd_rdir != NULL) ? 1 : 0;
	vdfd->fd_have_jdir = (fdp->fd_jdir != NULL) ? 1 : 0;

	vdo_space(ctx, sizeof(vdfd->fd_entries[0]) * vdfd->fd_nfiles,
	    M_WAITOK);

	if ((error = vps_snapshot_vnode(ctx, vps, fdp->fd_cdir,
	    M_WAITOK)))
		goto out;
	if ((error = vps_snapshot_vnode(ctx, vps, fdp->fd_rdir,
	    M_WAITOK)))
		goto out;
	if ((error = vps_snapshot_vnode(ctx, vps, fdp->fd_jdir,
	    M_WAITOK)))
		goto out;

	for (i = 0; i < fdp->fd_nfiles; i++) {

		fp = fget_locked(fdp, i);
		vdfd->fd_entries[i].fp = fp;
		if (fp == NULL)
			continue;
		vdfd->fd_entries[i].flags = fdp->fd_ofiles[i].fde_flags;
		vdfd->fd_entries[i].rights = fdp->fd_ofiles[i].fde_rights;

		DBGS("%s: idx=%d fp=%p\n", __func__, i, fp);

		found = 0;
		SLIST_FOREACH(vbo, &ctx->obj_list, list)
			if (vbo->type == VPS_DUMPOBJT_FILE &&
			    vbo->orig_ptr == fp)
				found = 1;

		if (found == 1) {
			DBGS("%s: fp %p already dumped\n", __func__, fp);
			continue;
		}

		o1 = vdo_create(ctx, VPS_DUMPOBJT_FILE, M_WAITOK);

		vdf = vdo_space(ctx, sizeof(*vdf), M_WAITOK);

		vdf->orig_ptr = fp;
		vdf->flags = fdp->fd_ofiles[i].fde_flags;
		vdf->f_type = fp->f_type;
		vdf->f_flag = fp->f_flag;
		vdf->f_offset = fp->f_offset;
		vdf->f_cred = fp->f_cred;

		DBGS("%s: file=%p flags=%08x offset=%d\n",
		    __func__, fp, fp->f_flag, (int)fp->f_offset);

		vps_snapshot_ucred(ctx, vps, fp->f_cred, M_WAITOK);

		switch (fp->f_type) {
		case DTYPE_VNODE:
			/* XXX e.g. named pipes are not dtype_vnode but
			       do refer to a vnode ? */
			if (fp->f_vnode == NULL)
				break;
			if ((error = vps_snapshot_vnode(ctx, vps,
			    fp->f_vnode, M_WAITOK))) {
				ERRMSG(ctx, "%s: vps_snapshot_vnode(): "
				    "%d\n", __func__, error);
				goto out;
			}
			break;

		case DTYPE_PTS:
			if (fp->f_data == NULL)
				break;
			if ((error = vps_snapshot_pts(ctx, vps,
			    fp->f_data)))
				goto out;
			break;

		case DTYPE_SOCKET:
			if (fp->f_data == NULL)
				break;
			if ((error = vps_snapshot_socket(ctx, vps,
			    fp->f_data)))
				goto out;
			break;

		case DTYPE_PIPE:
			if (fp->f_data == NULL)
				break;
			if ((error = vps_snapshot_pipe(ctx, vps,
			    fp->f_data)))
				goto out;
			break;

		case DTYPE_KQUEUE:
			DBGS("%s: KQUEUE fp=%p f_data=%p\n",
				__func__, fp, fp->f_data);
			if (fp->f_data == NULL)
				break;
			if ((error = vps_snapshot_kqueue(ctx, vps, fp)))
				goto out;
			/* kqueue has to be restored last */
			o1->prio = -100;
			break;

		default:
			ERRMSG(ctx, "%s: unhandled dtype %d\n",
			    __func__, fp->f_type);
			error = EINVAL;
			goto out;
			break;
		}

		vdo_close(ctx);

		/* Insert into list of dumped objects. */
		vbo = malloc(sizeof(*vbo), M_TEMP, M_WAITOK);
		vbo->type = VPS_DUMPOBJT_FILE;
		vbo->orig_ptr = fp;
		vbo->new_ptr = NULL;
		SLIST_INSERT_HEAD(&ctx->obj_list, vbo, list);
	}

	/* Insert into list of dumped objects. */
	vbo = malloc(sizeof(*vbo), M_TEMP, M_WAITOK);
	vbo->type = VPS_DUMPOBJT_FDSET;
	vbo->orig_ptr = fdp;
	vbo->new_ptr = NULL;
	SLIST_INSERT_HEAD(&ctx->obj_list, vbo, list);

 out:

	vdo_close(ctx);

	FILEDESC_XUNLOCK(fdp);

	return (error);
}

VPSFUNC
static int
vps_snapshot_pargs(struct vps_snapst_ctx *ctx, struct vps *vps,
    struct proc *p)
{
	struct vps_dump_pargs *vdp;
	struct pargs *pargs;
	int error = 0;
	int len2;

	pargs = p->p_args;
	if (pargs == NULL)
		return (0);

	len2 = roundup(pargs->ar_length, 8);

	vdo_create(ctx, VPS_DUMPOBJT_PARGS, M_WAITOK);

	vdp = vdo_space(ctx, sizeof(*vdp) + len2, M_WAITOK);

	vdp->ar_length = pargs->ar_length;
	memcpy(vdp->ar_args, pargs->ar_args, pargs->ar_length);

	vdo_close(ctx);

	DBGS("%s: [%s]\n", __func__, p->p_args->ar_args);

	return (error);
}

VPSFUNC
static int
vps_snapshot_vmpages(struct vps_snapst_ctx *ctx, struct vps *vps,
    vm_object_t vmo)
{
	struct vps_dumpobj *o1;
	struct vps_dump_vmpages *vdvmp;
	struct vps_dump_vmpageref *vdvmpr;
	int npages;
	void *newarr;
	vm_pindex_t pidx;

	npages = 0;

	if ((o1 = vdo_create(ctx, VPS_DUMPOBJT_VMPAGE, M_NOWAIT)) == NULL) {
		VM_OBJECT_WUNLOCK(vmo);
		vps_ctx_extend(ctx, NULL, PAGE_SIZE, M_WAITOK);
		vdo_discard(ctx, o1);
		return (ENOMEM);
	}
	if ((vdvmp = vdo_space(ctx, sizeof(*vdvmp), M_NOWAIT)) == NULL) {
		VM_OBJECT_WUNLOCK(vmo);
		vps_ctx_extend(ctx, NULL, PAGE_SIZE, M_WAITOK);
		vdo_discard(ctx, o1);
		return (ENOMEM);
	}

	for (pidx = 0; pidx < vmo->size; pidx++) {

		/*
		 * If there is anything it this address,
		 * it is either:
		 * a) a resident page
		 * b) a swapped out page
		 * We do not care if resident pages get swapped out
		 * until vpsctl reads them, because we lookup or swap
		 * them in again by that time.
		 */

		if (vm_page_lookup(vmo, pidx) == NULL) {
			if (vmo->type==OBJT_DEFAULT ||
			    vmo->type==OBJT_VNODE ||
			    vmo->type==OBJT_PHYS)
				// not having it
				continue;
			else if (vmo->type==OBJT_SWAP &&
			    vm_pager_has_page(vmo, pidx, NULL, NULL) ==
			    FALSE)
				// not having it
				continue;
		}

		if (ctx->nuserpages == ctx->page_ref_size) {
			/* Extend array by one page. */
			newarr = malloc(ctx->nuserpages *
			    sizeof(struct vps_page_ref) + PAGE_SIZE,
			    M_VPS_SNAPST, M_NOWAIT | M_ZERO);
			if (newarr == NULL) {
				VM_OBJECT_WUNLOCK(vmo);
				vps_ctx_extend(ctx, NULL, PAGE_SIZE,
				    M_WAITOK);
				ctx->nuserpages -= npages;
				vdo_discard(ctx, o1);
				return (ENOMEM);
			}
			memcpy(newarr, ctx->page_ref, ctx->nuserpages *
				sizeof(struct vps_page_ref));
			free(ctx->page_ref, M_VPS_SNAPST);
			ctx->page_ref = newarr;
			ctx->page_ref_size = (ctx->nuserpages *
			    sizeof(struct vps_page_ref)+PAGE_SIZE) /
			    sizeof(struct vps_page_ref);
		}
		ctx->page_ref[ctx->nuserpages].obj = vmo;
		ctx->page_ref[ctx->nuserpages].pidx = pidx;
		++npages;
		++ctx->nuserpages;

		if ((vdvmpr = vdo_space(ctx, sizeof(*vdvmpr),
		    M_NOWAIT)) == NULL) {
			VM_OBJECT_WUNLOCK(vmo);
			vps_ctx_extend(ctx, NULL, PAGE_SIZE, M_WAITOK);
			vdo_discard(ctx, o1);
			ctx->nuserpages -= npages;
			return (ENOMEM);
		}
		vdvmpr->pr_vmobject = vmo;
		vdvmpr->pr_pindex = pidx;

	}

	vdvmp->count = npages;
	DBGS("%s: dumped %d pages\n", __func__, npages);

	vdo_close(ctx);

	return (0);
}

VPSFUNC
static int
vps_snapshot_vpsfs_getuppervn(struct vps_snapst_ctx *ctx, struct vps *vps,
    struct vnode **vpp)
{
	struct vnode *lvp, *uvp;
	struct mount *mp;
	int error;

	/*
	 * If using vpsfs, vmobject only has a reference to the lower
	 * vnode. So we must try to get the vpsfs alias vnode in order
	 * to get the correct path.
	 * XXX Also this only works if the corresponding vpsfs mount
	 *     is the vps instances' root.
	 */
	if (vps_func->vpsfs_tag != NULL &&
	    vps_func->vpsfs_nodeget != NULL &&
	    vps->_rootvnode->v_tag == vps_func->vpsfs_tag) {

		lvp = *vpp;
		mp = vps->_rootvnode->v_mount;

		/* Exclusive lock is required by vpsfs_nodeget(). */
		error = vn_lock(lvp, LK_EXCLUSIVE);

		if (error != 0) {
			/* Give up */
			DBGS("%s: vn_lock(%p, LK_EXCLUSIVE): %d\n",
				__func__, lvp, error);
			*vpp = lvp;
			vput(lvp);
			return (error);
		}

		if ((error = vps_func->vpsfs_nodeget(mp,
		    lvp, &uvp))) {
			/* Give up */
			DBGS("%s: lvp=%p nothing found\n",
				__func__, lvp);
			*vpp = lvp;
			vput(lvp);
			return (error);
		} else {
			DBGS("%s: lvp=%p --> uvp=%p\n",
				__func__, lvp, uvp);
			*vpp = uvp;
		}

		vn_lock(lvp, LK_RELEASE);

	}

	return (0);
}

VPSFUNC
static int
vps_snapshot_vmobject(struct vps_snapst_ctx *ctx, struct vps *vps,
    struct vm_object *vmo)
{
	struct vps_dump_vmobject *vdvmo;
	struct vm_object *dumped_backing_obj;
	struct vps_dumpobj *o1, *save_obj;
	struct vps_restore_obj *vbo;
	struct vnode *save_vp;
	struct vnode *vp;
	int found;
	char dump_pages;
	int error = 0;

	if (vmo == NULL) {
		/* Nothing to do. */
		DBGS("%s: NULL object\n", __func__);
		return (0);
	}

	/* Look if this object is already dumped (shared memory). */
	SLIST_FOREACH(vbo, &ctx->obj_list, list)
		if (vbo->type == VPS_DUMPOBJT_VMOBJECT &&
		    vbo->orig_ptr == vmo) {
			/*
			DBGS("%s: object=%p already dumped, skipping.\n",
				__func__, vmo);
			*/
			return (0);
		}

	save_obj = ctx->curobj;

	/* Try to have a good size of pre-allocated space. */
	/* XXX */
	vps_ctx_extend(ctx, NULL, PAGE_SIZE + sizeof(struct vps_page_ref)
		* vmo->resident_page_count * 2, M_WAITOK);
 again1:
	VM_OBJECT_RLOCK(vmo);
	dumped_backing_obj = vmo->backing_object;
	if (vmo->backing_object != NULL) {
		/* Look if backing object is already dumped. */
		found = 0;
		SLIST_FOREACH(vbo, &ctx->obj_list, list) {
			if (vbo->type == VPS_DUMPOBJT_VMOBJECT &&
			    vbo->orig_ptr == vmo->backing_object) {
				found = 1;
				break;
			}
		}
		if (found == 0) {
			/* Dump backing object first. */
			DBGS("%s: obj=%p has backing_object=%p, dumping "
			    "this first.\n",
			    __func__, vmo, vmo->backing_object);
			VM_OBJECT_RUNLOCK(vmo);
			if ((error = vps_snapshot_vmobject(ctx, vps,
			    dumped_backing_obj)))
				return (error);
			VM_OBJECT_RLOCK(vmo);
		}
	}
	VM_OBJECT_RUNLOCK(vmo);

 again2:
	/*
	if (error == ENOMEM)
		vps_ctx_extend(ctx, NULL, PAGE_SIZE, M_WAITOK);
	*/
	error = 0;
	VM_OBJECT_WLOCK(vmo);

	if (vmo->backing_object != dumped_backing_obj) {
		VM_OBJECT_WUNLOCK(vmo);
		DBGS("%s: jumping to again1 because backing object "
		    "changed\n", __func__);
		vdo_discard(ctx, save_obj->next);
		goto again1;
	}

	if ((o1 = vdo_create(ctx, VPS_DUMPOBJT_VMOBJECT, M_NOWAIT)) ==
	    NULL) {
		VM_OBJECT_WUNLOCK(vmo);
		error = ENOMEM;
		vps_ctx_extend(ctx, NULL, PAGE_SIZE, M_WAITOK);
		goto again2;
	}

	/*
	DBGS("%s: obj=%p: size=%d ref_count=%d shadow_count=%d flags=%04x "
	    "type=%02x handle=%p backing_object=%p\n",
	    __func__, o, (int)o->size, o->ref_count,o->shadow_count,
	    o->flags, o->type, o->handle, o->backing_object);
	*/

	if ((vdvmo = vdo_space(ctx, sizeof(*vdvmo), M_NOWAIT)) == NULL) {
		VM_OBJECT_WUNLOCK(vmo);
		vps_ctx_extend(ctx, NULL, PAGE_SIZE, M_WAITOK);
		vdo_discard(ctx, o1);
		error = ENOMEM;
		goto again2;
	}

	vdvmo->orig_ptr = vmo;
	vdvmo->cred = vmo->cred;
	vdvmo->backing_object = vmo->backing_object;

	vdvmo->flags = vmo->flags;
	vdvmo->type = vmo->type;
	vdvmo->have_vnode = 0;
	vdvmo->is_sharedpageobj = 0;

	vdvmo->size = vmo->size;
	vdvmo->charge = vmo->charge;
	vdvmo->backing_object_offset = vmo->backing_object_offset;

	switch (vmo->type) {
		case OBJT_DEAD:
			DBGS("%s: obj=%p OBJT_DEAD\n", __func__, vmo);
			dump_pages = 0;
			break;

		case OBJT_VNODE:
			vp = vmo->handle;
			vref(vp);
			save_vp = vp;

			VM_OBJECT_WUNLOCK(vmo);
#if 1
			error = vps_snapshot_vpsfs_getuppervn(ctx, vps,
			    &vp);
			if (error != 0) {
				vdo_discard(ctx, o1);
				vrele(vp);
				goto out;
			}
#endif
			error = vps_snapshot_vnode(ctx, vps, vp,
			    M_WAITOK);
			if (error != 0) {
				vdo_discard(ctx, o1);
				vrele(vp);
				goto out;
			}

			VM_OBJECT_WLOCK(vmo);
			vrele(vp);

			/* XXX Why ?
			if (error != 0 && vmo->handle != save_vp) {
			*/
			if (vmo->handle != save_vp) {
				/* Object changed while it was unlocked. */
				VM_OBJECT_WUNLOCK(vmo);
				vdo_discard(ctx, o1);
				goto out;
			}

			dump_pages = (vmo->flags & OBJ_MIGHTBEDIRTY) ?
			    1 : 0;
			vdvmo->have_vnode = 1;

			break;

		case OBJT_PHYS:
			DBGS("%s: obj=%p OBJT_PHYS %s\n", __func__, vmo,
			    (vmo == shared_page_obj) ? "shared_page_obj" :
			    "");
			if (vmo == shared_page_obj) {
				dump_pages = 0;
				vdvmo->is_sharedpageobj = 1;
			} else {
				dump_pages = 1;
			}
			break;

		case OBJT_DEFAULT:
		case OBJT_SWAP:
			dump_pages = 1;
			break;

		/*
		case OBJT_DEVICE:
			break;
		*/

		default:
			panic("%s: unsupported object type obj=%p "
			    "type=%d\n", __func__, vmo, vmo->type);
			break;
	}

	if (vmo->cred != NULL) {
		error = vps_snapshot_ucred(ctx, vps, vmo->cred, M_NOWAIT);
		if (error == ENOMEM) {
			VM_OBJECT_WUNLOCK(vmo);
			vps_ctx_extend(ctx, NULL, PAGE_SIZE, M_WAITOK);
			vdo_discard(ctx, o1);
			goto again2;
		} else if (error != 0) {
			VM_OBJECT_WUNLOCK(vmo);
			vdo_discard(ctx, o1);
			goto out;
		}
	}

	/* Dump pages. */
	if (dump_pages) {
		error = vps_snapshot_vmpages(ctx, vps, vmo);
		if (error != 0) {
			VM_OBJECT_WUNLOCK(vmo);
			vdo_discard(ctx, o1);
			goto out;
		}
	}

	vm_object_reference_locked(vmo);
	VM_OBJECT_WUNLOCK(vmo);

	/* Insert in any case. */
	if (1) {
		/* Insert into list of dumped backing objects. */
		vbo = malloc(sizeof(*vbo), M_TEMP, M_WAITOK);
		vbo->type = VPS_DUMPOBJT_VMOBJECT;
		vbo->orig_ptr = vmo;
		vbo->new_ptr = NULL;
		SLIST_INSERT_HEAD(&ctx->obj_list, vbo, list);
	}

	vdo_close(ctx);

 out:

	return (error);
}

VPSFUNC
static int
vps_snapshot_vmspace(struct vps_snapst_ctx *ctx, struct vps *vps,
    struct vmspace *vmspace)
{
	struct vps_restore_obj *vbo;
	struct vps_dump_vmspace *vdvms;
	struct vps_dump_vmmapentry *vdvme;
	struct vm_map_entry *e;
	int i;
	int error = 0;

	SLIST_FOREACH(vbo, &ctx->obj_list, list)
		if (vbo->type == VPS_DUMPOBJT_VMSPACE &&
		    vbo->orig_ptr == vmspace)
			break;

	if (vbo != NULL) {
		DBGS("%s: vmspace %p already dumped\n",
		    __func__, vmspace);
		return (0);
	}

	DBGS("%s: vmspace=%p map=%p\n",
	    __func__, vmspace, &vmspace->vm_map);

	vdo_create(ctx, VPS_DUMPOBJT_VMSPACE, M_WAITOK);

	vdvms = vdo_space(ctx, sizeof(*vdvms), M_WAITOK);

	vdvms->vm_orig_ptr = vmspace;
	vdvms->vm_tsize = vmspace->vm_tsize;
	vdvms->vm_dsize = vmspace->vm_dsize;
	vdvms->vm_ssize = vmspace->vm_ssize;
	vdvms->vm_map.minoffset = vmspace->vm_map.min_offset;
	vdvms->vm_map.maxoffset = vmspace->vm_map.max_offset;

	vm_map_lock(&vmspace->vm_map);

	for (i = 0, e = vmspace->vm_map.header.next;
		e != &vmspace->vm_map.header;
		i++, e = e->next) {

		DBGS("%s: entry=%p: start=%016zx end=%016zx prot=%02x "
		    "max_prot=%02x " "object=%p eflags=%08x (%s)\n",
		    __func__, e, (size_t)e->start, (size_t)e->end,
		    e->protection, e->max_protection,
		    e->object.vm_object, e->eflags,
		    e->eflags & MAP_ENTRY_IS_SUB_MAP ? "submap" :
		    "vm object");

		vdo_create(ctx, VPS_DUMPOBJT_VMMAPENTRY, M_WAITOK);
		vdvme = vdo_space(ctx, sizeof(*vdvme), M_WAITOK);

		vdvme->map_object = e->object.vm_object;
		vdvme->offset = e->offset;
		vdvme->start = e->start;
		vdvme->end = e->end;
		vdvme->avail_ssize = e->avail_ssize;
		vdvme->eflags = e->eflags;
		vdvme->protection = e->protection;
		vdvme->max_protection = e->max_protection;
		vdvme->inheritance = e->inheritance;

		if (e->cred != NULL)
			vps_snapshot_ucred(ctx, vps, e->cred, M_WAITOK);

		vdvme->cred = e->cred;

		if (e->eflags & MAP_ENTRY_IS_SUB_MAP) {
			ERRMSG(ctx, "%s: WARNING: skipping submap\n",
			    __func__);
		} else
			if ((error = vps_snapshot_vmobject(ctx, vps,
			    e->object.vm_object)))
				goto out;

		vdo_close(ctx);
	}

	/* Insert into list of dumped objects. */
	vbo = malloc(sizeof(*vbo), M_TEMP, M_WAITOK);
	vbo->type = VPS_DUMPOBJT_VMSPACE;
	vbo->orig_ptr = vmspace;
	vbo->new_ptr = NULL;
	SLIST_INSERT_HEAD(&ctx->obj_list, vbo, list);

 out:
	vm_map_unlock(&vmspace->vm_map);

	vdo_close(ctx);

	return (error);
}

VPSFUNC
__attribute__((unused))
static int
vps_snapshot_umtx(struct vps_snapst_ctx *ctx, struct vps *vps,
    struct thread *td)
{
	/*struct umtx_q *uq;*/
	int error = 0;

	vdo_create(ctx, VPS_DUMPOBJT_UMTX, M_WAITOK);

	vps_umtx_snapshot(td);

	/*
	DBGS("%s: td->td_umtxq=%p\n", __func__, td->td_umtxq);
	if (td->td_umtxq == NULL) {
		error = 0;
		goto out;
	}

	mtx_lock_spin(&umtx_lock);
	mtx_unlock_spin(&umtx_lock);
	*/

 /*out:*/
	vdo_close(ctx);

	return (error);
}

VPSFUNC
static int
vps_snapshot_thread_savefpu(struct vps_snapst_ctx *ctx, struct vps *vps,
    struct thread *td)
{

	return (vps_md_snapshot_thread_savefpu(ctx, vps, td));
}

VPSFUNC
static int
vps_snapshot_thread(struct vps_snapst_ctx *ctx, struct vps *vps,
    struct thread *td)
{
	struct vps_dumpobj *o1;
	struct vps_dump_thread *vdtd;
	int error = 0;
	int i;

	/* XXX Has to happen in suspend, because filesystem sync
	   is finished. */
	/* Drain pending ktrace records. */
	ktruserret(td);

	/* pre-alloc some space to avoid re-doing ... */
	vps_ctx_extend(ctx, NULL, td->td_kstack_pages << PAGE_SHIFT,
	    M_WAITOK);

  again:
	o1 = vdo_create(ctx, VPS_DUMPOBJT_THREAD, M_WAITOK);

	thread_lock(td);

	/* Make sure the thread is stopped in ast(). <-- XXX */
	if ( ! (TD_IS_SUSPENDED(td)) ) {
		ERRMSG(ctx, "%s: td=%p not suspended !\n", __func__, td);
		error = EBUSY;
		thread_unlock(td);
		vdo_discard(ctx, o1);
		goto out;
	}

	if ((vdtd = vdo_space(ctx, sizeof(*vdtd), M_NOWAIT)) == NULL) {
		thread_unlock(td);
		vdo_discard(ctx, o1);
		goto again;
	}

	vdtd->td_tid = td->td_tid;
	vdtd->td_xsig = td->td_xsig;
	vdtd->td_dbgflags = td->td_dbgflags;
	vdtd->td_sigstk.ss_sp = PTRTO64(td->td_sigstk.ss_sp);
	vdtd->td_sigstk.ss_size = td->td_sigstk.ss_size;
	vdtd->td_sigstk.ss_flags = td->td_sigstk.ss_flags;
	for (i = 0; i < _SIG_WORDS; i++) {
		vdtd->td_sigmask[i] = td->td_sigmask.__bits[i];
		vdtd->td_oldsigmask[i] = td->td_oldsigmask.__bits[i];
	}

	vdtd->td_rqindex = td->td_rqindex;
	vdtd->td_base_pri = td->td_base_pri;
	vdtd->td_priority = td->td_priority;
	vdtd->td_pri_class = td->td_pri_class;
	vdtd->td_user_pri = td->td_user_pri;
	vdtd->td_base_user_pri = td->td_base_user_pri;

	vdtd->td_errno = td->td_errno;
	vdtd->td_retval[0] = td->td_retval[0];
	vdtd->td_retval[1] = td->td_retval[1];

	vdtd->td_kstack_pages = td->td_kstack_pages;

	/* The kstack includes the PCB. */
	if (vdo_append(ctx, (void *)td->td_kstack,
			td->td_kstack_pages << PAGE_SHIFT, M_NOWAIT)) {
		thread_unlock(td);
		vdo_discard(ctx, o1);
		goto again;
	}

	if (vps_md_snapshot_thread(vdtd, td) != 0) {
		thread_unlock(td);
		vdo_discard(ctx, o1);
		goto again;
	}

	if (vps_snapshot_thread_savefpu(ctx, vps, td) != 0) {
		thread_unlock(td);
		vdo_discard(ctx, o1);
		goto again;
	}

	thread_unlock(td);

	vps_md_print_thread(td);
#ifdef DDB
	db_trace_thread(td, 16);
#endif

	/* not yet
	if ((error = vps_snapshot_umtx(ctx, vps, td)))
		goto out;
	*/

 out:
	vdo_close(ctx);

	return (error);
}

VPSFUNC
static int
vps_snapshot_proc_one(struct vps_snapst_ctx *ctx, struct vps *vps,
    struct proc *p)
{
	struct thread *td;
	struct vps_dump_proc *vdp;
	int error = 0;
	int i;

	DBGS("%s: p=%p pid=%d ctx->cpos=%p delta=%p\n",
	    __func__, p, p->p_pid, ctx->cpos,
	    (void *)((caddr_t)ctx->cpos - (caddr_t)ctx->data));

	vdo_create(ctx, VPS_DUMPOBJT_PROC, M_WAITOK);

	PROC_LOCK(p);
	KASSERT(p->p_state == PRS_NORMAL,
	    ("%s: p=%p p_state=%d\n", __func__, p, p->p_state));
	PROC_UNLOCK(p);

	//PROC_LOCK(p);

	/* Dump structures. */

	vdp = vdo_space(ctx, sizeof(*vdp), M_WAITOK);
	memset(vdp, 0, sizeof(*vdp));

	/* sigacts */
	KASSERT(_SIG_MAXSIG <= (sizeof(vdp->p_sigacts.ps_sigact) /
	    sizeof(vdp->p_sigacts.ps_sigact[0])),
	    ("%s: vdp->p_sigacts.ps_sigact too small\n",
	    __func__));
	KASSERT(_SIG_WORDS <= (sizeof(vdp->p_sigacts.ps_sigonstack) /
	    sizeof(vdp->p_sigacts.ps_sigonstack[0])),
	    ("%s: vdp->p_sigacts.ps_sigonstack etc. too small\n",
	    __func__));

	vdp->p_sigacts.ps_maxsig = _SIG_MAXSIG;
	vdp->p_sigacts.ps_sigwords = _SIG_WORDS;

	vdp->p_sigacts.ps_flag = p->p_sigacts->ps_flag;
	for (i = 0; i < _SIG_MAXSIG; i++) {
		vdp->p_sigacts.ps_sigact[i] =
		    PTRTO64(p->p_sigacts->ps_sigact[i]);
		vdp->p_sigacts.ps_catchmask[i][0] =
		    p->p_sigacts->ps_catchmask[i].__bits[0];
		vdp->p_sigacts.ps_catchmask[i][1] =
		    p->p_sigacts->ps_catchmask[i].__bits[1];
		vdp->p_sigacts.ps_catchmask[i][2] =
		   p->p_sigacts->ps_catchmask[i].__bits[2];
		vdp->p_sigacts.ps_catchmask[i][3] =
		   p->p_sigacts->ps_catchmask[i].__bits[3];
	}
	for (i = 0; i < _SIG_WORDS; i++) {
		vdp->p_sigacts.ps_sigonstack[i] =
		    p->p_sigacts->ps_sigonstack.__bits[i];
		vdp->p_sigacts.ps_sigintr[i] =
		    p->p_sigacts->ps_sigintr.__bits[i];
		vdp->p_sigacts.ps_sigreset[i] =
		    p->p_sigacts->ps_sigreset.__bits[i];
		vdp->p_sigacts.ps_signodefer[i] =
		    p->p_sigacts->ps_signodefer.__bits[i];
		vdp->p_sigacts.ps_siginfo[i] =
		    p->p_sigacts->ps_siginfo.__bits[i];
		vdp->p_sigacts.ps_sigignore[i] =
		    p->p_sigacts->ps_sigignore.__bits[i];
		vdp->p_sigacts.ps_sigcatch[i] =
		    p->p_sigacts->ps_sigcatch.__bits[i];
		vdp->p_sigacts.ps_freebsd4[i] =
		    p->p_sigacts->ps_freebsd4.__bits[i];
		vdp->p_sigacts.ps_osigset[i] =
		    p->p_sigacts->ps_osigset.__bits[i];
		vdp->p_sigacts.ps_usertramp[i] =
		    p->p_sigacts->ps_usertramp.__bits[i];
	}

	/* plimit */
	/* XXX this is COW ... */

	KASSERT(RLIM_NLIMITS < (sizeof(vdp->p_limit.pl_rlimit) /
		sizeof(vdp->p_limit.pl_rlimit[0])),
		("%s: vdp->p_limit.pl_rlimit too small\n",
		__func__));

	vdp->p_limit.pl_nlimits = RLIM_NLIMITS;

	for (i = 0; i < RLIM_NLIMITS; i++) {
		vdp->p_limit.pl_rlimit[i].rlim_cur =
		    p->p_limit->pl_rlimit[i].rlim_cur;
		vdp->p_limit.pl_rlimit[i].rlim_max =
		    p->p_limit->pl_rlimit[i].rlim_max;
	}

	/* proc */
	vdp->p_pptr_id = (p->p_pptr) ? p->p_pptr->p_pid : 0;
	vdp->p_peers_id = (p->p_peers) ? p->p_peers->p_pid : 0;
	vdp->p_leader_id = (p->p_leader) ? p->p_leader->p_pid : 0;
	vdp->p_pgrp_id = (p->p_pgrp) ? p->p_pgrp->pg_id : 0;
	vdp->p_xthread_id = (p->p_xthread) ? p->p_xthread->td_tid : 0;

	vdp->p_pid = p->p_pid;
	vdp->p_swtick = p->p_swtick;
	vdp->p_cpulimit = p->p_cpulimit;
	vdp->p_flag = p->p_flag;
	vdp->p_state = p->p_state;
	vdp->p_stops = p->p_stops;
	vdp->p_oppid = p->p_oppid;
	vdp->p_xstat = p->p_xstat;
	vdp->p_stype = p->p_stype;
	vdp->p_step = p->p_step;
	vdp->p_sigparent = p->p_sigparent;
	vdp->p_ucred = p->p_ucred;
	vdp->p_tracecred = p->p_tracecred;
	vdp->p_traceflag = p->p_traceflag;
	vdp->p_vmspace = p->p_vmspace;
	vdp->p_fd = p->p_fd;
	strlcpy(vdp->p_comm, p->p_comm, min(sizeof(vdp->p_comm),
	    sizeof(p->p_comm)));

	/* credentials */
	vps_snapshot_ucred(ctx, vps, p->p_ucred, M_WAITOK);

	/* sysentvec */
	if ((error = vps_snapshot_sysentvec(ctx, vps, p)))
		goto out;

	/* ktrace */
	if (p->p_tracevp != NULL) {
		if ((error = vps_snapshot_vnode(ctx, vps, p->p_tracevp,
		    M_WAITOK)))
			goto out;
		vdp->p_have_tracevp = 1;
	}

	/* Executable vnode. */
	if (p->p_textvp != NULL) {
		if ((error = vps_snapshot_vnode(ctx, vps, p->p_textvp,
		    M_WAITOK)))
			goto out;
		vdp->p_have_textvp = 1;
	}

	/* Dump vmspace. */
	if ((error = vps_snapshot_vmspace(ctx, vps, p->p_vmspace)))
		goto out;

	/* Dump threads. */
	TAILQ_FOREACH(td, &p->p_threads, td_plist)
		if ((error = vps_snapshot_thread(ctx, vps, td)))
			goto out;

	/* Dump file set. */
	if ((error = vps_snapshot_fdset(ctx, vps, p)))
		goto out;

	/* Dump process argument list. */
	if ((error = vps_snapshot_pargs(ctx, vps, p)))
		goto out;

	if (vps_func->sem_snapshot_proc &&
	    (error = vps_func->sem_snapshot_proc(ctx, vps, p)))
		goto out;

	if (vps_func->shm_snapshot_proc &&
	    (error = vps_func->shm_snapshot_proc(ctx, vps, p)))
		goto out;

	if (vps_func->msg_snapshot_proc &&
	    (error = vps_func->msg_snapshot_proc(ctx, vps, p)))
		goto out;

  out:
	//PROC_UNLOCK(p);

	vdo_close(ctx);

	return (error);
}

/* * * * * * * * * * * * * * * * * * */

VPSFUNC
int
vps_snapshot_finish(struct vps_dev_ctx *dev_ctx, struct vps *vps)
{
	struct vps_snapst_ctx *ctx;

	ctx = dev_ctx->snapst;

	DBGS("%s: dev_ctx=%p\n", __func__, dev_ctx);

	if (ctx == NULL)
		return (0);

	if (vps == NULL)
	   vps = ctx->vps;

	if (vps != NULL && vps->vps_status != VPS_ST_SNAPSHOOTING )
		return (EINVAL);

	ctx->cmd = 0;
	ctx->vps = NULL;

	if (ctx->vps_vmobject) {
	        (void)vm_object_reference(ctx->vps_vmobject);
		/* Frees one reference. */
		(void)vm_map_remove(ctx->user_map, ctx->user_map_start,
			ctx->user_map_start +
			((ctx->nsyspages + ctx->nuserpages)
			<< PAGE_SHIFT));

		VM_OBJECT_WLOCK(ctx->vps_vmobject);
		vps_pager_ops.pgo_dealloc(ctx->vps_vmobject);
		VM_OBJECT_WUNLOCK(ctx->vps_vmobject);
		ctx->vps_vmobject = NULL;
	}

	while ( ! SLIST_EMPTY(&ctx->obj_list)) {
		struct vps_restore_obj *vbo;

		vbo = SLIST_FIRST(&ctx->obj_list);
		SLIST_REMOVE_HEAD(&ctx->obj_list, list);

		switch (vbo->type) {
		case VPS_DUMPOBJT_VMOBJECT:
			vm_object_deallocate(vbo->orig_ptr);
			break;
		case VPS_DUMPOBJT_UCRED:
			crfree(vbo->orig_ptr);
			break;
		default:
			break;
		}
		free(vbo, M_TEMP);
	}

	/*
	 * Clean up the memory mess.
	 */
	if (ctx->userpagelist)
		free(ctx->userpagelist, M_VPS_SNAPST);

	if (ctx->page_ref)
		free(ctx->page_ref, M_VPS_SNAPST);

	if (ctx->data)
		vps_ctx_free(ctx, vps);

	if (vps != NULL) {
		vps->vps_status = VPS_ST_SUSPENDED;
		/* XXX unlock vps */
	}

	free(dev_ctx->snapst, M_VPS_SNAPST);
	dev_ctx->snapst = NULL;
	dev_ctx->cmd = 0;

	DBGS("%s: finished snapshot\n", __func__);

	return (0);
}

static int
vps_snapst_modevent(module_t mod, int type, void *data)
{
	int error;

	error = 0;

	switch (type) {
	case MOD_LOAD:
	   vps_snapst_mod_refcnt = 0;
	   vps_func->vps_snapshot = vps_snapshot;
	   vps_func->vps_snapshot_finish = vps_snapshot_finish;
	   vps_func->vps_ctx_extend_hard = vps_ctx_extend_hard;
	   vps_func->vps_snapshot_ucred = vps_snapshot_ucred;
	   break;
	case MOD_UNLOAD:
	   if (vps_snapst_mod_refcnt > 0)
		return (EBUSY);
	   vps_func->vps_snapshot = NULL;
	   vps_func->vps_snapshot_finish = NULL;
	   vps_func->vps_ctx_extend_hard = NULL;
	   vps_func->vps_snapshot_ucred = NULL;
	   break;
	default:
	   error = EOPNOTSUPP;
	   break;
	}
	return (error);
}

static moduledata_t vps_snapst_mod = {
	"vps_snapst",
	vps_snapst_modevent,
	0
};

DECLARE_MODULE(vps_snapst, vps_snapst_mod, SI_SUB_PSEUDO, SI_ORDER_ANY);

#endif /* VPS */

/* EOF */
