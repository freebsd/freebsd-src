/*********************************************************************
 *                
 * Filename:      irtty.h
 * Version:       1.0
 * Description:   
 * Status:        Experimental.
 * Author:        Dag Brattli <dagb@cs.uit.no>
 * Created at:    Tue Dec  9 21:13:12 1997
 * Modified at:   Tue Jan 25 09:10:18 2000
 * Modified by:   Dag Brattli <dagb@cs.uit.no>
 *  
 *     Copyright (c) 1997, 1999-2000 Dag Brattli, All Rights Reserved.
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

#ifndef IRTTY_H
#define IRTTY_H

#include <linux/if.h>
#include <linux/skbuff.h>
#include <linux/termios.h>
#include <linux/netdevice.h>

#include <net/irda/irda.h>
#include <net/irda/irqueue.h>
#include <net/irda/irda_device.h>

/* Used by ioctl */
struct irtty_info {
	char name[6];
};

#define IRTTY_IOC_MAGIC 'e'
#define IRTTY_IOCTDONGLE  _IO(IRTTY_IOC_MAGIC, 1)
#define IRTTY_IOCGET     _IOR(IRTTY_IOC_MAGIC, 2, struct irtty_info)
#define IRTTY_IOC_MAXNR   2

struct irtty_cb {
	irda_queue_t q;     /* Must be first */
	magic_t magic;

	struct net_device *netdev; /* Yes! we are some kind of netdevice */
	struct irda_task *task;
	struct net_device_stats stats;

	struct tty_struct  *tty;
	struct irlap_cb    *irlap; /* The link layer we are binded to */

	chipio_t io;               /* IrDA controller information */
	iobuff_t tx_buff;          /* Transmit buffer */
	iobuff_t rx_buff;          /* Receive buffer */

	struct qos_info qos;       /* QoS capabilities for this device */
	dongle_t *dongle;          /* Dongle driver */

	__u32 new_speed;
 	__u32 flags;               /* Interface flags */

	INFRARED_MODE mode;
};
 
int irtty_register_dongle(struct dongle_reg *dongle);
void irtty_unregister_dongle(struct dongle_reg *dongle);

#endif





