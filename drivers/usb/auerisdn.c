/*****************************************************************************/
/*
 *      auerisdn.c  --  Auerswald PBX/System Telephone ISDN interface.
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

#include <linux/isdnif.h>
#include <linux/netdevice.h>
#include <linux/sched.h>

#undef	DEBUG			/* include debug macros until it's done */
#include <linux/usb.h>

#include "auerisdn.h"
#include "auermain.h"

/*-------------------------------------------------------------------*/
/* ISDN support defines                                              */
#define AUISDN_TEI	64	/* use a constant TEI */

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
/* Hisax Interface.     					     */

/* The interface to hisax is long-lasting because hisax_unregister()
   don't work well in Linux 2.4.x. So we have to hold each registered
   hisax interface until driver removal. */
static struct auerhisax auerhisax_table[AUER_MAX_DEVICES];


/*-------------------------------------------------------------------*/

/* Callback to L2 for HISAX */
/* This callback can be called from 3 sources:
   a) from hisax context (answer from a l2l1 function)
   b) from interrupt context (a D channel paket arrived)
   c) from kernel daemon context (probe/disconnecting)
*/
static void auerisdn_d_l1l2(struct auerisdn *ip, int pr, void *arg)
{
	struct sk_buff *skb;
	struct auerhisax *ahp;

	/* do the callback */
	ahp = ip->ahp;
	if (ahp) {
		ahp->hisax_d_if.ifc.l1l2(&ahp->hisax_d_if.ifc, pr, arg);
	} else {
		dbg("auerisdn_d_l1l2 with ahp == NULL");
		if (pr == (PH_DATA | INDICATION)) {
			skb = (struct sk_buff *) arg;
			if (skb) {
				skb_pull(skb, skb->len);
				dev_kfree_skb_any(skb);
			}
		}
	}
}


/* D-Channel sending completion function */
static void auerisdn_dcw_complete(struct urb *urb)
{
	struct auerbuf *bp = (struct auerbuf *) urb->context;
	struct auerswald *cp =
	    ((struct auerswald *) ((char *) (bp->list) -
				   (unsigned
				    long) (&((struct auerswald *) 0)->
					   bufctl)));

	dbg("auerisdn_dcw_complete with status %d", urb->status);

	/* reuse the buffer */
	auerbuf_releasebuf(bp);

	/* Wake up all processes waiting for a buffer */
	wake_up(&cp->bufferwait);
}


/* Translate non-ETSI ISDN messages from the device */
static void auerisdn_translate_incoming(struct auerswald *cp,
					unsigned char *msg,
					unsigned int len)
{
	struct auerbuf *bp;
	int ret;

	/* Translate incomming CONNECT -> CONNECT_ACK */
	/* Format:   0   1    2     3     4        5        6    7      */
	/*         SAPI TEI TXSEQ RXSEQ PID=08 CREFLEN=01 CREF MSG=7 ...*/
	/* CREF.7 == 0 -> Incoming Call                                 */

	/* Check for minimum length */
	if (len < 8)
		return;

	/* Check for a CONNECT, call originated from device */
	if (((msg[6] & 0x80) == 0) && (msg[7] == 0x07)) {
		dbg("false CONNECT from device found");
		/* change into CONNECT_ACK */
		msg[7] = 0x0F;

		/* Send a CONNECT_ACK back to the device */

		/* get a new data buffer */
		bp = auerbuf_getbuf(&cp->bufctl);
		if (!bp) {
			warn("no auerbuf free");
			return;
		}

		/* Form a CONNECT ACK */
		bp->bufp[0] =
		    cp->isdn.dchannelservice.id | AUH_DIRECT | AUH_UNSPLIT;
		bp->bufp[1] = 0x08;
		bp->bufp[2] = 0x01;
		bp->bufp[3] = msg[6] | 0x80;
		bp->bufp[4] = 0x0F;

		/* Set the transfer Parameters */
		bp->len = 5;
		bp->dr->bRequestType = AUT_WREQ;
		bp->dr->bRequest = AUV_WBLOCK;
		bp->dr->wValue = cpu_to_le16(0);
		bp->dr->wIndex =
		    cpu_to_le16(cp->isdn.dchannelservice.
				id | AUH_DIRECT | AUH_UNSPLIT);
		bp->dr->wLength = cpu_to_le16(5);
		FILL_CONTROL_URB(bp->urbp, cp->usbdev,
				 usb_sndctrlpipe(cp->usbdev, 0),
				 (unsigned char *) bp->dr, bp->bufp, 5,
				 auerisdn_dcw_complete, bp);
		/* up we go */
		ret = auerchain_submit_urb(&cp->controlchain, bp->urbp);
		if (ret)
			auerisdn_dcw_complete(bp->urbp);
		else
			dbg("auerisdn_translate: Write OK");
	}
	/* Check for a DISCONNECT and change to RELEASE */
	if (msg[7] == 0x45) {
		dbg("DISCONNECT changed to RELEASE");
		msg[7] = 0x4D;
		return;
	}
}


/* a D-channel paket arrived from the device */
static void auerisdn_dispatch_dc(struct auerscon *scp, struct auerbuf *bp)
{
	struct sk_buff *skb;
	struct auerhisax *ahp;
	struct auerswald *cp =
	    ((struct auerswald *) ((char *) (scp) -
				   (unsigned
				    long) (&((struct auerswald *) 0)->isdn.
					   dchannelservice)));
	unsigned char *sp;
	unsigned int l2_index;
	unsigned char c;
	unsigned char l2_header[10];
	unsigned long flags;

	dump("D-Channel paket arrived:", bp->bufp, bp->len);
	if (cp->disconnecting)
		return;

	/* add a self-generated L2 message header */
	l2_index = 0;
	l2_header[l2_index++] = 0x02;	/* SAPI 0, C/R = 1 */

	/* Parse the L3 message */
	sp = bp->bufp + AUH_SIZE;

	c = *sp++;		/* Protocol discriminator */
	if (c != 0x08) {
		warn("D channel paket is not ETSI");
		return;
	}
	c = *sp++;		/* Call Reference length byte */
	sp += c;		/* Skip Call Reference */

	/* translate charge IEs */
	/* Format of Auerswald Header:
	   0x32 len=0x0B 0xFF 0xFF 0x73 len=0x07 0x27 */
	/* Format of IE2_UNIT:
	   0x49 len=0x04 uu1 uu2 uu3 uu4 */
	/* Translate into: (?? Bytes)
	   0x1C Facility
	   0x?? restlen
	   0x91 Sup. Services
	   0xA1 Invoke
	   0x?? restlen
	   0x02 Invoke ID Tag
	   0x02 Invoke ID len
	   0x12 Invoke ID = 0x1234
	   0x34
	   0x02 OP Value Tag
	   0x01 Length of OPvalue
	   0x24 OpValue = AOCE
	   0x30 Universal Constructor Sequence
	   0x?? restlen
	   0x30 Universal Constructor Sequence
	   0x?? restlen
	   0xA1 Context Specific Constructor Recorded Units List
	   0x?? restlen
	   0x30 Universal Constructor Sequence
	   0x?? restlen
	   0x02 Universal Primitive Integer
	   0x?? len from IE2_UNIT
	   uu1  Recorded Units List
	   uu2
	   uu3
	   uu4
	 */
	{
		unsigned char *ucp = sp;	// pointer to start of msg
		int l = bp->len;	// length until EOP
		unsigned char alen;	// length of auerswald msg
		l -= (int) (ucp - bp->bufp);
		// scan for Auerswald Header
		for (; l >= 9; l--, ucp++) {	// 9 = minimal length of auerswald msg
			if (ucp[0] != 0x32)
				continue;
			if (ucp[2] != 0xFF)
				continue;
			if (ucp[3] != 0xFF)
				continue;
			if (ucp[4] != 0x73)
				continue;
			if (ucp[6] != 0x27)
				continue;
			// Auerswald Header found. Is it units?
			dbg("Auerswald msg header found");
			alen = ucp[1] + 2;
			if (ucp[7] == 0x49) {
				// yes
				unsigned char ul = ucp[8] + 1;	// length of charge integer
				unsigned char charge[32];
				// Copy charge info into new buffer
				unsigned char *xp = &ucp[8];
				int count;
				for (count = 0; count < ul; count++)
					charge[count] = *xp++;
				// Erase auerswald msg
				count = l - alen;
				xp = ucp;
				for (; count; count--, xp++)
					xp[0] = xp[alen];
				l -= alen;
				bp->len -= alen;
				// make room for new message
				count = l;
				xp = &ucp[l - 1];
				for (; count; count--, xp--);
				xp[21 + ul] = xp[0];
				l += (21 + ul);
				bp->len += (21 + ul);
				// insert IE header
				ucp[0] = 0x1C;
				ucp[1] = 19 + ul;
				ucp[2] = 0x91;
				ucp[3] = 0xA1;
				ucp[4] = 16 + ul;
				ucp[5] = 0x02;
				ucp[6] = 0x02;
				ucp[7] = 0x12;
				ucp[8] = 0x34;
				ucp[9] = 0x02;
				ucp[10] = 0x01;
				ucp[11] = 0x24;
				ucp[12] = 0x30;
				ucp[13] = 7 + ul;
				ucp[14] = 0x30;
				ucp[15] = 5 + ul;
				ucp[16] = 0xA1;
				ucp[17] = 3 + ul;
				ucp[18] = 0x30;
				ucp[19] = 1 + ul;
				ucp[20] = 0x02;
				// Insert charge units
				xp = &ucp[21];
				for (count = 0; count < ul; count++)
					*xp++ = charge[count];
				dump("Rearranged message:", bp->bufp,
				     bp->len);
				break;
			} else {
				// we can't handle something else, erase it
				int count = l - alen;
				unsigned char *xp = ucp;
				for (; count; count--, xp++)
					xp[0] = xp[alen];
				l -= alen;
				bp->len -= alen;
				dump("Shortened message:", bp->bufp,
				     bp->len);
			}
		}
	}


	c = *sp;		/* Message type */
	if (c == 0x05) {
		/* SETUP. Use an UI frame */
		dbg("SETUP");
		l2_header[l2_index++] = 0xFF;	/* TEI 127 */
		l2_header[l2_index++] = 0x03;	/* UI control field */
		skb = dev_alloc_skb(bp->len - AUH_SIZE + l2_index);
	} else {
		/* use an I frame */
		dbg("I Frame");
		l2_header[l2_index++] = (AUISDN_TEI << 1) | 0x01;	/* TEI byte */
		skb = dev_alloc_skb(bp->len - AUH_SIZE + l2_index + 2);
		if (skb) {
			ahp = cp->isdn.ahp;
			if (!ahp) {
				err("ahp == NULL");
				return;
			}
			spin_lock_irqsave(&ahp->seq_lock, flags);
			l2_header[l2_index++] = ahp->txseq;	/* transmitt sequence number */
			l2_header[l2_index++] = ahp->rxseq;	/* receive sequence number */
			ahp->txseq += 2;			/* next paket gets next number */
			spin_unlock_irqrestore(&ahp->seq_lock, flags);
		}
	}
	if (!skb) {
		err("no memory - skipped");
		return;
	}
	sp = skb_put(skb, bp->len - AUH_SIZE + l2_index);
	/* Add L2 header */
	memcpy(sp, l2_header, l2_index);
	memcpy(sp + l2_index, bp->bufp + AUH_SIZE, bp->len - AUH_SIZE);
	/* Translate false messages */
	auerisdn_translate_incoming(cp, sp, bp->len - AUH_SIZE + l2_index);
	/* Send message to L2 */
	auerisdn_d_l1l2(&cp->isdn, PH_DATA | INDICATION, skb);
}

/* D-channel is closed because the device is removed */
/* This is a no-op because ISDN close is handled different */
static void auerisdn_disconnect_dc(struct auerscon *scp)
{
}


/* confirmation helper function. */
static void auerisdn_d_confirmskb(struct auerswald *cp,
				  struct sk_buff *skb)
{
	/* free the skb */
	if (skb) {
		skb_pull(skb, skb->len);
		dev_kfree_skb_any(skb);
	}

	/* confirm the sending of data */
	dbg("Confirm PH_DATA");
	auerisdn_d_l1l2(&cp->isdn, PH_DATA | CONFIRM, NULL);
}

/* D-channel transfer function L2->L1 */
static void auerisdn_d_l2l1(struct hisax_if *hisax_d_if, int pr, void *arg)
{
	struct auerhisax *ahp;
	struct sk_buff *skb;
	unsigned int len;
	int ret;
	struct auerbuf *bp;
	struct auerswald *cp;
	unsigned long flags;
	unsigned int l2_index;
	unsigned char c;
	unsigned char l2_header[32];
	unsigned char *sp;

	dbg("hisax D-Channel l2l1 called");

	/* Get reference to auerhisax struct */
	cp = NULL;
	ahp = hisax_d_if->priv;
	if (ahp)
		cp = ahp->cp;
	if (cp && !cp->disconnecting) {
		/* normal usage */
		switch (pr) {
		case PH_ACTIVATE | REQUEST:	/* activation request */
			dbg("Activation Request");
			cp->isdn.dc_activated = 1;
			/* send activation back to layer 2 */
			auerisdn_d_l1l2(&cp->isdn,
					PH_ACTIVATE | INDICATION, NULL);
			break;
		case PH_DEACTIVATE | REQUEST:	/* deactivation request */
			dbg("Deactivation Request");
			cp->isdn.dc_activated = 0;
			/* send deactivation back to layer 2 */
			auerisdn_d_l1l2(&cp->isdn,
					PH_DEACTIVATE | INDICATION, NULL);
			break;
		case PH_DATA | REQUEST:	/* Transmit data request */
			skb = (struct sk_buff *) arg;
			len = skb->len;
			l2_index = 0;
			sp = skb->data;
			dump("Data Request:", sp, len);

			/* Parse the L2 header */
			if (!len)
				goto phd_free;
			c = *sp++;	/* SAPI */
			l2_header[l2_index++] = c;
			len--;
			if (!len)
				goto phd_free;
			c = *sp++;	/* TEI */
			l2_header[l2_index++] = c;
			len--;
			if (!len)
				goto phd_free;
			c = *sp++;	/* Control Field, Byte 1 */
			len--;
			if (!(c & 0x01)) {
				/* I FRAME */
				dbg("I Frame");
				if (!len)
					goto phd_free;
				spin_lock_irqsave(&ahp->seq_lock, flags);
				ahp->rxseq = c + 2;	/* store new sequence info */
				spin_unlock_irqrestore(&ahp->seq_lock,
						       flags);
				sp++;	/* skip Control Field, Byte 2 */
				len--;
				/* Check for RELEASE command */
				/* and change to RELEASE_COMPLETE */
				if (sp[3] == 0x4D)
					sp[3] = 0x5A;
				goto phd_send;
			}
			/* check the frame type */
			switch (c) {
			case 0x03:	/* UI frame */
				dbg("UI Frame");
				if (l2_header[0] == 0xFC) {
					dbg("TEI Managment");
					l2_header[0] = 0xFE;	/* set C/R bit in answer */
					l2_header[l2_index++] = c;	/* Answer is UI frame */
					if (!len)
						break;
					c = *sp++;	/* Managment ID */
					len--;
					if (c != 0x0F)
						break;
					l2_header[l2_index++] = c;
					/* Read Reference Number */
					if (!len)
						break;
					l2_header[l2_index++] = *sp++;
					len--;
					if (!len)
						break;
					l2_header[l2_index++] = *sp++;
					len--;
					if (!len)
						break;
					c = *sp++;	/* Message Type */
					len--;
					switch (c) {
					case 0x01:	/* Identity Request */
						dbg("Identity Request");
						l2_header[l2_index++] = 0x02;	/* Identity Assign */
						l2_header[l2_index++] =
						    (AUISDN_TEI << 1) |
						    0x01;
						goto phd_answer;
					default:
						dbg("Unhandled TEI Managment %X", (int) c);
						break;
					}
					// throw away
					goto phd_free;
				}
				/* else send UI frame out */
				goto phd_send;
			case 0x01:	/* RR frame */
			case 0x05:	/* RNR frame */
				dbg("RR/RNR Frame");
				if (!len)
					break;
				c = *sp++;	/* Control Field, Byte 2 */
				len--;
				if (!(c & 0x01))
					break;	/* P/F = 1 in commands */
				if (l2_header[0] & 0x02)
					break;	/* C/R = 0 from TE */
				dbg("Send RR as answer");
				l2_header[l2_index++] = 0x01;	/* send an RR as Answer */
				spin_lock_irqsave(&ahp->seq_lock, flags);
				l2_header[l2_index++] = ahp->rxseq | 0x01;
				spin_unlock_irqrestore(&ahp->seq_lock,
						       flags);
				goto phd_answer;
			case 0x7F:	/* SABME */
				dbg("SABME");
				spin_lock_irqsave(&ahp->seq_lock, flags);
				ahp->txseq = 0;
				ahp->rxseq = 0;
				spin_unlock_irqrestore(&ahp->seq_lock,
						       flags);
				l2_header[l2_index++] = 0x73;	/* UA */
				goto phd_answer;
			case 0x53:	/* DISC */
				dbg("DISC");
				/* Send back a UA */
				l2_header[l2_index++] = 0x73;	/* UA */
				goto phd_answer;
			default:
				dbg("Unhandled L2 Message %X", (int) c);
				break;
			}
			/* all done */
			goto phd_free;

			/* we have to generate a local answer */
			/* first, confirm old message, free old skb */
		      phd_answer:auerisdn_d_confirmskb(cp,
					      skb);

			/* allocate a new skbuff */
			skb = dev_alloc_skb(l2_index);
			if (!skb) {
				err("no memory for new skb");
				break;
			}
			dump("local answer to L2 is:", l2_header,
			     l2_index);
			memcpy(skb_put(skb, l2_index), l2_header,
			       l2_index);
			auerisdn_d_l1l2(&cp->isdn, PH_DATA | INDICATION,
					skb);
			break;

			/* we have to send the L3 message out */
		      phd_send:if (!len)
				goto phd_free;	/* no message left */

			/* get a new data buffer */
			bp = auerbuf_getbuf(&cp->bufctl);
			if (!bp) {
				warn("no auerbuf free");
				goto phd_free;
			}
			/* protect against too big write requests */
			/* Should not happen */
			if (len > cp->maxControlLength) {
				err("too long D-channel paket truncated");
				len = cp->maxControlLength;
			}

			/* Copy the data */
			memcpy(bp->bufp + AUH_SIZE, sp, len);

			/* set the header byte */
			*(bp->bufp) =
			    cp->isdn.dchannelservice.
			    id | AUH_DIRECT | AUH_UNSPLIT;

			/* Set the transfer Parameters */
			bp->len = len + AUH_SIZE;
			bp->dr->bRequestType = AUT_WREQ;
			bp->dr->bRequest = AUV_WBLOCK;
			bp->dr->wValue = cpu_to_le16(0);
			bp->dr->wIndex =
			    cpu_to_le16(cp->isdn.dchannelservice.
					id | AUH_DIRECT | AUH_UNSPLIT);
			bp->dr->wLength = cpu_to_le16(len + AUH_SIZE);
			FILL_CONTROL_URB(bp->urbp, cp->usbdev,
					 usb_sndctrlpipe(cp->usbdev, 0),
					 (unsigned char *) bp->dr,
					 bp->bufp, len + AUH_SIZE,
					 auerisdn_dcw_complete, bp);
			/* up we go */
			ret =
			    auerchain_submit_urb(&cp->controlchain,
						 bp->urbp);
			if (ret)
				auerisdn_dcw_complete(bp->urbp);
			else
				dbg("auerisdn_dwrite: Write OK");
			/* confirm message, free skb */
		      phd_free:auerisdn_d_confirmskb(cp,
					      skb);
			break;

		default:
			warn("pr %#x\n", pr);
			break;
		}
	} else {
		/* hisax interface is down */
		switch (pr) {
		case PH_ACTIVATE | REQUEST:	/* activation request */
			dbg("D channel PH_ACTIVATE | REQUEST with interface down");
			/* don't answer this request! Endless... */
			break;
		case PH_DEACTIVATE | REQUEST:	/* deactivation request */
			dbg("D channel PH_DEACTIVATE | REQUEST with interface down");
			hisax_d_if->l1l2(hisax_d_if,
					 PH_DEACTIVATE | INDICATION, NULL);
			break;
		case PH_DATA | REQUEST:	/* Transmit data request */
			dbg("D channel PH_DATA | REQUEST with interface down");
			skb = (struct sk_buff *) arg;
			/* free data buffer */
			if (skb) {
				skb_pull(skb, skb->len);
				dev_kfree_skb_any(skb);
			}
			/* send confirmation back to layer 2 */
			hisax_d_if->l1l2(hisax_d_if, PH_DATA | CONFIRM,
					 NULL);
			break;
		default:
			warn("pr %#x\n", pr);
			break;
		}
	}
}


/* Completion function for D channel open */
static void auerisdn_dcopen_complete(struct urb *urbp)
{
	struct auerbuf *bp = (struct auerbuf *) urbp->context;
	struct auerswald *cp =
	    ((struct auerswald *) ((char *) (bp->list) -
				   (unsigned
				    long) (&((struct auerswald *) 0)->
					   bufctl)));
	dbg("auerisdn_dcopen_complete called");

	auerbuf_releasebuf(bp);

	/* Wake up all processes waiting for a buffer */
	wake_up(&cp->bufferwait);
}


/* Open the D-channel once more */
static void auerisdn_dcopen(unsigned long data)
{
	struct auerswald *cp = (struct auerswald *) data;
	struct auerbuf *bp;
	int ret;

	if (cp->disconnecting)
		return;
	dbg("auerisdn_dcopen running");

	/* get a buffer for the command */
	bp = auerbuf_getbuf(&cp->bufctl);
	/* if no buffer available: can't change the mode */
	if (!bp) {
		err("auerisdn_dcopen: no data buffer available");
		return;
	}

	/* fill the control message */
	bp->dr->bRequestType = AUT_WREQ;
	bp->dr->bRequest = AUV_CHANNELCTL;
	bp->dr->wValue = cpu_to_le16(1);
	bp->dr->wIndex = cpu_to_le16(0);
	bp->dr->wLength = cpu_to_le16(0);
	FILL_CONTROL_URB(bp->urbp, cp->usbdev,
			 usb_sndctrlpipe(cp->usbdev, 0),
			 (unsigned char *) bp->dr, bp->bufp, 0,
			 (usb_complete_t) auerisdn_dcopen_complete, bp);

	/* submit the control msg */
	ret = auerchain_submit_urb(&cp->controlchain, bp->urbp);
	dbg("dcopen submitted");
	if (ret) {
		bp->urbp->status = ret;
		auerisdn_dcopen_complete(bp->urbp);
	}
	return;
}


/* Initialize the isdn related items in struct auerswald */
void auerisdn_init_dev(struct auerswald *cp)
{
	unsigned int u;
	cp->isdn.dchannelservice.id = AUH_UNASSIGNED;
	cp->isdn.dchannelservice.dispatch = auerisdn_dispatch_dc;
	cp->isdn.dchannelservice.disconnect = auerisdn_disconnect_dc;
	init_timer(&cp->isdn.dcopen_timer);
	cp->isdn.dcopen_timer.data = (unsigned long) cp;
	cp->isdn.dcopen_timer.function = auerisdn_dcopen;
	for (u = 0; u < AUISDN_BCHANNELS; u++) {
		cp->isdn.bc[u].cp = cp;
		cp->isdn.bc[u].mode = L1_MODE_NULL;
		cp->isdn.bc[u].channel = u;
		spin_lock_init(&cp->isdn.bc[u].txskb_lock);
	}
}


/* Connect to the HISAX interface. Returns 0 if successfull */
int auerisdn_probe(struct auerswald *cp)
{
	struct hisax_b_if *b_if[AUISDN_BCHANNELS];
	struct usb_endpoint_descriptor *ep;
	struct auerhisax *ahp;
	DECLARE_WAIT_QUEUE_HEAD(wqh);
	unsigned int u;
	unsigned char *ucp;
	unsigned int first_time;
	int ret;

	/* First allocate resources, then register hisax interface */

	/* Allocate RX buffers */
	for (u = 0; u < AUISDN_BCHANNELS; u++) {
		if (!cp->isdn.bc[u].rxbuf) {
			cp->isdn.bc[u].rxbuf =
			    (char *) kmalloc(AUISDN_RXSIZE, GFP_KERNEL);
			if (!cp->isdn.bc[u].rxbuf) {
				err("can't allocate buffer for B channel RX data");
				return -1;
			}
		}
	}

	/* Read out B-Channel output fifo size */
	ucp = kmalloc(32, GFP_KERNEL);
	if (!ucp) {
		err("Out of memory");
		return -3;
	}
	ret = usb_control_msg(cp->usbdev,			/* pointer to device */
			      usb_rcvctrlpipe(cp->usbdev, 0),	/* pipe to control endpoint */
			      AUV_GETINFO,			/* USB message request value */
			      AUT_RREQ,				/* USB message request type value */
			      0,				/* USB message value */
			      AUDI_OUTFSIZE,			/* USB message index value */
			      ucp,				/* pointer to the receive buffer */
			      32,				/* length of the buffer */
			      HZ * 2);				/* time to wait for the message to complete before timing out */
	if (ret < 4) {
		kfree(ucp);
		err("can't read TX Fifo sizes for B1,B2");
		return -4;
	}
	for (u = 0; u < AUISDN_BCHANNELS; u++) {
		ret = le16_to_cpup(ucp + u * 2);
		cp->isdn.bc[u].ofsize = ret;
		cp->isdn.bc[u].txfree = ret;
	}
	kfree(ucp);
	for (u = 0; u < AUISDN_BCHANNELS; u++) {
		dbg("B%d buffer size is %d", u, cp->isdn.bc[u].ofsize);
	}

	/* get the B channel output INT size */
	cp->isdn.intbo_endp = AU_IRQENDPBO;
	ep = usb_epnum_to_ep_desc(cp->usbdev, USB_DIR_OUT | AU_IRQENDPBO);
	if (!ep) {
		/* Some devices have another endpoint number here ... */
		cp->isdn.intbo_endp = AU_IRQENDPBO_2;
		ep = usb_epnum_to_ep_desc(cp->usbdev,
					  USB_DIR_OUT | AU_IRQENDPBO_2);
		if (!ep) {
			err("can't get B channel OUT endpoint");
			return -5;
		}
	}
	cp->isdn.outsize = ep->wMaxPacketSize;
	cp->isdn.outInterval = ep->bInterval;
	cp->isdn.usbdev = cp->usbdev;

	/* allocate the urb and data buffer */
	if (!cp->isdn.intbo_urbp) {
		cp->isdn.intbo_urbp = usb_alloc_urb(0);
		if (!cp->isdn.intbo_urbp) {
			err("can't allocate urb for B channel output endpoint");
			return -6;
		}
	}
	if (!cp->isdn.intbo_bufp) {
		cp->isdn.intbo_bufp =
		    (char *) kmalloc(cp->isdn.outsize, GFP_KERNEL);
		if (!cp->isdn.intbo_bufp) {
			err("can't allocate buffer for B channel output endpoint");
			return -7;
		}
	}

	/* get the B channel input INT size */
	ep = usb_epnum_to_ep_desc(cp->usbdev, USB_DIR_IN | AU_IRQENDPBI);
	if (!ep) {
		err("can't get B channel IN endpoint");
		return -8;
	}
	cp->isdn.insize = ep->wMaxPacketSize;

	/* allocate the urb and data buffer */
	if (!cp->isdn.intbi_urbp) {
		cp->isdn.intbi_urbp = usb_alloc_urb(0);
		if (!cp->isdn.intbi_urbp) {
			err("can't allocate urb for B channel input endpoint");
			return -9;
		}
	}
	if (!cp->isdn.intbi_bufp) {
		cp->isdn.intbi_bufp =
		    (char *) kmalloc(cp->isdn.insize, GFP_KERNEL);
		if (!cp->isdn.intbi_bufp) {
			err("can't allocate buffer for B channel input endpoint");
			return -10;
		}
	}

	/* setup urb */
	FILL_INT_URB(cp->isdn.intbi_urbp, cp->usbdev,
		     usb_rcvintpipe(cp->usbdev, AU_IRQENDPBI),
		     cp->isdn.intbi_bufp, cp->isdn.insize,
		     auerisdn_intbi_complete, cp, ep->bInterval);
	/* start the urb */
	cp->isdn.intbi_urbp->status = 0;	/* needed! */
	ret = usb_submit_urb(cp->isdn.intbi_urbp);
	if (ret < 0) {
		err("activation of B channel input int failed %d", ret);
		usb_free_urb(cp->isdn.intbi_urbp);
		cp->isdn.intbi_urbp = NULL;
		return -11;
	}

	/* request the D-channel service now */
	dbg("Requesting D channel now");
	cp->isdn.dchannelservice.id = AUH_DCHANNEL;
	if (auerswald_addservice(cp, &cp->isdn.dchannelservice)) {
		err("can not open D-channel");
		cp->isdn.dchannelservice.id = AUH_UNASSIGNED;
		return -2;
	}

	/* Find a free hisax interface */
	for (u = 0; u < AUER_MAX_DEVICES; u++) {
		ahp = &auerhisax_table[u];
		if (!ahp->cp) {
			first_time = (u == 0);
			goto ahp_found;
		}
	}
	/* no free interface found */
	return -12;

	/* we found a free hisax interface */
      ahp_found:
	/* Wait until ipppd timeout expired. The reason behind this ugly construct:
	   If we connect to a hisax device without waiting for ipppd we are not able
	   to make a new IP connection. */
	if (ahp->last_close) {
		unsigned long timeout = jiffies - ahp->last_close;
		if (timeout < AUISDN_IPTIMEOUT) {
			info("waiting for ipppd to timeout");
			sleep_on_timeout(&wqh, AUISDN_IPTIMEOUT - timeout);
		}
	}

	cp->isdn.ahp = ahp;
	u = ahp->hisax_registered;
	ahp->hisax_registered = 1;
	ahp->cp = cp;

	/* now do the registration */
	if (!u) {
		for (u = 0; u < AUISDN_BCHANNELS; u++) {
			b_if[u] = &ahp->hisax_b_if[u];
		}
		if (hisax_register
		    (&ahp->hisax_d_if, b_if, "auerswald_usb",
		     ISDN_PTYPE_EURO)) {
			err("hisax registration failed");
			ahp->cp = NULL;
			cp->isdn.ahp = NULL;
			ahp->hisax_registered = 0;
			return -13;
		}
		dbg("hisax interface registered");
	}

	/* send a D channel L1 activation indication to hisax */
	auerisdn_d_l1l2(&cp->isdn, PH_ACTIVATE | INDICATION, NULL);
	cp->isdn.dc_activated = 1;

	/* do another D channel activation for problematic devices */
	cp->isdn.dcopen_timer.expires = jiffies + HZ;
	dbg("add timer");
	add_timer(&cp->isdn.dcopen_timer);

	return 0;
}

/* The USB device was disconnected */
void auerisdn_disconnect(struct auerswald *cp)
{
	struct auerhisax *ahp;
	DECLARE_WAIT_QUEUE_HEAD(wqh);
	unsigned long flags;
	unsigned int u;
	int ret;
	unsigned int stop_bc;

	dbg("auerisdn_disconnect called");

	/* stop a running timer */
	del_timer_sync(&cp->isdn.dcopen_timer);

	/* first, stop the B channels */
	stop_bc = auerisdn_b_disconnect(cp);

	/* stop the D channels */
	auerisdn_d_l1l2(&cp->isdn, PH_DEACTIVATE | INDICATION, NULL);
	cp->isdn.dc_activated = 0;
	dbg("D-Channel disconnected");

	/* Wait a moment */
	sleep_on_timeout(&wqh, HZ / 10);

	/* Shut the connection to the hisax interface */
	ahp = cp->isdn.ahp;
	if (ahp) {
		dbg("closing connection to hisax interface");
		ahp->cp = NULL;
		cp->isdn.ahp = NULL;
		/* time of last closure */
		if (stop_bc)
			/* if we kill a running connection ... */
			ahp->last_close = jiffies;
		else
			ahp->last_close = 0;
	}

	/* Now free the memory */
	if (cp->isdn.intbi_urbp) {
		ret = usb_unlink_urb(cp->isdn.intbi_urbp);
		if (ret)
			dbg("B in: nonzero int unlink result received: %d",
			    ret);
		usb_free_urb(cp->isdn.intbi_urbp);
		cp->isdn.intbi_urbp = NULL;
	}
	kfree(cp->isdn.intbi_bufp);
	cp->isdn.intbi_bufp = NULL;
	
	if (cp->isdn.intbo_urbp) {
		cp->isdn.intbo_urbp->transfer_flags &= ~USB_ASYNC_UNLINK;
		ret = usb_unlink_urb(cp->isdn.intbo_urbp);
		if (ret)
			dbg("B out: nonzero int unlink result received: %d", ret);
		usb_free_urb(cp->isdn.intbo_urbp);
		cp->isdn.intbo_urbp = NULL;
	}
	kfree(cp->isdn.intbo_bufp);
	cp->isdn.intbo_bufp = NULL;

	/* Remove the rx and tx buffers */
	for (u = 0; u < AUISDN_BCHANNELS; u++) {
		kfree(cp->isdn.bc[u].rxbuf);
		cp->isdn.bc[u].rxbuf = NULL;
		spin_lock_irqsave(&cp->isdn.bc[u].txskb_lock, flags);
		if (cp->isdn.bc[u].txskb) {
			skb_pull(cp->isdn.bc[u].txskb,
				 cp->isdn.bc[u].txskb->len);
			dev_kfree_skb_any(cp->isdn.bc[u].txskb);
			cp->isdn.bc[u].txskb = NULL;
		}
		spin_unlock_irqrestore(&cp->isdn.bc[u].txskb_lock, flags);
	}

	/* Remove the D-channel connection */
	auerswald_removeservice(cp, &cp->isdn.dchannelservice);
}


/*-------------------------------------------------------------------*/
/* Environment for long-lasting hisax interface                      */

/* Wrapper for hisax B0 channel L2L1 */
static void auerisdn_b0_l2l1_wrapper(struct hisax_if *ifc, int pr,
				     void *arg)
{
	auerisdn_b_l2l1(ifc, pr, arg, 0);
}

/* Wrapper for hisax B1 channel L2L1 */
static void auerisdn_b1_l2l1_wrapper(struct hisax_if *ifc, int pr,
				     void *arg)
{
	auerisdn_b_l2l1(ifc, pr, arg, 1);
}

/* Init the global variables */
void auerisdn_init(void)
{
	struct auerhisax *ahp;
	unsigned int u;

	memset(&auerhisax_table, 0, sizeof(auerhisax_table));
	for (u = 0; u < AUER_MAX_DEVICES; u++) {
		ahp = &auerhisax_table[u];
		spin_lock_init(&ahp->seq_lock);
		ahp->hisax_d_if.ifc.priv = ahp;
		ahp->hisax_d_if.ifc.l2l1 = auerisdn_d_l2l1;
		ahp->hisax_b_if[0].ifc.priv = ahp;
		ahp->hisax_b_if[0].ifc.l2l1 = auerisdn_b0_l2l1_wrapper;
		ahp->hisax_b_if[1].ifc.priv = ahp;
		ahp->hisax_b_if[1].ifc.l2l1 = auerisdn_b1_l2l1_wrapper;
	}
}

/* Deinit the global variables */
void auerisdn_cleanup(void)
{
	struct auerhisax *ahp;
	int i;

	/* cleanup last allocated device first */
	for (i = AUER_MAX_DEVICES - 1; i >= 0; i--) {
		ahp = &auerhisax_table[i];
		if (ahp->cp) {
			err("hisax device %d open at cleanup", i);
		}
		if (ahp->hisax_registered) {
			hisax_unregister(&ahp->hisax_d_if);
			dbg("hisax interface %d freed", i);
		}
	}
}
