/*-
 * Copyright (c) 2011-2012 Semihalf.
 * All rights reserved.
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

#ifndef _QMAN_H
#define _QMAN_H

#include <sys/vmem.h>
#include <machine/vmparam.h>

struct qman_fq;
struct qman_fq;
struct dpaa_fd;
struct qman_portal;

/**
 * @group QMan private defines/declarations
 * @{
 */
/**
 * Maximum number of frame queues in all QMans.
 */
#define		QMAN_MAX_FQIDS			16

/**
 * Pool channel common to all software portals.
 * @note Value of 0 reflects the e_QM_FQ_CHANNEL_POOL1 from e_QmFQChannel
 *       type used in qman_fq_create().
 */
#define		QMAN_COMMON_POOL_CHANNEL	0

#define		QMAN_FQID_BASE			1

/* Counters */
#define	QMAN_COUNTER_FRAME	0
#define	QMAN_COUNTER_BYTES	1

/*
 * Portal defines
 */
#define QMAN_CE_PA(base)	(base)
#define QMAN_CI_PA(base)	((base) + 0x100000)

#define QMAN_PORTAL_CE_PA(base, n)	\
    (QMAN_CE_PA(base) + ((n) * QMAN_PORTAL_CE_SIZE))
#define QMAN_PORTAL_CI_PA(base, n)	\
    (QMAN_CI_PA(base) + ((n) * QMAN_PORTAL_CI_SIZE))

struct qman_softc {
	device_t	sc_dev;			/* device handle */
	int		sc_rrid;		/* register rid */
	struct resource	*sc_rres;		/* register resource */
	int		sc_irid;		/* interrupt rid */
	struct resource	*sc_ires;		/* interrupt resource */
	vmem_t		*sc_fqalloc;
	vmem_t		*sc_qpalloc;
	vmem_t		*sc_cgalloc;
	void		*sc_intr_cookie;
	int		sc_qman_base_channel;
	int		sc_qman_major;

	vm_paddr_t	sc_qp_pa;		/* QMAN portal PA */

	int		sc_fq_cpu[QMAN_MAX_FQIDS];
};

struct qman_fd {
	uint64_t dd:2;
	uint64_t liodn_off:6;
	uint64_t bpid:8;
	uint64_t eliodn_off:4;
	uint64_t _rsvd0:4;
	uint64_t addr:40;
	union {
		struct {
			uint32_t format:3;
			uint32_t offset:9;
			uint32_t length:20;
		};
		struct {
			uint32_t format2:3;
			uint32_t wlength:29;
		};
	};
	uint32_t cmd_stat;
};

_Static_assert(sizeof(struct qman_fd) == 16, "qman_fd size mismatch");

struct qman_dqrr_entry {
	uint8_t verb;
	uint8_t stat;
	uint16_t seqnum;
	uint8_t tok;
	uint8_t _rsvd0[3];
	uint32_t fqid;
	uint32_t ctxb;
	struct qman_fd fd;
	uint8_t _rsvd1[32];
};

/* Bits for qman_dqrr_entry fields */
#define	QMAN_DQRR_STAT_FQ_EMPTY		0x80
#define	QMAN_DQRR_STAT_FQ_HELD_ACTIVE	0x40
#define	QMAN_DQRR_STAT_FQ_FORCED	0x20
#define	QMAN_DQRR_STAT_HAS_FRAME	0x10
#define	QMAN_DQRR_STAT_VDQCR		0x02
#define	QMAN_DQRR_STAT_EXPIRED		0x01

struct qman_mr_entry {
	union {
		struct {
			uint8_t verb;
			uint8_t data[63];
		};
		struct {
			uint8_t verb;
			uint8_t dca;
			uint16_t seqnum;
			uint32_t rc:8;
			uint32_t orp:24;
			uint32_t fqid;
			uint32_t tag;
			struct qman_fd fd;
			uint8_t _rsvd[32];
		} ern;
		struct {
			uint8_t verb;
			uint8_t fqs;
			uint8_t _rsvd0[6];
			uint32_t fqid;
			uint32_t ctxb;
			uint8_t _rsvd1[48];
		} fqscn;
	};
};

_Static_assert(sizeof(struct qman_mr_entry) == 64, "bad sizeof qman_mr");
/** @> */

typedef int (*qman_cb_dqrr)(device_t, struct qman_fq *,
    struct qman_fd *, void *);
typedef void (*qman_cb_mr)(device_t, struct qman_fq *,
    struct qman_mr_entry *);

struct qman_cb {
	qman_cb_dqrr dqrr;
	qman_cb_mr ern;
	qman_cb_mr fqscn;
	void *ctx;
};
/**
 * @group QMan bus interface
 * @{
 */
int qman_attach(device_t dev);
int qman_detach(device_t dev);
int qman_suspend(device_t dev);
int qman_resume(device_t dev);
int qman_shutdown(device_t dev);
/** @> */
int qman_create_affine_portal(device_t, vm_offset_t, vm_offset_t, int);
void qman_set_sdest(uint16_t, int);


/**
 * @group QMan API
 * @{
 */

/**
 * Create Frame Queue Range.
 *
 * @param fqids_num			Number of frame queues in the range.
 *
 * @param channel			Dedicated channel serviced by this
 * 					Frame Queue Range.
 *
 * @param wq				Work Queue Number within the channel.
 *
 * @param force_fqid			If TRUE, fore allocation of specific
 * 					FQID. Notice that there can not be two
 * 					frame queues with the same ID in the
 * 					system.
 *
 * @param fqid_or_align			FQID if @force_fqid == TRUE, alignment
 * 					of FQIDs entries otherwise.
 *
 * @param init_parked			If TRUE, FQ state is initialized to
 * 					"parked" state on creation. Otherwise,
 * 					to "scheduled" state.
 *
 * @param hold_active			If TRUE, the FQ may be held in the
 * 					portal in "held active" state in
 * 					anticipation of more frames being
 * 					dequeued from it after the head frame
 * 					is removed from the FQ and the dequeue
 * 					response is returned. If FALSE the
 * 					"held_active" state of the FQ is not
 * 					allowed. This affects only on queues
 * 					destined to software portals. Refer to
 * 					the 6.3.4.6 of DPAA Reference Manual.
 *
 * @param prefer_in_cache		If TRUE, prefer this FQR to be in QMan
 * 					internal cache memory for all states.
 *
 * @param congst_avoid_ena		If TRUE, enable congestion avoidance
 * 					mechanism.
 *
 * @param congst_group			A handle to the congestion group. Only
 * 					relevant when @congst_avoid_ena == TRUE.
 *
 * @param overhead_accounting_len	For each frame add this number for CG
 * 					calculation (may be negative), if 0 -
 * 					disable feature.
 *
 * @param tail_drop_threshold		If not 0 - enable tail drop on this
 * 					FQR.
 *
 * @return				A handle to newly created FQR object.
 */
struct qman_fq *qman_fq_create(uint32_t fqids_num, int channel,
    uint8_t wq, bool force_fqid, uint32_t fqid_or_align, bool init_parked,
    bool hold_active, bool prefer_in_cache, bool congst_avoid_ena,
    void *congst_group, int8_t overhead_accounting_len,
    uint32_t tail_drop_threshold);

/**
 * Free Frame Queue Range.
 *
 * @param fq	A handle to FQR to be freed.
 * @return	E_OK on success; error code otherwise.
 */
int qman_fq_free(struct qman_fq *fq);

/**
 * Register the callback function.
 * The callback function will be called when a frame comes from this FQR.
 *
 * @param fq		A handle to FQR.
 * @param callback	A pointer to the callback function.
 * @param app		A pointer to the user's data.
 * @return		E_OK on success; error code otherwise.
 */
int	qman_fq_register_cb(struct qman_fq *fq, qman_cb_dqrr callback,
    void *ctx);

/**
 * Enqueue a frame on a given FQ.
 *
 * @param fq		A handle to FQ.
 * @param frame		A frame to be enqueued to the transmission.
 * @return		E_OK on success; error code otherwise.
 */
int qman_fq_enqueue(struct qman_fq *fq, struct dpaa_fd *frame);

/**
 * Get one of the FQ counter's value.
 *
 * @param fq		A handle to FQ.
 * @param counter	The requested counter.
 * @return		Counter's current value.
 */
uint32_t qman_fq_get_counter(struct qman_fq *fq, int counter);

/**
 * Pull frame from FQ.
 *
 * @param fq		A handle to FQ.
 * @param frame		The received frame.
 * @return		E_OK on success; error code otherwise.
 */
int qman_fq_pull_frame(struct qman_fq *fq, struct dpaa_fd *frame);

/**
 * Get FQID of the FQ.
 * @param fq	A handle to FQ.
 * @return	FQID of the FQ.
 */
uint32_t qman_fq_get_fqid(struct qman_fq *fq);

/*
 * Allocate a QMan channel to be used with an FQ.
 * @return	Channel ID
 */
int qman_alloc_channel(void);

/*
 * Free a channel
 * @param chan	Channel ID returned from qman_alloc_channel().
 */
void qman_free_channel(int);

/**
 * Poll frames from QMan.
 * This polls frames from the current software portal.
 *
 * @param source	Type of frames to be polled.
 * @return		E_OK on success; error otherwise.
 */
int qman_poll(int source);

/**
 * General received frame callback.
 * This is called, when user did not register his own callback for a given
 * frame queue range (fq).
 */
int qman_received_frame_callback(void *ctx, struct qman_fq *fq,
    void *qm_portal, uint32_t fqid_offset, struct dpaa_fd *frame);

/**
 * General rejected frame callback.
 * This is called, when user did not register his own callback for a given
 * frame queue range (fq).
 */
int qman_rejected_frame_callback(void *ctx, struct qman_fq *fq,
    void *qm_portal, uint32_t fqid_offset, struct dpaa_fd *frame,
    void *qm_rejected_frame_info);

/** @} */

#endif /* QMAN_H */
