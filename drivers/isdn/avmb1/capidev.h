/* $Id: capidev.h,v 1.1.4.1 2001/11/20 14:19:34 kai Exp $
 *
 * CAPI 2.0 Interface for Linux
 *
 * Copyright 1996 by Carsten Paeth <calle@calle.de>
 *
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
 *
 */

struct capidev {
	struct capidev *next;
	struct file    *file;
	__u16		applid;
	__u16		errcode;
	unsigned int    minor;

	struct sk_buff_head recv_queue;
	wait_queue_head_t recv_wait;

	/* Statistic */
	unsigned long	nrecvctlpkt;
	unsigned long	nrecvdatapkt;
	unsigned long	nsentctlpkt;
	unsigned long	nsentdatapkt;
};
