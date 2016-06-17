/*****************************************************************************/
/*
 *      auerisdn_b.c  --  Auerswald PBX/System Telephone ISDN B-channel interface.
 *
 *      Copyright (C) 2002  Wolfgang Mües (wolfgang@iksw-muees.de)
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
 /*****************************************************************************/

#include <linux/isdnif.h>	/* ISDN constants */
#include <linux/netdevice.h>	/* skb functions */

#undef DEBUG			/* include debug macros until it's done */
#include <linux/usb.h>		/* standard usb header */

#include "auerisdn.h"
#include "auermain.h"

/*-------------------------------------------------------------------*/
/* ISDN B channel support defines                                    */
#define AUISDN_BC_1MS		8	/* Bytes per channel and ms */
#define AUISDN_BC_INC		4	/* change INT OUT size increment */
#define AUISDN_BCDATATHRESHOLD	48	/* for unsymmetric 2-B-channels */
#define AUISDN_TOGGLETIME	6	/* Timeout for unsymmetric serve */

/*-------------------------------------------------------------------*/
/* Debug support 						     */
#ifdef DEBUG
#define dump( desc, adr, len) \
do {			\
	unsigned int u;	\
	printk (KERN_DEBUG); \
	printk (desc); \
	for (u = 0; u < len; u++) \
		printk (" %02X", adr[u] & 0xFF); \
	printk ("\n"); \
} while (0)
#else
#define dump( desc, adr, len)
#endif

/*-------------------------------------------------------------------*/

/* Callback to L2 for HISAX */
/* This callback can be called from 3 sources:
   a) from hisax context (answer from a l2l1 function)
   b) from interrupt context (a B channel paket arrived, a B channel paket was sent)
   c) from kernel daemon context (probe/disconnecting)
*/
void auerisdn_b_l1l2(struct auerisdnbc *bc, int pr, void *arg)
{
	struct auerhisax *ahp;
	struct sk_buff *skb;

	/* do the callback */
	ahp = bc->cp->isdn.ahp;
	if (ahp) {
		ahp->hisax_b_if[bc->channel].ifc.l1l2(&ahp->
						      hisax_b_if[bc->
								 channel].
						      ifc, pr, arg);
	} else {
		dbg("auerisdn_b_l1l2 called without ahp");
		if (pr == (PH_DATA | INDICATION)) {
			skb = (struct sk_buff *) arg;
			if (skb) {
				skb_pull(skb, skb->len);
				dev_kfree_skb_any(skb);
			}
		}
	}
}

/* fill the INT OUT data buffer with new data */
/* Transfer buffer size to fill is in urbp->transfer_buffer_length */
static void auerisdn_bintbo_newdata(struct auerisdn *ip)
{
	unsigned long flags;
	struct urb *urbp = ip->intbo_urbp;
	struct auerisdnbc *bc = &ip->bc[0];	/* start with B-channel 0 */
	struct sk_buff *skb;
	unsigned char *ucp;
	int buf_size;
	int len;
	int bytes_sent;
	int i;

	/* FIXME: this algorithm is fixed to 2 B-channels */
	/* Which B channel should we serve? */
	if (ip->bc[1].mode != L1_MODE_NULL) {
		/* B channel 1 is used */
		if (bc->mode != L1_MODE_NULL) {
			/* both B-channels are used */
			if (ip->intbo_toggletimer) {
				/* simply toggling */
				ip->intbo_toggletimer--;
				i = ip->intbo_index ^ 1;	/* serve both channels equal */
			} else {
				/* search the B channel with the most demand of data */
				i = bc->txfree - ip->bc[1].txfree;
				if (i < -AUISDN_BCDATATHRESHOLD)
					i = 1;	/* B channel 1 needs more data */
				else if (i > AUISDN_BCDATATHRESHOLD)
					i = 0;	/* B channel 0 needs more data */
				else
					i = ip->intbo_index ^ 1;	/* serve both channels equal */
				if (i == ip->intbo_index)
					ip->intbo_toggletimer =
					    AUISDN_TOGGLETIME;
			}
			bc = &ip->bc[i];
			ip->intbo_index = i;
		} else {
			bc = &ip->bc[1];
		}
	}
	dbg("INTBO: Fill B%d with %d Bytes, %d Bytes free",
	    bc->channel + 1, urbp->transfer_buffer_length - AUH_SIZE,
	    bc->txfree);

	/* Fill the buffer with data */
	ucp = ip->intbo_bufp;
	*ucp++ = AUH_B1CHANNEL + bc->channel;	/* First byte is channel nr. */
	buf_size = urbp->transfer_buffer_length - AUH_SIZE;
	len = 0;
	while (len < buf_size) {
		spin_lock_irqsave(&bc->txskb_lock, flags);
		if ((skb = bc->txskb)) {
			/* dump ("raw tx data:", skb->data, skb->len); */
			if (bc->mode == L1_MODE_TRANS) {
				bytes_sent = buf_size - len;
				if (skb->len < bytes_sent)
					bytes_sent = skb->len;
				{	/* swap tx bytes */
					register unsigned char *src =
					    skb->data;
					unsigned int count;
					for (count = 0; count < bytes_sent;
					     count++)
						*ucp++ =
						    isdnhdlc_bit_rev_tab
						    [*src++];
				}
				len += bytes_sent;
				bc->lastbyte = skb->data[bytes_sent - 1];
			} else {
				int bs =
				    isdnhdlc_encode(&bc->outp_hdlc_state,
						    skb->data, skb->len,
						    &bytes_sent,
						    ucp, buf_size - len);
				/* dump ("hdlc data:", ucp, bs); */
				len += bs;
				ucp += bs;
			}
			skb_pull(skb, bytes_sent);

			if (!skb->len) {
				// Frame sent
				bc->txskb = NULL;
				spin_unlock_irqrestore(&bc->txskb_lock,
						       flags);
				auerisdn_b_l1l2(bc, PH_DATA | CONFIRM,
						(void *) skb->truesize);
				dev_kfree_skb_any(skb);
				continue;	//while
			}
		} else {
			if (bc->mode == L1_MODE_TRANS) {
				memset(ucp, bc->lastbyte, buf_size - len);
				ucp += buf_size - len;
				len = buf_size;
				/* dbg ("fill = 0xFF"); */
			} else {
				// Send flags
				int bs =
				    isdnhdlc_encode(&bc->outp_hdlc_state,
						    NULL, 0, &bytes_sent,
						    ucp, buf_size - len);
				/* dbg ("fill = 0x%02X", (int)*ucp); */
				len += bs;
				ucp += bs;
			}
		}
		spin_unlock_irqrestore(&bc->txskb_lock, flags);
	}
	/* dbg ("%d Bytes to TX buffer", len); */
}


/* INT OUT completion handler */
static void auerisdn_bintbo_complete(struct urb *urbp)
{
	struct auerisdn *ip = urbp->context;

	/* unlink completion? */
	if ((urbp->status == -ENOENT) || (urbp->status == -ECONNRESET)) {
		/* should we restart with another size? */
		if (ip->intbo_state == INTBOS_CHANGE) {
			dbg("state => RESTART");
			ip->intbo_state = INTBOS_RESTART;
		} else {
			/* set up variables for later restart */
			dbg("INTBO stopped");
			ip->intbo_state = INTBOS_IDLE;
		}
		/* nothing more to do */
		return;
	}

	/* other state != 0? */
	if (urbp->status) {
		warn("auerisdn_bintbo_complete: status = %d",
		     urbp->status);
		return;
	}

	/* Should we fill in new data? */
	if (ip->intbo_state == INTBOS_CHANGE) {
		dbg("state == INTBOS_CHANGE, no new data");
		return;
	}

	/* fill in new data */
	auerisdn_bintbo_newdata(ip);
}

/* set up the INT OUT URB the first time */
/* Don't start the URB */
static void auerisdn_bintbo_setup(struct auerisdn *ip, unsigned int len)
{
	ip->intbo_state = INTBOS_IDLE;
	FILL_INT_URB(ip->intbo_urbp, ip->usbdev,
		     usb_sndintpipe(ip->usbdev, ip->intbo_endp),
		     ip->intbo_bufp, len, auerisdn_bintbo_complete, ip,
		     ip->outInterval);
	ip->intbo_urbp->transfer_flags |= USB_ASYNC_UNLINK;
	ip->intbo_urbp->status = 0;
}

/* restart the INT OUT endpoint */
static void auerisdn_bintbo_restart(struct auerisdn *ip)
{
	struct urb *urbp = ip->intbo_urbp;
	int status;

	/* dbg ("auerisdn_intbo_restart"); */

	/* fresh restart */
	auerisdn_bintbo_setup(ip, ip->paketsize + AUH_SIZE);

	/* Fill in new data */
	auerisdn_bintbo_newdata(ip);

	/* restart the urb */
	ip->intbo_state = INTBOS_RUNNING;
	status = usb_submit_urb(urbp);
	if (status < 0) {
		err("can't submit INT OUT urb, status = %d", status);
		urbp->status = status;
		urbp->complete(urbp);
	}
}

/* change the size of the INT OUT endpoint */
static void auerisdn_bchange(struct auerisdn *ip, unsigned int paketsize)
{
	/* changing... */
	dbg("txfree[0] = %d, txfree[1] = %d, old size = %d, new size = %d",
	    ip->bc[0].txfree, ip->bc[1].txfree, ip->paketsize, paketsize);
	ip->paketsize = paketsize;

	if (paketsize == 0) {
		/* stop the INT OUT endpoint */
		dbg("stop unlinking INT out urb");
		ip->intbo_state = INTBOS_IDLE;
		usb_unlink_urb(ip->intbo_urbp);
		return;
	}
	if (ip->intbo_state != INTBOS_IDLE) {
		/* dbg ("unlinking INT out urb"); */
		ip->intbo_state = INTBOS_CHANGE;
		usb_unlink_urb(ip->intbo_urbp);
	} else {
		/* dbg ("restart immediately"); */
		auerisdn_bintbo_restart(ip);
	}
}

/* serve the outgoing B channel interrupt */
/* Called from the INT IN completion handler */
static void auerisdn_bserv(struct auerisdn *ip)
{
	struct auerisdnbc *bc;
	unsigned int u;
	unsigned int paketsize;

	/* should we start the INT OUT endpoint again? */
	if (ip->intbo_state == INTBOS_RESTART) {
		/* dbg ("Restart INT OUT from INT IN"); */
		auerisdn_bintbo_restart(ip);
		return;
	}
	/* no new calculation if change already in progress */
	if (ip->intbo_state == INTBOS_CHANGE)
		return;

	/* calculation of transfer parameters for INT OUT endpoint */
	paketsize = 0;
	for (u = 0; u < AUISDN_BCHANNELS; u++) {
		bc = &ip->bc[u];
		if (bc->mode != L1_MODE_NULL) {	/* B channel is active */
			unsigned int bpp = AUISDN_BC_1MS * ip->outInterval;
			if (bc->txfree < bpp) {	/* buffer is full, throttle */
				bc->txsize = bpp - AUISDN_BC_INC;
				paketsize += bpp - AUISDN_BC_INC;
			} else if (bc->txfree < bpp * 2) {
				paketsize += bc->txsize;	/* schmidt-trigger, continue */
			} else if (bc->txfree < bpp * 4) {	/* we are in synch */
				bc->txsize = bpp;
				paketsize += bpp;
			} else if (bc->txfree > bc->ofsize / 2) {/* we have to fill the buffer */
				bc->txsize = bpp + AUISDN_BC_INC;
				paketsize += bpp + AUISDN_BC_INC;
			} else {
				paketsize += bc->txsize;	/* schmidt-trigger, continue */
			}
		}
	}

	/* check if we have to change the paket size */
	if (paketsize != ip->paketsize)
		auerisdn_bchange(ip, paketsize);
}

/* Send activation/deactivation state to L2 */
static void auerisdn_bconf(struct auerisdnbc *bc)
{
	unsigned long flags;
	struct sk_buff *skb;

	if (bc->mode == L1_MODE_NULL) {
		auerisdn_b_l1l2(bc, PH_DEACTIVATE | INDICATION, NULL);
		/* recycle old txskb */
		spin_lock_irqsave(&bc->txskb_lock, flags);
		skb = bc->txskb;
		bc->txskb = NULL;
		spin_unlock_irqrestore(&bc->txskb_lock, flags);
		if (skb) {
			skb_pull(skb, skb->len);
			auerisdn_b_l1l2(bc, PH_DATA | CONFIRM,
					(void *) skb->truesize);
			dev_kfree_skb_any(skb);
		}
	} else {
		auerisdn_b_l1l2(bc, PH_ACTIVATE | INDICATION, NULL);
	}
}

/* B channel setup completion handler */
static void auerisdn_bmode_complete(struct urb *urb)
{
	struct auerswald *cp;
	struct auerbuf *bp = (struct auerbuf *) urb->context;
	struct auerisdnbc *bc;
	int channel;

	dbg("auerisdn_bmode_complete called");
	cp = ((struct auerswald *) ((char *) (bp->list) -
				    (unsigned
				     long) (&((struct auerswald *) 0)->
					    bufctl)));

	/* select the B-channel */
	channel = le16_to_cpu(bp->dr->wIndex);
	channel -= AUH_B1CHANNEL;
	if (channel < 0)
		goto rel;
	if (channel >= AUISDN_BCHANNELS)
		goto rel;
	bc = &cp->isdn.bc[channel];

	/* Check for success */
	if (urb->status) {
		err("complete with non-zero status: %d", urb->status);
	} else {
		bc->mode = *bp->bufp;
	}
	/* Signal current mode to L2 */
	auerisdn_bconf(bc);

	/* reuse the buffer */
      rel:auerbuf_releasebuf(bp);

	/* Wake up all processes waiting for a buffer */
	wake_up(&cp->bufferwait);
}

/* Setup a B channel transfer mode */
static void auerisdn_bmode(struct auerisdnbc *bc, unsigned int mode)
{
	struct auerswald *cp = bc->cp;
	struct auerbuf *bp;
	int ret;

	/* don't allow activation on disconnect */
	if (cp->disconnecting) {
		mode = L1_MODE_NULL;

		/* Else check if something changed */
	} else if (bc->mode != mode) {
		if ((mode != L1_MODE_NULL) && (mode != L1_MODE_TRANS)) {
			/* init RX hdlc decoder */
			dbg("rcv init");
			isdnhdlc_rcv_init(&bc->inp_hdlc_state, 0);
			/* init TX hdlc decoder */
			dbg("out init");
			isdnhdlc_out_init(&bc->outp_hdlc_state, 0, 0);
		}
		/* stop ASAP */
		if (mode == L1_MODE_NULL)
			bc->mode = mode;
		if ((bc->mode == L1_MODE_NULL) || (mode == L1_MODE_NULL)) {
			/* Activation or deactivation required */

			/* get a buffer for the command */
			bp = auerbuf_getbuf(&cp->bufctl);
			/* if no buffer available: can't change the mode */
			if (!bp) {
				err("auerisdn_bmode: no data buffer available");
				return;
			}

			/* fill the control message */
			bp->dr->bRequestType = AUT_WREQ;
			bp->dr->bRequest = AUV_CHANNELCTL;
			if (mode != L1_MODE_NULL)
				bp->dr->wValue = cpu_to_le16(1);
			else
				bp->dr->wValue = cpu_to_le16(0);
			bp->dr->wIndex =
			    cpu_to_le16(AUH_B1CHANNEL + bc->channel);
			bp->dr->wLength = cpu_to_le16(0);
			*bp->bufp = mode;
			FILL_CONTROL_URB(bp->urbp, cp->usbdev,
					 usb_sndctrlpipe(cp->usbdev, 0),
					 (unsigned char *) bp->dr,
					 bp->bufp, 0,
					 (usb_complete_t)
					 auerisdn_bmode_complete, bp);

			/* submit the control msg */
			ret =
			    auerchain_submit_urb(&cp->controlchain,
						 bp->urbp);
			if (ret) {
				bp->urbp->status = ret;
				auerisdn_bmode_complete(bp->urbp);
			}
			return;
		}
	}
	/* new mode is set */
	bc->mode = mode;

	/* send confirmation to L2 */
	auerisdn_bconf(bc);
}

/* B-channel transfer function L2->L1 */
void auerisdn_b_l2l1(struct hisax_if *ifc, int pr, void *arg,
		     unsigned int channel)
{
	struct auerhisax *ahp;
	struct auerisdnbc *bc;
	struct auerswald *cp;
	struct sk_buff *skb;
	unsigned long flags;
	int mode;

	cp = NULL;
	ahp = (struct auerhisax *) ifc->priv;
	if (ahp)
		cp = ahp->cp;
	if (cp && !cp->disconnecting) {
		/* normal execution */
		bc = &cp->isdn.bc[channel];
		switch (pr) {
		case PH_ACTIVATE | REQUEST:	/* activation request */
			mode = (int) arg;	/* one of the L1_MODE constants */
			dbg("B%d, PH_ACTIVATE_REQUEST Mode = %d",
			    bc->channel + 1, mode);
			auerisdn_bmode(bc, mode);
			break;
		case PH_DEACTIVATE | REQUEST:	/* deactivation request */
			dbg("B%d, PH_DEACTIVATE_REQUEST", bc->channel + 1);
			auerisdn_bmode(bc, L1_MODE_NULL);
			break;
		case PH_DATA | REQUEST:	/* Transmit data request */
			skb = (struct sk_buff *) arg;
			spin_lock_irqsave(&bc->txskb_lock, flags);
			if (bc->txskb) {
				err("Overflow in B channel TX");
				skb_pull(skb, skb->len);
				dev_kfree_skb_any(skb);
			} else {
				if (cp->disconnecting
				    || (bc->mode == L1_MODE_NULL)) {
					skb_pull(skb, skb->len);
					spin_unlock_irqrestore(&bc->
							       txskb_lock,
							       flags);
					auerisdn_b_l1l2(bc,
							PH_DATA | CONFIRM,
							(void *) skb->
							truesize);
					dev_kfree_skb_any(skb);
					goto next;
				} else
					bc->txskb = skb;
			}
			spin_unlock_irqrestore(&bc->txskb_lock, flags);
		      next:break;
		default:
			warn("pr %#x\n", pr);
			break;
		}
	} else {
		/* hisax interface is down */
		switch (pr) {
		case PH_ACTIVATE | REQUEST:	/* activation request */
			dbg("B channel: PH_ACTIVATE | REQUEST with interface down");
			/* don't answer this request! Endless... */
			break;
		case PH_DEACTIVATE | REQUEST:	/* deactivation request */
			dbg("B channel: PH_DEACTIVATE | REQUEST with interface down");
			ifc->l1l2(ifc, PH_DEACTIVATE | INDICATION, NULL);
			break;
		case PH_DATA | REQUEST:	/* Transmit data request */
			dbg("B channel: PH_DATA | REQUEST with interface down");
			skb = (struct sk_buff *) arg;
			/* free data buffer */
			if (skb) {
				skb_pull(skb, skb->len);
				dev_kfree_skb_any(skb);
			}
			/* send confirmation back to layer 2 */
			ifc->l1l2(ifc, PH_DATA | CONFIRM, NULL);
			break;
		default:
			warn("pr %#x\n", pr);
			break;
		}
	}
}

/* Completion handler for B channel input endpoint */
void auerisdn_intbi_complete(struct urb *urb)
{
	unsigned int bytecount;
	unsigned char *ucp;
	int channel;
	unsigned int syncbit;
	unsigned int syncdata;
	struct auerisdnbc *bc;
	struct sk_buff *skb;
	int count;
	int status;
	struct auerswald *cp = (struct auerswald *) urb->context;
	/* do not respond to an error condition */
	if (urb->status != 0) {
		dbg("nonzero URB status = %d", urb->status);
		return;
	}
	if (cp->disconnecting)
		return;

	/* Parse and extract the header information */
	bytecount = urb->actual_length;
	ucp = cp->isdn.intbi_bufp;
	if (!bytecount)
		return;		/* no data */
	channel = *ucp & AUH_TYPEMASK;
	syncbit = *ucp & AUH_SYNC;
	ucp++;
	bytecount--;
	channel -= AUH_B1CHANNEL;
	if (channel < 0)
		return;		/* unknown data channel, no B1,B2 */
	if (channel >= AUISDN_BCHANNELS)
		return;		/* unknown data channel, no B1,B2 */
	bc = &cp->isdn.bc[channel];
	if (!bytecount)
		return;
	/* Calculate amount of bytes which are free in tx device buffer */
	bc->txfree = ((255 - *ucp++) * bc->ofsize) / 256;
	/* dbg ("%d Bytes free in TX buffer", bc->txfree); */
	bytecount--;

	/* Next Byte: TX sync information */
	if (syncbit) {
		if (!bytecount)
			goto int_tx;
		syncdata = *ucp++;
		dbg("Sync data = %d", syncdata);
		bytecount--;
	}
	/* The rest of the paket is plain data */
	if (!bytecount)
		goto int_tx;
	/* dump ("RX Data is:", ucp, bytecount); */

	/* Send B channel data to upper layers */
	while (bytecount > 0) {
		if (bc->mode == L1_MODE_NULL) {
			/* skip the data. Nobody needs them */
			status = 0;
			bytecount = 0;
		} else if (bc->mode == L1_MODE_TRANS) {
			{	/* swap rx bytes */
				register unsigned char *dest = bc->rxbuf;
				status = bytecount;
				for (; bytecount; bytecount--)
					*dest++ =
					    isdnhdlc_bit_rev_tab[*ucp++];
			}

		} else {
			status = isdnhdlc_decode(&bc->inp_hdlc_state, ucp,
						 bytecount, &count,
						 bc->rxbuf, AUISDN_RXSIZE);
			ucp += count;
			bytecount -= count;
		}
		if (status > 0) {
			/* Good frame received */
			if (!(skb = dev_alloc_skb(status))) {
				warn("receive out of memory");
				break;
			}
			memcpy(skb_put(skb, status), bc->rxbuf, status);
			/* dump ("HDLC Paket", bc->rxbuf, status); */
			auerisdn_b_l1l2(bc, PH_DATA | INDICATION, skb);
			/* these errors may actually happen at the start of a connection! */
		} else if (status == -HDLC_CRC_ERROR) {
			dbg("CRC error");
		} else if (status == -HDLC_FRAMING_ERROR) {
			dbg("framing error");
		} else if (status == -HDLC_LENGTH_ERROR) {
			dbg("length error");
		}
	}

      int_tx:			/* serve the outgoing B channel */
	auerisdn_bserv(&cp->isdn);
}

/* Stop the B channel activity. The device is disconnecting */
/* This function is called after cp->disconnecting is true */
unsigned int auerisdn_b_disconnect(struct auerswald *cp)
{
	unsigned int u;
	struct auerisdnbc *bc;
	unsigned int result = 0;

	/* Close the B channels */
	for (u = 0; u < AUISDN_BCHANNELS; u++) {
		bc = &cp->isdn.bc[u];
		if (bc->mode != L1_MODE_NULL) {	/* B channel is active */
			auerisdn_bmode(bc, L1_MODE_NULL);
			result = 1;
		}
	}
	/* return 1 if there is B channel traffic */
	return result;
}
