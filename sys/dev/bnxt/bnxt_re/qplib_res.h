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
 * Description: QPLib resource manager (header)
 */

#ifndef __BNXT_QPLIB_RES_H__
#define __BNXT_QPLIB_RES_H__

#include "hsi_struct_def.h"

extern const struct bnxt_qplib_gid bnxt_qplib_gid_zero;

#define CHIP_NUM_57508		0x1750
#define CHIP_NUM_57504		0x1751
#define CHIP_NUM_57502		0x1752
#define CHIP_NUM_58818          0xd818
#define CHIP_NUM_57608		0x1760

#define BNXT_QPLIB_MAX_QPC_COUNT	(64 * 1024)
#define BNXT_QPLIB_MAX_SRQC_COUNT	(64 * 1024)
#define BNXT_QPLIB_MAX_CQ_COUNT		(64 * 1024)
#define BNXT_QPLIB_MAX_CQ_COUNT_P5	(128 * 1024)

#define BNXT_QPLIB_DBR_VALID (0x1UL << 26)
#define BNXT_QPLIB_DBR_EPOCH_SHIFT   24
#define BNXT_QPLIB_DBR_TOGGLE_SHIFT  25

#define BNXT_QPLIB_DBR_PF_DB_OFFSET	0x10000
#define BNXT_QPLIB_DBR_VF_DB_OFFSET	0x4000

#define BNXT_QPLIB_DBR_KEY_INVALID	-1

/* chip gen type */
#define BNXT_RE_DEFAULT 0xf

enum bnxt_qplib_wqe_mode {
	BNXT_QPLIB_WQE_MODE_STATIC	= 0x00,
	BNXT_QPLIB_WQE_MODE_VARIABLE	= 0x01,
	BNXT_QPLIB_WQE_MODE_INVALID	= 0x02
};

#define BNXT_RE_PUSH_MODE_NONE	0
#define BNXT_RE_PUSH_MODE_WCB	1
#define BNXT_RE_PUSH_MODE_PPP	2
#define BNXT_RE_PUSH_ENABLED(mode) ((mode) == BNXT_RE_PUSH_MODE_WCB ||\
				    (mode) == BNXT_RE_PUSH_MODE_PPP)
#define BNXT_RE_PPP_ENABLED(cctx) ((cctx)->modes.db_push_mode ==\
				   BNXT_RE_PUSH_MODE_PPP)
#define	PCI_EXP_DEVCAP2_ATOMIC_ROUTE	0x00000040 /* Atomic Op routing */
#define	PCI_EXP_DEVCAP2_ATOMIC_COMP32	0x00000080 /* 32b AtomicOp completion */
#define	PCI_EXP_DEVCAP2_ATOMIC_COMP64	0x00000100 /* 64b AtomicOp completion */
#define	PCI_EXP_DEVCTL2_ATOMIC_EGRESS_BLOCK 0x0080 /* Block atomic egress */
#define PCI_EXP_DEVCTL2_ATOMIC_REQ	0x0040	/* Set Atomic requests */

int pci_enable_atomic_ops_to_root(struct pci_dev *dev, u32 cap_mask);

struct bnxt_qplib_drv_modes {
	u8	wqe_mode;
	u8	te_bypass;
	u8	db_push;
	/* To control advanced cc params display in configfs */
	u8	cc_pr_mode;
	/* Other modes to follow here e.g. GSI QP mode */
	u8	dbr_pacing;
	u8	dbr_pacing_ext;
	u8	dbr_drop_recov;
	u8	dbr_primary_pf;
	u8	dbr_pacing_v0;
};

struct bnxt_qplib_chip_ctx {
	u16     chip_num;
	u8      chip_rev;
	u8      chip_metal;
	u64	hwrm_intf_ver;
	struct bnxt_qplib_drv_modes	modes;
	u32	dbr_stat_db_fifo;
	u32	dbr_aeq_arm_reg;
	u32	dbr_throttling_reg;
	u16	hw_stats_size;
	u16	hwrm_cmd_max_timeout;
};

static inline bool _is_chip_num_p7(u16 chip_num)
{
	return (chip_num == CHIP_NUM_58818 ||
		chip_num == CHIP_NUM_57608);
}

static inline bool _is_chip_p7(struct bnxt_qplib_chip_ctx *cctx)
{
	return _is_chip_num_p7(cctx->chip_num);
}

/* SR2 is Gen P5 */
static inline bool _is_chip_gen_p5(struct bnxt_qplib_chip_ctx *cctx)
{
	return (cctx->chip_num == CHIP_NUM_57508 ||
		cctx->chip_num == CHIP_NUM_57504 ||
		cctx->chip_num == CHIP_NUM_57502);
}

static inline bool _is_chip_gen_p5_p7(struct bnxt_qplib_chip_ctx *cctx)
{
	return (_is_chip_gen_p5(cctx) || _is_chip_p7(cctx));
}

static inline bool _is_wqe_mode_variable(struct bnxt_qplib_chip_ctx *cctx)
{
	return cctx->modes.wqe_mode == BNXT_QPLIB_WQE_MODE_VARIABLE;
}

struct bnxt_qplib_db_pacing_data {
	u32 do_pacing;
	u32 pacing_th;
	u32 dev_err_state;
	u32 alarm_th;
	u32 grc_reg_offset;
	u32 fifo_max_depth;
	u32 fifo_room_mask;
	u8  fifo_room_shift;
};

static inline u8 bnxt_qplib_dbr_pacing_en(struct bnxt_qplib_chip_ctx *cctx)
{
	return cctx->modes.dbr_pacing;
}

static inline u8 bnxt_qplib_dbr_pacing_ext_en(struct bnxt_qplib_chip_ctx *cctx)
{
	return cctx->modes.dbr_pacing_ext;
}

static inline u8 bnxt_qplib_dbr_pacing_is_primary_pf(struct bnxt_qplib_chip_ctx *cctx)
{
	return cctx->modes.dbr_primary_pf;
}

static inline void bnxt_qplib_dbr_pacing_set_primary_pf
		(struct bnxt_qplib_chip_ctx *cctx, u8 val)
{
	cctx->modes.dbr_primary_pf = val;
}

/* Defines for handling the HWRM version check */
#define HWRM_VERSION_DEV_ATTR_MAX_DPI	0x1000A0000000D
#define HWRM_VERSION_ROCE_STATS_FN_ID	0x1000A00000045

#define PTR_CNT_PER_PG		(PAGE_SIZE / sizeof(void *))
#define PTR_MAX_IDX_PER_PG	(PTR_CNT_PER_PG - 1)
#define PTR_PG(x)		(((x) & ~PTR_MAX_IDX_PER_PG) / PTR_CNT_PER_PG)
#define PTR_IDX(x)		((x) & PTR_MAX_IDX_PER_PG)

#define HWQ_CMP(idx, hwq)	((idx) & ((hwq)->max_elements - 1))
#define HWQ_FREE_SLOTS(hwq)	(hwq->max_elements - \
				((HWQ_CMP(hwq->prod, hwq)\
				- HWQ_CMP(hwq->cons, hwq))\
				& (hwq->max_elements - 1)))
enum bnxt_qplib_hwq_type {
	HWQ_TYPE_CTX,
	HWQ_TYPE_QUEUE,
	HWQ_TYPE_L2_CMPL,
	HWQ_TYPE_MR
};

#define MAX_PBL_LVL_0_PGS		1
#define MAX_PBL_LVL_1_PGS		512
#define MAX_PBL_LVL_1_PGS_SHIFT		9
#define MAX_PDL_LVL_SHIFT		9

enum bnxt_qplib_pbl_lvl {
	PBL_LVL_0,
	PBL_LVL_1,
	PBL_LVL_2,
	PBL_LVL_MAX
};

#define ROCE_PG_SIZE_4K		(4 * 1024)
#define ROCE_PG_SIZE_8K		(8 * 1024)
#define ROCE_PG_SIZE_64K	(64 * 1024)
#define ROCE_PG_SIZE_2M		(2 * 1024 * 1024)
#define ROCE_PG_SIZE_8M		(8 * 1024 * 1024)
#define ROCE_PG_SIZE_1G		(1024 * 1024 * 1024)
enum bnxt_qplib_hwrm_pg_size {
	BNXT_QPLIB_HWRM_PG_SIZE_4K	= 0,
	BNXT_QPLIB_HWRM_PG_SIZE_8K	= 1,
	BNXT_QPLIB_HWRM_PG_SIZE_64K	= 2,
	BNXT_QPLIB_HWRM_PG_SIZE_2M	= 3,
	BNXT_QPLIB_HWRM_PG_SIZE_8M	= 4,
	BNXT_QPLIB_HWRM_PG_SIZE_1G	= 5,
};

struct bnxt_qplib_reg_desc {
	u8		bar_id;
	resource_size_t	bar_base;
	unsigned long	offset;
	void __iomem	*bar_reg;
	size_t		len;
};

struct bnxt_qplib_pbl {
	u32				pg_count;
	u32				pg_size;
	void				**pg_arr;
	dma_addr_t			*pg_map_arr;
};

struct bnxt_qplib_sg_info {
	struct scatterlist              *sghead;
	u32                             nmap;
	u32                             npages;
	u32				pgshft;
	u32				pgsize;
	bool				nopte;
};

struct bnxt_qplib_hwq_attr {
	struct bnxt_qplib_res		*res;
	struct bnxt_qplib_sg_info	*sginfo;
	enum bnxt_qplib_hwq_type	type;
	u32				depth;
	u32				stride;
	u32				aux_stride;
	u32				aux_depth;
};

struct bnxt_qplib_hwq {
	struct pci_dev			*pdev;
	spinlock_t			lock;
	struct bnxt_qplib_pbl		pbl[PBL_LVL_MAX];
	enum bnxt_qplib_pbl_lvl		level;		/* 0, 1, or 2 */
	void				**pbl_ptr;	/* ptr for easy access
							   to the PBL entries */
	dma_addr_t			*pbl_dma_ptr;	/* ptr for easy access
							   to the dma_addr */
	u32				max_elements;
	u32				depth;	/* original requested depth */
	u16				element_size;	/* Size of each entry */
	u16				qe_ppg;		/* queue entry per page */

	u32				prod;		/* raw */
	u32				cons;		/* raw */
	u8				cp_bit;
	u8				is_user;
	u64				*pad_pg;
	u32				pad_stride;
	u32				pad_pgofft;
};

struct bnxt_qplib_db_info {
	void __iomem		*db;
	void __iomem		*priv_db;
	struct bnxt_qplib_hwq	*hwq;
	struct bnxt_qplib_res   *res;
	u32			xid;
	u32			max_slot;
	u32			flags;
	u8			toggle;
	spinlock_t		lock;
	u64			shadow_key;
	u64			shadow_key_arm_ena;
	u32			seed; /* For DB pacing */
};

enum bnxt_qplib_db_info_flags_mask {
	BNXT_QPLIB_FLAG_EPOCH_CONS_SHIFT	= 0x0UL,
	BNXT_QPLIB_FLAG_EPOCH_PROD_SHIFT	= 0x1UL,
	BNXT_QPLIB_FLAG_EPOCH_CONS_MASK		= 0x1UL,
	BNXT_QPLIB_FLAG_EPOCH_PROD_MASK		= 0x2UL,
};

enum bnxt_qplib_db_epoch_flag_shift {
	BNXT_QPLIB_DB_EPOCH_CONS_SHIFT	= BNXT_QPLIB_DBR_EPOCH_SHIFT,
	BNXT_QPLIB_DB_EPOCH_PROD_SHIFT	= (BNXT_QPLIB_DBR_EPOCH_SHIFT - 1)
};

/* Tables */
struct bnxt_qplib_pd_tbl {
	unsigned long			*tbl;
	u32				max;
};

struct bnxt_qplib_sgid_tbl {
	struct bnxt_qplib_gid_info	*tbl;
	u16				*hw_id;
	u16				max;
	u16				active;
	void				*ctx;
	bool                            *vlan;
};

enum {
	BNXT_QPLIB_DPI_TYPE_KERNEL	= 0,
	BNXT_QPLIB_DPI_TYPE_UC		= 1,
	BNXT_QPLIB_DPI_TYPE_WC		= 2
};

struct bnxt_qplib_dpi {
	u32				dpi;
	u32				bit;
	void __iomem			*dbr;
	u64				umdbr;
	u8				type;
};

#define BNXT_QPLIB_MAX_EXTENDED_PPP_PAGES	512
struct bnxt_qplib_dpi_tbl {
	void				**app_tbl;
	unsigned long			*tbl;
	u16				max;
	u16				avail_ppp;
	struct bnxt_qplib_reg_desc	ucreg; /* Hold entire DB bar. */
	struct bnxt_qplib_reg_desc	wcreg;
	void __iomem			*priv_db;
};

struct bnxt_qplib_stats {
	dma_addr_t			dma_map;
	void				*dma;
	u32				size;
	u32				fw_id;
};

struct bnxt_qplib_vf_res {
	u32 max_qp;
	u32 max_mrw;
	u32 max_srq;
	u32 max_cq;
	u32 max_gid;
};

#define BNXT_QPLIB_MAX_QP_CTX_ENTRY_SIZE	448
#define BNXT_QPLIB_MAX_SRQ_CTX_ENTRY_SIZE	64
#define BNXT_QPLIB_MAX_CQ_CTX_ENTRY_SIZE	64
#define BNXT_QPLIB_MAX_MRW_CTX_ENTRY_SIZE	128

#define MAX_TQM_ALLOC_REQ		48
#define MAX_TQM_ALLOC_BLK_SIZE		8
struct bnxt_qplib_tqm_ctx {
	struct bnxt_qplib_hwq		pde;
	enum bnxt_qplib_pbl_lvl		pde_level; /* Original level */
	struct bnxt_qplib_hwq		qtbl[MAX_TQM_ALLOC_REQ];
	u8				qcount[MAX_TQM_ALLOC_REQ];
};

struct bnxt_qplib_hctx {
	struct bnxt_qplib_hwq	hwq;
	u32			max;
};

struct bnxt_qplib_refrec {
	void *handle;
	u32 xid;
};

struct bnxt_qplib_reftbl {
	struct bnxt_qplib_refrec *rec;
	u32 max;
	spinlock_t lock; /* reftbl lock */
};

struct bnxt_qplib_reftbls {
	struct bnxt_qplib_reftbl qpref;
	struct bnxt_qplib_reftbl cqref;
	struct bnxt_qplib_reftbl srqref;
};

#define GET_TBL_INDEX(id, tbl) ((id) % (((tbl)->max) - 1))
static inline u32 map_qp_id_to_tbl_indx(u32 qid, struct bnxt_qplib_reftbl *tbl)
{
	return (qid == 1) ? tbl->max : GET_TBL_INDEX(qid, tbl);
}

/*
 * This structure includes the number of various roce resource table sizes
 * actually allocated by the driver. May be less than the maximums the firmware
 * allows if the driver imposes lower limits than the firmware.
 */
struct bnxt_qplib_ctx {
	struct bnxt_qplib_hctx		qp_ctx;
	struct bnxt_qplib_hctx		mrw_ctx;
	struct bnxt_qplib_hctx		srq_ctx;
	struct bnxt_qplib_hctx		cq_ctx;
	struct bnxt_qplib_hctx		tim_ctx;
	struct bnxt_qplib_tqm_ctx	tqm_ctx;

	struct bnxt_qplib_stats		stats;
	struct bnxt_qplib_stats		stats2;
	struct bnxt_qplib_vf_res	vf_res;
};

struct bnxt_qplib_res {
	struct pci_dev			*pdev;
	struct bnxt_qplib_chip_ctx	*cctx;
	struct bnxt_qplib_dev_attr      *dattr;
	struct bnxt_qplib_ctx		*hctx;
	struct ifnet			*netdev;
	struct bnxt_en_dev		*en_dev;

	struct bnxt_qplib_rcfw		*rcfw;

	struct bnxt_qplib_pd_tbl	pd_tbl;
	struct mutex			pd_tbl_lock;
	struct bnxt_qplib_sgid_tbl	sgid_tbl;
	struct bnxt_qplib_dpi_tbl	dpi_tbl;
	struct mutex			dpi_tbl_lock;
	struct bnxt_qplib_reftbls	reftbl;
	bool				prio;
	bool				is_vf;
	struct bnxt_qplib_db_pacing_data *pacing_data;
};

struct bnxt_qplib_query_stats_info {
	u32 function_id;
	u8 collection_id;
	bool vf_valid;
};

struct bnxt_qplib_query_qp_info {
	u32 function_id;
	u32 num_qps;
	u32 start_index;
	bool vf_valid;
};

struct bnxt_qplib_query_fn_info {
	bool vf_valid;
	u32 host;
	u32 filter;
};


#define to_bnxt_qplib(ptr, type, member)	\
	container_of(ptr, type, member)

struct bnxt_qplib_pd;
struct bnxt_qplib_dev_attr;

bool _is_chip_gen_p5(struct bnxt_qplib_chip_ctx *cctx);
bool _is_chip_gen_p5_p7(struct bnxt_qplib_chip_ctx *cctx);
bool _is_chip_a0(struct bnxt_qplib_chip_ctx *cctx);
bool _is_chip_p7(struct bnxt_qplib_chip_ctx *cctx);
bool _is_alloc_mr_unified(struct bnxt_qplib_dev_attr *dattr);
void bnxt_qplib_free_hwq(struct bnxt_qplib_res *res,
			 struct bnxt_qplib_hwq *hwq);
int bnxt_qplib_alloc_init_hwq(struct bnxt_qplib_hwq *hwq,
			      struct bnxt_qplib_hwq_attr *hwq_attr);
void bnxt_qplib_get_guid(const u8 *dev_addr, u8 *guid);
int bnxt_qplib_alloc_pd(struct bnxt_qplib_res *res,
			struct bnxt_qplib_pd *pd);
int bnxt_qplib_dealloc_pd(struct bnxt_qplib_res *res,
			  struct bnxt_qplib_pd_tbl *pd_tbl,
			  struct bnxt_qplib_pd *pd);
int bnxt_qplib_alloc_dpi(struct bnxt_qplib_res *res,
			 struct bnxt_qplib_dpi *dpi,
			 void *app, u8 type);
int bnxt_qplib_dealloc_dpi(struct bnxt_qplib_res *res,
			   struct bnxt_qplib_dpi *dpi);
int bnxt_qplib_stop_res(struct bnxt_qplib_res *res);
void bnxt_qplib_clear_tbls(struct bnxt_qplib_res *res);
int bnxt_qplib_init_tbls(struct bnxt_qplib_res *res);
void bnxt_qplib_free_tbls(struct bnxt_qplib_res *res);
int bnxt_qplib_alloc_tbls(struct bnxt_qplib_res *res, u8 pppp_factor);
void bnxt_qplib_free_hwctx(struct bnxt_qplib_res *res);
int bnxt_qplib_alloc_hwctx(struct bnxt_qplib_res *res);
int bnxt_qplib_alloc_stat_mem(struct pci_dev *pdev,
			      struct bnxt_qplib_chip_ctx *cctx,
			      struct bnxt_qplib_stats *stats);
void bnxt_qplib_free_stat_mem(struct bnxt_qplib_res *res,
			      struct bnxt_qplib_stats *stats);

int bnxt_qplib_map_db_bar(struct bnxt_qplib_res *res);
void bnxt_qplib_unmap_db_bar(struct bnxt_qplib_res *res);
int bnxt_qplib_enable_atomic_ops_to_root(struct pci_dev *dev);
u8 _get_chip_gen_p5_type(struct bnxt_qplib_chip_ctx *cctx);

static inline void *bnxt_qplib_get_qe(struct bnxt_qplib_hwq *hwq,
				      u32 indx, u64 *pg)
{
	u32 pg_num, pg_idx;

	pg_num = (indx / hwq->qe_ppg);
	pg_idx = (indx % hwq->qe_ppg);
	if (pg)
		*pg = (u64)&hwq->pbl_ptr[pg_num];
	return (void *)((u8 *)hwq->pbl_ptr[pg_num] + hwq->element_size * pg_idx);
}

static inline void bnxt_qplib_hwq_incr_prod(struct bnxt_qplib_db_info *dbinfo,
					    struct bnxt_qplib_hwq *hwq, u32 cnt)
{
	/* move prod and update toggle/epoch if wrap around */
	hwq->prod += cnt;
	if (hwq->prod >= hwq->depth) {
		hwq->prod %= hwq->depth;
		dbinfo->flags ^= 1UL << BNXT_QPLIB_FLAG_EPOCH_PROD_SHIFT;
	}
}

static inline void bnxt_qplib_hwq_incr_cons(u32 max_elements, u32 *cons,
					    u32 cnt, u32 *dbinfo_flags)
{
	/* move cons and update toggle/epoch if wrap around */
	*cons += cnt;
	if (*cons >= max_elements) {
		*cons %= max_elements;
		*dbinfo_flags ^= 1UL << BNXT_QPLIB_FLAG_EPOCH_CONS_SHIFT;
	}
}

static inline u8 _get_pte_pg_size(struct bnxt_qplib_hwq *hwq)
{
	u8 pg_size = BNXT_QPLIB_HWRM_PG_SIZE_4K;
	struct bnxt_qplib_pbl *pbl;

	pbl = &hwq->pbl[hwq->level];
	switch (pbl->pg_size) {
		case ROCE_PG_SIZE_4K: pg_size = BNXT_QPLIB_HWRM_PG_SIZE_4K;
		break;
		case ROCE_PG_SIZE_8K: pg_size = BNXT_QPLIB_HWRM_PG_SIZE_8K;
		break;
		case ROCE_PG_SIZE_64K: pg_size = BNXT_QPLIB_HWRM_PG_SIZE_64K;
		break;
		case ROCE_PG_SIZE_2M: pg_size = BNXT_QPLIB_HWRM_PG_SIZE_2M;
		break;
		case ROCE_PG_SIZE_8M: pg_size = BNXT_QPLIB_HWRM_PG_SIZE_8M;
		break;
		case ROCE_PG_SIZE_1G: pg_size = BNXT_QPLIB_HWRM_PG_SIZE_1G;
		break;
		default:
		break;
	}
	return pg_size;
}

static inline u64 _get_base_addr(struct bnxt_qplib_hwq *hwq)
{
	return hwq->pbl[PBL_LVL_0].pg_map_arr[0];
}

static inline u8 _get_base_pg_size(struct bnxt_qplib_hwq *hwq)
{
	u8 pg_size = BNXT_QPLIB_HWRM_PG_SIZE_4K;
	struct bnxt_qplib_pbl *pbl;

	pbl = &hwq->pbl[PBL_LVL_0];
	switch (pbl->pg_size) {
		case ROCE_PG_SIZE_4K: pg_size = BNXT_QPLIB_HWRM_PG_SIZE_4K;
		break;
		case ROCE_PG_SIZE_8K: pg_size = BNXT_QPLIB_HWRM_PG_SIZE_8K;
		break;
		case ROCE_PG_SIZE_64K: pg_size = BNXT_QPLIB_HWRM_PG_SIZE_64K;
		break;
		case ROCE_PG_SIZE_2M: pg_size = BNXT_QPLIB_HWRM_PG_SIZE_2M;
		break;
		case ROCE_PG_SIZE_8M: pg_size = BNXT_QPLIB_HWRM_PG_SIZE_8M;
		break;
		case ROCE_PG_SIZE_1G: pg_size = BNXT_QPLIB_HWRM_PG_SIZE_1G;
		break;
		default:
		break;
	}
	return pg_size;
}

static inline enum bnxt_qplib_hwq_type _get_hwq_type(struct bnxt_qplib_res *res)
{
	return _is_chip_gen_p5_p7(res->cctx) ? HWQ_TYPE_QUEUE : HWQ_TYPE_L2_CMPL;
}

static inline bool _is_ext_stats_supported(u16 dev_cap_flags)
{
	return dev_cap_flags &
		CREQ_QUERY_FUNC_RESP_SB_EXT_STATS;
}

static inline int bnxt_ext_stats_supported(struct bnxt_qplib_chip_ctx *ctx,
					   u16 flags, bool virtfn)
{
	return (_is_ext_stats_supported(flags) &&
		((virtfn && _is_chip_p7(ctx)) || (!virtfn)));
}

static inline bool _is_hw_retx_supported(u16 dev_cap_flags)
{
	return dev_cap_flags &
		(CREQ_QUERY_FUNC_RESP_SB_HW_REQUESTER_RETX_ENABLED |
		 CREQ_QUERY_FUNC_RESP_SB_HW_RESPONDER_RETX_ENABLED);
}

/* Disable HW_RETX */
#define BNXT_RE_HW_RETX(a) _is_hw_retx_supported((a))

static inline bool _is_cqe_v2_supported(u16 dev_cap_flags)
{
	return dev_cap_flags &
		CREQ_QUERY_FUNC_RESP_SB_CQE_V2;
}

#define BNXT_DB_FIFO_ROOM_MASK      0x1fff8000
#define BNXT_DB_FIFO_ROOM_SHIFT     15
#define BNXT_MAX_FIFO_DEPTH         0x2c00

#define BNXT_DB_PACING_ALGO_THRESHOLD	250
#define BNXT_DEFAULT_PACING_PROBABILITY 0xFFFF

#define BNXT_DBR_PACING_WIN_BASE	0x2000
#define BNXT_DBR_PACING_WIN_MAP_OFF	4
#define BNXT_DBR_PACING_WIN_OFF(reg)	(BNXT_DBR_PACING_WIN_BASE +	\

static inline void bnxt_qplib_ring_db32(struct bnxt_qplib_db_info *info,
					bool arm)
{
	u32 key = 0;

	key = info->hwq->cons | (CMPL_DOORBELL_IDX_VALID |
		(CMPL_DOORBELL_KEY_CMPL & CMPL_DOORBELL_KEY_MASK));
	if (!arm)
		key |= CMPL_DOORBELL_MASK;
	/* memory barrier */
	wmb();
	writel(key, info->db);
}

#define BNXT_QPLIB_INIT_DBHDR(xid, type, indx, toggle)			\
	(((u64)(((xid) & DBC_DBC_XID_MASK) | DBC_DBC_PATH_ROCE |	\
	    (type) | BNXT_QPLIB_DBR_VALID) << 32) | (indx) |		\
	    ((toggle) << (BNXT_QPLIB_DBR_TOGGLE_SHIFT)))

static inline void bnxt_qplib_write_db(struct bnxt_qplib_db_info *info,
				       u64 key, void __iomem *db,
				       u64 *shadow_key)
{
	unsigned long flags;

	spin_lock_irqsave(&info->lock, flags);
	*shadow_key = key;
	writeq(key, db);
	spin_unlock_irqrestore(&info->lock, flags);
}

static inline void __replay_writeq(u64 key, void __iomem *db)
{
	/* No need to replay uninitialised shadow_keys */
	if (key != BNXT_QPLIB_DBR_KEY_INVALID)
		writeq(key, db);
}

static inline void bnxt_qplib_replay_db(struct bnxt_qplib_db_info *info,
					bool is_arm_ena)

{
	if (!spin_trylock_irq(&info->lock))
		return;

	if (is_arm_ena)
		__replay_writeq(info->shadow_key_arm_ena, info->priv_db);
	else
		__replay_writeq(info->shadow_key, info->db);

	spin_unlock_irq(&info->lock);
}

static inline void bnxt_qplib_ring_db(struct bnxt_qplib_db_info *info,
				      u32 type)
{
	u64 key = 0;
	u32 indx;
	u8 toggle = 0;

	if (type == DBC_DBC_TYPE_CQ_ARMALL ||
	    type == DBC_DBC_TYPE_CQ_ARMSE)
		toggle = info->toggle;

	indx = ((info->hwq->cons & DBC_DBC_INDEX_MASK) |
		((info->flags & BNXT_QPLIB_FLAG_EPOCH_CONS_MASK) <<
		 BNXT_QPLIB_DB_EPOCH_CONS_SHIFT));

	key = BNXT_QPLIB_INIT_DBHDR(info->xid, type, indx, toggle);
	bnxt_qplib_write_db(info, key, info->db, &info->shadow_key);
}

static inline void bnxt_qplib_ring_prod_db(struct bnxt_qplib_db_info *info,
					   u32 type)
{
	u64 key = 0;
	u32 indx;

	indx = (((info->hwq->prod / info->max_slot) & DBC_DBC_INDEX_MASK) |
		((info->flags & BNXT_QPLIB_FLAG_EPOCH_PROD_MASK) <<
		 BNXT_QPLIB_DB_EPOCH_PROD_SHIFT));
	key = BNXT_QPLIB_INIT_DBHDR(info->xid, type, indx, 0);
	bnxt_qplib_write_db(info, key, info->db, &info->shadow_key);
}

static inline void bnxt_qplib_armen_db(struct bnxt_qplib_db_info *info,
				       u32 type)
{
	u64 key = 0;
	u8 toggle = 0;

	if (type == DBC_DBC_TYPE_CQ_ARMENA)
		toggle = info->toggle;
	/* Index always at 0 */
	key = BNXT_QPLIB_INIT_DBHDR(info->xid, type, 0, toggle);
	bnxt_qplib_write_db(info, key, info->priv_db,
			    &info->shadow_key_arm_ena);
}

static inline void bnxt_qplib_cq_coffack_db(struct bnxt_qplib_db_info *info)
{
	u64 key = 0;

	/* Index always at 0 */
	key = BNXT_QPLIB_INIT_DBHDR(info->xid, DBC_DBC_TYPE_CQ_CUTOFF_ACK, 0, 0);
	bnxt_qplib_write_db(info, key, info->priv_db, &info->shadow_key);
}

static inline void bnxt_qplib_srq_arm_db(struct bnxt_qplib_db_info *info)
{
	u64 key = 0;

	/* Index always at 0 */
	key = BNXT_QPLIB_INIT_DBHDR(info->xid, DBC_DBC_TYPE_SRQ_ARM, 0, 0);
	bnxt_qplib_write_db(info, key, info->priv_db, &info->shadow_key);
}

static inline void bnxt_qplib_ring_nq_db(struct bnxt_qplib_db_info *info,
					 struct bnxt_qplib_chip_ctx *cctx,
					 bool arm)
{
	u32 type;

	type = arm ? DBC_DBC_TYPE_NQ_ARM : DBC_DBC_TYPE_NQ;
	if (_is_chip_gen_p5_p7(cctx))
		bnxt_qplib_ring_db(info, type);
	else
		bnxt_qplib_ring_db32(info, arm);
}

struct bnxt_qplib_max_res {
	u32 max_qp;
	u32 max_mr;
	u32 max_cq;
	u32 max_srq;
	u32 max_ah;
	u32 max_pd;
};

/*
 * Defines for maximum resources supported for chip revisions
 * Maximum PDs supported are restricted to Max QPs
 * GENP4 - Wh+
 * DEFAULT - Thor
 */
#define BNXT_QPLIB_GENP4_PF_MAX_QP	(16 * 1024)
#define BNXT_QPLIB_GENP4_PF_MAX_MRW	(16 * 1024)
#define BNXT_QPLIB_GENP4_PF_MAX_CQ	(16 * 1024)
#define BNXT_QPLIB_GENP4_PF_MAX_SRQ	(1 * 1024)
#define BNXT_QPLIB_GENP4_PF_MAX_AH	(16 * 1024)
#define BNXT_QPLIB_GENP4_PF_MAX_PD	 BNXT_QPLIB_GENP4_PF_MAX_QP

#define BNXT_QPLIB_DEFAULT_PF_MAX_QP	(64 * 1024)
#define BNXT_QPLIB_DEFAULT_PF_MAX_MRW	(256 * 1024)
#define BNXT_QPLIB_DEFAULT_PF_MAX_CQ	(64 * 1024)
#define BNXT_QPLIB_DEFAULT_PF_MAX_SRQ	(4 * 1024)
#define BNXT_QPLIB_DEFAULT_PF_MAX_AH	(64 * 1024)
#define BNXT_QPLIB_DEFAULT_PF_MAX_PD	BNXT_QPLIB_DEFAULT_PF_MAX_QP

#define BNXT_QPLIB_DEFAULT_VF_MAX_QP	(6 * 1024)
#define BNXT_QPLIB_DEFAULT_VF_MAX_MRW	(6 * 1024)
#define BNXT_QPLIB_DEFAULT_VF_MAX_CQ	(6 * 1024)
#define BNXT_QPLIB_DEFAULT_VF_MAX_SRQ	(4 * 1024)
#define BNXT_QPLIB_DEFAULT_VF_MAX_AH	(6 * 1024)
#define BNXT_QPLIB_DEFAULT_VF_MAX_PD	BNXT_QPLIB_DEFAULT_VF_MAX_QP

static inline void bnxt_qplib_max_res_supported(struct bnxt_qplib_chip_ctx *cctx,
						struct bnxt_qplib_res *qpl_res,
						struct bnxt_qplib_max_res *max_res,
						bool vf_res_limit)
{
	switch (cctx->chip_num) {
	case CHIP_NUM_57608:
	case CHIP_NUM_58818:
	case CHIP_NUM_57504:
	case CHIP_NUM_57502:
	case CHIP_NUM_57508:
		if (!qpl_res->is_vf) {
			max_res->max_qp = BNXT_QPLIB_DEFAULT_PF_MAX_QP;
			max_res->max_mr = BNXT_QPLIB_DEFAULT_PF_MAX_MRW;
			max_res->max_cq = BNXT_QPLIB_DEFAULT_PF_MAX_CQ;
			max_res->max_srq = BNXT_QPLIB_DEFAULT_PF_MAX_SRQ;
			max_res->max_ah = BNXT_QPLIB_DEFAULT_PF_MAX_AH;
			max_res->max_pd = BNXT_QPLIB_DEFAULT_PF_MAX_PD;
		} else {
			max_res->max_qp = BNXT_QPLIB_DEFAULT_VF_MAX_QP;
			max_res->max_mr = BNXT_QPLIB_DEFAULT_VF_MAX_MRW;
			max_res->max_cq = BNXT_QPLIB_DEFAULT_VF_MAX_CQ;
			max_res->max_srq = BNXT_QPLIB_DEFAULT_VF_MAX_SRQ;
			max_res->max_ah = BNXT_QPLIB_DEFAULT_VF_MAX_AH;
			max_res->max_pd = BNXT_QPLIB_DEFAULT_VF_MAX_PD;
		}
		break;
	default:
		/* Wh+/Stratus max resources */
		max_res->max_qp = BNXT_QPLIB_GENP4_PF_MAX_QP;
		max_res->max_mr = BNXT_QPLIB_GENP4_PF_MAX_MRW;
		max_res->max_cq = BNXT_QPLIB_GENP4_PF_MAX_CQ;
		max_res->max_srq = BNXT_QPLIB_GENP4_PF_MAX_SRQ;
		max_res->max_ah = BNXT_QPLIB_GENP4_PF_MAX_AH;
		max_res->max_pd = BNXT_QPLIB_GENP4_PF_MAX_PD;
		break;
	}
}
#endif
