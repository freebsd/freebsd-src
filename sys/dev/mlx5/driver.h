/*-
 * Copyright (c) 2013-2019, Mellanox Technologies, Ltd.  All rights reserved.
 * Copyright (c) 2022 NVIDIA corporation & affiliates.
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
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS `AS IS' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef MLX5_DRIVER_H
#define MLX5_DRIVER_H

#include "opt_ratelimit.h"

#include <linux/kernel.h>
#include <linux/completion.h>
#include <linux/pci.h>
#include <linux/cache.h>
#include <linux/rbtree.h>
#include <linux/if_ether.h>
#include <linux/semaphore.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/radix-tree.h>
#include <linux/idr.h>
#include <linux/wait.h>

#include <dev/mlx5/device.h>
#include <dev/mlx5/doorbell.h>
#include <dev/mlx5/srq.h>

#define MLX5_QCOUNTER_SETS_NETDEV 64
#define MLX5_MAX_NUMBER_OF_VFS 128

#define MLX5_INVALID_QUEUE_HANDLE 0xffffffff
#define MLX5_ST_SZ_BYTES(typ) (sizeof(struct mlx5_ifc_##typ##_bits) / 8)

enum {
	MLX5_BOARD_ID_LEN = 64,
	MLX5_MAX_NAME_LEN = 16,
};

enum {
	MLX5_CMD_TIMEOUT_MSEC	= 60 * 1000,
};

enum {
	CMD_OWNER_SW		= 0x0,
	CMD_OWNER_HW		= 0x1,
	CMD_STATUS_SUCCESS	= 0,
};

enum mlx5_sqp_t {
	MLX5_SQP_SMI		= 0,
	MLX5_SQP_GSI		= 1,
	MLX5_SQP_IEEE_1588	= 2,
	MLX5_SQP_SNIFFER	= 3,
	MLX5_SQP_SYNC_UMR	= 4,
};

enum {
	MLX5_MAX_PORTS	= 2,
};

enum {
	MLX5_EQ_VEC_PAGES	 = 0,
	MLX5_EQ_VEC_CMD		 = 1,
	MLX5_EQ_VEC_ASYNC	 = 2,
	MLX5_EQ_VEC_COMP_BASE,
};

enum {
	MLX5_ATOMIC_MODE_OFF		= 16,
	MLX5_ATOMIC_MODE_NONE		= 0 << MLX5_ATOMIC_MODE_OFF,
	MLX5_ATOMIC_MODE_IB_COMP	= 1 << MLX5_ATOMIC_MODE_OFF,
	MLX5_ATOMIC_MODE_CX		= 2 << MLX5_ATOMIC_MODE_OFF,
	MLX5_ATOMIC_MODE_8B		= 3 << MLX5_ATOMIC_MODE_OFF,
	MLX5_ATOMIC_MODE_16B		= 4 << MLX5_ATOMIC_MODE_OFF,
	MLX5_ATOMIC_MODE_32B		= 5 << MLX5_ATOMIC_MODE_OFF,
	MLX5_ATOMIC_MODE_64B		= 6 << MLX5_ATOMIC_MODE_OFF,
	MLX5_ATOMIC_MODE_128B		= 7 << MLX5_ATOMIC_MODE_OFF,
	MLX5_ATOMIC_MODE_256B		= 8 << MLX5_ATOMIC_MODE_OFF,
};

enum {
	MLX5_ATOMIC_MODE_DCT_OFF	= 20,
	MLX5_ATOMIC_MODE_DCT_NONE	= 0 << MLX5_ATOMIC_MODE_DCT_OFF,
	MLX5_ATOMIC_MODE_DCT_IB_COMP	= 1 << MLX5_ATOMIC_MODE_DCT_OFF,
	MLX5_ATOMIC_MODE_DCT_CX		= 2 << MLX5_ATOMIC_MODE_DCT_OFF,
	MLX5_ATOMIC_MODE_DCT_8B		= 3 << MLX5_ATOMIC_MODE_DCT_OFF,
	MLX5_ATOMIC_MODE_DCT_16B	= 4 << MLX5_ATOMIC_MODE_DCT_OFF,
	MLX5_ATOMIC_MODE_DCT_32B	= 5 << MLX5_ATOMIC_MODE_DCT_OFF,
	MLX5_ATOMIC_MODE_DCT_64B	= 6 << MLX5_ATOMIC_MODE_DCT_OFF,
	MLX5_ATOMIC_MODE_DCT_128B	= 7 << MLX5_ATOMIC_MODE_DCT_OFF,
	MLX5_ATOMIC_MODE_DCT_256B	= 8 << MLX5_ATOMIC_MODE_DCT_OFF,
};

enum {
	MLX5_ATOMIC_OPS_CMP_SWAP		= 1 << 0,
	MLX5_ATOMIC_OPS_FETCH_ADD		= 1 << 1,
	MLX5_ATOMIC_OPS_MASKED_CMP_SWAP		= 1 << 2,
	MLX5_ATOMIC_OPS_MASKED_FETCH_ADD	= 1 << 3,
};

enum {
	MLX5_REG_QPTS		 = 0x4002,
	MLX5_REG_QETCR		 = 0x4005,
	MLX5_REG_QPDP		 = 0x4007,
	MLX5_REG_QTCT		 = 0x400A,
	MLX5_REG_QPDPM		 = 0x4013,
	MLX5_REG_QHLL		 = 0x4016,
	MLX5_REG_QCAM		 = 0x4019,
	MLX5_REG_DCBX_PARAM	 = 0x4020,
	MLX5_REG_DCBX_APP	 = 0x4021,
	MLX5_REG_FPGA_CAP	 = 0x4022,
	MLX5_REG_FPGA_CTRL	 = 0x4023,
	MLX5_REG_FPGA_ACCESS_REG = 0x4024,
	MLX5_REG_FPGA_SHELL_CNTR = 0x4025,
	MLX5_REG_PCAP		 = 0x5001,
	MLX5_REG_PMLP		 = 0x5002,
	MLX5_REG_PMTU		 = 0x5003,
	MLX5_REG_PTYS		 = 0x5004,
	MLX5_REG_PAOS		 = 0x5006,
	MLX5_REG_PFCC		 = 0x5007,
	MLX5_REG_PPCNT		 = 0x5008,
	MLX5_REG_PUDE		 = 0x5009,
	MLX5_REG_PPTB		 = 0x500B,
	MLX5_REG_PBMC		 = 0x500C,
	MLX5_REG_PELC		 = 0x500E,
	MLX5_REG_PVLC		 = 0x500F,
	MLX5_REG_PMPE		 = 0x5010,
	MLX5_REG_PMAOS		 = 0x5012,
	MLX5_REG_PPLM		 = 0x5023,
	MLX5_REG_PDDR		 = 0x5031,
	MLX5_REG_PBSR		 = 0x5038,
	MLX5_REG_PCAM		 = 0x507f,
	MLX5_REG_NODE_DESC	 = 0x6001,
	MLX5_REG_HOST_ENDIANNESS = 0x7004,
	MLX5_REG_MTMP		 = 0x900a,
	MLX5_REG_MCIA		 = 0x9014,
	MLX5_REG_MFRL		 = 0x9028,
	MLX5_REG_MPCNT		 = 0x9051,
	MLX5_REG_MCQI		 = 0x9061,
	MLX5_REG_MCC		 = 0x9062,
	MLX5_REG_MCDA		 = 0x9063,
	MLX5_REG_MCAM		 = 0x907f,
};

enum dbg_rsc_type {
	MLX5_DBG_RSC_QP,
	MLX5_DBG_RSC_EQ,
	MLX5_DBG_RSC_CQ,
};

enum {
	MLX5_INTERFACE_PROTOCOL_IB  = 0,
	MLX5_INTERFACE_PROTOCOL_ETH = 1,
	MLX5_INTERFACE_NUMBER       = 2,
};

struct mlx5_field_desc {
	int			i;
};

struct mlx5_rsc_debug {
	struct mlx5_core_dev   *dev;
	void		       *object;
	enum dbg_rsc_type	type;
	struct mlx5_field_desc	fields[0];
};

enum mlx5_dev_event {
	MLX5_DEV_EVENT_SYS_ERROR,
	MLX5_DEV_EVENT_PORT_UP,
	MLX5_DEV_EVENT_PORT_DOWN,
	MLX5_DEV_EVENT_PORT_INITIALIZED,
	MLX5_DEV_EVENT_LID_CHANGE,
	MLX5_DEV_EVENT_PKEY_CHANGE,
	MLX5_DEV_EVENT_GUID_CHANGE,
	MLX5_DEV_EVENT_CLIENT_REREG,
	MLX5_DEV_EVENT_VPORT_CHANGE,
	MLX5_DEV_EVENT_ERROR_STATE_DCBX,
	MLX5_DEV_EVENT_REMOTE_CONFIG_CHANGE,
	MLX5_DEV_EVENT_LOCAL_OPER_CHANGE,
	MLX5_DEV_EVENT_REMOTE_CONFIG_APPLICATION_PRIORITY_CHANGE,
};

enum mlx5_port_status {
	MLX5_PORT_UP        = 1 << 0,
	MLX5_PORT_DOWN      = 1 << 1,
};

enum {
	MLX5_VSC_SPACE_SUPPORTED = 0x1,
	MLX5_VSC_SPACE_OFFSET	 = 0x4,
	MLX5_VSC_COUNTER_OFFSET	 = 0x8,
	MLX5_VSC_SEMA_OFFSET	 = 0xC,
	MLX5_VSC_ADDR_OFFSET	 = 0x10,
	MLX5_VSC_DATA_OFFSET	 = 0x14,
	MLX5_VSC_MAX_RETRIES	 = 0x1000,
};

#define MLX5_PROT_MASK(link_mode) (1 << link_mode)

struct mlx5_cmd_first {
	__be32		data[4];
};

struct cache_ent;
struct mlx5_fw_page {
	union {
		struct rb_node rb_node;
		struct list_head list;
	};
	struct mlx5_cmd_first first;
	struct mlx5_core_dev *dev;
	bus_dmamap_t dma_map;
	bus_addr_t dma_addr;
	void *virt_addr;
	struct cache_ent *cache;
	u32 numpages;
	u16 load_done;
#define	MLX5_LOAD_ST_NONE 0
#define	MLX5_LOAD_ST_SUCCESS 1
#define	MLX5_LOAD_ST_FAILURE 2
	u16 func_id;
};
#define	mlx5_cmd_msg mlx5_fw_page

struct mlx5_cmd_debug {
	void		       *in_msg;
	void		       *out_msg;
	u8			status;
	u16			inlen;
	u16			outlen;
};

struct cache_ent {
	/* protect block chain allocations
	 */
	spinlock_t		lock;
	struct list_head	head;
};

struct cmd_msg_cache {
	struct cache_ent	large;
	struct cache_ent	med;

};

struct mlx5_traffic_counter {
	u64         packets;
	u64         octets;
};

struct mlx5_fc_pool {
	struct mlx5_core_dev *dev;
	struct mutex pool_lock; /* protects pool lists */
	struct list_head fully_used;
	struct list_head partially_used;
	struct list_head unused;
	int available_fcs;
	int used_fcs;
	int threshold;
};

struct mlx5_fc_stats {
	spinlock_t counters_idr_lock; /* protects counters_idr */
	struct idr counters_idr;
	struct list_head counters;
	struct llist_head addlist;
	struct llist_head dellist;

	struct workqueue_struct *wq;
	struct delayed_work work;
	unsigned long next_query;
	unsigned long sampling_interval; /* jiffies */
	u32 *bulk_query_out;
	int bulk_query_len;
	size_t num_counters;
	bool bulk_query_alloc_failed;
	unsigned long next_bulk_query_alloc;
	struct mlx5_fc_pool fc_pool;
};

enum mlx5_cmd_mode {
	MLX5_CMD_MODE_POLLING,
	MLX5_CMD_MODE_EVENTS
};

struct mlx5_cmd_stats {
	u64		sum;
	u64		n;
	/* protect command average calculations */
	spinlock_t	lock;
};

struct mlx5_cmd {
	struct mlx5_fw_page *cmd_page;
	bus_dma_tag_t dma_tag;
	struct sx dma_sx;
	struct mtx dma_mtx;
#define	MLX5_DMA_OWNED(dev) mtx_owned(&(dev)->cmd.dma_mtx)
#define	MLX5_DMA_LOCK(dev) mtx_lock(&(dev)->cmd.dma_mtx)
#define	MLX5_DMA_UNLOCK(dev) mtx_unlock(&(dev)->cmd.dma_mtx)
	struct cv dma_cv;
#define	MLX5_DMA_DONE(dev) cv_broadcast(&(dev)->cmd.dma_cv)
#define	MLX5_DMA_WAIT(dev) cv_wait(&(dev)->cmd.dma_cv, &(dev)->cmd.dma_mtx)
	void	       *cmd_buf;
	dma_addr_t	dma;
	u16		cmdif_rev;
	u8		log_sz;
	u8		log_stride;
	int		max_reg_cmds;
	int		events;
	u32 __iomem    *vector;

	/* protect command queue allocations
	 */
	spinlock_t	alloc_lock;

	/* protect token allocations
	 */
	spinlock_t	token_lock;
	u8		token;
	unsigned long	bitmask;
	struct semaphore sem;
	struct semaphore pages_sem;
	enum mlx5_cmd_mode mode;
	struct mlx5_cmd_work_ent * volatile ent_arr[MLX5_MAX_COMMANDS];
	volatile enum mlx5_cmd_mode ent_mode[MLX5_MAX_COMMANDS];
	struct mlx5_cmd_debug dbg;
	struct cmd_msg_cache cache;
	int checksum_disabled;
	struct mlx5_cmd_stats stats[MLX5_CMD_OP_MAX];
};

struct mlx5_port_caps {
	int	gid_table_len;
	int	pkey_table_len;
	u8	ext_port_cap;
};

struct mlx5_buf {
	bus_dma_tag_t		dma_tag;
	bus_dmamap_t		dma_map;
	struct mlx5_core_dev   *dev;
	struct {
		void	       *buf;
	} direct;
	u64		       *page_list;
	int			npages;
	int			size;
	u8			page_shift;
	u8			load_done;
};

struct mlx5_frag_buf {
	struct mlx5_buf_list	*frags;
	int			npages;
	int			size;
	u8			page_shift;
};

struct mlx5_eq {
	struct mlx5_core_dev   *dev;
	__be32 __iomem	       *doorbell;
	u32			cons_index;
	struct mlx5_buf		buf;
	int			size;
	u8			irqn;
	u8			eqn;
	int			nent;
	u64			mask;
	struct list_head	list;
	int			index;
	struct mlx5_rsc_debug	*dbg;
};

struct mlx5_core_psv {
	u32	psv_idx;
	struct psv_layout {
		u32	pd;
		u16	syndrome;
		u16	reserved;
		u16	bg;
		u16	app_tag;
		u32	ref_tag;
	} psv;
};

struct mlx5_core_sig_ctx {
	struct mlx5_core_psv	psv_memory;
	struct mlx5_core_psv	psv_wire;
	struct ib_sig_err       err_item;
	bool			sig_status_checked;
	bool			sig_err_exists;
	u32			sigerr_count;
};

enum {
	MLX5_MKEY_MR = 1,
	MLX5_MKEY_MW,
	MLX5_MKEY_INDIRECT_DEVX,
};

struct mlx5_core_mkey {
	u64			iova;
	u64			size;
	u32			key;
	u32			pd;
	u32			type;
};

enum mlx5_res_type {
	MLX5_RES_QP	= MLX5_EVENT_QUEUE_TYPE_QP,
	MLX5_RES_RQ	= MLX5_EVENT_QUEUE_TYPE_RQ,
	MLX5_RES_SQ	= MLX5_EVENT_QUEUE_TYPE_SQ,
	MLX5_RES_SRQ	= 3,
	MLX5_RES_XSRQ	= 4,
	MLX5_RES_XRQ	= 5,
	MLX5_RES_DCT	= MLX5_EVENT_QUEUE_TYPE_DCT,
};

struct mlx5_core_rsc_common {
	enum mlx5_res_type	res;
	atomic_t		refcount;
	struct completion	free;
};

struct mlx5_uars_page {
	void __iomem	       *map;
	bool			wc;
	u32			index;
	struct list_head	list;
	unsigned int		bfregs;
	unsigned long	       *reg_bitmap; /* for non fast path bf regs */
	unsigned long	       *fp_bitmap;
	unsigned int		reg_avail;
	unsigned int		fp_avail;
	struct kref		ref_count;
	struct mlx5_core_dev   *mdev;
};

struct mlx5_bfreg_head {
	/* protect blue flame registers allocations */
	struct mutex		lock;
	struct list_head	list;
};

struct mlx5_bfreg_data {
	struct mlx5_bfreg_head	reg_head;
	struct mlx5_bfreg_head	wc_head;
};

struct mlx5_sq_bfreg {
	void __iomem	       *map;
	struct mlx5_uars_page  *up;
	bool			wc;
	u32			index;
	unsigned int		offset;
};

struct mlx5_core_srq {
	struct mlx5_core_rsc_common	common; /* must be first */
	u32				srqn;
	int				max;
	size_t				max_gs;
	size_t				max_avail_gather;
	int				wqe_shift;
	void				(*event)(struct mlx5_core_srq *, int);
	atomic_t			refcount;
	struct completion		free;
};

struct mlx5_ib_dev;
struct mlx5_eq_table {
	void __iomem	       *update_ci;
	void __iomem	       *update_arm_ci;
	struct list_head	comp_eqs_list;
	struct mlx5_eq		pages_eq;
	struct mlx5_eq		async_eq;
	struct mlx5_eq		cmd_eq;
	int			num_comp_vectors;
	spinlock_t		lock;	/* protect EQs list */
	struct mlx5_ib_dev	*dev;	/* for devx event notifier */
	bool (*cb)(struct mlx5_core_dev *mdev,
		   uint8_t event_type, void *data);
};

struct mlx5_core_health {
	struct mlx5_health_buffer __iomem	*health;
	__be32 __iomem		       *health_counter;
	struct timer_list		timer;
	u32				prev;
	int				miss_counter;
	u32				fatal_error;
	struct workqueue_struct	       *wq_watchdog;
	struct work_struct		work_watchdog;
	/* wq spinlock to synchronize draining */
	spinlock_t			wq_lock;
	struct workqueue_struct	       *wq;
	unsigned long			flags;
	struct work_struct		work;
	struct delayed_work		recover_work;
	unsigned int			last_reset_req;
	struct work_struct		work_cmd_completion;
	struct workqueue_struct	       *wq_cmd;
};

#define	MLX5_CQ_LINEAR_ARRAY_SIZE	1024

struct mlx5_cq_linear_array_entry {
	struct mlx5_core_cq * volatile cq;
};

struct mlx5_cq_table {
	/* protect radix tree
	 */
	spinlock_t		writerlock;
	atomic_t		writercount;
	struct radix_tree_root	tree;
	struct mlx5_cq_linear_array_entry linear_array[MLX5_CQ_LINEAR_ARRAY_SIZE];
};

struct mlx5_qp_table {
	/* protect radix tree
	 */
	spinlock_t		lock;
	struct radix_tree_root	tree;
};

struct mlx5_srq_table {
	/* protect radix tree
	 */
	spinlock_t		lock;
	struct radix_tree_root	tree;
};

struct mlx5_mr_table {
	/* protect radix tree
	 */
	spinlock_t		lock;
	struct radix_tree_root	tree;
};

#ifdef RATELIMIT
struct mlx5_rl_entry {
	u32			rate;
	u16			burst;
	u16			index;
	u32			qos_handle; /* schedule queue handle */
	u32			refcount;
};

struct mlx5_rl_table {
	struct mutex		rl_lock;
	u16			max_size;
	u32			max_rate;
	u32			min_rate;
	struct mlx5_rl_entry   *rl_entry;
};
#endif

struct mlx5_pme_stats {
	u64			status_counters[MLX5_MODULE_STATUS_NUM];
	u64			error_counters[MLX5_MODULE_EVENT_ERROR_NUM];
};

struct mlx5_priv {
	char			name[MLX5_MAX_NAME_LEN];
	struct mlx5_eq_table	eq_table;
	struct msix_entry	*msix_arr;
	MLX5_DECLARE_DOORBELL_LOCK(cq_uar_lock);
	int			disable_irqs;

	/* pages stuff */
	struct workqueue_struct *pg_wq;
	struct rb_root		page_root;
	s64			fw_pages;
	atomic_t		reg_pages;
	s64			pages_per_func[MLX5_MAX_NUMBER_OF_VFS];
	struct mlx5_core_health health;

	struct mlx5_srq_table	srq_table;

	/* start: qp staff */
	struct mlx5_qp_table	qp_table;

	/* end: qp staff */

	/* start: cq staff */
	struct mlx5_cq_table	cq_table;
	/* end: cq staff */

	/* start: mr staff */
	struct mlx5_mr_table	mr_table;
	/* end: mr staff */

	/* start: alloc staff */
	int			numa_node;

	struct mutex   pgdir_mutex;
	struct list_head        pgdir_list;
	/* end: alloc staff */

	/* protect mkey key part */
	spinlock_t		mkey_lock;
	u8			mkey_key;

	struct list_head        dev_list;
	struct list_head        ctx_list;
	spinlock_t              ctx_lock;
	unsigned long		pci_dev_data;
#ifdef RATELIMIT
	struct mlx5_rl_table	rl_table;
#endif
	struct mlx5_pme_stats pme_stats;

	struct mlx5_flow_steering *steering;
	struct mlx5_eswitch	*eswitch;

	struct mlx5_bfreg_data		bfregs;
	struct mlx5_uars_page	       *uar;
	struct mlx5_fc_stats		fc_stats;
	struct mlx5_ft_pool             *ft_pool;
};

enum mlx5_device_state {
	MLX5_DEVICE_STATE_UP,
	MLX5_DEVICE_STATE_INTERNAL_ERROR,
};

enum mlx5_interface_state {
	MLX5_INTERFACE_STATE_UP = 0x1,
	MLX5_INTERFACE_STATE_TEARDOWN = 0x2,
};

enum mlx5_pci_status {
	MLX5_PCI_STATUS_DISABLED,
	MLX5_PCI_STATUS_ENABLED,
};

#define	MLX5_MAX_RESERVED_GIDS	8

struct mlx5_rsvd_gids {
	unsigned int start;
	unsigned int count;
	struct ida ida;
};

struct mlx5_special_contexts {
	int resd_lkey;
};

struct mlx5_diag_cnt_id {
	u16	id;
	bool	enabled;
};

struct mlx5_diag_cnt {
#define	DIAG_LOCK(dc) mutex_lock(&(dc)->lock)
#define	DIAG_UNLOCK(dc) mutex_unlock(&(dc)->lock)
	struct mutex lock;
	struct sysctl_ctx_list sysctl_ctx;
	struct mlx5_diag_cnt_id *cnt_id;
	u16	num_of_samples;
	u16	sample_index;
	u8	num_cnt_id;
	u8	log_num_of_samples;
	u8	log_sample_period;
	u8	flag;
	u8	ready;
};

struct mlx5_flow_root_namespace;
struct mlx5_core_dev {
	struct pci_dev	       *pdev;
	/* sync pci state */
	struct mutex		pci_status_mutex;
	enum mlx5_pci_status	pci_status;
	char			board_id[MLX5_BOARD_ID_LEN];
	struct mlx5_cmd		cmd;
	struct mlx5_port_caps	port_caps[MLX5_MAX_PORTS];
	u32 hca_caps_cur[MLX5_CAP_NUM][MLX5_UN_SZ_DW(hca_cap_union)];
	u32 hca_caps_max[MLX5_CAP_NUM][MLX5_UN_SZ_DW(hca_cap_union)];
	struct {
		u32 pcam[MLX5_ST_SZ_DW(pcam_reg)];
		u32 mcam[MLX5_ST_SZ_DW(mcam_reg)];
		u32 qcam[MLX5_ST_SZ_DW(qcam_reg)];
		u32 fpga[MLX5_ST_SZ_DW(fpga_cap)];
	} caps;
	phys_addr_t		iseg_base;
	struct mlx5_init_seg __iomem *iseg;
	enum mlx5_device_state	state;
	/* sync interface state */
	struct mutex		intf_state_mutex;
	unsigned long		intf_state;
	void			(*event) (struct mlx5_core_dev *dev,
					  enum mlx5_dev_event event,
					  unsigned long param);
	struct mlx5_priv	priv;
	struct mlx5_profile	*profile;
	atomic_t		num_qps;
	struct mlx5_diag_cnt	diag_cnt;
	u32			vsc_addr;
	u32			issi;
	struct mlx5_special_contexts special_contexts;
	unsigned int module_status;
	unsigned int module_num;
	struct mlx5_flow_root_namespace *root_ns;
	struct mlx5_flow_root_namespace *fdb_root_ns;
	struct mlx5_flow_root_namespace *esw_egress_root_ns;
	struct mlx5_flow_root_namespace *esw_ingress_root_ns;
	struct mlx5_flow_root_namespace *sniffer_rx_root_ns;
	struct mlx5_flow_root_namespace *sniffer_tx_root_ns;
	struct mlx5_flow_root_namespace *nic_tx_root_ns;
	struct mlx5_flow_root_namespace *rdma_tx_root_ns;
	struct mlx5_flow_root_namespace *rdma_rx_root_ns;

	u32 num_q_counter_allocated[MLX5_INTERFACE_NUMBER];
	struct mlx5_crspace_regmap *dump_rege;
	uint32_t *dump_data;
	unsigned dump_size;
	bool dump_valid;
	bool dump_copyout;
	struct mtx dump_lock;

	bool			iov_pf;

	struct sysctl_ctx_list	sysctl_ctx;
	int			msix_eqvec;
	int			pwr_status;
	int			pwr_value;

	struct {
		struct mlx5_rsvd_gids	reserved_gids;
		atomic_t		roce_en;
	} roce;

	struct {
		spinlock_t	spinlock;
#define	MLX5_MPFS_TABLE_MAX 32
		long		bitmap[BITS_TO_LONGS(MLX5_MPFS_TABLE_MAX)];
	} mpfs;
#ifdef CONFIG_MLX5_FPGA
	struct mlx5_fpga_device	*fpga;
#endif
	struct xarray ipsec_sadb;
};

enum {
	MLX5_WOL_DISABLE       = 0,
	MLX5_WOL_SECURED_MAGIC = 1 << 1,
	MLX5_WOL_MAGIC         = 1 << 2,
	MLX5_WOL_ARP           = 1 << 3,
	MLX5_WOL_BROADCAST     = 1 << 4,
	MLX5_WOL_MULTICAST     = 1 << 5,
	MLX5_WOL_UNICAST       = 1 << 6,
	MLX5_WOL_PHY_ACTIVITY  = 1 << 7,
};

struct mlx5_db {
	__be32			*db;
	union {
		struct mlx5_db_pgdir		*pgdir;
		struct mlx5_ib_user_db_page	*user_page;
	}			u;
	dma_addr_t		dma;
	int			index;
};

struct mlx5_net_counters {
	u64	packets;
	u64	octets;
};

struct mlx5_ptys_reg {
	u8	an_dis_admin;
	u8	an_dis_ap;
	u8	local_port;
	u8	proto_mask;
	u32	eth_proto_cap;
	u16	ib_link_width_cap;
	u16	ib_proto_cap;
	u32	eth_proto_admin;
	u16	ib_link_width_admin;
	u16	ib_proto_admin;
	u32	eth_proto_oper;
	u16	ib_link_width_oper;
	u16	ib_proto_oper;
	u32	eth_proto_lp_advertise;
};

struct mlx5_pvlc_reg {
	u8	local_port;
	u8	vl_hw_cap;
	u8	vl_admin;
	u8	vl_operational;
};

struct mlx5_pmtu_reg {
	u8	local_port;
	u16	max_mtu;
	u16	admin_mtu;
	u16	oper_mtu;
};

struct mlx5_vport_counters {
	struct mlx5_net_counters	received_errors;
	struct mlx5_net_counters	transmit_errors;
	struct mlx5_net_counters	received_ib_unicast;
	struct mlx5_net_counters	transmitted_ib_unicast;
	struct mlx5_net_counters	received_ib_multicast;
	struct mlx5_net_counters	transmitted_ib_multicast;
	struct mlx5_net_counters	received_eth_broadcast;
	struct mlx5_net_counters	transmitted_eth_broadcast;
	struct mlx5_net_counters	received_eth_unicast;
	struct mlx5_net_counters	transmitted_eth_unicast;
	struct mlx5_net_counters	received_eth_multicast;
	struct mlx5_net_counters	transmitted_eth_multicast;
};

enum {
	MLX5_DB_PER_PAGE = MLX5_ADAPTER_PAGE_SIZE / L1_CACHE_BYTES,
};

struct mlx5_core_dct {
	struct mlx5_core_rsc_common	common; /* must be first */
	void (*event)(struct mlx5_core_dct *, int);
	int			dctn;
	struct completion	drained;
	struct mlx5_rsc_debug	*dbg;
	int			pid;
	u16			uid;
};

enum {
	MLX5_PTYS_IB = 1 << 0,
	MLX5_PTYS_EN = 1 << 2,
};

struct mlx5_db_pgdir {
	struct list_head	list;
	DECLARE_BITMAP(bitmap, MLX5_DB_PER_PAGE);
	struct mlx5_fw_page    *fw_page;
	__be32		       *db_page;
	dma_addr_t		db_dma;
};

typedef void (*mlx5_cmd_cbk_t)(int status, void *context);

struct mlx5_cmd_work_ent {
	struct mlx5_cmd_msg    *in;
	struct mlx5_cmd_msg    *out;
	int			uin_size;
	void		       *uout;
	int			uout_size;
	mlx5_cmd_cbk_t		callback;
        struct delayed_work     cb_timeout_work;
	void		       *context;
	int			idx;
	struct completion	done;
	struct mlx5_cmd        *cmd;
	struct work_struct	work;
	struct mlx5_cmd_layout *lay;
	int			ret;
	int			page_queue;
	u8			status;
	u8			token;
	u64			ts1;
	u64			ts2;
	u16			op;
	u8			busy;
	bool			polling;
};

struct mlx5_pas {
	u64	pa;
	u8	log_sz;
};

enum port_state_policy {
	MLX5_POLICY_DOWN        = 0,
	MLX5_POLICY_UP          = 1,
	MLX5_POLICY_FOLLOW      = 2,
	MLX5_POLICY_INVALID     = 0xffffffff
};

static inline void *
mlx5_buf_offset(struct mlx5_buf *buf, int offset)
{
	return ((char *)buf->direct.buf + offset);
}


extern struct workqueue_struct *mlx5_core_wq;

#define STRUCT_FIELD(header, field) \
	.struct_offset_bytes = offsetof(struct ib_unpacked_ ## header, field),      \
	.struct_size_bytes   = sizeof((struct ib_unpacked_ ## header *)0)->field

static inline struct mlx5_core_dev *pci2mlx5_core_dev(struct pci_dev *pdev)
{
	return pci_get_drvdata(pdev);
}

static inline u16 fw_rev_maj(struct mlx5_core_dev *dev)
{
	return ioread32be(&dev->iseg->fw_rev) & 0xffff;
}

static inline u16 fw_rev_min(struct mlx5_core_dev *dev)
{
	return ioread32be(&dev->iseg->fw_rev) >> 16;
}

static inline u16 fw_rev_sub(struct mlx5_core_dev *dev)
{
	return ioread32be(&dev->iseg->cmdif_rev_fw_sub) & 0xffff;
}

static inline u16 cmdif_rev_get(struct mlx5_core_dev *dev)
{
	return ioread32be(&dev->iseg->cmdif_rev_fw_sub) >> 16;
}

static inline int mlx5_get_gid_table_len(u16 param)
{
	if (param > 4) {
		printf("M4_CORE_DRV_NAME: WARN: ""gid table length is zero\n");
		return 0;
	}

	return 8 * (1 << param);
}

static inline void *mlx5_vzalloc(unsigned long size)
{
	void *rtn;

	rtn = kzalloc(size, GFP_KERNEL | __GFP_NOWARN);
	return rtn;
}

static inline void *mlx5_vmalloc(unsigned long size)
{
	void *rtn;

	rtn = kmalloc(size, GFP_KERNEL | __GFP_NOWARN);
	if (!rtn)
		rtn = vmalloc(size);
	return rtn;
}

static inline u32 mlx5_base_mkey(const u32 key)
{
	return key & 0xffffff00u;
}

int mlx5_cmd_init(struct mlx5_core_dev *dev);
void mlx5_cmd_cleanup(struct mlx5_core_dev *dev);
void mlx5_cmd_use_events(struct mlx5_core_dev *dev);
void mlx5_cmd_use_polling(struct mlx5_core_dev *dev);
void mlx5_cmd_mbox_status(void *out, u8 *status, u32 *syndrome);
int mlx5_core_get_caps(struct mlx5_core_dev *dev, enum mlx5_cap_type cap_type);

struct mlx5_async_ctx {
	struct mlx5_core_dev *dev;
	atomic_t num_inflight;
	struct wait_queue_head wait;
};

struct mlx5_async_work;

typedef void (*mlx5_async_cbk_t)(int status, struct mlx5_async_work *context);

struct mlx5_async_work {
	struct mlx5_async_ctx *ctx;
	mlx5_async_cbk_t user_callback;
};

void mlx5_cmd_init_async_ctx(struct mlx5_core_dev *dev,
			     struct mlx5_async_ctx *ctx);
void mlx5_cmd_cleanup_async_ctx(struct mlx5_async_ctx *ctx);
int mlx5_cmd_exec_cb(struct mlx5_async_ctx *ctx, void *in, int in_size,
		     void *out, int out_size, mlx5_async_cbk_t callback,
		     struct mlx5_async_work *work);
int mlx5_cmd_exec(struct mlx5_core_dev *dev, void *in, int in_size, void *out,
		  int out_size);
#define mlx5_cmd_exec_inout(dev, ifc_cmd, in, out)                             \
	({                                                                     \
		mlx5_cmd_exec(dev, in, MLX5_ST_SZ_BYTES(ifc_cmd##_in), out,    \
			      MLX5_ST_SZ_BYTES(ifc_cmd##_out));                \
	})

#define mlx5_cmd_exec_in(dev, ifc_cmd, in)                                     \
	({                                                                     \
		u32 _out[MLX5_ST_SZ_DW(ifc_cmd##_out)] = {};                   \
		mlx5_cmd_exec_inout(dev, ifc_cmd, in, _out);                   \
	})
int mlx5_cmd_exec_polling(struct mlx5_core_dev *dev, void *in, int in_size,
			  void *out, int out_size);
int mlx5_cmd_alloc_uar(struct mlx5_core_dev *dev, u32 *uarn);
int mlx5_cmd_free_uar(struct mlx5_core_dev *dev, u32 uarn);
int mlx5_alloc_bfreg(struct mlx5_core_dev *mdev, struct mlx5_sq_bfreg *bfreg,
		     bool map_wc, bool fast_path);
void mlx5_free_bfreg(struct mlx5_core_dev *mdev, struct mlx5_sq_bfreg *bfreg);
struct mlx5_uars_page *mlx5_get_uars_page(struct mlx5_core_dev *mdev);
void mlx5_put_uars_page(struct mlx5_core_dev *mdev, struct mlx5_uars_page *up);
void mlx5_health_cleanup(struct mlx5_core_dev *dev);
int mlx5_health_init(struct mlx5_core_dev *dev);
void mlx5_start_health_poll(struct mlx5_core_dev *dev);
void mlx5_stop_health_poll(struct mlx5_core_dev *dev, bool disable_health);
void mlx5_drain_health_wq(struct mlx5_core_dev *dev);
void mlx5_drain_health_recovery(struct mlx5_core_dev *dev);
void mlx5_trigger_health_work(struct mlx5_core_dev *dev);
void mlx5_trigger_health_watchdog(struct mlx5_core_dev *dev);

int mlx5_buf_alloc(struct mlx5_core_dev *dev, int size, int max_direct,
		   struct mlx5_buf *buf);
void mlx5_buf_free(struct mlx5_core_dev *dev, struct mlx5_buf *buf);
int mlx5_core_create_srq(struct mlx5_core_dev *dev, struct mlx5_core_srq *srq,
			 struct mlx5_srq_attr *in);
int mlx5_core_destroy_srq(struct mlx5_core_dev *dev, struct mlx5_core_srq *srq);
int mlx5_core_query_srq(struct mlx5_core_dev *dev, struct mlx5_core_srq *srq,
			struct mlx5_srq_attr *out);
int mlx5_core_query_vendor_id(struct mlx5_core_dev *mdev, u32 *vendor_id);
int mlx5_core_arm_srq(struct mlx5_core_dev *dev, struct mlx5_core_srq *srq,
		      u16 lwm, int is_srq);
void mlx5_init_mr_table(struct mlx5_core_dev *dev);
void mlx5_cleanup_mr_table(struct mlx5_core_dev *dev);
int mlx5_core_create_mkey_cb(struct mlx5_core_dev *dev,
			     struct mlx5_core_mkey *mkey,
			     struct mlx5_async_ctx *async_ctx, u32 *in,
			     int inlen, u32 *out, int outlen,
			     mlx5_async_cbk_t callback,
			     struct mlx5_async_work *context);
int mlx5_core_create_mkey(struct mlx5_core_dev *dev,
			  struct mlx5_core_mkey *mr,
			  u32 *in, int inlen);
int mlx5_core_destroy_mkey(struct mlx5_core_dev *dev, struct mlx5_core_mkey *mkey);
int mlx5_core_query_mkey(struct mlx5_core_dev *dev, struct mlx5_core_mkey *mkey,
			 u32 *out, int outlen);
int mlx5_core_dump_fill_mkey(struct mlx5_core_dev *dev, struct mlx5_core_mkey *mr,
			     u32 *mkey);
int mlx5_core_alloc_pd(struct mlx5_core_dev *dev, u32 *pdn, u16 uid);
int mlx5_core_dealloc_pd(struct mlx5_core_dev *dev, u32 pdn, u16 uid);
int mlx5_core_mad_ifc(struct mlx5_core_dev *dev, const void *inb, void *outb,
		      u16 opmod, u8 port);
void mlx5_fwp_flush(struct mlx5_fw_page *fwp);
void mlx5_fwp_invalidate(struct mlx5_fw_page *fwp);
struct mlx5_fw_page *mlx5_fwp_alloc(struct mlx5_core_dev *dev, gfp_t flags, unsigned num);
void mlx5_fwp_free(struct mlx5_fw_page *fwp);
u64 mlx5_fwp_get_dma(struct mlx5_fw_page *fwp, size_t offset);
void *mlx5_fwp_get_virt(struct mlx5_fw_page *fwp, size_t offset);
void mlx5_pagealloc_init(struct mlx5_core_dev *dev);
void mlx5_pagealloc_cleanup(struct mlx5_core_dev *dev);
int mlx5_pagealloc_start(struct mlx5_core_dev *dev);
void mlx5_pagealloc_stop(struct mlx5_core_dev *dev);
void mlx5_core_req_pages_handler(struct mlx5_core_dev *dev, u16 func_id,
				 s32 npages);
int mlx5_satisfy_startup_pages(struct mlx5_core_dev *dev, int boot);
int mlx5_reclaim_startup_pages(struct mlx5_core_dev *dev);
s64 mlx5_wait_for_reclaim_vfs_pages(struct mlx5_core_dev *dev);
void mlx5_register_debugfs(void);
void mlx5_unregister_debugfs(void);
int mlx5_eq_init(struct mlx5_core_dev *dev);
void mlx5_eq_cleanup(struct mlx5_core_dev *dev);
void mlx5_fill_page_array(struct mlx5_buf *buf, __be64 *pas);
void mlx5_cq_completion(struct mlx5_core_dev *dev, struct mlx5_eqe *eqe);
void mlx5_rsc_event(struct mlx5_core_dev *dev, u32 rsn, int event_type);
void mlx5_srq_event(struct mlx5_core_dev *dev, u32 srqn, int event_type);
struct mlx5_core_srq *mlx5_core_get_srq(struct mlx5_core_dev *dev, u32 srqn);
void mlx5_cmd_comp_handler(struct mlx5_core_dev *dev, u64 vector, enum mlx5_cmd_mode mode);
void mlx5_cq_event(struct mlx5_core_dev *dev, u32 cqn, int event_type);
int mlx5_create_map_eq(struct mlx5_core_dev *dev, struct mlx5_eq *eq, u8 vecidx,
		       int nent, u64 mask);
int mlx5_destroy_unmap_eq(struct mlx5_core_dev *dev, struct mlx5_eq *eq);
int mlx5_start_eqs(struct mlx5_core_dev *dev);
int mlx5_stop_eqs(struct mlx5_core_dev *dev);
int mlx5_vector2eqn(struct mlx5_core_dev *dev, int vector, int *eqn, int *irqn);
int mlx5_core_attach_mcg(struct mlx5_core_dev *dev, union ib_gid *mgid, u32 qpn);
int mlx5_core_detach_mcg(struct mlx5_core_dev *dev, union ib_gid *mgid, u32 qpn);
int mlx5_core_set_dc_cnak_trace(struct mlx5_core_dev *dev, int enable,
				u64 addr);

int mlx5_qp_debugfs_init(struct mlx5_core_dev *dev);
void mlx5_qp_debugfs_cleanup(struct mlx5_core_dev *dev);
int mlx5_core_access_reg(struct mlx5_core_dev *dev, void *data_in,
			 int size_in, void *data_out, int size_out,
			 u16 reg_num, int arg, int write);

void mlx5_toggle_port_link(struct mlx5_core_dev *dev);

int mlx5_debug_eq_add(struct mlx5_core_dev *dev, struct mlx5_eq *eq);
void mlx5_debug_eq_remove(struct mlx5_core_dev *dev, struct mlx5_eq *eq);
int mlx5_core_eq_query(struct mlx5_core_dev *dev, struct mlx5_eq *eq,
		       u32 *out, int outlen);
int mlx5_eq_debugfs_init(struct mlx5_core_dev *dev);
void mlx5_eq_debugfs_cleanup(struct mlx5_core_dev *dev);
int mlx5_cq_debugfs_init(struct mlx5_core_dev *dev);
void mlx5_cq_debugfs_cleanup(struct mlx5_core_dev *dev);
int mlx5_db_alloc(struct mlx5_core_dev *dev, struct mlx5_db *db);
void mlx5_db_free(struct mlx5_core_dev *dev, struct mlx5_db *db);

static inline struct domainset *
mlx5_dev_domainset(struct mlx5_core_dev *mdev)
{
	return (linux_get_vm_domain_set(mdev->priv.numa_node));
}

const char *mlx5_command_str(int command);
int mlx5_cmdif_debugfs_init(struct mlx5_core_dev *dev);
void mlx5_cmdif_debugfs_cleanup(struct mlx5_core_dev *dev);
int mlx5_core_create_psv(struct mlx5_core_dev *dev, u32 pdn,
			 int npsvs, u32 *sig_index);
int mlx5_core_destroy_psv(struct mlx5_core_dev *dev, int psv_num);
void mlx5_core_put_rsc(struct mlx5_core_rsc_common *common);
u8 mlx5_is_wol_supported(struct mlx5_core_dev *dev);
int mlx5_set_wol(struct mlx5_core_dev *dev, u8 wol_mode);
int mlx5_set_dropless_mode(struct mlx5_core_dev *dev, u16 timeout);
int mlx5_query_dropless_mode(struct mlx5_core_dev *dev, u16 *timeout);
int mlx5_query_wol(struct mlx5_core_dev *dev, u8 *wol_mode);
int mlx5_core_access_pvlc(struct mlx5_core_dev *dev,
			  struct mlx5_pvlc_reg *pvlc, int write);
int mlx5_core_access_ptys(struct mlx5_core_dev *dev,
			  struct mlx5_ptys_reg *ptys, int write);
int mlx5_core_access_pmtu(struct mlx5_core_dev *dev,
			  struct mlx5_pmtu_reg *pmtu, int write);
int mlx5_vxlan_udp_port_add(struct mlx5_core_dev *dev, u16 port);
int mlx5_vxlan_udp_port_delete(struct mlx5_core_dev *dev, u16 port);
int mlx5_query_port_cong_status(struct mlx5_core_dev *mdev, int protocol,
				int priority, int *is_enable);
int mlx5_modify_port_cong_status(struct mlx5_core_dev *mdev, int protocol,
				 int priority, int enable);
int mlx5_query_port_cong_params(struct mlx5_core_dev *mdev, int protocol,
				void *out, int out_size);
int mlx5_modify_port_cong_params(struct mlx5_core_dev *mdev,
				 void *in, int in_size);
int mlx5_query_port_cong_statistics(struct mlx5_core_dev *mdev, int clear,
				    void *out, int out_size);
int mlx5_set_diagnostic_params(struct mlx5_core_dev *mdev, void *in,
			       int in_size);
int mlx5_query_diagnostic_counters(struct mlx5_core_dev *mdev,
				   u8 num_of_samples, u16 sample_index,
				   void *out, int out_size);
int mlx5_vsc_find_cap(struct mlx5_core_dev *mdev);
int mlx5_vsc_lock(struct mlx5_core_dev *mdev);
void mlx5_vsc_unlock(struct mlx5_core_dev *mdev);
int mlx5_vsc_set_space(struct mlx5_core_dev *mdev, u16 space);
int mlx5_vsc_wait_on_flag(struct mlx5_core_dev *mdev, u32 expected);
int mlx5_vsc_write(struct mlx5_core_dev *mdev, u32 addr, const u32 *data);
int mlx5_vsc_read(struct mlx5_core_dev *mdev, u32 addr, u32 *data);
int mlx5_vsc_lock_addr_space(struct mlx5_core_dev *mdev, u32 addr);
int mlx5_vsc_unlock_addr_space(struct mlx5_core_dev *mdev, u32 addr);
int mlx5_pci_read_power_status(struct mlx5_core_dev *mdev,
			       u16 *p_power, u8 *p_status);

static inline u32 mlx5_mkey_to_idx(u32 mkey)
{
	return mkey >> 8;
}

static inline u32 mlx5_idx_to_mkey(u32 mkey_idx)
{
	return mkey_idx << 8;
}

static inline u8 mlx5_mkey_variant(u32 mkey)
{
	return mkey & 0xff;
}

enum {
	MLX5_PROF_MASK_QP_SIZE		= (u64)1 << 0,
	MLX5_PROF_MASK_MR_CACHE		= (u64)1 << 1,
};

enum {
	MAX_MR_CACHE_ENTRIES    = 15,
};

struct mlx5_interface {
	void *			(*add)(struct mlx5_core_dev *dev);
	void			(*remove)(struct mlx5_core_dev *dev, void *context);
	void			(*event)(struct mlx5_core_dev *dev, void *context,
					 enum mlx5_dev_event event, unsigned long param);
	void *                  (*get_dev)(void *context);
	int			protocol;
	struct list_head	list;
};

void *mlx5_get_protocol_dev(struct mlx5_core_dev *mdev, int protocol);
int mlx5_register_interface(struct mlx5_interface *intf);
void mlx5_unregister_interface(struct mlx5_interface *intf);

unsigned int mlx5_core_reserved_gids_count(struct mlx5_core_dev *dev);
int mlx5_core_roce_gid_set(struct mlx5_core_dev *dev, unsigned int index,
    u8 roce_version, u8 roce_l3_type, const u8 *gid,
    const u8 *mac, bool vlan, u16 vlan_id);

struct mlx5_profile {
	u64	mask;
	u8	log_max_qp;
	struct {
		int	size;
		int	limit;
	} mr_cache[MAX_MR_CACHE_ENTRIES];
};

enum {
	MLX5_PCI_DEV_IS_VF		= 1 << 0,
};

enum {
	MLX5_TRIGGERED_CMD_COMP = (u64)1 << 32,
};

static inline int mlx5_core_is_pf(struct mlx5_core_dev *dev)
{
	return !(dev->priv.pci_dev_data & MLX5_PCI_DEV_IS_VF);
}
#ifdef RATELIMIT
int mlx5_init_rl_table(struct mlx5_core_dev *dev);
void mlx5_cleanup_rl_table(struct mlx5_core_dev *dev);
int mlx5_rl_add_rate(struct mlx5_core_dev *dev, u32 rate, u32 burst, u16 *index);
void mlx5_rl_remove_rate(struct mlx5_core_dev *dev, u32 rate, u32 burst);
bool mlx5_rl_is_in_range(const struct mlx5_core_dev *dev, u32 rate, u32 burst);
int mlx5e_query_rate_limit_cmd(struct mlx5_core_dev *dev, u16 index, u32 *scq_handle);

static inline u32 mlx5_rl_get_scq_handle(struct mlx5_core_dev *dev, uint16_t index)
{
	KASSERT(index > 0,
	    ("invalid rate index for sq remap, failed retrieving SCQ handle"));

        return (dev->priv.rl_table.rl_entry[index - 1].qos_handle);
}

static inline bool mlx5_rl_is_supported(struct mlx5_core_dev *dev)
{
	return !!(dev->priv.rl_table.max_size);
}
#endif

void mlx5_disable_interrupts(struct mlx5_core_dev *);
void mlx5_poll_interrupts(struct mlx5_core_dev *);

static inline int mlx5_get_qp_default_ts(struct mlx5_core_dev *dev)
{
        return !MLX5_CAP_ROCE(dev, qp_ts_format) ?
                       MLX5_QPC_TIMESTAMP_FORMAT_FREE_RUNNING :
                       MLX5_QPC_TIMESTAMP_FORMAT_DEFAULT;
}

static inline int mlx5_get_rq_default_ts(struct mlx5_core_dev *dev)
{
        return !MLX5_CAP_GEN(dev, rq_ts_format) ?
                       MLX5_RQC_TIMESTAMP_FORMAT_FREE_RUNNING :
                       MLX5_RQC_TIMESTAMP_FORMAT_DEFAULT;
}

static inline int mlx5_get_sq_default_ts(struct mlx5_core_dev *dev)
{
        return !MLX5_CAP_GEN(dev, sq_ts_format) ?
                       MLX5_SQC_TIMESTAMP_FORMAT_FREE_RUNNING :
                       MLX5_SQC_TIMESTAMP_FORMAT_DEFAULT;
}

#endif /* MLX5_DRIVER_H */
