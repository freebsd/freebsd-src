/*
 *	Comtrol SV11 card driver
 *
 *	This is a slightly odd Z85230 synchronous driver. All you need to
 *	know basically is
 *
 *	Its a genuine Z85230
 *
 *	It supports DMA using two DMA channels in SYNC mode. The driver doesn't
 *	use these facilities
 *	
 *	The control port is at io+1, the data at io+3 and turning off the DMA
 *	is done by writing 0 to io+4
 *
 *	The hardware does the bus handling to avoid the need for delays between
 *	touching control registers.
 *
 *	Port B isnt wired (why - beats me)
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/net.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/if_arp.h>
#include <linux/delay.h>
#include <linux/ioport.h>
#include <net/arp.h>

#include <asm/io.h>
#include <asm/dma.h>
#include <asm/byteorder.h>
#include <net/syncppp.h>
#include "z85230.h"

static int dma;

struct sv11_device
{
	void *if_ptr;	/* General purpose pointer (used by SPPP) */
	struct z8530_dev sync;
	struct ppp_device netdev;
};

/*
 *	Network driver support routines
 */

/*
 *	Frame receive. Simple for our card as we do sync ppp and there
 *	is no funny garbage involved
 */
 
static void hostess_input(struct z8530_channel *c, struct sk_buff *skb)
{
	/* Drop the CRC - its not a good idea to try and negotiate it ;) */
	skb_trim(skb, skb->len-2);
	skb->protocol=__constant_htons(ETH_P_WAN_PPP);
	skb->mac.raw=skb->data;
	skb->dev=c->netdevice;
	/*
	 *	Send it to the PPP layer. We dont have time to process
	 *	it right now.
	 */
	netif_rx(skb);
	c->netdevice->last_rx = jiffies;
}
 
/*
 *	We've been placed in the UP state
 */ 
 
static int hostess_open(struct net_device *d)
{
	struct sv11_device *sv11=d->priv;
	int err = -1;
	
	/*
	 *	Link layer up
	 */
	switch(dma)
	{
		case 0:
			err=z8530_sync_open(d, &sv11->sync.chanA);
			break;
		case 1:
			err=z8530_sync_dma_open(d, &sv11->sync.chanA);
			break;
		case 2:
			err=z8530_sync_txdma_open(d, &sv11->sync.chanA);
			break;
	}
	
	if(err)
		return err;
	/*
	 *	Begin PPP
	 */
	err=sppp_open(d);
	if(err)
	{
		switch(dma)
		{
			case 0:
				z8530_sync_close(d, &sv11->sync.chanA);
				break;
			case 1:
				z8530_sync_dma_close(d, &sv11->sync.chanA);
				break;
			case 2:
				z8530_sync_txdma_close(d, &sv11->sync.chanA);
				break;
		}				
		return err;
	}
	sv11->sync.chanA.rx_function=hostess_input;
	
	/*
	 *	Go go go
	 */

	netif_start_queue(d);
	MOD_INC_USE_COUNT;
	return 0;
}

static int hostess_close(struct net_device *d)
{
	struct sv11_device *sv11=d->priv;
	/*
	 *	Discard new frames
	 */
	sv11->sync.chanA.rx_function=z8530_null_rx;
	/*
	 *	PPP off
	 */
	sppp_close(d);
	/*
	 *	Link layer down
	 */
	netif_stop_queue(d);
		
	switch(dma)
	{
		case 0:
			z8530_sync_close(d, &sv11->sync.chanA);
			break;
		case 1:
			z8530_sync_dma_close(d, &sv11->sync.chanA);
			break;
		case 2:
			z8530_sync_txdma_close(d, &sv11->sync.chanA);
			break;
	}
	MOD_DEC_USE_COUNT;
	return 0;
}

static int hostess_ioctl(struct net_device *d, struct ifreq *ifr, int cmd)
{
	/* struct sv11_device *sv11=d->priv;
	   z8530_ioctl(d,&sv11->sync.chanA,ifr,cmd) */
	return sppp_do_ioctl(d, ifr,cmd);
}

static struct net_device_stats *hostess_get_stats(struct net_device *d)
{
	struct sv11_device *sv11=d->priv;
	if(sv11)
		return z8530_get_stats(&sv11->sync.chanA);
	else
		return NULL;
}

/*
 *	Passed PPP frames, fire them downwind.
 */
 
static int hostess_queue_xmit(struct sk_buff *skb, struct net_device *d)
{
	struct sv11_device *sv11=d->priv;
	return z8530_queue_xmit(&sv11->sync.chanA, skb);
}

static int hostess_neigh_setup(struct neighbour *n)
{
	if (n->nud_state == NUD_NONE) {
		n->ops = &arp_broken_ops;
		n->output = n->ops->output;
	}
	return 0;
}

static int hostess_neigh_setup_dev(struct net_device *dev, struct neigh_parms *p)
{
	if (p->tbl->family == AF_INET) {
		p->neigh_setup = hostess_neigh_setup;
		p->ucast_probes = 0;
		p->mcast_probes = 0;
	}
	return 0;
}

/*
 *	Description block for a Comtrol Hostess SV11 card
 */
 
static struct sv11_device *sv11_init(int iobase, int irq)
{
	struct z8530_dev *dev;
	struct sv11_device *sv;
	unsigned long flags;
	
	/*
	 *	Get the needed I/O space
	 */
	 
	if(!request_region(iobase, 8, "Comtrol SV11"))
	{	
		printk(KERN_WARNING "hostess: I/O 0x%X already in use.\n", iobase);
		return NULL;
	}
	
	sv=(struct sv11_device *)kmalloc(sizeof(struct sv11_device), GFP_KERNEL);
	if(!sv)
		goto fail3;
			
	memset(sv, 0, sizeof(*sv));
	sv->if_ptr=&sv->netdev;
	
	sv->netdev.dev=(struct net_device *)kmalloc(sizeof(struct net_device), GFP_KERNEL);
	if(!sv->netdev.dev)
		goto fail2;

	dev=&sv->sync;
	
	/*
	 *	Stuff in the I/O addressing
	 */
	 
	dev->active = 0;
	
	dev->chanA.ctrlio=iobase+1;
	dev->chanA.dataio=iobase+3;
	dev->chanB.ctrlio=-1;
	dev->chanB.dataio=-1;
	dev->chanA.irqs=&z8530_nop;
	dev->chanB.irqs=&z8530_nop;
	
	outb(0, iobase+4);		/* DMA off */
	
	/* We want a fast IRQ for this device. Actually we'd like an even faster
	   IRQ ;) - This is one driver RtLinux is made for */
	   
	if(request_irq(irq, &z8530_interrupt, SA_INTERRUPT, "Hostess SV/11", dev)<0)
	{
		printk(KERN_WARNING "hostess: IRQ %d already in use.\n", irq);
		goto fail1;
	}
	
	dev->irq=irq;
	dev->chanA.private=sv;
	dev->chanA.netdevice=sv->netdev.dev;
	dev->chanA.dev=dev;
	dev->chanB.dev=dev;
	
	if(dma)
	{
		/*
		 *	You can have DMA off or 1 and 3 thats the lot
		 *	on the Comtrol.
		 */
		dev->chanA.txdma=3;
		dev->chanA.rxdma=1;
		outb(0x03|0x08, iobase+4);		/* DMA on */
		if(request_dma(dev->chanA.txdma, "Hostess SV/11 (TX)")!=0)
			goto fail;
			
		if(dma==1)
		{
			if(request_dma(dev->chanA.rxdma, "Hostess SV/11 (RX)")!=0)
				goto dmafail;
		}
	}
	save_flags(flags);
	cli();
	
	/*
	 *	Begin normal initialise
	 */
	 
	if(z8530_init(dev)!=0)
	{
		printk(KERN_ERR "Z8530 series device not found.\n");
		restore_flags(flags);
		goto dmafail2;
	}
	z8530_channel_load(&dev->chanB, z8530_dead_port);
	if(dev->type==Z85C30)
		z8530_channel_load(&dev->chanA, z8530_hdlc_kilostream);
	else
		z8530_channel_load(&dev->chanA, z8530_hdlc_kilostream_85230);
	
	restore_flags(flags);


	/*
	 *	Now we can take the IRQ
	 */
	if(dev_alloc_name(dev->chanA.netdevice,"hdlc%d")>=0)
	{
		struct net_device *d=dev->chanA.netdevice;

		/* 
		 *	Initialise the PPP components
		 */
		sppp_attach(&sv->netdev);
		
		/*
		 *	Local fields
		 */	
		
		d->base_addr = iobase;
		d->irq = irq;
		d->priv = sv;
		d->init = NULL;
		
		d->open = hostess_open;
		d->stop = hostess_close;
		d->hard_start_xmit = hostess_queue_xmit;
		d->get_stats = hostess_get_stats;
		d->set_multicast_list = NULL;
		d->do_ioctl = hostess_ioctl;
		d->neigh_setup = hostess_neigh_setup_dev;
		d->set_mac_address = NULL;
		
		if(register_netdev(d)==-1)
		{
			printk(KERN_ERR "%s: unable to register device.\n",
				d->name);
			goto fail;
		}				

		z8530_describe(dev, "I/O", iobase);
		dev->active=1;
		return sv;	
	}
dmafail2:
	if(dma==1)
		free_dma(dev->chanA.rxdma);
dmafail:
	if(dma)
		free_dma(dev->chanA.txdma);
fail:
	free_irq(irq, dev);
fail1:
	kfree(sv->netdev.dev);
fail2:
	kfree(sv);
fail3:
	release_region(iobase,8);
	return NULL;
}

static void sv11_shutdown(struct sv11_device *dev)
{
	sppp_detach(dev->netdev.dev);
	z8530_shutdown(&dev->sync);
	unregister_netdev(dev->netdev.dev);
	free_irq(dev->sync.irq, dev);
	if(dma)
	{
		if(dma==1)
			free_dma(dev->sync.chanA.rxdma);
		free_dma(dev->sync.chanA.txdma);
	}
	release_region(dev->sync.chanA.ctrlio-1, 8);
}

#ifdef MODULE

static int io=0x200;
static int irq=9;

MODULE_PARM(io,"i");
MODULE_PARM_DESC(io, "The I/O base of the Comtrol Hostess SV11 card");
MODULE_PARM(dma,"i");
MODULE_PARM_DESC(dma, "Set this to 1 to use DMA1/DMA3 for TX/RX");
MODULE_PARM(irq,"i");
MODULE_PARM_DESC(irq, "The interrupt line setting for the Comtrol Hostess SV11 card");

MODULE_AUTHOR("Alan Cox");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Modular driver for the Comtrol Hostess SV11");

static struct sv11_device *sv11_unit;

int init_module(void)
{
	printk(KERN_INFO "SV-11 Z85230 Synchronous Driver v 0.02.\n");
	printk(KERN_INFO "(c) Copyright 2001, Red Hat Inc.\n");	
	if((sv11_unit=sv11_init(io,irq))==NULL)
		return -ENODEV;
	return 0;
}

void cleanup_module(void)
{
	if(sv11_unit)
		sv11_shutdown(sv11_unit);
}

#endif

