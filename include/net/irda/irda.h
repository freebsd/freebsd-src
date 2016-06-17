/*********************************************************************
 *                
 * Filename:      irda.h
 * Version:       1.0
 * Description:   IrDA common include file for kernel internal use
 * Status:        Stable
 * Author:        Dag Brattli <dagb@cs.uit.no>
 * Created at:    Tue Dec  9 21:13:12 1997
 * Modified at:   Fri Jan 28 13:16:32 2000
 * Modified by:   Dag Brattli <dagb@cs.uit.no>
 * 
 *     Copyright (c) 1998-2000 Dag Brattli, All Rights Reserved.
 *     Copyright (c) 2000-2001 Jean Tourrilhes <jt@hpl.hp.com>
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

#ifndef NET_IRDA_H
#define NET_IRDA_H

#include <linux/config.h>
#include <linux/skbuff.h>
#include <linux/kernel.h>
#include <linux/if.h>
#include <linux/irda.h>

typedef __u32 magic_t;

#include <net/irda/qos.h>
#include <net/irda/irqueue.h>

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE 
#define FALSE 0
#endif

/* Hack to do small backoff when setting media busy in IrLAP */
#ifndef SMALL
#define SMALL 5
#endif

#ifndef IRDA_MIN /* Lets not mix this MIN with other header files */
#define IRDA_MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif

#ifndef ALIGN
#  define ALIGN __attribute__((aligned))
#endif
#ifndef PACK
#  define PACK __attribute__((packed))
#endif


#ifdef CONFIG_IRDA_DEBUG

extern __u32 irda_debug;

/* use 0 for production, 1 for verification, >2 for debug */
#define IRDA_DEBUG_LEVEL 0

#define IRDA_DEBUG(n, args...) (irda_debug >= (n)) ? (printk(KERN_DEBUG args)) : 0
#define ASSERT(expr, func) \
if(!(expr)) { \
        printk( "Assertion failed! %s:%s:%d %s\n", \
        __FILE__,__FUNCTION__,__LINE__,(#expr));  \
        func }
#else
#define IRDA_DEBUG(n, args...)
#define ASSERT(expr, func) \
if(!(expr)) do { \
        func } while (0)
#endif /* CONFIG_IRDA_DEBUG */

#define WARNING(args...) printk(KERN_WARNING args)
#define MESSAGE(args...) printk(KERN_INFO args)
#define ERROR(args...)   printk(KERN_ERR args)

#define MSECS_TO_JIFFIES(ms) (((ms)*HZ+999)/1000)

/*
 *  Magic numbers used by Linux-IrDA. Random numbers which must be unique to 
 *  give the best protection
 */

#define IRTTY_MAGIC        0x2357
#define LAP_MAGIC          0x1357
#define LMP_MAGIC          0x4321
#define LMP_LSAP_MAGIC     0x69333
#define LMP_LAP_MAGIC      0x3432
#define IRDA_DEVICE_MAGIC  0x63454
#define IAS_MAGIC          0x007
#define TTP_MAGIC          0x241169
#define TTP_TSAP_MAGIC     0x4345
#define IROBEX_MAGIC       0x341324
#define HB_MAGIC           0x64534
#define IRLAN_MAGIC        0x754
#define IAS_OBJECT_MAGIC   0x34234
#define IAS_ATTRIB_MAGIC   0x45232
#define IRDA_TASK_MAGIC    0x38423

#define IAS_DEVICE_ID 0x0000 /* Defined by IrDA, IrLMP section 4.1 (page 68) */
#define IAS_PNP_ID    0xd342
#define IAS_OBEX_ID   0x34323
#define IAS_IRLAN_ID  0x34234
#define IAS_IRCOMM_ID 0x2343
#define IAS_IRLPT_ID  0x9876

typedef enum { FLOW_STOP, FLOW_START } LOCAL_FLOW;

/* A few forward declarations (to make compiler happy) */
struct tsap_cb;		/* in <net/irda/irttp.h> */
struct lsap_cb;		/* in <net/irda/irlmp.h> */
struct iriap_cb;	/* in <net/irda/iriap.h> */
struct ias_value;	/* in <net/irda/irias_object.h> */
struct discovery_t;	/* in <net/irda/discovery.h> */

/* IrDA Socket */
struct irda_sock {
	__u32 saddr;          /* my local address */
	__u32 daddr;          /* peer address */

	struct lsap_cb *lsap; /* LSAP used by Ultra */
	__u8  pid;            /* Protocol IP (PID) used by Ultra */

	struct tsap_cb *tsap; /* TSAP used by this connection */
	__u8 dtsap_sel;       /* remote TSAP address */
	__u8 stsap_sel;       /* local TSAP address */
	
	__u32 max_sdu_size_rx;
	__u32 max_sdu_size_tx;
	__u32 max_data_size;
	__u8  max_header_size;
	struct qos_info qos_tx;

	__u16 mask;           /* Hint bits mask */
	__u16 hints;          /* Hint bits */

	__u32 ckey;           /* IrLMP client handle */
	__u32 skey;           /* IrLMP service handle */

	struct ias_object *ias_obj;   /* Our service name + lsap in IAS */
	struct iriap_cb *iriap;	      /* Used to query remote IAS */
	struct ias_value *ias_result; /* Result of remote IAS query */

	hashbin_t *cachelog;		/* Result of discovery query */
	struct discovery_t *cachediscovery;	/* Result of selective discovery query */

	int nslots;           /* Number of slots to use for discovery */

	int errno;            /* status of the IAS query */

	struct sock *sk;
	wait_queue_head_t query_wait;	/* Wait for the answer to a query */
	struct timer_list watchdog;	/* Timeout for discovery */

	LOCAL_FLOW tx_flow;
	LOCAL_FLOW rx_flow;
};

/*
 *  This type is used by the protocols that transmit 16 bits words in 
 *  little endian format. A little endian machine stores MSB of word in
 *  byte[1] and LSB in byte[0]. A big endian machine stores MSB in byte[0] 
 *  and LSB in byte[1].
 */
typedef union {
	__u16 word;
	__u8  byte[2];
} __u16_host_order;

/* 
 * Per-packet information we need to hide inside sk_buff 
 * (must not exceed 48 bytes, check with struct sk_buff) 
 */
struct irda_skb_cb {
	magic_t magic;       /* Be sure that we can trust the information */
	__u32   next_speed;  /* The Speed to be set *after* this frame */
	__u16   mtt;         /* Minimum turn around time */
	__u16   xbofs;       /* Number of xbofs required, used by SIR mode */
	__u16   next_xbofs;  /* Number of xbofs required *after* this frame */
	void    *context;    /* May be used by drivers */
	void    (*destructor)(struct sk_buff *skb); /* Used for flow control */
	__u16   xbofs_delay; /* Number of xbofs used for generating the mtt */
	__u8    line;        /* Used by IrCOMM in IrLPT mode */
};

/* Misc status information */
typedef enum {
	STATUS_OK,
	STATUS_ABORTED,
	STATUS_NO_ACTIVITY,
	STATUS_NOISY,
	STATUS_REMOTE,
} LINK_STATUS;

typedef enum {
	LOCK_NO_CHANGE,
	LOCK_LOCKED,
	LOCK_UNLOCKED,
} LOCK_STATUS;

typedef enum { /* FIXME check the two first reason codes */
	LAP_DISC_INDICATION=1, /* Received a disconnect request from peer */
	LAP_NO_RESPONSE,       /* To many retransmits without response */
	LAP_RESET_INDICATION,  /* To many retransmits, or invalid nr/ns */
	LAP_FOUND_NONE,        /* No devices were discovered */
	LAP_MEDIA_BUSY,
	LAP_PRIMARY_CONFLICT,
} LAP_REASON;

/*  
 *  IrLMP disconnect reasons. The order is very important, since they 
 *  correspond to disconnect reasons sent in IrLMP disconnect frames, so
 *  please do not touch :-)
 */
typedef enum {
	LM_USER_REQUEST = 1,  /* User request */
	LM_LAP_DISCONNECT,    /* Unexpected IrLAP disconnect */
	LM_CONNECT_FAILURE,   /* Failed to establish IrLAP connection */
	LM_LAP_RESET,         /* IrLAP reset */
	LM_INIT_DISCONNECT,   /* Link Management initiated disconnect */
	LM_LSAP_NOTCONN,      /* Data delivered on unconnected LSAP */
	LM_NON_RESP_CLIENT,   /* Non responsive LM-MUX client */
	LM_NO_AVAIL_CLIENT,   /* No available LM-MUX client */
	LM_CONN_HALF_OPEN,    /* Connection is half open */
	LM_BAD_SOURCE_ADDR,   /* Illegal source address (i.e 0x00) */
} LM_REASON;
#define LM_UNKNOWN 0xff       /* Unspecified disconnect reason */

/*
 *  Notify structure used between transport and link management layers
 */
typedef struct {
	int (*data_indication)(void *priv, void *sap, struct sk_buff *skb);
	int (*udata_indication)(void *priv, void *sap, struct sk_buff *skb);
	void (*connect_confirm)(void *instance, void *sap, 
				struct qos_info *qos, __u32 max_sdu_size,
				__u8 max_header_size, struct sk_buff *skb);
	void (*connect_indication)(void *instance, void *sap, 
				   struct qos_info *qos, __u32 max_sdu_size, 
				   __u8 max_header_size, struct sk_buff *skb);
	void (*disconnect_indication)(void *instance, void *sap, 
				      LM_REASON reason, struct sk_buff *);
	void (*flow_indication)(void *instance, void *sap, LOCAL_FLOW flow);
	void (*status_indication)(void *instance,
				  LINK_STATUS link, LOCK_STATUS lock);
	void *instance; /* Layer instance pointer */
	char name[16];  /* Name of layer */
} notify_t;

#define NOTIFY_MAX_NAME 16

#endif /* NET_IRDA_H */
