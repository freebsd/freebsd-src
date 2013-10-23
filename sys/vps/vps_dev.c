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
    "$Id: vps_dev.c 153 2013-06-03 16:18:17Z klaus $";

#include <sys/cdefs.h>

#include "opt_ddb.h"
#include "opt_global.h"

#ifdef VPS

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
#include <sys/mman.h>
#include <sys/jail.h>

#include <vm/pmap.h>

#include <net/if.h>
#include <netinet/in.h>

#include "vps_user.h"
#include "vps_int.h"
#include "vps.h"
#include "vps2.h"

#ifdef DIAGNOSTIC

#define DBGDEV	if (debug_dev) printf

static int debug_dev = 0;
SYSCTL_INT(_debug, OID_AUTO, vps_dev_debug, CTLFLAG_RW, &debug_dev, 0, "");

#else

#define DBGDEV(x, ...)

#endif /* DIAGNOSTIC */

static int		vps_dev_refcnt = 0;
static caddr_t		vps_dev_emptypage;
static struct cdev	*vps_dev_p;
static d_fdopen_t	vps_dev_fdopen;
static d_open_t		vps_dev_open;
static d_close_t	vps_dev_close;
static d_ioctl_t	vps_dev_ioctl;
static d_mmap_t		vps_dev_mmap;

static struct cdevsw vps_dev_cdevsw = {
	.d_version =	D_VERSION,
	.d_name =	"vps control device",
	.d_fdopen =	vps_dev_fdopen,
	.d_open =	vps_dev_open,
	.d_close =	vps_dev_close,
	.d_ioctl =	vps_dev_ioctl,
	.d_mmap =	vps_dev_mmap,
	.d_flags =	D_TRACKCLOSE,
};

LIST_HEAD(vps_dev_ctx_le, vps_dev_ctx) vps_dev_ctx_head;

MALLOC_DEFINE(M_VPS_DEV, "vps_dev",
    "Virtual Private Systems Device memory");

/* ----------------------- */

static int
vps_dev_attach(void)
{

	vps_dev_emptypage = malloc(PAGE_SIZE, M_VPS_DEV, M_WAITOK | M_ZERO);

	vps_dev_p = make_dev(&vps_dev_cdevsw, 123, UID_ROOT,
	    GID_WHEEL, 0600, "vps");

	LIST_INIT(&vps_dev_ctx_head);

	DBGDEV("%s: init done\n", __func__);

	return (0);
}

static int
vps_dev_detach(void)
{

	if (vps_dev_refcnt > 0)
		return (EBUSY);

	destroy_dev(vps_dev_p);

	free(vps_dev_emptypage, M_VPS_DEV);

	DBGDEV("%s: cleanup done\n", __func__);

	return (0);
}

static int
vps_dev_modevent(module_t mod, int type, void *data)
{
	int error;

	switch (type) {
	case MOD_LOAD:
		error = vps_dev_attach();
		break;
	case MOD_UNLOAD:
		error = vps_dev_detach();
		break;
	default:
		error = EOPNOTSUPP;
		break;
	}
	return (error);
}

static moduledata_t vps_dev_mod = {
	"vps_dev",
	vps_dev_modevent,
	0
};

DECLARE_MODULE(vps_dev, vps_dev_mod, SI_SUB_PSEUDO, SI_ORDER_ANY);

static struct vps_dev_ctx *
vps_dev_get_ctx(struct thread *td)
{
	struct vps_dev_ctx *ctx;

	if (jailed(td->td_ucred)) {
		DBGCORE("%s: td is jailed --> denying any vps-device "
		    "action !\n", __func__);
		return (NULL);
	}

	LIST_FOREACH(ctx, &vps_dev_ctx_head, list)
		if (ctx->fp == td->td_fpop) {
			DBGDEV("%s: td->td_fpop=%p ctx=%p\n",
				__func__, td->td_fpop, ctx);
			return (ctx);
		}

	DBGDEV("%s: ######## dev_ctx not found for td=%p td->td_fpop=%p "
	    "pid=%d\n", __func__, td, td->td_fpop, td->td_proc->p_pid);

	return (NULL);
}

static int
vps_dev_fdopen(struct cdev *dev, int fflags, struct thread *td,
    struct file *fp)
{
	struct vps_dev_ctx *ctx;

	if (jailed(td->td_ucred)) {
		DBGDEV("%s: td is jailed --> denying any vps-device "
		    "action !\n", __func__);
		return (EPERM);
	}

	atomic_add_int(&vps_dev_refcnt, 1);

	ctx = malloc(sizeof(*ctx), M_VPS_DEV, M_WAITOK | M_ZERO);
	ctx->fp = fp;

	LIST_INSERT_HEAD(&vps_dev_ctx_head, ctx, list);

	DBGDEV("%s: done refcnt=%d ctx=%p\n",
	    __func__, vps_dev_refcnt, ctx);

	return (0);
}

static int
vps_dev_open(struct cdev *dev, int flags, int fmt, struct thread *td)
{

	return (EOPNOTSUPP);
}

static int
vps_dev_close(struct cdev *dev, int flags, int fmt, struct thread *td)
{
	struct vps_dev_ctx *ctx;

	ctx = vps_dev_get_ctx(td);

	DBGDEV("%s: ctx=%p\n", __func__, ctx);

	if (ctx == NULL)
		return (0);

	LIST_REMOVE(ctx, list);

	if (ctx->snapst && vps_func->vps_snapshot_finish)
		vps_func->vps_snapshot_finish(ctx, NULL);

	if (ctx->data)
		free(ctx->data, M_VPS_DEV);

	free(ctx, M_VPS_DEV);

	atomic_subtract_int(&vps_dev_refcnt, 1);

	DBGDEV("%s: done refcnt=%d ctx=%p\n",
	    __func__, vps_dev_refcnt, ctx);

	return (0);
}

static int
vps_dev_ioctl(struct cdev *dev, u_long cmd, caddr_t data,
        int flags, struct thread *td)
{
	struct vps *vps;
	struct vps_dev_ctx *ctx;
	int error;

	ctx = vps_dev_get_ctx(td);
	if (ctx == NULL)
		return (EBADF);

	/* Needed for conext lookup in mmap pager function. */
	ctx->td = td;

	error = 0;
	vps = TD_TO_VPS(td);

	KASSERT(vps != NULL, ("%s: vps == NULL\n", __func__));

	DBGDEV("%s: td=%p ctx=%p cmd=0x%08lx\n",
	   __func__, td, ctx, cmd);

	switch (cmd) {
	case VPS_IOC_LIST:
		error = vps_ioc_list(vps, ctx, cmd, data, flags, td);
		break;
	case VPS_IOC_CREAT:
		error = vps_ioc_create(vps, ctx, cmd, data, flags, td);
		break;
	case VPS_IOC_DESTR:
		error = vps_ioc_destroy(vps, ctx, cmd, data, flags, td);
		break;
	case VPS_IOC_SWITCH:
		error = vps_ioc_switch(vps, ctx, cmd, data, flags, td);
		break;
	case VPS_IOC_SWITWT:
		error = vps_ioc_switchwait(vps, ctx, cmd, data, flags, td);
		break;
	case VPS_IOC_IFMOVE:
		error = vps_ioc_ifmove(vps, ctx, cmd, data, flags, td);
		break;
	case VPS_IOC_SUSPND:
		error = vps_ioc_suspend(vps, ctx, cmd, data, flags, td);
		break;
	case VPS_IOC_RESUME:
		error = vps_ioc_resume(vps, ctx, cmd, data, flags, td);
		break;
	case VPS_IOC_ABORT:
		error = vps_ioc_abort(vps, ctx, cmd, data, flags, td);
		break;
	case VPS_IOC_SNAPST:
		error = vps_ioc_snapshot(vps, ctx, cmd, data, flags, td);
		break;
	case VPS_IOC_SNAPSTFIN:
		error = vps_ioc_snapshot_finish(vps, ctx, cmd, data, flags,
		    td);
		break;
	case VPS_IOC_RESTOR:
		error = vps_ioc_restore(vps, ctx, cmd, data, flags, td);
		break;
	case VPS_IOC_ARGGET:
		error = vps_ioc_argget(vps, ctx, cmd, data, flags, td);
		break;
	case VPS_IOC_ARGSET:
		error = vps_ioc_argset(vps, ctx, cmd, data, flags, td);
		break;
	case VPS_IOC_GETXINFO:
		error = vps_ioc_getextinfo(vps, ctx, cmd, data, flags, td);
		break;
	case VPS_IOC_FSCALCPATH:
		error = vps_ioc_fscalcpath(vps, ctx, cmd, data, flags, td);
		break;
	case VPS_IOC_FSCALC:
		error = vps_ioc_fscalc(vps, ctx, cmd, data, flags, td);
		break;
	case VPS_IOC_GETCONSFD:
		error = vps_ioc_getconsfd(vps, ctx, cmd, data, flags, td);
		break;
	default:
		error = EOPNOTSUPP;
		break;
	}

	return (error);
}

VPSFUNC
static int
vps_dev_mmap(struct cdev *dev, vm_ooffset_t offset,
        vm_paddr_t *paddr, int nprot, vm_memattr_t *memattr)
{
	struct vps_dev_ctx *ctx, *ctx2;
	struct vps *vps;
	int error;

	ctx = NULL;
	LIST_FOREACH(ctx2, &vps_dev_ctx_head, list)
		if (ctx2->td == curthread) {
			ctx = ctx2;
			break;
		}

	if (ctx == NULL) {
		/*
		 * Returning error from this function leads to panic.
		 * Better return an empty page than let the
		 * user cause a kernel panic.
		 */
		DBGDEV("%s: ctx == NULL !\n", __func__);
		goto invalid;
	}

	if (ctx->data == NULL && ctx->cmd != VPS_IOC_SNAPST) {
		DBGDEV("%s: ctx->data == NULL !\n", __func__);
		goto invalid;
	}

	error = 0;
	vps = TD_TO_VPS(curthread);

	if (offset < 0) {
		DBGDEV("%s: offset=%zu < 0\n",
		    __func__, (size_t)offset);
		goto invalid;
	}

	if (nprot != PROT_READ) {
		DBGDEV("%s: nprot=%d != PROT_READ\n",
		    __func__, nprot);
		goto invalid;
	}

	if (1) {
		/* VPS_IOC_LIST */

		if (offset > ctx->length) {
			DBGDEV("%s: offset=%zu > ctx->length=%zu\n",
			    __func__, (size_t)offset, ctx->length);
			goto invalid;
		}

		*paddr = (vm_paddr_t)vtophys(ctx->data + offset);
	}

	return (0);

  invalid:
	*paddr = (vm_paddr_t)vtophys(vps_dev_emptypage);

	return (0);
}

#endif /* VPS */

/* EOF */
