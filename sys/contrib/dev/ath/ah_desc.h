/*-
 * Copyright (c) 2002-2004 Sam Leffler, Errno Consulting, Atheros
 * Communications, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the following conditions are met:
 * 1. The materials contained herein are unmodified and are used
 *    unmodified.
 * 2. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following NO
 *    ''WARRANTY'' disclaimer below (''Disclaimer''), without
 *    modification.
 * 3. Redistributions in binary form must reproduce at minimum a
 *    disclaimer similar to the Disclaimer below and any redistribution
 *    must be conditioned upon including a substantially similar
 *    Disclaimer requirement for further binary redistribution.
 * 4. Neither the names of the above-listed copyright holders nor the
 *    names of any contributors may be used to endorse or promote
 *    product derived from this software without specific prior written
 *    permission.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ''AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF NONINFRINGEMENT,
 * MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE
 * FOR SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGES.
 *
 * $Id: ah_desc.h,v 1.15 2004/10/24 02:17:43 sam Exp $
 */

#ifndef _DEV_ATH_DESC_H
#define _DEV_ATH_DESC_H

/*
 * Transmit descriptor status.  This structure is filled
 * in only after the tx descriptor process method finds a
 * ``done'' descriptor; at which point it returns something
 * other than HAL_EINPROGRESS.
 *
 * Note that ts_antenna may not be valid for all h/w.  It
 * should be used only if non-zero.
 */
struct ath_tx_status {
	u_int16_t	ts_seqnum;	/* h/w assigned sequence number */
	u_int16_t	ts_tstamp;	/* h/w assigned timestamp */
	u_int8_t	ts_status;	/* frame status, 0 => xmit ok */
	u_int8_t	ts_rate;	/* h/w transmit rate index */
#define	HAL_TXSTAT_ALTRATE	0x80	/* alternate xmit rate used */
	int8_t		ts_rssi;	/* tx ack RSSI */
	u_int8_t	ts_shortretry;	/* # short retries */
	u_int8_t	ts_longretry;	/* # long retries */
	u_int8_t	ts_virtcol;	/* virtual collision count */
	u_int8_t	ts_antenna;	/* antenna information */
};

#define	HAL_TXERR_XRETRY	0x01	/* excessive retries */
#define	HAL_TXERR_FILT		0x02	/* blocked by tx filtering */
#define	HAL_TXERR_FIFO		0x04	/* fifo underrun */

/*
 * Receive descriptor status.  This structure is filled
 * in only after the rx descriptor process method finds a
 * ``done'' descriptor; at which point it returns something
 * other than HAL_EINPROGRESS.
 *
 * If rx_status is zero, then the frame was received ok;
 * otherwise the error information is indicated and rs_phyerr
 * contains a phy error code if HAL_RXERR_PHY is set.  In general
 * the frame contents is undefined when an error occurred thought
 * for some errors (e.g. a decryption error), it may be meaningful.
 *
 * Note that the receive timestamp is expanded using the TSF to
 * a full 16 bits (regardless of what the h/w provides directly).
 *
 * rx_rssi is in units of dbm above the noise floor.  This value
 * is measured during the preamble and PLCP; i.e. with the initial
 * 4us of detection.  The noise floor is typically a consistent
 * -96dBm absolute power in a 20MHz channel.
 */
struct ath_rx_status {
	u_int16_t	rs_datalen;	/* rx frame length */
	u_int16_t	rs_tstamp;	/* h/w assigned timestamp */
	u_int8_t	rs_status;	/* rx status, 0 => recv ok */
	u_int8_t	rs_phyerr;	/* phy error code */
	int8_t		rs_rssi;	/* rx frame RSSI */
	u_int8_t	rs_keyix;	/* key cache index */
	u_int8_t	rs_rate;	/* h/w receive rate index */
	u_int8_t	rs_antenna;	/* antenna information */
	u_int8_t	rs_more;	/* more descriptors follow */
};

#define	HAL_RXERR_CRC		0x01	/* CRC error on frame */
#define	HAL_RXERR_PHY		0x02	/* PHY error, rs_phyerr is valid */
#define	HAL_RXERR_FIFO		0x04	/* fifo overrun */
#define	HAL_RXERR_DECRYPT	0x08	/* non-Michael decrypt error */
#define	HAL_RXERR_MIC		0x10	/* Michael MIC decrypt error */

enum {
	HAL_PHYERR_UNDERRUN		= 0,	/* Transmit underrun */
	HAL_PHYERR_TIMING		= 1,	/* Timing error */
	HAL_PHYERR_PARITY		= 2,	/* Illegal parity */
	HAL_PHYERR_RATE			= 3,	/* Illegal rate */
	HAL_PHYERR_LENGTH		= 4,	/* Illegal length */
	HAL_PHYERR_RADAR		= 5,	/* Radar detect */
	HAL_PHYERR_SERVICE		= 6,	/* Illegal service */
	HAL_PHYERR_TOR			= 7,	/* Transmit override receive */
	/* NB: these are specific to the 5212 */
	HAL_PHYERR_OFDM_TIMING		= 17,	/* */
	HAL_PHYERR_OFDM_SIGNAL_PARITY	= 18,	/* */
	HAL_PHYERR_OFDM_RATE_ILLEGAL	= 19,	/* */
	HAL_PHYERR_OFDM_LENGTH_ILLEGAL	= 20,	/* */
	HAL_PHYERR_OFDM_POWER_DROP	= 21,	/* */
	HAL_PHYERR_OFDM_SERVICE		= 22,	/* */
	HAL_PHYERR_OFDM_RESTART		= 23,	/* */
	HAL_PHYERR_CCK_TIMING		= 25,	/* */
	HAL_PHYERR_CCK_HEADER_CRC	= 26,	/* */
	HAL_PHYERR_CCK_RATE_ILLEGAL	= 27,	/* */
	HAL_PHYERR_CCK_SERVICE		= 30,	/* */
	HAL_PHYERR_CCK_RESTART		= 31,	/* */
};

/* value found in rs_keyix to mark invalid entries */
#define	HAL_RXKEYIX_INVALID	((u_int8_t) -1)
/* value used to specify no encryption key for xmit */
#define	HAL_TXKEYIX_INVALID	((u_int) -1)

/* XXX rs_antenna definitions */

/*
 * Definitions for the software frame/packet descriptors used by
 * the Atheros HAL.  This definition obscures hardware-specific
 * details from the driver.  Drivers are expected to fillin the
 * portions of a descriptor that are not opaque then use HAL calls
 * to complete the work.  Status for completed frames is returned
 * in a device-independent format.
 */
struct ath_desc {
	/*
	 * The following definitions are passed directly
	 * the hardware and managed by the HAL.  Drivers
	 * should not touch those elements marked opaque.
	 */
	u_int32_t	ds_link;	/* phys address of next descriptor */
	u_int32_t	ds_data;	/* phys address of data buffer */
	u_int32_t	ds_ctl0;	/* opaque DMA control 0 */
	u_int32_t	ds_ctl1;	/* opaque DMA control 1 */
	u_int32_t	ds_hw[4];	/* opaque h/w region */
	/*
	 * The remaining definitions are managed by software;
	 * these are valid only after the rx/tx process descriptor
	 * methods return a non-EINPROGRESS  code.
	 */
	union {
		struct ath_tx_status tx;/* xmit status */
		struct ath_rx_status rx;/* recv status */
	} ds_us;
} __packed;

#define	ds_txstat	ds_us.tx
#define	ds_rxstat	ds_us.rx

/* flags passed to tx descriptor setup methods */
#define	HAL_TXDESC_CLRDMASK	0x0001	/* clear destination filter mask */
#define	HAL_TXDESC_NOACK	0x0002	/* don't wait for ACK */
#define	HAL_TXDESC_RTSENA	0x0004	/* enable RTS */
#define	HAL_TXDESC_CTSENA	0x0008	/* enable CTS */
#define	HAL_TXDESC_INTREQ	0x0010	/* enable per-descriptor interrupt */
#define	HAL_TXDESC_VEOL		0x0020	/* mark virtual EOL */

/* flags passed to rx descriptor setup methods */
#define	HAL_RXDESC_INTREQ	0x0020	/* enable per-descriptor interrupt */
#endif /* _DEV_ATH_AR521XDMA_H */
