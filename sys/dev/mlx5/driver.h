/*-
 * Copyright (c) 2013-2015, Mellanox Technologies, Ltd.  All rights reserved.
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
 *
 * $FreeBSD$
 */

#ifndef MLX5_DRIVER_H
#define MLX5_DRIVER_H

#include <linux/kernel.h>
#include <linux/completion.h>
#include <linux/pci.h>
#include <linux/cache.h>
#include <linux/rbtree.h>
#include <linux/semaphore.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/radix-tree.h>

#include <dev/mlx5/device.h>
#include <dev/mlx5/doorbell.h>

enum {
	MLX5_BOARD_ID_LEN = 64,
	MLX5_MAX_NAME_LEN = 16,
};

enum {
	/* one minute for the sake of bringup. Generally, commands must always
	 * complete and we may need to increase this timeout value
	 */
	MLX5_CMD_TIMEOUT_MSEC	= 7200 * 1000,
	MLX5_CMD_WQ_MAX_NAME	= 32,
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
	MLX5_MAX_IRQ_NAME	= 32
};

enum {
	MLX5_ATOMIC_MODE_IB_COMP	= 1 << 16,
	MLX5_ATOMIC_MODE_CX		= 2 << 16,
	MLX5_ATOMIC_MODE_8B		= 3 << 16,
	MLX5_ATOMIC_MODE_16B		= 4 << 16,
	MLX5_ATOMIC_MODE_32B		= 5 << 16,
	MLX5_ATOMIC_MODE_64B		= 6 << 16,
	MLX5_ATOMIC_MODE_128B		= 7 << 16,
	MLX5_ATOMIC_MODE_256B		= 8 << 16,
};

enum {
	MLX5_REG_QETCR		 = 0x4005,
	MLX5_REG_QPDP		 = 0x4007,
	MLX5_REG_QTCT		 = 0x400A,
	MLX5_REG_PCAP		 = 0x5001,
	MLX5_REG_PMTU		 = 0x5003,
	MLX5_REG_PTYS		 = 0x5004,
	MLX5_REG_PAOS		 = 0x5006,
	MLX5_REG_PFCC		 = 0x5007,
	MLX5_REG_PPCNT		 = 0x5008,
	MLX5_REG_PMAOS		 = 0x5012,
	MLX5_REG_PUDE		 = 0x5009,
	MLX5_REG_PPTB		 = 0x500B,
	MLX5_REG_PBMC		 = 0x500C,
	MLX5_REG_PMPE		 = 0x5010,
	MLX5_REG_PELC		 = 0x500e,
	MLX5_REG_PVLC		 = 0x500f,
	MLX5_REG_PMLP		 = 0x5002,
	MLX5_REG_NODE_DESC	 = 0x6001,
	MLX5_REG_HOST_ENDIANNESS = 0x7004,
	MLX5_REG_MCIA		 = 0x9014,
};

enum dbg_rsc_type {
	MLX5_DBG_RSC_QP,
	MLX5_DBG_RSC_EQ,
	MLX5_DBG_RSC_CQ,
};

struct mlx5_field_desc {
	struct dentry	       *dent;
	int			i;
};

struct mlx5_rsc_debug {
	struct mlx5_core_dev   *dev;
	void		       *object;
	enum dbg_rsc_type	type;
	struct dentry	       *root;
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
};

enum mlx5_port_status {
	MLX5_PORT_UP        = 1 << 0,
	MLX5_PORT_DOWN      = 1 << 1,
};

enum mlx5_link_mode {
	MLX5_1000BASE_CX_SGMII	= 0,
	MLX5_1000BASE_KX	= 1,
	MLX5_10GBASE_CX4	= 2,
	MLX5_10GBASE_KX4	= 3,
	MLX5_10GBASE_KR		= 4,
	MLX5_20GBASE_KR2	= 5,
	MLX5_40GBASE_CR4	= 6,
	MLX5_40GBASE_KR4	= 7,
	MLX5_56GBASE_R4		= 8,
	MLX5_10GBASE_CR		= 12,
	MLX5_10GBASE_SR		= 13,
	MLX5_10GBASE_ER		= 14,
	MLX5_40GBASE_SR4	= 15,
	MLX5_40GBASE_LR4	= 16,
	MLX5_100GBASE_CR4	= 20,
	MLX5_100GBASE_SR4	= 21,
	MLX5_100GBASE_KR4	= 22,
	MLX5_100GBASE_LR4	= 23,
	MLX5_100BASE_TX		= 24,
	MLX5_1000BASE_T		= 25,
	MLX5_10GBASE_T		= 26,
	MLX5_25GBASE_CR		= 27,
	MLX5_25GBASE_KR		= 28,
	MLX5_25GBASE_SR		= 29,
	MLX5_50GBASE_CR2	= 30,
	MLX5_50GBASE_KR2	= 31,
	MLX5_LINK_MODES_NUMBER,
};

#define MLX5_PROT_MASK(link_mode) (1 << link_mode)

struct mlx5_uuar_info {
	struct mlx5_uar	       *uars;
	int			num_uars;
	int			num_low_latency_uuars;
	unsigned long	       *bitmap;
	unsigned int	       *count;
	struct mlx5_bf	       *bfs;

	/*
	 * protect uuar allocation data structs
	 */
	struct mutex		lock;
	u32			ver;
};

struct mlx5_bf {
	void __iomem	       *reg;
	void __iomem	       *regreg;
	int			buf_size;
	struct mlx5_uar	       *uar;
	unsigned long		offset;
	int			need_lock;
	/* protect blue flame buffer selection when needed
	 */
	spinlock_t		lock;

	/* serialize 64 bit writes when done as two 32 bit accesses
	 */
	spinlock_t		lock32;
	int			uuarn;
};

struct mlx5_cmd_first {
	__be32		data[4];
};

struct mlx5_cmd_msg {
	struct list_head		list;
	struct cache_ent	       *cache;
	u32				len;
	struct mlx5_cmd_first		first;
	struct mlx5_cmd_mailbox	       *next;
};

struct mlx5_cmd_debug {
	struct dentry	       *dbg_root;
	struct dentry	       *dbg_in;
	struct dentry	       *dbg_out;
	struct dentry	       *dbg_outlen;
	struct dentry	       *dbg_status;
	struct dentry	       *dbg_run;
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

struct mlx5_cmd_stats {
	u64		sum;
	u64		n;
	struct dentry  *root;
	struct dentry  *avg;
	struct dentry  *count;
	/* protect command average calculations */
	spinlock_t	lock;
};

struct mlx5_cmd {
	void	       *cmd_alloc_buf;
	dma_addr_t	alloc_dma;
	int		alloc_size;
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
	char		wq_name[MLX5_CMD_WQ_MAX_NAME];
	struct workqueue_struct *wq;
	struct semaphore sem;
	struct semaphore pages_sem;
	int	mode;
	struct mlx5_cmd_work_ent *ent_arr[MLX5_MAX_COMMANDS];
	struct pci_pool *pool;
	struct mlx5_cmd_debug dbg;
	struct cmd_msg_cache cache;
	int checksum_disabled;
	struct mlx5_cmd_stats stats[MLX5_CMD_OP_MAX];
	int moving_to_polling;
};

struct mlx5_port_caps {
	int	gid_table_len;
	int	pkey_table_len;
	u8	ext_port_cap;
};

struct mlx5_cmd_mailbox {
	void	       *buf;
	dma_addr_t	dma;
	struct mlx5_cmd_mailbox *next;
};

struct mlx5_buf_list {
	void		       *buf;
	dma_addr_t		map;
};

struct mlx5_buf {
	struct mlx5_buf_list	direct;
	struct mlx5_buf_list   *page_list;
	int			nbufs;
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
#if (__FreeBSD_version >= 1100000)
	struct ib_sig_err       err_item;
#endif
	bool			sig_status_checked;
	bool			sig_err_exists;
	u32			sigerr_count;
};

struct mlx5_core_mr {
	u64			iova;
	u64			size;
	u32			key;
	u32			pd;
};

enum mlx5_res_type {
	MLX5_RES_QP,
	MLX5_RES_SRQ,
	MLX5_RES_XSRQ,
};

struct mlx5_core_rsc_common {
	enum mlx5_res_type	res;
	atomic_t		refcount;
	struct completion	free;
};

struct mlx5_core_srq {
	struct mlx5_core_rsc_common	common; /* must be first */
	u32				srqn;
	int				max;
	int				max_gs;
	int				max_avail_gather;
	int				wqe_shift;
	void				(*event)(struct mlx5_core_srq *, int);
	atomic_t			refcount;
	struct completion		free;
};

struct mlx5_eq_table {
	void __iomem	       *update_ci;
	void __iomem	       *update_arm_ci;
	struct list_head	comp_eqs_list;
	struct mlx5_eq		pages_eq;
	struct mlx5_eq		async_eq;
	struct mlx5_eq		cmd_eq;
	int			num_comp_vectors;
	/* protect EQs list
	 */
	spinlock_t		lock;
};

struct mlx5_uar {
	u32			index;
	struct list_head	bf_list;
	unsigned		free_bf_bmap;
	void __iomem	       *bf_map;
	void __iomem	       *map;
};


struct mlx5_core_health {
	struct mlx5_health_buffer __iomem	*health;
	__be32 __iomem		       *health_counter;
	struct timer_list		timer;
	struct list_head		list;
	u32				prev;
	int				miss_counter;
};

#define	MLX5_CQ_LINEAR_ARRAY_SIZE	1024

struct mlx5_cq_linear_array_entry {
	spinlock_t	lock;
	struct mlx5_core_cq * volatile cq;
};

struct mlx5_cq_table {
	/* protect radix tree
	 */
	spinlock_t		lock;
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
	rwlock_t		lock;
	struct radix_tree_root	tree;
};

struct mlx5_irq_info {
	char name[MLX5_MAX_IRQ_NAME];
};

struct mlx5_priv {
	char			name[MLX5_MAX_NAME_LEN];
	struct mlx5_eq_table	eq_table;
	struct msix_entry	*msix_arr;
	struct mlx5_irq_info	*irq_info;
	struct mlx5_uuar_info	uuari;
	MLX5_DECLARE_DOORBELL_LOCK(cq_uar_lock);

	struct io_mapping	*bf_mapping;

	/* pages stuff */
	struct workqueue_struct *pg_wq;
	struct rb_root		page_root;
	int			fw_pages;
	int			reg_pages;
	struct list_head	free_list;

	struct mlx5_core_health health;

	struct mlx5_srq_table	srq_table;

	/* start: qp staff */
	struct mlx5_qp_table	qp_table;
	struct dentry	       *qp_debugfs;
	struct dentry	       *eq_debugfs;
	struct dentry	       *cq_debugfs;
	struct dentry	       *cmdif_debugfs;
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
	struct dentry	       *dbg_root;

	/* protect mkey key part */
	spinlock_t		mkey_lock;
	u8			mkey_key;

	struct list_head        dev_list;
	struct list_head        ctx_list;
	spinlock_t              ctx_lock;
};

struct mlx5_special_contexts {
	int resd_lkey;
};

struct mlx5_core_dev {
	struct pci_dev	       *pdev;
	char			board_id[MLX5_BOARD_ID_LEN];
	struct mlx5_cmd		cmd;
	struct mlx5_port_caps	port_caps[MLX5_MAX_PORTS];
	u32 hca_caps_cur[MLX5_CAP_NUM][MLX5_UN_SZ_DW(hca_cap_union)];
	u32 hca_caps_max[MLX5_CAP_NUM][MLX5_UN_SZ_DW(hca_cap_union)];
	struct mlx5_init_seg __iomem *iseg;
	void			(*event) (struct mlx5_core_dev *dev,
					  enum mlx5_dev_event event,
					  unsigned long param);
	struct mlx5_priv	priv;
	struct mlx5_profile	*profile;
	atomic_t		num_qps;
	u32			issi;
	struct mlx5_special_contexts special_contexts;
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
	MLX5_DB_PER_PAGE = PAGE_SIZE / L1_CACHE_BYTES,
};

enum {
	MLX5_COMP_EQ_SIZE = 1024,
};

enum {
	MLX5_PTYS_IB = 1 << 0,
	MLX5_PTYS_EN = 1 << 2,
};

struct mlx5_db_pgdir {
	struct list_head	list;
	DECLARE_BITMAP(bitmap, MLX5_DB_PER_PAGE);
	__be32		       *db_page;
	dma_addr_t		db_dma;
};

typedef void (*mlx5_cmd_cbk_t)(int status, void *context);

struct mlx5_cmd_work_ent {
	struct mlx5_cmd_msg    *in;
	struct mlx5_cmd_msg    *out;
	void		       *uout;
	int			uout_size;
	mlx5_cmd_cbk_t		callback;
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
};

struct mlx5_pas {
	u64	pa;
	u8	log_sz;
};

static inline void *mlx5_buf_offset(struct mlx5_buf *buf, int offset)
{
	if (likely(BITS_PER_LONG == 64 || buf->nbufs == 1))
		return buf->direct.buf + offset;
	else
		return buf->page_list[offset >> PAGE_SHIFT].buf +
			(offset & (PAGE_SIZE - 1));
}


extern struct workqueue_struct *mlx5_core_wq;

#define STRUCT_FIELD(header, field) \
	.struct_offset_bytes = offsetof(struct ib_unpacked_ ## header, field),      \
	.struct_size_bytes   = sizeof((struct ib_unpacked_ ## header *)0)->field

static inline struct mlx5_core_dev *pci2mlx5_core_dev(struct pci_dev *pdev)
{
	return pci_get_drvdata(pdev);
}

extern struct dentry *mlx5_debugfs_root;

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

static inline u32 mlx5_base_mkey(const u32 key)
{
	return key & 0xffffff00u;
}

int mlx5_cmd_init(struct mlx5_core_dev *dev);
void mlx5_cmd_cleanup(struct mlx5_core_dev *dev);
void mlx5_cmd_use_events(struct mlx5_core_dev *dev);
void mlx5_cmd_use_polling(struct mlx5_core_dev *dev);
int mlx5_cmd_status_to_err(struct mlx5_outbox_hdr *hdr);
int mlx5_cmd_status_to_err_v2(void *ptr);
int mlx5_core_get_caps(struct mlx5_core_dev *dev, enum mlx5_cap_type cap_type,
		       enum mlx5_cap_mode cap_mode);
int mlx5_cmd_exec(struct mlx5_core_dev *dev, void *in, int in_size, void *out,
		  int out_size);
int mlx5_cmd_exec_cb(struct mlx5_core_dev *dev, void *in, int in_size,
		     void *out, int out_size, mlx5_cmd_cbk_t callback,
		     void *context);
int mlx5_cmd_alloc_uar(struct mlx5_core_dev *dev, u32 *uarn);
int mlx5_cmd_free_uar(struct mlx5_core_dev *dev, u32 uarn);
int mlx5_alloc_uuars(struct mlx5_core_dev *dev, struct mlx5_uuar_info *uuari);
int mlx5_free_uuars(struct mlx5_core_dev *dev, struct mlx5_uuar_info *uuari);
int mlx5_alloc_map_uar(struct mlx5_core_dev *mdev, struct mlx5_uar *uar);
void mlx5_unmap_free_uar(struct mlx5_core_dev *mdev, struct mlx5_uar *uar);
void mlx5_health_cleanup(void);
void  __init mlx5_health_init(void);
void mlx5_start_health_poll(struct mlx5_core_dev *dev);
void mlx5_stop_health_poll(struct mlx5_core_dev *dev);
int mlx5_buf_alloc_node(struct mlx5_core_dev *dev, int size, int max_direct,
			struct mlx5_buf *buf, int node);
int mlx5_buf_alloc(struct mlx5_core_dev *dev, int size, int max_direct,
		   struct mlx5_buf *buf);
void mlx5_buf_free(struct mlx5_core_dev *dev, struct mlx5_buf *buf);
int mlx5_core_create_srq(struct mlx5_core_dev *dev, struct mlx5_core_srq *srq,
			 struct mlx5_create_srq_mbox_in *in, int inlen,
			 int is_xrc);
int mlx5_core_destroy_srq(struct mlx5_core_dev *dev, struct mlx5_core_srq *srq);
int mlx5_core_query_srq(struct mlx5_core_dev *dev, struct mlx5_core_srq *srq,
			struct mlx5_query_srq_mbox_out *out);
int mlx5_core_query_vendor_id(struct mlx5_core_dev *mdev, u32 *vendor_id);
int mlx5_core_arm_srq(struct mlx5_core_dev *dev, struct mlx5_core_srq *srq,
		      u16 lwm, int is_srq);
void mlx5_init_mr_table(struct mlx5_core_dev *dev);
void mlx5_cleanup_mr_table(struct mlx5_core_dev *dev);
int mlx5_core_create_mkey(struct mlx5_core_dev *dev, struct mlx5_core_mr *mr,
			  struct mlx5_create_mkey_mbox_in *in, int inlen,
			  mlx5_cmd_cbk_t callback, void *context,
			  struct mlx5_create_mkey_mbox_out *out);
int mlx5_core_destroy_mkey(struct mlx5_core_dev *dev, struct mlx5_core_mr *mr);
int mlx5_core_query_mkey(struct mlx5_core_dev *dev, struct mlx5_core_mr *mr,
			 struct mlx5_query_mkey_mbox_out *out, int outlen);
int mlx5_core_dump_fill_mkey(struct mlx5_core_dev *dev, struct mlx5_core_mr *mr,
			     u32 *mkey);
int mlx5_core_alloc_pd(struct mlx5_core_dev *dev, u32 *pdn);
int mlx5_core_dealloc_pd(struct mlx5_core_dev *dev, u32 pdn);
int mlx5_core_mad_ifc(struct mlx5_core_dev *dev, void *inb, void *outb,
		      u16 opmod, u8 port);
void mlx5_pagealloc_init(struct mlx5_core_dev *dev);
void mlx5_pagealloc_cleanup(struct mlx5_core_dev *dev);
int mlx5_pagealloc_start(struct mlx5_core_dev *dev);
void mlx5_pagealloc_stop(struct mlx5_core_dev *dev);
void mlx5_core_req_pages_handler(struct mlx5_core_dev *dev, u16 func_id,
				 s32 npages);
int mlx5_satisfy_startup_pages(struct mlx5_core_dev *dev, int boot);
int mlx5_reclaim_startup_pages(struct mlx5_core_dev *dev);
void mlx5_register_debugfs(void);
void mlx5_unregister_debugfs(void);
int mlx5_eq_init(struct mlx5_core_dev *dev);
void mlx5_eq_cleanup(struct mlx5_core_dev *dev);
void mlx5_fill_page_array(struct mlx5_buf *buf, __be64 *pas);
void mlx5_cq_completion(struct mlx5_core_dev *dev, u32 cqn);
void mlx5_rsc_event(struct mlx5_core_dev *dev, u32 rsn, int event_type);
void mlx5_srq_event(struct mlx5_core_dev *dev, u32 srqn, int event_type);
struct mlx5_core_srq *mlx5_core_get_srq(struct mlx5_core_dev *dev, u32 srqn);
void mlx5_cmd_comp_handler(struct mlx5_core_dev *dev, unsigned long vector);
void mlx5_cq_event(struct mlx5_core_dev *dev, u32 cqn, int event_type);
int mlx5_create_map_eq(struct mlx5_core_dev *dev, struct mlx5_eq *eq, u8 vecidx,
		       int nent, u64 mask, const char *name, struct mlx5_uar *uar);
int mlx5_destroy_unmap_eq(struct mlx5_core_dev *dev, struct mlx5_eq *eq);
int mlx5_start_eqs(struct mlx5_core_dev *dev);
int mlx5_stop_eqs(struct mlx5_core_dev *dev);
int mlx5_vector2eqn(struct mlx5_core_dev *dev, int vector, int *eqn, int *irqn);
int mlx5_core_attach_mcg(struct mlx5_core_dev *dev, union ib_gid *mgid, u32 qpn);
int mlx5_core_detach_mcg(struct mlx5_core_dev *dev, union ib_gid *mgid, u32 qpn);

int mlx5_qp_debugfs_init(struct mlx5_core_dev *dev);
void mlx5_qp_debugfs_cleanup(struct mlx5_core_dev *dev);
int mlx5_core_access_reg(struct mlx5_core_dev *dev, void *data_in,
			 int size_in, void *data_out, int size_out,
			 u16 reg_num, int arg, int write);

int mlx5_set_port_caps(struct mlx5_core_dev *dev, u8 port_num, u32 caps);
int mlx5_query_port_ptys(struct mlx5_core_dev *dev, u32 *ptys,
			 int ptys_size, int proto_mask);
int mlx5_query_port_proto_cap(struct mlx5_core_dev *dev,
			      u32 *proto_cap, int proto_mask);
int mlx5_query_port_proto_admin(struct mlx5_core_dev *dev,
				u32 *proto_admin, int proto_mask);
int mlx5_set_port_proto(struct mlx5_core_dev *dev, u32 proto_admin,
			int proto_mask);
int mlx5_set_port_status(struct mlx5_core_dev *dev,
			 enum mlx5_port_status status);
int mlx5_query_port_status(struct mlx5_core_dev *dev, u8 *status);
int mlx5_set_port_pause(struct mlx5_core_dev *dev, u32 port,
			u32 rx_pause, u32 tx_pause);
int mlx5_query_port_pause(struct mlx5_core_dev *dev, u32 port,
			  u32 *rx_pause, u32 *tx_pause);

int mlx5_set_port_mtu(struct mlx5_core_dev *dev, int mtu);
int mlx5_query_port_max_mtu(struct mlx5_core_dev *dev, int *max_mtu);
int mlx5_query_port_oper_mtu(struct mlx5_core_dev *dev, int *oper_mtu);

int mlx5_query_module_num(struct mlx5_core_dev *dev, int *module_num);
int mlx5_query_eeprom(struct mlx5_core_dev *dev, int i2c_addr, int page_num,
		      int device_addr, int size, int module_num, u32 *data,
		      int *size_read);

int mlx5_debug_eq_add(struct mlx5_core_dev *dev, struct mlx5_eq *eq);
void mlx5_debug_eq_remove(struct mlx5_core_dev *dev, struct mlx5_eq *eq);
int mlx5_core_eq_query(struct mlx5_core_dev *dev, struct mlx5_eq *eq,
		       struct mlx5_query_eq_mbox_out *out, int outlen);
int mlx5_eq_debugfs_init(struct mlx5_core_dev *dev);
void mlx5_eq_debugfs_cleanup(struct mlx5_core_dev *dev);
int mlx5_cq_debugfs_init(struct mlx5_core_dev *dev);
void mlx5_cq_debugfs_cleanup(struct mlx5_core_dev *dev);
int mlx5_db_alloc(struct mlx5_core_dev *dev, struct mlx5_db *db);
int mlx5_db_alloc_node(struct mlx5_core_dev *dev, struct mlx5_db *db,
		       int node);
void mlx5_db_free(struct mlx5_core_dev *dev, struct mlx5_db *db);

const char *mlx5_command_str(int command);
int mlx5_cmdif_debugfs_init(struct mlx5_core_dev *dev);
void mlx5_cmdif_debugfs_cleanup(struct mlx5_core_dev *dev);
int mlx5_core_create_psv(struct mlx5_core_dev *dev, u32 pdn,
			 int npsvs, u32 *sig_index);
int mlx5_core_destroy_psv(struct mlx5_core_dev *dev, int psv_num);
void mlx5_core_put_rsc(struct mlx5_core_rsc_common *common);
u8 mlx5_is_wol_supported(struct mlx5_core_dev *dev);
int mlx5_set_wol(struct mlx5_core_dev *dev, u8 wol_mode);
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
	MAX_MR_CACHE_ENTRIES    = 16,
};

enum {
	MLX5_INTERFACE_PROTOCOL_IB  = 0,
	MLX5_INTERFACE_PROTOCOL_ETH = 1,
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

struct mlx5_profile {
	u64	mask;
	u8	log_max_qp;
	struct {
		int	size;
		int	limit;
	} mr_cache[MAX_MR_CACHE_ENTRIES];
};


#define MLX5_EEPROM_MAX_BYTES			32
#define MLX5_EEPROM_IDENTIFIER_BYTE_MASK	0x000000ff
#define MLX5_EEPROM_REVISION_ID_BYTE_MASK	0x0000ff00
#define MLX5_EEPROM_PAGE_3_VALID_BIT_MASK	0x00040000
#endif /* MLX5_DRIVER_H */
