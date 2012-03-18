/*-
 * Copyright (c) 2011 Adrian Chadd, Xenion Lty Ltd
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
#ifndef	__IEEE80211_ALQ_H__
#define	__IEEE80211_ALQ_H__

/*
 * 64 byte ieee80211_alq_rec size.
 */
#define	IEEE80211_ALQ_PAYLOAD_SIZE	52

/*
 * timestamp
 * wlan interface
 * operation
 * sub-operation
 * rest of structure - operation specific
 */
struct ieee80211_alq_rec {
	uint32_t	r_timestamp;	/* XXX may wrap! */
	uint32_t	r_threadid;	/* current thread id */
	uint16_t	r_wlan;		/* wlan interface number */
	uint8_t		r_version;	/* version */
	uint8_t		r_op;		/* top-level operation id */
	u_char		r_payload[IEEE80211_ALQ_PAYLOAD_SIZE];
					/* operation-specific payload */
};

/* General logging function */
extern	void ieee80211_alq_log(struct ieee80211vap *vap, uint8_t op,
	    u_char *p, int l);

/*
 * Debugging entry points
 */

/*
 * This should be called by the driver on each RX frame.
 */
#define	IEEE80211_ALQ_OP_RXFRAME	0x1
#define	IEEE80211_ALQ_OP_TXFRAME	0x2
#define	IEEE80211_ALQ_OP_TXCOMPLETE	0x3
#define	IEEE80211_ALQ_OP_TX_BAW		0x4

/* Driver-specific - for descriptor contents, etc */
#define	IEEE80211_ALQ_OP_RX_DESC	0x81
#define	IEEE80211_ALQ_OP_TX_DESC	0x82
#define	IEEE80211_ALQ_OP_TX_DESCCOMP	0x83

struct ieee80211_alq_rx_frame_struct {
	uint64_t	tsf;	/* Network order */
	uintptr_t	bf;	/* Driver-specific buffer ptr */
	uint8_t		rxq;	/* Driver-specific RX queue */
	uint8_t		pad[3];	/* Pad alignment */
	struct ieee80211_qosframe wh;	/* XXX 4 bytes, QoS? */
};

struct ieee80211_alq_tx_frame {
	uint64_t	tsf;	/* Network order */
	uintptr_t	bf;	/* Driver-specific buffer ptr */
	uint32_t	tx_flags;	/* Driver-specific TX flags */
	uint8_t		txq;	/* Driver-specific TX queue */
	uint8_t		pad[3];	/* Pad alignment */
	struct ieee80211_qosframe wh;	/* XXX 4 bytes, QoS? */
};

struct ieee80211_alq_tx_frame_complete {
	uint64_t	tsf;	/* Network order */
	uintptr_t	bf;	/* Driver-specific buffer ptr */
	uint8_t		txq;	/* Driver-specific TX queue */
	uint8_t		txstatus;	/* driver-specific TX status */
	uint8_t		pad[2];	/* Pad alignment */
	struct ieee80211_qosframe wh;	/* XXX 4 bytes, QoS? */
};


/*
 * This is used for frame RX.
 */
static inline void
ieee80211_alq_rx_frame(struct ieee80211vap *vap,
    struct ieee80211_frame *wh, uint64_t tsf, void *bf, uint8_t rxq)
{
	struct ieee80211_alq_rx_frame_struct rf;
	
	memset(&rf, 0, sizeof(rf));
	rf.tsf = htole64(tsf);
	rf.bf = (uintptr_t) bf;
	rf.rxq = rxq;
	memcpy(&rf.wh, wh, sizeof(struct ieee80211_qosframe));
	ieee80211_alq_log(vap, IEEE80211_ALQ_OP_RXFRAME, (char *) &rf,
	    sizeof(rf));
}

/*
 * Frame TX scheduling
 */
static inline void
ieee80211_alq_tx_frame(struct ieee80211vap *vap,
    struct ieee80211_frame *wh, uint64_t tsf, void *bf, uint8_t txq,
    uint32_t tx_flags)
{

}

/*
 * Frame TX completion
 */
static inline void
ieee80211_alq_tx_frame_comp(struct ieee80211vap *vap,
    struct ieee80211_frame *wh, uint64_t tsf, void *bf, uint8_t txq,
    uint8_t tx_status)
{

}

struct ieee80211_alq_tx_baw_note_struct {
	uintptr_t	bf;
	uint8_t		tid;
	uint8_t		what;
	uint16_t	baw;
	uint16_t	wnd;
	uint16_t	new_baw;
};

/*
 * TX BAW noting - add, remove, etc
 */

#define	IEEE80211_ALQ_TX_BAW_ADD	0x1
#define	IEEE80211_ALQ_TX_BAW_COMPLETE	0x2

static inline void
ieee80211_alq_tx_baw_note(struct ieee80211vap *vap,
    struct ieee80211_frame *wh, void *bf, uint8_t tid, uint8_t what,
    uint16_t baw, uint16_t wnd, uint16_t new_baw)
{
	struct ieee80211_alq_tx_baw_note_struct tb;

	memset(&tb, 0, sizeof(tb));

	tb.bf = (uintptr_t) bf;
	tb.tid = tid;
	tb.what = what;
	tb.baw = htons(baw);
	tb.wnd = htons(wnd);
	tb.new_baw = htons(new_baw);

	ieee80211_alq_log(vap, IEEE80211_ALQ_OP_TX_BAW, (char *) &tb,
	    sizeof(tb));
}

#endif	/* __IEEE80211_ALQ_H__ */
