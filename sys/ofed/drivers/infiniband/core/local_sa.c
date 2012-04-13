/*
 * Copyright (c) 2006 Intel Corporation.  All rights reserved.
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

#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/rbtree.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/pci.h>
#include <linux/miscdevice.h>
#include <linux/random.h>

#include <rdma/ib_cache.h>
#include <rdma/ib_sa.h>
#include "sa.h"

MODULE_AUTHOR("Sean Hefty");
MODULE_DESCRIPTION("InfiniBand subnet administration caching");
MODULE_LICENSE("Dual BSD/GPL");

enum {
	SA_DB_MAX_PATHS_PER_DEST = 0x7F,
	SA_DB_MIN_RETRY_TIMER	 = 4000,  /*   4 sec */
	SA_DB_MAX_RETRY_TIMER	 = 256000 /* 256 sec */
};

static int set_paths_per_dest(const char *val, struct kernel_param *kp);
static unsigned long paths_per_dest = 0;
module_param_call(paths_per_dest, set_paths_per_dest, param_get_ulong,
		  &paths_per_dest, 0644);
MODULE_PARM_DESC(paths_per_dest, "Maximum number of paths to retrieve "
				 "to each destination (DGID).  Set to 0 "
				 "to disable cache.");

static int set_subscribe_inform_info(const char *val, struct kernel_param *kp);
static char subscribe_inform_info = 1;
module_param_call(subscribe_inform_info, set_subscribe_inform_info,
		  param_get_bool, &subscribe_inform_info, 0644);
MODULE_PARM_DESC(subscribe_inform_info,
		 "Subscribe for SA InformInfo/Notice events.");

static int do_refresh(const char *val, struct kernel_param *kp);
module_param_call(refresh, do_refresh, NULL, NULL, 0200);

static unsigned long retry_timer = SA_DB_MIN_RETRY_TIMER;

enum sa_db_lookup_method {
	SA_DB_LOOKUP_LEAST_USED,
	SA_DB_LOOKUP_RANDOM
};

static int set_lookup_method(const char *val, struct kernel_param *kp);
static int get_lookup_method(char *buf, struct kernel_param *kp);
static unsigned long lookup_method;
module_param_call(lookup_method, set_lookup_method, get_lookup_method,
		  &lookup_method, 0644);
MODULE_PARM_DESC(lookup_method, "Method used to return path records when "
				"multiple paths exist to a given destination.");

static void sa_db_add_dev(struct ib_device *device);
static void sa_db_remove_dev(struct ib_device *device);

static struct ib_client sa_db_client = {
	.name   = "local_sa",
	.add    = sa_db_add_dev,
	.remove = sa_db_remove_dev
};

static LIST_HEAD(dev_list);
static DEFINE_MUTEX(lock);
static rwlock_t rwlock;
static struct workqueue_struct *sa_wq;
static struct ib_sa_client sa_client;

enum sa_db_state {
	SA_DB_IDLE,
	SA_DB_REFRESH,
	SA_DB_DESTROY
};

struct sa_db_port {
	struct sa_db_device	*dev;
	struct ib_mad_agent	*agent;
	/* Limit number of outstanding MADs to SA to reduce SA flooding */
	struct ib_mad_send_buf	*msg;
	u16			sm_lid;
	u8			sm_sl;
	struct ib_inform_info	*in_info;
	struct ib_inform_info	*out_info;
	struct rb_root		paths;
	struct list_head	update_list;
	unsigned long		update_id;
	enum sa_db_state	state;
	struct work_struct	work;
	union ib_gid		gid;
	int			port_num;
};

struct sa_db_device {
	struct list_head	list;
	struct ib_device	*device;
	struct ib_event_handler event_handler;
	int			start_port;
	int			port_count;
	struct sa_db_port	port[0];
};

struct ib_sa_iterator {
	struct ib_sa_iterator	*next;
};

struct ib_sa_attr_iter {
	struct ib_sa_iterator	*iter;
	unsigned long		flags;
};

struct ib_sa_attr_list {
	struct ib_sa_iterator	iter;
	struct ib_sa_iterator	*tail;
	int			update_id;
	union ib_gid		gid;
	struct rb_node		node;
};

struct ib_path_rec_info {
	struct ib_sa_iterator	iter; /* keep first */
	struct ib_sa_path_rec	rec;
	unsigned long		lookups;
};

struct ib_sa_mad_iter {
	struct ib_mad_recv_wc	*recv_wc;
	struct ib_mad_recv_buf	*recv_buf;
	int			attr_size;
	int			attr_offset;
	int			data_offset;
	int			data_left;
	void			*attr;
	u8			attr_data[0];
};

enum sa_update_type {
	SA_UPDATE_FULL,
	SA_UPDATE_ADD,
	SA_UPDATE_REMOVE
};

struct update_info {
	struct list_head	list;
	union ib_gid		gid;
	enum sa_update_type	type;
};

struct sa_path_request {
	struct work_struct	work;
	struct ib_sa_client	*client;
	void			(*callback)(int, struct ib_sa_path_rec *, void *);
	void			*context;
	struct ib_sa_path_rec	path_rec;
};

static void process_updates(struct sa_db_port *port);

static void free_attr_list(struct ib_sa_attr_list *attr_list)
{
	struct ib_sa_iterator *cur;

	for (cur = attr_list->iter.next; cur; cur = attr_list->iter.next) {
		attr_list->iter.next = cur->next;
		kfree(cur);
	}
	attr_list->tail = &attr_list->iter;
}

static void remove_attr(struct rb_root *root, struct ib_sa_attr_list *attr_list)
{
	rb_erase(&attr_list->node, root);
	free_attr_list(attr_list);
	kfree(attr_list);
}

static void remove_all_attrs(struct rb_root *root)
{
	struct rb_node *node, *next_node;
	struct ib_sa_attr_list *attr_list;

	write_lock_irq(&rwlock);
	for (node = rb_first(root); node; node = next_node) {
		next_node = rb_next(node);
		attr_list = rb_entry(node, struct ib_sa_attr_list, node);
		remove_attr(root, attr_list);
	}
	write_unlock_irq(&rwlock);
}

static void remove_old_attrs(struct rb_root *root, unsigned long update_id)
{
	struct rb_node *node, *next_node;
	struct ib_sa_attr_list *attr_list;

	write_lock_irq(&rwlock);
	for (node = rb_first(root); node; node = next_node) {
		next_node = rb_next(node);
		attr_list = rb_entry(node, struct ib_sa_attr_list, node);
		if (attr_list->update_id != update_id)
			remove_attr(root, attr_list);
	}
	write_unlock_irq(&rwlock);
}

static struct ib_sa_attr_list *insert_attr_list(struct rb_root *root,
						struct ib_sa_attr_list *attr_list)
{
	struct rb_node **link = &root->rb_node;
	struct rb_node *parent = NULL;
	struct ib_sa_attr_list *cur_attr_list;
	int cmp;

	while (*link) {
		parent = *link;
		cur_attr_list = rb_entry(parent, struct ib_sa_attr_list, node);
		cmp = memcmp(&cur_attr_list->gid, &attr_list->gid,
			     sizeof attr_list->gid);
		if (cmp < 0)
			link = &(*link)->rb_left;
		else if (cmp > 0)
			link = &(*link)->rb_right;
		else
			return cur_attr_list;
	}
	rb_link_node(&attr_list->node, parent, link);
	rb_insert_color(&attr_list->node, root);
	return NULL;
}

static struct ib_sa_attr_list *find_attr_list(struct rb_root *root, u8 *gid)
{
	struct rb_node *node = root->rb_node;
	struct ib_sa_attr_list *attr_list;
	int cmp;

	while (node) {
		attr_list = rb_entry(node, struct ib_sa_attr_list, node);
		cmp = memcmp(&attr_list->gid, gid, sizeof attr_list->gid);
		if (cmp < 0)
			node = node->rb_left;
		else if (cmp > 0)
			node = node->rb_right;
		else
			return attr_list;
	}
	return NULL;
}

static int insert_attr(struct rb_root *root, unsigned long update_id, void *key,
		       struct ib_sa_iterator *iter)
{
	struct ib_sa_attr_list *attr_list;
	void *err;

	write_lock_irq(&rwlock);
	attr_list = find_attr_list(root, key);
	if (!attr_list) {
		write_unlock_irq(&rwlock);
		attr_list = kmalloc(sizeof *attr_list, GFP_KERNEL);
		if (!attr_list)
			return -ENOMEM;

		attr_list->iter.next = NULL;
		attr_list->tail = &attr_list->iter;
		attr_list->update_id = update_id;
		memcpy(attr_list->gid.raw, key, sizeof attr_list->gid);

		write_lock_irq(&rwlock);
		err = insert_attr_list(root, attr_list);
		if (err) {
			write_unlock_irq(&rwlock);
			kfree(attr_list);
			return PTR_ERR(err);
		}
	} else if (attr_list->update_id != update_id) {
		free_attr_list(attr_list);
		attr_list->update_id = update_id;
	}

	attr_list->tail->next = iter;
	iter->next = NULL;
	attr_list->tail = iter;
	write_unlock_irq(&rwlock);
	return 0;
}

static struct ib_sa_mad_iter *ib_sa_iter_create(struct ib_mad_recv_wc *mad_recv_wc)
{
	struct ib_sa_mad_iter *iter;
	struct ib_sa_mad *mad = (struct ib_sa_mad *) mad_recv_wc->recv_buf.mad;
	int attr_size, attr_offset;

	attr_offset = be16_to_cpu(mad->sa_hdr.attr_offset) * 8;
	attr_size = 64;		/* path record length */
	if (attr_offset < attr_size)
		return ERR_PTR(-EINVAL);

	iter = kzalloc(sizeof *iter + attr_size, GFP_KERNEL);
	if (!iter)
		return ERR_PTR(-ENOMEM);

	iter->data_left = mad_recv_wc->mad_len - IB_MGMT_SA_HDR;
	iter->recv_wc = mad_recv_wc;
	iter->recv_buf = &mad_recv_wc->recv_buf;
	iter->attr_offset = attr_offset;
	iter->attr_size = attr_size;
	return iter;
}

static void ib_sa_iter_free(struct ib_sa_mad_iter *iter)
{
	kfree(iter);
}

static void *ib_sa_iter_next(struct ib_sa_mad_iter *iter)
{
	struct ib_sa_mad *mad;
	int left, offset = 0;

	while (iter->data_left >= iter->attr_offset) {
		while (iter->data_offset < IB_MGMT_SA_DATA) {
			mad = (struct ib_sa_mad *) iter->recv_buf->mad;

			left = IB_MGMT_SA_DATA - iter->data_offset;
			if (left < iter->attr_size) {
				/* copy first piece of the attribute */
				iter->attr = &iter->attr_data;
				memcpy(iter->attr,
				       &mad->data[iter->data_offset], left);
				offset = left;
				break;
			} else if (offset) {
				/* copy the second piece of the attribute */
				memcpy(iter->attr + offset, &mad->data[0],
				       iter->attr_size - offset);
				iter->data_offset = iter->attr_size - offset;
				offset = 0;
			} else {
				iter->attr = &mad->data[iter->data_offset];
				iter->data_offset += iter->attr_size;
			}

			iter->data_left -= iter->attr_offset;
			goto out;
		}
		iter->data_offset = 0;
		iter->recv_buf = list_entry(iter->recv_buf->list.next,
					    struct ib_mad_recv_buf, list);
	}
	iter->attr = NULL;
out:
	return iter->attr;
}

/*
 * Copy path records from a received response and insert them into our cache.
 * A path record in the MADs are in network order, packed, and may
 * span multiple MAD buffers, just to make our life hard.
 */
static void update_path_db(struct sa_db_port *port,
			   struct ib_mad_recv_wc *mad_recv_wc,
			   enum sa_update_type type)
{
	struct ib_sa_mad_iter *iter;
	struct ib_path_rec_info *path_info;
	void *attr;
	int ret;

	iter = ib_sa_iter_create(mad_recv_wc);
	if (IS_ERR(iter))
		return;

	port->update_id += (type == SA_UPDATE_FULL);

	while ((attr = ib_sa_iter_next(iter)) &&
	       (path_info = kmalloc(sizeof *path_info, GFP_KERNEL))) {

		ib_sa_unpack_attr(&path_info->rec, attr, IB_SA_ATTR_PATH_REC);

		ret = insert_attr(&port->paths, port->update_id,
				  path_info->rec.dgid.raw, &path_info->iter);
		if (ret) {
			kfree(path_info);
			break;
		}
	}
	ib_sa_iter_free(iter);

	if (type == SA_UPDATE_FULL)
		remove_old_attrs(&port->paths, port->update_id);
}

static struct ib_mad_send_buf *get_sa_msg(struct sa_db_port *port,
					  struct update_info *update)
{
	struct ib_ah_attr ah_attr;
	struct ib_mad_send_buf *msg;

	msg = ib_create_send_mad(port->agent, 1, 0, 0, IB_MGMT_SA_HDR,
				 IB_MGMT_SA_DATA, GFP_KERNEL);
	if (IS_ERR(msg))
		return NULL;

	memset(&ah_attr, 0, sizeof ah_attr);
	ah_attr.dlid = port->sm_lid;
	ah_attr.sl = port->sm_sl;
	ah_attr.port_num = port->port_num;

	msg->ah = ib_create_ah(port->agent->qp->pd, &ah_attr);
	if (IS_ERR(msg->ah)) {
		ib_free_send_mad(msg);
		return NULL;
	}

	msg->timeout_ms = retry_timer;
	msg->retries = 0;
	msg->context[0] = port;
	msg->context[1] = update;
	return msg;
}

static __be64 form_tid(u32 hi_tid)
{
	static atomic_t tid;
	return cpu_to_be64((((u64) hi_tid) << 32) |
			   ((u32) atomic_inc_return(&tid)));
}

static void format_path_req(struct sa_db_port *port,
			    struct update_info *update,
			    struct ib_mad_send_buf *msg)
{
	struct ib_sa_mad *mad = msg->mad;
	struct ib_sa_path_rec path_rec;

	mad->mad_hdr.base_version  = IB_MGMT_BASE_VERSION;
	mad->mad_hdr.mgmt_class	   = IB_MGMT_CLASS_SUBN_ADM;
	mad->mad_hdr.class_version = IB_SA_CLASS_VERSION;
	mad->mad_hdr.method	   = IB_SA_METHOD_GET_TABLE;
	mad->mad_hdr.attr_id	   = cpu_to_be16(IB_SA_ATTR_PATH_REC);
	mad->mad_hdr.tid	   = form_tid(msg->mad_agent->hi_tid);

	mad->sa_hdr.comp_mask = IB_SA_PATH_REC_SGID | IB_SA_PATH_REC_NUMB_PATH;

	path_rec.sgid = port->gid;
	path_rec.numb_path = (u8) paths_per_dest;

	if (update->type == SA_UPDATE_ADD) {
		mad->sa_hdr.comp_mask |= IB_SA_PATH_REC_DGID;
		memcpy(&path_rec.dgid, &update->gid, sizeof path_rec.dgid);
	}

	ib_sa_pack_attr(mad->data, &path_rec, IB_SA_ATTR_PATH_REC);
}

static int send_query(struct sa_db_port *port,
		      struct update_info *update)
{
	int ret;

	port->msg = get_sa_msg(port, update);
	if (!port->msg)
		return -ENOMEM;

	format_path_req(port, update, port->msg);

	ret = ib_post_send_mad(port->msg, NULL);
	if (ret)
		goto err;

	return 0;

err:
	ib_destroy_ah(port->msg->ah);
	ib_free_send_mad(port->msg);
	return ret;
}

static void add_update(struct sa_db_port *port, u8 *gid,
		       enum sa_update_type type)
{
	struct update_info *update;

	update = kmalloc(sizeof *update, GFP_KERNEL);
	if (update) {
		if (gid)
			memcpy(&update->gid, gid, sizeof update->gid);
		update->type = type;
		list_add(&update->list, &port->update_list);
	}

	if (port->state == SA_DB_IDLE) {
		port->state = SA_DB_REFRESH;
		process_updates(port);
	}
}

static void clean_update_list(struct sa_db_port *port)
{
	struct update_info *update;

	while (!list_empty(&port->update_list)) {
		update = list_entry(port->update_list.next,
				    struct update_info, list);
		list_del(&update->list);
		kfree(update);
	}
}

static int notice_handler(int status, struct ib_inform_info *info,
			  struct ib_sa_notice *notice)
{
	struct sa_db_port *port = info->context;
	struct ib_sa_notice_data_gid *gid_data;
	struct ib_inform_info **pinfo;
	enum sa_update_type type;

	if (info->trap_number == IB_SA_SM_TRAP_GID_IN_SERVICE) {
		pinfo = &port->in_info;
		type = SA_UPDATE_ADD;
	} else {
		pinfo = &port->out_info;
		type = SA_UPDATE_REMOVE;
	}

	mutex_lock(&lock);
	if (port->state == SA_DB_DESTROY || !*pinfo) {
		mutex_unlock(&lock);
		return 0;
	}

	if (notice) {
		gid_data = (struct ib_sa_notice_data_gid *)
			   &notice->data_details;
		add_update(port, gid_data->gid, type);
		mutex_unlock(&lock);
	} else if (status == -ENETRESET) {
		*pinfo = NULL;
		mutex_unlock(&lock);
	} else {
		if (status)
			*pinfo = ERR_PTR(-EINVAL);
		port->state = SA_DB_IDLE;
		clean_update_list(port);
		mutex_unlock(&lock);
		queue_work(sa_wq, &port->work);
	}

	return status;
}

static int reg_in_info(struct sa_db_port *port)
{
	int ret = 0;

	port->in_info = ib_sa_register_inform_info(&sa_client,
						   port->dev->device,
						   port->port_num,
						   IB_SA_SM_TRAP_GID_IN_SERVICE,
						   GFP_KERNEL, notice_handler,
						   port);
	if (IS_ERR(port->in_info))
		ret = PTR_ERR(port->in_info);

	return ret;
}

static int reg_out_info(struct sa_db_port *port)
{
	int ret = 0;

	port->out_info = ib_sa_register_inform_info(&sa_client,
						    port->dev->device,
						    port->port_num,
						    IB_SA_SM_TRAP_GID_OUT_OF_SERVICE,
						    GFP_KERNEL, notice_handler,
						    port);
	if (IS_ERR(port->out_info))
		ret = PTR_ERR(port->out_info);

	return ret;
}

static void unsubscribe_port(struct sa_db_port *port)
{
	if (port->in_info && !IS_ERR(port->in_info))
		ib_sa_unregister_inform_info(port->in_info);

	if (port->out_info && !IS_ERR(port->out_info))
		ib_sa_unregister_inform_info(port->out_info);

	port->out_info = NULL;
	port->in_info = NULL;

}

static void cleanup_port(struct sa_db_port *port)
{
	unsubscribe_port(port);

	clean_update_list(port);
	remove_all_attrs(&port->paths);
}

static int update_port_info(struct sa_db_port *port)
{
	struct ib_port_attr port_attr;
	int ret;

	ret = ib_query_port(port->dev->device, port->port_num, &port_attr);
	if (ret)
		return ret;

	if (port_attr.state != IB_PORT_ACTIVE)
		return -ENODATA;

        port->sm_lid = port_attr.sm_lid;
	port->sm_sl = port_attr.sm_sl;
	return 0;
}

static void process_updates(struct sa_db_port *port)
{
	struct update_info *update;
	struct ib_sa_attr_list *attr_list;
	int ret;

	if (!paths_per_dest || update_port_info(port)) {
		cleanup_port(port);
		goto out;
	}

	/* Event registration is an optimization, so ignore failures. */
	if (subscribe_inform_info) {
		if (!port->out_info) {
			ret = reg_out_info(port);
			if (!ret)
				return;
		}

		if (!port->in_info) {
			ret = reg_in_info(port);
			if (!ret)
				return;
		}
	} else
		unsubscribe_port(port);

	while (!list_empty(&port->update_list)) {
		update = list_entry(port->update_list.next,
				    struct update_info, list);

		if (update->type == SA_UPDATE_REMOVE) {
			write_lock_irq(&rwlock);
			attr_list = find_attr_list(&port->paths,
						   update->gid.raw);
			if (attr_list)
				remove_attr(&port->paths, attr_list);
			write_unlock_irq(&rwlock);
		} else {
			ret = send_query(port, update);
			if (!ret)
				return;

		}
		list_del(&update->list);
		kfree(update);
	}
out:
	port->state = SA_DB_IDLE;
}

static void refresh_port_db(struct sa_db_port *port)
{
	if (port->state == SA_DB_DESTROY)
		return;

	if (port->state == SA_DB_REFRESH) {
		clean_update_list(port);
		ib_cancel_mad(port->agent, port->msg);
	}

	add_update(port, NULL, SA_UPDATE_FULL);
}

static void refresh_dev_db(struct sa_db_device *dev)
{
	int i;

	for (i = 0; i < dev->port_count; i++)
		refresh_port_db(&dev->port[i]);
}

static void refresh_db(void)
{
	struct sa_db_device *dev;

	list_for_each_entry(dev, &dev_list, list)
		refresh_dev_db(dev);
}

static int do_refresh(const char *val, struct kernel_param *kp)
{
	mutex_lock(&lock);
	refresh_db();
	mutex_unlock(&lock);
	return 0;
}

static int get_lookup_method(char *buf, struct kernel_param *kp)
{
	return sprintf(buf,
		       "%c %d round robin\n"
		       "%c %d random",
		       (lookup_method == SA_DB_LOOKUP_LEAST_USED) ? '*' : ' ',
		       SA_DB_LOOKUP_LEAST_USED,
		       (lookup_method == SA_DB_LOOKUP_RANDOM) ? '*' : ' ',
		       SA_DB_LOOKUP_RANDOM);
}

static int set_lookup_method(const char *val, struct kernel_param *kp)
{
	unsigned long method;
	int ret = 0;

	method = simple_strtoul(val, NULL, 0);

	switch (method) {
	case SA_DB_LOOKUP_LEAST_USED:
	case SA_DB_LOOKUP_RANDOM:
		lookup_method = method;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int set_paths_per_dest(const char *val, struct kernel_param *kp)
{
	int ret;

	mutex_lock(&lock);
	ret = param_set_ulong(val, kp);
	if (ret)
		goto out;

	if (paths_per_dest > SA_DB_MAX_PATHS_PER_DEST)
		paths_per_dest = SA_DB_MAX_PATHS_PER_DEST;
	refresh_db();
out:
	mutex_unlock(&lock);
	return ret;
}

static int set_subscribe_inform_info(const char *val, struct kernel_param *kp)
{
	int ret;

	ret = param_set_bool(val, kp);
	if (ret)
		return ret;

	return do_refresh(val, kp);
}

static void port_work_handler(struct work_struct *work)
{
	struct sa_db_port *port;

	port = container_of(work, typeof(*port), work);
	mutex_lock(&lock);
	refresh_port_db(port);
	mutex_unlock(&lock);
}

static void handle_event(struct ib_event_handler *event_handler,
			 struct ib_event *event)
{
	struct sa_db_device *dev;
	struct sa_db_port *port;

	dev = container_of(event_handler, typeof(*dev), event_handler);
	port = &dev->port[event->element.port_num - dev->start_port];

	switch (event->event) {
	case IB_EVENT_PORT_ERR:
	case IB_EVENT_LID_CHANGE:
	case IB_EVENT_SM_CHANGE:
	case IB_EVENT_CLIENT_REREGISTER:
	case IB_EVENT_PKEY_CHANGE:
	case IB_EVENT_PORT_ACTIVE:
		queue_work(sa_wq, &port->work);
		break;
	default:
		break;
	}
}

static void ib_free_path_iter(struct ib_sa_attr_iter *iter)
{
	read_unlock_irqrestore(&rwlock, iter->flags);
}

static int ib_create_path_iter(struct ib_device *device, u8 port_num,
			       union ib_gid *dgid, struct ib_sa_attr_iter *iter)
{
	struct sa_db_device *dev;
	struct sa_db_port *port;
	struct ib_sa_attr_list *list;

	dev = ib_get_client_data(device, &sa_db_client);
	if (!dev)
		return -ENODEV;

	port = &dev->port[port_num - dev->start_port];

	read_lock_irqsave(&rwlock, iter->flags);
	list = find_attr_list(&port->paths, dgid->raw);
	if (!list) {
		ib_free_path_iter(iter);
		return -ENODATA;
	}

	iter->iter = &list->iter;
	return 0;
}

static struct ib_sa_path_rec *ib_get_next_path(struct ib_sa_attr_iter *iter)
{
	struct ib_path_rec_info *next_path;

	iter->iter = iter->iter->next;
	if (iter->iter) {
		next_path = container_of(iter->iter, struct ib_path_rec_info, iter);
		return &next_path->rec;
	} else
		return NULL;
}

static int cmp_rec(struct ib_sa_path_rec *src,
		   struct ib_sa_path_rec *dst, ib_sa_comp_mask comp_mask)
{
	/* DGID check already done */
	if (comp_mask & IB_SA_PATH_REC_SGID &&
	    memcmp(&src->sgid, &dst->sgid, sizeof src->sgid))
		return -EINVAL;
	if (comp_mask & IB_SA_PATH_REC_DLID && src->dlid != dst->dlid)
		return -EINVAL;
	if (comp_mask & IB_SA_PATH_REC_SLID && src->slid != dst->slid)
		return -EINVAL;
	if (comp_mask & IB_SA_PATH_REC_RAW_TRAFFIC &&
	    src->raw_traffic != dst->raw_traffic)
		return -EINVAL;

	if (comp_mask & IB_SA_PATH_REC_FLOW_LABEL &&
	    src->flow_label != dst->flow_label)
		return -EINVAL;
	if (comp_mask & IB_SA_PATH_REC_HOP_LIMIT &&
	    src->hop_limit != dst->hop_limit)
		return -EINVAL;
	if (comp_mask & IB_SA_PATH_REC_TRAFFIC_CLASS &&
	    src->traffic_class != dst->traffic_class)
		return -EINVAL;
	if (comp_mask & IB_SA_PATH_REC_REVERSIBLE &&
	    dst->reversible && !src->reversible)
		return -EINVAL;
	/* Numb path check already done */
	if (comp_mask & IB_SA_PATH_REC_PKEY && src->pkey != dst->pkey)
		return -EINVAL;

	if (comp_mask & IB_SA_PATH_REC_SL && src->sl != dst->sl)
		return -EINVAL;

	if (ib_sa_check_selector(comp_mask, IB_SA_PATH_REC_MTU_SELECTOR,
				 IB_SA_PATH_REC_MTU, dst->mtu_selector,
				 src->mtu, dst->mtu))
		return -EINVAL;
	if (ib_sa_check_selector(comp_mask, IB_SA_PATH_REC_RATE_SELECTOR,
				 IB_SA_PATH_REC_RATE, dst->rate_selector,
				 src->rate, dst->rate))
		return -EINVAL;
	if (ib_sa_check_selector(comp_mask,
				 IB_SA_PATH_REC_PACKET_LIFE_TIME_SELECTOR,
				 IB_SA_PATH_REC_PACKET_LIFE_TIME,
				 dst->packet_life_time_selector,
				 src->packet_life_time, dst->packet_life_time))
		return -EINVAL;

	return 0;
}

static struct ib_sa_path_rec *get_random_path(struct ib_sa_attr_iter *iter,
					      struct ib_sa_path_rec *req_path,
					      ib_sa_comp_mask comp_mask)
{
	struct ib_sa_path_rec *path, *rand_path = NULL;
	int num, count = 0;

	for (path = ib_get_next_path(iter); path;
	     path = ib_get_next_path(iter)) {
		if (!cmp_rec(path, req_path, comp_mask)) {
			get_random_bytes(&num, sizeof num);
			if ((num % ++count) == 0)
				rand_path = path;
		}
	}

	return rand_path;
}

static struct ib_sa_path_rec *get_next_path(struct ib_sa_attr_iter *iter,
					    struct ib_sa_path_rec *req_path,
					    ib_sa_comp_mask comp_mask)
{
	struct ib_path_rec_info *cur_path, *next_path = NULL;
	struct ib_sa_path_rec *path;
	unsigned long lookups = ~0;

	for (path = ib_get_next_path(iter); path;
	     path = ib_get_next_path(iter)) {
		if (!cmp_rec(path, req_path, comp_mask)) {

			cur_path = container_of(iter->iter, struct ib_path_rec_info,
						iter);
			if (cur_path->lookups < lookups) {
				lookups = cur_path->lookups;
				next_path = cur_path;
			}
		}
	}

	if (next_path) {
		next_path->lookups++;
		return &next_path->rec;
	} else
		return NULL;
}

static void report_path(struct work_struct *work)
{
	struct sa_path_request *req;

	req = container_of(work, struct sa_path_request, work);
	req->callback(0, &req->path_rec, req->context);
	ib_sa_client_put(req->client);
	kfree(req);
}

/**
 * ib_sa_path_rec_get - Start a Path get query
 * @client:SA client
 * @device:device to send query on
 * @port_num: port number to send query on
 * @rec:Path Record to send in query
 * @comp_mask:component mask to send in query
 * @timeout_ms:time to wait for response
 * @gfp_mask:GFP mask to use for internal allocations
 * @callback:function called when query completes, times out or is
 * canceled
 * @context:opaque user context passed to callback
 * @sa_query:query context, used to cancel query
 *
 * Send a Path Record Get query to the SA to look up a path.  The
 * callback function will be called when the query completes (or
 * fails); status is 0 for a successful response, -EINTR if the query
 * is canceled, -ETIMEDOUT is the query timed out, or -EIO if an error
 * occurred sending the query.  The resp parameter of the callback is
 * only valid if status is 0.
 *
 * If the return value of ib_sa_path_rec_get() is negative, it is an
 * error code.  Otherwise it is a query ID that can be used to cancel
 * the query.
 */
int ib_sa_path_rec_get(struct ib_sa_client *client,
		       struct ib_device *device, u8 port_num,
		       struct ib_sa_path_rec *rec,
		       ib_sa_comp_mask comp_mask,
		       int timeout_ms, gfp_t gfp_mask,
		       void (*callback)(int status,
					struct ib_sa_path_rec *resp,
					void *context),
		       void *context,
		       struct ib_sa_query **sa_query)
{
	struct sa_path_request *req;
	struct ib_sa_attr_iter iter;
	struct ib_sa_path_rec *path_rec;
	int ret;

	if (!paths_per_dest)
		goto query_sa;

	if (!(comp_mask & IB_SA_PATH_REC_DGID) ||
	    !(comp_mask & IB_SA_PATH_REC_NUMB_PATH) || rec->numb_path != 1)
		goto query_sa;

	req = kmalloc(sizeof *req, gfp_mask);
	if (!req)
		goto query_sa;

	ret = ib_create_path_iter(device, port_num, &rec->dgid, &iter);
	if (ret)
		goto free_req;

	if (lookup_method == SA_DB_LOOKUP_RANDOM)
		path_rec = get_random_path(&iter, rec, comp_mask);
	else
		path_rec = get_next_path(&iter, rec, comp_mask);

	if (!path_rec)
		goto free_iter;

	memcpy(&req->path_rec, path_rec, sizeof *path_rec);
	ib_free_path_iter(&iter);

	INIT_WORK(&req->work, report_path);
	req->client = client;
	req->callback = callback;
	req->context = context;

	ib_sa_client_get(client);
	queue_work(sa_wq, &req->work);
	*sa_query = ERR_PTR(-EEXIST);
	return 0;

free_iter:
	ib_free_path_iter(&iter);
free_req:
	kfree(req);
query_sa:
	return ib_sa_path_rec_query(client, device, port_num, rec, comp_mask,
				    timeout_ms, gfp_mask, callback, context,
				    sa_query);
}
EXPORT_SYMBOL(ib_sa_path_rec_get);

static void recv_handler(struct ib_mad_agent *mad_agent,
			 struct ib_mad_recv_wc *mad_recv_wc)
{
	struct sa_db_port *port;
	struct update_info *update;
	struct ib_mad_send_buf *msg;
	enum sa_update_type type;

	msg = (struct ib_mad_send_buf *) (unsigned long) mad_recv_wc->wc->wr_id;
	port = msg->context[0];
	update = msg->context[1];

	mutex_lock(&lock);
	if (port->state == SA_DB_DESTROY ||
	    update != list_entry(port->update_list.next,
				 struct update_info, list)) {
		mutex_unlock(&lock);
	} else {
		type = update->type;
		mutex_unlock(&lock);
		update_path_db(mad_agent->context, mad_recv_wc, type);
	}

	ib_free_recv_mad(mad_recv_wc);
}

static void send_handler(struct ib_mad_agent *agent,
			 struct ib_mad_send_wc *mad_send_wc)
{
	struct ib_mad_send_buf *msg;
	struct sa_db_port *port;
	struct update_info *update;
	int ret;

	msg = mad_send_wc->send_buf;
	port = msg->context[0];
	update = msg->context[1];

	mutex_lock(&lock);
	if (port->state == SA_DB_DESTROY)
		goto unlock;

	if (update == list_entry(port->update_list.next,
				 struct update_info, list)) {

		if (mad_send_wc->status == IB_WC_RESP_TIMEOUT_ERR &&
		    msg->timeout_ms < SA_DB_MAX_RETRY_TIMER) {

			msg->timeout_ms <<= 1;
			ret = ib_post_send_mad(msg, NULL);
			if (!ret) {
				mutex_unlock(&lock);
				return;
			}
		}
		list_del(&update->list);
		kfree(update);
	}
	process_updates(port);
unlock:
	mutex_unlock(&lock);

	ib_destroy_ah(msg->ah);
	ib_free_send_mad(msg);
}

static int init_port(struct sa_db_device *dev, int port_num)
{
	struct sa_db_port *port;
	int ret;

	port = &dev->port[port_num - dev->start_port];
	port->dev = dev;
	port->port_num = port_num;
	INIT_WORK(&port->work, port_work_handler);
	port->paths = RB_ROOT;
	INIT_LIST_HEAD(&port->update_list);

	ret = ib_get_cached_gid(dev->device, port_num, 0, &port->gid);
	if (ret)
		return ret;

	port->agent = ib_register_mad_agent(dev->device, port_num, IB_QPT_GSI,
					    NULL, IB_MGMT_RMPP_VERSION,
					    send_handler, recv_handler, port);
	if (IS_ERR(port->agent))
		ret = PTR_ERR(port->agent);

	return ret;
}

static void destroy_port(struct sa_db_port *port)
{
	mutex_lock(&lock);
	port->state = SA_DB_DESTROY;
	mutex_unlock(&lock);

	ib_unregister_mad_agent(port->agent);
	cleanup_port(port);
	flush_workqueue(sa_wq);
}

static void sa_db_add_dev(struct ib_device *device)
{
	struct sa_db_device *dev;
	struct sa_db_port *port;
	int s, e, i, ret;

	if (rdma_node_get_transport(device->node_type) != RDMA_TRANSPORT_IB)
		return;

	if (device->node_type == RDMA_NODE_IB_SWITCH) {
		s = e = 0;
	} else {
		s = 1;
		e = device->phys_port_cnt;
	}

	dev = kzalloc(sizeof *dev + (e - s + 1) * sizeof *port, GFP_KERNEL);
	if (!dev)
		return;

	dev->start_port = s;
	dev->port_count = e - s + 1;
	dev->device = device;
	for (i = 0; i < dev->port_count; i++) {
		ret = init_port(dev, s + i);
		if (ret)
			goto err;
	}

	ib_set_client_data(device, &sa_db_client, dev);

	INIT_IB_EVENT_HANDLER(&dev->event_handler, device, handle_event);

	mutex_lock(&lock);
	list_add_tail(&dev->list, &dev_list);
	refresh_dev_db(dev);
	mutex_unlock(&lock);

	ib_register_event_handler(&dev->event_handler);
	return;
err:
	while (i--)
		destroy_port(&dev->port[i]);
	kfree(dev);
}

static void sa_db_remove_dev(struct ib_device *device)
{
	struct sa_db_device *dev;
	int i;

	dev = ib_get_client_data(device, &sa_db_client);
	if (!dev)
		return;

	ib_unregister_event_handler(&dev->event_handler);
	flush_workqueue(sa_wq);

	for (i = 0; i < dev->port_count; i++)
		destroy_port(&dev->port[i]);

	mutex_lock(&lock);
	list_del(&dev->list);
	mutex_unlock(&lock);

	kfree(dev);
}

int sa_db_init(void)
{
	int ret;

	rwlock_init(&rwlock);
	sa_wq = create_singlethread_workqueue("local_sa");
	if (!sa_wq)
		return -ENOMEM;

	ib_sa_register_client(&sa_client);
	ret = ib_register_client(&sa_db_client);
	if (ret)
		goto err;

	return 0;

err:
	ib_sa_unregister_client(&sa_client);
	destroy_workqueue(sa_wq);
	return ret;
}

void sa_db_cleanup(void)
{
	ib_unregister_client(&sa_db_client);
	ib_sa_unregister_client(&sa_client);
	destroy_workqueue(sa_wq);
}
