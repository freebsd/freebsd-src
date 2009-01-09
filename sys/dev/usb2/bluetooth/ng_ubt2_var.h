/*
 * ng_ubt_var.h
 */

/*-
 * Copyright (c) 2001-2002 Maksim Yevmenkin <m_evmenkin@yahoo.com>
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
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id: ng_ubt_var.h,v 1.2 2003/03/22 23:44:36 max Exp $
 * $FreeBSD$
 */

#ifndef _NG_UBT_VAR_H_
#define	_NG_UBT_VAR_H_

/* pullup wrapper */
#define	NG_UBT_M_PULLUP(m, s) \
	do { \
		if ((m)->m_len < (s)) \
			(m) = m_pullup((m), (s)); \
		if ((m) == NULL) { \
			NG_UBT_ALERT("%s: %s - m_pullup(%d) failed\n", \
				__func__, sc->sc_name, (s)); \
		} \
	} while (0)

/* Debug printf's */
#define	NG_UBT_DEBUG(level, sc, fmt, ...) do { \
    if ((sc)->sc_debug >= (level)) { \
        printf("%s:%s:%d: " fmt, (sc)->sc_name, \
	       __FUNCTION__, __LINE__,## __VA_ARGS__); \
    } \
} while (0)

#define	NG_UBT_ALERT(...) NG_UBT_DEBUG(NG_UBT_ALERT_LEVEL, __VA_ARGS__)
#define	NG_UBT_ERR(...)   NG_UBT_DEBUG(NG_UBT_ERR_LEVEL, __VA_ARGS__)
#define	NG_UBT_WARN(...)  NG_UBT_DEBUG(NG_UBT_WARN_LEVEL, __VA_ARGS__)
#define	NG_UBT_INFO(...)  NG_UBT_DEBUG(NG_UBT_INFO_LEVEL, __VA_ARGS__)

/* Bluetooth USB control request type */
#define	UBT_HCI_REQUEST		0x20
#define	UBT_DEFAULT_QLEN	12

/* Bluetooth USB defines */
#define	UBT_IF_0_N_TRANSFER  7		/* units */
#define	UBT_IF_1_N_TRANSFER  4		/* units */
#define	UBT_ISOC_NFRAMES    25		/* units */

/* USB device softc structure */
struct ubt_softc {
	/* State */
	ng_ubt_node_debug_ep sc_debug;	/* debug level */
	uint32_t sc_flags;		/* device flags */
#define	UBT_NEED_FRAME_TYPE	(1 << 0)/* device required frame type */
#define	UBT_HAVE_FRAME_TYPE UBT_NEED_FRAME_TYPE
#define	UBT_FLAG_READ_STALL     (1 << 1)/* read transfer has stalled */
#define	UBT_FLAG_WRITE_STALL    (1 << 2)/* write transfer has stalled */
#define	UBT_FLAG_INTR_STALL     (1 << 3)/* interrupt transfer has stalled */

	ng_ubt_node_stat_ep sc_stat;	/* statistic */
#define	NG_UBT_STAT_PCKTS_SENT(s)	(s).pckts_sent ++
#define	NG_UBT_STAT_BYTES_SENT(s, n)	(s).bytes_sent += (n)
#define	NG_UBT_STAT_PCKTS_RECV(s)	(s).pckts_recv ++
#define	NG_UBT_STAT_BYTES_RECV(s, n)	(s).bytes_recv += (n)
#define	NG_UBT_STAT_OERROR(s)		(s).oerrors ++
#define	NG_UBT_STAT_IERROR(s)		(s).ierrors ++
#define	NG_UBT_STAT_RESET(s)		bzero(&(s), sizeof((s)))

	uint8_t	sc_name[16];

	struct mtx sc_mtx;

	/* USB device specific */
	struct usb2_xfer *sc_xfer_if_0[UBT_IF_0_N_TRANSFER];
	struct usb2_xfer *sc_xfer_if_1[UBT_IF_1_N_TRANSFER];

	/* Interrupt pipe (HCI events) */
	struct mbuf *sc_intr_buffer;	/* interrupt buffer */

	/* Control pipe (HCI commands) */
	struct ng_bt_mbufq sc_cmdq;	/* HCI command queue */
#define	UBT_CTRL_BUFFER_SIZE (sizeof(ng_hci_cmd_pkt_t) + NG_HCI_CMD_PKT_SIZE)

	/* Bulk in pipe (ACL data) */
	struct mbuf *sc_bulk_in_buffer;	/* bulk-in buffer */

	/* Bulk out pipe (ACL data) */
	struct ng_bt_mbufq sc_aclq;	/* ACL data queue */
#define	UBT_BULK_READ_BUFFER_SIZE (MCLBYTES-1)	/* reserve one byte for ID-tag */
#define	UBT_BULK_WRITE_BUFFER_SIZE (MCLBYTES)

	/* Isoc. out pipe (ACL data) */
	struct ng_bt_mbufq sc_scoq;	/* SCO data queue */

	/* Isoc. in pipe (ACL data) */
	struct ng_bt_mbufq sc_sciq;	/* SCO data queue */

	/* Netgraph specific */
	node_p	sc_node;		/* pointer back to node */
	hook_p	sc_hook;		/* upstream hook */
};
typedef struct ubt_softc ubt_softc_t;
typedef struct ubt_softc *ubt_softc_p;

#endif					/* ndef _NG_UBT_VAR_H_ */
