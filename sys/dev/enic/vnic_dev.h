/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2008-2017 Cisco Systems, Inc.  All rights reserved.
 * Copyright 2007 Nuova Systems, Inc.  All rights reserved.
 */

#ifndef _VNIC_DEV_H_
#define _VNIC_DEV_H_

#include "enic_compat.h"
#include "vnic_resource.h"
#include "vnic_devcmd.h"

#ifndef VNIC_PADDR_TARGET
#define VNIC_PADDR_TARGET	0x0000000000000000ULL
#endif

enum vnic_dev_intr_mode {
	VNIC_DEV_INTR_MODE_UNKNOWN,
	VNIC_DEV_INTR_MODE_INTX,
	VNIC_DEV_INTR_MODE_MSI,
	VNIC_DEV_INTR_MODE_MSIX,
};

struct vnic_dev_bar {
	void __iomem *vaddr;
	unsigned long len;
};

struct vnic_dev_ring {
	void *descs;		/* vaddr */
	size_t size;
	bus_addr_t base_addr;	/* paddr */
	size_t base_align;
	void *descs_unaligned;
	size_t size_unaligned;
	bus_addr_t base_addr_unaligned;
	unsigned int desc_size;
	unsigned int desc_count;
	unsigned int desc_avail;
	unsigned int last_count;
	iflib_dma_info_t ifdip;
};

struct vnic_dev_iomap_info {
	bus_addr_t bus_addr;
	unsigned long len;
	void __iomem *vaddr;
};

struct vnic_dev;
struct vnic_stats;

void *vnic_dev_priv(struct vnic_dev *vdev);
unsigned int vnic_dev_get_res_count(struct vnic_dev *vdev,
    enum vnic_res_type type);
void vnic_register_cbacks(struct vnic_dev *vdev,
    void *(*alloc_consistent)(void *priv, size_t size,
	bus_addr_t *dma_handle, struct iflib_dma_info *res, u8 *name),
    void (*free_consistent)(void *priv,
	size_t size, void *vaddr,
	bus_addr_t dma_handle, struct iflib_dma_info *res));
void __iomem *vnic_dev_get_res(struct vnic_dev *vdev, enum vnic_res_type type,
    unsigned int index);
uint8_t vnic_dev_get_res_bar(struct vnic_dev *vdev,
    enum vnic_res_type type);
uint32_t vnic_dev_get_res_offset(struct vnic_dev *vdev,
    enum vnic_res_type type, unsigned int index);
unsigned long vnic_dev_get_res_type_len(struct vnic_dev *vdev,
					enum vnic_res_type type);
unsigned int vnic_dev_desc_ring_size(struct vnic_dev_ring *ring,
    unsigned int desc_count, unsigned int desc_size);
int vnic_dev_alloc_desc_ring(struct vnic_dev *vdev, struct vnic_dev_ring *ring,
    unsigned int desc_count, unsigned int desc_size);
void vnic_dev_free_desc_ring(struct vnic_dev *vdev,
    struct vnic_dev_ring *ring);
int vnic_dev_cmd(struct vnic_dev *vdev, enum vnic_devcmd_cmd cmd,
    u64 *a0, u64 *a1, int wait);
int vnic_dev_cmd_args(struct vnic_dev *vdev, enum vnic_devcmd_cmd cmd,
    u64 *args, int nargs, int wait);
void vnic_dev_cmd_proxy_by_index_start(struct vnic_dev *vdev, u16 index);
void vnic_dev_cmd_proxy_by_bdf_start(struct vnic_dev *vdev, u16 bdf);
void vnic_dev_cmd_proxy_end(struct vnic_dev *vdev);
int vnic_dev_fw_info(struct vnic_dev *vdev,
    struct vnic_devcmd_fw_info **fw_info);
int vnic_dev_capable_adv_filters(struct vnic_dev *vdev);
int vnic_dev_capable(struct vnic_dev *vdev, enum vnic_devcmd_cmd cmd);
int vnic_dev_capable_filter_mode(struct vnic_dev *vdev, u32 *mode,
    u8 *filter_actions);
void vnic_dev_capable_udp_rss_weak(struct vnic_dev *vdev, bool *cfg_chk,
    bool *weak);
int vnic_dev_asic_info(struct vnic_dev *vdev, u16 *asic_type, u16 *asic_rev);
int vnic_dev_spec(struct vnic_dev *vdev, unsigned int offset, size_t size,
    void *value);
int vnic_dev_stats_clear(struct vnic_dev *vdev);
int vnic_dev_stats_dump(struct vnic_dev *vdev, struct vnic_stats **stats);
int vnic_dev_counter_dma_cfg(struct vnic_dev *vdev, u32 period,
    u32 num_counters);
int vnic_dev_hang_notify(struct vnic_dev *vdev);
int vnic_dev_packet_filter(struct vnic_dev *vdev, int directed, int multicast,
    int broadcast, int promisc, int allmulti);
int vnic_dev_packet_filter_all(struct vnic_dev *vdev, int directed,
    int multicast, int broadcast, int promisc, int allmulti);
int vnic_dev_add_addr(struct vnic_dev *vdev, u8 *addr);
int vnic_dev_del_addr(struct vnic_dev *vdev, u8 *addr);
int vnic_dev_get_mac_addr(struct vnic_dev *vdev, u8 *mac_addr);
int vnic_dev_raise_intr(struct vnic_dev *vdev, u16 intr);
int vnic_dev_notify_set(struct vnic_dev *vdev, u16 intr);
void vnic_dev_set_reset_flag(struct vnic_dev *vdev, int state);
int vnic_dev_notify_unset(struct vnic_dev *vdev);
int vnic_dev_notify_setcmd(struct vnic_dev *vdev,
    void *notify_addr, bus_addr_t notify_pa, u16 intr);
int vnic_dev_notify_unsetcmd(struct vnic_dev *vdev);
int vnic_dev_link_status(struct vnic_dev *vdev);
u32 vnic_dev_port_speed(struct vnic_dev *vdev);
u32 vnic_dev_msg_lvl(struct vnic_dev *vdev);
u32 vnic_dev_mtu(struct vnic_dev *vdev);
u32 vnic_dev_link_down_cnt(struct vnic_dev *vdev);
u32 vnic_dev_notify_status(struct vnic_dev *vdev);
u32 vnic_dev_uif(struct vnic_dev *vdev);
int vnic_dev_close(struct vnic_dev *vdev);
int vnic_dev_enable(struct vnic_dev *vdev);
int vnic_dev_enable_wait(struct vnic_dev *vdev);
int vnic_dev_disable(struct vnic_dev *vdev);
int vnic_dev_open(struct vnic_dev *vdev, int arg);
int vnic_dev_open_done(struct vnic_dev *vdev, int *done);
int vnic_dev_init(struct vnic_dev *vdev, int arg);
int vnic_dev_init_done(struct vnic_dev *vdev, int *done, int *err);
int vnic_dev_init_prov(struct vnic_dev *vdev, u8 *buf, u32 len);
int vnic_dev_deinit(struct vnic_dev *vdev);
void vnic_dev_intr_coal_timer_info_default(struct vnic_dev *vdev);
int vnic_dev_intr_coal_timer_info(struct vnic_dev *vdev);
int vnic_dev_soft_reset(struct vnic_dev *vdev, int arg);
int vnic_dev_soft_reset_done(struct vnic_dev *vdev, int *done);
int vnic_dev_hang_reset(struct vnic_dev *vdev, int arg);
int vnic_dev_hang_reset_done(struct vnic_dev *vdev, int *done);
void vnic_dev_set_intr_mode(struct vnic_dev *vdev,
    enum vnic_dev_intr_mode intr_mode);
enum vnic_dev_intr_mode vnic_dev_get_intr_mode(struct vnic_dev *vdev);
u32 vnic_dev_intr_coal_timer_usec_to_hw(struct vnic_dev *vdev, u32 usec);
u32 vnic_dev_intr_coal_timer_hw_to_usec(struct vnic_dev *vdev, u32 hw_cycles);
u32 vnic_dev_get_intr_coal_timer_max(struct vnic_dev *vdev);
int vnic_dev_set_ig_vlan_rewrite_mode(struct vnic_dev *vdev,
    u8 ig_vlan_rewrite_mode);
struct enic;
struct vnic_dev *vnic_dev_register(struct vnic_dev *vdev,
    struct enic_bar_info *mem, unsigned int num_bars);
struct rte_pci_device *vnic_dev_get_pdev(struct vnic_dev *vdev);
int vnic_dev_alloc_stats_mem(struct vnic_dev *vdev);
int vnic_dev_alloc_counter_mem(struct vnic_dev *vdev);
int vnic_dev_cmd_init(struct vnic_dev *vdev);
int vnic_dev_get_size(void);
int vnic_dev_int13(struct vnic_dev *vdev, u64 arg, u32 op);
int vnic_dev_perbi(struct vnic_dev *vdev, u64 arg, u32 op);
u32 vnic_dev_perbi_rebuild_cnt(struct vnic_dev *vdev);
int vnic_dev_init_prov2(struct vnic_dev *vdev, u8 *buf, u32 len);
int vnic_dev_enable2(struct vnic_dev *vdev, int active);
int vnic_dev_enable2_done(struct vnic_dev *vdev, int *status);
int vnic_dev_deinit_done(struct vnic_dev *vdev, int *status);
int vnic_dev_set_mac_addr(struct vnic_dev *vdev, u8 *mac_addr);
int vnic_dev_classifier(struct vnic_dev *vdev, u8 cmd, u16 *entry,
    struct filter_v2 *data, struct filter_action_v2 *action_v2);
int vnic_dev_overlay_offload_ctrl(struct vnic_dev *vdev,
    u8 overlay, u8 config);
int vnic_dev_overlay_offload_cfg(struct vnic_dev *vdev, u8 overlay,
    u16 vxlan_udp_port_number);
int vnic_dev_capable_vxlan(struct vnic_dev *vdev);
bool vnic_dev_counter_alloc(struct vnic_dev *vdev, uint32_t *idx);
bool vnic_dev_counter_free(struct vnic_dev *vdev, uint32_t idx);
bool vnic_dev_counter_query(struct vnic_dev *vdev, uint32_t idx,
    bool reset, uint64_t *packets, uint64_t *bytes);
void vnic_dev_deinit_devcmd2(struct vnic_dev *vdev);

device_t dev_from_vnic_dev(struct vnic_dev *vdev);

#endif /* _VNIC_DEV_H_ */
