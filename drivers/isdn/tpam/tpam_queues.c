/* $Id: tpam_queues.c,v 1.1.2.1 2001/11/20 14:19:37 kai Exp $
 *
 * Turbo PAM ISDN driver for Linux. (Kernel Driver)
 *
 * Copyright 2001 Stelian Pop <stelian.pop@fr.alcove.com>, Alcôve
 *
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
 *
 * For all support questions please contact: <support@auvertech.fr>
 *
 */

#include <linux/pci.h>
#include <linux/sched.h>
#include <linux/tqueue.h>
#include <linux/interrupt.h>
#include <asm/io.h>

#include "tpam.h"

/* Local function prototype */
static int tpam_sendpacket(tpam_card *card, tpam_channel *channel);

/*
 * Queue a message to be send to the card when possible.
 *
 * 	card: the board
 * 	skb: the sk_buff containing the message.
 */
void tpam_enqueue(tpam_card *card, struct sk_buff *skb) {

	dprintk("TurboPAM(tpam_enqueue): card=%d\n", card->id);

	/* queue the sk_buff on the board's send queue */
	skb_queue_tail(&card->sendq, skb);

	/* queue the board's send task struct for immediate treatment */
	queue_task(&card->send_tq, &tq_immediate);
	mark_bh(IMMEDIATE_BH);
}

/*
 * Queue a data message to be send to the card when possible.
 *
 * 	card: the board
 * 	skb: the sk_buff containing the message and the data. This parameter
 * 		can be NULL if we want just to trigger the send of queued 
 * 		messages.
 */
void tpam_enqueue_data(tpam_channel *channel, struct sk_buff *skb) {
	
	dprintk("TurboPAM(tpam_enqueue_data): card=%d, channel=%d\n", 
		channel->card->id, channel->num);

	/* if existant, queue the sk_buff on the channel's send queue */
	if (skb)
		skb_queue_tail(&channel->sendq, skb);

	/* queue the channel's send task struct for immediate treatment */
	queue_task(&channel->card->send_tq, &tq_immediate);
	mark_bh(IMMEDIATE_BH);
}

/*
 * IRQ handler.
 *
 * If a message comes from the board we read it, construct a sk_buff containing
 * the message and we queue the sk_buff on the board's receive queue, and we
 * trigger the execution of the board's receive task queue.
 *
 * If a message ack comes from the board we can go on and send a new message,
 * so we trigger the execution of the board's send task queue.
 *
 * 	irq: the irq number
 * 	dev_id: the registered board to the irq
 * 	regs: not used.
 */
void tpam_irq(int irq, void *dev_id, struct pt_regs *regs) {
	tpam_card *card = (tpam_card *)dev_id;
	u32 ackupload, uploadptr;
	u32 waiting_too_long;
	u32 hpic;
	struct sk_buff *skb;
	pci_mpb mpb;
	skb_header *skbh;

	dprintk("TurboPAM(tpam_irq): IRQ received, card=%d\n", card->id);

	/* grab the board lock */
	spin_lock(&card->lock);

	/* get the message type */
	ackupload = copy_from_pam_dword(card, (void *)TPAM_ACKUPLOAD_REGISTER);

	/* acknowledge the interrupt */
	copy_to_pam_dword(card, (void *)TPAM_INTERRUPTACK_REGISTER, 0);
	readl(card->bar0 + TPAM_HINTACK_REGISTER);

	if (!ackupload) {
		/* it is a new message from the board */
		
		dprintk("TurboPAM(tpam_irq): message received, card=%d\n", 
			card->id);

		/* get the upload pointer */
		uploadptr = copy_from_pam_dword(card, 
					    (void *)TPAM_UPLOADPTR_REGISTER);
		
		/* get the beginning of the message (pci_mpb part) */
		copy_from_pam(card, &mpb, (void *)uploadptr, sizeof(pci_mpb));

		/* allocate the sk_buff */
		if (!(skb = alloc_skb(sizeof(skb_header) + sizeof(pci_mpb) + 
				      mpb.actualBlockTLVSize + 
				      mpb.actualDataSize, GFP_ATOMIC))) {
			printk(KERN_ERR "TurboPAM(tpam_irq): "
			       "alloc_skb failed\n");
			spin_unlock(&card->lock);
			return;
		}

		/* build the skb_header */
		skbh = (skb_header *)skb_put(skb, sizeof(skb_header));
		skbh->size = sizeof(pci_mpb) + mpb.actualBlockTLVSize;
		skbh->data_size = mpb.actualDataSize;
		skbh->ack = 0;
		skbh->ack_size = 0;

		/* copy the pci_mpb into the sk_buff */
		memcpy(skb_put(skb, sizeof(pci_mpb)), &mpb, sizeof(pci_mpb));

		/* copy the TLV block into the sk_buff */
		copy_from_pam(card, skb_put(skb, mpb.actualBlockTLVSize),
			      (void *)uploadptr + sizeof(pci_mpb), 
			      mpb.actualBlockTLVSize);

		/* if existent, copy the data block into the sk_buff */
		if (mpb.actualDataSize)
			copy_from_pam(card, skb_put(skb, mpb.actualDataSize),
				(void *)uploadptr + sizeof(pci_mpb) + 4096, 
				mpb.actualDataSize);

		/* wait for the board to become ready */
		waiting_too_long = 0;
		do {
			hpic = readl(card->bar0 + TPAM_HPIC_REGISTER);
			if (waiting_too_long++ > 0xfffffff) {
				kfree_skb(skb); 
				spin_unlock(&card->lock);
				printk(KERN_ERR "TurboPAM(tpam_irq): "
						"waiting too long...\n");
				return;
			}
		} while (hpic & 0x00000002);

		/* acknowledge the message */
        	copy_to_pam_dword(card, (void *)TPAM_ACKDOWNLOAD_REGISTER, 
				  0xffffffff);
        	readl(card->bar0 + TPAM_DSPINT_REGISTER);

		/* release the board lock */
		spin_unlock(&card->lock);
	
		if (mpb.messageID == ID_U3ReadyToReceiveInd) {
			/* this message needs immediate treatment */
			tpam_recv_U3ReadyToReceiveInd(card, skb);
			kfree_skb(skb);
		}
		else {
			/* put the message in the receive queue */
			skb_queue_tail(&card->recvq, skb);
			queue_task(&card->recv_tq, &tq_immediate);
			mark_bh(IMMEDIATE_BH);
		}
		return;
	}
	else {
		/* it is a ack from the board */

		dprintk("TurboPAM(tpam_irq): message acknowledged, card=%d\n",
			card->id);

		/* board is not busy anymore */
		card->busy = 0;
		
		/* release the lock */
		spin_unlock(&card->lock);

		/* schedule the send queue for execution */
		queue_task(&card->send_tq, &tq_immediate);
		mark_bh(IMMEDIATE_BH);
		return;
	}

	/* not reached */
}

/*
 * Run the board's receive task queue, dispatching each message on the queue,
 * to its treatment function.
 *
 * 	card: the board.
 */
void tpam_recv_tq(tpam_card *card) {
	pci_mpb *p;
	struct sk_buff *skb;

	/* for each message on the receive queue... */
        while ((skb = skb_dequeue(&card->recvq))) {

		/* point to the pci_mpb block */
		p = (pci_mpb *)(skb->data + sizeof(skb_header));

		/* dispatch the message */
		switch (p->messageID) {
			case ID_ACreateNCOCnf:
				tpam_recv_ACreateNCOCnf(card, skb);
				break;
			case ID_ADestroyNCOCnf:
				tpam_recv_ADestroyNCOCnf(card, skb);
				break;
			case ID_CConnectCnf:
				tpam_recv_CConnectCnf(card, skb);
				break;
			case ID_CConnectInd:
				tpam_recv_CConnectInd(card, skb);
				break;
			case ID_CDisconnectInd:
				tpam_recv_CDisconnectInd(card, skb);
				break;
			case ID_CDisconnectCnf:
				tpam_recv_CDisconnectCnf(card, skb);
				break;
			case ID_U3DataInd:
				tpam_recv_U3DataInd(card, skb);
				break;
			default:
				dprintk("TurboPAM(tpam_recv_tq): "
					"unknown messageID %d, card=%d\n", 
					p->messageID, card->id);
				break;
		}
		/* free the sk_buff */
		kfree_skb(skb);
	}
}

/*
 * Run the board's send task queue. If there is a message in the board's send
 * queue, it gets sended. If not, it examines each channel (one at the time,
 * using a round robin algorithm). For each channel, if there is a message
 * in the channel's send queue, it gets sended. This function sends only one
 * message, it does not consume all the queue.
 */
void tpam_send_tq(tpam_card *card) {
	int i;

	/* first, try to send a packet from the board's send queue */
	if (tpam_sendpacket(card, NULL))
		return;

	/* then, try each channel, in a round-robin manner */
	for (i=card->roundrobin; i<card->roundrobin+card->channels_used; i++) {
		if (tpam_sendpacket(card, 
				    &card->channels[i % card->channels_used])) {
			card->roundrobin = (i + 1) % card->channels_used;
			return;
		}
	}
}

/*
 * Try to send a packet from the board's send queue or from the channel's
 * send queue.
 *
 * 	card: the board.
 * 	channel: the channel (if NULL, the packet will be taken from the 
 * 		board's send queue. If not, it will be taken from the 
 * 		channel's send queue.
 *
 * Return: 0 if tpam_send_tq must try another card/channel combination
 * 	(meaning that no packet has been send), 1 if no more packets
 * 	can be send at that time (a packet has been send or the card is
 * 	still busy from a previous send).
 */
static int tpam_sendpacket(tpam_card *card, tpam_channel *channel) {
        struct sk_buff *skb;
	u32 hpic;
        u32 downloadptr;
	skb_header *skbh;
	u32 waiting_too_long;

	dprintk("TurboPAM(tpam_sendpacket), card=%d, channel=%d\n", 
		card->id, channel ? channel->num : -1);

	if (channel) {
		/* dequeue a packet from the channel's send queue */
		if (!(skb = skb_dequeue(&channel->sendq))) {
			dprintk("TurboPAM(tpam_sendpacket): "
				"card=%d, channel=%d, no packet\n", 
				card->id, channel->num);
			return 0;
		}

		/* if the channel is not ready to receive, requeue the packet
		 * and return 0 to give a chance to another channel */
		if (!channel->readytoreceive) {
			dprintk("TurboPAM(tpam_sendpacket): "
				"card=%d, channel=%d, channel not ready\n",
				card->id, channel->num);
			skb_queue_head(&channel->sendq, skb);
			return 0;
		}

		/* grab the board lock */
		spin_lock_irq(&card->lock);

		/* if the board is busy, requeue the packet and return 1 since
		 * there is no need to try another channel */
		if (card->busy) {
			dprintk("TurboPAM(tpam_sendpacket): "
				"card=%d, channel=%d, card busy\n",
				card->id, channel->num);
			skb_queue_head(&channel->sendq, skb);
			spin_unlock_irq(&card->lock);
			return 1;
		}
	}
	else {
		/* dequeue a packet from the board's send queue */
		if (!(skb = skb_dequeue(&card->sendq))) {
			dprintk("TurboPAM(tpam_sendpacket): "
				"card=%d, no packet\n", card->id);
			return 0;
		}

		/* grab the board lock */
		spin_lock_irq(&card->lock);

		/* if the board is busy, requeue the packet and return 1 since
		 * there is no need to try another channel */
		if (card->busy) {
			dprintk("TurboPAM(tpam_sendpacket): "
				"card=%d, card busy\n", card->id);
			skb_queue_head(&card->sendq, skb);
			spin_unlock_irq(&card->lock);
			return 1;
		}
	}

	/* wait for the board to become ready */
	waiting_too_long = 0;
	do {
		hpic = readl(card->bar0 + TPAM_HPIC_REGISTER);
		if (waiting_too_long++ > 0xfffffff) {
			spin_unlock_irq(&card->lock);
			printk(KERN_ERR "TurboPAM(tpam_sendpacket): "
					"waiting too long...\n");
			return 1;
		}
	} while (hpic & 0x00000002);

	skbh = (skb_header *)skb->data;
	dprintk("TurboPAM(tpam_sendpacket): "
		"card=%d, card ready, sending %d/%d bytes\n", 
		card->id, skbh->size, skbh->data_size);

	/* get the board's download pointer */
       	downloadptr = copy_from_pam_dword(card, 
					  (void *)TPAM_DOWNLOADPTR_REGISTER);

	/* copy the packet to the board at the downloadptr location */
       	copy_to_pam(card, (void *)downloadptr, skb->data + sizeof(skb_header), 
		    skbh->size);
	if (skbh->data_size)
		/* if there is some data in the packet, copy it too */
		copy_to_pam(card, (void *)downloadptr + sizeof(pci_mpb) + 4096,
			    skb->data + sizeof(skb_header) + skbh->size, 
			    skbh->data_size);

	/* card will become busy right now */
	card->busy = 1;

	/* interrupt the board */
	copy_to_pam_dword(card, (void *)TPAM_ACKDOWNLOAD_REGISTER, 0);
	readl(card->bar0 + TPAM_DSPINT_REGISTER);

	/* release the lock */
	spin_unlock_irq(&card->lock);

	/* if a data ack was requested by the ISDN link layer, send it now */
	if (skbh->ack) {
		isdn_ctrl ctrl;
		ctrl.driver = card->id;
		ctrl.command = ISDN_STAT_BSENT;
		ctrl.arg = channel->num;
		ctrl.parm.length = skbh->ack_size;
		(* card->interface.statcallb)(&ctrl);
	}

	/* free the sk_buff */
	kfree_skb(skb);

	return 1;
}

