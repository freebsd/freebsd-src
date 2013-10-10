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

#include <linux/completion.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/pci.h>
#include <linux/bitops.h>
#include <linux/random.h>

#include "sa.h"

MODULE_AUTHOR("Sean Hefty");
MODULE_DESCRIPTION("InfiniBand InformInfo & Notice event handling");
MODULE_LICENSE("Dual BSD/GPL");

static void inform_add_one(struct ib_device *device);
static void inform_remove_one(struct ib_device *device);

static struct ib_client inform_client = {
	.name   = "ib_notice",
	.add    = inform_add_one,
	.remove = inform_remove_one
};

static struct ib_sa_client	sa_client;
static struct workqueue_struct	*inform_wq;

struct inform_device;

struct inform_port {
	struct inform_device	*dev;
	spinlock_t		lock;
	struct rb_root		table;
	atomic_t		refcount;
	struct completion	comp;
	u8			port_num;
};

struct inform_device {
	struct ib_device	*device;
	struct ib_event_handler	event_handler;
	int			start_port;
	int			end_port;
	struct inform_port	port[0];
};

enum inform_state {
	INFORM_IDLE,
	INFORM_REGISTERING,
	INFORM_MEMBER,
	INFORM_BUSY,
	INFORM_ERROR
};

struct inform_member;

struct inform_group {
	u16			trap_number;
	struct rb_node		node;
	struct inform_port	*port;
	spinlock_t		lock;
	struct work_struct	work;
	struct list_head	pending_list;
	struct list_head	active_list;
	struct list_head	notice_list;
	struct inform_member	*last_join;
	int			members;
	enum inform_state	join_state; /* State relative to SA */
	atomic_t		refcount;
	enum inform_state	state;
	struct ib_sa_query	*query;
	int			query_id;
};

struct inform_member {
	struct ib_inform_info	info;
	struct ib_sa_client	*client;
	struct inform_group	*group;
	struct list_head	list;
	enum inform_state	state;
	atomic_t		refcount;
	struct completion	comp;
};

struct inform_notice {
	struct list_head	list;
	struct ib_sa_notice	notice;
};

static void reg_handler(int status, struct ib_sa_inform *inform,
			 void *context);
static void unreg_handler(int status, struct ib_sa_inform *inform,
			  void *context);

static struct inform_group *inform_find(struct inform_port *port,
					u16 trap_number)
{
	struct rb_node *node = port->table.rb_node;
	struct inform_group *group;

	while (node) {
		group = rb_entry(node, struct inform_group, node);
		if (trap_number < group->trap_number)
			node = node->rb_left;
		else if (trap_number > group->trap_number)
			node = node->rb_right;
		else
			return group;
	}
	return NULL;
}

static struct inform_group *inform_insert(struct inform_port *port,
					  struct inform_group *group)
{
	struct rb_node **link = &port->table.rb_node;
	struct rb_node *parent = NULL;
	struct inform_group *cur_group;

	while (*link) {
		parent = *link;
		cur_group = rb_entry(parent, struct inform_group, node);
		if (group->trap_number < cur_group->trap_number)
			link = &(*link)->rb_left;
		else if (group->trap_number > cur_group->trap_number)
			link = &(*link)->rb_right;
		else
			return cur_group;
	}
	rb_link_node(&group->node, parent, link);
	rb_insert_color(&group->node, &port->table);
	return NULL;
}

static void deref_port(struct inform_port *port)
{
	if (atomic_dec_and_test(&port->refcount))
		complete(&port->comp);
}

static void release_group(struct inform_group *group)
{
	struct inform_port *port = group->port;
	unsigned long flags;

	spin_lock_irqsave(&port->lock, flags);
	if (atomic_dec_and_test(&group->refcount)) {
		rb_erase(&group->node, &port->table);
		spin_unlock_irqrestore(&port->lock, flags);
		kfree(group);
		deref_port(port);
	} else
		spin_unlock_irqrestore(&port->lock, flags);
}

static void deref_member(struct inform_member *member)
{
	if (atomic_dec_and_test(&member->refcount))
		complete(&member->comp);
}

static void queue_reg(struct inform_member *member)
{
	struct inform_group *group = member->group;
	unsigned long flags;

	spin_lock_irqsave(&group->lock, flags);
	list_add(&member->list, &group->pending_list);
	if (group->state == INFORM_IDLE) {
		group->state = INFORM_BUSY;
		atomic_inc(&group->refcount);
		queue_work(inform_wq, &group->work);
	}
	spin_unlock_irqrestore(&group->lock, flags);
}

static int send_reg(struct inform_group *group, struct inform_member *member)
{
	struct inform_port *port = group->port;
	struct ib_sa_inform inform;
	int ret;

	memset(&inform, 0, sizeof inform);
	inform.lid_range_begin = cpu_to_be16(0xFFFF);
	inform.is_generic = 1;
	inform.subscribe = 1;
	inform.type = cpu_to_be16(IB_SA_EVENT_TYPE_ALL);
	inform.trap.generic.trap_num = cpu_to_be16(member->info.trap_number);
	inform.trap.generic.resp_time = 19;
	inform.trap.generic.producer_type =
				cpu_to_be32(IB_SA_EVENT_PRODUCER_TYPE_ALL);

	group->last_join = member;
	ret = ib_sa_informinfo_query(&sa_client, port->dev->device,
				     port->port_num, &inform, 3000, GFP_KERNEL,
				     reg_handler, group,&group->query);
	if (ret >= 0) {
		group->query_id = ret;
		ret = 0;
	}
	return ret;
}

static int send_unreg(struct inform_group *group)
{
	struct inform_port *port = group->port;
	struct ib_sa_inform inform;
	int ret;

	memset(&inform, 0, sizeof inform);
	inform.lid_range_begin = cpu_to_be16(0xFFFF);
	inform.is_generic = 1;
	inform.type = cpu_to_be16(IB_SA_EVENT_TYPE_ALL);
	inform.trap.generic.trap_num = cpu_to_be16(group->trap_number);
	inform.trap.generic.qpn = IB_QP1;
	inform.trap.generic.resp_time = 19;
	inform.trap.generic.producer_type =
				cpu_to_be32(IB_SA_EVENT_PRODUCER_TYPE_ALL);

	ret = ib_sa_informinfo_query(&sa_client, port->dev->device,
				     port->port_num, &inform, 3000, GFP_KERNEL,
				     unreg_handler, group, &group->query);
	if (ret >= 0) {
		group->query_id = ret;
		ret = 0;
	}
	return ret;
}

static void join_group(struct inform_group *group, struct inform_member *member)
{
	member->state = INFORM_MEMBER;
	group->members++;
	list_move(&member->list, &group->active_list);
}

static int fail_join(struct inform_group *group, struct inform_member *member,
		     int status)
{
	spin_lock_irq(&group->lock);
	list_del_init(&member->list);
	spin_unlock_irq(&group->lock);
	return member->info.callback(status, &member->info, NULL);
}

static void process_group_error(struct inform_group *group)
{
	struct inform_member *member;
	int ret;

	spin_lock_irq(&group->lock);
	while (!list_empty(&group->active_list)) {
		member = list_entry(group->active_list.next,
				    struct inform_member, list);
		atomic_inc(&member->refcount);
		list_del_init(&member->list);
		group->members--;
		member->state = INFORM_ERROR;
		spin_unlock_irq(&group->lock);

		ret = member->info.callback(-ENETRESET, &member->info, NULL);
		deref_member(member);
		if (ret)
			ib_sa_unregister_inform_info(&member->info);
		spin_lock_irq(&group->lock);
	}

	group->join_state = INFORM_IDLE;
	group->state = INFORM_BUSY;
	spin_unlock_irq(&group->lock);
}

/*
 * Report a notice to all active subscribers.  We use a temporary list to
 * handle unsubscription requests while the notice is being reported, which
 * avoids holding the group lock while in the user's callback.
 */
static void process_notice(struct inform_group *group,
			   struct inform_notice *info_notice)
{
	struct inform_member *member;
	struct list_head list;
	int ret;

	INIT_LIST_HEAD(&list);

	spin_lock_irq(&group->lock);
	list_splice_init(&group->active_list, &list);
	while (!list_empty(&list)) {

		member = list_entry(list.next, struct inform_member, list);
		atomic_inc(&member->refcount);
		list_move(&member->list, &group->active_list);
		spin_unlock_irq(&group->lock);

		ret = member->info.callback(0, &member->info,
					    &info_notice->notice);
		deref_member(member);
		if (ret)
			ib_sa_unregister_inform_info(&member->info);
		spin_lock_irq(&group->lock);
	}
	spin_unlock_irq(&group->lock);
}

static void inform_work_handler(struct work_struct *work)
{
	struct inform_group *group;
	struct inform_member *member;
	struct ib_inform_info *info;
	struct inform_notice *info_notice;
	int status, ret;

	group = container_of(work, typeof(*group), work);
retest:
	spin_lock_irq(&group->lock);
	while (!list_empty(&group->pending_list) ||
	       !list_empty(&group->notice_list) ||
	       (group->state == INFORM_ERROR)) {

		if (group->state == INFORM_ERROR) {
			spin_unlock_irq(&group->lock);
			process_group_error(group);
			goto retest;
		}

		if (!list_empty(&group->notice_list)) {
			info_notice = list_entry(group->notice_list.next,
						 struct inform_notice, list);
			list_del(&info_notice->list);
			spin_unlock_irq(&group->lock);
			process_notice(group, info_notice);
			kfree(info_notice);
			goto retest;
		}

		member = list_entry(group->pending_list.next,
				    struct inform_member, list);
		info = &member->info;
		atomic_inc(&member->refcount);

		if (group->join_state == INFORM_MEMBER) {
			join_group(group, member);
			spin_unlock_irq(&group->lock);
			ret = info->callback(0, info, NULL);
		} else {
			spin_unlock_irq(&group->lock);
			status = send_reg(group, member);
			if (!status) {
				deref_member(member);
				return;
			}
			ret = fail_join(group, member, status);
		}

		deref_member(member);
		if (ret)
			ib_sa_unregister_inform_info(&member->info);
		spin_lock_irq(&group->lock);
	}

	if (!group->members && (group->join_state == INFORM_MEMBER)) {
		group->join_state = INFORM_IDLE;
		spin_unlock_irq(&group->lock);
		if (send_unreg(group))
			goto retest;
	} else {
		group->state = INFORM_IDLE;
		spin_unlock_irq(&group->lock);
		release_group(group);
	}
}

/*
 * Fail a join request if it is still active - at the head of the pending queue.
 */
static void process_join_error(struct inform_group *group, int status)
{
	struct inform_member *member;
	int ret;

	spin_lock_irq(&group->lock);
	member = list_entry(group->pending_list.next,
			    struct inform_member, list);
	if (group->last_join == member) {
		atomic_inc(&member->refcount);
		list_del_init(&member->list);
		spin_unlock_irq(&group->lock);
		ret = member->info.callback(status, &member->info, NULL);
		deref_member(member);
		if (ret)
			ib_sa_unregister_inform_info(&member->info);
	} else
		spin_unlock_irq(&group->lock);
}

static void reg_handler(int status, struct ib_sa_inform *inform, void *context)
{
	struct inform_group *group = context;

	if (status)
		process_join_error(group, status);
	else
		group->join_state = INFORM_MEMBER;

	inform_work_handler(&group->work);
}

static void unreg_handler(int status, struct ib_sa_inform *rec, void *context)
{
	struct inform_group *group = context;

	inform_work_handler(&group->work);
}

int notice_dispatch(struct ib_device *device, u8 port_num,
		    struct ib_sa_notice *notice)
{
	struct inform_device *dev;
	struct inform_port *port;
	struct inform_group *group;
	struct inform_notice *info_notice;

	dev = ib_get_client_data(device, &inform_client);
	if (!dev)
		return 0; /* No one to give notice to. */

	port = &dev->port[port_num - dev->start_port];
	spin_lock_irq(&port->lock);
	group = inform_find(port, __be16_to_cpu(notice->trap.
						generic.trap_num));
	if (!group) {
		spin_unlock_irq(&port->lock);
		return 0;
	}

	atomic_inc(&group->refcount);
	spin_unlock_irq(&port->lock);

	info_notice = kmalloc(sizeof *info_notice, GFP_KERNEL);
	if (!info_notice) {
		release_group(group);
		return -ENOMEM;
	}

	info_notice->notice = *notice;

	spin_lock_irq(&group->lock);
	list_add(&info_notice->list, &group->notice_list);
	if (group->state == INFORM_IDLE) {
		group->state = INFORM_BUSY;
		spin_unlock_irq(&group->lock);
		inform_work_handler(&group->work);
	} else {
		spin_unlock_irq(&group->lock);
		release_group(group);
	}

	return 0;
}

static struct inform_group *acquire_group(struct inform_port *port,
					  u16 trap_number, gfp_t gfp_mask)
{
	struct inform_group *group, *cur_group;
	unsigned long flags;

	spin_lock_irqsave(&port->lock, flags);
	group = inform_find(port, trap_number);
	if (group)
		goto found;
	spin_unlock_irqrestore(&port->lock, flags);

	group = kzalloc(sizeof *group, gfp_mask);
	if (!group)
		return NULL;

	group->port = port;
	group->trap_number = trap_number;
	INIT_LIST_HEAD(&group->pending_list);
	INIT_LIST_HEAD(&group->active_list);
	INIT_LIST_HEAD(&group->notice_list);
	INIT_WORK(&group->work, inform_work_handler);
	spin_lock_init(&group->lock);

	spin_lock_irqsave(&port->lock, flags);
	cur_group = inform_insert(port, group);
	if (cur_group) {
		kfree(group);
		group = cur_group;
	} else
		atomic_inc(&port->refcount);
found:
	atomic_inc(&group->refcount);
	spin_unlock_irqrestore(&port->lock, flags);
	return group;
}

/*
 * We serialize all join requests to a single group to make our lives much
 * easier.  Otherwise, two users could try to join the same group
 * simultaneously, with different configurations, one could leave while the
 * join is in progress, etc., which makes locking around error recovery
 * difficult.
 */
struct ib_inform_info *
ib_sa_register_inform_info(struct ib_sa_client *client,
			   struct ib_device *device, u8 port_num,
			   u16 trap_number, gfp_t gfp_mask,
			   int (*callback)(int status,
					   struct ib_inform_info *info,
					   struct ib_sa_notice *notice),
			   void *context)
{
	struct inform_device *dev;
	struct inform_member *member;
	struct ib_inform_info *info;
	int ret;

	dev = ib_get_client_data(device, &inform_client);
	if (!dev)
		return ERR_PTR(-ENODEV);

	member = kzalloc(sizeof *member, gfp_mask);
	if (!member)
		return ERR_PTR(-ENOMEM);

	ib_sa_client_get(client);
	member->client = client;
	member->info.trap_number = trap_number;
	member->info.callback = callback;
	member->info.context = context;
	init_completion(&member->comp);
	atomic_set(&member->refcount, 1);
	member->state = INFORM_REGISTERING;

	member->group = acquire_group(&dev->port[port_num - dev->start_port],
				      trap_number, gfp_mask);
	if (!member->group) {
		ret = -ENOMEM;
		goto err;
	}

	/*
	 * The user will get the info structure in their callback.  They
	 * could then free the info structure before we can return from
	 * this routine.  So we save the pointer to return before queuing
	 * any callback.
	 */
	info = &member->info;
	queue_reg(member);
	return info;

err:
	ib_sa_client_put(member->client);
	kfree(member);
	return ERR_PTR(ret);
}
EXPORT_SYMBOL(ib_sa_register_inform_info);

void ib_sa_unregister_inform_info(struct ib_inform_info *info)
{
	struct inform_member *member;
	struct inform_group *group;

	member = container_of(info, struct inform_member, info);
	group = member->group;

	spin_lock_irq(&group->lock);
	if (member->state == INFORM_MEMBER)
		group->members--;

	list_del_init(&member->list);

	if (group->state == INFORM_IDLE) {
		group->state = INFORM_BUSY;
		spin_unlock_irq(&group->lock);
		/* Continue to hold reference on group until callback */
		queue_work(inform_wq, &group->work);
	} else {
		spin_unlock_irq(&group->lock);
		release_group(group);
	}

	deref_member(member);
	wait_for_completion(&member->comp);
	ib_sa_client_put(member->client);
	kfree(member);
}
EXPORT_SYMBOL(ib_sa_unregister_inform_info);

static void inform_groups_lost(struct inform_port *port)
{
	struct inform_group *group;
	struct rb_node *node;
	unsigned long flags;

	spin_lock_irqsave(&port->lock, flags);
	for (node = rb_first(&port->table); node; node = rb_next(node)) {
		group = rb_entry(node, struct inform_group, node);
		spin_lock(&group->lock);
		if (group->state == INFORM_IDLE) {
			atomic_inc(&group->refcount);
			queue_work(inform_wq, &group->work);
		}
		group->state = INFORM_ERROR;
		spin_unlock(&group->lock);
	}
	spin_unlock_irqrestore(&port->lock, flags);
}

static void inform_event_handler(struct ib_event_handler *handler,
				struct ib_event *event)
{
	struct inform_device *dev;

	dev = container_of(handler, struct inform_device, event_handler);

	switch (event->event) {
	case IB_EVENT_PORT_ERR:
	case IB_EVENT_LID_CHANGE:
	case IB_EVENT_SM_CHANGE:
	case IB_EVENT_CLIENT_REREGISTER:
		inform_groups_lost(&dev->port[event->element.port_num -
					      dev->start_port]);
		break;
	default:
		break;
	}
}

static void inform_add_one(struct ib_device *device)
{
	struct inform_device *dev;
	struct inform_port *port;
	int i;

	if (rdma_node_get_transport(device->node_type) != RDMA_TRANSPORT_IB)
		return;

	dev = kmalloc(sizeof *dev + device->phys_port_cnt * sizeof *port,
		      GFP_KERNEL);
	if (!dev)
		return;

	if (device->node_type == RDMA_NODE_IB_SWITCH)
		dev->start_port = dev->end_port = 0;
	else {
		dev->start_port = 1;
		dev->end_port = device->phys_port_cnt;
	}

	for (i = 0; i <= dev->end_port - dev->start_port; i++) {
		port = &dev->port[i];
		port->dev = dev;
		port->port_num = dev->start_port + i;
		spin_lock_init(&port->lock);
		port->table = RB_ROOT;
		init_completion(&port->comp);
		atomic_set(&port->refcount, 1);
	}

	dev->device = device;
	ib_set_client_data(device, &inform_client, dev);

	INIT_IB_EVENT_HANDLER(&dev->event_handler, device, inform_event_handler);
	ib_register_event_handler(&dev->event_handler);
}

static void inform_remove_one(struct ib_device *device)
{
	struct inform_device *dev;
	struct inform_port *port;
	int i;

	dev = ib_get_client_data(device, &inform_client);
	if (!dev)
		return;

	ib_unregister_event_handler(&dev->event_handler);
	flush_workqueue(inform_wq);

	for (i = 0; i <= dev->end_port - dev->start_port; i++) {
		port = &dev->port[i];
		deref_port(port);
		wait_for_completion(&port->comp);
	}

	kfree(dev);
}

int notice_init(void)
{
	int ret;

	inform_wq = create_singlethread_workqueue("ib_inform");
	if (!inform_wq)
		return -ENOMEM;

	ib_sa_register_client(&sa_client);

	ret = ib_register_client(&inform_client);
	if (ret)
		goto err;
	return 0;

err:
	ib_sa_unregister_client(&sa_client);
	destroy_workqueue(inform_wq);
	return ret;
}

void notice_cleanup(void)
{
	ib_unregister_client(&inform_client);
	ib_sa_unregister_client(&sa_client);
	destroy_workqueue(inform_wq);
}
