/*
 * Copyright (c) 2013,  Mellanox Technologies. All rights reserved.
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

#include <rdma/ib_peer_mem.h>
#include <rdma/ib_verbs.h>
#include <rdma/ib_umem.h>

static DEFINE_MUTEX(peer_memory_mutex);
static LIST_HEAD(peer_memory_list);

static int num_registered_peers;

/* This code uses the sysfs which is not supporeted by the FreeBSD.
 *  * Will be added in future to the sysctl */

#if 0
static struct kobject *peers_kobj;
static struct ib_peer_memory_client *get_peer_by_kobj(void *kobj);
static ssize_t version_show(struct kobject *kobj,
				struct kobj_attribute *attr, char *buf)
{
	struct ib_peer_memory_client *ib_peer_client = get_peer_by_kobj(kobj);

	if (ib_peer_client) {
		sprintf(buf, "%s\n", ib_peer_client->peer_mem->version);
		return strlen(buf);
	}
	/* not found - nothing is return */
	return 0;
}

static ssize_t num_alloc_mrs_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	struct ib_peer_memory_client *ib_peer_client = get_peer_by_kobj(kobj);

	if (ib_peer_client) {
		sprintf(buf, "%lu\n", ib_peer_client->stats.num_alloc_mrs);
		return strlen(buf);
	}
	/* not found - nothing is return */
	return 0;
}

static ssize_t num_reg_pages_show(struct kobject *kobj,
					struct kobj_attribute *attr, char *buf)
{
	struct ib_peer_memory_client *ib_peer_client = get_peer_by_kobj(kobj);

	if (ib_peer_client) {
		sprintf(buf, "%lu\n", ib_peer_client->stats.num_reg_pages);
		return strlen(buf);
	}
	/* not found - nothing is return */
	return 0;
}

static ssize_t num_dereg_pages_show(struct kobject *kobj,
					struct kobj_attribute *attr, char *buf)
{
	struct ib_peer_memory_client *ib_peer_client = get_peer_by_kobj(kobj);

	if (ib_peer_client) {
		sprintf(buf, "%lu\n", ib_peer_client->stats.num_dereg_pages);
		return strlen(buf);
	}
	/* not found - nothing is return */
	return 0;
}

static ssize_t num_free_callbacks_show(struct kobject *kobj,
					struct kobj_attribute *attr, char *buf)
{
	struct ib_peer_memory_client *ib_peer_client = get_peer_by_kobj(kobj);

	if (ib_peer_client) {
		sprintf(buf, "%lu\n", ib_peer_client->stats.num_free_callbacks);
		return strlen(buf);
	}
	/* not found - nothing is return */
	return 0;
}

static struct kobj_attribute version_attr = __ATTR_RO(version);
static struct kobj_attribute num_alloc_mrs = __ATTR_RO(num_alloc_mrs);
static struct kobj_attribute num_reg_pages = __ATTR_RO(num_reg_pages);
static struct kobj_attribute num_dereg_pages = __ATTR_RO(num_dereg_pages);
static struct kobj_attribute num_free_callbacks = __ATTR_RO(num_free_callbacks);

static struct attribute *peer_mem_attrs[] = {
			&version_attr.attr,
			&num_alloc_mrs.attr,
			&num_reg_pages.attr,
			&num_dereg_pages.attr,
			&num_free_callbacks.attr,
			NULL,
};
#endif

#if 0
static void destroy_peer_sysfs(struct ib_peer_memory_client *ib_peer_client)
{
	kobject_put(ib_peer_client->kobj);
	if (!num_registered_peers)
		kobject_put(peers_kobj);

	return;
}

/* This code uses the sysfs which is not supporeted by the FreeBSD.
 * Will be added in future to the sysctl */

static int create_peer_sysfs(struct ib_peer_memory_client *ib_peer_client)
{
	int ret;

	if (!num_registered_peers) {
		/* creating under /sys/kernel/mm */
		peers_kobj = kobject_create_and_add("memory_peers", mm_kobj);
		if (!peers_kobj)
			return -ENOMEM;
	}

	ib_peer_client->peer_mem_attr_group.attrs = peer_mem_attrs;
	/* Dir alreday was created explicitly to get its kernel object for further usage */
	ib_peer_client->peer_mem_attr_group.name =  NULL;
	ib_peer_client->kobj = kobject_create_and_add(ib_peer_client->peer_mem->name,
		peers_kobj);

	if (!ib_peer_client->kobj) {
		ret = -EINVAL;
		goto free;
	}

	/* Create the files associated with this kobject */
	ret = sysfs_create_group(ib_peer_client->kobj,
				&ib_peer_client->peer_mem_attr_group);
	if (ret)
		goto peer_free;

	return 0;

peer_free:
	kobject_put(ib_peer_client->kobj);

free:
	if (!num_registered_peers)
		kobject_put(peers_kobj);

	return ret;
}
#endif

static int ib_invalidate_peer_memory(void *reg_handle,
					  void *core_context)
{
	struct ib_peer_memory_client *ib_peer_client =
		(struct ib_peer_memory_client *)reg_handle;
	struct invalidation_ctx *invalidation_ctx;
	struct core_ticket *core_ticket;
	int need_unlock = 1;

	mutex_lock(&ib_peer_client->lock);
	ib_peer_client->stats.num_free_callbacks += 1;
	core_ticket = ib_peer_search_context(ib_peer_client,
					(unsigned long)core_context);
	if (!core_ticket)
		goto out;

	invalidation_ctx = (struct invalidation_ctx *)core_ticket->context;
	/* If context not ready yet mark to be invalidated */
	if (!invalidation_ctx->func) {
		invalidation_ctx->peer_invalidated = 1;
		goto out;
	}

	invalidation_ctx->func(invalidation_ctx->cookie,
					invalidation_ctx->umem, 0, 0);
	if (invalidation_ctx->inflight_invalidation) {

		/* init the completion to wait on before letting other thread to run */
		init_completion(&invalidation_ctx->comp);
		mutex_unlock(&ib_peer_client->lock);
		need_unlock = 0;
		wait_for_completion(&invalidation_ctx->comp);
	}

	kfree(invalidation_ctx);

out:
	if (need_unlock)
		mutex_unlock(&ib_peer_client->lock);

	return 0;
}

/* access to that peer client is under its lock - no extra lock is needed */
unsigned long ib_peer_insert_context(struct ib_peer_memory_client *ib_peer_client,
				void *context)
{
	struct core_ticket *core_ticket = kzalloc(sizeof(*core_ticket), GFP_KERNEL);

	ib_peer_client->last_ticket++;
	core_ticket->context = context;
	core_ticket->key = ib_peer_client->last_ticket;

	list_add_tail(&core_ticket->ticket_list,
			&ib_peer_client->core_ticket_list);

	return core_ticket->key;
}

int ib_peer_remove_context(struct ib_peer_memory_client *ib_peer_client,
				unsigned long key)
{
	struct core_ticket *core_ticket, *tmp;

	list_for_each_entry_safe(core_ticket, tmp, &ib_peer_client->core_ticket_list,
					ticket_list) {
		if (core_ticket->key == key) {
			list_del(&core_ticket->ticket_list);
			kfree(core_ticket);
			return 0;
		}
	}

	return 1;
}

struct core_ticket *ib_peer_search_context(struct ib_peer_memory_client *ib_peer_client,
						unsigned long key)
{
	struct core_ticket *core_ticket, *tmp;
	list_for_each_entry_safe(core_ticket, tmp, &ib_peer_client->core_ticket_list,
					ticket_list) {
		if (core_ticket->key == key)
			return core_ticket;
	}

	return NULL;
}


static int ib_memory_peer_check_mandatory(struct peer_memory_client
						     *peer_client)
{
#define PEER_MEM_MANDATORY_FUNC(x) {\
	offsetof(struct peer_memory_client, x), #x }

		static const struct {
			size_t offset;
			char  *name;
		} mandatory_table[] = {
			PEER_MEM_MANDATORY_FUNC(acquire),
			PEER_MEM_MANDATORY_FUNC(get_pages),
			PEER_MEM_MANDATORY_FUNC(put_pages),
			PEER_MEM_MANDATORY_FUNC(get_page_size),
			PEER_MEM_MANDATORY_FUNC(dma_map),
			PEER_MEM_MANDATORY_FUNC(dma_unmap)
		};
		int i;

		for (i = 0; i < ARRAY_SIZE(mandatory_table); ++i) {
			if (!*(void **) ((void *) peer_client + mandatory_table[i].offset)) {
				printk(KERN_WARNING "Peer memory %s is missing mandatory function %s\n",
				       peer_client->name, mandatory_table[i].name);
				return -EINVAL;
			}
		}

		return 0;
}



void *ib_register_peer_memory_client(struct peer_memory_client *peer_client,
					   invalidate_peer_memory *invalidate_callback)
{
	int ret = 0;
	struct ib_peer_memory_client *ib_peer_client = NULL;

	mutex_lock(&peer_memory_mutex);
	if (ib_memory_peer_check_mandatory(peer_client)) {
		ret = -EINVAL;
		goto out;
	}

	ib_peer_client = kzalloc(sizeof(*ib_peer_client), GFP_KERNEL);
	if (!ib_peer_client)
		goto out;
	ib_peer_client->peer_mem = peer_client;

	INIT_LIST_HEAD(&ib_peer_client->core_ticket_list);
	mutex_init(&ib_peer_client->lock);
#ifdef __FreeBSD__
	ib_peer_client->holdcount = 0;
	ib_peer_client->needwakeup = 0;
	cv_init(&ib_peer_client->peer_cv, "ibprcl");
#else
	ret = init_srcu_struct(&ib_peer_client->peer_srcu);
	if (ret)
		goto free;
#endif
#if 0
	if (create_peer_sysfs(ib_peer_client))
		goto free;
#endif
	*invalidate_callback = ib_invalidate_peer_memory;
	list_add_tail(&ib_peer_client->core_peer_list, &peer_memory_list);
	num_registered_peers++;
	goto out;
#if 0
free:
	kfree(ib_peer_client);
	ib_peer_client = NULL;
#endif
out:
	mutex_unlock(&peer_memory_mutex);
	return ib_peer_client;
}
EXPORT_SYMBOL(ib_register_peer_memory_client);

void ib_unregister_peer_memory_client(void *reg_handle)
{
	struct ib_peer_memory_client *ib_peer_client =
		(struct ib_peer_memory_client *)reg_handle;

	mutex_lock(&peer_memory_mutex);
	/* remove from list to prevent future core clients usage as it goes down  */
	list_del(&ib_peer_client->core_peer_list);
#ifdef __FreeBSD__
	while (ib_peer_client->holdcount != 0) {
		ib_peer_client->needwakeup = 1;
		cv_wait(&ib_peer_client->peer_cv, &peer_memory_mutex.sx);
	}
	cv_destroy(&ib_peer_client->peer_cv);
#else
	mutex_unlock(&peer_memory_mutex);
	/* peer memory can't go down while there are active clients */
	synchronize_srcu(&ib_peer_client->peer_srcu);
	cleanup_srcu_struct(&ib_peer_client->peer_srcu);
	mutex_lock(&peer_memory_mutex);
#endif
	num_registered_peers--;
/* This code uses the sysfs which is not supporeted by the FreeBSD.
 * Will be added in future to the sysctl */
#if 0
	destroy_peer_sysfs(ib_peer_client);
#endif
	mutex_unlock(&peer_memory_mutex);

	kfree(ib_peer_client);
}
EXPORT_SYMBOL(ib_unregister_peer_memory_client);

/* This code uses the sysfs which is not supporeted by the FreeBSD.
 * Will be added in future to the sysctl */

#if 0
static struct ib_peer_memory_client *get_peer_by_kobj(void *kobj)
{
	struct ib_peer_memory_client *ib_peer_client;

	mutex_lock(&peer_memory_mutex);
	list_for_each_entry(ib_peer_client, &peer_memory_list, core_peer_list) {
		if (ib_peer_client->kobj == kobj)
			goto found;
	}

	ib_peer_client = NULL;

found:

	mutex_unlock(&peer_memory_mutex);
	return ib_peer_client;
}
#endif

struct ib_peer_memory_client *ib_get_peer_client(struct ib_ucontext *context, unsigned long addr,
						size_t size, void **peer_client_context,
						int *srcu_key)
{
	struct ib_peer_memory_client *ib_peer_client;
	int ret;

	mutex_lock(&peer_memory_mutex);
	list_for_each_entry(ib_peer_client, &peer_memory_list, core_peer_list) {
		ret = ib_peer_client->peer_mem->acquire(addr, size,
						   context->peer_mem_private_data,
						   context->peer_mem_name,
						   peer_client_context);
		if (ret == 1)
			goto found;
	}

	ib_peer_client = NULL;

found:
	if (ib_peer_client) {
#ifdef __FreeBSD__
		ib_peer_client->holdcount++;
#else
		*srcu_key = srcu_read_lock(&ib_peer_client->peer_srcu);
#endif
	}

	mutex_unlock(&peer_memory_mutex);
	return ib_peer_client;

}
EXPORT_SYMBOL(ib_get_peer_client);

void ib_put_peer_client(struct ib_peer_memory_client *ib_peer_client,
				void *peer_client_context,
				int srcu_key)
{

	if (ib_peer_client->peer_mem->release)
		ib_peer_client->peer_mem->release(peer_client_context);

#ifdef __FreeBSD__
	ib_peer_client->holdcount--;
	if (ib_peer_client->holdcount == 0 && ib_peer_client->needwakeup) {
		cv_signal(&ib_peer_client->peer_cv);
	}
#else
	srcu_read_unlock(&ib_peer_client->peer_srcu, srcu_key);
#endif
	return;
}
EXPORT_SYMBOL(ib_put_peer_client);

