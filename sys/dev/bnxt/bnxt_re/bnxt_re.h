/*
 * Copyright (c) 2015-2024, Broadcom. All rights reserved.  The term
 * Broadcom refers to Broadcom Limited and/or its subsidiaries.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Description: main (header)
 */

#ifndef __BNXT_RE_H__
#define __BNXT_RE_H__

#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/mutex.h>
#include <linux/list.h>
#include <linux/rculist.h>
#include <linux/spinlock.h>
#include <net/ipv6.h>
#include <linux/if_ether.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/interrupt.h>
#include <linux/vmalloc.h>
#include <linux/delay.h>
#include <rdma/ib_verbs.h>
#include <rdma/ib_user_verbs.h>
#include <rdma/ib_umem.h>
#include <rdma/ib_addr.h>
#include <rdma/ib_mad.h>
#include <rdma/ib_cache.h>
#include <linux/pci.h>

#include "bnxt.h"
#include "bnxt_ulp.h"
#include "hsi_struct_def.h"
#include "qplib_res.h"
#include "qplib_sp.h"
#include "qplib_fp.h"
#include "qplib_rcfw.h"
#include "ib_verbs.h"
#include "stats.h"

#define ROCE_DRV_MODULE_NAME		"bnxt_re"
#define ROCE_DRV_MODULE_VERSION		"230.0.133.0"
#define ROCE_DRV_MODULE_RELDATE		"April 22, 2024"

#define BNXT_RE_REF_WAIT_COUNT		20
#define BNXT_RE_ROCE_V1_ETH_TYPE	0x8915
#define BNXT_RE_ROCE_V2_PORT_NO		4791
#define BNXT_RE_RES_FREE_WAIT_COUNT	1000

#define BNXT_RE_PAGE_SHIFT_4K		(12)
#define BNXT_RE_PAGE_SHIFT_8K		(13)
#define BNXT_RE_PAGE_SHIFT_64K		(16)
#define BNXT_RE_PAGE_SHIFT_2M		(21)
#define BNXT_RE_PAGE_SHIFT_8M		(23)
#define BNXT_RE_PAGE_SHIFT_1G		(30)

#define BNXT_RE_PAGE_SIZE_4K		BIT(BNXT_RE_PAGE_SHIFT_4K)
#define BNXT_RE_PAGE_SIZE_8K		BIT(BNXT_RE_PAGE_SHIFT_8K)
#define BNXT_RE_PAGE_SIZE_64K		BIT(BNXT_RE_PAGE_SHIFT_64K)
#define BNXT_RE_PAGE_SIZE_2M		BIT(BNXT_RE_PAGE_SHIFT_2M)
#define BNXT_RE_PAGE_SIZE_8M		BIT(BNXT_RE_PAGE_SHIFT_8M)
#define BNXT_RE_PAGE_SIZE_1G		BIT(BNXT_RE_PAGE_SHIFT_1G)

#define BNXT_RE_MAX_MR_SIZE_LOW		BIT(BNXT_RE_PAGE_SHIFT_1G)
#define BNXT_RE_MAX_MR_SIZE_HIGH	BIT(39)
#define BNXT_RE_MAX_MR_SIZE		BNXT_RE_MAX_MR_SIZE_HIGH

/* Number of MRs to reserve for PF, leaving remainder for VFs */
#define BNXT_RE_RESVD_MR_FOR_PF		(32 * 1024)
#define BNXT_RE_MAX_GID_PER_VF		128

#define BNXT_RE_MAX_VF_QPS_PER_PF	(6 * 1024)

/**
 * min_not_zero - return the minimum that is _not_ zero, unless both are zero
 * @x: value1
 * @y: value2
 */
#ifndef min_not_zero
#define min_not_zero(x, y) ({			\
	typeof(x) __x = (x);			\
	typeof(y) __y = (y);			\
	__x == 0 ? __y : ((__y == 0) ? __x : min(__x, __y)); })
#endif

struct ib_mr_init_attr {
	int		max_reg_descriptors;
	u32		flags;
};

struct bnxt_re_dev;

int bnxt_re_register_netdevice_notifier(struct notifier_block *nb);
int bnxt_re_unregister_netdevice_notifier(struct notifier_block *nb);
int ib_register_device_compat(struct bnxt_re_dev *rdev);

#ifndef __struct_group
#define __struct_group(TAG, NAME, ATTRS, MEMBERS...) \
	union { \
		struct { MEMBERS } ATTRS; \
		struct TAG { MEMBERS } ATTRS NAME; \
	}
#endif /* __struct_group */
#ifndef struct_group_attr
#define struct_group_attr(NAME, ATTRS, MEMBERS...) \
	__struct_group(/* no tag */, NAME, ATTRS, MEMBERS)
#endif /* struct_group_attr */
/*
 * Percentage of resources of each type reserved for PF.
 * Remaining resources are divided equally among VFs.
 * [0, 100]
 */

#define BNXT_RE_RQ_WQE_THRESHOLD	32
#define BNXT_RE_UD_QP_HW_STALL		0x400000

/*
 * Setting the default ack delay value to 16, which means
 * the default timeout is approx. 260ms(4 usec * 2 ^(timeout))
 */

#define BNXT_RE_DEFAULT_ACK_DELAY	16
#define BNXT_RE_BOND_PF_MAX		2

#define BNXT_RE_STATS_CTX_UPDATE_TIMER	250
#define BNXT_RE_30SEC_MSEC		(30 * 1000)

#define BNXT_RE_BOND_RESCHED_CNT	10

#define BNXT_RE_CHIP_NUM_57454         0xC454
#define BNXT_RE_CHIP_NUM_57452         0xC452

#define BNXT_RE_CHIP_NUM_5745X(chip_num)          \
	((chip_num) == BNXT_RE_CHIP_NUM_57454 ||       \
	 (chip_num) == BNXT_RE_CHIP_NUM_57452)

#define BNXT_RE_MIN_KERNEL_QP_TX_DEPTH	4096
#define BNXT_RE_STOP_QPS_BUDGET		200

#define BNXT_RE_HWRM_CMD_TIMEOUT(rdev) \
		((rdev)->chip_ctx->hwrm_cmd_max_timeout * 1000)

extern unsigned int min_tx_depth;
extern struct mutex bnxt_re_dev_lock;
extern struct mutex bnxt_re_mutex;
extern struct list_head bnxt_re_dev_list;

struct bnxt_re_ring_attr {
	dma_addr_t	*dma_arr;
	int		pages;
	int	 	type;
	u32		depth;
	u32		lrid; /* Logical ring id */
	u16		flags;
	u8		mode;
	u8		rsvd;
};

#define BNXT_RE_MAX_DEVICES		256
#define BNXT_RE_MSIX_FROM_MOD_PARAM	-1
#define BNXT_RE_MIN_MSIX		2
#define BNXT_RE_MAX_MSIX_VF		2
#define BNXT_RE_MAX_MSIX_PF		9
#define BNXT_RE_MAX_MSIX_NPAR_PF	5
#define BNXT_RE_MAX_MSIX		64
#define BNXT_RE_MAX_MSIX_GEN_P5_PF	BNXT_RE_MAX_MSIX
#define BNXT_RE_GEN_P5_MAX_VF		64

struct bnxt_re_nq_record {
	struct bnxt_msix_entry	msix_entries[BNXT_RE_MAX_MSIX];
	/* FP Notification Queue (CQ & SRQ) */
	struct bnxt_qplib_nq    nq[BNXT_RE_MAX_MSIX];
	int			num_msix;
	int			max_init;
	struct mutex		load_lock;
};

struct bnxt_re_work {
	struct work_struct	work;
	unsigned long		event;
	struct bnxt_re_dev      *rdev;
	struct ifnet		*vlan_dev;
	bool do_lag;

	/* netdev where we received the event */
	struct ifnet *netdev;
	struct auxiliary_device *adev;
};

/*
 * Data structure and defines to handle
 * recovery
 */
#define BNXT_RE_RECOVERY_IB_UNINIT_WAIT_RETRY   20
#define BNXT_RE_RECOVERY_IB_UNINIT_WAIT_TIME_MS 30000 /* 30sec timeout */
#define BNXT_RE_PRE_RECOVERY_REMOVE 0x1
#define BNXT_RE_COMPLETE_REMOVE 0x2
#define BNXT_RE_POST_RECOVERY_INIT 0x4
#define BNXT_RE_COMPLETE_INIT 0x8
#define BNXT_RE_COMPLETE_SHUTDOWN 0x10

/* QP1 SQ entry data strucutre */
struct bnxt_re_sqp_entries {
	u64 wrid;
	struct bnxt_qplib_sge sge;
	/* For storing the actual qp1 cqe */
	struct bnxt_qplib_cqe cqe;
	struct bnxt_re_qp *qp1_qp;
};

/* GSI QP mode enum */
enum bnxt_re_gsi_mode {
	BNXT_RE_GSI_MODE_INVALID = 0,
	BNXT_RE_GSI_MODE_ALL = 1,
	BNXT_RE_GSI_MODE_ROCE_V1,
	BNXT_RE_GSI_MODE_ROCE_V2_IPV4,
	BNXT_RE_GSI_MODE_ROCE_V2_IPV6,
	BNXT_RE_GSI_MODE_UD
};

enum bnxt_re_roce_cap {
	BNXT_RE_FLAG_ROCEV1_CAP = 1,
	BNXT_RE_FLAG_ROCEV2_CAP,
	BNXT_RE_FLAG_ROCEV1_V2_CAP,
};

#define BNXT_RE_MAX_GSI_SQP_ENTRIES	1024
struct bnxt_re_gsi_context {
	u8	gsi_qp_mode;
	bool	first_cq_created;
	/* Start: used only in gsi_mode_all */
	struct	bnxt_re_qp *gsi_qp;
	struct	bnxt_re_qp *gsi_sqp;
	struct	bnxt_re_ah *gsi_sah;
	struct	bnxt_re_sqp_entries *sqp_tbl;
	/* End: used only in gsi_mode_all */
};

struct bnxt_re_tc_rec {
	u8 cos_id_roce;
	u8 tc_roce;
	u8 cos_id_cnp;
	u8 tc_cnp;
	u8 tc_def;
	u8 cos_id_def;
	u8 max_tc;
	u8 roce_prio;
	u8 cnp_prio;
	u8 roce_dscp;
	u8 cnp_dscp;
	u8 prio_valid;
	u8 dscp_valid;
	bool ecn_enabled;
	bool serv_type_enabled;
	u64 cnp_dscp_bv;
	u64 roce_dscp_bv;
};

struct bnxt_re_dscp2pri {
	u8 dscp;
	u8 mask;
	u8 pri;
};

struct bnxt_re_cos2bw_cfg {
	u8	pad[3];
	struct_group_attr(cfg, __packed,
		u8	queue_id;
		__le32	min_bw;
		__le32	max_bw;
		u8	tsa;
		u8	pri_lvl;
		u8	bw_weight;
	);
	u8	unused;
};

#define BNXT_RE_AEQ_IDX			0
#define BNXT_RE_MAX_SGID_ENTRIES	256

#define BNXT_RE_DBGFS_FILE_MEM          65536
enum {
	BNXT_RE_STATS_QUERY = 1,
	BNXT_RE_QP_QUERY = 2,
	BNXT_RE_SERVICE_FN_QUERY = 3,
};

struct bnxt_re_dbg_file {
	struct bnxt_re_dev *rdev;
	u32 type;
	union {
		struct bnxt_qplib_query_stats_info sinfo;
		struct bnxt_qplib_query_fn_info fninfo;
	}params;
	char dbg_buf[BNXT_RE_DBGFS_FILE_MEM];
};

struct bnxt_re_debug_entries {
	/* Dir entries */
	struct dentry *qpinfo_dir;
	struct dentry *service_fn_dir;
	/* file entries */
	struct dentry *stat_query;
	struct bnxt_re_dbg_file stat_file;
	struct dentry *qplist_query;
	struct bnxt_re_dbg_file qp_file;
	struct dentry *service_fn_query;
	struct bnxt_re_dbg_file service_fn_file;
};

struct bnxt_re_en_dev_info {
	struct list_head en_list;
	struct bnxt_en_dev *en_dev;
	struct bnxt_re_dev *rdev;
	unsigned long flags;
#define BNXT_RE_FLAG_EN_DEV_NETDEV_REG		0
#define BNXT_RE_FLAG_EN_DEV_PRIMARY_DEV		1
#define BNXT_RE_FLAG_EN_DEV_SECONDARY_DEV	2
	u8 wqe_mode;
	u8 gsi_mode;
	bool te_bypass;
	bool ib_uninit_done;
	u32 num_msix_requested;
	wait_queue_head_t waitq;
};

#define BNXT_RE_DB_FIFO_ROOM_MASK_P5	0x1FFF8000
#define BNXT_RE_MAX_FIFO_DEPTH_P5	0x2c00
#define BNXT_RE_DB_FIFO_ROOM_SHIFT	15

#define BNXT_RE_DB_FIFO_ROOM_MASK_P7	0x3FFF8000
#define BNXT_RE_MAX_FIFO_DEPTH_P7	0x8000

#define BNXT_RE_DB_FIFO_ROOM_MASK(ctx)	\
	(_is_chip_p7((ctx)) ? \
	 BNXT_RE_DB_FIFO_ROOM_MASK_P7 :\
	 BNXT_RE_DB_FIFO_ROOM_MASK_P5)
#define BNXT_RE_MAX_FIFO_DEPTH(ctx)	\
	(_is_chip_p7((ctx)) ? \
	 BNXT_RE_MAX_FIFO_DEPTH_P7 :\
	 BNXT_RE_MAX_FIFO_DEPTH_P5)

struct bnxt_dbq_nq_list {
	int num_nql_entries;
	u16 nq_id[16];
};

#define BNXT_RE_ASYNC_ERR_REP_BASE(_type)				\
	 (ASYNC_EVENT_CMPL_ERROR_REPORT_BASE_EVENT_DATA1_ERROR_##_type)

#define BNXT_RE_ASYNC_ERR_DBR_TRESH(_type)				\
	 (ASYNC_EVENT_CMPL_ERROR_REPORT_DOORBELL_DROP_THRESHOLD_##_type)

#define BNXT_RE_EVENT_DBR_EPOCH(data)					\
	(((data) &							\
	  BNXT_RE_ASYNC_ERR_DBR_TRESH(EVENT_DATA1_EPOCH_MASK)) >>	\
	 BNXT_RE_ASYNC_ERR_DBR_TRESH(EVENT_DATA1_EPOCH_SFT))

#define BNXT_RE_EVENT_ERROR_REPORT_TYPE(data1)				\
	(((data1) &							\
	  BNXT_RE_ASYNC_ERR_REP_BASE(TYPE_MASK))  >>			\
	 BNXT_RE_ASYNC_ERR_REP_BASE(TYPE_SFT))

#define BNXT_RE_DBR_LIST_ADD(_rdev, _res, _type)			\
{									\
	spin_lock(&(_rdev)->res_list[_type].lock);			\
	list_add_tail(&(_res)->dbr_list,				\
		      &(_rdev)->res_list[_type].head);			\
	spin_unlock(&(_rdev)->res_list[_type].lock);			\
}

#define BNXT_RE_DBR_LIST_DEL(_rdev, _res, _type)			\
{									\
	spin_lock(&(_rdev)->res_list[_type].lock);			\
	list_del(&(_res)->dbr_list);					\
	spin_unlock(&(_rdev)->res_list[_type].lock);			\
}

#define BNXT_RE_CQ_PAGE_LIST_ADD(_uctx, _cq)				\
{									\
	mutex_lock(&(_uctx)->cq_lock);					\
	list_add_tail(&(_cq)->cq_list, &(_uctx)->cq_list);		\
	mutex_unlock(&(_uctx)->cq_lock);				\
}

#define BNXT_RE_CQ_PAGE_LIST_DEL(_uctx, _cq)				\
{									\
	mutex_lock(&(_uctx)->cq_lock);					\
	list_del(&(_cq)->cq_list);					\
	mutex_unlock(&(_uctx)->cq_lock);				\
}

#define BNXT_RE_NETDEV_EVENT(event, x)					\
	do {								\
		if ((event) == (x))					\
			return #x;					\
	} while (0)

/* Do not change the seq of this enum which is followed by dbr recov */
enum {
	BNXT_RE_RES_TYPE_CQ = 0,
	BNXT_RE_RES_TYPE_UCTX,
	BNXT_RE_RES_TYPE_QP,
	BNXT_RE_RES_TYPE_SRQ,
	BNXT_RE_RES_TYPE_MAX
};

struct bnxt_re_dbr_res_list {
	struct list_head head;
	spinlock_t lock;
};

struct bnxt_re_dbr_drop_recov_work {
	struct work_struct work;
	struct bnxt_re_dev *rdev;
	u32 curr_epoch;
};

struct bnxt_re_aer_work {
	struct work_struct work;
	struct bnxt_re_dev *rdev;
};

struct bnxt_re_dbq_stats {
	u64 fifo_occup_slab_1;
	u64 fifo_occup_slab_2;
	u64 fifo_occup_slab_3;
	u64 fifo_occup_slab_4;
	u64 fifo_occup_water_mark;
	u64 do_pacing_slab_1;
	u64 do_pacing_slab_2;
	u64 do_pacing_slab_3;
	u64 do_pacing_slab_4;
	u64 do_pacing_slab_5;
	u64 do_pacing_water_mark;
};

/* Device debug statistics */
struct bnxt_re_drv_dbg_stats {
	struct bnxt_re_dbq_stats dbq;
};

/* DB pacing counters */
struct bnxt_re_dbr_sw_stats {
	u64 dbq_int_recv;
	u64 dbq_int_en;
	u64 dbq_pacing_resched;
	u64 dbq_pacing_complete;
	u64 dbq_pacing_alerts;
	u64 dbr_drop_recov_events;
	u64 dbr_drop_recov_timeouts;
	u64 dbr_drop_recov_timeout_users;
	u64 dbr_drop_recov_event_skips;
};

struct bnxt_re_dev {
	struct ib_device		ibdev;
	struct list_head		list;
	atomic_t			ref_count;
	atomic_t			sched_count;
	unsigned long			flags;
#define BNXT_RE_FLAG_NETDEV_REGISTERED		0
#define BNXT_RE_FLAG_IBDEV_REGISTERED		1
#define BNXT_RE_FLAG_GOT_MSIX			2
#define BNXT_RE_FLAG_HAVE_L2_REF		3
#define BNXT_RE_FLAG_ALLOC_RCFW			4
#define BNXT_RE_FLAG_NET_RING_ALLOC		5
#define BNXT_RE_FLAG_RCFW_CHANNEL_EN		6
#define BNXT_RE_FLAG_ALLOC_CTX			7
#define BNXT_RE_FLAG_STATS_CTX_ALLOC		8
#define BNXT_RE_FLAG_STATS_CTX2_ALLOC		9
#define BNXT_RE_FLAG_RCFW_CHANNEL_INIT		10
#define BNXT_RE_FLAG_WORKER_REG			11
#define BNXT_RE_FLAG_TBLS_ALLOCINIT		12
#define BNXT_RE_FLAG_SETUP_NQ			13
#define BNXT_RE_FLAG_BOND_DEV_REGISTERED	14
#define BNXT_RE_FLAG_PER_PORT_DEBUG_INFO	15
#define BNXT_RE_FLAG_DEV_LIST_INITIALIZED	16
#define BNXT_RE_FLAG_ERR_DEVICE_DETACHED	17
#define BNXT_RE_FLAG_INIT_DCBX_CC_PARAM		18
#define BNXT_RE_FLAG_STOP_IN_PROGRESS		20
#define BNXT_RE_FLAG_ISSUE_ROCE_STATS		29
#define BNXT_RE_FLAG_ISSUE_CFA_FLOW_STATS	30
	struct ifnet			*netdev;
	struct auxiliary_device		*adev;
	struct bnxt_qplib_chip_ctx	*chip_ctx;
	struct bnxt_en_dev		*en_dev;
	struct bnxt_re_nq_record	nqr;
	int				id;
	struct delayed_work		worker;
	u16				worker_30s;
	struct bnxt_re_tc_rec		tc_rec[2];
	u8				cur_prio_map;
	/* RCFW Channel */
	struct bnxt_qplib_rcfw		rcfw;
	/* Device Resources */
	struct bnxt_qplib_dev_attr	*dev_attr;
	struct bnxt_qplib_res		qplib_res;
	struct bnxt_qplib_dpi		dpi_privileged;
	struct bnxt_qplib_cc_param	cc_param;
	struct mutex			cc_lock;
	struct mutex			qp_lock;
	struct list_head		qp_list;
	u8				roce_mode;

	/* Max of 2 lossless traffic class supported per port */
	u16				cosq[2];
	/* Start: QP for handling QP1 packets */
	struct bnxt_re_gsi_context	gsi_ctx;
	/* End: QP for handling QP1 packets */
	bool				is_virtfn;
	u32				num_vfs;
	u32				espeed;
	/*
	 * For storing the speed of slave interfaces.
	 * Same as espeed when bond is not configured
	 */
	u32				sl_espeed;
	/* To be used for a workaround for ISER stack */
	u32				min_tx_depth;
	/* To enable qp debug info. Disabled during driver load */
	u32				en_qp_dbg;
	/* Array to handle gid mapping */
	char				*gid_map;

	struct bnxt_re_device_stats	stats;
	struct bnxt_re_drv_dbg_stats	*dbg_stats;
	/* debugfs to expose per port information*/
	struct dentry                   *port_debug_dir;
	struct dentry                   *info;
	struct dentry                   *drv_dbg_stats;
	struct dentry                   *sp_perf_stats;
	struct dentry                   *pdev_debug_dir;
	struct dentry                   *pdev_qpinfo_dir;
	struct bnxt_re_debug_entries	*dbg_ent;
	struct workqueue_struct		*resolve_wq;
	struct list_head		mac_wq_list;
	struct workqueue_struct		*dcb_wq;
	struct workqueue_struct		*aer_wq;
	u32				event_bitmap[3];
	bool unreg_sched;
	u64	dbr_throttling_reg_off;
	u64	dbr_aeq_arm_reg_off;
	u64	dbr_db_fifo_reg_off;
	void *dbr_page;
	u64 dbr_bar_addr;
	u32 pacing_algo_th;
	u32 pacing_en_int_th;
	u32 do_pacing_save;
	struct workqueue_struct		*dbq_wq;
	struct workqueue_struct		*dbr_drop_recov_wq;
	struct work_struct		dbq_fifo_check_work;
	struct delayed_work		dbq_pacing_work;
	/* protect DB pacing */
	struct mutex dbq_lock;
	/* Control DBR pacing feature. Set if enabled */
	bool dbr_pacing;
	/* Control DBR recovery feature. Set if enabled */
	bool dbr_drop_recov;
	bool user_dbr_drop_recov;
	/* DBR recovery feature. Set if running */
	bool dbr_recovery_on;
	u32 user_dbr_drop_recov_timeout;
	 /*
	  * Value used for pacing algo when pacing is active
	  */
#define BNXT_RE_MAX_DBR_DO_PACING 0xFFFF
	u32 dbr_do_pacing;
	u32 dbq_watermark; /* Current watermark set in HW registers */
	u32 dbq_nq_id; /* Current NQ ID for DBQ events */
	u32 dbq_pacing_time; /* ms */
	u32 dbr_def_do_pacing; /* do_pacing when no congestion */
	u32 dbr_evt_curr_epoch;
	bool dbq_int_disable;

	bool mod_exit;
	struct bnxt_re_dbr_sw_stats *dbr_sw_stats;
	struct bnxt_re_dbr_res_list res_list[BNXT_RE_RES_TYPE_MAX];
	struct bnxt_dbq_nq_list nq_list;
	char dev_name[IB_DEVICE_NAME_MAX];
	atomic_t dbq_intr_running;
	u32 num_msix_requested;
	unsigned char	*dev_addr; /* For netdev->dev_addr */
};

#define BNXT_RE_RESOLVE_RETRY_COUNT_US	5000000 /* 5 sec */
struct bnxt_re_resolve_dmac_work{
	struct work_struct      work;
	struct list_head	list;
	struct bnxt_re_dev 	*rdev;
	struct ib_ah_attr	*ah_attr;
	struct bnxt_re_ah_info *ah_info;
	atomic_t		status_wait;
};

static inline u8 bnxt_re_get_prio(u8 prio_map)
{
	u8 prio = 0xFF;

	for (prio = 0; prio < 8; prio++)
		if (prio_map & (1UL << prio))
			break;
	return prio;
}

/* This should be called with bnxt_re_dev_lock mutex held */
static inline bool __bnxt_re_is_rdev_valid(struct bnxt_re_dev *rdev)
{
	struct bnxt_re_dev *tmp_rdev;

	list_for_each_entry(tmp_rdev, &bnxt_re_dev_list, list) {
		if (rdev == tmp_rdev)
			return true;
	}
	return false;
}

static inline bool bnxt_re_is_rdev_valid(struct bnxt_re_dev *rdev)
{
	struct bnxt_re_dev *tmp_rdev;

	mutex_lock(&bnxt_re_dev_lock);
	list_for_each_entry(tmp_rdev, &bnxt_re_dev_list, list) {
		if (rdev == tmp_rdev) {
			mutex_unlock(&bnxt_re_dev_lock);
			return true;
		}
	}
	mutex_unlock(&bnxt_re_dev_lock);

	pr_debug("bnxt_re: %s : Invalid rdev received rdev = %p\n",
		 __func__, rdev);
	return false;
}

int bnxt_re_send_hwrm_cmd(struct bnxt_re_dev *rdev, void *cmd,
			  int cmdlen);
void bnxt_re_stopqps_and_ib_uninit(struct bnxt_re_dev *rdev);
int bnxt_re_set_hwrm_dscp2pri(struct bnxt_re_dev *rdev,
			      struct bnxt_re_dscp2pri *d2p, u16 count,
			      u16 target_id);
int bnxt_re_query_hwrm_dscp2pri(struct bnxt_re_dev *rdev,
				struct bnxt_re_dscp2pri *d2p, u16 *count,
				u16 target_id);
int bnxt_re_query_hwrm_qportcfg(struct bnxt_re_dev *rdev,
				struct bnxt_re_tc_rec *cnprec, u16 tid);
int bnxt_re_hwrm_cos2bw_qcfg(struct bnxt_re_dev *rdev, u16 target_id,
			     struct bnxt_re_cos2bw_cfg *cfg);
int bnxt_re_hwrm_cos2bw_cfg(struct bnxt_re_dev *rdev, u16 target_id,
			    struct bnxt_re_cos2bw_cfg *cfg);
int bnxt_re_hwrm_pri2cos_cfg(struct bnxt_re_dev *rdev,
			     u16 target_id, u16 port_id,
			     u8 *cos_id_map, u8 pri_map);
int bnxt_re_prio_vlan_tx_update(struct bnxt_re_dev *rdev);
int bnxt_re_get_slot_pf_count(struct bnxt_re_dev *rdev);
struct bnxt_re_dev *bnxt_re_get_peer_pf(struct bnxt_re_dev *rdev);
struct bnxt_re_dev *bnxt_re_from_netdev(struct ifnet *netdev);
u8 bnxt_re_get_priority_mask(struct bnxt_re_dev *rdev, u8 selector);
struct bnxt_qplib_nq * bnxt_re_get_nq(struct bnxt_re_dev *rdev);
void bnxt_re_put_nq(struct bnxt_re_dev *rdev, struct bnxt_qplib_nq *nq);

#define to_bnxt_re(ptr, type, member)	\
	container_of(ptr, type, member)

#define to_bnxt_re_dev(ptr, member)	\
	container_of((ptr), struct bnxt_re_dev, member)

/* Even number functions from port 0 and odd number from port 1 */
#define BNXT_RE_IS_PORT0(rdev) (!(rdev->en_dev->pdev->devfn & 1))

#define BNXT_RE_ROCE_V1_PACKET		0
#define BNXT_RE_ROCEV2_IPV4_PACKET	2
#define BNXT_RE_ROCEV2_IPV6_PACKET	3
#define BNXT_RE_ACTIVE_MAP_PORT1    0x1  /*port-1 active */
#define BNXT_RE_ACTIVE_MAP_PORT2    0x2  /*port-2 active */

#define BNXT_RE_MEMBER_PORT_MAP		(BNXT_RE_ACTIVE_MAP_PORT1 | \
					BNXT_RE_ACTIVE_MAP_PORT2)

#define	rdev_to_dev(rdev)	((rdev) ? (&(rdev)->ibdev.dev) : NULL)

void bnxt_re_set_dma_device(struct ib_device *ibdev, struct bnxt_re_dev *rdev);
bool bnxt_re_is_rdev_valid(struct bnxt_re_dev *rdev);

#define bnxt_re_rdev_ready(rdev)	(bnxt_re_is_rdev_valid(rdev) && \
					 (test_bit(BNXT_RE_FLAG_IBDEV_REGISTERED, &rdev->flags)))
#define BNXT_RE_SRIOV_CFG_TIMEOUT 6

int bnxt_re_get_device_stats(struct bnxt_re_dev *rdev);
void bnxt_re_remove_device(struct bnxt_re_dev *rdev, u8 removal_type,
			   struct auxiliary_device *aux_dev);
void bnxt_re_destroy_lag(struct bnxt_re_dev **rdev);
int bnxt_re_add_device(struct bnxt_re_dev **rdev,
		       struct ifnet *netdev,
		       u8 qp_mode, u8 op_type, u8 wqe_mode, u32 num_msix_requested,
		       struct auxiliary_device *aux_dev);
void bnxt_re_create_base_interface(bool primary);
int bnxt_re_schedule_work(struct bnxt_re_dev *rdev, unsigned long event,
			  struct ifnet *vlan_dev,
			  struct ifnet *netdev,
			  struct auxiliary_device *aux_dev);
void bnxt_re_get_link_speed(struct bnxt_re_dev *rdev);
int _bnxt_re_ib_init(struct bnxt_re_dev *rdev);
int _bnxt_re_ib_init2(struct bnxt_re_dev *rdev);
void bnxt_re_init_resolve_wq(struct bnxt_re_dev *rdev);
void bnxt_re_uninit_resolve_wq(struct bnxt_re_dev *rdev);

/* The rdev ref_count is to protect immature removal of the device */
static inline void bnxt_re_hold(struct bnxt_re_dev *rdev)
{
	atomic_inc(&rdev->ref_count);
	dev_dbg(rdev_to_dev(rdev),
		"Hold ref_count = 0x%x", atomic_read(&rdev->ref_count));
}

static inline void bnxt_re_put(struct bnxt_re_dev *rdev)
{
	atomic_dec(&rdev->ref_count);
	dev_dbg(rdev_to_dev(rdev),
		"Put ref_count = 0x%x", atomic_read(&rdev->ref_count));
}

/*
* Responder Error reason codes
* FIXME: Remove these when the defs
* are properly included in hsi header
*/
enum res_err_state_reason {
	/* No error. */
	CFCQ_RES_ERR_STATE_REASON_NO_ERROR = 0,
	/*
	 * Incoming Send, RDMA write, or RDMA read exceeds the maximum
	 * transfer length. Detected on RX first and only packets for
	 * write. Detected on RX request for read. This is an RX
	 * Detected Error.
	 */
	CFCQ_RES_ERR_STATE_REASON_RES_EXCEED_MAX,
	/*
	 * RDMA write payload size does not match write length. Detected
	 * when total write payload is not equal to the RDMA write
	 * length that was given in the first or only packet of the
	 * request. This is an RX Detected Error.
	 */
	CFCQ_RES_ERR_STATE_REASON_RES_PAYLOAD_LENGTH_MISMATCH,
	/*
	 * Send payload exceeds RQ/SRQ WQE buffer capacity. The total
	 * send payload that arrived is more than the size of the WQE
	 * buffer that was fetched from the RQ/SRQ. This is an RX
	 * Detected Error.
	 */
	CFCQ_RES_ERR_STATE_REASON_RES_EXCEEDS_WQE,
	/*
	 * Responder detected opcode error. * First, only, middle, last
	 * for incoming requests are improperly ordered with respect to
	 * previous (PSN) packet. * First or middle packet is not full
	 * MTU size. This is an RX Detected Error.
	 */
	CFCQ_RES_ERR_STATE_REASON_RES_OPCODE_ERROR,
	/*
	 * PSN sequence error retry limit exceeded. The responder
	 * encountered a PSN sequence error for the same PSN too many
	 * times. This can occur via implicit or explicit NAK. This is
	 * an RX Detected Error.
	 */
	CFCQ_RES_ERR_STATE_REASON_RES_PSN_SEQ_ERROR_RETRY_LIMIT,
	/*
	 * Invalid R_Key. An incoming request contained an R_Key that
	 * did not reference a valid MR/MW. This error may be detected
	 * by the RX engine for RDMA write or by the TX engine for RDMA
	 * read (detected while servicing IRRQ). This is an RX Detected
	 * Error.
	 */
	CFCQ_RES_ERR_STATE_REASON_RES_RX_INVALID_R_KEY,
	/*
	 * Domain error. An incoming request specified an R_Key which
	 * referenced a MR/MW that was not in the same PD as the QP on
	 * which the request arrived. This is an RX Detected Error.
	 */
	CFCQ_RES_ERR_STATE_REASON_RES_RX_DOMAIN_ERROR,
	/*
	 * No permission. An incoming request contained an R_Key that
	 * referenced a MR/MW which did not have the access permission
	 * needed for the operation. This is an RX Detected Error.
	 */
	CFCQ_RES_ERR_STATE_REASON_RES_RX_NO_PERMISSION,
	/*
	 * Range error. An incoming request had a combination of R_Key,
	 * VA, and length that was out of bounds of the associated
	 * MR/MW. This is an RX Detected Error.
	 */
	CFCQ_RES_ERR_STATE_REASON_RES_RX_RANGE_ERROR,
	/*
	 * Invalid R_Key. An incoming request contained an R_Key that
	 * did not reference a valid MR/MW. This error may be detected
	 * by the RX engine for RDMA write or by the TX engine for RDMA
	 * read (detected while servicing IRRQ). This is a TX Detected
	 * Error.
	 */
	CFCQ_RES_ERR_STATE_REASON_RES_TX_INVALID_R_KEY,
	/*
	 * Domain error. An incoming request specified an R_Key which
	 * referenced a MR/MW that was not in the same PD as the QP on
	 * which the request arrived. This is a TX Detected Error.
	 */
	CFCQ_RES_ERR_STATE_REASON_RES_TX_DOMAIN_ERROR,
	/*
	 * No permission. An incoming request contained an R_Key that
	 * referenced a MR/MW which did not have the access permission
	 * needed for the operation. This is a TX Detected Error.
	 */
	CFCQ_RES_ERR_STATE_REASON_RES_TX_NO_PERMISSION,
	/*
	 * Range error. An incoming request had a combination of R_Key,
	 * VA, and length that was out of bounds of the associated
	 * MR/MW. This is a TX Detected Error.
	 */
	CFCQ_RES_ERR_STATE_REASON_RES_TX_RANGE_ERROR,
	/*
	 * IRRQ overflow. The peer sent us more RDMA read or atomic
	 * requests than the negotiated maximum. This is an RX Detected
	 * Error.
	 */
	CFCQ_RES_ERR_STATE_REASON_RES_IRRQ_OFLOW,
	/*
	 * Unsupported opcode. The peer sent us a request with an opcode
	 * for a request type that is not supported on this QP. This is
	 * an RX Detected Error.
	 */
	CFCQ_RES_ERR_STATE_REASON_RES_UNSUPPORTED_OPCODE,
	/*
	 * Unaligned atomic operation. The VA of an atomic request is on
	 * a memory boundary that prevents atomic execution. This is an
	 * RX Detected Error.
	 */
	CFCQ_RES_ERR_STATE_REASON_RES_UNALIGN_ATOMIC,
	/*
	 * Remote invalidate error. A send with invalidate request
	 * arrived in which the R_Key to invalidate did not describe a
	 * MR/MW which could be invalidated. RQ WQE completes with error
	 * status. This error is only reported if the send operation did
	 * not fail. If the send operation failed then the remote
	 * invalidate error is not reported. This is an RX Detected
	 * Error.
	 */
	CFCQ_RES_ERR_STATE_REASON_RES_REM_INVALIDATE,
	/*
	 * Local memory error. An RQ/SRQ SGE described an inaccessible
	 * memory. This is an RX Detected Error.
	 */
	CFCQ_RES_ERR_STATE_REASON_RES_MEMORY_ERROR,
	/*
	 * SRQ in error. The QP is moving to error state because it
	 * found SRQ it uses in error. This is an RX Detected Error.
	 */
	CFCQ_RES_ERR_STATE_REASON_RES_SRQ_ERROR,
	/*
	 * Completion error. No CQE space available on queue or CQ not
	 * in VALID state. This is a Completion Detected Error.
	 */
	CFCQ_RES_ERR_STATE_REASON_RES_CMP_ERROR,
	/*
	 * Invalid R_Key while resending responses to duplicate request.
	 * This is a TX Detected Error.
	 */
	CFCQ_RES_ERR_STATE_REASON_RES_IVALID_DUP_RKEY,
	/*
	 * Problem was found in the format of a WQE in the RQ/SRQ. This
	 * is an RX Detected Error.
	 */
	CFCQ_RES_ERR_STATE_REASON_RES_WQE_FORMAT_ERROR,
	/*
	 * A load error occurred on an attempt to load the CQ Context.
	 * This is a Completion Detected Error.
	 */
	CFCQ_RES_ERR_STATE_REASON_RES_CQ_LOAD_ERROR = 0x18,
	/*
	 * A load error occurred on an attempt to load the SRQ Context.
	 * This is an RX Detected Error.
	 */
	CFCQ_RES_ERR_STATE_REASON_RES_SRQ_LOAD_ERROR,
	/*
	 * A fatal error was detected on an attempt to read from or
	 * write to PCIe on the transmit side. This error is detected by
	 * the TX side, but has the priority of a Completion Detected
	 * Error.
	 */
	CFCQ_RES_ERR_STATE_REASON_RES_TX_PCI_ERROR = 0x1b,
	/*
	 * A fatal error was detected on an attempt to read from or
	 * write to PCIe on the receive side. This error is detected by
	 * the RX side (or CAGR), but has the priority of a Completion
	 * Detected Error.
	 */
	CFCQ_RES_ERR_STATE_REASON_RES_RX_PCI_ERROR = 0x1c
};

int bnxt_re_host_pf_id_query(struct bnxt_re_dev *rdev,
			     struct bnxt_qplib_query_fn_info *fn_info,
			     u32 *pf_mask, u32 *first_pf);

/* Default DCBx and CC values */
#define BNXT_RE_DEFAULT_CNP_DSCP	48
#define BNXT_RE_DEFAULT_CNP_PRI		7
#define BNXT_RE_DEFAULT_ROCE_DSCP	26
#define BNXT_RE_DEFAULT_ROCE_PRI	3

#define BNXT_RE_DEFAULT_L2_BW		50
#define BNXT_RE_DEFAULT_ROCE_BW		50

#define ROCE_PRIO_VALID	0x0
#define CNP_PRIO_VALID	0x1
#define ROCE_DSCP_VALID	0x0
#define CNP_DSCP_VALID	0x1

int bnxt_re_get_pri_dscp_settings(struct bnxt_re_dev *rdev,
				  u16 target_id,
				  struct bnxt_re_tc_rec *tc_rec);

int bnxt_re_setup_dscp(struct bnxt_re_dev *rdev);
int bnxt_re_clear_dscp(struct bnxt_re_dev *rdev);
int bnxt_re_setup_cnp_cos(struct bnxt_re_dev *rdev, bool reset);

static inline enum ib_port_state bnxt_re_get_link_state(struct bnxt_re_dev *rdev)
{
	if (if_getdrvflags(rdev->netdev) & IFF_DRV_RUNNING &&
	    if_getlinkstate(rdev->netdev) == LINK_STATE_UP)
		return IB_PORT_ACTIVE;
	return IB_PORT_DOWN;
}

static inline int bnxt_re_link_state(struct bnxt_re_dev *rdev)
{
	return bnxt_re_get_link_state(rdev) == IB_PORT_ACTIVE ? 1:0;
}

static inline int is_cc_enabled(struct bnxt_re_dev *rdev)
{
	return rdev->cc_param.enable;
}

static inline void bnxt_re_init_hwrm_hdr(struct bnxt_re_dev *rdev,
					 struct input *hdr, u16 opcd,
					 u16 crid, u16 trid)
{
	hdr->req_type = cpu_to_le16(opcd);
	hdr->cmpl_ring = cpu_to_le16(crid);
	hdr->target_id = cpu_to_le16(trid);
}

static inline void bnxt_re_fill_fw_msg(struct bnxt_fw_msg *fw_msg,
				       void *msg, int msg_len, void *resp,
				       int resp_max_len, int timeout)
{
	fw_msg->msg = msg;
	fw_msg->msg_len = msg_len;
	fw_msg->resp = resp;
	fw_msg->resp_max_len = resp_max_len;
	fw_msg->timeout = timeout;
}

static inline bool is_qport_service_type_supported(struct bnxt_re_dev *rdev)
{
	return rdev->tc_rec[0].serv_type_enabled;
}

static inline bool is_bnxt_roce_queue(struct bnxt_re_dev *rdev, u8 ser_prof, u8 prof_type)
{
	if (is_qport_service_type_supported(rdev))
		return (prof_type & HWRM_QUEUE_QPORTCFG_OUTPUT_QUEUE_ID1_SERVICE_PROFILE_TYPE_ROCE);
	else
		return (ser_prof == HWRM_QUEUE_QPORTCFG_OUTPUT_QUEUE_ID0_SERVICE_PROFILE_LOSSLESS_ROCE);
}

static inline bool is_bnxt_cnp_queue(struct bnxt_re_dev *rdev, u8 ser_prof, u8 prof_type)
{
	if (is_qport_service_type_supported(rdev))
		return (prof_type & HWRM_QUEUE_QPORTCFG_OUTPUT_QUEUE_ID1_SERVICE_PROFILE_TYPE_CNP);
	else
		return (ser_prof == HWRM_QUEUE_QPORTCFG_OUTPUT_QUEUE_ID0_SERVICE_PROFILE_LOSSY_ROCE_CNP);
}

#define BNXT_RE_MAP_SH_PAGE		0x0
#define BNXT_RE_MAP_WC			0x1
#define BNXT_RE_DBR_PAGE		0x2
#define BNXT_RE_MAP_DB_RECOVERY_PAGE	0x3

#define BNXT_RE_DBR_RECOV_USERLAND_TIMEOUT (20)  /*  20 ms */
#define BNXT_RE_DBR_INT_TIME 5 /* ms */
#define BNXT_RE_PACING_EN_INT_THRESHOLD 50 /* Entries in DB FIFO */
#define BNXT_RE_PACING_ALGO_THRESHOLD 250 /* Entries in DB FIFO */
/* Percentage of DB FIFO depth */
#define BNXT_RE_PACING_DBQ_THRESHOLD BNXT_RE_PACING_DBQ_HIGH_WATERMARK

#define BNXT_RE_PACING_ALARM_TH_MULTIPLE(ctx) (_is_chip_p7(ctx) ? 0 : 2)

/*
 * Maximum Percentage of configurable DB FIFO depth.
 * The Doorbell FIFO depth is 0x2c00. But the DBR_REG_DB_THROTTLING register has only 12 bits
 * to program the high watermark. This means user can configure maximum 36% only(4095/11264).
 */
#define BNXT_RE_PACING_DBQ_HIGH_WATERMARK 36

/* Default do_pacing value when there is no congestion */
#define BNXT_RE_DBR_DO_PACING_NO_CONGESTION 0x7F /* 1 in 512 probability */

enum {
	BNXT_RE_DBQ_EVENT_SCHED = 0,
	BNXT_RE_DBR_PACING_EVENT = 1,
	BNXT_RE_DBR_NQ_PACING_NOTIFICATION = 2,
};

struct bnxt_re_dbq_work {
	struct work_struct work;
	struct bnxt_re_dev *rdev;
	struct hwrm_async_event_cmpl cmpl;
	u32 event;
};

int bnxt_re_hwrm_qcaps(struct bnxt_re_dev *rdev);
int bnxt_re_enable_dbr_pacing(struct bnxt_re_dev *rdev);
int bnxt_re_disable_dbr_pacing(struct bnxt_re_dev *rdev);
int bnxt_re_set_dbq_throttling_reg(struct bnxt_re_dev *rdev,
				   u16 nq_id, u32 throttle);
void bnxt_re_pacing_alert(struct bnxt_re_dev *rdev);
int bnxt_re_hwrm_pri2cos_qcfg(struct bnxt_re_dev *rdev, struct bnxt_re_tc_rec *tc_rec,
			      u16 target_id);
void writel_fbsd(struct bnxt_softc *bp, u32, u8, u32);
u32 readl_fbsd(struct bnxt_softc *bp, u32, u8);

static inline unsigned int bnxt_re_get_total_mr_mw_count(struct bnxt_re_dev *rdev)
{
	return (atomic_read(&rdev->stats.rsors.mr_count) +
		atomic_read(&rdev->stats.rsors.mw_count));
}

static inline void bnxt_re_set_def_pacing_threshold(struct bnxt_re_dev *rdev)
{
	rdev->qplib_res.pacing_data->pacing_th = rdev->pacing_algo_th;
	rdev->qplib_res.pacing_data->alarm_th =
		rdev->pacing_algo_th * BNXT_RE_PACING_ALARM_TH_MULTIPLE(rdev->chip_ctx);
}

static inline void bnxt_re_set_def_do_pacing(struct bnxt_re_dev *rdev)
{
	rdev->qplib_res.pacing_data->do_pacing = rdev->dbr_def_do_pacing;
}

static inline void bnxt_re_set_pacing_dev_state(struct bnxt_re_dev *rdev)
{
	rdev->qplib_res.pacing_data->dev_err_state =
		test_bit(BNXT_RE_FLAG_ERR_DEVICE_DETACHED, &rdev->flags);
}
#endif
