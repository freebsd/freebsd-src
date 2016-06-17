/*********************************************************************
 *                
 * Filename:      toshoboe.c
 * Version:       0.1
 * Description:   Driver for the Toshiba OBOE (or type-O or 700 or 701)
 *                FIR Chipset. 
 * Status:        Experimental.
 * Author:        James McKenzie <james@fishsoup.dhs.org>
 * Created at:    Sat May 8  12:35:27 1999
 * Modified:      Paul Bristow <paul.bristow@technologist.com>
 * Modified:      Mon Nov 11 19:10:05 1999
 * 
 *     Copyright (c) 1999-2000 James McKenzie, All Rights Reserved.
 *      
 *     This program is free software; you can redistribute it and/or 
 *     modify it under the terms of the GNU General Public License as 
 *     published by the Free Software Foundation; either version 2 of 
 *     the License, or (at your option) any later version.
 *  
 *     Neither James McKenzie nor Cambridge University admit liability nor
 *     provide warranty for any of this software. This material is 
 *     provided "AS-IS" and at no charge.
 *
 *     Applicable Models : Libretto 100CT. and many more
 *     Toshiba refers to this chip as the type-O IR port.
 *
 ********************************************************************/

/* This driver is experimental, I have only three ir devices */
/* an olivetti notebook which doesn't have FIR, a toshiba libretto, and */
/* an hp printer, this works fine at 4MBPS with my HP printer */

static char *rcsid = "$Id: toshoboe.c,v 1.91 1999/06/29 14:21:06 root Exp $";

/* Define this to have only one frame in the XMIT or RECV queue */
/* Toshiba's drivers do this, but it disables back to back tansfers */
/* I think that the chip may have some problems certainly, I have */
/* seen it jump over tasks in the taskfile->xmit with this turned on */
#define ONETASK 

/* To adjust the number of tasks in use edit toshoboe.h */

/* Define this to enable FIR and MIR support */
#define ENABLE_FAST

/* Size of IO window */
#define CHIP_IO_EXTENT	0x1f

/* Transmit and receive buffer sizes, adjust at your peril */
#define RX_BUF_SZ 	4196
#define TX_BUF_SZ	4196

/* No user servicable parts below here */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/ioport.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/rtnetlink.h>

#include <asm/system.h>
#include <asm/io.h>

#include <net/irda/wrapper.h>
#include <net/irda/irda.h>
#include <net/irda/irmod.h>
#include <net/irda/irlap_frame.h>
#include <net/irda/irda_device.h>

#include <linux/pm.h>

#include <net/irda/toshoboe.h>

#define PCI_DEVICE_ID_FIR701b  0x0d01

static struct pci_device_id toshoboe_pci_tbl[] __initdata = {
	{ PCI_VENDOR_ID_TOSHIBA, PCI_DEVICE_ID_FIR701, PCI_ANY_ID, PCI_ANY_ID, },
	{ PCI_VENDOR_ID_TOSHIBA, PCI_DEVICE_ID_FIR701b, PCI_ANY_ID, PCI_ANY_ID, },
	{ }			/* Terminating entry */
};
MODULE_DEVICE_TABLE(pci, toshoboe_pci_tbl);

static const char *driver_name = "toshoboe";

static int max_baud = 4000000;

/* Shutdown the chip and point the taskfile reg somewhere else */
static void
toshoboe_stopchip (struct toshoboe_cb *self)
{
  IRDA_DEBUG (4, "%s()\n", __FUNCTION__);

  outb_p (0x0e, OBOE_REG_11);

  outb_p (0x00, OBOE_RST);
  outb_p (0x3f, OBOE_TFP2);     /*Write the taskfile address */
  outb_p (0xff, OBOE_TFP1);
  outb_p (0xff, OBOE_TFP0);
  outb_p (0x0f, OBOE_REG_1B);
  outb_p (0xff, OBOE_REG_1A);
  outb_p (0x00, OBOE_ISR);      /*FIXME: should i do this to disbale ints */
  outb_p (0x80, OBOE_RST);
  outb_p (0xe, OBOE_LOCK);

}

/*Set the baud rate */
static void
toshoboe_setbaud (struct toshoboe_cb *self, int baud)
{
  unsigned long flags;
  IRDA_DEBUG (4, "%s()\n", __FUNCTION__);

  printk (KERN_WARNING "ToshOboe: setting baud to %d\n", baud);

  save_flags (flags);
  cli ();
  switch (baud)
    {
    case 2400:
      outb_p (OBOE_PMDL_SIR, OBOE_PMDL);
      outb_p (OBOE_SMDL_SIR, OBOE_SMDL);
      outb_p (0xbf, OBOE_UDIV);
      break;
    case 4800:
      outb_p (OBOE_PMDL_SIR, OBOE_PMDL);
      outb_p (OBOE_SMDL_SIR, OBOE_SMDL);
      outb_p (0x5f, OBOE_UDIV);
      break;
    case 9600:
      outb_p (OBOE_PMDL_SIR, OBOE_PMDL);
      outb_p (OBOE_SMDL_SIR, OBOE_SMDL);
      outb_p (0x2f, OBOE_UDIV);
      break;
    case 19200:
      outb_p (OBOE_PMDL_SIR, OBOE_PMDL);
      outb_p (OBOE_SMDL_SIR, OBOE_SMDL);
      outb_p (0x17, OBOE_UDIV);
      break;
    case 38400:
      outb_p (OBOE_PMDL_SIR, OBOE_PMDL);
      outb_p (OBOE_SMDL_SIR, OBOE_SMDL);
      outb_p (0xb, OBOE_UDIV);
      break;
    case 57600:
      outb_p (OBOE_PMDL_SIR, OBOE_PMDL);
      outb_p (OBOE_SMDL_SIR, OBOE_SMDL);
      outb_p (0x7, OBOE_UDIV);
      break;
    case 115200:
      outb_p (OBOE_PMDL_SIR, OBOE_PMDL);
      outb_p (OBOE_SMDL_SIR, OBOE_SMDL);
      outb_p (0x3, OBOE_UDIV);
      break;
    case 1152000:
      outb_p (OBOE_PMDL_MIR, OBOE_PMDL);
      outb_p (OBOE_SMDL_MIR, OBOE_SMDL);
      outb_p (0x1, OBOE_UDIV);
      break;
    case 4000000:
      outb_p (OBOE_PMDL_FIR, OBOE_PMDL);
      outb_p (OBOE_SMDL_FIR, OBOE_SMDL);
      outb_p (0x0, OBOE_UDIV);
      break;
    }

  restore_flags (flags);

  outb_p (0x00, OBOE_RST);
  outb_p (0x80, OBOE_RST);
  outb_p (0x01, OBOE_REG_9);

  self->io.speed = baud;
}

/* Wake the chip up and get it looking at the taskfile */
static void
toshoboe_startchip (struct toshoboe_cb *self)
{
  __u32 physaddr;

  IRDA_DEBUG (4, "%s()\n", __FUNCTION__);


  outb_p (0, OBOE_LOCK);
  outb_p (0, OBOE_RST);
  outb_p (OBOE_NTR_VAL, OBOE_NTR);
  outb_p (0xf0, OBOE_REG_D);
  outb_p (0xff, OBOE_ISR);
  outb_p (0x0f, OBOE_REG_1B);
  outb_p (0xff, OBOE_REG_1A);


  physaddr = virt_to_bus (self->taskfile);

  outb_p ((physaddr >> 0x0a) & 0xff, OBOE_TFP0);
  outb_p ((physaddr >> 0x12) & 0xff, OBOE_TFP1);
  outb_p ((physaddr >> 0x1a) & 0x3f, OBOE_TFP2);

  outb_p (0x0e, OBOE_REG_11);
  outb_p (0x80, OBOE_RST);

  toshoboe_setbaud (self, 9600);

}

/*Let the chip look at memory */
static void
toshoboe_enablebm (struct toshoboe_cb *self)
{
  IRDA_DEBUG (4, "%s()\n", __FUNCTION__);
  pci_set_master (self->pdev);
}

/*Don't let the chip look at memory */
static void
toshoboe_disablebm (struct toshoboe_cb *self)
{
  __u8 command;
  IRDA_DEBUG (4, "%s()\n", __FUNCTION__);

  pci_read_config_byte (self->pdev, PCI_COMMAND, &command);
  command &= ~PCI_COMMAND_MASTER;
  pci_write_config_byte (self->pdev, PCI_COMMAND, command);

}

/*setup the taskfile */
static void
toshoboe_initbuffs (struct toshoboe_cb *self)
{
  int i;
  unsigned long flags;

  IRDA_DEBUG (4, "%s()\n", __FUNCTION__);

  save_flags (flags);
  cli ();

  for (i = 0; i < TX_SLOTS; ++i)
    {
      self->taskfile->xmit[i].len = 0;
      self->taskfile->xmit[i].control = 0x00;
      self->taskfile->xmit[i].buffer = virt_to_bus (self->xmit_bufs[i]);
    }

  for (i = 0; i < RX_SLOTS; ++i)
    {
      self->taskfile->recv[i].len = 0;
      self->taskfile->recv[i].control = 0x83;
      self->taskfile->recv[i].buffer = virt_to_bus (self->recv_bufs[i]);
    }

  restore_flags (flags);
}

/*Transmit something */
static int
toshoboe_hard_xmit (struct sk_buff *skb, struct net_device *dev)
{
  struct toshoboe_cb *self;
  __s32 speed;
  int mtt, len;

  self = (struct toshoboe_cb *) dev->priv;

  ASSERT (self != NULL, return 0;
    );

  /* Check if we need to change the speed */
  speed = irda_get_next_speed(skb);
  if ((speed != self->io.speed) && (speed != -1)) {
	/* Check for empty frame */
	if (!skb->len) {
	    toshoboe_setbaud(self, speed); 
	    dev_kfree_skb(skb);
	    return 0;
	} else
	    self->new_speed = speed;
  }

  netif_stop_queue(dev);
  
  if (self->stopped) {
	  dev_kfree_skb(skb);
    return 0;
  }

#ifdef ONETASK
  if (self->txpending)
    return -EBUSY;

  self->txs = inb_p (OBOE_XMTT) - OBOE_XMTT_OFFSET;

  self->txs &= 0x3f;

#endif

  if (self->taskfile->xmit[self->txs].control)
    return -EBUSY;


  if (inb_p (OBOE_RST) & OBOE_RST_WRAP)
    {
      len = async_wrap_skb (skb, self->xmit_bufs[self->txs], TX_BUF_SZ);
    }
  else
    {
      len = skb->len;
      memcpy (self->xmit_bufs[self->txs], skb->data, len);
    }
  self->taskfile->xmit[self->txs].len = len & 0x0fff;



  outb_p (0, OBOE_RST);
  outb_p (0x1e, OBOE_REG_11);

  self->taskfile->xmit[self->txs].control = 0x84;

  mtt = irda_get_mtt (skb);
  if (mtt)
    udelay (mtt);

  self->txpending++;

  /*FIXME: ask about busy,media_busy stuff, for the moment */
  /*busy means can't queue any more */
#ifndef ONETASK
  if (self->txpending != TX_SLOTS)
  {
  	netif_wake_queue(dev);
  }
#endif

  outb_p (0x80, OBOE_RST);
  outb_p (1, OBOE_REG_9);

  self->txs++;
  self->txs %= TX_SLOTS;

  dev_kfree_skb (skb);

  return 0;
}

/*interrupt handler */
static void
toshoboe_interrupt (int irq, void *dev_id, struct pt_regs *regs)
{
  struct toshoboe_cb *self = (struct toshoboe_cb *) dev_id;
  __u8 irqstat;
  struct sk_buff *skb;

  if (self == NULL)
    {
      printk (KERN_WARNING "%s: irq %d for unknown device.\n",
              driver_name, irq);
      return;
    }

  IRDA_DEBUG (4, "%s()\n", __FUNCTION__);

  irqstat = inb_p (OBOE_ISR);

/* woz it us */
  if (!(irqstat & 0xf8))
    return;

  outb_p (irqstat, OBOE_ISR);   /*Acknologede it */


/* Txdone */
  if (irqstat & OBOE_ISR_TXDONE)
    {
      self->txpending--;

      self->stats.tx_packets++;

      if (self->new_speed) {
	      toshoboe_setbaud(self, self->new_speed);

	      self->new_speed = 0;
      }
      /* Tell network layer that we want more frames */
      netif_wake_queue(self->netdev);
    }

  if (irqstat & OBOE_ISR_RXDONE)
    {

#ifdef ONETASK
      self->rxs = inb_p (OBOE_RCVT);
      self->rxs += (RX_SLOTS - 1);
      self->rxs %= RX_SLOTS;
#else
      while (self->taskfile->recv[self->rxs].control == 0)
#endif
        {
          int len = self->taskfile->recv[self->rxs].len;

          if (len > 2)
            len -= 2;

          skb = dev_alloc_skb (len + 1);
          if (skb)
            {
              skb_reserve (skb, 1);

              skb_put (skb, len);
              memcpy (skb->data, self->recv_bufs[self->rxs], len);

              self->stats.rx_packets++;
              skb->dev = self->netdev;
              skb->mac.raw = skb->data;
              skb->protocol = htons (ETH_P_IRDA);
            }
          else
            {
              printk (KERN_INFO "%s(), memory squeeze, dropping frame.\n", __FUNCTION__);
            }

          self->taskfile->recv[self->rxs].control = 0x83;
          self->taskfile->recv[self->rxs].len = 0x0;

          self->rxs++;
          self->rxs %= RX_SLOTS;

          if (skb)
            netif_rx (skb);

        }

    }

  if (irqstat & OBOE_ISR_20)
    {
      printk (KERN_WARNING "Oboe_irq: 20\n");
    }
  if (irqstat & OBOE_ISR_10)
    {
      printk (KERN_WARNING "Oboe_irq: 10\n");
    }
  if (irqstat & 0x8)
    {
      /*FIXME: I think this is a TX or RX error of some sort */

      self->stats.tx_errors++;
      self->stats.rx_errors++;

    }


}

static int
toshoboe_net_init (struct net_device *dev)
{
  IRDA_DEBUG (4, "%s()\n", __FUNCTION__);

  /* Setup to be a normal IrDA network device driver */
  irda_device_setup (dev);

  /* Insert overrides below this line! */
  return 0;
}


static void 
toshoboe_initptrs (struct toshoboe_cb *self)
{

  unsigned long flags;
  save_flags (flags);
  cli ();

  /*FIXME: need to test this carefully to check which one */
  /*of the two possible startup logics the chip uses */
  /*although it won't make any difference if no-one xmits durining init */
  /*and none what soever if using ONETASK */

  self->rxs = inb_p (OBOE_RCVT);
  self->txs = inb_p (OBOE_XMTT) - OBOE_XMTT_OFFSET;

#if 0
  self->rxs = 0;
  self->txs = 0;
#endif
#if 0
  self->rxs = RX_SLOTS - 1;
  self->txs = 0;
#endif


  self->txpending = 0;

  restore_flags (flags);

}


static int
toshoboe_net_open (struct net_device *dev)
{
  struct toshoboe_cb *self;
  char hwname[32];

  IRDA_DEBUG (4, "%s()\n", __FUNCTION__);

  ASSERT (dev != NULL, return -1;
    );
  self = (struct toshoboe_cb *) dev->priv;

  ASSERT (self != NULL, return 0;
    );

  if (self->stopped)
    return 0;

  if (request_irq (self->io.irq, toshoboe_interrupt,
                   SA_SHIRQ | SA_INTERRUPT, dev->name, (void *) self))
    {

      return -EAGAIN;
    }

  toshoboe_initbuffs (self);
  toshoboe_enablebm (self);
  toshoboe_startchip (self);
  toshoboe_initptrs (self);

  /* Ready to play! */
  netif_start_queue(dev);  
  /* Give self a hardware name */
  sprintf(hwname, "Toshiba-FIR @ 0x%03x", self->base);
  /* 
   * Open new IrLAP layer instance, now that everything should be
   * initialized properly 
   */
  self->irlap = irlap_open(dev, &self->qos, hwname);	

  self->open = 1;
	
  MOD_INC_USE_COUNT;

  return 0;

}

static int
toshoboe_net_close (struct net_device *dev)
{
  struct toshoboe_cb *self;

  IRDA_DEBUG (4, "%s()\n", __FUNCTION__);

  ASSERT (dev != NULL, return -1;
    );
  self = (struct toshoboe_cb *) dev->priv;

  /* Stop device */
  netif_stop_queue(dev);
    
  /* Stop and remove instance of IrLAP */
  if (self->irlap)
	  irlap_close(self->irlap);
  self->irlap = NULL;

  self->open = 0;

  free_irq (self->io.irq, (void *) self);

  if (!self->stopped)
    {
      toshoboe_stopchip (self);
      toshoboe_disablebm (self);
    }

  MOD_DEC_USE_COUNT;

  return 0;

}

/*
 * Function toshoboe_net_ioctl (dev, rq, cmd)
 *
 *    Process IOCTL commands for this device
 *
 */
static int toshoboe_net_ioctl(struct net_device *dev, struct ifreq *rq, int cmd)
{
	struct if_irda_req *irq = (struct if_irda_req *) rq;
	struct toshoboe_cb *self;
	unsigned long flags;
	int ret = 0;

	ASSERT(dev != NULL, return -1;);

	self = dev->priv;

	ASSERT(self != NULL, return -1;);

	IRDA_DEBUG(2, "%s(), %s, (cmd=0x%X)\n", __FUNCTION__, dev->name, cmd);
	
	/* Disable interrupts & save flags */
	save_flags(flags);
	cli();
	switch (cmd) {
	case SIOCSBANDWIDTH: /* Set bandwidth */
		if (!capable(CAP_NET_ADMIN)) {
			ret = -EPERM;
			goto out;
		}
		/* toshoboe_setbaud(self, irq->ifr_baudrate); */
                /* Just change speed once - inserted by Paul Bristow */
	        self->new_speed = irq->ifr_baudrate;
		break;
	case SIOCSMEDIABUSY: /* Set media busy */
		if (!capable(CAP_NET_ADMIN)) {
			ret = -EPERM;
			goto out;
		}
		irda_device_set_media_busy(self->netdev, TRUE);
		break;
	case SIOCGRECEIVING: /* Check if we are receiving right now */
		irq->ifr_receiving = 0; /* Can't tell */
		break;
	default:
		ret = -EOPNOTSUPP;
	}
out:
	restore_flags(flags);
	return ret;
}

MODULE_DESCRIPTION("Toshiba OBOE IrDA Device Driver");
MODULE_AUTHOR("James McKenzie <james@fishsoup.dhs.org>");
MODULE_LICENSE("GPL");

MODULE_PARM (max_baud, "i");
MODULE_PARM_DESC(max_baus, "Maximum baud rate");

static void
toshoboe_remove (struct pci_dev *pci_dev)
{
  int i;
  struct toshoboe_cb *self = (struct toshoboe_cb*)pci_get_drvdata(pci_dev);

  IRDA_DEBUG (4, "%s()\n", __FUNCTION__);

  ASSERT (self != NULL, return;
    );

  if (!self->stopped)
    {
      toshoboe_stopchip (self);
      toshoboe_disablebm (self);
    }

  release_region (self->io.sir_base, self->io.sir_ext);


  for (i = 0; i < TX_SLOTS; ++i)
    {
      kfree (self->xmit_bufs[i]);
      self->xmit_bufs[i] = NULL;
    }

  for (i = 0; i < RX_SLOTS; ++i)
    {
      kfree (self->recv_bufs[i]);
      self->recv_bufs[i] = NULL;
    }

  if (self->netdev) {
	  /* Remove netdevice */
	  rtnl_lock();
	  unregister_netdevice(self->netdev);
	  rtnl_unlock();
  }

  kfree (self->taskfilebuf);
  self->taskfilebuf = NULL;
  self->taskfile = NULL;

  return;

}

static int
toshoboe_probe (struct pci_dev *pci_dev, const struct pci_device_id *pdid)
{
  struct toshoboe_cb *self;
  struct net_device *dev;
  int i = 0;
  int ok = 0;
  int err;

  IRDA_DEBUG (4, "%s()\n", __FUNCTION__);

  if ((err=pci_enable_device(pci_dev)))
	  return err;

  self = kmalloc (sizeof (struct toshoboe_cb), GFP_KERNEL);

  if (self == NULL)
    {
      printk (KERN_ERR "IrDA: Can't allocate memory for "
              "IrDA control block!\n");
      return -ENOMEM;
    }

  memset (self, 0, sizeof (struct toshoboe_cb));

  self->open = 0;
  self->stopped = 0;
  self->pdev = pci_dev;
  self->base = pci_resource_start(pci_dev,0);

  self->io.sir_base = self->base;
  self->io.irq = pci_dev->irq;
  self->io.sir_ext = CHIP_IO_EXTENT;
  self->io.speed = 9600;

  /* Lock the port that we need */
  if (NULL==request_region (self->io.sir_base, self->io.sir_ext, driver_name))
    {
      IRDA_DEBUG (0, "%s(), can't get iobase of 0x%03x\n",
      	__FUNCTION__, self->io.sir_base);

      err = -EBUSY;
      goto freeself;
    }

  irda_init_max_qos_capabilies (&self->qos);
  self->qos.baud_rate.bits = 0;

  if (max_baud >= 2400)
    self->qos.baud_rate.bits |= IR_2400;
  /*if (max_baud>=4800) idev->qos.baud_rate.bits|=IR_4800; */
  if (max_baud >= 9600)
    self->qos.baud_rate.bits |= IR_9600;
  if (max_baud >= 19200)
    self->qos.baud_rate.bits |= IR_19200;
  if (max_baud >= 115200)
    self->qos.baud_rate.bits |= IR_115200;
#ifdef ENABLE_FAST
  if (max_baud >= 576000)
    self->qos.baud_rate.bits |= IR_576000;
  if (max_baud >= 1152000)
    self->qos.baud_rate.bits |= IR_1152000;
  if (max_baud >= 4000000)
    self->qos.baud_rate.bits |= (IR_4000000 << 8);
#endif


  self->qos.min_turn_time.bits = 0xff;  /*FIXME: what does this do? */

  irda_qos_bits_to_value (&self->qos);

  self->flags = IFF_SIR | IFF_DMA | IFF_PIO;

#ifdef ENABLE_FAST
  if (max_baud >= 576000)
    self->flags |= IFF_FIR;
#endif

  /* Now setup the endless buffers we need */

  self->txs = 0;
  self->rxs = 0;

  self->taskfilebuf = kmalloc (OBOE_TASK_BUF_LEN, GFP_KERNEL);
  if (!self->taskfilebuf)
    {
      printk (KERN_ERR "toshoboe: kmalloc for DMA failed()\n");
      err = -ENOMEM;
      goto freeregion;
    }


  memset (self->taskfilebuf, 0, OBOE_TASK_BUF_LEN);

  /*We need to align the taskfile on a taskfile size boundary */
  {
    __u32 addr;

    addr = (__u32) self->taskfilebuf;
    addr &= ~(sizeof (struct OboeTaskFile) - 1);
    addr += sizeof (struct OboeTaskFile);

    self->taskfile = (struct OboeTaskFile *) addr;
  }

  for (i = 0; i < TX_SLOTS; ++i)
    {
      self->xmit_bufs[i] = kmalloc (TX_BUF_SZ, GFP_KERNEL | GFP_DMA);
      if (self->xmit_bufs[i])
        ok++;
    }

  for (i = 0; i < RX_SLOTS; ++i)
    {
      self->recv_bufs[i] = kmalloc (TX_BUF_SZ, GFP_KERNEL | GFP_DMA);
      if (self->recv_bufs[i])
        ok++;
    }

  if (ok != RX_SLOTS + TX_SLOTS)
    {
      printk (KERN_ERR "toshoboe: kmalloc for buffers failed()\n");
      err = -ENOMEM;
      goto freebufs;

  }


  if (!(dev = dev_alloc("irda%d", &err))) {
      ERROR("%s(), dev_alloc() failed!\n", __FUNCTION__);
      err = -ENOMEM;
      goto freebufs;
  }
  dev->priv = (void *) self;
  self->netdev = dev;
  
  MESSAGE("IrDA: Registered device %s\n", dev->name);

  dev->init = toshoboe_net_init;
  dev->hard_start_xmit = toshoboe_hard_xmit;
  dev->open = toshoboe_net_open;
  dev->stop = toshoboe_net_close;
  dev->do_ioctl = toshoboe_net_ioctl;

  rtnl_lock();
  err = register_netdevice(dev);
  rtnl_unlock();
  if (err) {
	  ERROR("%s(), register_netdev() failed!\n", __FUNCTION__);
	  /* XXX there is not freeing for dev? */
          goto freebufs;
  }
  pci_set_drvdata(pci_dev,self);

  printk (KERN_WARNING "ToshOboe: Using ");
#ifdef ONETASK
  printk ("single");
#else
  printk ("multiple");
#endif
  printk (" tasks, version %s\n", rcsid);

  return (0);
freebufs:
      for (i = 0; i < TX_SLOTS; ++i)
        if (self->xmit_bufs[i])
          kfree (self->xmit_bufs[i]);
      for (i = 0; i < RX_SLOTS; ++i)
        if (self->recv_bufs[i])
          kfree (self->recv_bufs[i]);
      kfree(self->taskfilebuf);
freeregion:
      release_region (self->io.sir_base, self->io.sir_ext);
freeself:
      kfree (self);
      return err;
}

static int
toshoboe_suspend (struct pci_dev *pci_dev, u32 crap)
{
  int i = 10;
  struct toshoboe_cb *self = (struct toshoboe_cb*)pci_get_drvdata(pci_dev);

  printk (KERN_WARNING "ToshOboe: suspending\n");

  if (!self || self->stopped)
    return 0;

  self->stopped = 1;

  if (!self->open)
    return 0;

/*FIXME: can't sleep here wait one second */

  while ((i--) && (self->txpending))
    udelay (100);

  toshoboe_stopchip (self);
  toshoboe_disablebm (self);

  self->txpending = 0;
  return 0;
}


static int 
toshoboe_resume (struct pci_dev *pci_dev)
{
  struct toshoboe_cb *self = (struct toshoboe_cb*)pci_get_drvdata(pci_dev);
  unsigned long flags;

  if (!self)
    return 0;

  if (!self->stopped)
    return 0;

  if (!self->open)
    {
      self->stopped = 0;
      return 0;
    }

  save_flags (flags);
  cli ();

  toshoboe_initbuffs (self);
  toshoboe_enablebm (self);
  toshoboe_startchip (self);

  toshoboe_setbaud (self, self->io.speed);

  toshoboe_initptrs (self);

  netif_wake_queue(self->netdev);
  restore_flags (flags);
  printk (KERN_WARNING "ToshOboe: waking up\n");
  return 0;
}

static struct pci_driver toshoboe_pci_driver = {
  name		: "toshoboe",
  id_table	: toshoboe_pci_tbl,
  probe		: toshoboe_probe,
  remove	: toshoboe_remove,
  suspend	: toshoboe_suspend,
  resume	: toshoboe_resume 
};

int __init
toshoboe_init (void)
{
  return pci_module_init(&toshoboe_pci_driver);
}

void
toshoboe_cleanup (void)
{
  pci_unregister_driver(&toshoboe_pci_driver);
}

module_init(toshoboe_init);
module_exit(toshoboe_cleanup);
