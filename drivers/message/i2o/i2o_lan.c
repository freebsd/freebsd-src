/*
 *	drivers/i2o/i2o_lan.c
 *
 * 	I2O LAN CLASS OSM 		May 26th 2000
 *
 *	(C) Copyright 1999, 2000 	University of Helsinki,
 *		      			Department of Computer Science
 *
 * 	This code is still under development / test.
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License
 *	as published by the Free Software Foundation; either version
 *	2 of the License, or (at your option) any later version.
 *
 *	Authors: 	Auvo Häkkinen <Auvo.Hakkinen@cs.Helsinki.FI>
 *	Fixes:		Juha Sievänen <Juha.Sievanen@cs.Helsinki.FI>
 *	 		Taneli Vähäkangas <Taneli.Vahakangas@cs.Helsinki.FI>
 *			Deepak Saxena <deepak@plexity.net>
 *
 *	Tested:		in FDDI environment (using SysKonnect's DDM)
 *			in Gigabit Eth environment (using SysKonnect's DDM)
 *			in Fast Ethernet environment (using Intel 82558 DDM)
 *
 *	TODO:		tests for other LAN classes (Token Ring, Fibre Channel)
 */

#include <linux/config.h>
#include <linux/module.h>

#include <linux/pci.h>

#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/fddidevice.h>
#include <linux/trdevice.h>
#include <linux/fcdevice.h>

#include <linux/skbuff.h>
#include <linux/if_arp.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/spinlock.h>
#include <linux/tqueue.h>
#include <asm/io.h>

#include <linux/errno.h>

#include <linux/i2o.h>
#include "i2o_lan.h"

//#define DRIVERDEBUG
#ifdef DRIVERDEBUG
#define dprintk(s, args...) printk(s, ## args)
#else
#define dprintk(s, args...)
#endif

/* The following module parameters are used as default values
 * for per interface values located in the net_device private area.
 * Private values are changed via /proc filesystem.
 */
static u32 max_buckets_out = I2O_LAN_MAX_BUCKETS_OUT;
static u32 bucket_thresh   = I2O_LAN_BUCKET_THRESH;
static u32 rx_copybreak    = I2O_LAN_RX_COPYBREAK;
static u8  tx_batch_mode   = I2O_LAN_TX_BATCH_MODE;
static u32 i2o_event_mask  = I2O_LAN_EVENT_MASK;

#define MAX_LAN_CARDS 16
static struct net_device *i2o_landevs[MAX_LAN_CARDS+1];
static int unit = -1; 	  /* device unit number */

static void i2o_lan_reply(struct i2o_handler *h, struct i2o_controller *iop, struct i2o_message *m);
static void i2o_lan_send_post_reply(struct i2o_handler *h, struct i2o_controller *iop, struct i2o_message *m);
static int i2o_lan_receive_post(struct net_device *dev);
static void i2o_lan_receive_post_reply(struct i2o_handler *h, struct i2o_controller *iop, struct i2o_message *m);
static void i2o_lan_release_buckets(struct net_device *dev, u32 *msg);

static int i2o_lan_reset(struct net_device *dev);
static void i2o_lan_handle_event(struct net_device *dev, u32 *msg);

/* Structures to register handlers for the incoming replies. */

static struct i2o_handler i2o_lan_send_handler = {
	i2o_lan_send_post_reply, 	// For send replies
	NULL,
	NULL,
	NULL,
	"I2O LAN OSM send",
	-1,
	I2O_CLASS_LAN
};
static int lan_send_context;

static struct i2o_handler i2o_lan_receive_handler = {
	i2o_lan_receive_post_reply,	// For receive replies
	NULL,
	NULL,
	NULL,
	"I2O LAN OSM receive",
	-1,
	I2O_CLASS_LAN
};
static int lan_receive_context;

static struct i2o_handler i2o_lan_handler = {
	i2o_lan_reply,			// For other replies
	NULL,
	NULL,
	NULL,
	"I2O LAN OSM",
	-1,
	I2O_CLASS_LAN
};
static int lan_context;

DECLARE_TASK_QUEUE(i2o_post_buckets_task);
struct tq_struct run_i2o_post_buckets_task = {
	routine: (void (*)(void *)) run_task_queue,
	data: (void *) 0
};

/* Functions to handle message failures and transaction errors:
==============================================================*/

/*
 * i2o_lan_handle_failure(): Fail bit has been set since IOP's message
 * layer cannot deliver the request to the target, or the target cannot
 * process the request.
 */
static void i2o_lan_handle_failure(struct net_device *dev, u32 *msg)
{
	struct i2o_lan_local *priv = (struct i2o_lan_local *)dev->priv;
	struct i2o_device *i2o_dev = priv->i2o_dev;
	struct i2o_controller *iop = i2o_dev->controller;

	u32 *preserved_msg = (u32*)(iop->mem_offset + msg[7]);
	u32 *sgl_elem = &preserved_msg[4];
	struct sk_buff *skb = NULL;
	u8 le_flag;

	i2o_report_status(KERN_INFO, dev->name, msg);

	/* If PacketSend failed, free sk_buffs reserved by upper layers */

	if (msg[1] >> 24 == LAN_PACKET_SEND) {
		do {
			skb = (struct sk_buff *)(sgl_elem[1]);
			dev_kfree_skb_irq(skb);

			atomic_dec(&priv->tx_out);

			le_flag = *sgl_elem >> 31;
			sgl_elem +=3;
		} while (le_flag == 0); /* Last element flag not set */

		if (netif_queue_stopped(dev))
			netif_wake_queue(dev);
	}

	/* If ReceivePost failed, free sk_buffs we have reserved */

	if (msg[1] >> 24 == LAN_RECEIVE_POST) {
		do {
			skb = (struct sk_buff *)(sgl_elem[1]);
			dev_kfree_skb_irq(skb);

			atomic_dec(&priv->buckets_out);

			le_flag = *sgl_elem >> 31;
			sgl_elem +=3;
		} while (le_flag == 0); /* Last element flag not set */
	}

	/* Release the preserved msg frame by resubmitting it as a NOP */

	preserved_msg[0] = THREE_WORD_MSG_SIZE | SGL_OFFSET_0;
	preserved_msg[1] = I2O_CMD_UTIL_NOP << 24 | HOST_TID << 12 | 0;
	preserved_msg[2] = 0;
	i2o_post_message(iop, msg[7]);
}
/*
 * i2o_lan_handle_transaction_error(): IOP or DDM has rejected the request
 * for general cause (format error, bad function code, insufficient resources,
 * etc.). We get one transaction_error for each failed transaction.
 */
static void i2o_lan_handle_transaction_error(struct net_device *dev, u32 *msg)
{
	struct i2o_lan_local *priv = (struct i2o_lan_local *)dev->priv;
	struct sk_buff *skb;

	i2o_report_status(KERN_INFO, dev->name, msg);

	/* If PacketSend was rejected, free sk_buff reserved by upper layers */

	if (msg[1] >> 24 == LAN_PACKET_SEND) {
		skb = (struct sk_buff *)(msg[3]); // TransactionContext
		dev_kfree_skb_irq(skb);
		atomic_dec(&priv->tx_out);

		if (netif_queue_stopped(dev))
			netif_wake_queue(dev);
 	}

	/* If ReceivePost was rejected, free sk_buff we have reserved */

	if (msg[1] >> 24 == LAN_RECEIVE_POST) {
		skb = (struct sk_buff *)(msg[3]);
		dev_kfree_skb_irq(skb);
		atomic_dec(&priv->buckets_out);
	}
}

/*
 * i2o_lan_handle_status(): Common parts of handling a not succeeded request
 * (status != SUCCESS).
 */
static int i2o_lan_handle_status(struct net_device *dev, u32 *msg)
{
	/* Fail bit set? */

	if (msg[0] & MSG_FAIL) {
		i2o_lan_handle_failure(dev, msg);
		return -1;
	}

	/* Message rejected for general cause? */

	if ((msg[4]>>24) == I2O_REPLY_STATUS_TRANSACTION_ERROR) {
		i2o_lan_handle_transaction_error(dev, msg);
		return -1;
	}

	/* Else have to handle it in the callback function */

	return 0;
}

/* Callback functions called from the interrupt routine:
=======================================================*/

/*
 * i2o_lan_send_post_reply(): Callback function to handle PostSend replies.
 */
static void i2o_lan_send_post_reply(struct i2o_handler *h,
			struct i2o_controller *iop, struct i2o_message *m)
{
	u32 *msg = (u32 *)m;
	u8 unit  = (u8)(msg[2]>>16); // InitiatorContext
	struct net_device *dev = i2o_landevs[unit];
	struct i2o_lan_local *priv = (struct i2o_lan_local *)dev->priv;
	u8 trl_count  = msg[3] & 0x000000FF;

	if ((msg[4] >> 24) != I2O_REPLY_STATUS_SUCCESS) {
		if (i2o_lan_handle_status(dev, msg))
			return;
	}

#ifdef DRIVERDEBUG
	i2o_report_status(KERN_INFO, dev->name, msg);
#endif

	/* DDM has handled transmit request(s), free sk_buffs.
	 * We get similar single transaction reply also in error cases 
	 * (except if msg failure or transaction error).
	 */
	while (trl_count) {
		dev_kfree_skb_irq((struct sk_buff *)msg[4 + trl_count]);
		dprintk(KERN_INFO "%s: tx skb freed (trl_count=%d).\n",
			dev->name, trl_count);
		atomic_dec(&priv->tx_out);
		trl_count--;
	}

	/* If priv->tx_out had reached tx_max_out, the queue was stopped */

	if (netif_queue_stopped(dev))
 		netif_wake_queue(dev);
}

/*
 * i2o_lan_receive_post_reply(): Callback function to process incoming packets.
 */
static void i2o_lan_receive_post_reply(struct i2o_handler *h,
			struct i2o_controller *iop, struct i2o_message *m)
{
	u32 *msg = (u32 *)m;
	u8 unit  = (u8)(msg[2]>>16); // InitiatorContext
	struct net_device *dev = i2o_landevs[unit];

	struct i2o_lan_local *priv = (struct i2o_lan_local *)dev->priv;
	struct i2o_bucket_descriptor *bucket = (struct i2o_bucket_descriptor *)&msg[6];
	struct i2o_packet_info *packet;
	u8 trl_count = msg[3] & 0x000000FF;
	struct sk_buff *skb, *old_skb;
	unsigned long flags = 0;

	if ((msg[4] >> 24) != I2O_REPLY_STATUS_SUCCESS) {
		if (i2o_lan_handle_status(dev, msg))
			return;

		i2o_lan_release_buckets(dev, msg);
		return;
	}

#ifdef DRIVERDEBUG
	i2o_report_status(KERN_INFO, dev->name, msg);
#endif

	/* Else we are receiving incoming post. */

	while (trl_count--) {
		skb = (struct sk_buff *)bucket->context;
		packet = (struct i2o_packet_info *)bucket->packet_info;
		atomic_dec(&priv->buckets_out);

		/* Sanity checks: Any weird characteristics in bucket? */

		if (packet->flags & 0x0f || ! packet->flags & 0x40) {
			if (packet->flags & 0x01)
				printk(KERN_WARNING "%s: packet with errors, error code=0x%02x.\n",
					dev->name, packet->status & 0xff);

			/* The following shouldn't happen, unless parameters in
			 * LAN_OPERATION group are changed during the run time.
			 */
			 if (packet->flags & 0x0c)
				printk(KERN_DEBUG "%s: multi-bucket packets not supported!\n", 
					dev->name);
					
			if (! packet->flags & 0x40)
				printk(KERN_DEBUG "%s: multiple packets in a bucket not supported!\n", 
					dev->name);

			dev_kfree_skb_irq(skb);

			bucket++;
			continue;
		}

		/* Copy short packet to a new skb */
		
		if (packet->len < priv->rx_copybreak) {
			old_skb = skb;
			skb = (struct sk_buff *)dev_alloc_skb(packet->len+2);
			if (skb == NULL) {
				printk(KERN_ERR "%s: Can't allocate skb.\n", dev->name);
				return;
			}
			skb_reserve(skb, 2);
			memcpy(skb_put(skb, packet->len), old_skb->data, packet->len);

			spin_lock_irqsave(&priv->fbl_lock, flags);
			if (priv->i2o_fbl_tail < I2O_LAN_MAX_BUCKETS_OUT)
				priv->i2o_fbl[++priv->i2o_fbl_tail] = old_skb;
			else
				dev_kfree_skb_irq(old_skb);

			spin_unlock_irqrestore(&priv->fbl_lock, flags);
		} else
			skb_put(skb, packet->len);

		/* Deliver to upper layers */

		skb->dev = dev;
		skb->protocol = priv->type_trans(skb, dev);
		netif_rx(skb);

		dev->last_rx = jiffies;

		dprintk(KERN_INFO "%s: Incoming packet (%d bytes) delivered "
			"to upper level.\n", dev->name, packet->len);

		bucket++; // to next Packet Descriptor Block
	}

#ifdef DRIVERDEBUG
	if (msg[5] == 0)
		printk(KERN_INFO "%s: DDM out of buckets (priv->count = %d)!\n",
		       dev->name, atomic_read(&priv->buckets_out));
#endif

	/* If DDM has already consumed bucket_thresh buckets, post new ones */

	if (atomic_read(&priv->buckets_out) <= priv->max_buckets_out - priv->bucket_thresh) {
		run_i2o_post_buckets_task.data = (void *)dev;
		queue_task(&run_i2o_post_buckets_task, &tq_immediate);
		mark_bh(IMMEDIATE_BH);
	}

	return;
}

/*
 * i2o_lan_reply(): Callback function to handle other incoming messages
 * except SendPost and ReceivePost.
 */
static void i2o_lan_reply(struct i2o_handler *h, struct i2o_controller *iop,
			  struct i2o_message *m)
{
	u32 *msg = (u32 *)m;
	u8 unit  = (u8)(msg[2]>>16); // InitiatorContext
	struct net_device *dev = i2o_landevs[unit];

	if ((msg[4] >> 24) != I2O_REPLY_STATUS_SUCCESS) {
		if (i2o_lan_handle_status(dev, msg))
			return;

		/* In other error cases just report and continue */

		i2o_report_status(KERN_INFO, dev->name, msg);
	}

#ifdef DRIVERDEBUG
	i2o_report_status(KERN_INFO, dev->name, msg);
#endif
	switch (msg[1] >> 24) {
		case LAN_RESET:
		case LAN_SUSPEND:
			/* default reply without payload */
		break;

		case I2O_CMD_UTIL_EVT_REGISTER: 
		case I2O_CMD_UTIL_EVT_ACK:
			i2o_lan_handle_event(dev, msg);
		break;

		case I2O_CMD_UTIL_PARAMS_SET:
			/* default reply, results in ReplyPayload (not examined) */
			switch (msg[3] >> 16) {
			    case 1: dprintk(KERN_INFO "%s: Reply to set MAC filter mask.\n",
					dev->name);
			    break;
			    case 2: dprintk(KERN_INFO "%s: Reply to set MAC table.\n", 
					dev->name);
			    break;
			    default: printk(KERN_WARNING "%s: Bad group 0x%04X\n",
			 		dev->name,msg[3] >> 16);
			}
		break;

		default:
			printk(KERN_ERR "%s: No handler for the reply.\n",
		       		dev->name);
			i2o_report_status(KERN_INFO, dev->name, msg);
	}
}

/* Functions used by the above callback functions:
=================================================*/
/*
 * i2o_lan_release_buckets(): Free unused buckets (sk_buffs).
 */
static void i2o_lan_release_buckets(struct net_device *dev, u32 *msg)
{
	struct i2o_lan_local *priv = (struct i2o_lan_local *)dev->priv;
	u8 trl_elem_size = (u8)(msg[3]>>8 & 0x000000FF);
	u8 trl_count = (u8)(msg[3] & 0x000000FF);
	u32 *pskb = &msg[6];

	while (trl_count--) {
		dprintk(KERN_DEBUG "%s: Releasing unused rx skb %p (trl_count=%d).\n",
			dev->name, (struct sk_buff*)(*pskb),trl_count+1);
		dev_kfree_skb_irq((struct sk_buff *)(*pskb));
		pskb += 1 + trl_elem_size;
		atomic_dec(&priv->buckets_out);
	}
}

/*
 * i2o_lan_event_reply(): Handle events.
 */
static void i2o_lan_handle_event(struct net_device *dev, u32 *msg)
{
	struct i2o_lan_local *priv = (struct i2o_lan_local *)dev->priv;
	struct i2o_device *i2o_dev = priv->i2o_dev;
	struct i2o_controller *iop = i2o_dev->controller;
	u32 max_evt_data_size =iop->status_block->inbound_frame_size-5;
	struct i2o_reply {
		u32 header[4];
		u32 evt_indicator;
		u32 data[max_evt_data_size];
	} *evt = (struct i2o_reply *)msg;
	int evt_data_len = ((msg[0]>>16) - 5) * 4; /* real size*/

	printk(KERN_INFO "%s: I2O event - ", dev->name);

	if (msg[1]>>24 == I2O_CMD_UTIL_EVT_ACK) {
		printk("Event acknowledgement reply.\n");
		return;
	}

	/* Else evt->function == I2O_CMD_UTIL_EVT_REGISTER) */

	switch (evt->evt_indicator) {
	case I2O_EVT_IND_STATE_CHANGE:  {
		struct state_data {
			u16 status;
			u8 state;
			u8 data;
		} *evt_data = (struct state_data *)(evt->data[0]);

		printk("State chance 0x%08x.\n", evt->data[0]);

		/* If the DDM is in error state, recovery may be
		 * possible if status = Transmit or Receive Control
		 * Unit Inoperable.
		 */
		if (evt_data->state==0x05 && evt_data->status==0x0003)
			i2o_lan_reset(dev);
		break;
	}

	case I2O_EVT_IND_FIELD_MODIFIED: {
		u16 *work16 = (u16 *)evt->data;
		printk("Group 0x%04x, field %d changed.\n", work16[0], work16[1]);
		break;
	}

	case I2O_EVT_IND_VENDOR_EVT: {
		int i;
		printk("Vendor event:\n");
		for (i = 0; i < evt_data_len / 4; i++)
			printk("   0x%08x\n", evt->data[i]);
		break;
	}

	case I2O_EVT_IND_DEVICE_RESET:
		/* Spec 2.0 p. 6-121:
		 * The event of _DEVICE_RESET should also be responded
		 */
		printk("Device reset.\n");
		if (i2o_event_ack(iop, msg) < 0)
			printk("%s: Event Acknowledge timeout.\n", dev->name);
		break;

#if 0
	case I2O_EVT_IND_EVT_MASK_MODIFIED:
		printk("Event mask modified, 0x%08x.\n", evt->data[0]);
		break;

	case I2O_EVT_IND_GENERAL_WARNING:
		printk("General warning 0x%04x.\n", evt->data[0]);
		break;

	case I2O_EVT_IND_CONFIGURATION_FLAG:
		printk("Configuration requested.\n");
		break;

	case I2O_EVT_IND_CAPABILITY_CHANGE:
		printk("Capability change 0x%04x.\n", evt->data[0]);
		break;

	case I2O_EVT_IND_DEVICE_STATE:
		printk("Device state changed 0x%08x.\n", evt->data[0]);
		break;
#endif
	case I2O_LAN_EVT_LINK_DOWN:
		netif_carrier_off(dev); 
		printk("Link to the physical device is lost.\n");
		break;

	case I2O_LAN_EVT_LINK_UP:
		netif_carrier_on(dev); 
		printk("Link to the physical device is (re)established.\n");
		break;

	case I2O_LAN_EVT_MEDIA_CHANGE:
		printk("Media change.\n");
		break;
	default:
		printk("0x%08x. No handler.\n", evt->evt_indicator);
	}
}

/*
 * i2o_lan_receive_post(): Post buckets to receive packets.
 */
static int i2o_lan_receive_post(struct net_device *dev)
{
	struct i2o_lan_local *priv = (struct i2o_lan_local *)dev->priv;
	struct i2o_device *i2o_dev = priv->i2o_dev;
	struct i2o_controller *iop = i2o_dev->controller;
	struct sk_buff *skb;
	u32 m, *msg;
	u32 bucket_len = (dev->mtu + dev->hard_header_len);
	u32 total = priv->max_buckets_out - atomic_read(&priv->buckets_out);
	u32 bucket_count;
	u32 *sgl_elem;
	unsigned long flags;

	/* Send (total/bucket_count) separate I2O requests */

	while (total) {
		m = I2O_POST_READ32(iop);
		if (m == 0xFFFFFFFF)
			return -ETIMEDOUT;
		msg = (u32 *)(iop->mem_offset + m);

		bucket_count = (total >= priv->sgl_max) ? priv->sgl_max : total;
		total -= bucket_count;
		atomic_add(bucket_count, &priv->buckets_out);

		dprintk(KERN_INFO "%s: Sending %d buckets (size %d) to LAN DDM.\n",
			dev->name, bucket_count, bucket_len);

		/* Fill in the header */

		__raw_writel(I2O_MESSAGE_SIZE(4 + 3 * bucket_count) | SGL_OFFSET_4, msg);
		__raw_writel(LAN_RECEIVE_POST<<24 | HOST_TID<<12 | i2o_dev->lct_data.tid, msg+1);
		__raw_writel(priv->unit << 16 | lan_receive_context, msg+2);
		__raw_writel(bucket_count, msg+3);
		sgl_elem = &msg[4];

		/* Fill in the payload - contains bucket_count SGL elements */

		while (bucket_count--) {
			spin_lock_irqsave(&priv->fbl_lock, flags);
			if (priv->i2o_fbl_tail >= 0)
				skb = priv->i2o_fbl[priv->i2o_fbl_tail--];
			else {
				skb = dev_alloc_skb(bucket_len + 2);
				if (skb == NULL) {
					spin_unlock_irqrestore(&priv->fbl_lock, flags);
					return -ENOMEM;
				}
				skb_reserve(skb, 2);
			}
			spin_unlock_irqrestore(&priv->fbl_lock, flags);

			__raw_writel(0x51000000 | bucket_len, sgl_elem);
			__raw_writel((u32)skb,		      sgl_elem+1);
			__raw_writel(virt_to_bus(skb->data),  sgl_elem+2);
			sgl_elem += 3;
		}

		/* set LE flag and post  */
		__raw_writel(__raw_readl(sgl_elem-3) | 0x80000000, (sgl_elem-3));
		i2o_post_message(iop, m);
	}

	return 0;
}

/* Functions called from the network stack, and functions called by them:
========================================================================*/

/*
 * i2o_lan_reset(): Reset the LAN adapter into the operational state and
 * 	restore it to full operation.
 */
static int i2o_lan_reset(struct net_device *dev)
{
	struct i2o_lan_local *priv = (struct i2o_lan_local *)dev->priv;
	struct i2o_device *i2o_dev = priv->i2o_dev;
	struct i2o_controller *iop = i2o_dev->controller;
	u32 msg[5];

	dprintk(KERN_INFO "%s: LAN RESET MESSAGE.\n", dev->name);
	msg[0] = FIVE_WORD_MSG_SIZE | SGL_OFFSET_0;
	msg[1] = LAN_RESET<<24 | HOST_TID<<12 | i2o_dev->lct_data.tid;
	msg[2] = priv->unit << 16 | lan_context; // InitiatorContext
	msg[3] = 0; 				 // TransactionContext
	msg[4] = 0;				 // Keep posted buckets

	if (i2o_post_this(iop, msg, sizeof(msg)) < 0)
		return -ETIMEDOUT;

	return 0;
}

/*
 * i2o_lan_suspend(): Put LAN adapter into a safe, non-active state.
 * 	IOP replies to any LAN class message with status error_no_data_transfer
 *	/ suspended.
 */
static int i2o_lan_suspend(struct net_device *dev)
{
	struct i2o_lan_local *priv = (struct i2o_lan_local *)dev->priv;
	struct i2o_device *i2o_dev = priv->i2o_dev;
	struct i2o_controller *iop = i2o_dev->controller;
	u32 msg[5];

	dprintk(KERN_INFO "%s: LAN SUSPEND MESSAGE.\n", dev->name);
	msg[0] = FIVE_WORD_MSG_SIZE | SGL_OFFSET_0;
	msg[1] = LAN_SUSPEND<<24 | HOST_TID<<12 | i2o_dev->lct_data.tid;
	msg[2] = priv->unit << 16 | lan_context; // InitiatorContext
	msg[3] = 0; 				 // TransactionContext
	msg[4] = 1 << 16; 			 // return posted buckets

	if (i2o_post_this(iop, msg, sizeof(msg)) < 0)
		return -ETIMEDOUT;

	return 0;
}

/*
 * i2o_set_ddm_parameters:
 * These settings are done to ensure proper initial values for DDM.
 * They can be changed via proc file system or vai configuration utility.
 */
static void i2o_set_ddm_parameters(struct net_device *dev)
{
	struct i2o_lan_local *priv = (struct i2o_lan_local *)dev->priv;
	struct i2o_device *i2o_dev = priv->i2o_dev;
	struct i2o_controller *iop = i2o_dev->controller;
	u32 val;

	/*
	 * When PacketOrphanlimit is set to the maximum packet length,
	 * the packets will never be split into two separate buckets
	 */
	val = dev->mtu + dev->hard_header_len;
	if (i2o_set_scalar(iop, i2o_dev->lct_data.tid, 0x0004, 2, &val, sizeof(val)) < 0)
		printk(KERN_WARNING "%s: Unable to set PacketOrphanLimit.\n",
		       dev->name);
	else
		dprintk(KERN_INFO "%s: PacketOrphanLimit set to %d.\n",
			dev->name, val);

	/* When RxMaxPacketsBucket = 1, DDM puts only one packet into bucket */

	val = 1;
	if (i2o_set_scalar(iop, i2o_dev->lct_data.tid, 0x0008, 4, &val, sizeof(val)) <0)
		printk(KERN_WARNING "%s: Unable to set RxMaxPacketsBucket.\n",
		       dev->name);
	else
		dprintk(KERN_INFO "%s: RxMaxPacketsBucket set to %d.\n", 
			dev->name, val);
	return;
}

/* Functions called from the network stack:
==========================================*/

/*
 * i2o_lan_open(): Open the device to send/receive packets via
 * the network device.
 */
static int i2o_lan_open(struct net_device *dev)
{
	struct i2o_lan_local *priv = (struct i2o_lan_local *)dev->priv;
	struct i2o_device *i2o_dev = priv->i2o_dev;
	struct i2o_controller *iop = i2o_dev->controller;
	u32 mc_addr_group[64];

	MOD_INC_USE_COUNT;

	if (i2o_claim_device(i2o_dev, &i2o_lan_handler)) {
		printk(KERN_WARNING "%s: Unable to claim the I2O LAN device.\n", dev->name);
		MOD_DEC_USE_COUNT;
		return -EAGAIN;
	}
	dprintk(KERN_INFO "%s: I2O LAN device (tid=%d) claimed by LAN OSM.\n",
		dev->name, i2o_dev->lct_data.tid);

	if (i2o_event_register(iop, i2o_dev->lct_data.tid,
			       priv->unit << 16 | lan_context, 0, priv->i2o_event_mask) < 0)
		printk(KERN_WARNING "%s: Unable to set the event mask.\n", dev->name);

	i2o_lan_reset(dev);

	/* Get the max number of multicast addresses */

	if (i2o_query_scalar(iop, i2o_dev->lct_data.tid, 0x0001, -1,
			     &mc_addr_group, sizeof(mc_addr_group)) < 0 ) {
		printk(KERN_WARNING "%s: Unable to query LAN_MAC_ADDRESS group.\n", dev->name);
		MOD_DEC_USE_COUNT;
		return -EAGAIN;
	}
	priv->max_size_mc_table = mc_addr_group[8];

	/* Malloc space for free bucket list to resuse reveive post buckets */

	priv->i2o_fbl = kmalloc(priv->max_buckets_out * sizeof(struct sk_buff *),
				GFP_KERNEL);
	if (priv->i2o_fbl == NULL) {
		MOD_DEC_USE_COUNT;
		return -ENOMEM;
	}
	priv->i2o_fbl_tail = -1;
	priv->send_active = 0;

	i2o_set_ddm_parameters(dev);
	i2o_lan_receive_post(dev);

	netif_start_queue(dev);

	return 0;
}

/*
 * i2o_lan_close(): End the transfering.
 */
static int i2o_lan_close(struct net_device *dev)
{
	struct i2o_lan_local *priv = (struct i2o_lan_local *)dev->priv;
	struct i2o_device *i2o_dev = priv->i2o_dev;
	struct i2o_controller *iop = i2o_dev->controller;
	int ret = 0;

	netif_stop_queue(dev);
	i2o_lan_suspend(dev);

	if (i2o_event_register(iop, i2o_dev->lct_data.tid,
			       priv->unit << 16 | lan_context, 0, 0) < 0)
		printk(KERN_WARNING "%s: Unable to clear the event mask.\n",
		       dev->name);

	while (priv->i2o_fbl_tail >= 0)
		dev_kfree_skb(priv->i2o_fbl[priv->i2o_fbl_tail--]);

	kfree(priv->i2o_fbl);

	if (i2o_release_device(i2o_dev, &i2o_lan_handler)) {
		printk(KERN_WARNING "%s: Unable to unclaim I2O LAN device "
		       "(tid=%d).\n", dev->name, i2o_dev->lct_data.tid);
		ret = -EBUSY;
	}

	MOD_DEC_USE_COUNT;

	return ret;
}

/*
 * i2o_lan_tx_timeout(): Tx timeout handler.
 */
static void i2o_lan_tx_timeout(struct net_device *dev)
{
 	if (!netif_queue_stopped(dev))
		netif_start_queue(dev);
}

/*
 * i2o_lan_batch_send(): Send packets in batch. 
 * Both i2o_lan_sdu_send and i2o_lan_packet_send use this.
 */
static void i2o_lan_batch_send(struct net_device *dev)
{
	struct i2o_lan_local *priv = (struct i2o_lan_local *)dev->priv;
	struct i2o_controller *iop = priv->i2o_dev->controller;

	spin_lock_irq(&priv->tx_lock);
	if (priv->tx_count != 0) {
		dev->trans_start = jiffies;
		i2o_post_message(iop, priv->m);
		dprintk(KERN_DEBUG "%s: %d packets sent.\n", dev->name, priv->tx_count);
		priv->tx_count = 0;
	}
	priv->send_active = 0;
	spin_unlock_irq(&priv->tx_lock);
	MOD_DEC_USE_COUNT;
}

#ifdef CONFIG_NET_FC
/*
 * i2o_lan_sdu_send(): Send a packet, MAC header added by the DDM.
 * Must be supported by Fibre Channel, optional for Ethernet/802.3,
 * Token Ring, FDDI
 */
static int i2o_lan_sdu_send(struct sk_buff *skb, struct net_device *dev)
{
	struct i2o_lan_local *priv = (struct i2o_lan_local *)dev->priv;
	struct i2o_device *i2o_dev = priv->i2o_dev;
	struct i2o_controller *iop = i2o_dev->controller;
	int tickssofar = jiffies - dev->trans_start;
	u32 m, *msg;
	u32 *sgl_elem;

	spin_lock_irq(&priv->tx_lock);

	priv->tx_count++;
	atomic_inc(&priv->tx_out);

	/* 
	 * If tx_batch_mode = 0x00 forced to immediate mode
	 * If tx_batch_mode = 0x01 forced to batch mode
	 * If tx_batch_mode = 0x10 switch automatically, current mode immediate
	 * If tx_batch_mode = 0x11 switch automatically, current mode batch
	 *	If gap between two packets is > 0 ticks, switch to immediate
	 */
	if (priv->tx_batch_mode >> 1) // switch automatically
		priv->tx_batch_mode = tickssofar ? 0x02 : 0x03;

	if (priv->tx_count == 1) {
		m = I2O_POST_READ32(iop);
		if (m == 0xFFFFFFFF) {
			spin_unlock_irq(&priv->tx_lock);
			return 1;
		}
		msg = (u32 *)(iop->mem_offset + m);
		priv->m = m;

		__raw_writel(NINE_WORD_MSG_SIZE | 1<<12 | SGL_OFFSET_4, msg);
		__raw_writel(LAN_PACKET_SEND<<24 | HOST_TID<<12 | i2o_dev->lct_data.tid, msg+1);
		__raw_writel(priv->unit << 16 | lan_send_context, msg+2); // InitiatorContext
		__raw_writel(1 << 30 | 1 << 3, msg+3); 		 	  // TransmitControlWord

		__raw_writel(0xD7000000 | skb->len, msg+4);  	     // MAC hdr included
		__raw_writel((u32)skb, msg+5);  		     // TransactionContext
		__raw_writel(virt_to_bus(skb->data), msg+6);
		__raw_writel((u32)skb->mac.raw, msg+7);
		__raw_writel((u32)skb->mac.raw+4, msg+8);

		if ((priv->tx_batch_mode & 0x01) && !priv->send_active) {
			priv->send_active = 1;
			MOD_INC_USE_COUNT;
			if (schedule_task(&priv->i2o_batch_send_task) == 0)
				MOD_DEC_USE_COUNT;
		}
	} else {  /* Add new SGL element to the previous message frame */

		msg = (u32 *)(iop->mem_offset + priv->m);
		sgl_elem = &msg[priv->tx_count * 5 + 1];

		__raw_writel(I2O_MESSAGE_SIZE((__raw_readl(msg)>>16) + 5) | 1<<12 | SGL_OFFSET_4, msg);
		__raw_writel(__raw_readl(sgl_elem-5) & 0x7FFFFFFF, sgl_elem-5); /* clear LE flag */
		__raw_writel(0xD5000000 | skb->len, sgl_elem);
		__raw_writel((u32)skb, sgl_elem+1);
		__raw_writel(virt_to_bus(skb->data), sgl_elem+2);
		__raw_writel((u32)(skb->mac.raw), sgl_elem+3);
		__raw_writel((u32)(skb->mac.raw)+1, sgl_elem+4);
	}

	/* If tx not in batch mode or frame is full, send immediatelly */

	if (!(priv->tx_batch_mode & 0x01) || priv->tx_count == priv->sgl_max) {
		dev->trans_start = jiffies;
		i2o_post_message(iop, priv->m);
		dprintk(KERN_DEBUG "%s: %d packets sent.\n", dev->name, priv->tx_count);
		priv->tx_count = 0;
	}

	/* If DDMs TxMaxPktOut reached, stop queueing layer to send more */

	if (atomic_read(&priv->tx_out) >= priv->tx_max_out)
		netif_stop_queue(dev);

	spin_unlock_irq(&priv->tx_lock);
	return 0;
}
#endif /* CONFIG_NET_FC */

/*
 * i2o_lan_packet_send(): Send a packet as is, including the MAC header.
 *
 * Must be supported by Ethernet/802.3, Token Ring, FDDI, optional for
 * Fibre Channel
 */
static int i2o_lan_packet_send(struct sk_buff *skb, struct net_device *dev)
{
	struct i2o_lan_local *priv = (struct i2o_lan_local *)dev->priv;
	struct i2o_device *i2o_dev = priv->i2o_dev;
	struct i2o_controller *iop = i2o_dev->controller;
	int tickssofar = jiffies - dev->trans_start;
	u32 m, *msg;
	u32 *sgl_elem;

	spin_lock_irq(&priv->tx_lock);

	priv->tx_count++;
	atomic_inc(&priv->tx_out);

	/* 
	 * If tx_batch_mode = 0x00 forced to immediate mode
	 * If tx_batch_mode = 0x01 forced to batch mode
	 * If tx_batch_mode = 0x10 switch automatically, current mode immediate
	 * If tx_batch_mode = 0x11 switch automatically, current mode batch
	 *	If gap between two packets is > 0 ticks, switch to immediate
	 */
	if (priv->tx_batch_mode >> 1) // switch automatically
		priv->tx_batch_mode = tickssofar ? 0x02 : 0x03;

	if (priv->tx_count == 1) {
		m = I2O_POST_READ32(iop);
		if (m == 0xFFFFFFFF) {
			spin_unlock_irq(&priv->tx_lock);
			return 1;
		}
		msg = (u32 *)(iop->mem_offset + m);
		priv->m = m;

		__raw_writel(SEVEN_WORD_MSG_SIZE | 1<<12 | SGL_OFFSET_4, msg);
		__raw_writel(LAN_PACKET_SEND<<24 | HOST_TID<<12 | i2o_dev->lct_data.tid, msg+1);
		__raw_writel(priv->unit << 16 | lan_send_context, msg+2); // InitiatorContext
		__raw_writel(1 << 30 | 1 << 3, msg+3); 		 	  // TransmitControlWord
			// bit 30: reply as soon as transmission attempt is complete
			// bit 3: Suppress CRC generation
		__raw_writel(0xD5000000 | skb->len, msg+4);  	     // MAC hdr included
		__raw_writel((u32)skb, msg+5);  		     // TransactionContext
		__raw_writel(virt_to_bus(skb->data), msg+6);

		if ((priv->tx_batch_mode & 0x01) && !priv->send_active) {
			priv->send_active = 1;
			MOD_INC_USE_COUNT;
			if (schedule_task(&priv->i2o_batch_send_task) == 0)
				MOD_DEC_USE_COUNT;
		}
	} else {  /* Add new SGL element to the previous message frame */

		msg = (u32 *)(iop->mem_offset + priv->m);
		sgl_elem = &msg[priv->tx_count * 3 + 1];

		__raw_writel(I2O_MESSAGE_SIZE((__raw_readl(msg)>>16) + 3) | 1<<12 | SGL_OFFSET_4, msg);
		__raw_writel(__raw_readl(sgl_elem-3) & 0x7FFFFFFF, sgl_elem-3); /* clear LE flag */
		__raw_writel(0xD5000000 | skb->len, sgl_elem);
		__raw_writel((u32)skb, sgl_elem+1);
		__raw_writel(virt_to_bus(skb->data), sgl_elem+2);
	}

	/* If tx is in immediate mode or frame is full, send now */

	if (!(priv->tx_batch_mode & 0x01) || priv->tx_count == priv->sgl_max) {
		dev->trans_start = jiffies;
		i2o_post_message(iop, priv->m);
		dprintk(KERN_DEBUG "%s: %d packets sent.\n", dev->name, priv->tx_count);
		priv->tx_count = 0;
	}

	/* If DDMs TxMaxPktOut reached, stop queueing layer to send more */

	if (atomic_read(&priv->tx_out) >= priv->tx_max_out)
		netif_stop_queue(dev);

	spin_unlock_irq(&priv->tx_lock);
	return 0;
}

/*
 * i2o_lan_get_stats(): Fill in the statistics.
 */
static struct net_device_stats *i2o_lan_get_stats(struct net_device *dev)
{
	struct i2o_lan_local *priv = (struct i2o_lan_local *)dev->priv;
	struct i2o_device *i2o_dev = priv->i2o_dev;
	struct i2o_controller *iop = i2o_dev->controller;
	u64 val64[16];
	u64 supported_group[4] = { 0, 0, 0, 0 };

	if (i2o_query_scalar(iop, i2o_dev->lct_data.tid, 0x0100, -1, val64,
			     sizeof(val64)) < 0)
		printk(KERN_INFO "%s: Unable to query LAN_HISTORICAL_STATS.\n", dev->name);
	else {
		dprintk(KERN_DEBUG "%s: LAN_HISTORICAL_STATS queried.\n", dev->name);
		priv->stats.tx_packets = val64[0];
		priv->stats.tx_bytes   = val64[1];
		priv->stats.rx_packets = val64[2];
		priv->stats.rx_bytes   = val64[3];
		priv->stats.tx_errors  = val64[4];
		priv->stats.rx_errors  = val64[5];
		priv->stats.rx_dropped = val64[6];
	}

	if (i2o_query_scalar(iop, i2o_dev->lct_data.tid, 0x0180, -1,
			     &supported_group, sizeof(supported_group)) < 0)
		printk(KERN_INFO "%s: Unable to query LAN_SUPPORTED_OPTIONAL_HISTORICAL_STATS.\n", dev->name);

	if (supported_group[2]) {
		if (i2o_query_scalar(iop, i2o_dev->lct_data.tid, 0x0183, -1,
				     val64, sizeof(val64)) < 0)
			printk(KERN_INFO "%s: Unable to query LAN_OPTIONAL_RX_HISTORICAL_STATS.\n", dev->name);
		else {
			dprintk(KERN_DEBUG "%s: LAN_OPTIONAL_RX_HISTORICAL_STATS queried.\n", dev->name);
			priv->stats.multicast	     = val64[4];
			priv->stats.rx_length_errors = val64[10];
			priv->stats.rx_crc_errors    = val64[0];
		}
	}

	if (i2o_dev->lct_data.sub_class == I2O_LAN_ETHERNET) {
		u64 supported_stats = 0;
		if (i2o_query_scalar(iop, i2o_dev->lct_data.tid, 0x0200, -1,
				     val64, sizeof(val64)) < 0)
			printk(KERN_INFO "%s: Unable to query LAN_802_3_HISTORICAL_STATS.\n", dev->name);
		else {
			dprintk(KERN_DEBUG "%s: LAN_802_3_HISTORICAL_STATS queried.\n", dev->name);
	 		priv->stats.transmit_collision = val64[1] + val64[2];
			priv->stats.rx_frame_errors    = val64[0];
			priv->stats.tx_carrier_errors  = val64[6];
		}

		if (i2o_query_scalar(iop, i2o_dev->lct_data.tid, 0x0280, -1,
				     &supported_stats, sizeof(supported_stats)) < 0)
			printk(KERN_INFO "%s: Unable to query LAN_SUPPORTED_802_3_HISTORICAL_STATS.\n", dev->name);

		if (supported_stats != 0) {
			if (i2o_query_scalar(iop, i2o_dev->lct_data.tid, 0x0281, -1,
					     val64, sizeof(val64)) < 0)
				printk(KERN_INFO "%s: Unable to query LAN_OPTIONAL_802_3_HISTORICAL_STATS.\n", dev->name);
			else {
				dprintk(KERN_DEBUG "%s: LAN_OPTIONAL_802_3_HISTORICAL_STATS queried.\n", dev->name);
				if (supported_stats & 0x1)
					priv->stats.rx_over_errors = val64[0];
				if (supported_stats & 0x4)
					priv->stats.tx_heartbeat_errors = val64[2];
			}
		}
	}

#ifdef CONFIG_TR
	if (i2o_dev->lct_data.sub_class == I2O_LAN_TR) {
		if (i2o_query_scalar(iop, i2o_dev->lct_data.tid, 0x0300, -1,
				     val64, sizeof(val64)) < 0)
			printk(KERN_INFO "%s: Unable to query LAN_802_5_HISTORICAL_STATS.\n", dev->name);
		else {
			struct tr_statistics *stats =
				(struct tr_statistics *)&priv->stats;
			dprintk(KERN_DEBUG "%s: LAN_802_5_HISTORICAL_STATS queried.\n", dev->name);

			stats->line_errors		= val64[0];
			stats->internal_errors		= val64[7];
			stats->burst_errors		= val64[4];
			stats->A_C_errors		= val64[2];
			stats->abort_delimiters		= val64[3];
			stats->lost_frames		= val64[1];
			/* stats->recv_congest_count	= ?;  FIXME ??*/
			stats->frame_copied_errors	= val64[5];
			stats->frequency_errors		= val64[6];
			stats->token_errors		= val64[9];
		}
		/* Token Ring optional stats not yet defined */
	}
#endif

#ifdef CONFIG_FDDI
	if (i2o_dev->lct_data.sub_class == I2O_LAN_FDDI) {
		if (i2o_query_scalar(iop, i2o_dev->lct_data.tid, 0x0400, -1,
				     val64, sizeof(val64)) < 0)
			printk(KERN_INFO "%s: Unable to query LAN_FDDI_HISTORICAL_STATS.\n", dev->name);
		else {
			dprintk(KERN_DEBUG "%s: LAN_FDDI_HISTORICAL_STATS queried.\n", dev->name);
			priv->stats.smt_cf_state = val64[0];
			memcpy(priv->stats.mac_upstream_nbr, &val64[1], FDDI_K_ALEN);
			memcpy(priv->stats.mac_downstream_nbr, &val64[2], FDDI_K_ALEN);
			priv->stats.mac_error_cts = val64[3];
			priv->stats.mac_lost_cts  = val64[4];
			priv->stats.mac_rmt_state = val64[5];
			memcpy(priv->stats.port_lct_fail_cts, &val64[6], 8);
			memcpy(priv->stats.port_lem_reject_cts, &val64[7], 8);
			memcpy(priv->stats.port_lem_cts, &val64[8], 8);
			memcpy(priv->stats.port_pcm_state, &val64[9], 8);
		}
		/* FDDI optional stats not yet defined */
	}
#endif

#ifdef CONFIG_NET_FC
	/* Fibre Channel Statistics not yet defined in 1.53 nor 2.0 */
#endif

	return (struct net_device_stats *)&priv->stats;
}

/* 
 * i2o_lan_set_mc_filter(): Post a request to set multicast filter.
 */
int i2o_lan_set_mc_filter(struct net_device *dev, u32 filter_mask)
{
	struct i2o_lan_local *priv = (struct i2o_lan_local *)dev->priv;	
	struct i2o_device *i2o_dev = priv->i2o_dev;
	struct i2o_controller *iop = i2o_dev->controller;
	u32 msg[10]; 

	msg[0] = TEN_WORD_MSG_SIZE | SGL_OFFSET_5;
	msg[1] = I2O_CMD_UTIL_PARAMS_SET << 24 | HOST_TID << 12 | i2o_dev->lct_data.tid;
	msg[2] = priv->unit << 16 | lan_context;
	msg[3] = 0x0001 << 16 | 3 ;	// TransactionContext: group&field
	msg[4] = 0;
	msg[5] = 0xCC000000 | 16; 			// Immediate data SGL
	msg[6] = 1;					// OperationCount
	msg[7] = 0x0001<<16 | I2O_PARAMS_FIELD_SET;	// Group, Operation
	msg[8] = 3 << 16 | 1; 				// FieldIndex, FieldCount 
	msg[9] = filter_mask;				// Value

	return i2o_post_this(iop, msg, sizeof(msg));
}

/* 
 * i2o_lan_set_mc_table(): Post a request to set LAN_MULTICAST_MAC_ADDRESS table.
 */
int i2o_lan_set_mc_table(struct net_device *dev)
{
	struct i2o_lan_local *priv = (struct i2o_lan_local *)dev->priv;	
	struct i2o_device *i2o_dev = priv->i2o_dev;
	struct i2o_controller *iop = i2o_dev->controller;
	struct dev_mc_list *mc;
	u32 msg[10 + 2 * dev->mc_count]; 
	u8 *work8 = (u8 *)(msg + 10);

	msg[0] = I2O_MESSAGE_SIZE(10 + 2 * dev->mc_count) | SGL_OFFSET_5;
	msg[1] = I2O_CMD_UTIL_PARAMS_SET << 24 | HOST_TID << 12 | i2o_dev->lct_data.tid;
	msg[2] = priv->unit << 16 | lan_context;	// InitiatorContext
	msg[3] = 0x0002 << 16 | (u16)-1;		// TransactionContext
	msg[4] = 0;					// OperationFlags
	msg[5] = 0xCC000000 | (16 + 8 * dev->mc_count);	// Immediate data SGL
	msg[6] = 2;					// OperationCount
	msg[7] = 0x0002 << 16 | I2O_PARAMS_TABLE_CLEAR;	// Group, Operation
	msg[8] = 0x0002 << 16 | I2O_PARAMS_ROW_ADD;     // Group, Operation
	msg[9] = dev->mc_count << 16 | (u16)-1; 	// RowCount, FieldCount

        for (mc = dev->mc_list; mc ; mc = mc->next, work8 += 8) {
		  memset(work8, 0, 8);
                  memcpy(work8, mc->dmi_addr, mc->dmi_addrlen); // Values
	}

	return i2o_post_this(iop, msg, sizeof(msg));
}

/*
 * i2o_lan_set_multicast_list(): Enable a network device to receive packets
 *      not send to the protocol address.
 */
static void i2o_lan_set_multicast_list(struct net_device *dev)
{
	struct i2o_lan_local *priv = (struct i2o_lan_local *)dev->priv;
	u32 filter_mask;

	if (dev->flags & IFF_PROMISC) {
		filter_mask = 0x00000002;
		dprintk(KERN_INFO "%s: Enabling promiscuous mode...\n", dev->name);
	} else if ((dev->flags & IFF_ALLMULTI) || dev->mc_count > priv->max_size_mc_table) {
		filter_mask = 0x00000004;
		dprintk(KERN_INFO "%s: Enabling all multicast mode...\n", dev->name);
	} else if (dev->mc_count) {
		filter_mask = 0x00000000;
		dprintk(KERN_INFO "%s: Enabling multicast mode...\n", dev->name);
		if (i2o_lan_set_mc_table(dev) < 0)
			printk(KERN_WARNING "%s: Unable to send MAC table.\n", dev->name);
	} else {
		filter_mask = 0x00000300; // Broadcast, Multicast disabled
		dprintk(KERN_INFO "%s: Enabling unicast mode...\n", dev->name);
	}

	/* Finally copy new FilterMask to DDM */

	if (i2o_lan_set_mc_filter(dev, filter_mask) < 0)
		printk(KERN_WARNING "%s: Unable to send MAC FilterMask.\n", dev->name);
}

/*
 * i2o_lan_change_mtu(): Change maximum transfer unit size.
 */
static int i2o_lan_change_mtu(struct net_device *dev, int new_mtu)
{
	struct i2o_lan_local *priv = (struct i2o_lan_local *)dev->priv;
	struct i2o_device *i2o_dev = priv->i2o_dev;
	u32 max_pkt_size;

	if (i2o_query_scalar(i2o_dev->controller, i2o_dev->lct_data.tid,
		 	     0x0000, 6, &max_pkt_size, 4) < 0)
		return -EFAULT;

	if (new_mtu < 68 || new_mtu > 9000 || new_mtu > max_pkt_size)
		return -EINVAL;

	dev->mtu = new_mtu;

	i2o_lan_suspend(dev);   	// to SUSPENDED state, return buckets

	while (priv->i2o_fbl_tail >= 0) // free buffered buckets
		dev_kfree_skb(priv->i2o_fbl[priv->i2o_fbl_tail--]);

	i2o_lan_reset(dev);		// to OPERATIONAL state
	i2o_set_ddm_parameters(dev); 	// reset some parameters
	i2o_lan_receive_post(dev); 	// post new buckets (new size)

	return 0;
}

/* Functions to initialize I2O LAN OSM:
======================================*/

/*
 * i2o_lan_register_device(): Register LAN class device to kernel.
 */
struct net_device *i2o_lan_register_device(struct i2o_device *i2o_dev)
{
	struct net_device *dev = NULL;
	struct i2o_lan_local *priv = NULL;
	u8 hw_addr[8];
	u32 tx_max_out = 0;
	unsigned short (*type_trans)(struct sk_buff *, struct net_device *);
	void (*unregister_dev)(struct net_device *dev);

	switch (i2o_dev->lct_data.sub_class) {
	case I2O_LAN_ETHERNET:
		dev = init_etherdev(NULL, sizeof(struct i2o_lan_local));
		if (dev == NULL)
			return NULL;
		type_trans = eth_type_trans;
		unregister_dev = unregister_netdev;
		break;

#ifdef CONFIG_ANYLAN
	case I2O_LAN_100VG:
		printk(KERN_ERR "i2o_lan: 100base VG not yet supported.\n");
		return NULL;
		break;
#endif

#ifdef CONFIG_TR
	case I2O_LAN_TR:
		dev = init_trdev(NULL, sizeof(struct i2o_lan_local));
		if (dev==NULL)
			return NULL;
		type_trans = tr_type_trans;
		unregister_dev = unregister_trdev;
		break;
#endif

#ifdef CONFIG_FDDI
	case I2O_LAN_FDDI:
	{
		int size = sizeof(struct net_device) + sizeof(struct i2o_lan_local);

		dev = (struct net_device *) kmalloc(size, GFP_KERNEL);
		if (dev == NULL)
			return NULL;
		memset((char *)dev, 0, size);
	    	dev->priv = (void *)(dev + 1);

		if (dev_alloc_name(dev, "fddi%d") < 0) {
			printk(KERN_WARNING "i2o_lan: Too many FDDI devices.\n");
			kfree(dev);
			return NULL;
		}
		type_trans = fddi_type_trans;
		unregister_dev = (void *)unregister_netdevice;

		fddi_setup(dev);
		register_netdev(dev);
	}
	break;
#endif

#ifdef CONFIG_NET_FC
	case I2O_LAN_FIBRE_CHANNEL:
		dev = init_fcdev(NULL, sizeof(struct i2o_lan_local));
		if (dev == NULL)
			return NULL;
		type_trans = NULL;
/* FIXME: Move fc_type_trans() from drivers/net/fc/iph5526.c to net/802/fc.c
 * and export it in include/linux/fcdevice.h
 *		type_trans = fc_type_trans;
 */
		unregister_dev = (void *)unregister_fcdev;
		break;
#endif

	case I2O_LAN_UNKNOWN:
	default:
		printk(KERN_ERR "i2o_lan: LAN type 0x%04x not supported.\n",
		       i2o_dev->lct_data.sub_class);
		return NULL;
	}

	priv = (struct i2o_lan_local *)dev->priv;
	priv->i2o_dev = i2o_dev;
	priv->type_trans = type_trans;
	priv->sgl_max = (i2o_dev->controller->status_block->inbound_frame_size - 4) / 3;
	atomic_set(&priv->buckets_out, 0);

	/* Set default values for user configurable parameters */
	/* Private values are changed via /proc file system */

	priv->max_buckets_out = max_buckets_out;
	priv->bucket_thresh   = bucket_thresh;
	priv->rx_copybreak    = rx_copybreak;
	priv->tx_batch_mode   = tx_batch_mode & 0x03;
	priv->i2o_event_mask  = i2o_event_mask;

	priv->tx_lock	      = SPIN_LOCK_UNLOCKED;
	priv->fbl_lock	      = SPIN_LOCK_UNLOCKED;

	unit++;
	i2o_landevs[unit] = dev;
	priv->unit = unit;

	if (i2o_query_scalar(i2o_dev->controller, i2o_dev->lct_data.tid,
			     0x0001, 0, &hw_addr, sizeof(hw_addr)) < 0) {
		printk(KERN_ERR "%s: Unable to query hardware address.\n", dev->name);
		unit--;
		unregister_dev(dev);
		kfree(dev);
		return NULL;
	}
	dprintk(KERN_DEBUG "%s: hwaddr = %02X:%02X:%02X:%02X:%02X:%02X\n",
 		dev->name, hw_addr[0], hw_addr[1], hw_addr[2], hw_addr[3],
		hw_addr[4], hw_addr[5]);

	dev->addr_len = 6;
	memcpy(dev->dev_addr, hw_addr, 6);

	if (i2o_query_scalar(i2o_dev->controller, i2o_dev->lct_data.tid,
			     0x0007, 2, &tx_max_out, sizeof(tx_max_out)) < 0) {
		printk(KERN_ERR "%s: Unable to query max TX queue.\n", dev->name);
		unit--;
		unregister_dev(dev);
		kfree(dev);
		return NULL;
	}
	dprintk(KERN_INFO "%s: Max TX Outstanding = %d.\n", dev->name, tx_max_out);
	priv->tx_max_out = tx_max_out;
	atomic_set(&priv->tx_out, 0);
	priv->tx_count = 0;

	INIT_LIST_HEAD(&priv->i2o_batch_send_task.list);
	priv->i2o_batch_send_task.sync    = 0;
	priv->i2o_batch_send_task.routine = (void *)i2o_lan_batch_send;
	priv->i2o_batch_send_task.data    = (void *)dev;

	dev->open		= i2o_lan_open;
	dev->stop		= i2o_lan_close;
	dev->get_stats		= i2o_lan_get_stats;
	dev->set_multicast_list = i2o_lan_set_multicast_list;
	dev->tx_timeout		= i2o_lan_tx_timeout;
	dev->watchdog_timeo	= I2O_LAN_TX_TIMEOUT;

#ifdef CONFIG_NET_FC
	if (i2o_dev->lct_data.sub_class == I2O_LAN_FIBRE_CHANNEL)
		dev->hard_start_xmit = i2o_lan_sdu_send;
	else
#endif
		dev->hard_start_xmit = i2o_lan_packet_send;

	if (i2o_dev->lct_data.sub_class == I2O_LAN_ETHERNET)
		dev->change_mtu	= i2o_lan_change_mtu;

	return dev;
}

static int __init i2o_lan_init(void)
{
	struct net_device *dev;
	int i;

	printk(KERN_INFO "I2O LAN OSM (C) 1999 University of Helsinki.\n");

	/* Module params are used as global defaults for private values */

	if (max_buckets_out > I2O_LAN_MAX_BUCKETS_OUT)
		max_buckets_out = I2O_LAN_MAX_BUCKETS_OUT;
	if (bucket_thresh > max_buckets_out)
		bucket_thresh = max_buckets_out;

	/* Install handlers for incoming replies */

	if (i2o_install_handler(&i2o_lan_send_handler) < 0) {
 		printk(KERN_ERR "i2o_lan: Unable to register I2O LAN OSM.\n");
		return -EINVAL;
	}
	lan_send_context = i2o_lan_send_handler.context;

	if (i2o_install_handler(&i2o_lan_receive_handler) < 0) {
 		printk(KERN_ERR "i2o_lan: Unable to register I2O LAN OSM.\n");
		return -EINVAL;
	}
	lan_receive_context = i2o_lan_receive_handler.context;

	if (i2o_install_handler(&i2o_lan_handler) < 0) {
 		printk(KERN_ERR "i2o_lan: Unable to register I2O LAN OSM.\n");
		return -EINVAL;
	}
	lan_context = i2o_lan_handler.context;

	for(i=0; i <= MAX_LAN_CARDS; i++)
		i2o_landevs[i] = NULL;

	for (i=0; i < MAX_I2O_CONTROLLERS; i++) {
		struct i2o_controller *iop = i2o_find_controller(i);
		struct i2o_device *i2o_dev;

		if (iop==NULL)
			continue;

		for (i2o_dev=iop->devices;i2o_dev != NULL;i2o_dev=i2o_dev->next) {

			if (i2o_dev->lct_data.class_id != I2O_CLASS_LAN)
				continue;

			/* Make sure device not already claimed by an ISM */
			if (i2o_dev->lct_data.user_tid != 0xFFF)
				continue;

			if (unit == MAX_LAN_CARDS) {
				i2o_unlock_controller(iop);
				printk(KERN_WARNING "i2o_lan: Too many I2O LAN devices.\n");
				return -EINVAL;
			}

			dev = i2o_lan_register_device(i2o_dev);
 			if (dev == NULL) {
				printk(KERN_ERR "i2o_lan: Unable to register I2O LAN device 0x%04x.\n",
				       i2o_dev->lct_data.sub_class);
				continue;
			}

			printk(KERN_INFO "%s: I2O LAN device registered, "
				"subclass = 0x%04x, unit = %d, tid = %d.\n",
				dev->name, i2o_dev->lct_data.sub_class,
				((struct i2o_lan_local *)dev->priv)->unit,
				i2o_dev->lct_data.tid);
		}

		i2o_unlock_controller(iop);
	}

	dprintk(KERN_INFO "%d I2O LAN devices found and registered.\n", unit+1);

	return 0;
}

static void i2o_lan_exit(void)
{
	int i;

	for (i = 0; i <= unit; i++) {
		struct net_device *dev = i2o_landevs[i];
		struct i2o_lan_local *priv = (struct i2o_lan_local *)dev->priv;
		struct i2o_device *i2o_dev = priv->i2o_dev;

		switch (i2o_dev->lct_data.sub_class) {
		case I2O_LAN_ETHERNET:
			unregister_netdev(dev);
			break;
#ifdef CONFIG_FDDI
		case I2O_LAN_FDDI:
			unregister_netdevice(dev);
			break;
#endif
#ifdef CONFIG_TR
		case I2O_LAN_TR:
			unregister_trdev(dev);
			break;
#endif
#ifdef CONFIG_NET_FC
		case I2O_LAN_FIBRE_CHANNEL:
			unregister_fcdev(dev);
			break;
#endif
		default:
			printk(KERN_WARNING "%s: Spurious I2O LAN subclass 0x%08x.\n",
			       dev->name, i2o_dev->lct_data.sub_class);
		}

		dprintk(KERN_INFO "%s: I2O LAN device unregistered.\n",
			dev->name);
		kfree(dev);
	}

	i2o_remove_handler(&i2o_lan_handler);
	i2o_remove_handler(&i2o_lan_send_handler);
	i2o_remove_handler(&i2o_lan_receive_handler);
}

EXPORT_NO_SYMBOLS;

MODULE_AUTHOR("University of Helsinki, Department of Computer Science");
MODULE_DESCRIPTION("I2O Lan OSM");
MODULE_LICENSE("GPL");


MODULE_PARM(max_buckets_out, "1-" __MODULE_STRING(I2O_LAN_MAX_BUCKETS_OUT) "i");
MODULE_PARM_DESC(max_buckets_out, "Total number of buckets to post (1-)");
MODULE_PARM(bucket_thresh, "1-" __MODULE_STRING(I2O_LAN_MAX_BUCKETS_OUT) "i");
MODULE_PARM_DESC(bucket_thresh, "Bucket post threshold (1-)");
MODULE_PARM(rx_copybreak, "1-" "i");
MODULE_PARM_DESC(rx_copybreak, "Copy breakpoint for copy only small frames (1-)");
MODULE_PARM(tx_batch_mode, "0-2" "i");
MODULE_PARM_DESC(tx_batch_mode, "0=Send immediatelly, 1=Send in batches, 2=Switch automatically");

module_init(i2o_lan_init);
module_exit(i2o_lan_exit);
