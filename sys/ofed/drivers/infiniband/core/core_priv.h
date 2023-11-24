/*-
 * SPDX-License-Identifier: BSD-2-Clause OR GPL-2.0
 *
 * Copyright (c) 2004 Topspin Communications.  All rights reserved.
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

#ifndef _CORE_PRIV_H
#define _CORE_PRIV_H

#include <linux/list.h>
#include <linux/spinlock.h>

#include <rdma/ib_verbs.h>

#include <net/if_vlan_var.h>

/* Total number of ports combined across all struct ib_devices's */
#define RDMA_MAX_PORTS 8192

#ifdef CONFIG_INFINIBAND_ADDR_TRANS_CONFIGFS
int cma_configfs_init(void);
void cma_configfs_exit(void);
#else
static inline int cma_configfs_init(void)
{
	return 0;
}

static inline void cma_configfs_exit(void)
{
}
#endif
struct cma_device;
void cma_ref_dev(struct cma_device *cma_dev);
void cma_deref_dev(struct cma_device *cma_dev);
typedef bool (*cma_device_filter)(struct ib_device *, void *);
struct cma_device *cma_enum_devices_by_ibdev(cma_device_filter	filter,
					     void		*cookie);
int cma_get_default_gid_type(struct cma_device *cma_dev,
			     unsigned int port);
int cma_set_default_gid_type(struct cma_device *cma_dev,
			     unsigned int port,
			     enum ib_gid_type default_gid_type);
struct ib_device *cma_get_ib_dev(struct cma_device *cma_dev);

int  ib_device_register_sysfs(struct ib_device *device,
			      int (*port_callback)(struct ib_device *,
						   u8, struct kobject *));
void ib_device_unregister_sysfs(struct ib_device *device);

void ib_cache_setup(void);
void ib_cache_cleanup(void);

typedef void (*roce_netdev_callback)(struct ib_device *device, u8 port,
	      if_t idev, void *cookie);

typedef int (*roce_netdev_filter)(struct ib_device *device, u8 port,
	     if_t idev, void *cookie);

void ib_enum_roce_netdev(struct ib_device *ib_dev,
			 roce_netdev_filter filter,
			 void *filter_cookie,
			 roce_netdev_callback cb,
			 void *cookie);
void ib_enum_all_roce_netdevs(roce_netdev_filter filter,
			      void *filter_cookie,
			      roce_netdev_callback cb,
			      void *cookie);

enum ib_cache_gid_default_mode {
	IB_CACHE_GID_DEFAULT_MODE_SET,
	IB_CACHE_GID_DEFAULT_MODE_DELETE
};

int ib_cache_gid_parse_type_str(const char *buf);

const char *ib_cache_gid_type_str(enum ib_gid_type gid_type);

void ib_cache_gid_set_default_gid(struct ib_device *ib_dev, u8 port,
				  if_t ndev,
				  unsigned long gid_type_mask,
				  enum ib_cache_gid_default_mode mode);

int ib_cache_gid_add(struct ib_device *ib_dev, u8 port,
		     union ib_gid *gid, struct ib_gid_attr *attr);

int ib_cache_gid_del(struct ib_device *ib_dev, u8 port,
		     union ib_gid *gid, struct ib_gid_attr *attr);

int ib_cache_gid_del_all_netdev_gids(struct ib_device *ib_dev, u8 port,
				     if_t ndev);
void ib_cache_gid_del_all_by_netdev(if_t ndev);

int roce_gid_mgmt_init(void);
void roce_gid_mgmt_cleanup(void);

int roce_rescan_device(struct ib_device *ib_dev);
unsigned long roce_gid_type_mask_support(struct ib_device *ib_dev, u8 port);

int ib_cache_setup_one(struct ib_device *device);
void ib_cache_cleanup_one(struct ib_device *device);
void ib_cache_release_one(struct ib_device *device);

#define	ib_rdmacg_try_charge(...) ({ 0; })

int addr_init(void);
void addr_cleanup(void);

int ib_mad_init(void);
void ib_mad_cleanup(void);

int ib_sa_init(void);
void ib_sa_cleanup(void);

int ib_port_register_module_stat(struct ib_device *device, u8 port_num,
				 struct kobject *kobj, struct kobj_type *ktype,
				 const char *name);
void ib_port_unregister_module_stat(struct kobject *kobj);

static inline struct ib_qp *_ib_create_qp(struct ib_device *dev,
					  struct ib_pd *pd,
					  struct ib_qp_init_attr *attr,
					  struct ib_udata *udata,
					  struct ib_uqp_object *uobj)
{
	struct ib_qp *qp;

	if (!dev->create_qp)
		return ERR_PTR(-EOPNOTSUPP);

	qp = dev->create_qp(pd, attr, udata);
	if (IS_ERR(qp))
		return qp;

	qp->device = dev;
	qp->pd = pd;
	qp->uobject = uobj;
	qp->real_qp = qp;

	qp->qp_type = attr->qp_type;
	qp->rwq_ind_tbl = attr->rwq_ind_tbl;
	qp->send_cq = attr->send_cq;
	qp->recv_cq = attr->recv_cq;
	qp->srq = attr->srq;
	qp->rwq_ind_tbl = attr->rwq_ind_tbl;
	qp->event_handler = attr->event_handler;

	atomic_set(&qp->usecnt, 0);
	spin_lock_init(&qp->mr_lock);

	return qp;
}

struct rdma_umap_priv {
	struct vm_area_struct *vma;
	struct list_head list;
	struct rdma_user_mmap_entry *entry;
};

void rdma_umap_priv_init(struct rdma_umap_priv *priv,
			 struct vm_area_struct *vma,
			 struct rdma_user_mmap_entry *entry);

#endif /* _CORE_PRIV_H */
