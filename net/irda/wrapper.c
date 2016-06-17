/*********************************************************************
 *                
 * Filename:      wrapper.c
 * Version:       1.2
 * Description:   IrDA SIR async wrapper layer
 * Status:        Stable
 * Author:        Dag Brattli <dagb@cs.uit.no>
 * Created at:    Mon Aug  4 20:40:53 1997
 * Modified at:   Fri Jan 28 13:21:09 2000
 * Modified by:   Dag Brattli <dagb@cs.uit.no>
 * Modified at:   Fri May 28  3:11 CST 1999
 * Modified by:   Horst von Brand <vonbrand@sleipnir.valparaiso.cl>
 * 
 *     Copyright (c) 1998-2000 Dag Brattli <dagb@cs.uit.no>, 
 *     All Rights Reserved.
 *     
 *     This program is free software; you can redistribute it and/or 
 *     modify it under the terms of the GNU General Public License as 
 *     published by the Free Software Foundation; either version 2 of 
 *     the License, or (at your option) any later version.
 *
 *     Neither Dag Brattli nor University of Tromsø admit liability nor
 *     provide warranty for any of this software. This material is 
 *     provided "AS-IS" and at no charge.
 *
 ********************************************************************/

#include <linux/skbuff.h>
#include <linux/string.h>
#include <asm/byteorder.h>

#include <net/irda/irda.h>
#include <net/irda/wrapper.h>
#include <net/irda/irtty.h>
#include <net/irda/crc.h>
#include <net/irda/irlap.h>
#include <net/irda/irlap_frame.h>
#include <net/irda/irda_device.h>

static inline int stuff_byte(__u8 byte, __u8 *buf);

static void state_outside_frame(struct net_device *dev, 
				struct net_device_stats *stats, 
				iobuff_t *rx_buff, __u8 byte);
static void state_begin_frame(struct net_device *dev, 
			      struct net_device_stats *stats, 
			      iobuff_t *rx_buff, __u8 byte);
static void state_link_escape(struct net_device *dev, 
			      struct net_device_stats *stats, 
			      iobuff_t *rx_buff, __u8 byte);
static void state_inside_frame(struct net_device *dev, 
			       struct net_device_stats *stats, 
			       iobuff_t *rx_buff, __u8 byte);

static void (*state[])(struct net_device *dev, struct net_device_stats *stats, 
		       iobuff_t *rx_buff, __u8 byte) = 
{ 
	state_outside_frame,
	state_begin_frame,
	state_link_escape,
	state_inside_frame,
};

/*
 * Function async_wrap (skb, *tx_buff, buffsize)
 *
 *    Makes a new buffer with wrapping and stuffing, should check that 
 *    we don't get tx buffer overflow.
 */
int async_wrap_skb(struct sk_buff *skb, __u8 *tx_buff, int buffsize)
{
	struct irda_skb_cb *cb = (struct irda_skb_cb *) skb->cb;
	int xbofs;
 	int i;
	int n;
	union {
		__u16 value;
		__u8 bytes[2];
	} fcs;

	/* Initialize variables */
	fcs.value = INIT_FCS;
	n = 0;

	/*
	 *  Send  XBOF's for required min. turn time and for the negotiated
	 *  additional XBOFS
	 */
	
	if (cb->magic != LAP_MAGIC) {
		/* 
		 * This will happen for all frames sent from user-space.
		 * Nothing to worry about, but we set the default number of 
		 * BOF's
		 */
		IRDA_DEBUG(1, "%s(), wrong magic in skb!\n", __FUNCTION__);
		xbofs = 10;
	} else
		xbofs = cb->xbofs + cb->xbofs_delay;

	IRDA_DEBUG(4, "%s(), xbofs=%d\n", __FUNCTION__, xbofs);

	/* Check that we never use more than 115 + 48 xbofs */
	if (xbofs > 163) {
		IRDA_DEBUG(0, "%s(), too many xbofs (%d)\n", __FUNCTION__, xbofs);
		xbofs = 163;
	}

	memset(tx_buff+n, XBOF, xbofs);
	n += xbofs;

	/* Start of packet character BOF */
	tx_buff[n++] = BOF;

	/* Insert frame and calc CRC */
	for (i=0; i < skb->len; i++) {
		/*
		 *  Check for the possibility of tx buffer overflow. We use
		 *  bufsize-5 since the maximum number of bytes that can be 
		 *  transmitted after this point is 5.
		 */
		ASSERT(n < (buffsize-5), return n;);

		n += stuff_byte(skb->data[i], tx_buff+n);
		fcs.value = irda_fcs(fcs.value, skb->data[i]);
	}
	
	/* Insert CRC in little endian format (LSB first) */
	fcs.value = ~fcs.value;
#ifdef __LITTLE_ENDIAN
	n += stuff_byte(fcs.bytes[0], tx_buff+n);
	n += stuff_byte(fcs.bytes[1], tx_buff+n);
#else /* ifdef __BIG_ENDIAN */
	n += stuff_byte(fcs.bytes[1], tx_buff+n);
	n += stuff_byte(fcs.bytes[0], tx_buff+n);
#endif
	tx_buff[n++] = EOF;

	return n;
}

/*
 * Function stuff_byte (byte, buf)
 *
 *    Byte stuff one single byte and put the result in buffer pointed to by
 *    buf. The buffer must at all times be able to have two bytes inserted.
 * 
 */
static inline int stuff_byte(__u8 byte, __u8 *buf) 
{
	switch (byte) {
	case BOF: /* FALLTHROUGH */
	case EOF: /* FALLTHROUGH */
	case CE:
		/* Insert transparently coded */
		buf[0] = CE;               /* Send link escape */
		buf[1] = byte^IRDA_TRANS;    /* Complement bit 5 */
		return 2;
		/* break; */
	default:
		 /* Non-special value, no transparency required */
		buf[0] = byte;
		return 1;
		/* break; */
	}
}

/*
 * Function async_bump (buf, len, stats)
 *
 *    Got a frame, make a copy of it, and pass it up the stack! We can try
 *    to inline it since it's only called from state_inside_frame
 */
inline void async_bump(struct net_device *dev, struct net_device_stats *stats,
		       __u8 *buf, int len)
{
       	struct sk_buff *skb;

	skb = dev_alloc_skb(len+1);
	if (!skb)  {
		stats->rx_dropped++;
		return;
	}

	/* Align IP header to 20 bytes */
	skb_reserve(skb, 1);
	
        /* Copy data without CRC */
	memcpy(skb_put(skb, len-2), buf, len-2); 
	
	/* Feed it to IrLAP layer */
	skb->dev = dev;
	skb->mac.raw  = skb->data;
	skb->protocol = htons(ETH_P_IRDA);

	netif_rx(skb);

	stats->rx_packets++;
	stats->rx_bytes += len;	
}

/*
 * Function async_unwrap_char (dev, rx_buff, byte)
 *
 *    Parse and de-stuff frame received from the IrDA-port
 *
 */
inline void async_unwrap_char(struct net_device *dev, 
			      struct net_device_stats *stats, 
			      iobuff_t *rx_buff, __u8 byte)
{
	(*state[rx_buff->state])(dev, stats, rx_buff, byte);
}
	 
/*
 * Function state_outside_frame (dev, rx_buff, byte)
 *
 *    Not receiving any frame (or just bogus data)
 *
 */
static void state_outside_frame(struct net_device *dev, 
				struct net_device_stats *stats, 
				iobuff_t *rx_buff, __u8 byte)
{
	switch (byte) {
	case BOF:
		rx_buff->state = BEGIN_FRAME;
		rx_buff->in_frame = TRUE;
		break;
	case XBOF:
		/* idev->xbofs++; */
		break;
	case EOF:
		irda_device_set_media_busy(dev, TRUE);
		break;
	default:
		irda_device_set_media_busy(dev, TRUE);
		break;
	}
}

/*
 * Function state_begin_frame (idev, byte)
 *
 *    Begin of frame detected
 *
 */
static void state_begin_frame(struct net_device *dev, 
			      struct net_device_stats *stats, 
			      iobuff_t *rx_buff, __u8 byte)
{
	/* Time to initialize receive buffer */
	rx_buff->data = rx_buff->head;
	rx_buff->len = 0;
	rx_buff->fcs = INIT_FCS;

	switch (byte) {
	case BOF:
		/* Continue */
		break;
	case CE:
		/* Stuffed byte */
		rx_buff->state = LINK_ESCAPE;
		break;
	case EOF:
		/* Abort frame */
		rx_buff->state = OUTSIDE_FRAME;
		IRDA_DEBUG(1, "%s(), abort frame\n", __FUNCTION__);
		stats->rx_errors++;
		stats->rx_frame_errors++;
		break;
	default:
		rx_buff->data[rx_buff->len++] = byte;
		rx_buff->fcs = irda_fcs(rx_buff->fcs, byte);
		rx_buff->state = INSIDE_FRAME;
		break;
	}
}

/*
 * Function state_link_escape (dev, byte)
 *
 *    Found link escape character
 *
 */
static void state_link_escape(struct net_device *dev, 
			      struct net_device_stats *stats, 
			      iobuff_t *rx_buff, __u8 byte)
{
	switch (byte) {
	case BOF: /* New frame? */
		IRDA_DEBUG(1, "%s(), Discarding incomplete frame\n", __FUNCTION__);
		rx_buff->state = BEGIN_FRAME;
		irda_device_set_media_busy(dev, TRUE);
		break;
	case CE:
		WARNING("%s(), state not defined\n", __FUNCTION__);
		break;
	case EOF: /* Abort frame */
		rx_buff->state = OUTSIDE_FRAME;
		break;
	default:
		/* 
		 *  Stuffed char, complement bit 5 of byte 
		 *  following CE, IrLAP p.114 
		 */
		byte ^= IRDA_TRANS;
		if (rx_buff->len < rx_buff->truesize)  {
			rx_buff->data[rx_buff->len++] = byte;
			rx_buff->fcs = irda_fcs(rx_buff->fcs, byte);
			rx_buff->state = INSIDE_FRAME;
		} else {
			IRDA_DEBUG(1, "%s(), rx buffer overflow\n", __FUNCTION__);
			rx_buff->state = OUTSIDE_FRAME;
		}
		break;
	}
}

/*
 * Function state_inside_frame (dev, byte)
 *
 *    Handle bytes received within a frame
 *
 */
static void state_inside_frame(struct net_device *dev, 
			       struct net_device_stats *stats,
			       iobuff_t *rx_buff, __u8 byte)
{
	int ret = 0; 

	switch (byte) {
	case BOF: /* New frame? */
		IRDA_DEBUG(1, "%s(), Discarding incomplete frame\n", __FUNCTION__);
		rx_buff->state = BEGIN_FRAME;
		irda_device_set_media_busy(dev, TRUE);
		break;
	case CE: /* Stuffed char */
		rx_buff->state = LINK_ESCAPE;
		break;
	case EOF: /* End of frame */
		rx_buff->state = OUTSIDE_FRAME;
		rx_buff->in_frame = FALSE;
		
		/* Test FCS and signal success if the frame is good */
		if (rx_buff->fcs == GOOD_FCS) {
			/* Deliver frame */
			async_bump(dev, stats, rx_buff->data, rx_buff->len);
			ret = TRUE;
			break;
		} else {
			/* Wrong CRC, discard frame!  */
			irda_device_set_media_busy(dev, TRUE); 

			IRDA_DEBUG(1, "%s(), crc error\n", __FUNCTION__);
			stats->rx_errors++;
			stats->rx_crc_errors++;
		}			
		break;
	default: /* Must be the next byte of the frame */
		if (rx_buff->len < rx_buff->truesize)  {
			rx_buff->data[rx_buff->len++] = byte;
			rx_buff->fcs = irda_fcs(rx_buff->fcs, byte);
		} else {
			IRDA_DEBUG(1, "%s(), Rx buffer overflow, aborting\n", __FUNCTION__);
			rx_buff->state = OUTSIDE_FRAME;
		}
		break;
	}
}


