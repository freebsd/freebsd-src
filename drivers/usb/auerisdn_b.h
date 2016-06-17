/*****************************************************************************/
/*
 *      auerisdn_b.h  --  Auerswald PBX/System Telephone ISDN B channel interface.
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

#ifndef AUERISDN_B_H
#define AUERISDN_B_H

#include <../drivers/isdn/hisax/hisax_if.h>
#include <linux/skbuff.h>
#include "auerbuf.h"
#include <../drivers/isdn/hisax/isdnhdlc.h>

#define AUISDN_RXSIZE	4096	/* RX buffer size */
#define AUISDN_BCHANNELS   2	/* Number of supported B channels */

/* states for intbo_state */
#define INTBOS_IDLE	0
#define INTBOS_RUNNING	1
#define INTBOS_CHANGE 	2
#define INTBOS_RESTART	3

/* ...................................................................*/
/* B channel state data */
struct auerswald;
struct auerisdnbc {
	struct auerswald *cp;	/* Context to usb device */
	struct sk_buff *txskb;	/* sk buff to transmitt */
	spinlock_t txskb_lock;	/* protect against races */
	unsigned int mode;	/* B-channel mode */
	unsigned int channel;	/* Number of this B-channel */
	unsigned int ofsize;	/* Size of device OUT fifo in Bytes */
	int txfree;		/* free bytes in tx buffer of device */
	unsigned int txsize;	/* size of data paket for this channel */
	unsigned char *rxbuf;	/* Receiver input buffer */
	struct isdnhdlc_vars inp_hdlc_state;	/* state for RX software HDLC */
	struct isdnhdlc_vars outp_hdlc_state;	/* state for TX software HDLC */
	unsigned int lastbyte;	/* last byte sent out to trans. B channel */
};

/* Function Prototypes */
void auerisdn_b_l2l1(struct hisax_if *ifc, int pr, void *arg,
		     unsigned int channel);
void auerisdn_b_l1l2(struct auerisdnbc *bc, int pr, void *arg);

void auerisdn_intbi_complete(struct urb *urb);
unsigned int auerisdn_b_disconnect(struct auerswald *cp);

#endif	/* AUERISDN_B_H */
