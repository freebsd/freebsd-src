/*
 * ng_ubt.h
 *
 * Copyright (c) 2001-2002 Maksim Yevmenkin <m_evmenkin@yahoo.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id: ng_ubt.h,v 1.1 2002/11/09 19:09:02 max Exp $
 * $FreeBSD$
 */

#ifndef _NG_UBT_H_
#define _NG_UBT_H_

/* XXX FIXME Does not belong here. Move to usbdevs.h later */
#define USB_VENDOR_MSI			0x0db0 /* MSI www.msi.com.tw */
#define USB_VENDOR_EPOX			0x0a12 /* EPoX www.epox.com */

#define USB_PRODUCT_3COM_3CREB96	0x00a0 /* 3Com Bluetooth USB dongle */
#define USB_PRODUCT_MITSUMI_BT_DONGLE	0x641f /* Mitsumi Bluetooth USB dongle*/
#define USB_PRODUCT_TDK_BT_DONGLE	0x0309 /* TDK Bluetooth USB dongle */
#define USB_PRODUCT_MSI_BT_DONGLE	0x1967 /* MSI Bluetooth USB dongle */
#define USB_PRODUCT_DBW_120M_BT_DONGLE	0x2033 /* D-Link DBW-120M */
#define USB_PRODUCT_BT_DG02_DONGLE	0x0001 /* EPoX BT-DG02 USB dongle */

/* XXX FIXME Does not belong here. Move to usb.h later */
#define UICLASS_WIRELESS_CONTROLLER	0xe0	/* Wireless Controller */
#define UISUBCLASS_RF_CONTROLLER	0x01	/* RF Controller */
#define UIPROTO_BLUETOOTH		0x01	/* Bluetooth programming */

/**************************************************************************
 **************************************************************************
 **     Netgraph node hook name, type name and type cookie and commands 
 **************************************************************************  
 **************************************************************************/

#define NG_UBT_NODE_TYPE	"ubt"
#define NG_UBT_HOOK		"hook"

#define NGM_UBT_COOKIE		1021837971

/* Debug levels */
#define NG_UBT_ALERT_LEVEL	1
#define NG_UBT_ERR_LEVEL	2
#define NG_UBT_WARN_LEVEL	3
#define NG_UBT_INFO_LEVEL	4

/**************************************************************************
 **************************************************************************
 **                    UBT node command/event parameters
 **************************************************************************
 **************************************************************************/

#define NGM_UBT_NODE_SET_DEBUG	1		/* set debug level */
#define NGM_UBT_NODE_GET_DEBUG	2		/* get debug level */
typedef u_int16_t		ng_ubt_node_debug_ep; 

#define NGM_UBT_NODE_SET_QLEN	3		/* set queue length */
#define NGM_UBT_NODE_GET_QLEN	4		/* get queue length */ 
typedef struct {
	int32_t		queue;			/* queue index */
#define	NGM_UBT_NODE_QUEUE_IN	1		/* incoming queue */
#define	NGM_UBT_NODE_QUEUE_CMD	2		/* commands */
#define	NGM_UBT_NODE_QUEUE_ACL	3		/* ACL data */
#define	NGM_UBT_NODE_QUEUE_SCO	4		/* SCO data */

	int32_t		qlen;			/* queue length */
} ng_ubt_node_qlen_ep;

#define NGM_UBT_NODE_GET_STAT	5		/* get statistic */
typedef struct {
	u_int32_t	pckts_recv;		/* # of packets received */
	u_int32_t	bytes_recv;		/* # of bytes received */
	u_int32_t	pckts_sent;		/* # of packets sent */
	u_int32_t	bytes_sent;		/* # of bytes sent */
	u_int32_t	oerrors;		/* # of output errors */
	u_int32_t	ierrors;		/* # of input errors */
} ng_ubt_node_stat_ep;

#define NGM_UBT_NODE_RESET_STAT	6		/* reset statistic */

#endif /* ndef _NG_UBT_H_ */

