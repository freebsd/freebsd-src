/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright © 2021-2023 Dmitry Salychev
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef	_DPAA2_SWP_H
#define	_DPAA2_SWP_H

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>

#include "dpaa2_types.h"
#include "dpaa2_bp.h"

/*
 * DPAA2 QBMan software portal.
 */

/* All QBMan commands and result structures use this "valid bit" encoding. */
#define DPAA2_SWP_VALID_BIT		((uint32_t) 0x80)

#define DPAA2_SWP_TIMEOUT		100000	/* in us */
#define DPAA2_SWP_CMD_PARAMS_N		8u
#define DPAA2_SWP_RSP_PARAMS_N		8u

/*
 * Maximum number of buffers that can be acquired/released through a single
 * QBMan command.
 */
#define DPAA2_SWP_BUFS_PER_CMD		7u

/*
 * Number of times to retry DPIO portal operations while waiting for portal to
 * finish executing current command and become available.
 *
 * We want to avoid being stuck in a while loop in case hardware becomes
 * unresponsive, but not give up too easily if the portal really is busy for
 * valid reasons.
 */
#define DPAA2_SWP_BUSY_RETRIES		1000

/* Versions of the QBMan software portals. */
#define DPAA2_SWP_REV_4000		0x04000000
#define DPAA2_SWP_REV_4100		0x04010000
#define DPAA2_SWP_REV_4101		0x04010001
#define DPAA2_SWP_REV_5000		0x05000000

#define DPAA2_SWP_REV_MASK		0xFFFF0000

/* Registers in the cache-inhibited area of the software portal. */
#define DPAA2_SWP_CINH_CR		0x600 /* Management Command reg.*/
#define DPAA2_SWP_CINH_EQCR_PI		0x800 /* Enqueue Ring, Producer Index */
#define DPAA2_SWP_CINH_EQCR_CI		0x840 /* Enqueue Ring, Consumer Index */
#define DPAA2_SWP_CINH_CR_RT		0x900 /* CR Read Trigger */
#define DPAA2_SWP_CINH_VDQCR_RT		0x940 /* VDQCR Read Trigger */
#define DPAA2_SWP_CINH_EQCR_AM_RT	0x980
#define DPAA2_SWP_CINH_RCR_AM_RT	0x9C0
#define DPAA2_SWP_CINH_DQPI		0xA00 /* DQRR Producer Index reg. */
#define DPAA2_SWP_CINH_DQRR_ITR		0xA80 /* DQRR interrupt timeout reg. */
#define DPAA2_SWP_CINH_DCAP		0xAC0 /* DQRR Consumption Ack. reg. */
#define DPAA2_SWP_CINH_SDQCR		0xB00 /* Static Dequeue Command reg. */
#define DPAA2_SWP_CINH_EQCR_AM_RT2	0xB40
#define DPAA2_SWP_CINH_RCR_PI		0xC00 /* Release Ring, Producer Index */
#define DPAA2_SWP_CINH_RAR		0xCC0 /* Release Array Allocation reg. */
#define DPAA2_SWP_CINH_CFG		0xD00
#define DPAA2_SWP_CINH_ISR		0xE00
#define DPAA2_SWP_CINH_IER		0xE40
#define DPAA2_SWP_CINH_ISDR		0xE80
#define DPAA2_SWP_CINH_IIR		0xEC0
#define DPAA2_SWP_CINH_ITPR		0xF40

/* Registers in the cache-enabled area of the software portal. */
#define DPAA2_SWP_CENA_EQCR(n)		(0x000 + ((uint32_t)(n) << 6))
#define DPAA2_SWP_CENA_DQRR(n)		(0x200 + ((uint32_t)(n) << 6))
#define DPAA2_SWP_CENA_RCR(n)		(0x400 + ((uint32_t)(n) << 6))
#define DPAA2_SWP_CENA_CR		(0x600) /* Management Command reg. */
#define DPAA2_SWP_CENA_RR(vb)		(0x700 + ((uint32_t)(vb) >> 1))
#define DPAA2_SWP_CENA_VDQCR		(0x780)
#define DPAA2_SWP_CENA_EQCR_CI		(0x840)

/* Registers in the cache-enabled area of the software portal (memory-backed). */
#define DPAA2_SWP_CENA_DQRR_MEM(n)	(0x0800 + ((uint32_t)(n) << 6))
#define DPAA2_SWP_CENA_RCR_MEM(n)	(0x1400 + ((uint32_t)(n) << 6))
#define DPAA2_SWP_CENA_CR_MEM		(0x1600) /* Management Command reg. */
#define DPAA2_SWP_CENA_RR_MEM		(0x1680) /* Management Response reg. */
#define DPAA2_SWP_CENA_VDQCR_MEM	(0x1780)
#define DPAA2_SWP_CENA_EQCR_CI_MEMBACK	(0x1840)

/* Shifts in the portal's configuration register. */
#define DPAA2_SWP_CFG_DQRR_MF_SHIFT	20
#define DPAA2_SWP_CFG_EST_SHIFT		16
#define DPAA2_SWP_CFG_CPBS_SHIFT	15
#define DPAA2_SWP_CFG_WN_SHIFT		14
#define DPAA2_SWP_CFG_RPM_SHIFT		12
#define DPAA2_SWP_CFG_DCM_SHIFT		10
#define DPAA2_SWP_CFG_EPM_SHIFT		8
#define DPAA2_SWP_CFG_VPM_SHIFT		7
#define DPAA2_SWP_CFG_CPM_SHIFT		6
#define DPAA2_SWP_CFG_SD_SHIFT		5
#define DPAA2_SWP_CFG_SP_SHIFT		4
#define DPAA2_SWP_CFG_SE_SHIFT		3
#define DPAA2_SWP_CFG_DP_SHIFT		2
#define DPAA2_SWP_CFG_DE_SHIFT		1
#define DPAA2_SWP_CFG_EP_SHIFT		0

/* Static Dequeue Command Register attribute codes */
#define DPAA2_SDQCR_FC_SHIFT		29 /* Dequeue Command Frame Count */
#define DPAA2_SDQCR_FC_MASK		0x1
#define DPAA2_SDQCR_DCT_SHIFT		24 /* Dequeue Command Type */
#define DPAA2_SDQCR_DCT_MASK		0x3
#define DPAA2_SDQCR_TOK_SHIFT		16 /* Dequeue Command Token */
#define DPAA2_SDQCR_TOK_MASK		0xff
#define DPAA2_SDQCR_SRC_SHIFT		0  /* Dequeue Source */
#define DPAA2_SDQCR_SRC_MASK		0xffff

/*
 * Read trigger bit is used to trigger QMan to read a command from memory,
 * without having software perform a cache flush to force a write of the command
 * to QMan.
 *
 * NOTE: Implemented in QBMan 5.0 or above.
 */
#define DPAA2_SWP_RT_MODE		((uint32_t)0x100)

/* Interrupt Enable Register bits. */
#define DPAA2_SWP_INTR_EQRI		0x01
#define DPAA2_SWP_INTR_EQDI		0x02
#define DPAA2_SWP_INTR_DQRI		0x04
#define DPAA2_SWP_INTR_RCRI		0x08
#define DPAA2_SWP_INTR_RCDI		0x10
#define DPAA2_SWP_INTR_VDCI		0x20

/* "Write Enable" bitmask for a command to configure SWP WQ Channel.*/
#define DPAA2_WQCHAN_WE_EN		(0x1u) /* Enable CDAN generation */
#define DPAA2_WQCHAN_WE_ICD		(0x2u) /* Interrupt Coalescing Disable */
#define DPAA2_WQCHAN_WE_CTX		(0x4u)

/* Definitions for parsing DQRR entries. */
#define DPAA2_DQRR_RESULT_MASK		(0x7Fu)
#define DPAA2_DQRR_RESULT_DQ		(0x60u)
#define DPAA2_DQRR_RESULT_FQRN		(0x21u)
#define DPAA2_DQRR_RESULT_FQRNI		(0x22u)
#define DPAA2_DQRR_RESULT_FQPN		(0x24u)
#define DPAA2_DQRR_RESULT_FQDAN		(0x25u)
#define DPAA2_DQRR_RESULT_CDAN		(0x26u)
#define DPAA2_DQRR_RESULT_CSCN_MEM	(0x27u)
#define DPAA2_DQRR_RESULT_CGCU		(0x28u)
#define DPAA2_DQRR_RESULT_BPSCN		(0x29u)
#define DPAA2_DQRR_RESULT_CSCN_WQ	(0x2au)

/* Frame dequeue statuses */
#define DPAA2_DQ_STAT_FQEMPTY		(0x80u) /* FQ is empty */
#define DPAA2_DQ_STAT_HELDACTIVE	(0x40u) /* FQ is held active */
#define DPAA2_DQ_STAT_FORCEELIGIBLE	(0x20u) /* FQ force eligible */
#define DPAA2_DQ_STAT_VALIDFRAME	(0x10u) /* valid frame */
#define DPAA2_DQ_STAT_ODPVALID		(0x04u) /* FQ ODP enable */
#define DPAA2_DQ_STAT_VOLATILE		(0x02u) /* volatile dequeue (VDC) */
#define DPAA2_DQ_STAT_EXPIRED		(0x01u) /* VDC is expired */

/*
 * Portal flags.
 *
 * TODO: Use the same flags for both MC and software portals.
 */
#define DPAA2_SWP_DEF			0x0u
#define DPAA2_SWP_NOWAIT_ALLOC		0x2u	/* Do not sleep during init */
#define DPAA2_SWP_LOCKED		0x4000u	/* Wait till portal's unlocked */
#define DPAA2_SWP_DESTROYED		0x8000u /* Terminate any operations */

/* Command return codes. */
#define DPAA2_SWP_STAT_OK		0x0
#define DPAA2_SWP_STAT_NO_MEMORY	0x9	/* No memory available */
#define DPAA2_SWP_STAT_PORTAL_DISABLED	0xFD	/* QBMan portal disabled */
#define DPAA2_SWP_STAT_EINVAL		0xFE	/* Invalid argument */
#define DPAA2_SWP_STAT_ERR		0xFF	/* General error */

#define DPAA2_EQ_DESC_SIZE		32u	/* Enqueue Command Descriptor */
#define DPAA2_FDR_DESC_SIZE		32u	/* Descriptor of the FDR */
#define DPAA2_FD_SIZE			32u	/* Frame Descriptor */
#define DPAA2_FDR_SIZE			64u	/* Frame Dequeue Response */
#define DPAA2_SCN_SIZE			16u	/* State Change Notification */
#define DPAA2_FA_SIZE			64u	/* SW Frame Annotation */
#define DPAA2_SGE_SIZE			16u	/* S/G table entry */
#define DPAA2_DQ_SIZE			64u	/* Dequeue Response */
#define DPAA2_SWP_CMD_SIZE		64u	/* SWP Command */
#define DPAA2_SWP_RSP_SIZE		64u	/* SWP Command Response */

/* Opaque token for static dequeues. */
#define DPAA2_SWP_SDQCR_TOKEN		0xBBu
/* Opaque token for static dequeues. */
#define DPAA2_SWP_VDQCR_TOKEN		0xCCu

#define DPAA2_SWP_LOCK(__swp, __flags) do {		\
	mtx_assert(&(__swp)->lock, MA_NOTOWNED);	\
	mtx_lock(&(__swp)->lock);			\
	*(__flags) = (__swp)->flags;			\
	(__swp)->flags |= DPAA2_SWP_LOCKED;		\
} while (0)

#define DPAA2_SWP_UNLOCK(__swp) do {		\
	mtx_assert(&(__swp)->lock, MA_OWNED);	\
	(__swp)->flags &= ~DPAA2_SWP_LOCKED;	\
	mtx_unlock(&(__swp)->lock);		\
} while (0)

enum dpaa2_fd_format {
	DPAA2_FD_SINGLE = 0,
	DPAA2_FD_LIST,
	DPAA2_FD_SG
};

/**
 * @brief Enqueue command descriptor.
 */
struct dpaa2_eq_desc {
	uint8_t		verb;
	uint8_t		dca;
	uint16_t	seqnum;
	uint16_t	orpid;
	uint16_t	_reserved;
	uint32_t	tgtid;
	uint32_t	tag;
	uint16_t	qdbin;
	uint8_t		qpri;
	uint8_t		_reserved1[3];
	uint8_t		wae;
	uint8_t		rspid;
	uint64_t	rsp_addr;
} __packed;
CTASSERT(sizeof(struct dpaa2_eq_desc) == DPAA2_EQ_DESC_SIZE);

/**
 * @brief Frame Dequeue Response (FDR) descriptor.
 */
struct dpaa2_fdr_desc {
	uint8_t		verb;
	uint8_t		stat;
	uint16_t	seqnum;
	uint16_t	oprid;
	uint8_t		_reserved;
	uint8_t		tok;
	uint32_t	fqid;
	uint32_t	_reserved1;
	uint32_t	fq_byte_cnt;
	uint32_t	fq_frm_cnt;
	uint64_t	fqd_ctx;
} __packed;
CTASSERT(sizeof(struct dpaa2_fdr_desc) == DPAA2_FDR_DESC_SIZE);

/**
 * @brief State Change Notification Message (SCNM).
 */
struct dpaa2_scn {
	uint8_t		verb;
	uint8_t		stat;
	uint8_t		state;
	uint8_t		_reserved;
	uint32_t	rid_tok;
	uint64_t	ctx;
} __packed;
CTASSERT(sizeof(struct dpaa2_scn) == DPAA2_SCN_SIZE);

/**
 * @brief DPAA2 frame descriptor.
 *
 * addr:		Memory address of the start of the buffer holding the
 *			frame data or the buffer containing the scatter/gather
 *			list.
 * data_length:		Length of the frame data (in bytes).
 * bpid_ivp_bmt:	Buffer pool ID (14 bit + BMT bit + IVP bit)
 * offset_fmt_sl:	Frame data offset, frame format and short-length fields.
 * frame_ctx:		Frame context. This field allows the sender of a frame
 *			to communicate some out-of-band information to the
 *			receiver of the frame.
 * ctrl:		Control bits (ERR, CBMT, ASAL, PTAC, DROPP, SC, DD).
 * flow_ctx:		Frame flow context. Associates the frame with a flow
 *			structure. QMan may use the FLC field for 3 purposes:
 *			stashing control, order definition point identification,
 *			and enqueue replication control.
 */
struct dpaa2_fd {
	uint64_t	addr;
	uint32_t	data_length;
	uint16_t	bpid_ivp_bmt;
	uint16_t	offset_fmt_sl;
	uint32_t	frame_ctx;
	uint32_t	ctrl;
	uint64_t	flow_ctx;
} __packed;
CTASSERT(sizeof(struct dpaa2_fd) == DPAA2_FD_SIZE);

/**
 * @brief DPAA2 frame annotation.
 */
struct dpaa2_fa {
	uint32_t		 magic;
	struct dpaa2_buf	*buf;
	union {
		struct { /* Tx frame annotation */
			struct dpaa2_ni_tx_ring *tx;
		};
#ifdef __notyet__
		struct { /* Rx frame annotation */
			uint64_t		 _notused;
		};
#endif
	};
} __packed;
CTASSERT(sizeof(struct dpaa2_fa) <= DPAA2_FA_SIZE);

/**
 * @brief DPAA2 scatter/gather entry.
 */
struct dpaa2_sg_entry {
	uint64_t	addr;
	uint32_t	len;
	uint16_t	bpid;
	uint16_t	offset_fmt;
} __packed;
CTASSERT(sizeof(struct dpaa2_sg_entry) == DPAA2_SGE_SIZE);

/**
 * @brief Frame Dequeue Response (FDR).
 */
struct dpaa2_fdr {
	struct dpaa2_fdr_desc	 desc;
	struct dpaa2_fd		 fd;
} __packed;
CTASSERT(sizeof(struct dpaa2_fdr) == DPAA2_FDR_SIZE);

/**
 * @brief Dequeue Response Message.
 */
struct dpaa2_dq {
	union {
		struct {
			uint8_t	 verb;
			uint8_t	 _reserved[63];
		} common;
		struct dpaa2_fdr fdr; /* Frame Dequeue Response */
		struct dpaa2_scn scn; /* State Change Notification */
	};
} __packed;
CTASSERT(sizeof(struct dpaa2_dq) == DPAA2_DQ_SIZE);

/**
 * @brief Descriptor of the QBMan software portal.
 *
 * cena_res:		Unmapped cache-enabled part of the portal's I/O memory.
 * cena_map:		Mapped cache-enabled part of the portal's I/O memory.
 * cinh_res:		Unmapped cache-inhibited part of the portal's I/O memory.
 * cinh_map:		Mapped cache-inhibited part of the portal's I/O memory.
 *
 * dpio_dev:		Device associated with the DPIO object to manage this
 *			portal.
 * swp_version:		Hardware IP version of the software portal.
 * swp_clk:		QBMAN clock frequency value in Hz.
 * swp_cycles_ratio:	How many 256 QBMAN cycles fit into one ns.
 * swp_id:		Software portal ID.
 *
 * has_notif:		True if the notification mode is used.
 * has_8prio:		True for a channel with 8 priority WQs. Ignored unless
 *			"has_notif" is true.
 */
struct dpaa2_swp_desc {
	struct resource		*cena_res;
	struct resource_map	*cena_map;
	struct resource		*cinh_res;
	struct resource_map	*cinh_map;

	device_t		 dpio_dev;
	uint32_t		 swp_version;
	uint32_t		 swp_clk;
	uint32_t		 swp_cycles_ratio;
	uint16_t		 swp_id;

	bool			 has_notif;
	bool			 has_8prio;
};

/**
 * @brief Command holds data to be written to the software portal.
 */
struct dpaa2_swp_cmd {
	uint64_t	params[DPAA2_SWP_CMD_PARAMS_N];
};
CTASSERT(sizeof(struct dpaa2_swp_cmd) == DPAA2_SWP_CMD_SIZE);

/**
 * @brief Command response holds data received from the software portal.
 */
struct dpaa2_swp_rsp {
	uint64_t	params[DPAA2_SWP_RSP_PARAMS_N];
};
CTASSERT(sizeof(struct dpaa2_swp_rsp) == DPAA2_SWP_RSP_SIZE);

/**
 * @brief QBMan software portal.
 *
 * res:		Unmapped cache-enabled and cache-inhibited parts of the portal.
 * map:		Mapped cache-enabled and cache-inhibited parts of the portal.
 * desc:	Descriptor of the QBMan software portal.
 * lock:	Lock to guard an access to the portal.
 * cv:		Conditional variable helps to wait for the helper object's state
 *		change.
 * flags:	Current state of the object.
 * sdq:		Push dequeues status.
 * mc:		Management commands data.
 * mr:		Management response data.
 * dqrr:	Dequeue Response Ring is used to issue frame dequeue responses
 * 		from the QBMan to the driver.
 * eqcr:	Enqueue Command Ring is used to issue frame enqueue commands
 *		from the driver to the QBMan.
 */
struct dpaa2_swp {
	struct resource		*cena_res;
	struct resource_map	*cena_map;
	struct resource		*cinh_res;
	struct resource_map	*cinh_map;

	struct mtx		 lock;
	struct dpaa2_swp_desc	*desc;
	uint16_t		 flags;

	/* Static Dequeue Command Register value (to obtain CDANs). */
	uint32_t		 sdq;

	/* Volatile Dequeue Command (to obtain frames). */
	struct {
		uint32_t	 valid_bit; /* 0x00 or 0x80 */
	} vdq;

	struct {
		bool		 atomic;
		bool		 writes_cinh;
		bool		 mem_backed;
	} cfg; /* Software portal configuration. */

	struct {
		uint32_t	 valid_bit; /* 0x00 or 0x80 */
	} mc;

	struct {
		uint32_t	 valid_bit; /* 0x00 or 0x80 */
	} mr;

	struct {
		uint32_t	 next_idx;
		uint32_t	 valid_bit;
		uint8_t		 ring_size;
		bool		 reset_bug; /* dqrr reset workaround */
		uint32_t	 irq_threshold;
		uint32_t	 irq_itp;
	} dqrr;

	struct {
		uint32_t	 pi; /* producer index */
		uint32_t	 pi_vb; /* PI valid bits */
		uint32_t	 pi_ring_size;
		uint32_t	 pi_ci_mask;
		uint32_t	 ci;
		int		 available;
		uint32_t	 pend;
		uint32_t	 no_pfdr;
	} eqcr;
};

/* Management routines. */
int dpaa2_swp_init_portal(struct dpaa2_swp **swp, struct dpaa2_swp_desc *desc,
    uint16_t flags);
void dpaa2_swp_free_portal(struct dpaa2_swp *swp);
uint32_t dpaa2_swp_set_cfg(uint8_t max_fill, uint8_t wn, uint8_t est,
    uint8_t rpm, uint8_t dcm, uint8_t epm, int sd, int sp, int se, int dp,
    int de, int ep);

/* Read/write registers of a software portal. */
void dpaa2_swp_write_reg(struct dpaa2_swp *swp, uint32_t o, uint32_t v);
uint32_t dpaa2_swp_read_reg(struct dpaa2_swp *swp, uint32_t o);

/* Helper routines. */
void dpaa2_swp_set_ed_norp(struct dpaa2_eq_desc *ed, bool resp_always);
void dpaa2_swp_set_ed_fq(struct dpaa2_eq_desc *ed, uint32_t fqid);
void dpaa2_swp_set_intr_trigger(struct dpaa2_swp *swp, uint32_t mask);
uint32_t dpaa2_swp_get_intr_trigger(struct dpaa2_swp *swp);
uint32_t dpaa2_swp_read_intr_status(struct dpaa2_swp *swp);
void dpaa2_swp_clear_intr_status(struct dpaa2_swp *swp, uint32_t mask);
void dpaa2_swp_set_push_dequeue(struct dpaa2_swp *swp, uint8_t chan_idx,
    bool en);
int dpaa2_swp_set_irq_coalescing(struct dpaa2_swp *swp, uint32_t threshold,
    uint32_t holdoff);

/* Software portal commands. */
int dpaa2_swp_conf_wq_channel(struct dpaa2_swp *swp, uint16_t chan_id,
    uint8_t we_mask, bool cdan_en, uint64_t ctx);
int dpaa2_swp_query_bp(struct dpaa2_swp *swp, uint16_t bpid,
    struct dpaa2_bp_conf *conf);
int dpaa2_swp_release_bufs(struct dpaa2_swp *swp, uint16_t bpid, bus_addr_t *buf,
    uint32_t buf_num);
int dpaa2_swp_dqrr_next_locked(struct dpaa2_swp *swp, struct dpaa2_dq *dq,
    uint32_t *idx);
int dpaa2_swp_pull(struct dpaa2_swp *swp, uint16_t chan_id,
    struct dpaa2_buf *buf, uint32_t frames_n);
int dpaa2_swp_enq(struct dpaa2_swp *swp, struct dpaa2_eq_desc *ed,
    struct dpaa2_fd *fd);
int dpaa2_swp_enq_mult(struct dpaa2_swp *swp, struct dpaa2_eq_desc *ed,
    struct dpaa2_fd *fd, uint32_t *flags, int frames_n);

#endif /* _DPAA2_SWP_H */
