/* -*- linux-c -*- */
#ifndef _RING_H_
#define _RING_H_

/*
 * Copyright (C) 2001 By Joachim Martillo, Telford Tools, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 *
 */

#include <linux/version.h>
#include <linux/pci.h>
#ifdef __KERNEL__
#include <linux/netdevice.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 4, 0)
#define dev_kfree_skb_irq(s) dev_kfree_skb((s))
#define dev_kfree_skb_any(s) dev_kfree_skb((s))
#define	net_device_stats enet_statistics
#define net_device device
#else
#define NETSTATS_VER2
#endif
#endif

#define	OWNER		((unsigned short)0x8000) /* mask for ownership bit */
#define	OWN_DRIVER 	((unsigned short)0x8000) /* value of owner bit == host */
#define	OWN_SAB		((unsigned short)0x0000) /* value of owner bit == sab or
						  * receive or send */
#define LISTSIZE 32
#define RXSIZE (8192+3)
#define MAXNAMESIZE 11
#define MAXSAB8253XDEVICES 256

struct counters
{
	unsigned int interruptcount;
	unsigned int freecount;
	unsigned int receivepacket;
	unsigned int receivebytes;
	unsigned int transmitpacket;
	unsigned int transmitbytes;  
	unsigned int tx_drops;
	unsigned int rx_drops;
};

typedef struct ring_descriptor
{
	unsigned short		Count;
	unsigned char		sendcrc;
	unsigned char		crcindex;
	unsigned int		crc;
	struct sk_buff*		HostVaddr;
	struct ring_descriptor*	VNext;
} RING_DESCRIPTOR;

typedef struct dcontrol2
{
	RING_DESCRIPTOR *receive;
	RING_DESCRIPTOR *transmit;
} DCONTROL2;

#endif
