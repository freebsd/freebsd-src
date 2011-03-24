/*
 * Copyright (c) 2004, 2005 Topspin Communications.  All rights reserved.
 * Copyright (c) 2006, 2007 Cisco Systems, Inc.  All rights reserved.
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
 */

#if HAVE_CONFIG_H
#  include <config.h>
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <alloca.h>
#include <errno.h>

#include <infiniband/arch.h>

#include "ibverbs.h"

static pthread_mutex_t device_list_lock = PTHREAD_MUTEX_INITIALIZER;
static int num_devices;
static struct ibv_device **device_list;

struct ibv_device **__ibv_get_device_list(int *num)
{
	struct ibv_device **l = 0;
	int i;

	if (num)
		*num = 0;

	pthread_mutex_lock(&device_list_lock);

	if (!num_devices)
		num_devices = ibverbs_init(&device_list);

	if (num_devices < 0) {
		errno = -num_devices;
		goto out;
	}

	l = calloc(num_devices + 1, sizeof (struct ibv_device *));
	if (!l) {
		errno = ENOMEM;
		goto out;
	}

	for (i = 0; i < num_devices; ++i)
		l[i] = device_list[i];
	if (num)
		*num = num_devices;

out:
	pthread_mutex_unlock(&device_list_lock);
	return l;
}
default_symver(__ibv_get_device_list, ibv_get_device_list);

void __ibv_free_device_list(struct ibv_device **list)
{
	free(list);
}
default_symver(__ibv_free_device_list, ibv_free_device_list);

const char *__ibv_get_device_name(struct ibv_device *device)
{
	return device->name;
}
default_symver(__ibv_get_device_name, ibv_get_device_name);

uint64_t __ibv_get_device_guid(struct ibv_device *device)
{
	char attr[24];
	uint64_t guid = 0;
	uint16_t parts[4];
	int i;

	if (ibv_read_sysfs_file(device->ibdev_path, "node_guid",
				attr, sizeof attr) < 0)
		return 0;

	if (sscanf(attr, "%hx:%hx:%hx:%hx",
		   parts, parts + 1, parts + 2, parts + 3) != 4)
		return 0;

	for (i = 0; i < 4; ++i)
		guid = (guid << 16) | parts[i];

	return htonll(guid);
}
default_symver(__ibv_get_device_guid, ibv_get_device_guid);

struct ibv_context *__ibv_open_device(struct ibv_device *device)
{
	char *devpath;
	int cmd_fd;
	struct ibv_context *context;

	if (asprintf(&devpath, "/dev/%s", device->dev_name) < 0)
		return NULL;

	/*
	 * We'll only be doing writes, but we need O_RDWR in case the
	 * provider needs to mmap() the file.
	 */
	cmd_fd = open(devpath, O_RDWR);
	free(devpath);

	if (cmd_fd < 0)
		return NULL;

	context = device->ops.alloc_context(device, cmd_fd);
	if (!context)
		goto err;

	context->device = device;
	context->cmd_fd = cmd_fd;
	pthread_mutex_init(&context->mutex, NULL);

	return context;

err:
	close(cmd_fd);

	return NULL;
}
default_symver(__ibv_open_device, ibv_open_device);

int __ibv_close_device(struct ibv_context *context)
{
	int async_fd = context->async_fd;
	int cmd_fd   = context->cmd_fd;
	int cq_fd    = -1;

	if (abi_ver <= 2) {
		struct ibv_abi_compat_v2 *t = context->abi_compat;
		cq_fd = t->channel.fd;
		free(context->abi_compat);
	}

	context->device->ops.free_context(context);

	close(async_fd);
	close(cmd_fd);
	if (abi_ver <= 2)
		close(cq_fd);

	return 0;
}
default_symver(__ibv_close_device, ibv_close_device);

int __ibv_get_async_event(struct ibv_context *context,
			  struct ibv_async_event *event)
{
	struct ibv_kern_async_event ev;

	if (read(context->async_fd, &ev, sizeof ev) != sizeof ev)
		return -1;

	event->event_type = ev.event_type;

	if (event->event_type & IBV_XRC_QP_EVENT_FLAG) {
		event->element.xrc_qp_num = ev.element;
	} else
		switch (event->event_type) {
		case IBV_EVENT_CQ_ERR:
			event->element.cq = (void *) (uintptr_t) ev.element;
			break;

		case IBV_EVENT_QP_FATAL:
		case IBV_EVENT_QP_REQ_ERR:
		case IBV_EVENT_QP_ACCESS_ERR:
		case IBV_EVENT_COMM_EST:
		case IBV_EVENT_SQ_DRAINED:
		case IBV_EVENT_PATH_MIG:
		case IBV_EVENT_PATH_MIG_ERR:
		case IBV_EVENT_QP_LAST_WQE_REACHED:
			event->element.qp = (void *) (uintptr_t) ev.element;
			break;

		case IBV_EVENT_SRQ_ERR:
		case IBV_EVENT_SRQ_LIMIT_REACHED:
			event->element.srq = (void *) (uintptr_t) ev.element;
			break;
		default:
			event->element.port_num = ev.element;
			break;
		}

	if (context->ops.async_event)
		context->ops.async_event(event);

	return 0;
}
default_symver(__ibv_get_async_event, ibv_get_async_event);

void __ibv_ack_async_event(struct ibv_async_event *event)
{
	switch (event->event_type) {
	case IBV_EVENT_CQ_ERR:
	{
		struct ibv_cq *cq = event->element.cq;

		pthread_mutex_lock(&cq->mutex);
		++cq->async_events_completed;
		pthread_cond_signal(&cq->cond);
		pthread_mutex_unlock(&cq->mutex);

		return;
	}

	case IBV_EVENT_QP_FATAL:
	case IBV_EVENT_QP_REQ_ERR:
	case IBV_EVENT_QP_ACCESS_ERR:
	case IBV_EVENT_COMM_EST:
	case IBV_EVENT_SQ_DRAINED:
	case IBV_EVENT_PATH_MIG:
	case IBV_EVENT_PATH_MIG_ERR:
	case IBV_EVENT_QP_LAST_WQE_REACHED:
	{
		struct ibv_qp *qp = event->element.qp;

		pthread_mutex_lock(&qp->mutex);
		++qp->events_completed;
		pthread_cond_signal(&qp->cond);
		pthread_mutex_unlock(&qp->mutex);

		return;
	}

	case IBV_EVENT_SRQ_ERR:
	case IBV_EVENT_SRQ_LIMIT_REACHED:
	{
		struct ibv_srq *srq = event->element.srq;

		pthread_mutex_lock(&srq->mutex);
		++srq->events_completed;
		pthread_cond_signal(&srq->cond);
		pthread_mutex_unlock(&srq->mutex);

		return;
	}

	default:
		return;
	}
}
default_symver(__ibv_ack_async_event, ibv_ack_async_event);
