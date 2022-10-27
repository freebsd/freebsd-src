/*-
 * Copyright (c) 2016 Microsoft Corp.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _VMBUS_REG_H_
#define _VMBUS_REG_H_

#include <sys/param.h>
#include <dev/hyperv/include/hyperv.h> /* XXX for hyperv_guid */
#include <dev/hyperv/include/vmbus.h>
#if defined(__aarch64__)
#include <dev/hyperv/vmbus/aarch64/hyperv_reg.h>
#else
#include <dev/hyperv/vmbus/x86/hyperv_reg.h>
#endif
#include <dev/hyperv/vmbus/hyperv_common_reg.h>

/*
 * Hyper-V SynIC message format.
 */

#define VMBUS_MSG_DSIZE_MAX		240
#define VMBUS_MSG_SIZE			256

struct vmbus_message {
	uint32_t	msg_type;	/* HYPERV_MSGTYPE_ */
	uint8_t		msg_dsize;	/* data size */
	uint8_t		msg_flags;	/* VMBUS_MSGFLAG_ */
	uint16_t	msg_rsvd;
	uint64_t	msg_id;
	uint8_t		msg_data[VMBUS_MSG_DSIZE_MAX];
} __packed;
CTASSERT(sizeof(struct vmbus_message) == VMBUS_MSG_SIZE);

#define VMBUS_MSGFLAG_PENDING		0x01

/*
 * Hyper-V SynIC event flags
 */

#ifdef __LP64__
#define VMBUS_EVTFLAGS_MAX	32
#define VMBUS_EVTFLAG_SHIFT	6
#else
#define VMBUS_EVTFLAGS_MAX	64
#define VMBUS_EVTFLAG_SHIFT	5
#endif
#define VMBUS_EVTFLAG_LEN	(1 << VMBUS_EVTFLAG_SHIFT)
#define VMBUS_EVTFLAG_MASK	(VMBUS_EVTFLAG_LEN - 1)
#define VMBUS_EVTFLAGS_SIZE	256

struct vmbus_evtflags {
	u_long		evt_flags[VMBUS_EVTFLAGS_MAX];
} __packed;
CTASSERT(sizeof(struct vmbus_evtflags) == VMBUS_EVTFLAGS_SIZE);

/*
 * Hyper-V Monitor Notification Facility
 */

struct vmbus_mon_trig {
	uint32_t	mt_pending;
	uint32_t	mt_armed;
} __packed;

#define VMBUS_MONTRIGS_MAX	4
#define VMBUS_MONTRIG_LEN	32

struct vmbus_mnf {
	uint32_t	mnf_state;
	uint32_t	mnf_rsvd1;

	struct vmbus_mon_trig mnf_trigs[VMBUS_MONTRIGS_MAX];
	uint8_t		mnf_rsvd2[536];

	uint16_t	mnf_lat[VMBUS_MONTRIGS_MAX][VMBUS_MONTRIG_LEN];
	uint8_t		mnf_rsvd3[256];

	struct hyperv_mon_param
			mnf_param[VMBUS_MONTRIGS_MAX][VMBUS_MONTRIG_LEN];
	uint8_t		mnf_rsvd4[1984];
} __packed;
CTASSERT(sizeof(struct vmbus_mnf) == PAGE_SIZE);

/*
 * Buffer ring
 */
struct vmbus_bufring {
	/*
	 * If br_windex == br_rindex, this bufring is empty; this
	 * means we can _not_ write data to the bufring, if the
	 * write is going to make br_windex same as br_rindex.
	 */
	volatile uint32_t	br_windex;
	volatile uint32_t	br_rindex;

	/*
	 * Interrupt mask {0,1}
	 *
	 * For TX bufring, host set this to 1, when it is processing
	 * the TX bufring, so that we can safely skip the TX event
	 * notification to host.
	 *
	 * For RX bufring, once this is set to 1 by us, host will not
	 * further dispatch interrupts to us, even if there are data
	 * pending on the RX bufring.  This effectively disables the
	 * interrupt of the channel to which this RX bufring is attached.
	 */
	volatile uint32_t	br_imask;

	/*
	 * WS2012/Win8 and later versions of Hyper-V implement interrupt
	 * driven flow management. The feature bit feat_pending_snd_sz
	 * is set by the host on the host->guest buffer ring, and by the
	 * guest on the guest->host buffer ring.
	 *
	 * The meaning of the feature bit is a bit complex in that it has
	 * semantics that apply to both buffer rings.  If the guest sets
	 * the feature bit in the guest->host buffer ring, the guest is
	 * telling the host that:
	 * 1) It will set the br_pending_snd_sz field in the guest->host buffer
	 *    ring when it is waiting for space to become available, and
	 * 2) It will read the pending_send_sz field in the host->guest
	 *    ring buffer and interrupt the host when it frees enough space
	 *
	 * Similarly, if the host sets the feature bit in the host->guest
	 * ring buffer, the host is telling the guest that:
	 * 1) It will set the pending_send_sz field in the host->guest ring
	 *    buffer when it is waiting for space to become available, and
	 * 2) It will read the pending_send_sz field in the guest->host
	 *    ring buffer and interrupt the guest when it frees enough space
	 *
	 * If either the guest or host does not set the feature bit that it
	 * owns, that guest or host must do polling if it encounters a full
	 * ring buffer, and not signal the other end with an interrupt.
	 */
	volatile uint32_t	br_pending_snd_sz;
	uint32_t		br_rsvd1[12];
	union	{
		struct {
			uint32_t feat_pending_snd_sz:1;
		};
		uint32_t value;
	} br_feature_bits;

	/* Padding to PAGE_SIZE */
	uint8_t			br_rsvd2[4020];

	/*
	 * Total guest to host interrupt count
	 * - For rx ring, this counts the guest signaling host when this rx
	 * ring changing from full to not full.
	 *
	 * - For tx ring, this counts the guest signaling host when this tx
	 * ring changing from empty to non empty.
	 */
	uint64_t		br_g2h_intr_cnt;

	uint8_t			br_data[];
} __packed;
CTASSERT(sizeof(struct vmbus_bufring) == PAGE_SIZE);

/*
 * Channel
 */

#define VMBUS_CHAN_MAX_COMPAT	256
#define VMBUS_CHAN_MAX		(VMBUS_EVTFLAG_LEN * VMBUS_EVTFLAGS_MAX)

/*
 * Channel packets
 */

#define VMBUS_CHANPKT_SIZE_ALIGN	(1 << VMBUS_CHANPKT_SIZE_SHIFT)

#define VMBUS_CHANPKT_SETLEN(pktlen, len)		\
do {							\
	(pktlen) = (len) >> VMBUS_CHANPKT_SIZE_SHIFT;	\
} while (0)

#define VMBUS_CHANPKT_TOTLEN(tlen)	\
	roundup2((tlen), VMBUS_CHANPKT_SIZE_ALIGN)

#define VMBUS_CHANPKT_HLEN_MIN		\
	(sizeof(struct vmbus_chanpkt_hdr) >> VMBUS_CHANPKT_SIZE_SHIFT)

struct vmbus_chanpkt {
	struct vmbus_chanpkt_hdr cp_hdr;
} __packed;

struct vmbus_chanpkt_sglist {
	struct vmbus_chanpkt_hdr cp_hdr;
	uint32_t	cp_rsvd;
	uint32_t	cp_gpa_cnt;
	struct vmbus_gpa cp_gpa[];
} __packed;

struct vmbus_chanpkt_prplist {
	struct vmbus_chanpkt_hdr cp_hdr;
	uint32_t	cp_rsvd;
	uint32_t	cp_range_cnt;
	struct vmbus_gpa_range cp_range[];
} __packed;

/*
 * Channel messages
 * - Embedded in vmbus_message.msg_data, e.g. response and notification.
 * - Embedded in hypercall_postmsg_in.hc_data, e.g. request.
 */

#define VMBUS_CHANMSG_TYPE_CHOFFER		1	/* NOTE */
#define VMBUS_CHANMSG_TYPE_CHRESCIND		2	/* NOTE */
#define VMBUS_CHANMSG_TYPE_CHREQUEST		3	/* REQ */
#define VMBUS_CHANMSG_TYPE_CHOFFER_DONE		4	/* NOTE */
#define VMBUS_CHANMSG_TYPE_CHOPEN		5	/* REQ */
#define VMBUS_CHANMSG_TYPE_CHOPEN_RESP		6	/* RESP */
#define VMBUS_CHANMSG_TYPE_CHCLOSE		7	/* REQ */
#define VMBUS_CHANMSG_TYPE_GPADL_CONN		8	/* REQ */
#define VMBUS_CHANMSG_TYPE_GPADL_SUBCONN	9	/* REQ */
#define VMBUS_CHANMSG_TYPE_GPADL_CONNRESP	10	/* RESP */
#define VMBUS_CHANMSG_TYPE_GPADL_DISCONN	11	/* REQ */
#define VMBUS_CHANMSG_TYPE_GPADL_DISCONNRESP	12	/* RESP */
#define VMBUS_CHANMSG_TYPE_CHFREE		13	/* REQ */
#define VMBUS_CHANMSG_TYPE_CONNECT		14	/* REQ */
#define VMBUS_CHANMSG_TYPE_CONNECT_RESP		15	/* RESP */
#define VMBUS_CHANMSG_TYPE_DISCONNECT		16	/* REQ */
#define VMBUS_CHANMSG_TYPE_17			17
#define VMBUS_CHANMSG_TYPE_18			18
#define VMBUS_CHANMSG_TYPE_19			19
#define VMBUS_CHANMSG_TYPE_20			20
#define VMBUS_CHANMSG_TYPE_TL_CONN		21	/* REQ */
#define VMBUS_CHANMSG_TYPE_22			22
#define VMBUS_CHANMSG_TYPE_TL_RESULT		23	/* RESP */
#define VMBUS_CHANMSG_TYPE_MAX			24

struct vmbus_chanmsg_hdr {
	uint32_t	chm_type;	/* VMBUS_CHANMSG_TYPE_ */
	uint32_t	chm_rsvd;
} __packed;

/* VMBUS_CHANMSG_TYPE_CONNECT */
struct vmbus_chanmsg_connect {
	struct vmbus_chanmsg_hdr chm_hdr;
	uint32_t	chm_ver;
	uint32_t	chm_rsvd;
	uint64_t	chm_evtflags;
	uint64_t	chm_mnf1;
	uint64_t	chm_mnf2;
} __packed;

/* VMBUS_CHANMSG_TYPE_CONNECT_RESP */
struct vmbus_chanmsg_connect_resp {
	struct vmbus_chanmsg_hdr chm_hdr;
	uint8_t		chm_done;
} __packed;

/* VMBUS_CHANMSG_TYPE_CHREQUEST */
struct vmbus_chanmsg_chrequest {
	struct vmbus_chanmsg_hdr chm_hdr;
} __packed;

/* VMBUS_CHANMSG_TYPE_DISCONNECT */
struct vmbus_chanmsg_disconnect {
	struct vmbus_chanmsg_hdr chm_hdr;
} __packed;

/* VMBUS_CHANMSG_TYPE_TL_CONN */
/* Hyper-V socket guest connect request */
struct vmbus_chanmsg_tl_connect {
	struct vmbus_chanmsg_hdr chm_hdr;
	struct hyperv_guid guest_endpoint_id;
	struct hyperv_guid host_service_id;
} __packed;


/* VMBUS_CHANMSG_TYPE_CHOPEN */
struct vmbus_chanmsg_chopen {
	struct vmbus_chanmsg_hdr chm_hdr;
	uint32_t	chm_chanid;
	uint32_t	chm_openid;
	uint32_t	chm_gpadl;
	uint32_t	chm_vcpuid;
	uint32_t	chm_txbr_pgcnt;
#define VMBUS_CHANMSG_CHOPEN_UDATA_SIZE	120
	uint8_t		chm_udata[VMBUS_CHANMSG_CHOPEN_UDATA_SIZE];
} __packed;

/* VMBUS_CHANMSG_TYPE_CHOPEN_RESP */
struct vmbus_chanmsg_chopen_resp {
	struct vmbus_chanmsg_hdr chm_hdr;
	uint32_t	chm_chanid;
	uint32_t	chm_openid;
	uint32_t	chm_status;
} __packed;

/* VMBUS_CHANMSG_TYPE_GPADL_CONN */
struct vmbus_chanmsg_gpadl_conn {
	struct vmbus_chanmsg_hdr chm_hdr;
	uint32_t	chm_chanid;
	uint32_t	chm_gpadl;
	uint16_t	chm_range_len;
	uint16_t	chm_range_cnt;
	struct vmbus_gpa_range chm_range;
} __packed;

#define VMBUS_CHANMSG_GPADL_CONN_PGMAX		26
CTASSERT(__offsetof(struct vmbus_chanmsg_gpadl_conn,
    chm_range.gpa_page[VMBUS_CHANMSG_GPADL_CONN_PGMAX]) <=
    HYPERCALL_POSTMSGIN_DSIZE_MAX);

/* VMBUS_CHANMSG_TYPE_GPADL_SUBCONN */
struct vmbus_chanmsg_gpadl_subconn {
	struct vmbus_chanmsg_hdr chm_hdr;
	uint32_t	chm_msgno;
	uint32_t	chm_gpadl;
	uint64_t	chm_gpa_page[];
} __packed;

#define VMBUS_CHANMSG_GPADL_SUBCONN_PGMAX	28
CTASSERT(__offsetof(struct vmbus_chanmsg_gpadl_subconn,
    chm_gpa_page[VMBUS_CHANMSG_GPADL_SUBCONN_PGMAX]) <=
    HYPERCALL_POSTMSGIN_DSIZE_MAX);

/* VMBUS_CHANMSG_TYPE_GPADL_CONNRESP */
struct vmbus_chanmsg_gpadl_connresp {
	struct vmbus_chanmsg_hdr chm_hdr;
	uint32_t	chm_chanid;
	uint32_t	chm_gpadl;
	uint32_t	chm_status;
} __packed;

/* VMBUS_CHANMSG_TYPE_CHCLOSE */
struct vmbus_chanmsg_chclose {
	struct vmbus_chanmsg_hdr chm_hdr;
	uint32_t	chm_chanid;
} __packed;

/* VMBUS_CHANMSG_TYPE_GPADL_DISCONN */
struct vmbus_chanmsg_gpadl_disconn {
	struct vmbus_chanmsg_hdr chm_hdr;
	uint32_t	chm_chanid;
	uint32_t	chm_gpadl;
} __packed;

/* VMBUS_CHANMSG_TYPE_CHFREE */
struct vmbus_chanmsg_chfree {
	struct vmbus_chanmsg_hdr chm_hdr;
	uint32_t	chm_chanid;
} __packed;

/* VMBUS_CHANMSG_TYPE_CHRESCIND */
struct vmbus_chanmsg_chrescind {
	struct vmbus_chanmsg_hdr chm_hdr;
	uint32_t	chm_chanid;
} __packed;

/* Size of the user defined data buffer for non-pipe offers */
#define VMBUS_CHANMSG_CHOFFER_UDATA_SIZE		120

/* Size of the user defined data buffer for pipe offers. */
#define VMBUS_CHANMSG_CHOFFER_UDATA_PIPE_SIZE		116

/* VMBUS_CHANMSG_TYPE_CHOFFER */
struct vmbus_chanmsg_choffer {
	struct vmbus_chanmsg_hdr chm_hdr;
	struct hyperv_guid chm_chtype;
	struct hyperv_guid chm_chinst;
	uint64_t	chm_chlat;	/* unit: 100ns */
	uint32_t	chm_chrev;
	uint32_t	chm_svrctx_sz;
	uint16_t	chm_chflags;
	uint16_t	chm_mmio_sz;	/* unit: MB */

	union {
		/* Non-pipes */
		struct {
			uint8_t	user_def[VMBUS_CHANMSG_CHOFFER_UDATA_SIZE];
		} std;
		/*
		 * Pipes:
		 * For integrated pipe protocol, which is implemented on
		 * top of standard user-defined data. Pipe clients have
		 * VMBUS_CHANMSG_CHOFFER_UDATA_PIPE_SIZE bytes left for
		 * their own user.
		 */
		struct {
			uint32_t pipe_mode;
			uint8_t
			    user_def[VMBUS_CHANMSG_CHOFFER_UDATA_PIPE_SIZE];
		} pipe;
	} chm_udata;

	uint16_t	chm_subidx;
	uint16_t	chm_rsvd;
	uint32_t	chm_chanid;
	uint8_t		chm_montrig;
	uint8_t		chm_flags1;	/* VMBUS_CHOFFER_FLAG1_ */
	uint16_t	chm_flags2;
	uint32_t	chm_connid;
} __packed;
CTASSERT(sizeof(struct vmbus_chanmsg_choffer) <= VMBUS_MSG_DSIZE_MAX);

/* Server Flag */
#define VMBUS_CHAN_TLNPI_PROVIDER_OFFER			0x2000

#define VMBUS_CHOFFER_FLAG1_HASMNF	0x01

#endif	/* !_VMBUS_REG_H_ */
