/*-
 * SPDX-License-Identifier: GPL-2.0 or Linux-OpenIB
 *
 * Copyright (c) 2021 - 2022 Intel Corporation
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenFabrics.org BSD license below:
 *
 *   Redistribution and use in source and binary forms, with or
 *   without modification, are permitted provided that the following
 *   conditions are met:
 *
 *    - Redistributions of source code must retain the above
 *	copyright notice, this list of conditions and the following
 *	disclaimer.
 *
 *    - Redistributions in binary form must reproduce the above
 *	copyright notice, this list of conditions and the following
 *	disclaimer in the documentation and/or other materials
 *	provided with the distribution.
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
/*$FreeBSD$*/

#ifndef FBSD_KCOMPAT_H
#define FBSD_KCOMPAT_H
#include "ice_rdma.h"

#define TASKLET_DATA_TYPE	unsigned long
#define TASKLET_FUNC_TYPE	void (*)(TASKLET_DATA_TYPE)

#ifndef tasklet_setup
#define tasklet_setup(tasklet, callback)				\
	tasklet_init((tasklet), (TASKLET_FUNC_TYPE)(callback),		\
		      (TASKLET_DATA_TYPE)(tasklet))
#endif
#ifndef from_tasklet
#define from_tasklet(var, callback_tasklet, tasklet_fieldname) \
	container_of(callback_tasklet, typeof(*var), tasklet_fieldname)
#endif

#define set_ibdev_dma_device(ibdev, dev) \
	ibdev.dma_device = (dev)
#define set_max_sge(props, rf)  \
	((props)->max_sge = (rf)->sc_dev.hw_attrs.uk_attrs.max_hw_wq_frags)
#define kc_set_props_ip_gid_caps(props) \
	((props)->port_cap_flags  |= IB_PORT_IP_BASED_GIDS)
#define rdma_query_gid(ibdev, port, index, gid) \
	ib_get_cached_gid(ibdev, port, index, gid, NULL)
#define kmap(pg) page_address(pg)
#define kmap_local_page(pg) page_address(pg)
#define kunmap(pg)
#define kunmap_local(pg)
#define kc_free_lsmm_dereg_mr(iwdev, iwqp) \
	((iwdev)->ibdev.dereg_mr((iwqp)->lsmm_mr))

#define IB_UVERBS_CQ_FLAGS_TIMESTAMP_COMPLETION IB_CQ_FLAGS_TIMESTAMP_COMPLETION
#define kc_irdma_destroy_qp(ibqp, udata) irdma_destroy_qp(ibqp)
#ifndef IB_QP_ATTR_STANDARD_BITS
#define IB_QP_ATTR_STANDARD_BITS GENMASK(20, 0)
#endif

#define IRDMA_QOS_MODE_VLAN 0x0
#define IRDMA_QOS_MODE_DSCP 0x1

#define IRDMA_VER_LEN 24

void kc_set_roce_uverbs_cmd_mask(struct irdma_device *iwdev);
void kc_set_rdma_uverbs_cmd_mask(struct irdma_device *iwdev);

struct irdma_tunable_info {
	struct sysctl_ctx_list irdma_sysctl_ctx;
	struct sysctl_oid *irdma_sysctl_tree;
	char drv_ver[IRDMA_VER_LEN];
	u8 roce_ena;
};

static inline int irdma_iw_query_pkey(struct ib_device *ibdev, u8 port, u16 index,
				      u16 *pkey)
{
	*pkey = 0;
	return 0;
}

static inline int cq_validate_flags(u32 flags, u8 hw_rev)
{
	/* GEN1 does not support CQ create flags */
	if (hw_rev == IRDMA_GEN_1)
		return flags ? -EOPNOTSUPP : 0;

	return flags & ~IB_UVERBS_CQ_FLAGS_TIMESTAMP_COMPLETION ? -EOPNOTSUPP : 0;
}
static inline u64 *irdma_next_pbl_addr(u64 *pbl, struct irdma_pble_info **pinfo,
				       u32 *idx)
{
	*idx += 1;
	if (!(*pinfo) || *idx != (*pinfo)->cnt)
		return ++pbl;
	*idx = 0;
	(*pinfo)++;

	return (*pinfo)->addr;
}
struct ib_cq *irdma_create_cq(struct ib_device *ibdev,
			      const struct ib_cq_init_attr *attr,
			      struct ib_ucontext *context,
			      struct ib_udata *udata);
struct ib_qp *irdma_create_qp(struct ib_pd *ibpd,
			      struct ib_qp_init_attr *init_attr,
			      struct ib_udata *udata);
struct ib_ah *irdma_create_ah(struct ib_pd *ibpd,
			      struct ib_ah_attr *attr,
			      struct ib_udata *udata);
struct ib_ah *irdma_create_ah_stub(struct ib_pd *ibpd,
				   struct ib_ah_attr *attr,
				   struct ib_udata *udata);
void irdma_ether_copy(u8 *dmac, struct ib_ah_attr *attr);

int irdma_destroy_ah(struct ib_ah *ibah);
int irdma_destroy_ah_stub(struct ib_ah *ibah);
int irdma_destroy_qp(struct ib_qp *ibqp);
int irdma_dereg_mr(struct ib_mr *ib_mr);
void irdma_get_eth_speed_and_width(u32 link_speed, u8 *active_speed,
				   u8 *active_width);
enum rdma_link_layer irdma_get_link_layer(struct ib_device *ibdev,
					  u8 port_num);
int irdma_roce_port_immutable(struct ib_device *ibdev, u8 port_num,
			      struct ib_port_immutable *immutable);
int irdma_iw_port_immutable(struct ib_device *ibdev, u8 port_num,
			    struct ib_port_immutable *immutable);
int irdma_query_gid(struct ib_device *ibdev, u8 port, int index,
		    union ib_gid *gid);
int irdma_query_gid_roce(struct ib_device *ibdev, u8 port, int index,
			 union ib_gid *gid);
int irdma_query_pkey(struct ib_device *ibdev, u8 port, u16 index,
		     u16 *pkey);
int irdma_query_port(struct ib_device *ibdev, u8 port,
		     struct ib_port_attr *props);
struct rdma_hw_stats *irdma_alloc_hw_stats(struct ib_device *ibdev, u8 port_num);
int irdma_get_hw_stats(struct ib_device *ibdev,
		       struct rdma_hw_stats *stats, u8 port_num,
		       int index);

int irdma_register_qset(struct irdma_sc_vsi *vsi,
			struct irdma_ws_node *tc_node);
void irdma_unregister_qset(struct irdma_sc_vsi *vsi,
			   struct irdma_ws_node *tc_node);
void ib_unregister_device(struct ib_device *ibdev);
int rdma_user_mmap_io(struct ib_ucontext *ucontext, struct vm_area_struct *vma,
		      unsigned long pfn, unsigned long size, pgprot_t prot);
void irdma_disassociate_ucontext(struct ib_ucontext *context);
int kc_irdma_set_roce_cm_info(struct irdma_qp *iwqp,
			      struct ib_qp_attr *attr,
			      u16 *vlan_id);
struct irdma_device *kc_irdma_get_device(struct ifnet *netdev);
void kc_irdma_put_device(struct irdma_device *iwdev);

void kc_set_loc_seq_num_mss(struct irdma_cm_node *cm_node);
u16 kc_rdma_get_udp_sport(u32 fl, u32 lqpn, u32 rqpn);

void irdma_get_dev_fw_str(struct ib_device *dev, char *str, size_t str_len);

int irdma_modify_port(struct ib_device *ibdev, u8 port, int mask,
		      struct ib_port_modify *props);
int irdma_get_dst_mac(struct irdma_cm_node *cm_node, struct sockaddr *dst_sin,
		      u8 *dst_mac);
int irdma_resolve_neigh_lpb_chk(struct irdma_device *iwdev, struct irdma_cm_node *cm_node,
				struct irdma_cm_info *cm_info);
int irdma_addr_resolve_neigh(struct irdma_cm_node *cm_node, u32 dst_ip,
			     int arpindex);
int irdma_addr_resolve_neigh_ipv6(struct irdma_cm_node *cm_node, u32 *dest,
				  int arpindex);
void irdma_dcqcn_tunables_init(struct irdma_pci_f *rf);
u32 irdma_create_stag(struct irdma_device *iwdev);
void irdma_free_stag(struct irdma_device *iwdev, u32 stag);

int irdma_hwdereg_mr(struct ib_mr *ib_mr);
int irdma_rereg_user_mr(struct ib_mr *ib_mr, int flags, u64 start, u64 len,
			u64 virt, int new_access, struct ib_pd *new_pd,
			struct ib_udata *udata);
struct irdma_mr;
struct irdma_cq;
struct irdma_cq_buf;
struct ib_mr *irdma_alloc_mr(struct ib_pd *pd, enum ib_mr_type mr_type,
			     u32 max_num_sg);
int irdma_hwreg_mr(struct irdma_device *iwdev, struct irdma_mr *iwmr,
		   u16 access);
struct ib_mr *irdma_rereg_mr_trans(struct irdma_mr *iwmr, u64 start, u64 len,
				   u64 virt, struct ib_udata *udata);
int irdma_hw_alloc_mw(struct irdma_device *iwdev, struct irdma_mr *iwmr);
struct ib_mw *irdma_alloc_mw(struct ib_pd *pd, enum ib_mw_type type,
			     struct ib_udata *udata);
int irdma_hw_alloc_stag(struct irdma_device *iwdev, struct irdma_mr *iwmr);
void irdma_cq_free_rsrc(struct irdma_pci_f *rf, struct irdma_cq *iwcq);
int irdma_validate_qp_attrs(struct ib_qp_init_attr *init_attr,
			    struct irdma_device *iwdev);
void irdma_setup_virt_qp(struct irdma_device *iwdev,
                         struct irdma_qp *iwqp,
                         struct irdma_qp_init_info *init_info);
int irdma_setup_kmode_qp(struct irdma_device *iwdev,
			 struct irdma_qp *iwqp,
			 struct irdma_qp_init_info *info,
			 struct ib_qp_init_attr *init_attr);
int irdma_setup_umode_qp(struct ib_udata *udata,
			 struct irdma_device *iwdev,
			 struct irdma_qp *iwqp,
			 struct irdma_qp_init_info *info,
			 struct ib_qp_init_attr *init_attr);
void irdma_roce_fill_and_set_qpctx_info(struct irdma_qp *iwqp,
					struct irdma_qp_host_ctx_info *ctx_info);
void irdma_iw_fill_and_set_qpctx_info(struct irdma_qp *iwqp,
				      struct irdma_qp_host_ctx_info *ctx_info);
int irdma_cqp_create_qp_cmd(struct irdma_qp *iwqp);
void irdma_dealloc_push_page(struct irdma_pci_f *rf,
			     struct irdma_sc_qp *qp);
int irdma_process_resize_list(struct irdma_cq *iwcq, struct irdma_device *iwdev,
			      struct irdma_cq_buf *lcqe_buf);
int irdma_destroy_cq(struct ib_cq *ib_cq);
struct ib_ucontext *irdma_alloc_ucontext(struct ib_device *, struct ib_udata *);
int irdma_dealloc_ucontext(struct ib_ucontext *);
struct ib_pd *irdma_alloc_pd(struct ib_device *, struct ib_ucontext *,
			     struct ib_udata *);
int irdma_dealloc_pd(struct ib_pd *);
int irdma_add_gid(struct ib_device *, u8, unsigned int, const union ib_gid *,
		  const struct ib_gid_attr *, void **);
int irdma_del_gid(struct ib_device *, u8, unsigned int, void **);
struct ib_device *ib_device_get_by_netdev(struct ifnet *ndev, int driver_id);
void ib_device_put(struct ib_device *device);
void ib_unregister_device_put(struct ib_device *device);
enum ib_mtu ib_mtu_int_to_enum(int mtu);
struct irdma_pbl *irdma_get_pbl(unsigned long va, struct list_head *pbl_list);
void irdma_clean_cqes(struct irdma_qp *iwqp, struct irdma_cq *iwcq);
void irdma_remove_push_mmap_entries(struct irdma_qp *iwqp);

struct irdma_ucontext;
void irdma_del_memlist(struct irdma_mr *iwmr, struct irdma_ucontext *ucontext);
void irdma_copy_user_pgaddrs(struct irdma_mr *iwmr, u64 *pbl,
			     enum irdma_pble_level level);
void irdma_reg_ipaddr_event_cb(struct irdma_pci_f *rf);
void irdma_dereg_ipaddr_event_cb(struct irdma_pci_f *rf);

/* Introduced in this series https://lore.kernel.org/linux-rdma/0-v2-270386b7e60b+28f4-umem_1_jgg@nvidia.com/
 * An irdma version helper doing same for older functions with difference that iova is passed in
 * as opposed to derived from umem->iova.
 */
static inline size_t irdma_ib_umem_num_dma_blocks(struct ib_umem *umem, unsigned long pgsz, u64 iova)
{
	/* some older OFED distros do not have ALIGN_DOWN */
#ifndef ALIGN_DOWN
#define ALIGN_DOWN(x, a)	ALIGN((x) - ((a) - 1), (a))
#endif

	return (size_t)((ALIGN(iova + umem->length, pgsz) -
			 ALIGN_DOWN(iova, pgsz))) / pgsz;
}

#endif /* FBSD_KCOMPAT_H */
