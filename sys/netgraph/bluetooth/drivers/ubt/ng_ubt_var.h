/*
 * ng_ubt_var.h
 *
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
 * $Id: ng_ubt_var.h,v 1.1 2002/11/09 19:09:02 max Exp $
 * $FreeBSD$
 */

#ifndef _NG_UBT_VAR_H_
#define _NG_UBT_VAR_H_

/* pullup wrapper */
#define NG_UBT_M_PULLUP(m, s) \
	do { \
		if ((m)->m_len < (s)) \
			(m) = m_pullup((m), (s)); \
		if ((m) == NULL) \
			NG_UBT_ALERT("%s: %s - m_pullup(%d) failed\n", \
				__func__, USBDEVNAME(sc->sc_dev), (s)); \
	} while (0)

/* Debug printf's */
#define NG_UBT_ALERT	if (sc->sc_debug >= NG_UBT_ALERT_LEVEL) printf
#define NG_UBT_ERR	if (sc->sc_debug >= NG_UBT_ERR_LEVEL)   printf
#define NG_UBT_WARN	if (sc->sc_debug >= NG_UBT_WARN_LEVEL)  printf
#define NG_UBT_INFO	if (sc->sc_debug >= NG_UBT_INFO_LEVEL)  printf

/* Bluetooth USB control request type */
#define UBT_HCI_REQUEST		0x20
#define UBT_DEFAULT_QLEN	12

/* USB device softc structure */
struct ubt_softc {
	/* State */
	ng_ubt_node_debug_ep	 sc_debug;	/* debug level */
	u_int32_t		 sc_flags;	/* device flags */
#define UBT_NEED_FRAME_TYPE	(1 << 0)	/* device required frame type */
#define UBT_HAVE_FRAME_TYPE UBT_NEED_FRAME_TYPE
#define UBT_CMD_XMIT		(1 << 1)	/* CMD xmit in progress */
#define UBT_ACL_XMIT		(1 << 2)	/* ACL xmit in progress */
#define UBT_SCO_XMIT		(1 << 3)	/* SCO xmit in progress */

	ng_ubt_node_stat_ep	 sc_stat;	/* statistic */
#define NG_UBT_STAT_PCKTS_SENT(s)	(s).pckts_sent ++
#define NG_UBT_STAT_BYTES_SENT(s, n)	(s).bytes_sent += (n)
#define NG_UBT_STAT_PCKTS_RECV(s)	(s).pckts_recv ++
#define NG_UBT_STAT_BYTES_RECV(s, n)	(s).bytes_recv += (n)
#define NG_UBT_STAT_OERROR(s)		(s).oerrors ++
#define NG_UBT_STAT_IERROR(s)		(s).ierrors ++
#define NG_UBT_STAT_RESET(s)		bzero(&(s), sizeof((s)))

	/* USB device specific */
	USBBASEDEVICE		 sc_dev;	/* pointer back to USB device */
	usbd_device_handle	 sc_udev;	/* USB device handle */

	usbd_interface_handle	 sc_iface0;	/* USB interface 0 */
	usbd_interface_handle	 sc_iface1;	/* USB interface 1 */

	struct ifqueue		 sc_inq;	/* incoming queue */
	void			*sc_ith;	/* SWI interrupt handler */

	/* Interrupt pipe (HCI events) */
	int			 sc_intr_ep;	/* interrupt endpoint */
	usbd_pipe_handle	 sc_intr_pipe;	/* interrupt pipe handle */
	usbd_xfer_handle	 sc_intr_xfer;	/* intr xfer */
	u_int8_t		*sc_intr_buffer; /* interrupt buffer */
#define UBT_INTR_BUFFER_SIZE \
		(sizeof(ng_hci_event_pkt_t) + NG_HCI_EVENT_PKT_SIZE)

	/* Control pipe (HCI commands) */
	usbd_xfer_handle	 sc_ctrl_xfer;	/* control xfer handle */
	void			*sc_ctrl_buffer; /* control buffer */
	struct ifqueue		 sc_cmdq;	/* HCI command queue */
#define UBT_CTRL_BUFFER_SIZE \
		(sizeof(ng_hci_cmd_pkt_t) + NG_HCI_CMD_PKT_SIZE)

	/* Bulk in pipe (ACL data) */
	int			 sc_bulk_in_ep;	/* bulk-in enpoint */
	usbd_pipe_handle	 sc_bulk_in_pipe; /* bulk-in pipe */
	usbd_xfer_handle	 sc_bulk_in_xfer; /* bulk-in xfer */
	void			*sc_bulk_in_buffer; /* bulk-in buffer */

	/* Bulk out pipe (ACL data) */
	int			 sc_bulk_out_ep; /* bulk-out endpoint */
	usbd_pipe_handle	 sc_bulk_out_pipe; /* bulk-out pipe */
	usbd_xfer_handle	 sc_bulk_out_xfer; /* bulk-out xfer */
	void			*sc_bulk_out_buffer; /* bulk-out buffer */
	struct ifqueue		 sc_aclq;	/* ACL data queue */
#define UBT_BULK_BUFFER_SIZE \
		512	/* XXX should be big enough to hold one frame */

	/* Isoc. in pipe (SCO data) */
	int			 sc_isoc_in_ep; /* isoc-in endpoint */
	usbd_pipe_handle	 sc_isoc_in_pipe; /* isoc-in pipe */
	usbd_xfer_handle	 sc_isoc_in_xfer; /* isoc-in xfer */
	void			*sc_isoc_in_buffer; /* isoc-in buffer */
	u_int16_t		*sc_isoc_in_frlen; /* isoc-in. frame length */

	/* Isoc. out pipe (ACL data) */
	int			 sc_isoc_out_ep; /* isoc-out endpoint */
	usbd_pipe_handle	 sc_isoc_out_pipe; /* isoc-out pipe */
	usbd_xfer_handle	 sc_isoc_out_xfer; /* isoc-out xfer */
	void			*sc_isoc_out_buffer; /* isoc-in buffer */
	u_int16_t		*sc_isoc_out_frlen; /* isoc-out. frame length */
	struct ifqueue		 sc_scoq;	/* SCO data queue */

	int			 sc_isoc_size; /* max. size of isoc. packet */
	u_int32_t		 sc_isoc_nframes; /* num. isoc. frames */
#define UBT_ISOC_BUFFER_SIZE \
		(sizeof(ng_hci_scodata_pkt_t) + NG_HCI_SCO_PKT_SIZE)
	
	/* Netgraph specific */
	node_p			 sc_node;	/* pointer back to node */
	hook_p			 sc_hook;	/* upstream hook */
};
typedef struct ubt_softc	ubt_softc_t;
typedef struct ubt_softc *	ubt_softc_p;

#endif /* ndef _NG_UBT_VAR_H_ */

