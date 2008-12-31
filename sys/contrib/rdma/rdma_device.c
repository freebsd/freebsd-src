/*
 * Copyright (c) 2004 Topspin Communications.  All rights reserved.
 * Copyright (c) 2005 Sun Microsystems, Inc. All rights reserved.
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
 * $Id: device.c 1349 2004-12-16 21:09:43Z roland $
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/contrib/rdma/rdma_device.c,v 1.1.2.1.2.1 2008/11/25 02:59:29 kensmith Exp $");

#include <sys/types.h>
#include <sys/param.h>
#include <sys/condvar.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/libkern.h>
#include <sys/socket.h>
#include <sys/module.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/rwlock.h>
#include <sys/queue.h>
#include <sys/taskqueue.h>
#include <sys/priv.h>
#include <sys/syslog.h>

#include <contrib/rdma/core_priv.h>

struct ib_client_data {
	TAILQ_ENTRY(ib_client_data) list;
	struct ib_client *client;
	void *            data;
};

static TAILQ_HEAD(, ib_device) device_list;
static TAILQ_HEAD(client_list_s, ib_client) client_list;

/*
 * device_mutex protects access to both device_list and client_list.
 * There's no real point to using multiple locks or something fancier
 * like an rwsem: we always access both lists, and we're always
 * modifying one list or the other list.  In any case this is not a
 * hot path so there's no point in trying to optimize.
 */
static struct mtx device_mutex;

static int ib_device_check_mandatory(struct ib_device *device)
{
#define IB_MANDATORY_FUNC(x) { offsetof(struct ib_device, x), #x }
#define MANDATORY_TABLE_DEPTH 19
	static const struct {
		size_t offset;
		char  *name;
	} mandatory_table[] = {
		IB_MANDATORY_FUNC(query_device),
		IB_MANDATORY_FUNC(query_port),
		IB_MANDATORY_FUNC(query_pkey),
		IB_MANDATORY_FUNC(query_gid),
		IB_MANDATORY_FUNC(alloc_pd),
		IB_MANDATORY_FUNC(dealloc_pd),
		IB_MANDATORY_FUNC(create_ah),
		IB_MANDATORY_FUNC(destroy_ah),
		IB_MANDATORY_FUNC(create_qp),
		IB_MANDATORY_FUNC(modify_qp),
		IB_MANDATORY_FUNC(destroy_qp),
		IB_MANDATORY_FUNC(post_send),
		IB_MANDATORY_FUNC(post_recv),
		IB_MANDATORY_FUNC(create_cq),
		IB_MANDATORY_FUNC(destroy_cq),
		IB_MANDATORY_FUNC(poll_cq),
		IB_MANDATORY_FUNC(req_notify_cq),
		IB_MANDATORY_FUNC(get_dma_mr),
		IB_MANDATORY_FUNC(dereg_mr)
	};
	int i;

	for (i = 0; i < MANDATORY_TABLE_DEPTH; ++i) {
		if (!*(void **) ((void *) ((unsigned long)device + mandatory_table[i].offset))) {
			log(LOG_WARNING, "Device %s is missing mandatory function %s\n",
			       device->name, mandatory_table[i].name);
			return (EINVAL);
		}
	}

	return 0;
}

static struct ib_device *__ib_device_get_by_name(const char *name)
{
	struct ib_device *device;

	TAILQ_FOREACH(device, &device_list, core_list)
		if (!strncmp(name, device->name, IB_DEVICE_NAME_MAX))
			return device;

	return NULL;
}


static int alloc_name(char *name)
{
	long *inuse;
	char buf[IB_DEVICE_NAME_MAX];
	struct ib_device *device;
	int i;

	inuse = malloc(PAGE_SIZE, M_DEVBUF, M_NOWAIT);
	if (!inuse)
		return (ENOMEM);

	TAILQ_FOREACH(device, &device_list, core_list) {
		if (!sscanf(device->name, name, &i))
			continue;
		if (i < 0 || i >= PAGE_SIZE * 8)
			continue;
		snprintf(buf, sizeof buf, name, i);
		if (!strncmp(buf, device->name, IB_DEVICE_NAME_MAX))
			setbit(inuse, i);
	}

	i = find_first_zero_bit(inuse, PAGE_SIZE * 8);
	free(inuse, M_DEVBUF);
	snprintf(buf, sizeof buf, name, i);

	if (__ib_device_get_by_name(buf))
		return (ENFILE);

	strlcpy(name, buf, IB_DEVICE_NAME_MAX);
	return 0;
}

static int start_port(struct ib_device *device)
{
	return (device->node_type == RDMA_NODE_IB_SWITCH) ? 0 : 1;
}


static int end_port(struct ib_device *device)
{
	return (device->node_type == RDMA_NODE_IB_SWITCH) ?
		0 : device->phys_port_cnt;
}

/**
 * ib_alloc_device - allocate an IB device struct
 * @size:size of structure to allocate
 *
 * Low-level drivers should use ib_alloc_device() to allocate &struct
 * ib_device.  @size is the size of the structure to be allocated,
 * including any private data used by the low-level driver.
 * ib_dealloc_device() must be used to free structures allocated with
 * ib_alloc_device().
 */
struct ib_device *ib_alloc_device(size_t size)
{
	void *dev;

	if (size < sizeof (struct ib_device))
		panic("size=%zd < sizeof (struct ib_device)=%zd)",
		    size, sizeof (struct ib_device));
	
	dev = malloc(size, M_DEVBUF, M_NOWAIT);
	if (dev)
		bzero(dev, size);
	return dev;
}

/**
 * ib_dealloc_device - free an IB device struct
 * @device:structure to free
 *
 * Free a structure allocated with ib_alloc_device().
 */
void ib_dealloc_device(struct ib_device *device)
{
	if (device->reg_state == IB_DEV_UNINITIALIZED) {
		free(device, M_DEVBUF);
		return;
	}

	if (device->reg_state != IB_DEV_UNREGISTERED)
		panic("device->reg_state=%d != IB_DEV_UNREGISTERED)",
		    device->reg_state);
#ifdef notyet
	ib_device_unregister_sysfs(device);
#endif
}

static int add_client_context(struct ib_device *device, struct ib_client *client)
{
	struct ib_client_data *context;

	context = malloc(sizeof *context, M_DEVBUF, M_NOWAIT);
	if (!context) {
		log(LOG_WARNING, "Couldn't allocate client context for %s/%s\n",
		       device->name, client->name);
		return (ENOMEM);
	}

	context->client = client;
	context->data   = NULL;

	mtx_lock(&device->client_data_lock);
	TAILQ_INSERT_TAIL(&device->client_data_list, context, list);
	mtx_unlock(&device->client_data_lock);

	return 0;
}

static int read_port_table_lengths(struct ib_device *device)
{
	struct ib_port_attr *tprops = NULL;
	int num_ports, ret = ENOMEM;
	u8 port_index;

	tprops = malloc(sizeof *tprops, M_DEVBUF, M_NOWAIT);
	if (!tprops)
		goto out;

	num_ports = end_port(device) - start_port(device) + 1;

	device->pkey_tbl_len = malloc(sizeof *device->pkey_tbl_len * num_ports,
				       M_DEVBUF, M_NOWAIT);
	device->gid_tbl_len = malloc(sizeof *device->gid_tbl_len * num_ports,
				      M_DEVBUF, M_NOWAIT);
	if (!device->pkey_tbl_len || !device->gid_tbl_len)
		goto err;

	for (port_index = 0; port_index < num_ports; ++port_index) {
		ret = ib_query_port(device, port_index + start_port(device),
					tprops);
		if (ret)
			goto err;
		device->pkey_tbl_len[port_index] = tprops->pkey_tbl_len;
		device->gid_tbl_len[port_index]  = tprops->gid_tbl_len;
	}

	ret = 0;
	goto out;

err:
	free(device->gid_tbl_len, M_DEVBUF);
	free(device->pkey_tbl_len, M_DEVBUF);
out:
	free(tprops, M_DEVBUF);
	return ret;
}

/**
 * ib_register_device - Register an IB device with IB core
 * @device:Device to register
 *
 * Low-level drivers use ib_register_device() to register their
 * devices with the IB core.  All registered clients will receive a
 * callback for each device that is added. @device must be allocated
 * with ib_alloc_device().
 */
int ib_register_device(struct ib_device *device)
{
	int ret;

	mtx_lock(&device_mutex);

	if (strchr(device->name, '%')) {
		ret = alloc_name(device->name);
		if (ret)
			goto out;
	}

	if (ib_device_check_mandatory(device)) {
		ret = EINVAL;
		goto out;
	}

	TAILQ_INIT(&device->event_handler_list);
	TAILQ_INIT(&device->client_data_list);
	mtx_init(&device->event_handler_lock, "ib event handler", NULL, 
		 MTX_DUPOK|MTX_DEF);
	mtx_init(&device->client_data_lock, "ib client data", NULL, 
		 MTX_DUPOK|MTX_DEF);

	ret = read_port_table_lengths(device);
	if (ret) {
		log(LOG_WARNING, "Couldn't create table lengths cache for device %s\n",
		       device->name);
		goto out;
	}

#ifdef notyet
	ret = ib_device_register_sysfs(device);
	if (ret) {
		log(LOG_WARNING, "Couldn't register device %s with driver model\n",
		       device->name);
		free(device->gid_tbl_len, M_DEVBUF);
		free(device->pkey_tbl_len, M_DEVBUF);
		goto out;
	}
#endif

	TAILQ_INSERT_TAIL(&device_list, device, core_list);

	device->reg_state = IB_DEV_REGISTERED;

	{
		struct ib_client *client;

		TAILQ_FOREACH(client, &client_list, list)
			if (client->add && !add_client_context(device, client))
				client->add(device);
	}

 out:
	mtx_unlock(&device_mutex);
	return ret;
}

/**
 * ib_unregister_device - Unregister an IB device
 * @device:Device to unregister
 *
 * Unregister an IB device.  All clients will receive a remove callback.
 */
void ib_unregister_device(struct ib_device *device)
{
	struct ib_client *client;
	struct ib_client_data *context, *tmp;

	mtx_lock(&device_mutex);

	TAILQ_FOREACH_REVERSE(client, &client_list, client_list_s, list)
		if (client->remove)
			client->remove(device);

	TAILQ_REMOVE(&device_list, device, core_list);

	free(device->gid_tbl_len, M_DEVBUF);
	free(device->pkey_tbl_len, M_DEVBUF);

	mtx_unlock(&device_mutex);

	mtx_lock(&device->client_data_lock);
	TAILQ_FOREACH_SAFE(context, &device->client_data_list, list, tmp)
		free(context, M_DEVBUF);
	mtx_unlock(&device->client_data_lock);

	device->reg_state = IB_DEV_UNREGISTERED;
}

/**
 * ib_register_client - Register an IB client
 * @client:Client to register
 *
 * Upper level users of the IB drivers can use ib_register_client() to
 * register callbacks for IB device addition and removal.  When an IB
 * device is added, each registered client's add method will be called
 * (in the order the clients were registered), and when a device is
 * removed, each client's remove method will be called (in the reverse
 * order that clients were registered).  In addition, when
 * ib_register_client() is called, the client will receive an add
 * callback for all devices already registered.
 */
int ib_register_client(struct ib_client *client)
{
	struct ib_device *device;

	mtx_lock(&device_mutex);

	TAILQ_INSERT_TAIL(&client_list, client, list);
	TAILQ_FOREACH(device, &device_list, core_list)
		if (client->add && !add_client_context(device, client))
			client->add(device);

	mtx_unlock(&device_mutex);

	return 0;
}

/**
 * ib_unregister_client - Unregister an IB client
 * @client:Client to unregister
 *
 * Upper level users use ib_unregister_client() to remove their client
 * registration.  When ib_unregister_client() is called, the client
 * will receive a remove callback for each IB device still registered.
 */
void ib_unregister_client(struct ib_client *client)
{
	struct ib_client_data *context, *tmp;
	struct ib_device *device;

	mtx_lock(&device_mutex);

	TAILQ_FOREACH(device, &device_list, core_list) {
		if (client->remove)
			client->remove(device);

		mtx_lock(&device->client_data_lock);
		TAILQ_FOREACH_SAFE(context, &device->client_data_list, list,tmp)
			if (context->client == client) {
				TAILQ_REMOVE(&device->client_data_list, context,
					list);
				free(context, M_DEVBUF);
			}
		mtx_unlock(&device->client_data_lock);
	}
	TAILQ_REMOVE(&client_list, client, list);

	mtx_unlock(&device_mutex);
}

/**
 * ib_get_client_data - Get IB client context
 * @device:Device to get context for
 * @client:Client to get context for
 *
 * ib_get_client_data() returns client context set with
 * ib_set_client_data().
 */
void *ib_get_client_data(struct ib_device *device, struct ib_client *client)
{
	struct ib_client_data *context;
	void *ret = NULL;

	mtx_lock(&device->client_data_lock);
	TAILQ_FOREACH(context, &device->client_data_list, list)
		if (context->client == client) {
			ret = context->data;
			break;
		}
	mtx_unlock(&device->client_data_lock);

	return ret;
}

/**
 * ib_set_client_data - Set IB client context
 * @device:Device to set context for
 * @client:Client to set context for
 * @data:Context to set
 *
 * ib_set_client_data() sets client context that can be retrieved with
 * ib_get_client_data().
 */
void ib_set_client_data(struct ib_device *device, struct ib_client *client,
			void *data)
{
	struct ib_client_data *context;

	mtx_lock(&device->client_data_lock);
	TAILQ_FOREACH(context, &device->client_data_list, list)
		if (context->client == client) {
			context->data = data;
			goto out;
		}

	log(LOG_WARNING, "No client context found for %s/%s\n",
	       device->name, client->name);

out:
	mtx_unlock(&device->client_data_lock);
}

/**
 * ib_register_event_handler - Register an IB event handler
 * @event_handler:Handler to register
 *
 * ib_register_event_handler() registers an event handler that will be
 * called back when asynchronous IB events occur (as defined in
 * chapter 11 of the InfiniBand Architecture Specification).  This
 * callback may occur in interrupt context.
 */
int ib_register_event_handler  (struct ib_event_handler *event_handler)
{
	mtx_lock(&event_handler->device->event_handler_lock);
	TAILQ_INSERT_TAIL(&event_handler->device->event_handler_list, 
		event_handler, list);
	mtx_unlock(&event_handler->device->event_handler_lock);

	return 0;
}

/**
 * ib_unregister_event_handler - Unregister an event handler
 * @event_handler:Handler to unregister
 *
 * Unregister an event handler registered with
 * ib_register_event_handler().
 */
int ib_unregister_event_handler(struct ib_event_handler *event_handler)
{
	mtx_lock(&event_handler->device->event_handler_lock);
	TAILQ_REMOVE(&event_handler->device->event_handler_list, event_handler,
		list);
	mtx_unlock(&event_handler->device->event_handler_lock);

	return 0;
}

/**
 * ib_dispatch_event - Dispatch an asynchronous event
 * @event:Event to dispatch
 *
 * Low-level drivers must call ib_dispatch_event() to dispatch the
 * event to all registered event handlers when an asynchronous event
 * occurs.
 */
void ib_dispatch_event(struct ib_event *event)
{
	struct ib_event_handler *handler;

	mtx_lock(&event->device->event_handler_lock);

	TAILQ_FOREACH(handler, &event->device->event_handler_list, list)
		handler->handler(handler, event);

	mtx_unlock(&event->device->event_handler_lock);
}

/**
 * ib_query_device - Query IB device attributes
 * @device:Device to query
 * @device_attr:Device attributes
 *
 * ib_query_device() returns the attributes of a device through the
 * @device_attr pointer.
 */
int ib_query_device(struct ib_device *device,
		    struct ib_device_attr *device_attr)
{
	return device->query_device(device, device_attr);
}

/**
 * ib_query_port - Query IB port attributes
 * @device:Device to query
 * @port_num:Port number to query
 * @port_attr:Port attributes
 *
 * ib_query_port() returns the attributes of a port through the
 * @port_attr pointer.
 */
int ib_query_port(struct ib_device *device,
		  u8 port_num,
		  struct ib_port_attr *port_attr)
{
	if (port_num < start_port(device) || port_num > end_port(device))
		return (EINVAL);

	return device->query_port(device, port_num, port_attr);
}

/**
 * ib_query_gid - Get GID table entry
 * @device:Device to query
 * @port_num:Port number to query
 * @index:GID table index to query
 * @gid:Returned GID
 *
 * ib_query_gid() fetches the specified GID table entry.
 */
int ib_query_gid(struct ib_device *device,
		 u8 port_num, int index, union ib_gid *gid)
{
	return device->query_gid(device, port_num, index, gid);
}

/**
 * ib_query_pkey - Get P_Key table entry
 * @device:Device to query
 * @port_num:Port number to query
 * @index:P_Key table index to query
 * @pkey:Returned P_Key
 *
 * ib_query_pkey() fetches the specified P_Key table entry.
 */
int ib_query_pkey(struct ib_device *device,
		  u8 port_num, u16 index, u16 *pkey)
{
	return device->query_pkey(device, port_num, index, pkey);
}

/**
 * ib_modify_device - Change IB device attributes
 * @device:Device to modify
 * @device_modify_mask:Mask of attributes to change
 * @device_modify:New attribute values
 *
 * ib_modify_device() changes a device's attributes as specified by
 * the @device_modify_mask and @device_modify structure.
 */
int ib_modify_device(struct ib_device *device,
		     int device_modify_mask,
		     struct ib_device_modify *device_modify)
{
	return device->modify_device(device, device_modify_mask,
				     device_modify);
}

/**
 * ib_modify_port - Modifies the attributes for the specified port.
 * @device: The device to modify.
 * @port_num: The number of the port to modify.
 * @port_modify_mask: Mask used to specify which attributes of the port
 *   to change.
 * @port_modify: New attribute values for the port.
 *
 * ib_modify_port() changes a port's attributes as specified by the
 * @port_modify_mask and @port_modify structure.
 */
int ib_modify_port(struct ib_device *device,
		   u8 port_num, int port_modify_mask,
		   struct ib_port_modify *port_modify)
{
	if (port_num < start_port(device) || port_num > end_port(device))
		return (EINVAL);

	return device->modify_port(device, port_num, port_modify_mask,
				   port_modify);
}

/**
 * ib_find_gid - Returns the port number and GID table index where
 *   a specified GID value occurs.
 * @device: The device to query.
 * @gid: The GID value to search for.
 * @port_num: The port number of the device where the GID value was found.
 * @index: The index into the GID table where the GID was found.  This
 *   parameter may be NULL.
 */
int ib_find_gid(struct ib_device *device, union ib_gid *gid,
		u8 *port_num, u16 *index)
{
	union ib_gid tmp_gid;
	int ret, port, i;

	for (port = start_port(device); port <= end_port(device); ++port) {
		for (i = 0; i < device->gid_tbl_len[port - start_port(device)]; ++i) {
			ret = ib_query_gid(device, port, i, &tmp_gid);
			if (ret)
				return ret;
			if (!memcmp(&tmp_gid, gid, sizeof *gid)) {
				*port_num = port;
				if (index)
					*index = i;
				return 0;
			}
		}
	}

	return (ENOENT);
}

/**
 * ib_find_pkey - Returns the PKey table index where a specified
 *   PKey value occurs.
 * @device: The device to query.
 * @port_num: The port number of the device to search for the PKey.
 * @pkey: The PKey value to search for.
 * @index: The index into the PKey table where the PKey was found.
 */
int ib_find_pkey(struct ib_device *device,
		 u8 port_num, u16 pkey, u16 *index)
{
	int ret, i;
	u16 tmp_pkey;

	for (i = 0; i < device->pkey_tbl_len[port_num - start_port(device)]; ++i) {
		ret = ib_query_pkey(device, port_num, i, &tmp_pkey);
		if (ret)
			return ret;

		if (pkey == tmp_pkey) {
			*index = i;
			return 0;
		}
	}

	return (ENOENT);
}

static int rdma_core_init(void)
{
	int ret;
#ifdef notyet
	ret = ib_sysfs_setup();
	if (ret)
		log(LOG_WARNING, "Couldn't create InfiniBand device class\n");
#endif

	mtx_init(&device_mutex, "rdma_device mutex", NULL, MTX_DEF);
	TAILQ_INIT(&client_list);
	TAILQ_INIT(&device_list);
	ret = ib_cache_setup();
	if (ret) {
		log(LOG_WARNING, "Couldn't set up InfiniBand P_Key/GID cache\n");
#ifdef notyet
		ib_sysfs_cleanup();
#endif
	}

	return ret;
}

static void rdma_core_cleanup(void)
{
	ib_cache_cleanup();
#ifdef notyet
	ib_sysfs_cleanup();
	/* Make sure that any pending umem accounting work is done. */
	flush_scheduled_work();
#endif
}

static int 
rdma_core_load(module_t mod, int cmd, void *arg)
{
        int err = 0;

        switch (cmd) {
        case MOD_LOAD:
                printf("Loading rdma_core.\n");
                rdma_core_init();
                break;
        case MOD_QUIESCE:
                break;
        case MOD_UNLOAD:
                printf("Unloading rdma_core.\n");
		rdma_core_cleanup();
                break;
        case MOD_SHUTDOWN:
                break;
        default:
                err = EOPNOTSUPP;
                break;
        }

        return (err);
}

static moduledata_t mod_data = {
	"rdma_core",
	rdma_core_load,
	0
};

MODULE_VERSION(rdma_core, 1);
DECLARE_MODULE(rdma_core, mod_data, SI_SUB_EXEC, SI_ORDER_ANY);
