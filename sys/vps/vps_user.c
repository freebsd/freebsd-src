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
    "$Id: vps_user.c 155 2013-06-04 09:15:51Z klaus $";

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
#include <sys/jail.h>

#include <net/if.h>
#include <netinet/in.h>

#include "vps_user.h"
#include "vps_account.h"
#include "vps.h"
#include "vps2.h"

/*
#define INCLUDE_CURVPS 1
*/

#ifdef DIAGNOSTIC

#define DBGUSER	if (debug_user) printf

static int debug_user = 1;

SYSCTL_INT(_debug, OID_AUTO, vps_user_debug, CTLFLAG_RW,
    &debug_user, 0, "");

#else

#define DBGUSER(x, ...)

#endif /* DIAGNOSTIC */

MALLOC_DECLARE(M_VPS_DEV);

int
vps_ioc_list(struct vps *vps, struct vps_dev_ctx *ctx, u_long cmd,
    caddr_t data, int flags, struct thread *td)
{
	struct vps *vps2;
	struct vps_info *info;
	int vpscnt;

	sx_slock(&vps_all_lock);

	vpscnt = 0;

#ifdef INCLUDE_CURVPS
	/* Include own vps. */
	++vpscnt;
#endif
	/* Calculate data size to allocate. */
	LIST_FOREACH(vps2, &vps->vps_child_head, vps_sibling)
		++vpscnt;

	ctx->length = sizeof(*info) * vpscnt;
	/*
	 * Always round up to pages, otherwise mmap produces rubbish.
	 */
	ctx->length = ((ctx->length >> PAGE_SHIFT) + 1 ) << PAGE_SHIFT;
	ctx->data = malloc(ctx->length, M_VPS_DEV, M_WAITOK | M_ZERO);

	info = (struct vps_info *)ctx->data;

#ifdef INCLUDE_CURVPS
	if (1) {
		/* Include own vps. */
		vps2 = vps;
		strncpy(info->name, vps2->vps_name, sizeof(info->name));
		strncpy(info->fsroot, vps2->_rootpath,
		    sizeof(info->fsroot));
		info->status = vps2->vps_status;
		info->nprocs = VPS_VPS(vps2, nprocs);
		info->nsocks = vps2->vnet->vnet_sockcnt;
		info->nifaces = vps2->vnet->vnet_ifcnt;
		info->restore_count = vps2->restore_count;
		info->acc.virt = vps2->vps_acc->virt.cur;
		info->acc.phys = vps2->vps_acc->phys.cur;
		info->acc.pctcpu = vps2->vps_acc->pctcpu.cur;
		++info;
	}
#endif
	LIST_FOREACH(vps2, &vps->vps_child_head, vps_sibling) {
		strncpy(info->name, vps2->vps_name, sizeof(info->name));
		strncpy(info->fsroot, vps2->_rootpath,
		    sizeof(info->fsroot));
		info->status = vps2->vps_status;
		info->nprocs = VPS_VPS(vps2, nprocs);
		info->nsocks = vps2->vnet->vnet_sockcnt;
		info->nifaces = vps2->vnet->vnet_ifcnt;
		info->restore_count = vps2->restore_count;
		info->acc.virt = vps2->vps_acc->virt.cur;
		info->acc.phys = vps2->vps_acc->phys.cur;
		info->acc.pctcpu = vps2->vps_acc->pctcpu.cur;
		++info;
	}
	sx_sunlock(&vps_all_lock);

	/* Return count of vps info structures to user. */
	*((int *)data) = vpscnt;

	return (0);
}

int
vps_ioc_create(struct vps *vps, struct vps_dev_ctx *ctx, u_long cmd,
    caddr_t data, int flags, struct thread *td)
{
	struct vps *vps2;
	struct vps_param *vps_pr;
	int error;

	error = 0;
	vps_pr = (struct vps_param *)data;
	vps_pr->name[sizeof(vps_pr->name) -1] = '\0';
	vps_pr->fsroot[sizeof(vps_pr->fsroot) -1] = '\0';
	if (strlen(vps_pr->name) == 0)
		return (EINVAL);

	sx_xlock(&vps_all_lock);
	vps2 = vps_by_name(vps, vps_pr->name);
	if (vps2) {
		sx_xunlock(&vps_all_lock);
		return (EADDRINUSE);
	}

	vps2 = vps_alloc(vps, vps_pr, vps_pr->name, &error);

	sx_xunlock(&vps_all_lock);

	return (error);
}

int
vps_ioc_destroy(struct vps *vps, struct vps_dev_ctx *ctx, u_long cmd,
    caddr_t data, int flags, struct thread *td)
{
	struct vps *vps2;
	char *vps_name;
	int error;

	vps_name = (char *)data;
	vps_name[MAXHOSTNAMELEN-1] = '\0';
	if (strlen(vps_name) == 0)
		return (EINVAL);

	sx_xlock(&vps_all_lock);
	vps2 = vps_by_name(vps, vps_name);
	if (vps2)
		error = vps_free(vps2);
	else
		error = ESRCH;
	sx_xunlock(&vps_all_lock);

	return (error);
}

int
vps_ioc_switch(struct vps *vps, struct vps_dev_ctx *ctx, u_long cmd,
    caddr_t data, int flags, struct thread *td)
{
	struct vps *vps2;
	char *vps_name;
	int error;

	vps_name = (char *)data;
	vps_name[MAXHOSTNAMELEN-1] = '\0';
	if (strlen(vps_name) == 0)
		return (EINVAL);

	sx_slock(&vps_all_lock);
	vps2 = vps_by_name(vps, vps_name);
	if (vps2)
		error = vps_switch_proc(td, vps2, 0);
	else
		error = ESRCH;
	sx_sunlock(&vps_all_lock);

	return (error);
}

/*
 * We don't support this anymore.
 * Instead the userspace tool uses a pts to wait for the switched process.
 */
int
vps_ioc_switchwait(struct vps *vps, struct vps_dev_ctx *ctx, u_long cmd,
    caddr_t data, int flags, struct thread *td)
{

	return (EOPNOTSUPP);
}

int
vps_ioc_ifmove(struct vps *vps, struct vps_dev_ctx *ctx, u_long cmd,
    caddr_t data, int flags, struct thread *td)
{
	struct vps_arg_ifmove *va_ifmove;
	struct vps *vps2;
	int error;

	va_ifmove = (struct vps_arg_ifmove *) data;
	va_ifmove->vps_name[sizeof(va_ifmove->vps_name) -1] = '\0';
	va_ifmove->if_name[sizeof(va_ifmove->if_name) -1] = '\0';
	va_ifmove->if_newname[sizeof(va_ifmove->if_newname) -1] = '\0';
	if (strlen(va_ifmove->vps_name) == 0)
		return (EINVAL);
	if (strlen(va_ifmove->if_name) == 0)
		return (EINVAL);
	if (strlen(va_ifmove->if_newname) == 0)
		return (EINVAL);

	sx_slock(&vps_all_lock);
	vps2 = vps_by_name(vps, va_ifmove->vps_name);
	if (vps2) {
		DBGUSER("%s: if_name=[%s] if_newname=[%s]\n",
		    __func__, va_ifmove->if_name, va_ifmove->if_newname);
		CURVNET_SET_QUIET(TD_TO_VNET(td));
		error = if_vmove_vps(td, va_ifmove->if_name,
		    sizeof(va_ifmove->if_name), vps2,
		    va_ifmove->if_newname);
		CURVNET_RESTORE();
	} else
		error = ESRCH;
	sx_sunlock(&vps_all_lock);

	return (error);
}

int
vps_ioc_suspend(struct vps *vps, struct vps_dev_ctx *ctx, u_long cmd,
    caddr_t data, int flags, struct thread *td)
{
	struct vps *vps2;
	struct vps_arg_flags *va;
	int error;

	va = (struct vps_arg_flags *)data;
	va->vps_name[MAXHOSTNAMELEN-1] = '\0';
	if (strlen(va->vps_name) == 0)
		return (EINVAL);
	if (va->flags & ~(VPS_SUSPEND_RELINKFILES))
		return (EINVAL);

	if (vps_func->vps_suspend == NULL)
		return (EOPNOTSUPP);

	sx_slock(&vps_all_lock);
	vps2 = vps_by_name(vps, va->vps_name);
	if (vps2) {
		sx_xlock(&vps2->vps_lock);
		error = vps_func->vps_suspend(vps2, va->flags);
		sx_xunlock(&vps2->vps_lock);
	} else
		error = ESRCH;
	sx_sunlock(&vps_all_lock);

	return (error);
}

int
vps_ioc_resume(struct vps *vps, struct vps_dev_ctx *ctx, u_long cmd,
    caddr_t data, int flags, struct thread *td)
{
	struct vps *vps2;
	struct vps_arg_flags *va;
	int error;

	va = (struct vps_arg_flags *)data;
	va->vps_name[MAXHOSTNAMELEN-1] = '\0';
	if (strlen(va->vps_name) == 0)
		return (EINVAL);
	if (va->flags & 1)
		return (EINVAL);

	if (vps_func->vps_resume == NULL)
		return (EOPNOTSUPP);

	sx_slock(&vps_all_lock);
	vps2 = vps_by_name(vps, va->vps_name);
	if (vps2) {
		if (vps2->vps_status == VPS_ST_SNAPSHOOTING &&
		    vps_func->vps_snapshot_finish)
			vps_func->vps_snapshot_finish(ctx, vps2);
		sx_xlock(&vps2->vps_lock);
		error = vps_func->vps_resume(vps2, va->flags);
		sx_xunlock(&vps2->vps_lock);
	} else
		error = ESRCH;
	sx_sunlock(&vps_all_lock);

	return (error);
}

int
vps_ioc_abort(struct vps *vps, struct vps_dev_ctx *ctx, u_long cmd,
    caddr_t data, int flags, struct thread *td)
{
	struct vps *vps2;
	struct vps_arg_flags *va;
	int error;

	va = (struct vps_arg_flags *)data;
	va->vps_name[MAXHOSTNAMELEN-1] = '\0';
	if (strlen(va->vps_name) == 0)
		return (EINVAL);
	if (va->flags & 1)
		return (EINVAL);

	if (vps_func->vps_abort == NULL)
		return (EOPNOTSUPP);

	sx_slock(&vps_all_lock);
	vps2 = vps_by_name(vps, va->vps_name);
	if (vps2) {
		if (vps2->vps_status == VPS_ST_SNAPSHOOTING &&
		    vps_func->vps_snapshot_finish)
			vps_func->vps_snapshot_finish(ctx, vps2);
		sx_xlock(&vps2->vps_lock);
		error = vps_func->vps_abort(vps2, va->flags);
		sx_xunlock(&vps2->vps_lock);
	} else
		error = ESRCH;
	sx_sunlock(&vps_all_lock);

	return (error);
}

int
vps_ioc_snapshot(struct vps *vps, struct vps_dev_ctx *ctx, u_long cmd,
    caddr_t data, int flags, struct thread *td)
{
	struct vps *vps2;
	struct vps_arg_snapst *va_snapst;
	int error;

	va_snapst = (struct vps_arg_snapst *)data;
	va_snapst->vps_name[MAXHOSTNAMELEN-1] = '\0';
	if (strlen(va_snapst->vps_name) == 0)
		return (EINVAL);

	sx_slock(&vps_all_lock);
	vps2 = vps_by_name(vps, va_snapst->vps_name);
	if (vps2) {
		if (vps_func->vps_snapshot == NULL)
			error = EOPNOTSUPP;
		else {
			sx_xlock(&vps2->vps_lock);
			error = vps_func->vps_snapshot(ctx, vps2,
			    va_snapst);
			sx_xunlock(&vps2->vps_lock);
		}
	} else
		error = ESRCH;
	sx_sunlock(&vps_all_lock);

	return (error);
}

int
vps_ioc_snapshot_finish(struct vps *vps, struct vps_dev_ctx *ctx,
    u_long cmd, caddr_t data, int flags, struct thread *td)
{
	int error;

	if (vps_func->vps_snapshot_finish == NULL)
		error = EOPNOTSUPP;
	else
		error = vps_func->vps_snapshot_finish(ctx, NULL);

	return (error);
}

int
vps_ioc_restore(struct vps *vps, struct vps_dev_ctx *ctx, u_long cmd,
    caddr_t data, int flags, struct thread *td)
{
	struct vps *vps2;
	struct vps_arg_snapst *va_snapst;
	int error;

	va_snapst = (struct vps_arg_snapst *)data;
	va_snapst->vps_name[MAXHOSTNAMELEN-1] = '\0';
	if (strlen(va_snapst->vps_name) == 0)
		return (EINVAL);

	sx_xlock(&vps_all_lock);
	vps2 = vps_by_name(vps, va_snapst->vps_name);
	if (vps2 == NULL) {
		if (vps_func->vps_restore == NULL)
			error = EOPNOTSUPP;
		else
			error = vps_func->vps_restore(ctx, va_snapst);
	} else
		error = EADDRINUSE;
	sx_xunlock(&vps_all_lock);

	return (error);
}

int
vps_ioc_argset(struct vps *vps, struct vps_dev_ctx *ctx, u_long cmd,
    caddr_t data, int flags, struct thread *td)
{
	struct vps *vps2;
	struct vps_arg_set *va;
	struct vps_arg_item *item;
	caddr_t kdata;
	size_t kdatalen;
	int error;

	va = (struct vps_arg_set *)data;
	va->vps_name[sizeof(va->vps_name) - 1] = '\0';
	if (strlen(va->vps_name) == 0)
		return (EINVAL);

	sx_xlock(&vps_all_lock);
	vps2 = vps_by_name(vps, va->vps_name);
	if (vps2 == NULL)
		error = ENOENT;
	else {
		kdatalen = va->datalen;
		DBGUSER("%s: kdatalen=%zu\n", __func__, kdatalen);
		kdata = malloc(kdatalen, M_TEMP, M_WAITOK);
		if ((error = copyin(va->data, kdata, kdatalen)))
			goto fail;
		for (item = (struct vps_arg_item *)kdata;
		    (caddr_t)item < kdata + kdatalen;
		    item++) {
			DBGUSER("%s: item=%p type=%u revoke=%u\n",
			    __func__, item, item->type, item->revoke);
			switch (item->type) {
			case VPS_ARG_ITEM_PRIV:
				error = vps_priv_setitem(vps, vps2, item);
				break;
			case VPS_ARG_ITEM_IP4:
			case VPS_ARG_ITEM_IP6:
				error = vps_ip_setitem(vps, vps2, item);
				break;
			case VPS_ARG_ITEM_LIMIT:
				error = vps_limit_setitem(vps, vps2, item);
				break;
			default:
				error = EINVAL;
				break;
			}
			if (error != 0)
				break;
		}
  fail:
		free(kdata, M_TEMP);
	}
	sx_xunlock(&vps_all_lock);

	return (error);
}

int
vps_ioc_argget(struct vps *vps, struct vps_dev_ctx *ctx, u_long cmd,
    caddr_t data, int flags, struct thread *td)
{
	struct vps *vps2;
	struct vps_arg_get *va;
	size_t kdatalen, len, klenused;
	caddr_t kdata;
	int error;

	va = (struct vps_arg_get *)data;
	va->vps_name[sizeof(va->vps_name) - 1] = '\0';
	if (strlen(va->vps_name) == 0)
		return (EINVAL);

	sx_xlock(&vps_all_lock);
	vps2 = vps_by_name(vps, va->vps_name);
	if (vps2 == NULL)
		error = ENOENT;
	else {
		error = 0;
		kdatalen = va->datalen;
		klenused = 0;
		kdata = malloc(kdatalen, M_TEMP, M_WAITOK | M_ZERO);

		len = kdatalen - klenused;
		if ((error = vps_priv_getitemall(vps, vps2, kdata +
		    klenused, &len)))
			goto fail;
		klenused += len;

		len = kdatalen - klenused;
		if ((error = vps_ip_getitemall(vps, vps2, kdata + klenused,
		    &len)))
			goto fail;
		klenused += len;

		len = kdatalen - klenused;
		error = vps_limit_getitemall(vps, vps2, kdata + klenused,
		    &len);
		if (error != 0 && error != EOPNOTSUPP)
			goto fail;
		klenused += len;

		KASSERT(klenused <= kdatalen,
		    ("%s: klenused=%zu > kdatalen=%zu !!!\n",
		    __func__, klenused, kdatalen));

		if ((error = copyout(kdata, va->data, klenused)))
			goto fail;
  fail:
		va->datalen = klenused;
		free(kdata, M_TEMP);
	}
	sx_xunlock(&vps_all_lock);

	return (error);
}

int
vps_ioc_getextinfo(struct vps *vps, struct vps_dev_ctx *ctx, u_long cmd,
    caddr_t data, int flags, struct thread *td)
{
	struct vps *vps2;
	struct vps_getextinfo *vx;
	struct vps_extinfo *xinfo;
	size_t kdatalen, klenused;
	caddr_t kdata;
	int error;

	vx = (struct vps_getextinfo *)data;
	vx->vps_name[sizeof(vx->vps_name) - 1] = '\0';
	if (strlen(vx->vps_name) == 0)
		return (EINVAL);

	if (vx->datalen < sizeof(*xinfo))
		return (EINVAL);

	sx_xlock(&vps_all_lock);
	vps2 = vps_by_name(vps, vx->vps_name);
	if (vps2 == NULL)
		error = ENOENT;
	else {
		error = 0;
		kdatalen = vx->datalen;
		klenused = 0;
		kdata = malloc(kdatalen, M_TEMP, M_WAITOK | M_ZERO);

		xinfo = (struct vps_extinfo *)(kdata + klenused);

		strncpy(xinfo->name, vps2->vps_name, sizeof(xinfo->name));
		strncpy(xinfo->fsroot, vps2->_rootpath,
		    sizeof(xinfo->fsroot));
		xinfo->status = vps2->vps_status;
		xinfo->nprocs = VPS_VPS(vps2, nprocs);
		xinfo->nsocks = vps2->vnet->vnet_sockcnt;
		xinfo->nifaces = vps2->vnet->vnet_ifcnt;
		xinfo->restore_count = vps2->restore_count;

		klenused += sizeof(*xinfo);

		KASSERT(klenused <= kdatalen,
		    ("%s: klenused=%zu > kdatalen=%zu !!!\n",
		    __func__, klenused, kdatalen));

		if ((error = copyout(kdata, vx->data, klenused)))
			goto fail;
  fail:
		vx->datalen = klenused;
		free(kdata, M_TEMP);
	}
	sx_xunlock(&vps_all_lock);

	return (error);
}

int
vps_ioc_fscalc(struct vps *vps, struct vps_dev_ctx *ctx, u_long cmd,
	caddr_t data, int flags, struct thread *td)
{

	return (EOPNOTSUPP);
}

int
vps_ioc_fscalcpath(struct vps *vps, struct vps_dev_ctx *ctx, u_long cmd,
	caddr_t data, int flags, struct thread *td)
{
	struct vps_arg_get *va;
	struct vps_arg_item *item;
	size_t kdatalen;
	caddr_t kdata;
	int error;

	va = (struct vps_arg_get *)data;
	va->vps_name[sizeof (va->vps_name) - 1] = '\0';
	if (strlen (va->vps_name) == 0)
		return (EINVAL);

	if (va->datalen < sizeof(*item) * 2)
		return (EINVAL);

	error = 0;
	kdatalen = va->datalen;
	kdata = malloc(kdatalen, M_TEMP, M_WAITOK | M_ZERO);

	/* XXX
	if ((error = vps_account_vpsfs_calc_path(vps, va->vps_name,
	    kdata, &kdatalen)))
		goto fail;
	*/
	error = EOPNOTSUPP;
	goto fail;

	if ((error = copyout(kdata, va->data, kdatalen)))
		goto fail;

 fail:
	va->datalen = kdatalen;
	free(kdata, M_TEMP);

	return (error);
}

int
vps_ioc_getconsfd(struct vps *vps, struct vps_dev_ctx *ctx, u_long cmd,
	caddr_t data, int flags, struct thread *td)
{
	struct vps *vps2;
	struct vps_arg_getconsfd *va;
	int error;

	va = (struct vps_arg_getconsfd *)data;
	va->vps_name[MAXHOSTNAMELEN-1] = '\0';
	if (strlen(va->vps_name) == 0)
		return (EINVAL);

	sx_xlock(&vps_all_lock);
	vps2 = vps_by_name(vps, va->vps_name);
	if (vps2)
		error = vps_console_getfd(vps2, td, &va->consfd);
	else
		error = ESRCH;
	sx_xunlock(&vps_all_lock);

	return (error);
}

#endif /* VPS */

/* EOF */
