/*
 * USB Compaq iPAQ driver
 *
 *	Copyright (C) 2001 - 2002
 *	    Ganesh Varadarajan <ganesh@veritas.com>
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 */

#ifndef __LINUX_USB_SERIAL_IPAQ_H
#define __LINUX_USB_SERIAL_IPAQ_H

#define ASKEY_VENDOR_ID		0x1690
#define ASKEY_PRODUCT_ID	0x0601

#define BCOM_VENDOR_ID		0x0960
#define BCOM_0065_ID		0x0065
#define BCOM_0066_ID		0x0066
#define BCOM_0067_ID		0x0067

#define CASIO_VENDOR_ID		0x07cf
#define CASIO_2001_ID		0x2001
#define CASIO_EM500_ID		0x2002

#define COMPAQ_VENDOR_ID	0x049f
#define COMPAQ_IPAQ_ID		0x0003
#define COMPAQ_0032_ID		0x0032

#define DELL_VENDOR_ID		0x413c
#define DELL_AXIM_ID		0x4001

#define FSC_VENDOR_ID		0x0bf8
#define FSC_LOOX_ID		0x1001

#define HP_VENDOR_ID		0x03f0
#define HP_JORNADA_548_ID	0x1016
#define HP_JORNADA_568_ID	0x1116
#define HP_2016_ID		0x2016
#define HP_2116_ID		0x2116
#define HP_2216_ID		0x2216
#define HP_3016_ID		0x3016
#define HP_3116_ID		0x3116
#define HP_3216_ID		0x3216
#define HP_4016_ID		0x4016
#define HP_4116_ID		0x4116
#define HP_4216_ID		0x4216
#define HP_5016_ID		0x5016
#define HP_5116_ID		0x5116
#define HP_5216_ID		0x5216

#define LINKUP_VENDOR_ID	0x094b
#define LINKUP_PRODUCT_ID	0x0001

#define MICROSOFT_VENDOR_ID	0x045e
#define MICROSOFT_00CE_ID	0x00ce

#define PORTATEC_VENDOR_ID	0x0961
#define PORTATEC_PRODUCT_ID	0x0010

#define ROVER_VENDOR_ID		0x047b
#define ROVER_P5_ID		0x3000

#define SAGEM_VENDOR_ID		0x5e04
#define SAGEM_WIRELESS_ID	0xce00

#define SOCKET_VENDOR_ID	0x0104
#define SOCKET_PRODUCT_ID	0x00be

#define TOSHIBA_VENDOR_ID	0x0930
#define TOSHIBA_PRODUCT_ID	0x0700
#define TOSHIBA_E310_ID		0x0705
#define TOSHIBA_E740_ID		0x0706
#define TOSHIBA_E335_ID		0x0707

#define HTC_VENDOR_ID		0x0bb4
#define HTC_PRODUCT_ID		0x00ce

#define NEC_VENDOR_ID		0x0409
#define NEC_PRODUCT_ID		0x00d5

#define ASUS_VENDOR_ID		0x0b05
#define ASUS_A600_PRODUCT_ID	0x4201

/*
 * Since we can't queue our bulk write urbs (don't know why - it just
 * doesn't work), we can send down only one write urb at a time. The simplistic
 * approach taken by the generic usbserial driver will work, but it's not good
 * for performance. Therefore, we buffer upto URBDATA_QUEUE_MAX bytes of write
 * requests coming from the line discipline. This is done by chaining them
 * in lists of struct ipaq_packet, each packet holding a maximum of
 * PACKET_SIZE bytes.
 *
 * ipaq_write() can be called from bottom half context; hence we can't
 * allocate memory for packets there. So we initialize a pool of packets at
 * the first open and maintain a freelist.
 *
 * The value of PACKET_SIZE was empirically determined by
 * checking the maximum write sizes sent down by the ppp ldisc.
 * URBDATA_QUEUE_MAX is set to 64K, which is the maximum TCP window size.
 */

struct ipaq_packet {
	char			*data;
	size_t			len;
	size_t			written;
	struct list_head	list;
};

struct ipaq_private {
	int			active;
	int			queue_len;
	int			free_len;
	struct list_head	queue;
	struct list_head	freelist;
};

#define URBDATA_SIZE		4096
#define URBDATA_QUEUE_MAX	(64 * 1024)
#define PACKET_SIZE		256

#endif
