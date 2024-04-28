/*-
 * Broadcom NetXtreme-C/E network driver.
 *
 * Copyright (c) 2024 Broadcom, All Rights Reserved.
 * The term Broadcom refers to Broadcom Limited and/or its subsidiaries
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS'
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef BNXT_ULP_H
#define BNXT_ULP_H

#include <linux/rcupdate.h>
#include "bnxt.h"

#define BNXT_ROCE_ULP	0
#define BNXT_OTHER_ULP	1
#define BNXT_MAX_ULP	2

#define BNXT_MIN_ROCE_CP_RINGS	2
#define BNXT_MIN_ROCE_STAT_CTXS	1

struct hwrm_async_event_cmpl;
struct bnxt_softc;
struct bnxt_bar_info;

struct bnxt_msix_entry {
	uint32_t	vector;
	uint32_t	ring_idx;
	uint32_t	db_offset;
};

struct bnxt_ulp_ops {
	void (*ulp_async_notifier)(void *, struct hwrm_async_event_cmpl *);
	void (*ulp_stop)(void *);
	void (*ulp_start)(void *);
	void (*ulp_sriov_config)(void *, int);
	void (*ulp_shutdown)(void *);
	void (*ulp_irq_stop)(void *);
	void (*ulp_irq_restart)(void *, struct bnxt_msix_entry *);
};

struct bnxt_fw_msg {
	void	*msg;
	int	msg_len;
	void	*resp;
	int	resp_max_len;
	int	timeout;
};

struct bnxt_ulp {
	void		*handle;
	struct bnxt_ulp_ops __rcu *ulp_ops;
	unsigned long	*async_events_bmap;
	u16		max_async_event_id;
	u16		msix_requested;
	u16		msix_base;
	atomic_t	ref_count;
};

struct bnxt_en_dev {
	struct ifnet *net;
	struct pci_dev *pdev;
	struct bnxt_softc *softc;
	u32 flags;
	#define BNXT_EN_FLAG_ROCEV1_CAP		0x1
	#define BNXT_EN_FLAG_ROCEV2_CAP		0x2
	#define BNXT_EN_FLAG_ROCE_CAP		(BNXT_EN_FLAG_ROCEV1_CAP | \
						 BNXT_EN_FLAG_ROCEV2_CAP)
	#define BNXT_EN_FLAG_MSIX_REQUESTED	0x4
	#define BNXT_EN_FLAG_ULP_STOPPED	0x8
	#define BNXT_EN_FLAG_ASYM_Q		0x10
	#define BNXT_EN_FLAG_MULTI_HOST		0x20
#define BNXT_EN_ASYM_Q(edev)		((edev)->flags & BNXT_EN_FLAG_ASYM_Q)
#define BNXT_EN_MH(edev)		((edev)->flags & BNXT_EN_FLAG_MULTI_HOST)
	const struct bnxt_en_ops	*en_ops;
	struct bnxt_ulp			ulp_tbl[BNXT_MAX_ULP];
	int				l2_db_size;	/* Doorbell BAR size in
							 * bytes mapped by L2
							 * driver.
							 */
	int				l2_db_size_nc;	/* Doorbell BAR size in
							 * bytes mapped as non-
							 * cacheable.
							 */
	u32				ulp_version;	/* bnxt_re checks the
							 * ulp_version is correct
							 * to ensure compatibility
							 * with bnxt_en.
							 */
	#define BNXT_ULP_VERSION	0x695a0008	/* Change this when any interface
							 * structure or API changes
							 * between bnxt_en and bnxt_re.
							 */
	unsigned long			en_state;
	void __iomem			*bar0;
	u16				hw_ring_stats_size;
	u16				pf_port_id;
	u8				port_partition_type;
#define BNXT_EN_NPAR(edev)		((edev)->port_partition_type)
	u8				port_count;
	struct bnxt_dbr			*en_dbr;
	struct bnxt_bar_info		hwrm_bar;
	u32				espeed;
};

struct bnxt_en_ops {
	int (*bnxt_register_device)(struct bnxt_en_dev *, int,
				    struct bnxt_ulp_ops *, void *);
	int (*bnxt_unregister_device)(struct bnxt_en_dev *, int);
	int (*bnxt_request_msix)(struct bnxt_en_dev *, int,
				 struct bnxt_msix_entry *, int);
	int (*bnxt_free_msix)(struct bnxt_en_dev *, int);
	int (*bnxt_send_fw_msg)(struct bnxt_en_dev *, int,
				struct bnxt_fw_msg *);
	int (*bnxt_register_fw_async_events)(struct bnxt_en_dev *, int,
					     unsigned long *, u16);
	int (*bnxt_dbr_complete)(struct bnxt_en_dev *, int, u32);
};

static inline bool bnxt_ulp_registered(struct bnxt_en_dev *edev, int ulp_id)
{
	if (edev && rcu_access_pointer(edev->ulp_tbl[ulp_id].ulp_ops))
		return true;
	return false;
}

int bnxt_get_ulp_msix_num(struct bnxt_softc *bp);
int bnxt_get_ulp_msix_base(struct bnxt_softc *bp);
int bnxt_get_ulp_stat_ctxs(struct bnxt_softc *bp);
void bnxt_ulp_stop(struct bnxt_softc *bp);
void bnxt_ulp_start(struct bnxt_softc *bp, int err);
void bnxt_ulp_sriov_cfg(struct bnxt_softc *bp, int num_vfs);
void bnxt_ulp_shutdown(struct bnxt_softc *bp);
void bnxt_ulp_irq_stop(struct bnxt_softc *bp);
void bnxt_ulp_irq_restart(struct bnxt_softc *bp, int err);
void bnxt_ulp_async_events(struct bnxt_softc *bp, struct hwrm_async_event_cmpl *cmpl);
struct bnxt_en_dev *bnxt_ulp_probe(struct net_device *dev);
void bnxt_aux_dev_release(struct device *dev);
int bnxt_rdma_aux_device_add(struct bnxt_softc *bp);
int bnxt_rdma_aux_device_del(struct bnxt_softc *bp);
#endif
