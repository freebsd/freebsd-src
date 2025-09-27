/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Scott Long
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
 */

#include "opt_thunderbolt.h"

/* Userspace control device for USB4 / TB3 */
#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/malloc.h>
#include <sys/queue.h>
#include <sys/sysctl.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/nv.h>
#include <sys/taskqueue.h>
#include <sys/gsb_crc32.h>
#include <sys/endian.h>
#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/bus.h>
#include <machine/stdarg.h>

#include <dev/thunderbolt/nhi_reg.h>
#include <dev/thunderbolt/nhi_var.h>
#include <dev/thunderbolt/tb_reg.h>
#include <dev/thunderbolt/tb_var.h>
#include <dev/thunderbolt/tbcfg_reg.h>
#include <dev/thunderbolt/router_var.h>
#include <dev/thunderbolt/tb_debug.h>
#include <dev/thunderbolt/tb_dev.h>
#include <dev/thunderbolt/tb_ioctl.h>

struct tbdev_if;
struct tbdev_dm;
struct tbdev_rt;

struct tbdev_if {
	TAILQ_ENTRY(tbdev_if)	dev_next;
	char			name[SPECNAMELEN];
};

struct tbdev_dm {
	TAILQ_ENTRY(tbdev_dm)	dev_next;
	char			uid[16];
};

struct tbdev_rt {
	TAILQ_ENTRY(tbdev_rt)	dev_next;
	uint64_t		route;
};

static int tbdev_static_ioctl(struct cdev *dev, u_long cmd, caddr_t addr, int flag, struct thread *td);

static struct cdevsw tbdev_static_devsw = {
	.d_version = D_VERSION,
	.d_ioctl = tbdev_static_ioctl,
	.d_name = "tbt"
};
static struct cdev *tb_dev = NULL;

static TAILQ_HEAD(, tbdev_if) tbdev_head = TAILQ_HEAD_INITIALIZER(tbdev_head);
static TAILQ_HEAD(, tbdev_dm) tbdomain_head = TAILQ_HEAD_INITIALIZER(tbdomain_head);
static TAILQ_HEAD(, tbdev_rt) tbrouter_head = TAILQ_HEAD_INITIALIZER(tbrouter_head);

static struct mtx tbdev_mtx;
MTX_SYSINIT(tbdev_mtx, &tbdev_mtx, "TBT Device Mutex", MTX_DEF);

MALLOC_DEFINE(M_THUNDERBOLT, "thunderbolt", "memory for thunderbolt");

static void
tbdev_init(void *arg)
{

	tb_dev = make_dev(&tbdev_static_devsw, 0, UID_ROOT, GID_OPERATOR,
	    0644, TBT_DEVICE_NAME);
	if (tb_dev == NULL)
		printf("Cannot create Thunderbolt system device\n");

	return;
}

SYSINIT(tbdev_init, SI_SUB_KICK_SCHEDULER, SI_ORDER_FIRST, tbdev_init, NULL);

static void
tbdev_uninit(void *arg)
{
	if (tb_dev != NULL) {
		destroy_dev(tb_dev);
		tb_dev = NULL;
	}
}

SYSUNINIT(tbdev_uninit, SI_SUB_KICK_SCHEDULER, SI_ORDER_ANY, tbdev_uninit, NULL);

int
tbdev_add_interface(struct nhi_softc *nhi)
{
	struct tbdev_if *ifce;

	ifce = malloc(sizeof(struct tbdev_if), M_THUNDERBOLT, M_ZERO|M_NOWAIT);
	if (ifce == NULL)
		return (ENOMEM);

	strlcpy(ifce->name, device_get_nameunit(nhi->dev), SPECNAMELEN);
	mtx_lock(&tbdev_mtx);
	TAILQ_INSERT_TAIL(&tbdev_head, ifce, dev_next);
	mtx_unlock(&tbdev_mtx);

	return (0);
}

int
tbdev_remove_interface(struct nhi_softc *nhi)
{
	struct tbdev_if *ifce = NULL, *if_back;
	const char *name;

	name = device_get_nameunit(nhi->dev);
	mtx_lock(&tbdev_mtx);
	TAILQ_FOREACH_SAFE(ifce, &tbdev_head, dev_next, if_back) {
		if (strncmp(name, ifce->name, SPECNAMELEN) == 0) {
			TAILQ_REMOVE(&tbdev_head, ifce, dev_next);
			break;
		}
	}
	mtx_unlock(&tbdev_mtx);

	if (ifce != NULL)
		free(ifce, M_THUNDERBOLT);

	return (0);
}

int
tbdev_add_domain(void *domain)
{

	return (0);
}

int
tbdev_remove_domain(void *domain)
{

	return (0);
}

int
tbdev_add_router(struct router_softc *rt)
{

	return (0);
}

int
tbdev_remove_router(struct router_softc *rt)
{

	return (0);
}

static int
tbdev_discover(caddr_t addr)
{
	nvlist_t	*nvl = NULL;
	struct tbt_ioc	*ioc = (struct tbt_ioc *)addr;
	struct tbdev_if *dev;
	struct tbdev_dm	*dm;
	struct tbdev_rt	*rt;
	void		*nvlpacked = NULL;
	const char	*cmd = NULL;
	int		error = 0;

	if ((ioc->data == NULL) || (ioc->size == 0)) {
		printf("data or size is 0\n");
		return (EINVAL);
	}

	if ((ioc->len == 0) || (ioc->len > TBT_IOCMAXLEN) ||
	    (ioc->len > ioc->size)) {
		printf("len is wrong\n");
		return (EINVAL);
	}

	nvlpacked = malloc(ioc->len, M_THUNDERBOLT, M_NOWAIT);
	if (nvlpacked == NULL) {
		printf("cannot allocate nvlpacked\n");
		return (ENOMEM);
	}

	error = copyin(ioc->data, nvlpacked, ioc->len);
	if (error) {
		free(nvlpacked, M_THUNDERBOLT);
		printf("error %d from copyin\n", error);
		return (error);
	}

	nvl = nvlist_unpack(nvlpacked, ioc->len, NV_FLAG_NO_UNIQUE);
	if (nvl == NULL) {
		free(nvlpacked, M_THUNDERBOLT);
		printf("cannot unpack nvlist\n");
		return (EINVAL);
	}
	free(nvlpacked, M_THUNDERBOLT);
	nvlpacked = NULL;

	if (nvlist_exists_string(nvl, TBT_DISCOVER_TYPE))
		cmd = nvlist_get_string(nvl, TBT_DISCOVER_TYPE);
	if (cmd == NULL) {
		printf("cannot find type string\n");
		error = EINVAL;
		goto out;
	}

	mtx_lock(&tbdev_mtx);
	if (strncmp(cmd, TBT_DISCOVER_IFACE, TBT_NAMLEN) == 0) {
		TAILQ_FOREACH(dev, &tbdev_head, dev_next)
			nvlist_add_string(nvl, TBT_DISCOVER_IFACE, dev->name);
	} else if (strncmp(cmd, TBT_DISCOVER_DOMAIN, TBT_NAMLEN) == 0) {
		TAILQ_FOREACH(dm, &tbdomain_head, dev_next)
			nvlist_add_string(nvl, TBT_DISCOVER_DOMAIN, dm->uid);
	} else if (strncmp(cmd, TBT_DISCOVER_ROUTER, TBT_NAMLEN) == 0) {
		TAILQ_FOREACH(rt, &tbrouter_head, dev_next)
			nvlist_add_number(nvl, TBT_DISCOVER_ROUTER, rt->route);
	} else {
		printf("cannot find supported tpye\n");
		error = EINVAL;
		goto out;
	}
	mtx_unlock(&tbdev_mtx);

	error = nvlist_error(nvl);
	if (error != 0) {
		printf("error %d state in nvlist\n", error);
		return (error);
	}

	nvlpacked = nvlist_pack(nvl, &ioc->len);
	if (nvlpacked == NULL) {
		printf("cannot allocate new packed buffer\n");
		return (ENOMEM);
	}
	if (ioc->size < ioc->len) {
		printf("packed buffer is too big to copyout\n");
		return (ENOSPC);
	}

	error = copyout(nvlpacked, ioc->data, ioc->len);
	if (error)
		printf("error %d on copyout\n", error);

out:
	if (nvlpacked != NULL)
		free(nvlpacked, M_NVLIST);
	if (nvl != NULL)
		nvlist_destroy(nvl);

	return (error);
}

static int
tbdev_request(caddr_t addr)
{
	struct tbt_ioc	*ioc = (struct tbt_ioc *)addr;
	nvlist_t	*nvl = NULL;
	void		*nvlpacked = NULL;
	int		error = 0;

	if ((ioc->data == NULL) || (ioc->size == 0))
		return (ENOMEM);

	nvlpacked = nvlist_pack(nvl, &ioc->len);
	if (nvlpacked == NULL)
		return (ENOMEM);
	if (ioc->size < ioc->len)
		return (ENOSPC);

	error = copyout(nvlpacked, ioc->data, ioc->len);
	return (error);
}

static int
tbdev_static_ioctl(struct cdev *dev, u_long cmd, caddr_t addr, int flags,
    struct thread *td)
{
	int error = 0;

	switch (cmd) {
	case TBT_DISCOVER:
		error = tbdev_discover(addr);
		break;
	case TBT_REQUEST:
		error = tbdev_request(addr);
		break;
	default:
		error = EINVAL;
	}

	return (error);
}
