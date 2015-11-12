/*
 * Copyright (c) 2005 Topspin Communications.  All rights reserved.
 * Copyright (c) 2005, 2006, 2007 Cisco Systems.  All rights reserved.
 * Copyright (c) 2005 PathScale, Inc.  All rights reserved.
 * Copyright (c) 2006 Mellanox Technologies.  All rights reserved.
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

#include <linux/file.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/moduleparam.h>
#include <linux/rbtree.h>
#include <linux/lockdep.h>
#include <rdma/ib_addr.h>

#include <asm/uaccess.h>
#include <asm/fcntl.h>
#include <sys/priv.h>

#include "uverbs.h"

static int disable_raw_qp_enforcement;
module_param_named(disable_raw_qp_enforcement, disable_raw_qp_enforcement, int,
		   0444);
MODULE_PARM_DESC(disable_raw_qp_enforcement,  "Disable RAW QP enforcement for "
		 "being opened by root (default: 0)");

struct uverbs_lock_class {
	struct lock_class_key	key;
	char			name[16];
};

static struct uverbs_lock_class pd_lock_class	= { .name = "PD-uobj" };
static struct uverbs_lock_class mr_lock_class	= { .name = "MR-uobj" };
static struct uverbs_lock_class mw_lock_class	= { .name = "MW-uobj" };
static struct uverbs_lock_class cq_lock_class	= { .name = "CQ-uobj" };
static struct uverbs_lock_class qp_lock_class	= { .name = "QP-uobj" };
static struct uverbs_lock_class ah_lock_class	= { .name = "AH-uobj" };
static struct uverbs_lock_class srq_lock_class	= { .name = "SRQ-uobj" };
static struct uverbs_lock_class xrcd_lock_class = { .name = "XRCD-uobj" };
static struct uverbs_lock_class dct_lock_class	= { .name = "DCT-uobj" };

static int uverbs_copy_from_udata(void *dest, struct ib_udata *udata, size_t len)
{
	return copy_from_user(dest, udata->inbuf, len) ? -EFAULT : 0;
}

static int uverbs_copy_to_udata(struct ib_udata *udata, void *src, size_t len)
{
	return copy_to_user(udata->outbuf, src, len) ? -EFAULT : 0;
}

static struct ib_udata_ops uverbs_copy = {
	.copy_from = uverbs_copy_from_udata,
	.copy_to   = uverbs_copy_to_udata
};

#define INIT_UDATA(udata, ibuf, obuf, ilen, olen)			\
	do {								\
		(udata)->ops    = &uverbs_copy;				\
		(udata)->inbuf  = (void __user *) (ibuf);		\
		(udata)->outbuf = (void __user *) (obuf);		\
		(udata)->inlen  = (ilen);				\
		(udata)->outlen = (olen);				\
	} while (0)

enum uverbs_cmd_type {
	IB_USER_VERBS_CMD_BASIC,
	IB_USER_VERBS_CMD_EXTENDED
};

/*
 * The ib_uobject locking scheme is as follows:
 *
 * - ib_uverbs_idr_lock protects the uverbs idrs themselves, so it
 *   needs to be held during all idr operations.  When an object is
 *   looked up, a reference must be taken on the object's kref before
 *   dropping this lock.
 *
 * - Each object also has an rwsem.  This rwsem must be held for
 *   reading while an operation that uses the object is performed.
 *   For example, while registering an MR, the associated PD's
 *   uobject.mutex must be held for reading.  The rwsem must be held
 *   for writing while initializing or destroying an object.
 *
 * - In addition, each object has a "live" flag.  If this flag is not
 *   set, then lookups of the object will fail even if it is found in
 *   the idr.  This handles a reader that blocks and does not acquire
 *   the rwsem until after the object is destroyed.  The destroy
 *   operation will set the live flag to 0 and then drop the rwsem;
 *   this will allow the reader to acquire the rwsem, see that the
 *   live flag is 0, and then drop the rwsem and its reference to
 *   object.  The underlying storage will not be freed until the last
 *   reference to the object is dropped.
 */

static void init_uobj(struct ib_uobject *uobj, u64 user_handle,
		      struct ib_ucontext *context, struct uverbs_lock_class *c)
{
	uobj->user_handle = user_handle;
	uobj->context     = context;
	kref_init(&uobj->ref);
	init_rwsem(&uobj->mutex);
	lockdep_set_class_and_name(&uobj->mutex, &c->key, c->name);
	uobj->live        = 0;
}

static void release_uobj(struct kref *kref)
{
	kfree(container_of(kref, struct ib_uobject, ref));
}

static void put_uobj(struct ib_uobject *uobj)
{
	kref_put(&uobj->ref, release_uobj);
}

static void put_uobj_read(struct ib_uobject *uobj)
{
	up_read(&uobj->mutex);
	put_uobj(uobj);
}

static void put_uobj_write(struct ib_uobject *uobj)
{
	up_write(&uobj->mutex);
	put_uobj(uobj);
}

static int idr_add_uobj(struct idr *idr, struct ib_uobject *uobj)
{
	int ret;

retry:
	if (!idr_pre_get(idr, GFP_KERNEL))
		return -ENOMEM;

	spin_lock(&ib_uverbs_idr_lock);
	ret = idr_get_new(idr, uobj, &uobj->id);
	spin_unlock(&ib_uverbs_idr_lock);

	if (ret == -EAGAIN)
		goto retry;

	return ret;
}

void idr_remove_uobj(struct idr *idr, struct ib_uobject *uobj)
{
	spin_lock(&ib_uverbs_idr_lock);
	idr_remove(idr, uobj->id);
	spin_unlock(&ib_uverbs_idr_lock);
}

static struct ib_uobject *__idr_get_uobj(struct idr *idr, int id,
					 struct ib_ucontext *context)
{
	struct ib_uobject *uobj;

	spin_lock(&ib_uverbs_idr_lock);
	uobj = idr_find(idr, id);
	if (uobj) {
		if (uobj->context == context)
			kref_get(&uobj->ref);
		else
			uobj = NULL;
	}
	spin_unlock(&ib_uverbs_idr_lock);

	return uobj;
}

static struct ib_uobject *idr_read_uobj(struct idr *idr, int id,
					struct ib_ucontext *context, int nested)
{
	struct ib_uobject *uobj;

	uobj = __idr_get_uobj(idr, id, context);
	if (!uobj)
		return NULL;

	if (nested)
		down_read_nested(&uobj->mutex, SINGLE_DEPTH_NESTING);
	else
		down_read(&uobj->mutex);
	if (!uobj->live) {
		put_uobj_read(uobj);
		return NULL;
	}

	return uobj;
}

static struct ib_uobject *idr_write_uobj(struct idr *idr, int id,
					 struct ib_ucontext *context)
{
	struct ib_uobject *uobj;

	uobj = __idr_get_uobj(idr, id, context);
	if (!uobj)
		return NULL;

	down_write(&uobj->mutex);
	if (!uobj->live) {
		put_uobj_write(uobj);
		return NULL;
	}

	return uobj;
}

static void *idr_read_obj(struct idr *idr, int id, struct ib_ucontext *context,
			  int nested)
{
	struct ib_uobject *uobj;

	uobj = idr_read_uobj(idr, id, context, nested);
	return uobj ? uobj->object : NULL;
}

static struct ib_pd *idr_read_pd(int pd_handle, struct ib_ucontext *context)
{
	return idr_read_obj(&ib_uverbs_pd_idr, pd_handle, context, 0);
}

static void put_pd_read(struct ib_pd *pd)
{
	put_uobj_read(pd->uobject);
}

static struct ib_cq *idr_read_cq(int cq_handle, struct ib_ucontext *context, int nested)
{
	return idr_read_obj(&ib_uverbs_cq_idr, cq_handle, context, nested);
}

static void put_cq_read(struct ib_cq *cq)
{
	put_uobj_read(cq->uobject);
}

static struct ib_ah *idr_read_ah(int ah_handle, struct ib_ucontext *context)
{
	return idr_read_obj(&ib_uverbs_ah_idr, ah_handle, context, 0);
}

static void put_ah_read(struct ib_ah *ah)
{
	put_uobj_read(ah->uobject);
}

static struct ib_qp *idr_read_qp(int qp_handle, struct ib_ucontext *context)
{
	return idr_read_obj(&ib_uverbs_qp_idr, qp_handle, context, 0);
}

static struct ib_qp *idr_write_qp(int qp_handle, struct ib_ucontext *context)
{
	struct ib_uobject *uobj;

	uobj = idr_write_uobj(&ib_uverbs_qp_idr, qp_handle, context);
	return uobj ? uobj->object : NULL;
}

static void put_qp_read(struct ib_qp *qp)
{
	put_uobj_read(qp->uobject);
}

static void put_qp_write(struct ib_qp *qp)
{
	put_uobj_write(qp->uobject);
}

static struct ib_dct *idr_read_dct(int dct_handle, struct ib_ucontext *context)
{
	return idr_read_obj(&ib_uverbs_dct_idr, dct_handle, context, 0);
}

static void put_dct_read(struct ib_dct *dct)
{
	put_uobj_read(dct->uobject);
}

static struct ib_srq *idr_read_srq(int srq_handle, struct ib_ucontext *context)
{
	return idr_read_obj(&ib_uverbs_srq_idr, srq_handle, context, 0);
}

static void put_srq_read(struct ib_srq *srq)
{
	put_uobj_read(srq->uobject);
}

static struct ib_xrcd *idr_read_xrcd(int xrcd_handle, struct ib_ucontext *context,
				     struct ib_uobject **uobj)
{
	*uobj = idr_read_uobj(&ib_uverbs_xrcd_idr, xrcd_handle, context, 0);
	return *uobj ? (*uobj)->object : NULL;
}

static void put_xrcd_read(struct ib_uobject *uobj)
{
	put_uobj_read(uobj);
}

ssize_t ib_uverbs_get_context(struct ib_uverbs_file *file,
			      const char __user *buf,
			      int in_len, int out_len)
{
	struct ib_uverbs_get_context      cmd;
	struct ib_uverbs_get_context_resp resp;
	struct ib_udata                   udata;
	struct ib_device                 *ibdev = file->device->ib_dev;
	struct ib_ucontext		 *ucontext;
	struct file			 *filp;
	int ret;

	if (out_len < sizeof resp)
		return -ENOSPC;

	if (copy_from_user(&cmd, buf, sizeof cmd))
		return -EFAULT;

	mutex_lock(&file->mutex);

	if (file->ucontext) {
		ret = -EINVAL;
		goto err;
	}

	INIT_UDATA(&udata, buf + sizeof cmd,
		   (unsigned long) cmd.response + sizeof resp,
		   in_len - sizeof cmd, out_len - sizeof resp);

	ucontext = ibdev->alloc_ucontext(ibdev, &udata);
	if (IS_ERR(ucontext)) {
		ret = PTR_ERR(ucontext);
		goto err;
	}

	ucontext->device = ibdev;
	INIT_LIST_HEAD(&ucontext->pd_list);
	INIT_LIST_HEAD(&ucontext->mr_list);
	INIT_LIST_HEAD(&ucontext->mw_list);
	INIT_LIST_HEAD(&ucontext->cq_list);
	INIT_LIST_HEAD(&ucontext->qp_list);
	INIT_LIST_HEAD(&ucontext->srq_list);
	INIT_LIST_HEAD(&ucontext->ah_list);
	INIT_LIST_HEAD(&ucontext->xrcd_list);
	INIT_LIST_HEAD(&ucontext->rule_list);
	INIT_LIST_HEAD(&ucontext->dct_list);
	ucontext->closing = 0;
	ucontext->peer_mem_private_data = NULL;
	ucontext->peer_mem_name = NULL;

	resp.num_comp_vectors = file->device->num_comp_vectors;

	ret = get_unused_fd();
	if (ret < 0)
		goto err_free;
	resp.async_fd = ret;

	filp = ib_uverbs_alloc_event_file(file, 1);
	if (IS_ERR(filp)) {
		ret = PTR_ERR(filp);
		goto err_fd;
	}

	file->async_file = filp->private_data;

	INIT_IB_EVENT_HANDLER(&file->event_handler, file->device->ib_dev,
			      ib_uverbs_event_handler);
	ret = ib_register_event_handler(&file->event_handler);
	if (ret)
		goto err_file;

	if (copy_to_user((void __user *) (unsigned long) cmd.response,
			 &resp, sizeof resp)) {
		ret = -EFAULT;
		goto err_file;
	}
	kref_get(&file->async_file->ref);
	kref_get(&file->ref);
	file->ucontext = ucontext;

	fd_install(resp.async_fd, filp);

	mutex_unlock(&file->mutex);

	return in_len;

err_file:
	fput(filp);

err_fd:
	put_unused_fd(resp.async_fd);

err_free:
	ibdev->dealloc_ucontext(ucontext);

err:
	mutex_unlock(&file->mutex);
	return ret;
}

static void ib_uverbs_query_device_assign(
		struct ib_uverbs_query_device_resp *resp,
		struct ib_device_attr *attr,
		struct ib_uverbs_file *file)
{
	memset(resp, 0, sizeof(*resp));

	resp->fw_ver		       = attr->fw_ver;
	resp->node_guid		       = file->device->ib_dev->node_guid;
	resp->sys_image_guid	       = attr->sys_image_guid;
	resp->max_mr_size	       = attr->max_mr_size;
	resp->page_size_cap	       = attr->page_size_cap;
	resp->vendor_id		       = attr->vendor_id;
	resp->vendor_part_id	       = attr->vendor_part_id;
	resp->hw_ver		       = attr->hw_ver;
	resp->max_qp		       = attr->max_qp;
	resp->max_qp_wr		       = attr->max_qp_wr;
	resp->device_cap_flags	       = attr->device_cap_flags;
	resp->max_sge		       = attr->max_sge;
	resp->max_sge_rd	       = attr->max_sge_rd;
	resp->max_cq		       = attr->max_cq;
	resp->max_cqe		       = attr->max_cqe;
	resp->max_mr		       = attr->max_mr;
	resp->max_pd		       = attr->max_pd;
	resp->max_qp_rd_atom	       = attr->max_qp_rd_atom;
	resp->max_ee_rd_atom	       = attr->max_ee_rd_atom;
	resp->max_res_rd_atom	       = attr->max_res_rd_atom;
	resp->max_qp_init_rd_atom      = attr->max_qp_init_rd_atom;
	resp->max_ee_init_rd_atom      = attr->max_ee_init_rd_atom;
	resp->atomic_cap	       = attr->atomic_cap;
	resp->max_ee		       = attr->max_ee;
	resp->max_rdd		       = attr->max_rdd;
	resp->max_mw		       = attr->max_mw;
	resp->max_raw_ipv6_qp	       = attr->max_raw_ipv6_qp;
	resp->max_raw_ethy_qp	       = attr->max_raw_ethy_qp;
	resp->max_mcast_grp	       = attr->max_mcast_grp;
	resp->max_mcast_qp_attach       = attr->max_mcast_qp_attach;
	resp->max_total_mcast_qp_attach = attr->max_total_mcast_qp_attach;
	resp->max_ah		       = attr->max_ah;
	resp->max_fmr		       = attr->max_fmr;
	resp->max_map_per_fmr	       = attr->max_map_per_fmr;
	resp->max_srq		       = attr->max_srq;
	resp->max_srq_wr	       = attr->max_srq_wr;
	resp->max_srq_sge	       = attr->max_srq_sge;
	resp->max_pkeys		       = attr->max_pkeys;
	resp->local_ca_ack_delay       = attr->local_ca_ack_delay;
	resp->phys_port_cnt	       = file->device->ib_dev->phys_port_cnt;
}

ssize_t ib_uverbs_query_device(struct ib_uverbs_file *file,
			       const char __user *buf,
			       int in_len, int out_len)
{
	struct ib_uverbs_query_device      cmd;
	struct ib_uverbs_query_device_resp resp;
	struct ib_device_attr              attr;
	int                                ret;

	if (out_len < sizeof resp)
		return -ENOSPC;

	if (copy_from_user(&cmd, buf, sizeof cmd))
		return -EFAULT;

	ret = ib_query_device(file->device->ib_dev, &attr);
	if (ret)
		return ret;

	ib_uverbs_query_device_assign(&resp, &attr, file);

	if (copy_to_user((void __user *)(unsigned long) cmd.response,
			 &resp, sizeof(resp)))
		return -EFAULT;

	return in_len;
}

ssize_t ib_uverbs_query_port(struct ib_uverbs_file *file,
			     const char __user *buf,
			     int in_len, int out_len)
{
	struct ib_uverbs_query_port      cmd;
	struct ib_uverbs_query_port_resp resp;
	struct ib_port_attr              attr;
	int                              ret;

	if (out_len < sizeof resp)
		return -ENOSPC;

	if (copy_from_user(&cmd, buf, sizeof cmd))
		return -EFAULT;

	ret = ib_query_port(file->device->ib_dev, cmd.port_num, &attr);
	if (ret)
		return ret;

	memset(&resp, 0, sizeof resp);

	resp.state 	     = attr.state;
	resp.max_mtu 	     = attr.max_mtu;
	resp.active_mtu      = attr.active_mtu;
	resp.gid_tbl_len     = attr.gid_tbl_len;
	resp.port_cap_flags  = attr.port_cap_flags;
	resp.max_msg_sz      = attr.max_msg_sz;
	resp.bad_pkey_cntr   = attr.bad_pkey_cntr;
	resp.qkey_viol_cntr  = attr.qkey_viol_cntr;
	resp.pkey_tbl_len    = attr.pkey_tbl_len;
	resp.lid 	     = attr.lid;
	resp.sm_lid 	     = attr.sm_lid;
	resp.lmc 	     = attr.lmc;
	resp.max_vl_num      = attr.max_vl_num;
	resp.sm_sl 	     = attr.sm_sl;
	resp.subnet_timeout  = attr.subnet_timeout;
	resp.init_type_reply = attr.init_type_reply;
	resp.active_width    = attr.active_width;
	resp.active_speed    = attr.active_speed;
	resp.phys_state      = attr.phys_state;
	resp.link_layer      = rdma_port_get_link_layer(file->device->ib_dev,
							cmd.port_num);

	if (copy_to_user((void __user *) (unsigned long) cmd.response,
			 &resp, sizeof resp))
		return -EFAULT;

	return in_len;
}

ssize_t ib_uverbs_alloc_pd(struct ib_uverbs_file *file,
			   const char __user *buf,
			   int in_len, int out_len)
{
	struct ib_uverbs_alloc_pd      cmd;
	struct ib_uverbs_alloc_pd_resp resp;
	struct ib_udata                udata;
	struct ib_uobject             *uobj;
	struct ib_pd                  *pd;
	int                            ret;

	if (out_len < sizeof resp)
		return -ENOSPC;

	if (copy_from_user(&cmd, buf, sizeof cmd))
		return -EFAULT;

	INIT_UDATA(&udata, buf + sizeof cmd,
		   (unsigned long) cmd.response + sizeof resp,
		   in_len - sizeof cmd, out_len - sizeof resp);

	uobj = kmalloc(sizeof *uobj, GFP_KERNEL);
	if (!uobj)
		return -ENOMEM;

	init_uobj(uobj, 0, file->ucontext, &pd_lock_class);
	down_write(&uobj->mutex);

	pd = file->device->ib_dev->alloc_pd(file->device->ib_dev,
					    file->ucontext, &udata);
	if (IS_ERR(pd)) {
		ret = PTR_ERR(pd);
		goto err;
	}

	pd->device  = file->device->ib_dev;
	pd->uobject = uobj;
	atomic_set(&pd->usecnt, 0);

	uobj->object = pd;
	ret = idr_add_uobj(&ib_uverbs_pd_idr, uobj);
	if (ret)
		goto err_idr;

	memset(&resp, 0, sizeof resp);
	resp.pd_handle = uobj->id;

	if (copy_to_user((void __user *) (unsigned long) cmd.response,
			 &resp, sizeof resp)) {
		ret = -EFAULT;
		goto err_copy;
	}

	mutex_lock(&file->mutex);
	list_add_tail(&uobj->list, &file->ucontext->pd_list);
	mutex_unlock(&file->mutex);

	uobj->live = 1;

	up_write(&uobj->mutex);

	return in_len;

err_copy:
	idr_remove_uobj(&ib_uverbs_pd_idr, uobj);

err_idr:
	ib_dealloc_pd(pd);

err:
	put_uobj_write(uobj);
	return ret;
}

ssize_t ib_uverbs_dealloc_pd(struct ib_uverbs_file *file,
			     const char __user *buf,
			     int in_len, int out_len)
{
	struct ib_uverbs_dealloc_pd cmd;
	struct ib_uobject          *uobj;
	int                         ret;

	if (copy_from_user(&cmd, buf, sizeof cmd))
		return -EFAULT;

	uobj = idr_write_uobj(&ib_uverbs_pd_idr, cmd.pd_handle, file->ucontext);
	if (!uobj)
		return -EINVAL;

	ret = ib_dealloc_pd(uobj->object);
	if (!ret)
		uobj->live = 0;

	put_uobj_write(uobj);

	if (ret)
		return ret;

	idr_remove_uobj(&ib_uverbs_pd_idr, uobj);

	mutex_lock(&file->mutex);
	list_del(&uobj->list);
	mutex_unlock(&file->mutex);

	put_uobj(uobj);

	return in_len;
}

struct xrcd_table_entry {
	struct rb_node  node;
	struct ib_xrcd *xrcd;
	struct inode   *inode;
};

static int xrcd_table_insert(struct ib_uverbs_device *dev,
			    struct inode *inode,
			    struct ib_xrcd *xrcd)
{
	struct xrcd_table_entry *entry, *scan;
	struct rb_node **p = &dev->xrcd_tree.rb_node;
	struct rb_node *parent = NULL;

	entry = kmalloc(sizeof *entry, GFP_KERNEL);
	if (!entry)
		return -ENOMEM;

	entry->xrcd  = xrcd;
	entry->inode = inode;

	while (*p) {
		parent = *p;
		scan = rb_entry(parent, struct xrcd_table_entry, node);

		if (inode < scan->inode) {
			p = &(*p)->rb_left;
		} else if (inode > scan->inode) {
			p = &(*p)->rb_right;
		} else {
			kfree(entry);
			return -EEXIST;
		}
	}

	rb_link_node(&entry->node, parent, p);
	rb_insert_color(&entry->node, &dev->xrcd_tree);
	igrab(inode);
	return 0;
}

static struct xrcd_table_entry *xrcd_table_search(struct ib_uverbs_device *dev,
						  struct inode *inode)
{
	struct xrcd_table_entry *entry;
	struct rb_node *p = dev->xrcd_tree.rb_node;

	while (p) {
		entry = rb_entry(p, struct xrcd_table_entry, node);

		if (inode < entry->inode)
			p = p->rb_left;
		else if (inode > entry->inode)
			p = p->rb_right;
		else
			return entry;
	}

	return NULL;
}

static struct ib_xrcd *find_xrcd(struct ib_uverbs_device *dev, struct inode *inode)
{
	struct xrcd_table_entry *entry;

	entry = xrcd_table_search(dev, inode);
	if (!entry)
		return NULL;

	return entry->xrcd;
}

static void xrcd_table_delete(struct ib_uverbs_device *dev,
			      struct inode *inode)
{
	struct xrcd_table_entry *entry;

	entry = xrcd_table_search(dev, inode);
	if (entry) {
		iput(inode);
		rb_erase(&entry->node, &dev->xrcd_tree);
		kfree(entry);
	}
}

ssize_t ib_uverbs_open_xrcd(struct ib_uverbs_file *file,
			 const char __user *buf, int in_len,
			 int out_len)
{
	struct ib_uverbs_open_xrcd	cmd;
	struct ib_uverbs_open_xrcd_resp	resp;
	struct ib_udata              udata;
	struct ib_uxrcd_object         *obj;
	struct ib_xrcd                 *xrcd = NULL;
	struct fd			f = {NULL};
	struct inode                   *inode = NULL;
	int				ret = 0;
	int				new_xrcd = 0;

	if (out_len < sizeof resp)
		return -ENOSPC;

	if (copy_from_user(&cmd, buf, sizeof cmd))
		return -EFAULT;

	INIT_UDATA(&udata, buf + sizeof cmd,
		   (unsigned long) cmd.response + sizeof resp,
		   in_len - sizeof cmd, out_len - sizeof resp);

	mutex_lock(&file->device->xrcd_tree_mutex);

	if (cmd.fd != -1) {
		/* search for file descriptor */
		f = fdget(cmd.fd);
		if (!f.file) {
			ret = -EBADF;
			goto err_tree_mutex_unlock;
		}

		inode = f.file->f_dentry->d_inode;
		xrcd = find_xrcd(file->device, inode);
		if (!xrcd && !(cmd.oflags & O_CREAT)) {
			/* no file descriptor. Need CREATE flag */
			ret = -EAGAIN;
			goto err_tree_mutex_unlock;
		}

		if (xrcd && cmd.oflags & O_EXCL) {
		ret = -EINVAL;
			goto err_tree_mutex_unlock;
		}
	}

	obj = kmalloc(sizeof *obj, GFP_KERNEL);
	if (!obj) {
		ret = -ENOMEM;
		goto err_tree_mutex_unlock;
	}

	init_uobj(&obj->uobject, 0, file->ucontext, &xrcd_lock_class);

	down_write(&obj->uobject.mutex);

	if (!xrcd) {
		xrcd = file->device->ib_dev->alloc_xrcd(file->device->ib_dev,
							file->ucontext, &udata);
		if (IS_ERR(xrcd)) {
			ret = PTR_ERR(xrcd);
			goto err;
		}

		xrcd->inode   = inode;
		xrcd->device  = file->device->ib_dev;
		atomic_set(&xrcd->usecnt, 0);
		mutex_init(&xrcd->tgt_qp_mutex);
		INIT_LIST_HEAD(&xrcd->tgt_qp_list);
		new_xrcd = 1;
	}

	atomic_set(&obj->refcnt, 0);
	obj->uobject.object = xrcd;
	ret = idr_add_uobj(&ib_uverbs_xrcd_idr, &obj->uobject);
	if (ret)
		goto err_idr;

	memset(&resp, 0, sizeof resp);
	resp.xrcd_handle = obj->uobject.id;

	if (inode) {
		if (new_xrcd) {
			/* create new inode/xrcd table entry */
			ret = xrcd_table_insert(file->device, inode, xrcd);
			if (ret)
				goto err_insert_xrcd;
		}
		atomic_inc(&xrcd->usecnt);
	}

	if (copy_to_user((void __user *) (unsigned long) cmd.response,
			 &resp, sizeof resp)) {
		ret = -EFAULT;
		goto err_copy;
	}

	if (f.file)
		fdput(f);

	mutex_lock(&file->mutex);
	list_add_tail(&obj->uobject.list, &file->ucontext->xrcd_list);
	mutex_unlock(&file->mutex);

	obj->uobject.live = 1;
	up_write(&obj->uobject.mutex);

	mutex_unlock(&file->device->xrcd_tree_mutex);
	return in_len;

err_copy:
	if (inode) {
		if (new_xrcd)
			xrcd_table_delete(file->device, inode);
		atomic_dec(&xrcd->usecnt);
	}

err_insert_xrcd:
	idr_remove_uobj(&ib_uverbs_xrcd_idr, &obj->uobject);

err_idr:
	ib_dealloc_xrcd(xrcd);

err:
	put_uobj_write(&obj->uobject);

err_tree_mutex_unlock:
	if (f.file)
		fdput(f);

	mutex_unlock(&file->device->xrcd_tree_mutex);

	return ret;
}

ssize_t ib_uverbs_close_xrcd(struct ib_uverbs_file *file,
			   const char __user *buf, int in_len,
			   int out_len)
{
	struct ib_uverbs_close_xrcd cmd;
	struct ib_uobject	 *uobj;
	struct ib_xrcd              *xrcd = NULL;
	struct inode                *inode = NULL;
	struct ib_uxrcd_object      *obj;
	int                         live;
	int                         ret = 0;

	if (copy_from_user(&cmd, buf, sizeof cmd))
		return -EFAULT;

	mutex_lock(&file->device->xrcd_tree_mutex);
	uobj = idr_write_uobj(&ib_uverbs_xrcd_idr, cmd.xrcd_handle, file->ucontext);
	if (!uobj) {
		ret = -EINVAL;
		goto out;
	}

	xrcd  = uobj->object;
	inode = xrcd->inode;
	obj   = container_of(uobj, struct ib_uxrcd_object, uobject);
	if (atomic_read(&obj->refcnt)) {
		put_uobj_write(uobj);
		ret = -EBUSY;
		goto out;
	}

	if (!inode || atomic_dec_and_test(&xrcd->usecnt)) {
		ret = ib_dealloc_xrcd(uobj->object);
	if (!ret)
		uobj->live = 0;
	}

	live = uobj->live;
	if (inode && ret)
		atomic_inc(&xrcd->usecnt);

	put_uobj_write(uobj);

	if (ret)
		goto out;

	if (inode && !live)
		xrcd_table_delete(file->device, inode);

	idr_remove_uobj(&ib_uverbs_xrcd_idr, uobj);
	mutex_lock(&file->mutex);
	list_del(&uobj->list);
	mutex_unlock(&file->mutex);

	put_uobj(uobj);
	ret = in_len;

out:
	mutex_unlock(&file->device->xrcd_tree_mutex);
	return ret;
}

void ib_uverbs_dealloc_xrcd(struct ib_uverbs_device *dev,
			    struct ib_xrcd *xrcd)
{
	struct inode *inode;

	inode = xrcd->inode;
	if (inode && !atomic_dec_and_test(&xrcd->usecnt))
		return;

	ib_dealloc_xrcd(xrcd);

	if (inode)
		xrcd_table_delete(dev, inode);
}

ssize_t ib_uverbs_reg_mr(struct ib_uverbs_file *file,
			    const char __user *buf, int in_len,
			    int out_len)
{
	struct ib_uverbs_reg_mr      cmd;
	struct ib_uverbs_reg_mr_resp resp;
	struct ib_udata                 udata;
	struct ib_uobject           *uobj;
	struct ib_pd                *pd;
	struct ib_mr                *mr;
	int                             ret;

	if (out_len < sizeof resp)
		return -ENOSPC;

	if (copy_from_user(&cmd, buf, sizeof cmd))
		return -EFAULT;

	INIT_UDATA(&udata, buf + sizeof cmd,
		   (unsigned long) cmd.response + sizeof resp,
		   in_len - sizeof cmd, out_len - sizeof resp);

	if ((cmd.start & ~PAGE_MASK) != (cmd.hca_va & ~PAGE_MASK))
		return -EINVAL;

	ret = ib_check_mr_access(cmd.access_flags);
	if (ret)
		return ret;

	uobj = kmalloc(sizeof *uobj, GFP_KERNEL);
	if (!uobj)
		return -ENOMEM;

	init_uobj(uobj, 0, file->ucontext, &mr_lock_class);
	down_write(&uobj->mutex);

	pd = idr_read_pd(cmd.pd_handle, file->ucontext);
	if (!pd) {
			ret = -EINVAL;
		goto err_free;
	}
	/* We first get a new "obj id" to be passed later to reg mr for
	    further use as mr_id.
	*/
	ret = idr_add_uobj(&ib_uverbs_mr_idr, uobj);
	if (ret)
		goto err_put;

	mr = pd->device->reg_user_mr(pd, cmd.start, cmd.length, cmd.hca_va,
				     cmd.access_flags, &udata, uobj->id);
	if (IS_ERR(mr)) {
		ret = PTR_ERR(mr);
		goto err_remove_uobj;
	}

	mr->device  = pd->device;
	mr->pd      = pd;
	mr->uobject = uobj;
	atomic_inc(&pd->usecnt);
	atomic_set(&mr->usecnt, 0);

	uobj->object = mr;

	memset(&resp, 0, sizeof resp);
	resp.lkey      = mr->lkey;
	resp.rkey      = mr->rkey;
	resp.mr_handle = uobj->id;

	if (copy_to_user((void __user *) (unsigned long) cmd.response,
			 &resp, sizeof resp)) {
		ret = -EFAULT;
		goto err_copy;
	}

	put_pd_read(pd);

	mutex_lock(&file->mutex);
	list_add_tail(&uobj->list, &file->ucontext->mr_list);
	mutex_unlock(&file->mutex);

	uobj->live = 1;

	up_write(&uobj->mutex);

	return in_len;

err_copy:
	ib_dereg_mr(mr);

err_remove_uobj:
	idr_remove_uobj(&ib_uverbs_mr_idr, uobj);

err_put:
	put_pd_read(pd);

err_free:
	put_uobj_write(uobj);
	return ret;
}

ssize_t ib_uverbs_dereg_mr(struct ib_uverbs_file *file,
			    const char __user *buf, int in_len,
			    int out_len)
{
	struct ib_uverbs_dereg_mr cmd;
	struct ib_mr             *mr;
	struct ib_uobject	 *uobj;
	int				ret = -EINVAL;

	if (copy_from_user(&cmd, buf, sizeof cmd))
		return -EFAULT;

	uobj = idr_write_uobj(&ib_uverbs_mr_idr, cmd.mr_handle, file->ucontext);
	if (!uobj)
		return -EINVAL;

	mr = uobj->object;

	ret = ib_dereg_mr(mr);
	if (!ret)
		uobj->live = 0;

	put_uobj_write(uobj);

	if (ret)
		return ret;

	idr_remove_uobj(&ib_uverbs_mr_idr, uobj);

	mutex_lock(&file->mutex);
	list_del(&uobj->list);
	mutex_unlock(&file->mutex);

	put_uobj(uobj);

	return in_len;
}

ssize_t ib_uverbs_alloc_mw(struct ib_uverbs_file *file,
			  const char __user *buf, int in_len,
			  int out_len)
{
	struct ib_uverbs_alloc_mw      cmd;
	struct ib_uverbs_alloc_mw_resp resp;
	struct ib_uobject             *uobj;
	struct ib_pd                  *pd;
	struct ib_mw                  *mw;
	int                            ret;

	if (out_len < sizeof(resp))
		return -ENOSPC;

	if (copy_from_user(&cmd, buf, sizeof(cmd)))
		return -EFAULT;

	uobj = kmalloc(sizeof(*uobj), GFP_KERNEL);
	if (!uobj)
		return -ENOMEM;

	init_uobj(uobj, 0, file->ucontext, &mw_lock_class);
	down_write(&uobj->mutex);

	pd = idr_read_pd(cmd.pd_handle, file->ucontext);
	if (!pd) {
		ret = -EINVAL;
		goto err_free;
	}

	mw = pd->device->alloc_mw(pd, cmd.mw_type);
	if (IS_ERR(mw)) {
		ret = PTR_ERR(mw);
		goto err_put;
	}

	mw->device  = pd->device;
	mw->pd      = pd;
	mw->uobject = uobj;
	atomic_inc(&pd->usecnt);

	uobj->object = mw;
	ret = idr_add_uobj(&ib_uverbs_mw_idr, uobj);
	if (ret)
		goto err_unalloc;

	memset(&resp, 0, sizeof(resp));
	resp.rkey      = mw->rkey;
	resp.mw_handle = uobj->id;

	if (copy_to_user((void __user *)(unsigned long)cmd.response,
			 &resp, sizeof(resp))) {
		ret = -EFAULT;
		goto err_copy;
	}

	put_pd_read(pd);

	mutex_lock(&file->mutex);
	list_add_tail(&uobj->list, &file->ucontext->mw_list);
	mutex_unlock(&file->mutex);

	uobj->live = 1;

	up_write(&uobj->mutex);

	return in_len;

err_copy:
	idr_remove_uobj(&ib_uverbs_mw_idr, uobj);

err_unalloc:
	ib_dealloc_mw(mw);

err_put:
	put_pd_read(pd);

err_free:
	put_uobj_write(uobj);
	return ret;
}

ssize_t ib_uverbs_dealloc_mw(struct ib_uverbs_file *file,
			     const char __user *buf, int in_len,
			     int out_len)
{
	struct ib_uverbs_dealloc_mw cmd;
	struct ib_mw               *mw;
	struct ib_uobject		*uobj;
	int                        	 ret = -EINVAL;

	if (copy_from_user(&cmd, buf, sizeof(cmd)))
		return -EFAULT;

	uobj = idr_write_uobj(&ib_uverbs_mw_idr, cmd.mw_handle, file->ucontext);
	if (!uobj)
		return -EINVAL;

	mw = uobj->object;

	ret = ib_dealloc_mw(mw);
	if (!ret)
		uobj->live = 0;

	put_uobj_write(uobj);

	if (ret)
		return ret;

	idr_remove_uobj(&ib_uverbs_mw_idr, uobj);

	mutex_lock(&file->mutex);
	list_del(&uobj->list);
	mutex_unlock(&file->mutex);

	put_uobj(uobj);

	return in_len;
}

ssize_t ib_uverbs_create_comp_channel(struct ib_uverbs_file *file,
			    const char __user *buf, int in_len,
			    int out_len)
{
	struct ib_uverbs_create_comp_channel	   cmd;
	struct ib_uverbs_create_comp_channel_resp  resp;
	struct file				  *filp;
	int ret;

	if (out_len < sizeof resp)
		return -ENOSPC;

	if (copy_from_user(&cmd, buf, sizeof cmd))
		return -EFAULT;

	ret = get_unused_fd();
	if (ret < 0)
		return ret;
	resp.fd = ret;

	filp = ib_uverbs_alloc_event_file(file, 0);
	if (IS_ERR(filp)) {
		put_unused_fd(resp.fd);
		return PTR_ERR(filp);
	}

	if (copy_to_user((void __user *) (unsigned long) cmd.response,
			 &resp, sizeof resp)) {
		put_unused_fd(resp.fd);
		fput(filp);
		return -EFAULT;
	}

	fd_install(resp.fd, filp);
	return in_len;
}

static ssize_t create_cq(struct ib_uverbs_file *file,
			 const char __user *buf, int in_len,
			 int out_len, void *vcmd, int ex,
			 void __user *response)
{
	struct ib_uverbs_create_cq     *cmd;
	struct ib_uverbs_create_cq_ex  *cmd_e;
	struct ib_uverbs_create_cq_resp resp;
	struct ib_udata                 udata;
	struct ib_ucq_object           *obj;
	struct ib_uverbs_event_file    *ev_file = NULL;
	struct ib_cq                   *cq;
	struct ib_cq_init_attr		attr;
	int				cmd_sz;
	int                             ret;

	if (out_len < sizeof resp)
		return -ENOSPC;

	cmd = vcmd;
	cmd_e = vcmd;
	cmd_sz = ex ? sizeof(*cmd_e) : sizeof(*cmd);
	INIT_UDATA(&udata, buf + cmd_sz, response + sizeof(resp),
		   in_len - sizeof(cmd), out_len - sizeof(resp));

	if (cmd->comp_vector >= file->device->num_comp_vectors)
		return -EINVAL;

	obj = kmalloc(sizeof *obj, GFP_KERNEL);
	if (!obj)
		return -ENOMEM;

	init_uobj(&obj->uobject, cmd->user_handle, file->ucontext,
		  &cq_lock_class);
	down_write(&obj->uobject.mutex);

	if (cmd->comp_channel >= 0) {
		ev_file = ib_uverbs_lookup_comp_file(cmd->comp_channel);
		if (!ev_file) {
			ret = -EINVAL;
			goto err;
		}
	}

	obj->uverbs_file	   = file;
	obj->comp_events_reported  = 0;
	obj->async_events_reported = 0;
	INIT_LIST_HEAD(&obj->comp_list);
	INIT_LIST_HEAD(&obj->async_list);

	memset(&attr, 0, sizeof(attr));
	attr.cqe = cmd->cqe;
	attr.comp_vector = cmd->comp_vector;
	if (ex && (cmd_e->comp_mask & IB_UVERBS_CREATE_CQ_EX_CAP_FLAGS))
		attr.flags = cmd_e->create_flags;
	cq = file->device->ib_dev->create_cq(file->device->ib_dev, &attr,
					     file->ucontext, &udata);
	if (IS_ERR(cq)) {
		ret = PTR_ERR(cq);
		goto err_file;
	}

	cq->device        = file->device->ib_dev;
	cq->uobject       = &obj->uobject;
	cq->comp_handler  = ib_uverbs_comp_handler;
	cq->event_handler = ib_uverbs_cq_event_handler;
	cq->cq_context    = ev_file;
	atomic_set(&cq->usecnt, 0);

	obj->uobject.object = cq;
	ret = idr_add_uobj(&ib_uverbs_cq_idr, &obj->uobject);
	if (ret)
		goto err_free;

	memset(&resp, 0, sizeof resp);
	resp.cq_handle = obj->uobject.id;
	resp.cqe       = cq->cqe;

	if (copy_to_user(response, &resp, sizeof(resp))) {
		ret = -EFAULT;
		goto err_copy;
	}

	mutex_lock(&file->mutex);
	list_add_tail(&obj->uobject.list, &file->ucontext->cq_list);
	mutex_unlock(&file->mutex);

	obj->uobject.live = 1;

	up_write(&obj->uobject.mutex);

	return in_len;

err_copy:
	idr_remove_uobj(&ib_uverbs_cq_idr, &obj->uobject);

err_free:
	ib_destroy_cq(cq);

err_file:
	if (ev_file)
		ib_uverbs_release_ucq(file, ev_file, obj);

err:
	put_uobj_write(&obj->uobject);
	return ret;
}

ssize_t ib_uverbs_create_cq(struct ib_uverbs_file *file,
			   const char __user *buf, int in_len,
			   int out_len)
{
	struct ib_uverbs_create_cq cmd;

	if (copy_from_user(&cmd, buf, sizeof(cmd)))
		return -EFAULT;

	return create_cq(file, buf, in_len, out_len, &cmd,
			 IB_USER_VERBS_CMD_BASIC,
			 (void __user *) (unsigned long) cmd.response);
}

ssize_t ib_uverbs_resize_cq(struct ib_uverbs_file *file,
			    const char __user *buf, int in_len,
			    int out_len)
{
	struct ib_uverbs_resize_cq	cmd;
	struct ib_uverbs_resize_cq_resp	resp;
	struct ib_udata                 udata;
	struct ib_cq			*cq;
	int				ret = -EINVAL;

	if (copy_from_user(&cmd, buf, sizeof cmd))
		return -EFAULT;

	INIT_UDATA(&udata, buf + sizeof cmd,
		   (unsigned long) cmd.response + sizeof resp,
		   in_len - sizeof cmd, out_len - sizeof resp);

	cq = idr_read_cq(cmd.cq_handle, file->ucontext, 0);
	if (!cq)
		return -EINVAL;

	ret = cq->device->resize_cq(cq, cmd.cqe, &udata);
	if (ret)
		goto out;

	resp.cqe = cq->cqe;

	if (copy_to_user((void __user *) (unsigned long) cmd.response,
			 &resp, sizeof resp.cqe))
		ret = -EFAULT;

out:
	put_cq_read(cq);

	return ret ? ret : in_len;
}

static int copy_wc_to_user(void __user *dest, struct ib_wc *wc)
{
	struct ib_uverbs_wc tmp;

	tmp.wr_id		= wc->wr_id;
	tmp.status		= wc->status;
	tmp.opcode		= wc->opcode;
	tmp.vendor_err		= wc->vendor_err;
	tmp.byte_len		= wc->byte_len;
	tmp.ex.imm_data		= (__u32 __force) wc->ex.imm_data;
	tmp.qp_num		= wc->qp->qp_num;
	tmp.src_qp		= wc->src_qp;
	tmp.wc_flags		= wc->wc_flags;
	tmp.pkey_index		= wc->pkey_index;
	tmp.slid		= wc->slid;
	tmp.sl			= wc->sl;
	tmp.dlid_path_bits	= wc->dlid_path_bits;
	tmp.port_num		= wc->port_num;
	tmp.reserved		= 0;

	if (copy_to_user(dest, &tmp, sizeof tmp))
		return -EFAULT;

	return 0;
}

ssize_t ib_uverbs_poll_cq(struct ib_uverbs_file *file,
			    const char __user *buf, int in_len,
			    int out_len)
{
	struct ib_uverbs_poll_cq       cmd;
	struct ib_uverbs_poll_cq_resp  resp;
	u8 __user                     *header_ptr;
	u8 __user                     *data_ptr;
	struct ib_cq                  *cq;
	struct ib_wc                   wc;
	int                        ret;

	if (copy_from_user(&cmd, buf, sizeof cmd))
		return -EFAULT;

	cq = idr_read_cq(cmd.cq_handle, file->ucontext, 0);
	if (!cq)
		return -EINVAL;

	/* we copy a struct ib_uverbs_poll_cq_resp to user space */
	header_ptr = (void __user *)(unsigned long) cmd.response;
	data_ptr = header_ptr + sizeof resp;

	memset(&resp, 0, sizeof resp);
	while (resp.count < cmd.ne) {
		ret = ib_poll_cq(cq, 1, &wc);
		if (ret < 0)
			goto out_put;
		if (!ret)
			break;

		ret = copy_wc_to_user(data_ptr, &wc);
		if (ret)
			goto out_put;

		data_ptr += sizeof(struct ib_uverbs_wc);
		++resp.count;
	}

	if (copy_to_user(header_ptr, &resp, sizeof resp)) {
		ret = -EFAULT;
		goto out_put;
	}

	ret = in_len;

out_put:
	put_cq_read(cq);
	return ret;
}

ssize_t ib_uverbs_req_notify_cq(struct ib_uverbs_file *file,
				const char __user *buf, int in_len,
				int out_len)
{
	struct ib_uverbs_req_notify_cq cmd;
	struct ib_cq                  *cq;

	if (copy_from_user(&cmd, buf, sizeof cmd))
		return -EFAULT;

	cq = idr_read_cq(cmd.cq_handle, file->ucontext, 0);
	if (!cq)
		return -EINVAL;

	ib_req_notify_cq(cq, cmd.solicited_only ?
			 IB_CQ_SOLICITED : IB_CQ_NEXT_COMP);

	put_cq_read(cq);

	return in_len;
}

ssize_t ib_uverbs_destroy_cq(struct ib_uverbs_file *file,
			     const char __user *buf, int in_len,
			     int out_len)
{
	struct ib_uverbs_destroy_cq      cmd;
	struct ib_uverbs_destroy_cq_resp resp;
	struct ib_uobject		*uobj;
	struct ib_cq               	*cq;
	struct ib_ucq_object        	*obj;
	struct ib_uverbs_event_file	*ev_file;
	int                        	 ret = -EINVAL;

	if (copy_from_user(&cmd, buf, sizeof cmd))
		return -EFAULT;

	uobj = idr_write_uobj(&ib_uverbs_cq_idr, cmd.cq_handle, file->ucontext);
	if (!uobj)
		return -EINVAL;
	cq      = uobj->object;
	ev_file = cq->cq_context;
	obj     = container_of(cq->uobject, struct ib_ucq_object, uobject);

	ret = ib_destroy_cq(cq);
	if (!ret)
		uobj->live = 0;

	put_uobj_write(uobj);

	if (ret)
		return ret;

	idr_remove_uobj(&ib_uverbs_cq_idr, uobj);

	mutex_lock(&file->mutex);
	list_del(&uobj->list);
	mutex_unlock(&file->mutex);

	ib_uverbs_release_ucq(file, ev_file, obj);

	memset(&resp, 0, sizeof resp);
	resp.comp_events_reported  = obj->comp_events_reported;
	resp.async_events_reported = obj->async_events_reported;

	put_uobj(uobj);

	if (copy_to_user((void __user *) (unsigned long) cmd.response,
			 &resp, sizeof resp))
		return -EFAULT;

	return in_len;
}

ssize_t ib_uverbs_create_qp(struct ib_uverbs_file *file,
			    const char __user *buf, int in_len,
			    int out_len)
{
	void __user		       *response;
	struct ib_udata                 udata;
	struct ib_uqp_object           *obj;
	struct ib_device	       *device;
	struct ib_pd                   *pd = NULL;
	struct ib_xrcd		       *xrcd = NULL;
	struct ib_uobject	       *uninitialized_var(xrcd_uobj);
	struct ib_cq                   *scq = NULL, *rcq = NULL;
	struct ib_srq                  *srq = NULL;
	struct ib_qp                   *qp;
	struct ib_qp_init_attr          attr;
	int ret;
	union {
		struct ib_uverbs_create_qp          basic;
	} cmd_obj;
	struct ib_uverbs_create_qp                 *cmd;
	size_t                                      cmd_size = 0;
	union {
		struct ib_uverbs_create_qp_resp     basic;
	} resp_obj;
	struct ib_uverbs_create_qp_resp            *resp;
	size_t                                      resp_size = 0;

	cmd_size = sizeof(cmd_obj.basic);
	cmd = &cmd_obj.basic;

	resp_size = sizeof(resp_obj.basic);
	resp = &resp_obj.basic;

	if (out_len < resp_size)
		return -ENOSPC;

	if (copy_from_user(&cmd_obj, buf, cmd_size))
		return -EFAULT;

	response = (void __user *) (unsigned long) cmd->response;

	if (!disable_raw_qp_enforcement &&
	    cmd->qp_type == IB_QPT_RAW_PACKET && !priv_check(curthread, PRIV_NET_RAW))
		return -EPERM;

	INIT_UDATA(&udata, buf + cmd_size, response + resp_size,
		   in_len - cmd_size, out_len - resp_size);

	obj = kzalloc(sizeof *obj, GFP_KERNEL);
	if (!obj)
		return -ENOMEM;

	init_uobj(&obj->uevent.uobject, cmd->user_handle, file->ucontext, &qp_lock_class);
	down_write(&obj->uevent.uobject.mutex);

	if (cmd->qp_type == IB_QPT_XRC_TGT) {
		xrcd = idr_read_xrcd(cmd->pd_handle, file->ucontext, &xrcd_uobj);
		if (!xrcd) {
			ret = -EINVAL;
			goto err_put;
		}
		device = xrcd->device;
	} else {
		if (cmd->qp_type == IB_QPT_XRC_INI) {
			cmd->max_recv_wr = 0;
			cmd->max_recv_sge = 0;
		} else {
			if (cmd->is_srq) {
				srq = idr_read_srq(cmd->srq_handle, file->ucontext);
				if (!srq || srq->srq_type != IB_SRQT_BASIC) {
			ret = -EINVAL;
					goto err_put;
		}
		}

			if (cmd->recv_cq_handle != cmd->send_cq_handle) {
				rcq = idr_read_cq(cmd->recv_cq_handle, file->ucontext, 0);
				if (!rcq) {
				ret = -EINVAL;
					goto err_put;
			}
			}
		}

		scq = idr_read_cq(cmd->send_cq_handle, file->ucontext, !!rcq);
		rcq = rcq ?: scq;
		pd  = idr_read_pd(cmd->pd_handle, file->ucontext);
		if (!pd || !scq) {
			ret = -EINVAL;
			goto err_put;
	}

		device = pd->device;
		}

	memset(&attr, 0, sizeof attr);
	attr.event_handler = ib_uverbs_qp_event_handler;
	attr.qp_context    = file;
	attr.send_cq       = scq;
	attr.recv_cq       = rcq;
	attr.srq           = srq;
	attr.xrcd	   = xrcd;
	attr.sq_sig_type   = cmd->sq_sig_all ? IB_SIGNAL_ALL_WR : IB_SIGNAL_REQ_WR;
	attr.qp_type       = cmd->qp_type;
	attr.create_flags  = 0;

	attr.cap.max_send_wr     = cmd->max_send_wr;
	attr.cap.max_recv_wr     = cmd->max_recv_wr;
	attr.cap.max_send_sge    = cmd->max_send_sge;
	attr.cap.max_recv_sge    = cmd->max_recv_sge;
	attr.cap.max_inline_data = cmd->max_inline_data;

	obj->uevent.events_reported     = 0;
	INIT_LIST_HEAD(&obj->uevent.event_list);
	INIT_LIST_HEAD(&obj->mcast_list);

	if (cmd->qp_type == IB_QPT_XRC_TGT)
		qp = ib_create_qp(pd, &attr);
	else
		qp = device->create_qp(pd, &attr, &udata);

	if (IS_ERR(qp)) {
		ret = PTR_ERR(qp);
		goto err_put;
	}

	if (cmd->qp_type != IB_QPT_XRC_TGT) {
		qp->real_qp	  = qp;
		qp->device	  = device;
		qp->pd		  = pd;
		qp->send_cq	  = attr.send_cq;
		qp->recv_cq	  = attr.recv_cq;
		qp->srq		  = attr.srq;
		qp->event_handler = attr.event_handler;
		qp->qp_context	  = attr.qp_context;
		qp->qp_type	  = attr.qp_type;
		atomic_set(&qp->usecnt, 0);
		atomic_inc(&pd->usecnt);
		atomic_inc(&attr.send_cq->usecnt);
		if (attr.recv_cq)
			atomic_inc(&attr.recv_cq->usecnt);
		if (attr.srq)
			atomic_inc(&attr.srq->usecnt);
	}
	qp->uobject = &obj->uevent.uobject;

	obj->uevent.uobject.object = qp;
	ret = idr_add_uobj(&ib_uverbs_qp_idr, &obj->uevent.uobject);
	if (ret)
		goto err_destroy;

	memset(&resp_obj, 0, sizeof(resp_obj));
	resp->qpn             = qp->qp_num;
	resp->qp_handle       = obj->uevent.uobject.id;
	resp->max_recv_sge    = attr.cap.max_recv_sge;
	resp->max_send_sge    = attr.cap.max_send_sge;
	resp->max_recv_wr     = attr.cap.max_recv_wr;
	resp->max_send_wr     = attr.cap.max_send_wr;
	resp->max_inline_data = attr.cap.max_inline_data;

	if (copy_to_user(response, &resp_obj, resp_size)) {
			ret = -EFAULT;
		goto err_copy;
		}

	if (xrcd) {
		obj->uxrcd = container_of(xrcd_uobj, struct ib_uxrcd_object, uobject);
		atomic_inc(&obj->uxrcd->refcnt);
		put_xrcd_read(xrcd_uobj);
		}

	if (pd)
		put_pd_read(pd);
	if (scq)
		put_cq_read(scq);
	if (rcq && rcq != scq)
		put_cq_read(rcq);
	if (srq)
		put_srq_read(srq);

	mutex_lock(&file->mutex);
	list_add_tail(&obj->uevent.uobject.list, &file->ucontext->qp_list);
	mutex_unlock(&file->mutex);

	obj->uevent.uobject.live = 1;

	up_write(&obj->uevent.uobject.mutex);

	return in_len;

err_copy:
	idr_remove_uobj(&ib_uverbs_qp_idr, &obj->uevent.uobject);

err_destroy:
	ib_destroy_qp(qp);

err_put:
	if (xrcd)
		put_xrcd_read(xrcd_uobj);
	if (pd)
		put_pd_read(pd);
	if (scq)
		put_cq_read(scq);
	if (rcq && rcq != scq)
		put_cq_read(rcq);
	if (srq)
		put_srq_read(srq);

	put_uobj_write(&obj->uevent.uobject);
	return ret;
}

ssize_t ib_uverbs_open_qp(struct ib_uverbs_file *file,
			  const char __user *buf, int in_len, int out_len)
{
	struct ib_uverbs_open_qp        cmd;
	struct ib_uverbs_create_qp_resp resp;
	struct ib_udata                 udata;
	struct ib_uqp_object           *obj;
	struct ib_xrcd		       *xrcd;
	struct ib_uobject	       *uninitialized_var(xrcd_uobj);
	struct ib_qp                   *qp;
	struct ib_qp_open_attr          attr;
	int ret;

	if (out_len < sizeof resp)
		return -ENOSPC;

	if (copy_from_user(&cmd, buf, sizeof cmd))
		return -EFAULT;

	INIT_UDATA(&udata, buf + sizeof cmd,
		   (unsigned long) cmd.response + sizeof resp,
		   in_len - sizeof cmd, out_len - sizeof resp);

	obj = kmalloc(sizeof *obj, GFP_KERNEL);
	if (!obj)
		return -ENOMEM;

	init_uobj(&obj->uevent.uobject, cmd.user_handle, file->ucontext, &qp_lock_class);
	down_write(&obj->uevent.uobject.mutex);

	xrcd = idr_read_xrcd(cmd.pd_handle, file->ucontext, &xrcd_uobj);
	if (!xrcd) {
		ret = -EINVAL;
		goto err_put;
	}

	attr.event_handler = ib_uverbs_qp_event_handler;
	attr.qp_context    = file;
	attr.qp_num        = cmd.qpn;
	attr.qp_type       = cmd.qp_type;

	obj->uevent.events_reported = 0;
	INIT_LIST_HEAD(&obj->uevent.event_list);
	INIT_LIST_HEAD(&obj->mcast_list);

	qp = ib_open_qp(xrcd, &attr);
	if (IS_ERR(qp)) {
		ret = PTR_ERR(qp);
		goto err_put;
		}

	qp->uobject = &obj->uevent.uobject;

	obj->uevent.uobject.object = qp;
	ret = idr_add_uobj(&ib_uverbs_qp_idr, &obj->uevent.uobject);
	if (ret)
		goto err_destroy;

	memset(&resp, 0, sizeof resp);
	resp.qpn       = qp->qp_num;
	resp.qp_handle = obj->uevent.uobject.id;

	if (copy_to_user((void __user *) (unsigned long) cmd.response,
			 &resp, sizeof resp)) {
		ret = -EFAULT;
		goto err_remove;
	}

	obj->uxrcd = container_of(xrcd_uobj, struct ib_uxrcd_object, uobject);
	atomic_inc(&obj->uxrcd->refcnt);
	put_xrcd_read(xrcd_uobj);

	mutex_lock(&file->mutex);
	list_add_tail(&obj->uevent.uobject.list, &file->ucontext->qp_list);
	mutex_unlock(&file->mutex);

	obj->uevent.uobject.live = 1;
	up_write(&obj->uevent.uobject.mutex);

	return in_len;

err_remove:
	idr_remove_uobj(&ib_uverbs_qp_idr, &obj->uevent.uobject);

err_destroy:
	ib_destroy_qp(qp);

err_put:
	put_xrcd_read(xrcd_uobj);
	put_uobj_write(&obj->uevent.uobject);
	return ret;
}

ssize_t ib_uverbs_query_qp(struct ib_uverbs_file *file,
				const char __user *buf, int in_len,
				int out_len)
{
	struct ib_uverbs_query_qp      cmd;
	struct ib_uverbs_query_qp_resp resp;
	struct ib_qp                   *qp;
	struct ib_qp_attr              *attr;
	struct ib_qp_init_attr         *init_attr;
	int                            ret;

	if (copy_from_user(&cmd, buf, sizeof cmd))
		return -EFAULT;

	attr      = kmalloc(sizeof *attr, GFP_KERNEL);
	init_attr = kmalloc(sizeof *init_attr, GFP_KERNEL);
	if (!attr || !init_attr) {
		ret = -ENOMEM;
		goto out;
	}

	qp = idr_read_qp(cmd.qp_handle, file->ucontext);
	if (!qp) {
		ret = -EINVAL;
		goto out;
	}

	ret = ib_query_qp(qp, attr, cmd.attr_mask, init_attr);

	put_qp_read(qp);

	if (ret)
		goto out;

	memset(&resp, 0, sizeof resp);

	resp.qp_state               = attr->qp_state;
	resp.cur_qp_state           = attr->cur_qp_state;
	resp.path_mtu               = attr->path_mtu;
	resp.path_mig_state         = attr->path_mig_state;
	resp.qkey                   = attr->qkey;
	resp.rq_psn                 = attr->rq_psn;
	resp.sq_psn                 = attr->sq_psn;
	resp.dest_qp_num            = attr->dest_qp_num;
	resp.qp_access_flags        = attr->qp_access_flags;
	resp.pkey_index             = attr->pkey_index;
	resp.alt_pkey_index         = attr->alt_pkey_index;
	resp.sq_draining            = attr->sq_draining;
	resp.max_rd_atomic          = attr->max_rd_atomic;
	resp.max_dest_rd_atomic     = attr->max_dest_rd_atomic;
	resp.min_rnr_timer          = attr->min_rnr_timer;
	resp.port_num               = attr->port_num;
	resp.timeout                = attr->timeout;
	resp.retry_cnt              = attr->retry_cnt;
	resp.rnr_retry              = attr->rnr_retry;
	resp.alt_port_num           = attr->alt_port_num;
	resp.alt_timeout            = attr->alt_timeout;

	memcpy(resp.dest.dgid, attr->ah_attr.grh.dgid.raw, 16);
	resp.dest.flow_label        = attr->ah_attr.grh.flow_label;
	resp.dest.sgid_index        = attr->ah_attr.grh.sgid_index;
	resp.dest.hop_limit         = attr->ah_attr.grh.hop_limit;
	resp.dest.traffic_class     = attr->ah_attr.grh.traffic_class;
	resp.dest.dlid              = attr->ah_attr.dlid;
	resp.dest.sl                = attr->ah_attr.sl;
	resp.dest.src_path_bits     = attr->ah_attr.src_path_bits;
	resp.dest.static_rate       = attr->ah_attr.static_rate;
	resp.dest.is_global         = !!(attr->ah_attr.ah_flags & IB_AH_GRH);
	resp.dest.port_num          = attr->ah_attr.port_num;

	memcpy(resp.alt_dest.dgid, attr->alt_ah_attr.grh.dgid.raw, 16);
	resp.alt_dest.flow_label    = attr->alt_ah_attr.grh.flow_label;
	resp.alt_dest.sgid_index    = attr->alt_ah_attr.grh.sgid_index;
	resp.alt_dest.hop_limit     = attr->alt_ah_attr.grh.hop_limit;
	resp.alt_dest.traffic_class = attr->alt_ah_attr.grh.traffic_class;
	resp.alt_dest.dlid          = attr->alt_ah_attr.dlid;
	resp.alt_dest.sl            = attr->alt_ah_attr.sl;
	resp.alt_dest.src_path_bits = attr->alt_ah_attr.src_path_bits;
	resp.alt_dest.static_rate   = attr->alt_ah_attr.static_rate;
	resp.alt_dest.is_global     = !!(attr->alt_ah_attr.ah_flags & IB_AH_GRH);
	resp.alt_dest.port_num      = attr->alt_ah_attr.port_num;

	resp.max_send_wr            = init_attr->cap.max_send_wr;
	resp.max_recv_wr            = init_attr->cap.max_recv_wr;
	resp.max_send_sge           = init_attr->cap.max_send_sge;
	resp.max_recv_sge           = init_attr->cap.max_recv_sge;
	resp.max_inline_data        = init_attr->cap.max_inline_data;
	resp.sq_sig_all             = init_attr->sq_sig_type == IB_SIGNAL_ALL_WR;

	if (copy_to_user((void __user *) (unsigned long) cmd.response,
			 &resp, sizeof resp))
		ret = -EFAULT;

out:
	kfree(attr);
	kfree(init_attr);

	return ret ? ret : in_len;
}

/* Remove ignored fields set in the attribute mask */
static int modify_qp_mask(enum ib_qp_type qp_type, int mask)
{
	switch (qp_type) {
	case IB_QPT_XRC_INI:
		return mask & ~(IB_QP_MAX_DEST_RD_ATOMIC | IB_QP_MIN_RNR_TIMER);
	case IB_QPT_XRC_TGT:
		return mask & ~(IB_QP_MAX_QP_RD_ATOMIC | IB_QP_RETRY_CNT |
				IB_QP_RNR_RETRY);
	default:
		return mask;
	}
}

static ssize_t __uverbs_modify_qp(struct ib_uverbs_file *file,
			    const char __user *buf, int in_len,
				  int out_len,
				  enum uverbs_cmd_type cmd_type)
{
	struct ib_uverbs_modify_qp_ex	cmd;
	struct ib_udata			udata;
	struct ib_qp		       *qp;
	struct ib_qp_attr	       *attr;
	struct ib_qp_attr_ex	       *attrx;
	int ret;
	void			       *p;
	union ib_gid			sgid;
	union ib_gid		       *dgid;
	u8				port_num;

	if (cmd_type == IB_USER_VERBS_CMD_BASIC) {
		p = &cmd;
		p += sizeof(cmd.comp_mask);
		if (copy_from_user(p, buf,
				   sizeof(struct ib_uverbs_modify_qp)))
		return -EFAULT;
	} else {
		if (copy_from_user(&cmd, buf, sizeof(cmd)))
			return -EFAULT;
	}

	INIT_UDATA(&udata, buf + sizeof cmd, NULL, in_len - sizeof cmd,
		   out_len);

	attrx = kzalloc(sizeof(*attrx), GFP_KERNEL);
	if (!attrx)
		return -ENOMEM;
	attr = (struct ib_qp_attr *)attrx;

	qp = idr_read_qp(cmd.qp_handle, file->ucontext);
	if (!qp) {
		kfree(attrx);
		return -EINVAL;
	}

	attr->qp_state 		  = cmd.qp_state;
	attr->cur_qp_state 	  = cmd.cur_qp_state;
	attr->path_mtu 		  = cmd.path_mtu;
	attr->path_mig_state 	  = cmd.path_mig_state;
	attr->qkey 		  = cmd.qkey;
	attr->rq_psn 		  = cmd.rq_psn;
	attr->sq_psn 		  = cmd.sq_psn;
	attr->dest_qp_num 	  = cmd.dest_qp_num;
	attr->qp_access_flags 	  = cmd.qp_access_flags;
	attr->pkey_index 	  = cmd.pkey_index;
	attr->alt_pkey_index 	  = cmd.alt_pkey_index;
	attr->en_sqd_async_notify = cmd.en_sqd_async_notify;
	attr->max_rd_atomic 	  = cmd.max_rd_atomic;
	attr->max_dest_rd_atomic  = cmd.max_dest_rd_atomic;
	attr->min_rnr_timer 	  = cmd.min_rnr_timer;
	attr->port_num 		  = cmd.port_num;
	attr->timeout 		  = cmd.timeout;
	attr->retry_cnt 	  = cmd.retry_cnt;
	attr->rnr_retry 	  = cmd.rnr_retry;
	attr->alt_port_num 	  = cmd.alt_port_num;
	attr->alt_timeout 	  = cmd.alt_timeout;

	memcpy(attr->ah_attr.grh.dgid.raw, cmd.dest.dgid, 16);
	attr->ah_attr.grh.flow_label        = cmd.dest.flow_label;
	attr->ah_attr.grh.sgid_index        = cmd.dest.sgid_index;
	attr->ah_attr.grh.hop_limit         = cmd.dest.hop_limit;
	attr->ah_attr.grh.traffic_class     = cmd.dest.traffic_class;
	attr->ah_attr.dlid 	    	    = cmd.dest.dlid;
	attr->ah_attr.sl   	    	    = cmd.dest.sl;
	attr->ah_attr.src_path_bits 	    = cmd.dest.src_path_bits;
	attr->ah_attr.static_rate   	    = cmd.dest.static_rate;
	attr->ah_attr.ah_flags 	    	    = cmd.dest.is_global ? IB_AH_GRH : 0;
	attr->ah_attr.port_num 	    	    = cmd.dest.port_num;

	memcpy(attr->alt_ah_attr.grh.dgid.raw, cmd.alt_dest.dgid, 16);
	attr->alt_ah_attr.grh.flow_label    = cmd.alt_dest.flow_label;
	attr->alt_ah_attr.grh.sgid_index    = cmd.alt_dest.sgid_index;
	attr->alt_ah_attr.grh.hop_limit     = cmd.alt_dest.hop_limit;
	attr->alt_ah_attr.grh.traffic_class = cmd.alt_dest.traffic_class;
	attr->alt_ah_attr.dlid 	    	    = cmd.alt_dest.dlid;
	attr->alt_ah_attr.sl   	    	    = cmd.alt_dest.sl;
	attr->alt_ah_attr.src_path_bits     = cmd.alt_dest.src_path_bits;
	attr->alt_ah_attr.static_rate       = cmd.alt_dest.static_rate;
	attr->alt_ah_attr.ah_flags 	    = cmd.alt_dest.is_global ? IB_AH_GRH : 0;
	attr->alt_ah_attr.port_num 	    = cmd.alt_dest.port_num;
	port_num = (cmd.attr_mask & IB_QP_PORT) ? cmd.port_num : qp->port_num;
	if ((cmd.attr_mask & IB_QP_AV) && port_num &&
	    (rdma_port_get_link_layer(qp->device, port_num) ==
	     IB_LINK_LAYER_ETHERNET)) {
		ret = ib_query_gid(qp->device, port_num,
				   attr->ah_attr.grh.sgid_index, &sgid);
	if (ret)
			goto out;
		dgid = &attr->ah_attr.grh.dgid;
		if (rdma_link_local_addr((struct in6_addr *)dgid->raw)) {
			rdma_get_ll_mac((struct in6_addr *)dgid->raw,
					attr->ah_attr.dmac);
			rdma_get_ll_mac((struct in6_addr *)sgid.raw,
					attr->smac);
			attr->vlan_id = rdma_get_vlan_id(&sgid);
		} else {
			ret = rdma_addr_find_dmac_by_grh(&sgid, dgid,
							 attr->ah_attr.dmac,
							 &attr->vlan_id);
			if (ret)
				goto out;
			ret = rdma_addr_find_smac_by_sgid(&sgid, attr->smac,
							  NULL);
			if (ret)
				goto out;
		}
		cmd.attr_mask |= IB_QP_SMAC;
		if (attr->vlan_id < 0xFFFF)
			cmd.attr_mask |= IB_QP_VID;
	}
	if (cmd_type == IB_USER_VERBS_CMD_EXTENDED) {
		if (cmd.comp_mask & IB_UVERBS_QP_ATTR_DCT_KEY)
			attrx->dct_key = cmd.dct_key;
	}

	if (qp->real_qp == qp) {
		ret = qp->device->modify_qp(qp, attr,
			modify_qp_mask(qp->qp_type, cmd.attr_mask), &udata);
		if (!ret && (cmd.attr_mask & IB_QP_PORT))
			qp->port_num = attr->port_num;
	} else {
		ret = ib_modify_qp(qp, attr, modify_qp_mask(qp->qp_type, cmd.attr_mask));
	}

	if (ret)
		goto out;

	ret = in_len;

out:
	put_qp_read(qp);
	kfree(attrx);

	return ret;
}

ssize_t ib_uverbs_modify_qp(struct ib_uverbs_file *file,
			    const char __user *buf, int in_len,
			    int out_len)
{
	return __uverbs_modify_qp(file, buf, in_len, out_len,
				  IB_USER_VERBS_CMD_BASIC);
}

ssize_t ib_uverbs_destroy_qp(struct ib_uverbs_file *file,
			     const char __user *buf, int in_len,
			     int out_len)
{
	struct ib_uverbs_destroy_qp      cmd;
	struct ib_uverbs_destroy_qp_resp resp;
	struct ib_uobject	   *uobj;
	struct ib_qp               	*qp;
	struct ib_uqp_object        	*obj;
	int                        	 ret = -EINVAL;

	if (copy_from_user(&cmd, buf, sizeof cmd))
		return -EFAULT;

	memset(&resp, 0, sizeof resp);

	uobj = idr_write_uobj(&ib_uverbs_qp_idr, cmd.qp_handle, file->ucontext);
	if (!uobj)
		return -EINVAL;
	qp  = uobj->object;
	obj = container_of(uobj, struct ib_uqp_object, uevent.uobject);

	if (!list_empty(&obj->mcast_list)) {
		put_uobj_write(uobj);
		return -EBUSY;
	}

	ret = ib_destroy_qp(qp);
	if (!ret)
		uobj->live = 0;

	put_uobj_write(uobj);

	if (ret)
		return ret;

	if (obj->uxrcd)
		atomic_dec(&obj->uxrcd->refcnt);

	idr_remove_uobj(&ib_uverbs_qp_idr, uobj);

	mutex_lock(&file->mutex);
	list_del(&uobj->list);
	mutex_unlock(&file->mutex);

	ib_uverbs_release_uevent(file, &obj->uevent);

	resp.events_reported = obj->uevent.events_reported;

	put_uobj(uobj);

	if (copy_to_user((void __user *) (unsigned long) cmd.response,
			 &resp, sizeof resp))
		return -EFAULT;

	return in_len;
}

ssize_t ib_uverbs_post_send(struct ib_uverbs_file *file,
			       const char __user *buf, int in_len,
			       int out_len)
{
	struct ib_uverbs_post_send      cmd;
	struct ib_uverbs_post_send_resp resp;
	struct ib_uverbs_send_wr       *user_wr;
	struct ib_send_wr              *wr = NULL, *last, *next, *bad_wr;
	struct ib_qp                 *qp;
	int                             i, sg_ind;
	int				is_ud;
	ssize_t                         ret = -EINVAL;

	if (copy_from_user(&cmd, buf, sizeof cmd))
		return -EFAULT;

	if (in_len < sizeof cmd + cmd.wqe_size * cmd.wr_count +
	    cmd.sge_count * sizeof (struct ib_uverbs_sge))
		return -EINVAL;

	if (cmd.wqe_size < sizeof (struct ib_uverbs_send_wr))
		return -EINVAL;

	user_wr = kmalloc(cmd.wqe_size, GFP_KERNEL);
	if (!user_wr)
		return -ENOMEM;

	qp = idr_read_qp(cmd.qp_handle, file->ucontext);
	if (!qp)
		goto out;

	is_ud = qp->qp_type == IB_QPT_UD;
	sg_ind = 0;
	last = NULL;
	for (i = 0; i < cmd.wr_count; ++i) {
		if (copy_from_user(user_wr,
				   buf + sizeof cmd + i * cmd.wqe_size,
				   cmd.wqe_size)) {
			ret = -EFAULT;
			goto out_put;
		}

		if (user_wr->num_sge + sg_ind > cmd.sge_count) {
			ret = -EINVAL;
			goto out_put;
		}

		next = kmalloc(ALIGN(sizeof *next, sizeof (struct ib_sge)) +
			       user_wr->num_sge * sizeof (struct ib_sge),
			       GFP_KERNEL);
		if (!next) {
		ret = -ENOMEM;
		goto out_put;
	}

		if (!last)
			wr = next;
	else
			last->next = next;
		last = next;

		next->next       = NULL;
		next->wr_id      = user_wr->wr_id;
		next->num_sge    = user_wr->num_sge;
		next->opcode     = user_wr->opcode;
		next->send_flags = user_wr->send_flags;

		if (is_ud) {
			next->wr.ud.ah = idr_read_ah(user_wr->wr.ud.ah,
						     file->ucontext);
			if (!next->wr.ud.ah) {
				ret = -EINVAL;
		goto out_put;
			}
			next->wr.ud.remote_qpn  = user_wr->wr.ud.remote_qpn;
			next->wr.ud.remote_qkey = user_wr->wr.ud.remote_qkey;
		} else {
			switch (next->opcode) {
			case IB_WR_RDMA_WRITE_WITH_IMM:
				next->ex.imm_data =
					(__be32 __force) user_wr->ex.imm_data;
			case IB_WR_RDMA_WRITE:
			case IB_WR_RDMA_READ:
				next->wr.rdma.remote_addr =
					user_wr->wr.rdma.remote_addr;
				next->wr.rdma.rkey        =
					user_wr->wr.rdma.rkey;
				break;
			case IB_WR_SEND_WITH_IMM:
				next->ex.imm_data =
					(__be32 __force) user_wr->ex.imm_data;
				break;
			case IB_WR_SEND_WITH_INV:
				next->ex.invalidate_rkey =
					user_wr->ex.invalidate_rkey;
				break;
			case IB_WR_ATOMIC_CMP_AND_SWP:
			case IB_WR_ATOMIC_FETCH_AND_ADD:
				next->wr.atomic.remote_addr =
					user_wr->wr.atomic.remote_addr;
				next->wr.atomic.compare_add =
					user_wr->wr.atomic.compare_add;
				next->wr.atomic.swap = user_wr->wr.atomic.swap;
				next->wr.atomic.rkey = user_wr->wr.atomic.rkey;
				break;
			default:
				break;
			}
		}

		if (next->num_sge) {
			next->sg_list = (void *) next +
				ALIGN(sizeof *next, sizeof (struct ib_sge));
			if (copy_from_user(next->sg_list,
					   buf + sizeof cmd +
					   cmd.wr_count * cmd.wqe_size +
					   sg_ind * sizeof (struct ib_sge),
					   next->num_sge * sizeof (struct ib_sge))) {
				ret = -EFAULT;
				goto out_put;
			}
			sg_ind += next->num_sge;
		} else
			next->sg_list = NULL;
	}

	resp.bad_wr = 0;
	ret = qp->device->post_send(qp->real_qp, wr, &bad_wr);
	if (ret)
		for (next = wr; next; next = next->next) {
			++resp.bad_wr;
			if (next == bad_wr)
			break;
		}

	if (copy_to_user((void __user *) (unsigned long) cmd.response,
			 &resp, sizeof resp))
		ret = -EFAULT;

out_put:
	put_qp_read(qp);

	while (wr) {
		if (is_ud && wr->wr.ud.ah)
			put_ah_read(wr->wr.ud.ah);
		next = wr->next;
		kfree(wr);
		wr = next;
	}

out:
	kfree(user_wr);

	return ret ? ret : in_len;
}

static struct ib_recv_wr *ib_uverbs_unmarshall_recv(const char __user *buf,
						    int in_len,
						    u32 wr_count,
						    u32 sge_count,
						    u32 wqe_size)
{
	struct ib_uverbs_recv_wr *user_wr;
	struct ib_recv_wr        *wr = NULL, *last, *next;
	int                       sg_ind;
	int                       i;
	int ret;

	if (in_len < wqe_size * wr_count +
	    sge_count * sizeof (struct ib_uverbs_sge))
		return ERR_PTR(-EINVAL);

	if (wqe_size < sizeof (struct ib_uverbs_recv_wr))
		return ERR_PTR(-EINVAL);

	user_wr = kmalloc(wqe_size, GFP_KERNEL);
	if (!user_wr)
		return ERR_PTR(-ENOMEM);

	sg_ind = 0;
	last = NULL;
	for (i = 0; i < wr_count; ++i) {
		if (copy_from_user(user_wr, buf + i * wqe_size,
				   wqe_size)) {
			ret = -EFAULT;
			goto err;
		}

		if (user_wr->num_sge + sg_ind > sge_count) {
		ret = -EINVAL;
		goto err;
	}

		next = kmalloc(ALIGN(sizeof *next, sizeof (struct ib_sge)) +
			       user_wr->num_sge * sizeof (struct ib_sge),
			       GFP_KERNEL);
		if (!next) {
			ret = -ENOMEM;
			goto err;
	}

		if (!last)
			wr = next;
		else
			last->next = next;
		last = next;

		next->next       = NULL;
		next->wr_id      = user_wr->wr_id;
		next->num_sge    = user_wr->num_sge;

		if (next->num_sge) {
			next->sg_list = (void *) next +
				ALIGN(sizeof *next, sizeof (struct ib_sge));
			if (copy_from_user(next->sg_list,
					   buf + wr_count * wqe_size +
					   sg_ind * sizeof (struct ib_sge),
					   next->num_sge * sizeof (struct ib_sge))) {
				ret = -EFAULT;
				goto err;
			}
			sg_ind += next->num_sge;
		} else
			next->sg_list = NULL;
	}

	kfree(user_wr);
	return wr;

err:
	kfree(user_wr);

	while (wr) {
		next = wr->next;
		kfree(wr);
		wr = next;
	}

	return ERR_PTR(ret);
}

ssize_t ib_uverbs_post_recv(struct ib_uverbs_file *file,
			    const char __user *buf, int in_len,
			    int out_len)
{
	struct ib_uverbs_post_recv      cmd;
	struct ib_uverbs_post_recv_resp resp;
	struct ib_recv_wr              *wr, *next, *bad_wr;
	struct ib_qp                   *qp;
	ssize_t                         ret = -EINVAL;

	if (copy_from_user(&cmd, buf, sizeof cmd))
		return -EFAULT;

	wr = ib_uverbs_unmarshall_recv(buf + sizeof cmd,
				       in_len - sizeof cmd, cmd.wr_count,
				       cmd.sge_count, cmd.wqe_size);
	if (IS_ERR(wr))
		return PTR_ERR(wr);

	qp = idr_read_qp(cmd.qp_handle, file->ucontext);
	if (!qp)
		goto out;

	resp.bad_wr = 0;
	ret = qp->device->post_recv(qp->real_qp, wr, &bad_wr);

	put_qp_read(qp);

	if (ret)
		for (next = wr; next; next = next->next) {
			++resp.bad_wr;
			if (next == bad_wr)
				break;
		}

	if (copy_to_user((void __user *) (unsigned long) cmd.response,
			 &resp, sizeof resp))
		ret = -EFAULT;

out:
	while (wr) {
		next = wr->next;
		kfree(wr);
		wr = next;
	}

	return ret ? ret : in_len;
}

ssize_t ib_uverbs_post_srq_recv(struct ib_uverbs_file *file,
				const char __user *buf, int in_len,
				int out_len)
{
	struct ib_uverbs_post_srq_recv      cmd;
	struct ib_uverbs_post_srq_recv_resp resp;
	struct ib_recv_wr                  *wr, *next, *bad_wr;
	struct ib_srq                      *srq;
	ssize_t                             ret = -EINVAL;

	if (copy_from_user(&cmd, buf, sizeof cmd))
		return -EFAULT;

	wr = ib_uverbs_unmarshall_recv(buf + sizeof cmd,
				       in_len - sizeof cmd, cmd.wr_count,
				       cmd.sge_count, cmd.wqe_size);
	if (IS_ERR(wr))
		return PTR_ERR(wr);

	srq = idr_read_srq(cmd.srq_handle, file->ucontext);
	if (!srq)
		goto out;

	resp.bad_wr = 0;
	ret = srq->device->post_srq_recv(srq, wr, &bad_wr);

	put_srq_read(srq);

	if (ret)
		for (next = wr; next; next = next->next) {
			++resp.bad_wr;
			if (next == bad_wr)
				break;
		}

	if (copy_to_user((void __user *) (unsigned long) cmd.response,
			 &resp, sizeof resp))
		ret = -EFAULT;

out:
	while (wr) {
		next = wr->next;
		kfree(wr);
		wr = next;
	}

	return ret ? ret : in_len;
}

ssize_t ib_uverbs_create_ah(struct ib_uverbs_file *file,
			     const char __user *buf, int in_len,
			     int out_len)
{
	struct ib_uverbs_create_ah	 cmd;
	struct ib_uverbs_create_ah_resp	 resp;
	struct ib_uobject		*uobj;
	struct ib_pd			*pd;
	struct ib_ah			*ah;
	struct ib_ah_attr		attr;
	int ret;

	if (out_len < sizeof resp)
		return -ENOSPC;

	if (copy_from_user(&cmd, buf, sizeof cmd))
		return -EFAULT;

	uobj = kmalloc(sizeof *uobj, GFP_KERNEL);
	if (!uobj)
		return -ENOMEM;

	init_uobj(uobj, cmd.user_handle, file->ucontext, &ah_lock_class);
	down_write(&uobj->mutex);

	pd  = idr_read_pd(cmd.pd_handle, file->ucontext);
	if (!pd) {
		ret = -EINVAL;
		goto err;
	}

	attr.dlid 	       = cmd.attr.dlid;
	attr.sl 	       = cmd.attr.sl;
	attr.src_path_bits     = cmd.attr.src_path_bits;
	attr.static_rate       = cmd.attr.static_rate;
	attr.ah_flags          = cmd.attr.is_global ? IB_AH_GRH : 0;
	attr.port_num 	       = cmd.attr.port_num;
	attr.grh.flow_label    = cmd.attr.grh.flow_label;
	attr.grh.sgid_index    = cmd.attr.grh.sgid_index;
	attr.grh.hop_limit     = cmd.attr.grh.hop_limit;
	attr.grh.traffic_class = cmd.attr.grh.traffic_class;
	memcpy(attr.grh.dgid.raw, cmd.attr.grh.dgid, 16);

	ah = ib_create_ah(pd, &attr);
	if (IS_ERR(ah)) {
		ret = PTR_ERR(ah);
		goto err_put;
	}

	ah->uobject  = uobj;
	uobj->object = ah;

	ret = idr_add_uobj(&ib_uverbs_ah_idr, uobj);
	if (ret)
		goto err_destroy;

	resp.ah_handle = uobj->id;

	if (copy_to_user((void __user *) (unsigned long) cmd.response,
			 &resp, sizeof resp)) {
		ret = -EFAULT;
		goto err_copy;
	}

	put_pd_read(pd);

	mutex_lock(&file->mutex);
	list_add_tail(&uobj->list, &file->ucontext->ah_list);
	mutex_unlock(&file->mutex);

	uobj->live = 1;

	up_write(&uobj->mutex);

	return in_len;

err_copy:
	idr_remove_uobj(&ib_uverbs_ah_idr, uobj);

err_destroy:
	ib_destroy_ah(ah);

err_put:
	put_pd_read(pd);

err:
	put_uobj_write(uobj);
	return ret;
}

ssize_t ib_uverbs_destroy_ah(struct ib_uverbs_file *file,
			     const char __user *buf, int in_len, int out_len)
{
	struct ib_uverbs_destroy_ah cmd;
	struct ib_ah		   *ah;
	struct ib_uobject	   *uobj;
	int                         ret;

	if (copy_from_user(&cmd, buf, sizeof cmd))
		return -EFAULT;

	uobj = idr_write_uobj(&ib_uverbs_ah_idr, cmd.ah_handle, file->ucontext);
	if (!uobj)
		return -EINVAL;
	ah = uobj->object;

	ret = ib_destroy_ah(ah);
	if (!ret)
		uobj->live = 0;

	put_uobj_write(uobj);

	if (ret)
		return ret;

	idr_remove_uobj(&ib_uverbs_ah_idr, uobj);

	mutex_lock(&file->mutex);
	list_del(&uobj->list);
	mutex_unlock(&file->mutex);

	put_uobj(uobj);

	return in_len;
}

ssize_t ib_uverbs_attach_mcast(struct ib_uverbs_file *file,
			       const char __user *buf, int in_len,
			       int out_len)
{
	struct ib_uverbs_attach_mcast cmd;
	struct ib_qp                 *qp;
	struct ib_uqp_object         *obj;
	struct ib_uverbs_mcast_entry *mcast;
	int                           ret;

	if (copy_from_user(&cmd, buf, sizeof cmd))
		return -EFAULT;

	qp = idr_write_qp(cmd.qp_handle, file->ucontext);
	if (!qp)
		return -EINVAL;

	obj = container_of(qp->uobject, struct ib_uqp_object, uevent.uobject);

	list_for_each_entry(mcast, &obj->mcast_list, list)
		if (cmd.mlid == mcast->lid &&
		    !memcmp(cmd.gid, mcast->gid.raw, sizeof mcast->gid.raw)) {
			ret = 0;
			goto out_put;
		}

	mcast = kmalloc(sizeof *mcast, GFP_KERNEL);
	if (!mcast) {
		ret = -ENOMEM;
		goto out_put;
	}

	mcast->lid = cmd.mlid;
	memcpy(mcast->gid.raw, cmd.gid, sizeof mcast->gid.raw);

	ret = ib_attach_mcast(qp, &mcast->gid, cmd.mlid);
	if (!ret)
		list_add_tail(&mcast->list, &obj->mcast_list);
	else
		kfree(mcast);

out_put:
	put_qp_write(qp);

	return ret ? ret : in_len;
}

ssize_t ib_uverbs_detach_mcast(struct ib_uverbs_file *file,
			      const char __user *buf, int in_len,
			      int out_len)
{
	struct ib_uverbs_detach_mcast cmd;
	struct ib_uqp_object         *obj;
	struct ib_qp                 *qp;
	struct ib_uverbs_mcast_entry *mcast;
	int                         	  ret = -EINVAL;

	if (copy_from_user(&cmd, buf, sizeof cmd))
		return -EFAULT;

	qp = idr_write_qp(cmd.qp_handle, file->ucontext);
	if (!qp)
		return -EINVAL;

	ret = ib_detach_mcast(qp, (union ib_gid *) cmd.gid, cmd.mlid);
	if (ret)
		goto out_put;

	obj = container_of(qp->uobject, struct ib_uqp_object, uevent.uobject);

	list_for_each_entry(mcast, &obj->mcast_list, list)
		if (cmd.mlid == mcast->lid &&
		    !memcmp(cmd.gid, mcast->gid.raw, sizeof mcast->gid.raw)) {
			list_del(&mcast->list);
			kfree(mcast);
			break;
		}

out_put:
	put_qp_write(qp);

	return ret ? ret : in_len;
}

static int __uverbs_create_xsrq(struct ib_uverbs_file *file,
				struct ib_uverbs_create_xsrq *cmd,
				struct ib_udata *udata)
{
	struct ib_uverbs_create_srq_resp resp;
	struct ib_usrq_object           *obj;
	struct ib_pd                    *pd;
	struct ib_srq                   *srq;
	struct ib_uobject               *uninitialized_var(xrcd_uobj);
	struct ib_srq_init_attr          attr;
	int ret;

	obj = kmalloc(sizeof(*obj), GFP_KERNEL);
	if (!obj)
		return -ENOMEM;

	init_uobj(&obj->uevent.uobject, cmd->user_handle, file->ucontext, &srq_lock_class);
	down_write(&obj->uevent.uobject.mutex);

	if (cmd->srq_type == IB_SRQT_XRC) {
		attr.ext.xrc.xrcd  = idr_read_xrcd(cmd->xrcd_handle, file->ucontext, &xrcd_uobj);
		if (!attr.ext.xrc.xrcd) {
			ret = -EINVAL;
			goto err;
		}

		obj->uxrcd = container_of(xrcd_uobj, struct ib_uxrcd_object, uobject);
		atomic_inc(&obj->uxrcd->refcnt);

		attr.ext.xrc.cq  = idr_read_cq(cmd->cq_handle, file->ucontext, 0);
		if (!attr.ext.xrc.cq) {
			ret = -EINVAL;
			goto err_put_xrcd;
		}
	}

	pd  = idr_read_pd(cmd->pd_handle, file->ucontext);
	if (!pd) {
		ret = -EINVAL;
		goto err_put_cq;
		}

	attr.event_handler  = ib_uverbs_srq_event_handler;
	attr.srq_context    = file;
	attr.srq_type       = cmd->srq_type;
	attr.attr.max_wr    = cmd->max_wr;
	attr.attr.max_sge   = cmd->max_sge;
	attr.attr.srq_limit = cmd->srq_limit;

	obj->uevent.events_reported = 0;
	INIT_LIST_HEAD(&obj->uevent.event_list);

	srq = pd->device->create_srq(pd, &attr, udata);
	if (IS_ERR(srq)) {
		ret = PTR_ERR(srq);
		goto err_put;
	}

	srq->device        = pd->device;
	srq->pd            = pd;
	srq->srq_type	   = cmd->srq_type;
	srq->uobject       = &obj->uevent.uobject;
	srq->event_handler = attr.event_handler;
	srq->srq_context   = attr.srq_context;

	if (cmd->srq_type == IB_SRQT_XRC) {
		srq->ext.xrc.cq   = attr.ext.xrc.cq;
		srq->ext.xrc.xrcd = attr.ext.xrc.xrcd;
		atomic_inc(&attr.ext.xrc.cq->usecnt);
		atomic_inc(&attr.ext.xrc.xrcd->usecnt);
	}

	atomic_inc(&pd->usecnt);
	atomic_set(&srq->usecnt, 0);

	obj->uevent.uobject.object = srq;
	ret = idr_add_uobj(&ib_uverbs_srq_idr, &obj->uevent.uobject);
	if (ret)
		goto err_destroy;

	memset(&resp, 0, sizeof resp);
	resp.srq_handle = obj->uevent.uobject.id;
	resp.max_wr     = attr.attr.max_wr;
	resp.max_sge    = attr.attr.max_sge;
	if (cmd->srq_type == IB_SRQT_XRC)
		resp.srqn = srq->ext.xrc.srq_num;

	if (copy_to_user((void __user *) (unsigned long) cmd->response,
			 &resp, sizeof resp)) {
		ret = -EFAULT;
		goto err_copy;
	}

	if (cmd->srq_type == IB_SRQT_XRC) {
		put_uobj_read(xrcd_uobj);
		put_cq_read(attr.ext.xrc.cq);
	}
	put_pd_read(pd);

	mutex_lock(&file->mutex);
	list_add_tail(&obj->uevent.uobject.list, &file->ucontext->srq_list);
	mutex_unlock(&file->mutex);

	obj->uevent.uobject.live = 1;

	up_write(&obj->uevent.uobject.mutex);

	return 0;

err_copy:
	idr_remove_uobj(&ib_uverbs_srq_idr, &obj->uevent.uobject);

err_destroy:
	ib_destroy_srq(srq);

err_put:
	put_pd_read(pd);

err_put_cq:
	if (cmd->srq_type == IB_SRQT_XRC)
		put_cq_read(attr.ext.xrc.cq);

err_put_xrcd:
	if (cmd->srq_type == IB_SRQT_XRC) {
		atomic_dec(&obj->uxrcd->refcnt);
		put_uobj_read(xrcd_uobj);
	}

err:
	put_uobj_write(&obj->uevent.uobject);
	return ret;
}

ssize_t ib_uverbs_create_srq(struct ib_uverbs_file *file,
				  const char __user *buf, int in_len,
				  int out_len)
{
	struct ib_uverbs_create_srq      cmd;
	struct ib_uverbs_create_xsrq     xcmd;
	struct ib_uverbs_create_srq_resp resp;
	struct ib_udata	udata;
	int ret;

	if (out_len < sizeof resp)
		return -ENOSPC;

	if (copy_from_user(&cmd, buf, sizeof cmd))
		return -EFAULT;

	xcmd.response	 = cmd.response;
	xcmd.user_handle = cmd.user_handle;
	xcmd.srq_type	 = IB_SRQT_BASIC;
	xcmd.pd_handle	 = cmd.pd_handle;
	xcmd.max_wr	 = cmd.max_wr;
	xcmd.max_sge	 = cmd.max_sge;
	xcmd.srq_limit	 = cmd.srq_limit;

	INIT_UDATA(&udata, buf + sizeof cmd,
		   (unsigned long) cmd.response + sizeof resp,
		   in_len - sizeof cmd, out_len - sizeof resp);

	ret = __uverbs_create_xsrq(file, &xcmd, &udata);
	if (ret)
		return ret;

	return in_len;
}

ssize_t ib_uverbs_create_xsrq(struct ib_uverbs_file *file,
			      const char __user *buf, int in_len, int out_len)
{
	struct ib_uverbs_create_xsrq     cmd;
	struct ib_uverbs_create_srq_resp resp;
	struct ib_udata                  udata;
	int ret;

	if (out_len < sizeof resp)
		return -ENOSPC;

	if (copy_from_user(&cmd, buf, sizeof cmd))
		return -EFAULT;

	INIT_UDATA(&udata, buf + sizeof cmd,
		   (unsigned long) cmd.response + sizeof resp,
		   in_len - sizeof cmd, out_len - sizeof resp);

	ret = __uverbs_create_xsrq(file, &cmd, &udata);
	if (ret)
		return ret;

	return in_len;
}

ssize_t ib_uverbs_modify_srq(struct ib_uverbs_file *file,
			     const char __user *buf, int in_len,
			     int out_len)
{
	struct ib_uverbs_modify_srq cmd;
	struct ib_udata             udata;
	struct ib_srq              *srq;
	struct ib_srq_attr          attr;
	int                         ret;

	if (copy_from_user(&cmd, buf, sizeof cmd))
		return -EFAULT;

	INIT_UDATA(&udata, buf + sizeof cmd, NULL, in_len - sizeof cmd,
		   out_len);

	srq = idr_read_srq(cmd.srq_handle, file->ucontext);
	if (!srq)
		return -EINVAL;

	attr.max_wr    = cmd.max_wr;
	attr.srq_limit = cmd.srq_limit;

	ret = srq->device->modify_srq(srq, &attr, cmd.attr_mask, &udata);

	put_srq_read(srq);

	return ret ? ret : in_len;
}

ssize_t ib_uverbs_query_srq(struct ib_uverbs_file *file,
			    const char __user *buf,
			    int in_len, int out_len)
{
	struct ib_uverbs_query_srq      cmd;
	struct ib_uverbs_query_srq_resp resp;
	struct ib_srq_attr              attr;
	struct ib_srq                   *srq;
	int                             ret;

	if (out_len < sizeof resp)
		return -ENOSPC;

	if (copy_from_user(&cmd, buf, sizeof cmd))
		return -EFAULT;

	srq = idr_read_srq(cmd.srq_handle, file->ucontext);
	if (!srq)
		return -EINVAL;

	ret = ib_query_srq(srq, &attr);

	put_srq_read(srq);

	if (ret)
	return ret;

	memset(&resp, 0, sizeof resp);

	resp.max_wr    = attr.max_wr;
	resp.max_sge   = attr.max_sge;
	resp.srq_limit = attr.srq_limit;

	if (copy_to_user((void __user *) (unsigned long) cmd.response,
			 &resp, sizeof resp))
		return -EFAULT;

	return in_len;
}

ssize_t ib_uverbs_destroy_srq(struct ib_uverbs_file *file,
				   const char __user *buf, int in_len,
				   int out_len)
{
	struct ib_uverbs_destroy_srq      cmd;
	struct ib_uverbs_destroy_srq_resp resp;
	struct ib_uobject		 *uobj;
	struct ib_srq               	 *srq;
	struct ib_uevent_object        	 *obj;
	int                         	  ret = -EINVAL;
	struct ib_usrq_object	*us;
	enum ib_srq_type srq_type;

	if (copy_from_user(&cmd, buf, sizeof cmd))
		return -EFAULT;

	uobj = idr_write_uobj(&ib_uverbs_srq_idr, cmd.srq_handle, file->ucontext);
	if (!uobj)
		return -EINVAL;
	srq = uobj->object;
	obj = container_of(uobj, struct ib_uevent_object, uobject);
	srq_type = srq->srq_type;

	ret = ib_destroy_srq(srq);
	if (!ret)
		uobj->live = 0;

	put_uobj_write(uobj);

	if (ret)
		return ret;

	if (srq_type == IB_SRQT_XRC) {
		us = container_of(obj, struct ib_usrq_object, uevent);
		atomic_dec(&us->uxrcd->refcnt);
	}

	idr_remove_uobj(&ib_uverbs_srq_idr, uobj);

	mutex_lock(&file->mutex);
	list_del(&uobj->list);
	mutex_unlock(&file->mutex);

	ib_uverbs_release_uevent(file, obj);

	memset(&resp, 0, sizeof resp);
	resp.events_reported = obj->events_reported;

	put_uobj(uobj);

	if (copy_to_user((void __user *) (unsigned long) cmd.response,
			 &resp, sizeof resp))
		ret = -EFAULT;

	return ret ? ret : in_len;
}

ssize_t ib_uverbs_exp_create_dct(struct ib_uverbs_file *file,
				 struct ib_udata *ucore,
				 struct ib_udata *uhw)
{
	int in_len			= ucore->inlen + uhw->inlen;
	int out_len			= ucore->outlen + uhw->outlen;
	struct ib_uverbs_create_dct	 cmd;
	struct ib_uverbs_create_dct_resp resp;
	struct ib_udata			 udata;
	struct ib_udct_object		*obj;
	struct ib_dct			*dct;
	int                             ret;
	struct ib_dct_init_attr		attr;
	struct ib_pd			*pd = NULL;
	struct ib_cq			*cq = NULL;
	struct ib_srq			*srq = NULL;

	if (out_len < sizeof(resp))
		return -ENOSPC;

	ret = ucore->ops->copy_from(&cmd, ucore, sizeof(cmd));
	if (ret)
		return ret;

	obj = kmalloc(sizeof(*obj), GFP_KERNEL);
	if (!obj)
		return -ENOMEM;

	init_uobj(&obj->uobject, cmd.user_handle, file->ucontext,
		  &dct_lock_class);
	down_write(&obj->uobject.mutex);

	pd = idr_read_pd(cmd.pd_handle, file->ucontext);
	if (!pd) {
		ret = -EINVAL;
		goto err_pd;
	}

	cq = idr_read_cq(cmd.cq_handle, file->ucontext, 0);
	if (!cq) {
		ret = -EINVAL;
		goto err_put;
	}

	srq = idr_read_srq(cmd.srq_handle, file->ucontext);
	if (!srq) {
		ret = -EINVAL;
		goto err_put;
	}

	attr.cq = cq;
	attr.access_flags = cmd.access_flags;
	attr.min_rnr_timer = cmd.min_rnr_timer;
	attr.srq = srq;
	attr.tclass = cmd.tclass;
	attr.flow_label = cmd.flow_label;
	attr.dc_key = cmd.dc_key;
	attr.mtu = cmd.mtu;
	attr.port = cmd.port;
	attr.pkey_index = cmd.pkey_index;
	attr.gid_index = cmd.gid_index;
	attr.hop_limit = cmd.hop_limit;
	attr.create_flags = cmd.create_flags;

	dct = ib_create_dct(pd, &attr, &udata);
	if (IS_ERR(dct)) {
		ret = PTR_ERR(dct);
		goto err_put;
	}

	dct->device        = file->device->ib_dev;
	dct->uobject       = &obj->uobject;

	obj->uobject.object = dct;
	ret = idr_add_uobj(&ib_uverbs_dct_idr, &obj->uobject);
	if (ret)
		goto err_dct;

	memset(&resp, 0, sizeof(resp));
	resp.dct_handle = obj->uobject.id;
	resp.dctn = dct->dct_num;

	ret = ucore->ops->copy_to(ucore, &resp, sizeof(resp));
	if (ret)
		goto err_copy;

	mutex_lock(&file->mutex);
	list_add_tail(&obj->uobject.list, &file->ucontext->dct_list);
	mutex_unlock(&file->mutex);

	obj->uobject.live = 1;

	put_srq_read(srq);
	put_cq_read(cq);
	put_pd_read(pd);

	up_write(&obj->uobject.mutex);

	return in_len;

err_copy:
	idr_remove_uobj(&ib_uverbs_dct_idr, &obj->uobject);

err_dct:
	ib_destroy_dct(dct);

err_put:
	if (srq)
		put_srq_read(srq);

	if (cq)
		put_cq_read(cq);

	put_pd_read(pd);

err_pd:
	put_uobj_write(&obj->uobject);
	return ret;
}

ssize_t ib_uverbs_exp_destroy_dct(struct ib_uverbs_file *file,
				  struct ib_udata *ucore,
				  struct ib_udata *uhw)
{
	int in_len				= ucore->inlen + uhw->inlen;
	int out_len				= ucore->outlen + uhw->outlen;
	struct ib_uverbs_destroy_dct		cmd;
	struct ib_uverbs_destroy_dct_resp	resp;
	struct ib_uobject	       *uobj;
	struct ib_dct			       *dct;
	struct ib_udct_object		       *obj;
	int					ret;

	if (out_len < sizeof(resp))
		return -ENOSPC;

	ret = ucore->ops->copy_from(&cmd, ucore, sizeof(cmd));
	if (ret)
		return ret;

	uobj = idr_write_uobj(&ib_uverbs_dct_idr, cmd.user_handle, file->ucontext);
	if (!uobj)
		return -EINVAL;

	dct      = uobj->object;
	obj     = container_of(dct->uobject, struct ib_udct_object, uobject);

	ret = ib_destroy_dct(dct);
	if (!ret)
		uobj->live = 0;

	put_uobj_write(uobj);

	if (ret)
		return ret;

	idr_remove_uobj(&ib_uverbs_dct_idr, uobj);

	mutex_lock(&file->mutex);
	list_del(&uobj->list);
	mutex_unlock(&file->mutex);

	memset(&resp, 0, sizeof(resp));

	put_uobj(uobj);

	ret = ucore->ops->copy_to(ucore, &resp, sizeof(resp));
	if (ret)
		return ret;

	return in_len;
}

ssize_t ib_uverbs_exp_query_dct(struct ib_uverbs_file *file,
				struct ib_udata *ucore,
				struct ib_udata *uhw)
{
	int in_len			= ucore->inlen + uhw->inlen;
	int out_len			= ucore->outlen + uhw->outlen;
	struct ib_uverbs_query_dct	cmd;
	struct ib_uverbs_query_dct_resp	resp;
	struct ib_dct		       *dct;
	struct ib_dct_attr	       *attr;
	int				err;

	if (out_len < sizeof(resp))
		return -ENOSPC;

	err = ucore->ops->copy_from(&cmd, ucore, sizeof(cmd));
	if (err)
		return err;

	attr = kmalloc(sizeof(*attr), GFP_KERNEL);
	if (!attr) {
		err = -ENOMEM;
		goto out;
	}

	dct = idr_read_dct(cmd.dct_handle, file->ucontext);
	if (!dct) {
		err = -EINVAL;
		goto out;
	}

	err = ib_query_dct(dct, attr);

	put_dct_read(dct);

	if (err)
		goto out;

	memset(&resp, 0, sizeof(resp));

	resp.dc_key = attr->dc_key;
	resp.access_flags = attr->access_flags;
	resp.flow_label = attr->flow_label;
	resp.key_violations = attr->key_violations;
	resp.port = attr->port;
	resp.min_rnr_timer	    = attr->min_rnr_timer;
	resp.tclass = attr->tclass;
	resp.mtu = attr->mtu;
	resp.pkey_index = attr->pkey_index;
	resp.gid_index = attr->gid_index;
	resp.hop_limit = attr->hop_limit;
	resp.state = attr->state;

	err = ucore->ops->copy_to(ucore, &resp, sizeof(resp));

out:
	kfree(attr);

	return err ? err : in_len;
}

/*
 * Experimental functions
 */

static struct uverbs_lock_class rule_lock_class = { .name = "RULE-uobj" };

static int kern_spec_to_ib_spec(struct ib_uverbs_flow_spec *kern_spec,
				union ib_flow_spec *ib_spec)
{
	ib_spec->type = kern_spec->type;

	switch (ib_spec->type) {
	case IB_FLOW_SPEC_ETH:
		ib_spec->eth.size = sizeof(struct ib_flow_spec_eth);
		memcpy(&ib_spec->eth.val, &kern_spec->eth.val,
		       sizeof(struct ib_flow_eth_filter));
		memcpy(&ib_spec->eth.mask, &kern_spec->eth.mask,
		       sizeof(struct ib_flow_eth_filter));
		break;
	case IB_FLOW_SPEC_IB:
		ib_spec->ib.size = sizeof(struct ib_flow_spec_ib);
		memcpy(&ib_spec->ib.val, &kern_spec->ib.val,
		       sizeof(struct ib_flow_ib_filter));
		memcpy(&ib_spec->ib.mask, &kern_spec->ib.mask,
		       sizeof(struct ib_flow_ib_filter));
		break;
	case IB_FLOW_SPEC_IPV4:
		ib_spec->ipv4.size = sizeof(struct ib_flow_spec_ipv4);
		memcpy(&ib_spec->ipv4.val, &kern_spec->ipv4.val,
		       sizeof(struct ib_flow_ipv4_filter));
		memcpy(&ib_spec->ipv4.mask, &kern_spec->ipv4.mask,
		       sizeof(struct ib_flow_ipv4_filter));
		break;
	case IB_FLOW_SPEC_TCP:
	case IB_FLOW_SPEC_UDP:
		ib_spec->tcp_udp.size = sizeof(struct ib_flow_spec_tcp_udp);
		memcpy(&ib_spec->tcp_udp.val, &kern_spec->tcp_udp.val,
		       sizeof(struct ib_flow_tcp_udp_filter));
		memcpy(&ib_spec->tcp_udp.mask, &kern_spec->tcp_udp.mask,
		       sizeof(struct ib_flow_tcp_udp_filter));
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

int ib_uverbs_ex_create_flow(struct ib_uverbs_file *file,
			     struct ib_udata *ucore,
			     struct ib_udata *uhw)
{
	struct ib_uverbs_create_flow	  cmd;
	struct ib_uverbs_create_flow_resp resp;
	struct ib_uobject		*uobj;
	struct ib_flow			  *flow_id;
	struct ib_uverbs_flow_attr	  *kern_flow_attr;
	struct ib_flow_attr		  *flow_attr;
	struct ib_qp			  *qp;
	int err = 0;
	void *kern_spec;
	void *ib_spec;
	int i;

	if (ucore->outlen < sizeof(resp))
		return -ENOSPC;

	err = ib_copy_from_udata(&cmd, ucore, sizeof(cmd));
	if (err)
		return err;

	ucore->inbuf += sizeof(cmd);
	ucore->inlen -= sizeof(cmd);

	if (cmd.comp_mask)
		return -EINVAL;

	if (!priv_check(curthread, PRIV_NET_RAW) && !disable_raw_qp_enforcement)
		return -EPERM;

	if (cmd.flow_attr.num_of_specs > IB_FLOW_SPEC_SUPPORT_LAYERS)
		return -EINVAL;

	if (cmd.flow_attr.size > ucore->inlen ||
	    cmd.flow_attr.size >
	    (cmd.flow_attr.num_of_specs * sizeof(struct ib_uverbs_flow_spec)))
		return -EINVAL;

	if (cmd.flow_attr.num_of_specs) {
		kern_flow_attr = kmalloc(sizeof(*kern_flow_attr) +
					 cmd.flow_attr.size, GFP_KERNEL);
		if (!kern_flow_attr)
		return -ENOMEM;

		memcpy(kern_flow_attr, &cmd.flow_attr, sizeof(*kern_flow_attr));
		err = ib_copy_from_udata(kern_flow_attr + 1, ucore,
					 cmd.flow_attr.size);
		if (err)
			goto err_free_attr;
	} else {
		kern_flow_attr = &cmd.flow_attr;
	}

	uobj = kmalloc(sizeof(*uobj), GFP_KERNEL);
	if (!uobj) {
		err = -ENOMEM;
		goto err_free_attr;
	}
	init_uobj(uobj, 0, file->ucontext, &rule_lock_class);
	down_write(&uobj->mutex);

	qp = idr_read_qp(cmd.qp_handle, file->ucontext);
	if (!qp) {
		err = -EINVAL;
		goto err_uobj;
	}

	flow_attr = kmalloc(sizeof(*flow_attr) + cmd.flow_attr.size,
			    GFP_KERNEL);
	if (!flow_attr) {
		err = -ENOMEM;
		goto err_put;
	}

	flow_attr->type = kern_flow_attr->type;
	flow_attr->priority = kern_flow_attr->priority;
	flow_attr->num_of_specs = kern_flow_attr->num_of_specs;
	flow_attr->port = kern_flow_attr->port;
	flow_attr->flags = kern_flow_attr->flags;
	flow_attr->size = sizeof(*flow_attr);

	kern_spec = kern_flow_attr + 1;
	ib_spec = flow_attr + 1;
	for (i = 0; i < flow_attr->num_of_specs &&
	     cmd.flow_attr.size >
	     offsetof(struct ib_uverbs_flow_spec, reserved) &&
	     cmd.flow_attr.size >=
	     ((struct ib_uverbs_flow_spec *)kern_spec)->size; i++) {
		err = kern_spec_to_ib_spec(kern_spec, ib_spec);
		if (err)
			goto err_free;
		flow_attr->size +=
			((union ib_flow_spec *)ib_spec)->size;
		cmd.flow_attr.size -=
			((struct ib_uverbs_flow_spec *)kern_spec)->size;
		kern_spec += ((struct ib_uverbs_flow_spec *)kern_spec)->size;
		ib_spec += ((union ib_flow_spec *)ib_spec)->size;
	}
	if (cmd.flow_attr.size || (i != flow_attr->num_of_specs)) {
		pr_warn("create flow failed, flow %d: %d bytes left from uverb cmd\n",
			i, cmd.flow_attr.size);
		goto err_free;
		}
	flow_id = ib_create_flow(qp, flow_attr, IB_FLOW_DOMAIN_USER);
	if (IS_ERR(flow_id)) {
		err = PTR_ERR(flow_id);
		goto err_free;
	}
	flow_id->qp = qp;
	flow_id->uobject = uobj;
	uobj->object = flow_id;

	err = idr_add_uobj(&ib_uverbs_rule_idr, uobj);
	if (err)
		goto destroy_flow;

	memset(&resp, 0, sizeof(resp));
	resp.flow_handle = uobj->id;

	err = ib_copy_to_udata(ucore,
			       &resp, sizeof(resp));
	if (err)
		goto err_copy;

	put_qp_read(qp);
	mutex_lock(&file->mutex);
	list_add_tail(&uobj->list, &file->ucontext->rule_list);
	mutex_unlock(&file->mutex);

	uobj->live = 1;

	up_write(&uobj->mutex);
	kfree(flow_attr);
	if (cmd.flow_attr.num_of_specs)
		kfree(kern_flow_attr);
	return 0;
err_copy:
	idr_remove_uobj(&ib_uverbs_rule_idr, uobj);
destroy_flow:
	ib_destroy_flow(flow_id);
err_free:
	kfree(flow_attr);
err_put:
	put_qp_read(qp);
err_uobj:
	put_uobj_write(uobj);
err_free_attr:
	if (cmd.flow_attr.num_of_specs)
		kfree(kern_flow_attr);
	return err;
}

int ib_uverbs_ex_destroy_flow(struct ib_uverbs_file *file,
			      struct ib_udata *ucore,
			      struct ib_udata *uhw)
{
	struct ib_uverbs_destroy_flow	cmd;
	struct ib_flow			*flow_id;
	struct ib_uobject		*uobj;
	int				ret;

	ret = ib_copy_from_udata(&cmd, ucore, sizeof(cmd));
	if (ret)
		return ret;

	uobj = idr_write_uobj(&ib_uverbs_rule_idr, cmd.flow_handle,
			      file->ucontext);
	if (!uobj)
		return -EINVAL;
	flow_id = uobj->object;

	ret = ib_destroy_flow(flow_id);
	if (!ret)
		uobj->live = 0;

	put_uobj_write(uobj);

	idr_remove_uobj(&ib_uverbs_rule_idr, uobj);

	mutex_lock(&file->mutex);
	list_del(&uobj->list);
	mutex_unlock(&file->mutex);

	put_uobj(uobj);

	return ret;
}

ssize_t ib_uverbs_exp_modify_qp(struct ib_uverbs_file *file,
				struct ib_udata *ucore, struct ib_udata *uhw)
{
	const char __user *buf = ucore->inbuf;
	int in_len = ucore->inlen + uhw->inlen;
	int out_len = ucore->outlen + uhw->outlen;

	return __uverbs_modify_qp(file, buf, in_len, out_len,
				  IB_USER_VERBS_CMD_EXTENDED);
}


ssize_t ib_uverbs_exp_create_cq(struct ib_uverbs_file *file,
				struct ib_udata *ucore, struct ib_udata *uhw)
{
	const char __user *buf = ucore->inbuf;
	int in_len = ucore->inlen + uhw->inlen;
	int out_len = ucore->outlen + uhw->outlen;
	struct ib_uverbs_create_cq_ex cmd;

	if (copy_from_user(&cmd, buf, sizeof(cmd)))
		return -EFAULT;

	return create_cq(file, buf, in_len, out_len, &cmd,
			 IB_USER_VERBS_CMD_EXTENDED, ucore->outbuf);
}

ssize_t ib_uverbs_exp_modify_cq(struct ib_uverbs_file *file,
				struct ib_udata *ucore, struct ib_udata *uhw)
{
	const char __user *buf = ucore->inbuf;
	int in_len = ucore->inlen + uhw->inlen;
	struct ib_uverbs_modify_cq_ex cmd;
	struct ib_cq               *cq;
	struct ib_cq_attr           attr;
	int ret;

	if (copy_from_user(&cmd, buf, sizeof(cmd)))
		return -EFAULT;

	cq = idr_read_cq(cmd.cq_handle, file->ucontext, 0);
	if (!cq)
		return -EINVAL;

	attr.moderation.cq_count  = cmd.cq_count;
	attr.moderation.cq_period = cmd.cq_period;
	attr.cq_cap_flags         = cmd.cq_cap_flags;

	ret = ib_modify_cq(cq, &attr, cmd.attr_mask);

	put_cq_read(cq);

	return ret ? ret : in_len;
}


ssize_t ib_uverbs_exp_query_device(struct ib_uverbs_file *file,
				   struct ib_udata *ucore, struct ib_udata *uhw)
{
	struct ib_uverbs_exp_query_device_resp resp;
	struct ib_exp_device_attr              exp_attr;
	int                                    ret;

	if (ucore->outlen + uhw->outlen < sizeof(resp))
		return -ENOSPC;

	memset(&resp, 0, sizeof(resp));
	memset(&exp_attr, 0, sizeof(exp_attr));
	ret = ib_exp_query_device(file->device->ib_dev, &exp_attr);
	if (ret)
		return ret;

	ib_uverbs_query_device_assign(&resp.base, &exp_attr.base, file);

	resp.comp_mask = 0;
	resp.device_cap_flags2 = 0;

	/*
	 * Handle regular attr fields
	 */
	if (exp_attr.base.comp_mask & IB_DEVICE_ATTR_WITH_TIMESTAMP_MASK) {
		resp.timestamp_mask = exp_attr.base.timestamp_mask;
		resp.comp_mask |= IB_EXP_DEVICE_ATTR_WITH_TIMESTAMP_MASK;
	}

	if (exp_attr.base.comp_mask & IB_DEVICE_ATTR_WITH_HCA_CORE_CLOCK) {
		resp.hca_core_clock = exp_attr.base.hca_core_clock;
		resp.comp_mask |= IB_EXP_DEVICE_ATTR_WITH_HCA_CORE_CLOCK;
	}

	/*
	 *  Handle experimental attr fields
	 */
	if (exp_attr.exp_comp_mask & IB_EXP_DEVICE_ATTR_CAP_FLAGS2) {
		resp.device_cap_flags2 = exp_attr.device_cap_flags2;
		resp.comp_mask |= IB_EXP_DEVICE_ATTR_CAP_FLAGS2;
	}

	if (exp_attr.exp_comp_mask & IB_EXP_DEVICE_ATTR_DC_REQ_RD) {
		resp.dc_rd_req = exp_attr.dc_rd_req;
		resp.comp_mask |= IB_EXP_DEVICE_ATTR_DC_REQ_RD;
	}

	if (exp_attr.exp_comp_mask & IB_EXP_DEVICE_ATTR_DC_RES_RD) {
		resp.dc_rd_res = exp_attr.dc_rd_res;
		resp.comp_mask |= IB_EXP_DEVICE_ATTR_DC_RES_RD;
	}

	if (exp_attr.exp_comp_mask & IB_EXP_DEVICE_ATTR_INLINE_RECV_SZ) {
		resp.inline_recv_sz = exp_attr.inline_recv_sz;
		resp.comp_mask |= IB_EXP_DEVICE_ATTR_INLINE_RECV_SZ;
	}

	if (exp_attr.exp_comp_mask & IB_EXP_DEVICE_ATTR_RSS_TBL_SZ) {
		resp.max_rss_tbl_sz = exp_attr.max_rss_tbl_sz;
		resp.comp_mask |= IB_EXP_DEVICE_ATTR_RSS_TBL_SZ;
	}

	if (copy_to_user(ucore->outbuf, &resp, sizeof(resp)))
		return -EFAULT;

	return ucore->inlen + uhw->inlen;
}

ssize_t ib_uverbs_exp_create_qp(struct ib_uverbs_file *file,
				struct ib_udata *ucore, struct ib_udata *uhw)
{
	struct ib_uqp_object           *obj;
	struct ib_device	       *device;
	struct ib_pd                   *pd = NULL;
	struct ib_xrcd		       *xrcd = NULL;
	struct ib_uobject	       *uninitialized_var(xrcd_uobj);
	struct ib_cq                   *scq = NULL, *rcq = NULL;
	struct ib_srq                  *srq = NULL;
	struct ib_qp                   *qp;
	struct ib_exp_qp_init_attr	attr;
	int				ret;
	struct ib_uverbs_exp_create_qp	cmd_exp;
	struct ib_uverbs_exp_create_qp_resp resp_exp;
	struct ib_qp                   *parentqp = NULL;

	memset(&cmd_exp, 0, sizeof(cmd_exp));

	ret = ucore->ops->copy_from(&cmd_exp, ucore, sizeof(cmd_exp));
	if (ret)
		return ret;

	if (!disable_raw_qp_enforcement &&
	    cmd_exp.qp_type == IB_QPT_RAW_PACKET && !priv_check(curthread,
		    PRIV_NET_RAW))
		return -EPERM;

	obj = kzalloc(sizeof(*obj), GFP_KERNEL);
	if (!obj)
		return -ENOMEM;

	init_uobj(&obj->uevent.uobject, cmd_exp.user_handle, file->ucontext,
		  &qp_lock_class);
	down_write(&obj->uevent.uobject.mutex);

	if (cmd_exp.qp_type == IB_QPT_XRC_TGT) {
		xrcd = idr_read_xrcd(cmd_exp.pd_handle, file->ucontext, &xrcd_uobj);
		if (!xrcd) {
			ret = -EINVAL;
			goto err_put;
		}
		device = xrcd->device;
	} else {
		if (cmd_exp.qp_type == IB_QPT_XRC_INI) {
			cmd_exp.max_recv_wr = 0;
			cmd_exp.max_recv_sge = 0;
		} else {
			if (cmd_exp.is_srq) {
				srq = idr_read_srq(cmd_exp.srq_handle, file->ucontext);
				if (!srq || srq->srq_type != IB_SRQT_BASIC) {
					ret = -EINVAL;
					goto err_put;
				}
			}

			if (cmd_exp.recv_cq_handle != cmd_exp.send_cq_handle) {
				rcq = idr_read_cq(cmd_exp.recv_cq_handle, file->ucontext, 0);
				if (!rcq) {
					ret = -EINVAL;
					goto err_put;
				}
			}
		}

		scq = idr_read_cq(cmd_exp.send_cq_handle, file->ucontext, !!rcq);
		rcq = rcq ?: scq;
		pd  = idr_read_pd(cmd_exp.pd_handle, file->ucontext);
		if (!pd || !scq) {
			ret = -EINVAL;
			goto err_put;
		}

		device = pd->device;
	}

	memset(&attr, 0, sizeof(attr));
	attr.event_handler = ib_uverbs_qp_event_handler;
	attr.qp_context    = file;
	attr.send_cq       = scq;
	attr.recv_cq       = rcq;
	attr.srq           = srq;
	attr.xrcd	   = xrcd;
	attr.sq_sig_type   = cmd_exp.sq_sig_all ? IB_SIGNAL_ALL_WR : IB_SIGNAL_REQ_WR;
	attr.qp_type       = cmd_exp.qp_type;
	attr.create_flags  = 0;

	attr.cap.max_send_wr     = cmd_exp.max_send_wr;
	attr.cap.max_recv_wr     = cmd_exp.max_recv_wr;
	attr.cap.max_send_sge    = cmd_exp.max_send_sge;
	attr.cap.max_recv_sge    = cmd_exp.max_recv_sge;
	attr.cap.max_inline_data = cmd_exp.max_inline_data;

	if (cmd_exp.comp_mask & IB_UVERBS_EXP_CREATE_QP_CAP_FLAGS)
		attr.create_flags |= cmd_exp.qp_cap_flags &
				     (IB_QP_CREATE_CROSS_CHANNEL |
				      IB_QP_CREATE_MANAGED_SEND |
				      IB_QP_CREATE_MANAGED_RECV);

	if (cmd_exp.comp_mask & IB_UVERBS_EXP_CREATE_QP_QPG) {
		struct ib_uverbs_qpg *qpg;
		if (cmd_exp.qp_type != IB_QPT_RAW_PACKET &&
		    cmd_exp.qp_type != IB_QPT_UD) {
			ret = -EINVAL;
			goto err_put;
		}
		qpg = &cmd_exp.qpg;
		switch (qpg->qpg_type) {
		case IB_QPG_PARENT:
			attr.parent_attrib.rss_child_count =
				qpg->parent_attrib.rss_child_count;
			attr.parent_attrib.tss_child_count =
				qpg->parent_attrib.tss_child_count;
			break;
		case IB_QPG_CHILD_RX:
		case IB_QPG_CHILD_TX:
			parentqp = idr_read_qp(qpg->parent_handle,
					       file->ucontext);
			if (!parentqp) {
				ret = -EINVAL;
				goto err_put;
			}
			attr.qpg_parent = parentqp;
			break;
		default:
			ret = -EINVAL;
			goto err_put;
		}
		attr.qpg_type = qpg->qpg_type;
	}

	if (cmd_exp.comp_mask & IB_UVERBS_EXP_CREATE_QP_INL_RECV)
		attr.max_inl_recv = cmd_exp.max_inl_recv;

	obj->uevent.events_reported     = 0;
	INIT_LIST_HEAD(&obj->uevent.event_list);
	INIT_LIST_HEAD(&obj->mcast_list);

	if (cmd_exp.qp_type == IB_QPT_XRC_TGT)
		qp = ib_create_qp(pd, (struct ib_qp_init_attr *)&attr);
	else
		qp = device->exp_create_qp(pd, &attr, uhw);

	if (IS_ERR(qp)) {
		ret = PTR_ERR(qp);
		goto err_put;
	}

	if (cmd_exp.qp_type != IB_QPT_XRC_TGT) {
		qp->real_qp	  = qp;
		qp->device	  = device;
		qp->pd		  = pd;
		qp->send_cq	  = attr.send_cq;
		qp->recv_cq	  = attr.recv_cq;
		qp->srq		  = attr.srq;
		qp->event_handler = attr.event_handler;
		qp->qp_context	  = attr.qp_context;
		qp->qp_type	  = attr.qp_type;
		atomic_set(&qp->usecnt, 0);
		atomic_inc(&pd->usecnt);
		atomic_inc(&attr.send_cq->usecnt);
		if (attr.recv_cq)
			atomic_inc(&attr.recv_cq->usecnt);
		if (attr.srq)
			atomic_inc(&attr.srq->usecnt);
	}
	qp->uobject = &obj->uevent.uobject;

	obj->uevent.uobject.object = qp;
	ret = idr_add_uobj(&ib_uverbs_qp_idr, &obj->uevent.uobject);
	if (ret)
		goto err_destroy;

	memset(&resp_exp, 0, sizeof(resp_exp));
	resp_exp.qpn             = qp->qp_num;
	resp_exp.qp_handle       = obj->uevent.uobject.id;
	resp_exp.max_recv_sge    = attr.cap.max_recv_sge;
	resp_exp.max_send_sge    = attr.cap.max_send_sge;
	resp_exp.max_recv_wr     = attr.cap.max_recv_wr;
	resp_exp.max_send_wr     = attr.cap.max_send_wr;
	resp_exp.max_inline_data = attr.cap.max_inline_data;

	if (cmd_exp.comp_mask & IB_UVERBS_EXP_CREATE_QP_INL_RECV) {
		resp_exp.comp_mask |= IB_UVERBS_EXP_CREATE_QP_RESP_INL_RECV;
		resp_exp.max_inl_recv = attr.max_inl_recv;
	}

	ret = ucore->ops->copy_to(ucore, &resp_exp, sizeof(resp_exp));
	if (ret)
		goto err_copy;

	if (xrcd) {
		obj->uxrcd = container_of(xrcd_uobj, struct ib_uxrcd_object, uobject);
		atomic_inc(&obj->uxrcd->refcnt);
		put_xrcd_read(xrcd_uobj);
	}

	if (pd)
		put_pd_read(pd);
	if (scq)
		put_cq_read(scq);
	if (rcq && rcq != scq)
		put_cq_read(rcq);
	if (srq)
		put_srq_read(srq);
	if (parentqp)
		put_qp_read(parentqp);

	mutex_lock(&file->mutex);
	list_add_tail(&obj->uevent.uobject.list, &file->ucontext->qp_list);
	mutex_unlock(&file->mutex);

	obj->uevent.uobject.live = 1;

	up_write(&obj->uevent.uobject.mutex);

	return ucore->inlen + uhw->inlen;

err_copy:
	idr_remove_uobj(&ib_uverbs_qp_idr, &obj->uevent.uobject);

err_destroy:
	ib_destroy_qp(qp);

err_put:
	if (xrcd)
		put_xrcd_read(xrcd_uobj);
	if (pd)
		put_pd_read(pd);
	if (scq)
		put_cq_read(scq);
	if (rcq && rcq != scq)
		put_cq_read(rcq);
	if (srq)
		put_srq_read(srq);
	if (parentqp)
		put_qp_read(parentqp);

	put_uobj_write(&obj->uevent.uobject);
	return ret;
}

int ib_exp_query_device(struct ib_device *device,
			struct ib_exp_device_attr *device_attr)
{
	return device->exp_query_device(device, device_attr);
}
EXPORT_SYMBOL(ib_exp_query_device);
