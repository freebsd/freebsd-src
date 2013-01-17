/*
 * Copyright (c) 2004 Topspin Communications.  All rights reserved.
 * Copyright (c) 2005 Intel Corporation. All rights reserved.
 * Copyright (c) 2005 Sun Microsystems, Inc. All rights reserved.
 * Copyright (c) 2005 Voltaire, Inc. All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * $Id: cache.c 1349 2004-12-16 21:09:43Z roland $
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/taskqueue.h>
#include <sys/lock.h>
#include <sys/rwlock.h>
#include <sys/mutex.h>
#include <sys/module.h>
#include <sys/syslog.h>


#ifdef needed
#include <sys/condvar.h>
#include <sys/socket.h>
#include <sys/condvar.h>
#endif

#include <contrib/rdma/ib_cache.h>

#include "core_priv.h"

struct ib_pkey_cache {
	int             table_len;
	u16             table[0];
};

struct ib_gid_cache {
	int             table_len;
	union ib_gid    table[0];
};

struct ib_update_work {
	struct task 	  task;
	struct ib_device  *device;
	u8                 port_num;
};

static inline int start_port(struct ib_device *device)
{
	return (device->node_type == RDMA_NODE_IB_SWITCH) ? 0 : 1;
}

static inline int end_port(struct ib_device *device)
{
	return (device->node_type == RDMA_NODE_IB_SWITCH) ?
		0 : device->phys_port_cnt;
}

int ib_get_cached_gid(struct ib_device *device,
		      u8                port_num,
		      int               index,
		      union ib_gid     *gid)
{
	struct ib_gid_cache *cache;
	int ret = 0;

	if (port_num < start_port(device) || port_num > end_port(device))
		return -EINVAL;

	mtx_lock(&device->cache.lock);

	cache = device->cache.gid_cache[port_num - start_port(device)];

	if (index < 0 || index >= cache->table_len)
		ret = -EINVAL;
	else
		*gid = cache->table[index];

	mtx_unlock(&device->cache.lock);

	return ret;
}

int ib_find_cached_gid(struct ib_device *device,
		       union ib_gid	*gid,
		       u8               *port_num,
		       u16              *index)
{
	struct ib_gid_cache *cache;
	int p, i;
	int ret = -ENOENT;

	*port_num = -1;
	if (index)
		*index = -1;

	mtx_lock(&device->cache.lock);

	for (p = 0; p <= end_port(device) - start_port(device); ++p) {
		cache = device->cache.gid_cache[p];
		for (i = 0; i < cache->table_len; ++i) {
			if (!memcmp(gid, &cache->table[i], sizeof *gid)) {
				*port_num = p + start_port(device);
				if (index)
					*index = i;
				ret = 0;
				goto found;
			}
		}
	}
found:
	mtx_unlock(&device->cache.lock);

	return ret;
}

int ib_get_cached_pkey(struct ib_device *device,
		       u8                port_num,
		       int               index,
		       u16              *pkey)
{
	struct ib_pkey_cache *cache;
	int ret = 0;

	if (port_num < start_port(device) || port_num > end_port(device))
		return -EINVAL;

	mtx_lock(&device->cache.lock);

	cache = device->cache.pkey_cache[port_num - start_port(device)];

	if (index < 0 || index >= cache->table_len)
		ret = -EINVAL;
	else
		*pkey = cache->table[index];

	mtx_unlock(&device->cache.lock);

	return ret;
}

int ib_find_cached_pkey(struct ib_device *device,
			u8                port_num,
			u16               pkey,
			u16              *index)
{
	struct ib_pkey_cache *cache;
	int i;
	int ret = -ENOENT;

	if (port_num < start_port(device) || port_num > end_port(device))
		return -EINVAL;

	mtx_lock(&device->cache.lock);

	cache = device->cache.pkey_cache[port_num - start_port(device)];

	*index = -1;

	for (i = 0; i < cache->table_len; ++i)
		if ((cache->table[i] & 0x7fff) == (pkey & 0x7fff)) {
			*index = i;
			ret = 0;
			break;
		}

	mtx_unlock(&device->cache.lock);

	return ret;
}

int ib_get_cached_lmc(struct ib_device *device,
		      u8                port_num,
		      u8                *lmc)
{
	int ret = 0;

	if (port_num < start_port(device) || port_num > end_port(device))
		return -EINVAL;

	mtx_lock(&device->cache.lock);
	*lmc = device->cache.lmc_cache[port_num - start_port(device)];
	mtx_unlock(&device->cache.lock);

	return ret;
}

static void ib_cache_update(struct ib_device *device,
			    u8                port)
{
	struct ib_port_attr       *tprops = NULL;
	struct ib_pkey_cache      *pkey_cache = NULL, *old_pkey_cache;
	struct ib_gid_cache       *gid_cache = NULL, *old_gid_cache;
	int                        i;
	int                        ret;

	tprops = malloc(sizeof *tprops, M_DEVBUF, M_NOWAIT);
	if (!tprops)
		return;

	ret = ib_query_port(device, port, tprops);
	if (ret) {
		log(LOG_WARNING, "ib_query_port failed (%d) for %s\n",
		       ret, device->name);
		goto err;
	}

	pkey_cache = malloc(sizeof *pkey_cache + tprops->pkey_tbl_len *
			     sizeof *pkey_cache->table, M_DEVBUF, M_NOWAIT);
	if (!pkey_cache)
		goto err;

	pkey_cache->table_len = tprops->pkey_tbl_len;

	gid_cache = malloc(sizeof *gid_cache + tprops->gid_tbl_len *
			    sizeof *gid_cache->table, M_DEVBUF, M_NOWAIT);
	if (!gid_cache)
		goto err;

	gid_cache->table_len = tprops->gid_tbl_len;

	for (i = 0; i < pkey_cache->table_len; ++i) {
		ret = ib_query_pkey(device, port, i, pkey_cache->table + i);
		if (ret) {
			log(LOG_WARNING, "ib_query_pkey failed (%d) for %s (index %d)\n",
			       ret, device->name, i);
			goto err;
		}
	}

	for (i = 0; i < gid_cache->table_len; ++i) {
		ret = ib_query_gid(device, port, i, gid_cache->table + i);
		if (ret) {
			log(LOG_WARNING, "ib_query_gid failed (%d) for %s (index %d)\n",
			       ret, device->name, i);
			goto err;
		}
	}

	mtx_lock(&device->cache.lock);

	old_pkey_cache = device->cache.pkey_cache[port - start_port(device)];
	old_gid_cache  = device->cache.gid_cache [port - start_port(device)];

	device->cache.pkey_cache[port - start_port(device)] = pkey_cache;
	device->cache.gid_cache [port - start_port(device)] = gid_cache;

	device->cache.lmc_cache[port - start_port(device)] = tprops->lmc;

	mtx_unlock(&device->cache.lock);

	free(old_pkey_cache, M_DEVBUF);
	free(old_gid_cache, M_DEVBUF);
	free(tprops, M_DEVBUF);
	return;

err:
	free(pkey_cache, M_DEVBUF);
	free(gid_cache, M_DEVBUF);
	free(tprops, M_DEVBUF);
}

static void ib_cache_task(void *context, int pending)
{
	struct ib_update_work *work = context;

	ib_cache_update(work->device, work->port_num);
	free(work, M_DEVBUF);
}

static void ib_cache_event(struct ib_event_handler *handler,
			   struct ib_event *event)
{
	struct ib_update_work *work;

	if (event->event == IB_EVENT_PORT_ERR    ||
	    event->event == IB_EVENT_PORT_ACTIVE ||
	    event->event == IB_EVENT_LID_CHANGE  ||
	    event->event == IB_EVENT_PKEY_CHANGE ||
	    event->event == IB_EVENT_SM_CHANGE   ||
	    event->event == IB_EVENT_CLIENT_REREGISTER) {
		work = malloc(sizeof *work, M_DEVBUF, M_NOWAIT);
		if (work) {
			TASK_INIT(&work->task, 0, ib_cache_task, work);
			work->device   = event->device;
			work->port_num = event->element.port_num;
			taskqueue_enqueue(taskqueue_thread, &work->task);
		}
	}
}

static void ib_cache_setup_one(struct ib_device *device)
{
	int p;

	mtx_init(&device->cache.lock, "ib device cache", NULL, 
		MTX_DUPOK|MTX_DEF);

	device->cache.pkey_cache =
		malloc(sizeof *device->cache.pkey_cache *
			(end_port(device) - start_port(device) + 1), M_DEVBUF, 
			M_NOWAIT);
	device->cache.gid_cache =
		malloc(sizeof *device->cache.gid_cache *
			(end_port(device) - start_port(device) + 1), M_DEVBUF, 
			M_NOWAIT);

	device->cache.lmc_cache = malloc(sizeof *device->cache.lmc_cache *
					  (end_port(device) -
					  start_port(device) + 1),
					  M_DEVBUF, M_NOWAIT);

	if (!device->cache.pkey_cache || !device->cache.gid_cache ||
	    !device->cache.lmc_cache) {
		log(LOG_WARNING, "Couldn't allocate cache "
		       "for %s\n", device->name);
		goto err;
	}

	for (p = 0; p <= end_port(device) - start_port(device); ++p) {
		device->cache.pkey_cache[p] = NULL;
		device->cache.gid_cache [p] = NULL;
		ib_cache_update(device, p + start_port(device));
	}

	INIT_IB_EVENT_HANDLER(&device->cache.event_handler,
			      device, ib_cache_event);
	if (ib_register_event_handler(&device->cache.event_handler))
		goto err_cache;

	return;

err_cache:
	for (p = 0; p <= end_port(device) - start_port(device); ++p) {
		free(device->cache.pkey_cache[p], M_DEVBUF);
		free(device->cache.gid_cache[p], M_DEVBUF);
	}

err:
	free(device->cache.pkey_cache, M_DEVBUF);
	free(device->cache.gid_cache, M_DEVBUF);
	free(device->cache.lmc_cache, M_DEVBUF);
}

static void ib_cache_cleanup_one(struct ib_device *device)
{
	int p;

	ib_unregister_event_handler(&device->cache.event_handler);
#ifdef XXX
	flush_scheduled_work();
#endif

	for (p = 0; p <= end_port(device) - start_port(device); ++p) {
		free(device->cache.pkey_cache[p], M_DEVBUF);
		free(device->cache.gid_cache[p], M_DEVBUF);
	}

	free(device->cache.pkey_cache, M_DEVBUF);
	free(device->cache.gid_cache, M_DEVBUF);
	free(device->cache.lmc_cache, M_DEVBUF);
}

static struct ib_client cache_client = {
	.name   = "cache",
	.add    = ib_cache_setup_one,
	.remove = ib_cache_cleanup_one
};

int ib_cache_setup(void)
{
	return ib_register_client(&cache_client);
}

void ib_cache_cleanup(void)
{
	ib_unregister_client(&cache_client);
}
