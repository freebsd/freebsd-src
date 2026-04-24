/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Justin Hibbits
 */

#ifndef	QMAN_VAR_H
#define	QMAN_VAR_H

#include "dpaa_common.h"
#include "portals.h"

struct qman_eqcr_entry {
	uint8_t verb;
	uint8_t dca;
	uint16_t seqnum;
	uint32_t orp;
	uint32_t fqid;
	uint32_t tag;
	struct dpaa_fd fd;
	uint8_t _rsvd[32];
};
_Static_assert(sizeof(struct qman_eqcr_entry) == 64, "EQCR entry mis-sized");
DPAA_RING_DECLARE(qman_eqcr);
DPAA_RING_DECLARE(qman_dqrr);
DPAA_RING_DECLARE(qman_mr);

union qman_mc_command {
	struct {
		uint8_t	verb;
		uint8_t data[63];
	} common;
	struct {
		uint8_t verb;
		uint8_t _rsvd0;
		uint16_t we_mask;
		uint32_t fqid;	/* Only bottom 24 bits allowed */
		uint16_t count;
		uint8_t orpc;
		uint8_t cgid;
		uint16_t fq_ctrl;
		uint16_t dest_chan:13;
		uint16_t dest_wq:3;
		uint16_t ics_cred;
		uint16_t td_thresh_oac;
		uint32_t context_b;
		uint32_t context_a;
		uint8_t _rsvd1[32];
	} init_fq;
	struct {
		uint8_t verb;
		uint8_t _rsvd0[3];
		uint32_t fqid; /* Only bottom 24 bits used */
		uint8_t _rsvd1[56];
	} query_fq;
	struct {
		uint8_t verb;
		uint8_t _rsvd0[3];
		uint32_t fqid;
		uint8_t _rsvd1[56];
	} query_fq_np;
	struct {
		uint8_t verb;
		uint8_t _rsvd0[3];
		uint32_t fqid;
		uint8_t _rsvd1;
		uint8_t count;
		uint8_t _rsvd2[10];
		uint32_t context_b;
		uint8_t _rsvd3[40];
	} alter_fqs;
};

union qman_mc_result {
	struct {
		uint8_t verb;
		uint8_t data[63];
	} common;
	struct {
		uint8_t verb;
		uint8_t rslt;
		uint8_t _rsvd[62];
	} init_fq;
	struct {
		uint8_t verb;
		uint8_t rslt;
		uint8_t _rsvd0[8];
		uint8_t orpc;
		uint8_t cgid;
		uint16_t fq_ctrl;
		uint16_t dest_wq;
		uint16_t ics_cred;
		uint16_t td_thresh;
		uint32_t context_b;
		uint32_t context_a;
		uint16_t oac;
		uint8_t _rsvd1[30];
	} query_fq;
	struct {
		uint8_t verb;
		uint8_t rslt;
		uint8_t _rsvd0;
		uint8_t state;
		uint32_t fqd_link;
		uint16_t odp_seq;
		uint16_t orp_nesn;
		uint16_t orp_ea_hseq;
		uint16_t orp_ea_tseq;
		uint32_t orp_ea_hptr;
		uint32_t orp_ea_tptr;
		uint32_t pfdr_hptr;
		uint32_t pfdr_tptr;
		uint8_t _rsvd1[5];
		uint8_t is;
		uint16_t ics_surp;
		uint32_t byte_cnt;
		uint32_t frm_cnt;
		uint32_t _rsvd2;
		uint16_t ra1_sfdr;
		uint16_t ra2_sfdr;
		uint16_t _rsvd3;
		uint16_t od1_sfdr;
		uint16_t od2_sfdr;
		uint16_t od3_sfdr;
	} query_fq_np;
	struct {
		uint8_t verb;
		uint8_t rslt;
		uint8_t fqs;
		uint8_t _rsvd[61];
	} alter_fqs;
};

struct qman_mc {
	uint8_t polarity;
	bool busy;
};

struct qman_fq {
	uint32_t fqid;
	struct qman_cb cb;
};

struct qman_portal_softc {
	struct dpaa_portal_softc sc_base;

	/* Rings (Enqueue, Dequeue, Message */
	struct qman_eqcr_ring sc_eqcr;
	struct qman_dqrr_ring sc_dqrr;
	struct qman_mr_ring sc_mr;
	struct qman_mc sc_mc;

	int sc_affine_channel;
};

struct qman_fq *qman_fq_from_index(uint32_t fqid);

union qman_mc_result *
qman_portal_mc_send_raw(device_t, union qman_mc_command *);
int qman_portal_fq_enqueue(device_t, struct qman_fq *, struct dpaa_fd *);
void qman_portal_static_dequeue_channel(device_t, int);
void qman_portal_static_dequeue_rm_channel(device_t dev, int channel);

extern int qman_channel_base;
DPCPU_DECLARE(device_t, qman_affine_portal);

#endif	/* QMAN_VAR_H */
