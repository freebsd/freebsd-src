/*
 * NET		An implementation of the IEEE 802.2 LLC protocol for the
 *		LINUX operating system.  LLC is implemented as a set of
 *		state machines and callbacks for higher networking layers.
 *
 *		Code for initialization, termination, registration and
 *		MAC layer glue.
 *
 *		Written by Tim Alpaerts, Tim_Alpaerts@toyota-motor-europe.com
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 *	Changes
 *		Alan Cox	:	Chainsawed to Linux format
 *					Added llc_ to names
 *					Started restructuring handlers
 *
 *              Horst von Brand :      Add #include <linux/string.h>
 */

#include <linux/module.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/unistd.h>
#include <linux/string.h>
#include <linux/netdevice.h>
#include <linux/init.h>
#include <net/p8022.h>

#include <asm/byteorder.h>

#include <net/llc_frame.h>
#include <net/llc.h>

/*
 *	All incoming frames pass thru mac_data_indicate().
 *	On entry the llc structure related to the frame is passed as parameter. 
 *	The received sk_buffs with pdus other than I_CMD and I_RSP
 *	are freed by mac_data_indicate() after processing,
 *	the I pdu buffers are freed by the cl2llc client when it no longer needs
 *	the skb.
*/

int llc_mac_data_indicate(llcptr lp, struct sk_buff *skb)
{
	int ll;      		/* logical length == 802.3 length field */
	unsigned char p_flag;
	unsigned char type;
	frameptr fr;
	int free=1;

	lp->inc_skb=NULL;
	
	/*
	 *	Truncate buffer to true 802.3 length
	 *	[FIXME: move to 802.2 demux]
	 */

	ll = *(skb->data -2) * 256 + *(skb->data -1);
	skb_trim( skb, ll );

	fr = (frameptr) skb->data;
	type = llc_decode_frametype( fr );


	if (type <= FRMR_RSP)
	{
		/*
		 *	PDU is of the type 2 set
		 */
		if ((lp->llc_mode == MODE_ABM)||(type == SABME_CMD))
			llc_process_otype2_frame(lp, skb, type);

	}
	else
	{
		/*
		 *	PDU belongs to type 1 set
		 */
	        p_flag = fr->u_hdr.u_pflag;
        	switch(type)
        	{
		        case TEST_CMD:
				llc_sendpdu(lp, TEST_RSP, 0,ll -3,
					fr->u_hdr.u_info);
				break;
			case TEST_RSP:
				lp->llc_callbacks|=LLC_TEST_INDICATION;
				lp->inc_skb=skb;
				free=0;
				break;
			case XID_CMD:
				/*
				 *	Basic format XID is handled by LLC itself
				 *	Doc 5.4.1.1.2 p 48/49
				 */

				if ((ll == 6)&&(fr->u_hdr.u_info[0] == 0x81))
				{
					lp->k = fr->u_hdr.u_info[2];
					llc_sendpdu(lp, XID_RSP,
						fr->u_hdr.u_pflag, ll -3,
						fr->u_hdr.u_info);
				}
				break;

			case XID_RSP:
				if( ll == 6 && fr->u_hdr.u_info[0] == 0x81 )
				{
					lp->k = fr->u_hdr.u_info[2];
				}
				lp->llc_callbacks|=LLC_XID_INDICATION;
				lp->inc_skb=skb;
				free=0;
				break;

			case UI_CMD:
				lp->llc_callbacks|=LLC_UI_DATA;
				skb_pull(skb,3);
				lp->inc_skb=skb;
				free=0;
				break;

			default:;
				/*
				 *	All other type 1 pdus ignored for now
				 */
		}
	}

	if (free&&(!(IS_IFRAME(fr))))
	{
		/*
		 *	No auto free for I pdus
		 */
		skb->sk = NULL;
		kfree_skb(skb);
	}

	if(lp->llc_callbacks)
	{
		if ( lp->llc_event != NULL ) lp->llc_event(lp);
		lp->llc_callbacks=0;
	}
	return 0;
}


/*
 *	Create an LLC client. As it is the job of the caller to clean up
 *	LLC's on device down, the device list must be locked before this call.
 */

int register_cl2llc_client(llcptr lp, const char *device, void (*event)(llcptr), u8 *rmac, u8 ssap, u8 dsap)
{
	char eye_init[] = "LLC\0";

	memset(lp, 0, sizeof(*lp));
	lp->dev = __dev_get_by_name(device);
	if(lp->dev == NULL)
		return -ENODEV;
	memcpy(lp->eye, eye_init, sizeof(lp->eye));
	lp->rw = 1;
	lp->k = 127;
	lp->n1 = 1490;
	lp->n2 = 10;
	lp->timer_interval[P_TIMER] = HZ;    /* 1 sec */
	lp->timer_interval[REJ_TIMER] = HZ/8;
	lp->timer_interval[ACK_TIMER] = HZ/8;
	lp->timer_interval[BUSY_TIMER] = HZ*2;
	lp->local_sap = ssap;
	lp->llc_event = event;
	memcpy(lp->remote_mac, rmac, sizeof(lp->remote_mac));
	lp->state = 0;
	lp->llc_mode = MODE_ADM;
	lp->remote_sap = dsap;
	skb_queue_head_init(&lp->atq);
	skb_queue_head_init(&lp->rtq);
	MOD_INC_USE_COUNT;
	return 0;
}


void unregister_cl2llc_client(llcptr lp)
{
	llc_cancel_timers(lp);
	MOD_DEC_USE_COUNT;
	kfree(lp);
}


EXPORT_SYMBOL(register_cl2llc_client);
EXPORT_SYMBOL(unregister_cl2llc_client);
EXPORT_SYMBOL(llc_data_request);
EXPORT_SYMBOL(llc_unit_data_request);
EXPORT_SYMBOL(llc_test_request);
EXPORT_SYMBOL(llc_xid_request);
EXPORT_SYMBOL(llc_mac_data_indicate);
EXPORT_SYMBOL(llc_cancel_timers);

#define ALL_TYPES_8022 0

static int __init llc_init(void)
{
	printk(KERN_NOTICE "IEEE 802.2 LLC for Linux 2.1 (c) 1996 Tim Alpaerts\n");
	return 0;
}


module_init(llc_init);
