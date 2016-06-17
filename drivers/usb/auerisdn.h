/*****************************************************************************/
/*
 *      auerisdn.h  --  Auerswald PBX/System Telephone ISDN interface.
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

#ifndef AUERISDN_H
#define AUERISDN_H

#if (CONFIG_USB_AUERISDN || CONFIG_USB_AUERISDN_MODULE)

#include <linux/timer.h>
#include "auerserv.h"
#include "auerisdn_b.h"

#define AUISDN_IPTIMEOUT (HZ * 60)	/* IP Timeout is 40s */

struct auerswald;
struct auerhisax;

struct auerisdn {
	struct auerscon dchannelservice;	/* serving the D channel */
	struct auerhisax *ahp;			/* Reference to hisax interface */
	unsigned int dc_activated;		/* 1 if D-Channel is activated */
	struct auerisdnbc bc[AUISDN_BCHANNELS];	/* B channel data */
	unsigned int insize;			/* Max. Block Size of Input INT */
	unsigned int outsize;			/* Max. Block Size of Output INT */
	unsigned int outInterval;		/* nr. of ms between INT OUT transfers */
	struct urb *intbi_urbp;			/* B channel Input Interrupt urb */
	unsigned char *intbi_bufp;		/* B channel Input data buffer */
	unsigned int paketsize;			/* Data size of the INT OUT pakets */
	struct usb_device *usbdev;		/* USB device handle */
	unsigned int intbo_state;		/* Status of INT OUT urb */
	struct urb *intbo_urbp;			/* B channel Output Interrupt urb */
	unsigned char *intbo_bufp;		/* B channel Output data buffer */
	unsigned int intbo_index;		/* Index of last served B channel */
	unsigned int intbo_toggletimer;		/* data toggle timer for 2 b channels */
	unsigned int intbo_endp;		/* grrr.. different on some devices */
	struct timer_list dcopen_timer;		/* Open D-channel once more later... */
};

struct auerhisax {
	struct hisax_d_if hisax_d_if;		/* Hisax D-Channel interface */
	struct hisax_b_if hisax_b_if[AUISDN_BCHANNELS];	/* Hisax B-channel interfaces */
	struct auerswald *cp;			/* Context to usb device */
	unsigned int hisax_registered;		/* 1 if registered at hisax interface */
	unsigned char txseq;			/* L2 emulation: tx sequence byte */
	unsigned char rxseq;			/* L2 emulation: rx sequence byte */
	spinlock_t seq_lock;			/* Lock sequence numbers */
	unsigned long last_close;		/* Time of last close in jiffies */
};

/* Function Prototypes */
void auerisdn_init_dev(struct auerswald *cp);

int auerisdn_probe(struct auerswald *cp);

void auerisdn_disconnect(struct auerswald *cp);

void auerisdn_init(void);

void auerisdn_cleanup(void);

#else	/* no CONFIG_USB_AUERISDN */

struct auerisdn {
	int dummy;
};

/* Dummy ISDN functions */
#define auerisdn_init_dev( cp)
#define auerisdn_probe( cp) 0
#define auerisdn_disconnect( cp)
#define auerisdn_init()
#define auerisdn_cleanup()
#endif	/* CONFIG_USB_AUERISDN */

#endif	/* AUERISDN_H */
