/* 
**
**  RCpci45.c  
**
**
**
**  ---------------------------------------------------------------------
**  ---     Copyright (c) 1998, 1999, RedCreek Communications Inc.    ---
**  ---                   All rights reserved.                        ---
**  ---------------------------------------------------------------------
**
** Written by Pete Popov and Brian Moyle.
**
** Known Problems
** 
** None known at this time.
**
**  This program is free software; you can redistribute it and/or modify
**  it under the terms of the GNU General Public License as published by
**  the Free Software Foundation; either version 2 of the License, or
**  (at your option) any later version.

**  This program is distributed in the hope that it will be useful,
**  but WITHOUT ANY WARRANTY; without even the implied warranty of
**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**  GNU General Public License for more details.

**  You should have received a copy of the GNU General Public License
**  along with this program; if not, write to the Free Software
**  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
**
**  Pete Popov, Oct 2001: Fixed a few bugs to make the driver functional
**  again. Note that this card is not supported or manufactured by 
**  RedCreek anymore.
**   
**  Rasmus Andersen, December 2000: Converted to new PCI API and general
**  cleanup.
**
**  Pete Popov, January 11,99: Fixed a couple of 2.1.x problems 
**  (virt_to_bus() not called), tested it under 2.2pre5 (as a module), and 
**  added a #define(s) to enable the use of the same file for both, the 2.0.x 
**  kernels as well as the 2.1.x.
**
**  Ported to 2.1.x by Alan Cox 1998/12/9. 
**
**  Sometime in mid 1998, written by Pete Popov and Brian Moyle.
**
***************************************************************************/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/ptrace.h>
#include <linux/errno.h>
#include <linux/in.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/pci.h>
#include <linux/timer.h>
#include <asm/irq.h>		/* For NR_IRQS only. */
#include <asm/bitops.h>
#include <asm/uaccess.h>

static char version[] __initdata =
    "RedCreek Communications PCI linux driver version 2.20\n";

#define RC_LINUX_MODULE
#include "rclanmtl.h"
#include "rcif.h"

#define RUN_AT(x) (jiffies + (x))

#define NEW_MULTICAST

#define MAX_ETHER_SIZE        1520
#define MAX_NMBR_RCV_BUFFERS    96
#define RC_POSTED_BUFFERS_LOW_MARK MAX_NMBR_RCV_BUFFERS-16
#define BD_SIZE 3		/* Bucket Descriptor size */
#define BD_LEN_OFFSET 2		/* Bucket Descriptor offset to length field */

/* RedCreek LAN device Target ID */
#define RC_LAN_TARGET_ID  0x10
/* RedCreek's OSM default LAN receive Initiator */
#define DEFAULT_RECV_INIT_CONTEXT  0xA17

/* minimum msg buffer size needed by the card 
 * Note that the size of this buffer is hard code in the
 * ipsec card's firmware. Thus, the size MUST be a minimum
 * of 16K. Otherwise the card will end up using memory
 * that does not belong to it.
 */
#define MSG_BUF_SIZE  16384

static U32 DriverControlWord;

static void rc_timer (unsigned long);

static int RCopen (struct net_device *);
static int RC_xmit_packet (struct sk_buff *, struct net_device *);
static void RCinterrupt (int, void *, struct pt_regs *);
static int RCclose (struct net_device *dev);
static struct net_device_stats *RCget_stats (struct net_device *);
static int RCioctl (struct net_device *, struct ifreq *, int);
static int RCconfig (struct net_device *, struct ifmap *);
static void RCxmit_callback (U32, U16, PU32, struct net_device *);
static void RCrecv_callback (U32, U8, U32, PU32, struct net_device *);
static void RCreset_callback (U32, U32, U32, struct net_device *);
static void RCreboot_callback (U32, U32, U32, struct net_device *);
static int RC_allocate_and_post_buffers (struct net_device *, int);

static struct pci_device_id rcpci45_pci_table[] __devinitdata = {
	{ PCI_VENDOR_ID_REDCREEK, PCI_DEVICE_ID_RC45, PCI_ANY_ID, PCI_ANY_ID,},
	{}
};
MODULE_DEVICE_TABLE (pci, rcpci45_pci_table);
MODULE_LICENSE("GPL");

static void __devexit
rcpci45_remove_one (struct pci_dev *pdev)
{
	struct net_device *dev = pci_get_drvdata (pdev);
	PDPA pDpa = dev->priv;

	if (!dev) {
		printk (KERN_ERR "%s: remove non-existent device\n",
				dev->name);
		return;
	}

	RCResetIOP (dev);
	unregister_netdev (dev);
	free_irq (dev->irq, dev);
	iounmap ((void *) dev->base_addr);
	pci_release_regions (pdev);
	if (pDpa->msgbuf)
		kfree (pDpa->msgbuf);
	if (pDpa->pPab)
		kfree (pDpa->pPab);
	kfree (dev);
	pci_set_drvdata (pdev, NULL);
}

static int
rcpci45_init_one (struct pci_dev *pdev, const struct pci_device_id *ent)
{
	unsigned long *vaddr;
	PDPA pDpa;
	int error;
	static int card_idx = -1;
	struct net_device *dev;
	unsigned long pci_start, pci_len;

	card_idx++;

	/* 
	 * Allocate and fill new device structure. 
	 * We need enough for struct net_device plus DPA plus the LAN 
	 * API private area, which requires a minimum of 16KB.  The top 
	 * of the allocated area will be assigned to struct net_device; 
	 * the next chunk will be assigned to DPA; and finally, the rest 
	 * will be assigned to the LAN API layer.
	 */

	dev = init_etherdev (NULL, sizeof (*pDpa));
	if (!dev) {
		printk (KERN_ERR
			"(rcpci45 driver:) init_etherdev alloc failed\n");
		error = -ENOMEM;
		goto err_out;
	}

	error = pci_enable_device (pdev);
	if (error) {
		printk (KERN_ERR
			"(rcpci45 driver:) %d: pci enable device error\n",
			card_idx);
		goto err_out;
	}
	error = -ENOMEM;
	pci_start = pci_resource_start (pdev, 0);
	pci_len = pci_resource_len (pdev, 0);
	printk("pci_start %lx pci_len %lx\n", pci_start, pci_len);

	pci_set_drvdata (pdev, dev);

	pDpa = dev->priv;
	pDpa->id = card_idx;
	pDpa->pci_addr = pci_start;

	if (!pci_start || !(pci_resource_flags (pdev, 0) & IORESOURCE_MEM)) {
		printk (KERN_ERR
			"(rcpci45 driver:) No PCI mem resources! Aborting\n");
		error = -EBUSY;
		goto err_out_free_dev;
	}

	/*
	 * pDpa->msgbuf is where the card will dma the I2O 
	 * messages. Thus, we need contiguous physical pages of
	 * memory.
	 */
	pDpa->msgbuf = kmalloc (MSG_BUF_SIZE, GFP_DMA | GFP_KERNEL);
	if (!pDpa->msgbuf) {
		printk (KERN_ERR "(rcpci45 driver:) \
			Could not allocate %d byte memory for the \
				private msgbuf!\n", MSG_BUF_SIZE);
		goto err_out_free_dev;
	}

	/*
	 * Save the starting address of the LAN API private area.  We'll
	 * pass that to RCInitI2OMsgLayer().
	 *
	 */
	pDpa->PLanApiPA = (void *) (((long) pDpa->msgbuf + 0xff) & ~0xff);

	/* The adapter is accessible through memory-access read/write, not
	 * I/O read/write.  Thus, we need to map it to some virtual address
	 * area in order to access the registers as normal memory.
	 */
	error = pci_request_regions (pdev, dev->name);
	if (error)
		goto err_out_free_msgbuf;

	vaddr = (ulong *) ioremap (pci_start, pci_len);
	if (!vaddr) {
		printk (KERN_ERR
			"(rcpci45 driver:) \
			Unable to remap address range from %lu to %lu\n",
			pci_start, pci_start + pci_len);
		goto err_out_free_region;
	}

	dev->base_addr = (unsigned long) vaddr;
	dev->irq = pdev->irq;
	dev->open = &RCopen;
	dev->hard_start_xmit = &RC_xmit_packet;
	dev->stop = &RCclose;
	dev->get_stats = &RCget_stats;
	dev->do_ioctl = &RCioctl;
	dev->set_config = &RCconfig;

	return 0;		/* success */

err_out_free_region:
	pci_release_regions (pdev);
err_out_free_msgbuf:
	kfree (pDpa->msgbuf);
err_out_free_dev:
	unregister_netdev (dev);
	kfree (dev);
err_out:
	card_idx--;
	return -ENODEV;
}

static struct pci_driver rcpci45_driver = {
	name:		"rcpci45",
	id_table:	rcpci45_pci_table,
	probe:		rcpci45_init_one,
	remove:		__devexit_p(rcpci45_remove_one),
};

static int __init
rcpci_init_module (void)
{
	int rc = pci_module_init (&rcpci45_driver);
	if (!rc)
		printk (KERN_ERR "%s", version);
	return rc;
}

static int
RCopen (struct net_device *dev)
{
	int post_buffers = MAX_NMBR_RCV_BUFFERS;
	PDPA pDpa = dev->priv;
	int count = 0;
	int requested = 0;
	int error;

	MOD_INC_USE_COUNT;
	if (pDpa->nexus) {
		/* This is not the first time RCopen is called.  Thus,
		 * the interface was previously opened and later closed
		 * by RCclose().  RCclose() does a Shutdown; to wake up
		 * the adapter, a reset is mandatory before we can post
		 * receive buffers.  However, if the adapter initiated 
		 * a reboot while the interface was closed -- and interrupts
		 * were turned off -- we need will need to reinitialize
		 * the adapter, rather than simply waking it up.  
		 */
		printk (KERN_INFO "Waking up adapter...\n");
		RCResetLANCard (dev, 0, 0, 0);
	} else {
		pDpa->nexus = 1;
		/* 
		 * RCInitI2OMsgLayer is done only once, unless the
		 * adapter was sent a warm reboot
		 */
		error = RCInitI2OMsgLayer (dev, (PFNTXCALLBACK) RCxmit_callback,
					   (PFNRXCALLBACK) RCrecv_callback,
					   (PFNCALLBACK) RCreboot_callback);
		if (error) {
			printk (KERN_ERR "%s: Unable to init msg layer (%x)\n",
					dev->name, error);
			goto err_out;
		}
		if ((error = RCGetMAC (dev, NULL))) {
			printk (KERN_ERR "%s: Unable to get adapter MAC\n",
					dev->name);
			goto err_out;
		}
	}

	/* Request a shared interrupt line. */
	error = request_irq (dev->irq, RCinterrupt, SA_SHIRQ, dev->name, dev);
	if (error) {
		printk (KERN_ERR "%s: unable to get IRQ %d\n", 
				dev->name, dev->irq);
		goto err_out;
	}

	DriverControlWord |= WARM_REBOOT_CAPABLE;
	RCReportDriverCapability (dev, DriverControlWord);

	printk (KERN_INFO "%s: RedCreek Communications IPSEC VPN adapter\n",
		dev->name);

	RCEnableI2OInterrupts (dev);

	while (post_buffers) {
		if (post_buffers > MAX_NMBR_POST_BUFFERS_PER_MSG)
			requested = MAX_NMBR_POST_BUFFERS_PER_MSG;
		else
			requested = post_buffers;
		count = RC_allocate_and_post_buffers (dev, requested);

		if (count < requested) {
			/*
			 * Check to see if we were able to post 
			 * any buffers at all.
			 */
			if (post_buffers == MAX_NMBR_RCV_BUFFERS) {
				printk (KERN_ERR "%s: \
					unable to allocate any buffers\n", 
						dev->name);
				goto err_out_free_irq;
			}
			printk (KERN_WARNING "%s: \
			unable to allocate all requested buffers\n", dev->name);
			break;	/* we'll try to post more buffers later */
		} else
			post_buffers -= count;
	}
	pDpa->numOutRcvBuffers = MAX_NMBR_RCV_BUFFERS - post_buffers;
	pDpa->shutdown = 0;	/* just in case */
	netif_start_queue (dev);
	return 0;

err_out_free_irq:
	free_irq (dev->irq, dev);
err_out:
	MOD_DEC_USE_COUNT;
	return error;
}

static int
RC_xmit_packet (struct sk_buff *skb, struct net_device *dev)
{

	PDPA pDpa = dev->priv;
	singleTCB tcb;
	psingleTCB ptcb = &tcb;
	RC_RETURN status = 0;

	netif_stop_queue (dev);

	if (pDpa->shutdown || pDpa->reboot) {
		printk ("RC_xmit_packet: tbusy!\n");
		return 1;
	}

	/*
	 * The user is free to reuse the TCB after RCI2OSendPacket() 
	 * returns, since the function copies the necessary info into its 
	 * own private space.  Thus, our TCB can be a local structure.  
	 * The skb, on the other hand, will be freed up in our interrupt 
	 * handler.
	 */

	ptcb->bcount = 1;

	/* 
	 * we'll get the context when the adapter interrupts us to tell us that
	 * the transmission is done. At that time, we can free skb.
	 */
	ptcb->b.context = (U32) skb;
	ptcb->b.scount = 1;
	ptcb->b.size = skb->len;
	ptcb->b.addr = virt_to_bus ((void *) skb->data);

	if ((status = RCI2OSendPacket (dev, (U32) NULL, (PRCTCB) ptcb))
	    != RC_RTN_NO_ERROR) {
		printk ("%s: send error 0x%x\n", dev->name, (uint) status);
		return 1;
	} else {
		dev->trans_start = jiffies;
		netif_wake_queue (dev);
	}
	/*
	 * That's it!
	 */
	return 0;
}

/*
 * RCxmit_callback()
 *
 * The transmit callback routine. It's called by RCProcI2OMsgQ()
 * because the adapter is done with one or more transmit buffers and
 * it's returning them to us, or we asked the adapter to return the
 * outstanding transmit buffers by calling RCResetLANCard() with 
 * RC_RESOURCE_RETURN_PEND_TX_BUFFERS flag. 
 * All we need to do is free the buffers.
 */
static void
RCxmit_callback (U32 Status,
		 U16 PcktCount, PU32 BufferContext, struct net_device *dev)
{
	struct sk_buff *skb;
	PDPA pDpa = dev->priv;

	if (!pDpa) {
		printk (KERN_ERR "%s: Fatal Error in xmit callback, !pDpa\n",
				dev->name);
		return;
	}

	if (Status != I2O_REPLY_STATUS_SUCCESS)
		printk (KERN_INFO "%s: xmit_callback: Status = 0x%x\n", 
				dev->name, (uint) Status);
	if (pDpa->shutdown || pDpa->reboot)
		printk (KERN_INFO "%s: xmit callback: shutdown||reboot\n",
				dev->name);

	while (PcktCount--) {
		skb = (struct sk_buff *) (BufferContext[0]);
		BufferContext++;
		dev_kfree_skb_irq (skb);
	}
	netif_wake_queue (dev);
}

static void
RCreset_callback (U32 Status, U32 p1, U32 p2, struct net_device *dev)
{
	PDPA pDpa = dev->priv;

	printk ("RCreset_callback Status 0x%x\n", (uint) Status);
	/*
	 * Check to see why we were called.
	 */
	if (pDpa->shutdown) {
		printk (KERN_INFO "%s: shutting down interface\n",
				dev->name);
		pDpa->shutdown = 0;
		pDpa->reboot = 0;
	} else if (pDpa->reboot) {
		printk (KERN_INFO "%s: reboot, shutdown adapter\n",
				dev->name);
		/*
		 * We don't set any of the flags in RCShutdownLANCard()
		 * and we don't pass a callback routine to it.
		 * The adapter will have already initiated the reboot by
		 * the time the function returns.
		 */
		RCDisableI2OInterrupts (dev);
		RCShutdownLANCard (dev, 0, 0, 0);
		printk (KERN_INFO "%s: scheduling timer...\n", dev->name);
		init_timer (&pDpa->timer);
		pDpa->timer.expires = RUN_AT ((40 * HZ) / 10);	/* 4 sec. */
		pDpa->timer.data = (unsigned long) dev;
		pDpa->timer.function = &rc_timer;	/* timer handler */
		add_timer (&pDpa->timer);
	}
}

static void
RCreboot_callback (U32 Status, U32 p1, U32 p2, struct net_device *dev)
{
	PDPA pDpa = dev->priv;

	printk (KERN_INFO "%s: reboot: rcv buffers outstanding = %d\n",
		 dev->name, (uint) pDpa->numOutRcvBuffers);

	if (pDpa->shutdown) {
		printk (KERN_INFO "%s: skip reboot, shutdown initiated\n",
				dev->name);
		return;
	}
	pDpa->reboot = 1;
	/*
	 * OK, we reset the adapter and ask it to return all
	 * outstanding transmit buffers as well as the posted
	 * receive buffers.  When the adapter is done returning
	 * those buffers, it will call our RCreset_callback() 
	 * routine.  In that routine, we'll call RCShutdownLANCard()
	 * to tell the adapter that it's OK to start the reboot and
	 * schedule a timer callback routine to execute 3 seconds 
	 * later; this routine will reinitialize the adapter at that time.
	 */
	RCResetLANCard (dev, RC_RESOURCE_RETURN_POSTED_RX_BUCKETS |
			RC_RESOURCE_RETURN_PEND_TX_BUFFERS, 0,
			(PFNCALLBACK) RCreset_callback);
}

int
broadcast_packet (unsigned char *address)
{
	int i;
	for (i = 0; i < 6; i++)
		if (address[i] != 0xff)
			return 0;

	return 1;
}

/*
 * RCrecv_callback()
 * 
 * The receive packet callback routine.  This is called by
 * RCProcI2OMsgQ() after the adapter posts buffers which have been
 * filled (one ethernet packet per buffer).
 */
static void
RCrecv_callback (U32 Status,
		 U8 PktCount,
		 U32 BucketsRemain,
		 PU32 PacketDescBlock, struct net_device *dev)
{

	U32 len, count;
	PDPA pDpa = dev->priv;
	struct sk_buff *skb;
	singleTCB tcb;
	psingleTCB ptcb = &tcb;

	ptcb->bcount = 1;

	if ((pDpa->shutdown || pDpa->reboot) && !Status)
		printk (KERN_INFO "%s: shutdown||reboot && !Status (%d)\n",
				dev->name, PktCount);

	if ((Status != I2O_REPLY_STATUS_SUCCESS) || pDpa->shutdown) {
		/*
		 * Free whatever buffers the adapter returned, but don't
		 * pass them to the kernel.
		 */

		if (!pDpa->shutdown && !pDpa->reboot)
			printk (KERN_INFO "%s: recv error status = 0x%x\n",
					dev->name, (uint) Status);
		else
			printk (KERN_DEBUG "%s: Returning %d buffs stat 0x%x\n",
					dev->name, PktCount, (uint) Status);
		/*
		 * TO DO: check the nature of the failure and put the 
		 * adapter in failed mode if it's a hard failure.  
		 * Send a reset to the adapter and free all outstanding memory.
		 */
		if (PacketDescBlock) {
			while (PktCount--) {
				skb = (struct sk_buff *) PacketDescBlock[0];
				dev_kfree_skb (skb);
				pDpa->numOutRcvBuffers--;
				/* point to next context field */
				PacketDescBlock += BD_SIZE;
			}
		}
		return;
	} else {
		while (PktCount--) {
			skb = (struct sk_buff *) PacketDescBlock[0];
			len = PacketDescBlock[2];
			skb->dev = dev;
			skb_put (skb, len);	/* adjust length and tail */
			skb->protocol = eth_type_trans (skb, dev);
			netif_rx (skb);	/* send the packet to the kernel */
			dev->last_rx = jiffies;
			pDpa->numOutRcvBuffers--;	
			/* point to next context field */
			PacketDescBlock += BD_SIZE;
		}
	}

	/*
	 * Replenish the posted receive buffers. 
	 * DO NOT replenish buffers if the driver has already
	 * initiated a reboot or shutdown!
	 */

	if (!pDpa->shutdown && !pDpa->reboot) {
		count = RC_allocate_and_post_buffers (dev,
						      MAX_NMBR_RCV_BUFFERS -
						      pDpa->numOutRcvBuffers);
		pDpa->numOutRcvBuffers += count;
	}

}

/*
 * RCinterrupt()
 * 
 * Interrupt handler. 
 * This routine sets up a couple of pointers and calls
 * RCProcI2OMsgQ(), which in turn process the message and
 * calls one of our callback functions.
 */
static void
RCinterrupt (int irq, void *dev_id, struct pt_regs *regs)
{

	PDPA pDpa;
	struct net_device *dev = dev_id;

	pDpa = dev->priv;

	if (pDpa->shutdown)
		printk (KERN_DEBUG "%s: shutdown, service irq\n",
				dev->name);

	RCProcI2OMsgQ (dev);
}

#define REBOOT_REINIT_RETRY_LIMIT 4
static void
rc_timer (unsigned long data)
{
	struct net_device *dev = (struct net_device *) data;
	PDPA pDpa = dev->priv;
	int init_status;
	static int retry;
	int post_buffers = MAX_NMBR_RCV_BUFFERS;
	int count = 0;
	int requested = 0;

	if (pDpa->reboot) {
		init_status =
		    RCInitI2OMsgLayer (dev, (PFNTXCALLBACK) RCxmit_callback,
				       (PFNRXCALLBACK) RCrecv_callback,
				       (PFNCALLBACK) RCreboot_callback);

		switch (init_status) {
		case RC_RTN_NO_ERROR:

			pDpa->reboot = 0;
			pDpa->shutdown = 0;	/* just in case */
			RCReportDriverCapability (dev, DriverControlWord);
			RCEnableI2OInterrupts (dev);


			if (!(dev->flags & IFF_UP)) {
				retry = 0;
				return;
			}
			while (post_buffers) {
				if (post_buffers > 
						MAX_NMBR_POST_BUFFERS_PER_MSG)
					requested = 
						MAX_NMBR_POST_BUFFERS_PER_MSG;
				else
					requested = post_buffers;
				count =
				    RC_allocate_and_post_buffers (dev,
								  requested);
				post_buffers -= count;
				if (count < requested)
					break;
			}
			pDpa->numOutRcvBuffers =
			    MAX_NMBR_RCV_BUFFERS - post_buffers;
			printk ("Initialization done.\n");
			netif_wake_queue (dev);
			retry = 0;
			return;
		case RC_RTN_FREE_Q_EMPTY:
			retry++;
			printk (KERN_WARNING "%s inbound free q empty\n",
					dev->name);
			break;
		default:
			retry++;
			printk (KERN_WARNING "%s bad stat after reboot: %d\n",
					dev->name, init_status);
			break;
		}

		if (retry > REBOOT_REINIT_RETRY_LIMIT) {
			printk (KERN_WARNING "%s unable to reinitialize adapter after reboot\n", dev->name);
			printk (KERN_WARNING "%s decrementing driver and closing interface\n", dev->name);
			RCDisableI2OInterrupts (dev);
			dev->flags &= ~IFF_UP;
			MOD_DEC_USE_COUNT;
		} else {
			printk (KERN_INFO "%s: rescheduling timer...\n",
					dev->name);
			init_timer (&pDpa->timer);
			pDpa->timer.expires = RUN_AT ((40 * HZ) / 10);
			pDpa->timer.data = (unsigned long) dev;
			pDpa->timer.function = &rc_timer;
			add_timer (&pDpa->timer);
		}
	} else
		printk (KERN_WARNING "%s: unexpected timer irq\n", dev->name);
}

static int
RCclose (struct net_device *dev)
{
	PDPA pDpa = dev->priv;

	printk("RCclose\n");
	netif_stop_queue (dev);

	if (pDpa->reboot) {
		printk (KERN_INFO "%s skipping reset -- adapter already in reboot mode\n", dev->name);
		dev->flags &= ~IFF_UP;
		pDpa->shutdown = 1;
		MOD_DEC_USE_COUNT;
		return 0;
	}

	pDpa->shutdown = 1;

	/*
	 * We can't allow the driver to be unloaded until the adapter returns
	 * all posted receive buffers.  It doesn't hurt to tell the adapter
	 * to return all posted receive buffers and outstanding xmit buffers,
	 * even if there are none.
	 */

	RCShutdownLANCard (dev, RC_RESOURCE_RETURN_POSTED_RX_BUCKETS |
			   RC_RESOURCE_RETURN_PEND_TX_BUFFERS, 0,
			   (PFNCALLBACK) RCreset_callback);

	dev->flags &= ~IFF_UP;
	MOD_DEC_USE_COUNT;
	return 0;
}

static struct net_device_stats *
RCget_stats (struct net_device *dev)
{
	RCLINKSTATS RCstats;

	PDPA pDpa = dev->priv;

	if (!pDpa) {
		return 0;
	} else if (!(dev->flags & IFF_UP)) {
		return 0;
	}

	memset (&RCstats, 0, sizeof (RCLINKSTATS));
	if ((RCGetLinkStatistics (dev, &RCstats, (void *) 0)) ==
	    RC_RTN_NO_ERROR) {

		/* total packets received    */
		pDpa->stats.rx_packets = RCstats.Rcv_good
		/* total packets transmitted    */;
		pDpa->stats.tx_packets = RCstats.TX_good;

		pDpa->stats.rx_errors = RCstats.Rcv_CRCerr + 
			RCstats.Rcv_alignerr + RCstats.Rcv_reserr + 
			RCstats.Rcv_orun + RCstats.Rcv_cdt + RCstats.Rcv_runt;

		pDpa->stats.tx_errors = RCstats.TX_urun + RCstats.TX_crs + 
			RCstats.TX_def + RCstats.TX_totcol;

		/*
		 * This needs improvement.
		 */
		pDpa->stats.rx_dropped = 0; /* no space in linux buffers   */
		pDpa->stats.tx_dropped = 0; /* no space available in linux */
		pDpa->stats.multicast = 0;  /* multicast packets received  */
		pDpa->stats.collisions = RCstats.TX_totcol;

		/* detailed rx_errors: */
		pDpa->stats.rx_length_errors = 0;
		pDpa->stats.rx_over_errors = RCstats.Rcv_orun;
		pDpa->stats.rx_crc_errors = RCstats.Rcv_CRCerr;
		pDpa->stats.rx_frame_errors = 0;
		pDpa->stats.rx_fifo_errors = 0;	
		pDpa->stats.rx_missed_errors = 0;

		/* detailed tx_errors */
		pDpa->stats.tx_aborted_errors = 0;
		pDpa->stats.tx_carrier_errors = 0;
		pDpa->stats.tx_fifo_errors = 0;
		pDpa->stats.tx_heartbeat_errors = 0;
		pDpa->stats.tx_window_errors = 0;

		return ((struct net_device_stats *) &(pDpa->stats));
	}
	return 0;
}

static int
RCioctl (struct net_device *dev, struct ifreq *rq, int cmd)
{
	RCuser_struct RCuser;
	PDPA pDpa = dev->priv;

	if (!capable (CAP_NET_ADMIN))
		return -EPERM;

	switch (cmd) {

	case RCU_PROTOCOL_REV:
		/*
		 * Assign user protocol revision, to tell user-level
		 * controller program whether or not it's in sync.
		 */
		rq->ifr_ifru.ifru_data = (caddr_t) USER_PROTOCOL_REV;
		break;

	case RCU_COMMAND:
		{
			if (copy_from_user
			    (&RCuser, rq->ifr_data, sizeof (RCuser)))
				return -EFAULT;

			dprintk ("RCioctl: RCuser_cmd = 0x%x\n", RCuser.cmd);

			switch (RCuser.cmd) {
			case RCUC_GETFWVER:
				RCUD_GETFWVER = &RCuser.RCUS_GETFWVER;
				RCGetFirmwareVer (dev,
						  (PU8) & RCUD_GETFWVER->
						  FirmString, NULL);
				break;
			case RCUC_GETINFO:
				RCUD_GETINFO = &RCuser.RCUS_GETINFO;
				RCUD_GETINFO->mem_start = dev->base_addr;
				RCUD_GETINFO->mem_end =
				    dev->base_addr + pDpa->pci_addr_len;
				RCUD_GETINFO->base_addr = pDpa->pci_addr;
				RCUD_GETINFO->irq = dev->irq;
				break;
			case RCUC_GETIPANDMASK:
				RCUD_GETIPANDMASK = &RCuser.RCUS_GETIPANDMASK;
				RCGetRavlinIPandMask (dev,
						      (PU32) &
						      RCUD_GETIPANDMASK->IpAddr,
						      (PU32) &
						      RCUD_GETIPANDMASK->
						      NetMask, NULL);
				break;
			case RCUC_GETLINKSTATISTICS:
				RCUD_GETLINKSTATISTICS =
				    &RCuser.RCUS_GETLINKSTATISTICS;
				RCGetLinkStatistics (dev,
						     (P_RCLINKSTATS) &
						     RCUD_GETLINKSTATISTICS->
						     StatsReturn, NULL);
				break;
			case RCUC_GETLINKSTATUS:
				RCUD_GETLINKSTATUS = &RCuser.RCUS_GETLINKSTATUS;
				RCGetLinkStatus (dev,
						 (PU32) & RCUD_GETLINKSTATUS->
						 ReturnStatus, NULL);
				break;
			case RCUC_GETMAC:
				RCUD_GETMAC = &RCuser.RCUS_GETMAC;
				RCGetMAC (dev, NULL);
				memcpy(RCUD_GETMAC, dev->dev_addr, 8);
				break;
			case RCUC_GETPROM:
				RCUD_GETPROM = &RCuser.RCUS_GETPROM;
				RCGetPromiscuousMode (dev,
						      (PU32) & RCUD_GETPROM->
						      PromMode, NULL);
				break;
			case RCUC_GETBROADCAST:
				RCUD_GETBROADCAST = &RCuser.RCUS_GETBROADCAST;
				RCGetBroadcastMode (dev,
						    (PU32) & RCUD_GETBROADCAST->
						    BroadcastMode, NULL);
				break;
			case RCUC_GETSPEED:
				if (!(dev->flags & IFF_UP)) {
					return -ENODATA;
				}
				RCUD_GETSPEED = &RCuser.RCUS_GETSPEED;
				RCGetLinkSpeed (dev,
						(PU32) & RCUD_GETSPEED->
						LinkSpeedCode, NULL);
				break;
			case RCUC_SETIPANDMASK:
				RCUD_SETIPANDMASK = &RCuser.RCUS_SETIPANDMASK;
				RCSetRavlinIPandMask (dev,
						      (U32) RCUD_SETIPANDMASK->
						      IpAddr,
						      (U32) RCUD_SETIPANDMASK->
						      NetMask);
				break;
			case RCUC_SETMAC:
				RCSetMAC (dev, (PU8) & RCUD_SETMAC->mac);
				break;
			case RCUC_SETSPEED:
				RCUD_SETSPEED = &RCuser.RCUS_SETSPEED;
				RCSetLinkSpeed (dev,
						(U16) RCUD_SETSPEED->
						LinkSpeedCode);
				break;
			case RCUC_SETPROM:
				RCUD_SETPROM = &RCuser.RCUS_SETPROM;
				RCSetPromiscuousMode (dev,
						      (U16) RCUD_SETPROM->
						      PromMode);
				break;
			case RCUC_SETBROADCAST:
				RCUD_SETBROADCAST = &RCuser.RCUS_SETBROADCAST;
				RCSetBroadcastMode (dev,
						    (U16) RCUD_SETBROADCAST->
						    BroadcastMode);
				break;
			default:
				RCUD_DEFAULT = &RCuser.RCUS_DEFAULT;
				RCUD_DEFAULT->rc = 0x11223344;
				break;
			}
			if (copy_to_user (rq->ifr_data, &RCuser, 
						sizeof (RCuser)))
				return -EFAULT;
			break;
		}		/* RCU_COMMAND */

	default:
		rq->ifr_ifru.ifru_data = (caddr_t) 0x12345678;
		return -EINVAL;
	}
	return 0;
}

static int
RCconfig (struct net_device *dev, struct ifmap *map)
{
	/*
	 * To be completed ...
	 */
	return 0;
	if (dev->flags & IFF_UP)	/* can't act on a running interface */
		return -EBUSY;

	/* Don't allow changing the I/O address */
	if (map->base_addr != dev->base_addr) {
		printk (KERN_WARNING "%s Change I/O address not implemented\n",
				dev->name);
		return -EOPNOTSUPP;
	}
	return 0;
}

static void __exit
rcpci_cleanup_module (void)
{
	pci_unregister_driver (&rcpci45_driver);
}

module_init (rcpci_init_module);
module_exit (rcpci_cleanup_module);

static int
RC_allocate_and_post_buffers (struct net_device *dev, int numBuffers)
{

	int i;
	PU32 p;
	psingleB pB;
	struct sk_buff *skb;
	RC_RETURN status;
	U32 res;

	if (!numBuffers)
		return 0;
	else if (numBuffers > MAX_NMBR_POST_BUFFERS_PER_MSG) {
		printk (KERN_ERR "%s: Too many buffers requested!\n",
				dev->name);
		numBuffers = 32;
	}

	p = (PU32) kmalloc (sizeof (U32) + numBuffers * sizeof (singleB),
			    GFP_DMA | GFP_ATOMIC);

	if (!p) {
		printk (KERN_WARNING "%s unable to allocate TCB\n",
				dev->name);
		return 0;
	}

	p[0] = 0;		/* Buffer Count */
	pB = (psingleB) ((U32) p + sizeof (U32));/* point to the first buffer */

	for (i = 0; i < numBuffers; i++) {
		skb = dev_alloc_skb (MAX_ETHER_SIZE + 2);
		if (!skb) {
			printk (KERN_WARNING 
					"%s: unable to allocate enough skbs!\n",
					dev->name);
			if (*p != 0) {	/* did we allocate any buffers */
				break;
			} else {
				kfree (p);	/* Free the TCB */
				return 0;
			}
		}
		skb_reserve (skb, 2);	/* Align IP on 16 byte boundaries */
		pB->context = (U32) skb;
		pB->scount = 1;	/* segment count */
		pB->size = MAX_ETHER_SIZE;
		pB->addr = virt_to_bus ((void *) skb->data);
		p[0]++;
		pB++;
	}

	if ((status = RCPostRecvBuffers (dev, (PRCTCB) p)) != RC_RTN_NO_ERROR) {
		printk (KERN_WARNING "%s: Post buffer failed, error 0x%x\n",
				dev->name, status);
		/* point to the first buffer */
		pB = (psingleB) ((U32) p + sizeof (U32));
		while (p[0]) {
			skb = (struct sk_buff *) pB->context;
			dev_kfree_skb (skb);
			p[0]--;
			pB++;
		}
	}
	res = p[0];
	kfree (p);
	return (res);		/* return the number of posted buffers */
}
