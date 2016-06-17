/*********************************************************************
 * 
 * Filename:	  irport.c
 * Version:	  1.0
 * Description:   Half duplex serial port SIR driver for IrDA. 
 * Status:	  Experimental.
 * Author:	  Dag Brattli <dagb@cs.uit.no>
 * Created at:	  Sun Aug  3 13:49:59 1997
 * Modified at:   Fri Jan 28 20:22:38 2000
 * Modified by:   Dag Brattli <dagb@cs.uit.no>
 * Sources:	  serial.c by Linus Torvalds 
 * 
 *     Copyright (c) 1997, 1998, 1999-2000 Dag Brattli, All Rights Reserved.
 *     
 *     This program is free software; you can redistribute it and/or 
 *     modify it under the terms of the GNU General Public License as 
 *     published by the Free Software Foundation; either version 2 of 
 *     the License, or (at your option) any later version.
 * 
 *     This program is distributed in the hope that it will be useful,
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *     GNU General Public License for more details.
 * 
 *     You should have received a copy of the GNU General Public License 
 *     along with this program; if not, write to the Free Software 
 *     Foundation, Inc., 59 Temple Place, Suite 330, Boston, 
 *     MA 02111-1307 USA
 *
 *     This driver is ment to be a small half duplex serial driver to be
 *     used for IR-chipsets that has a UART (16550) compatibility mode. 
 *     Eventually it will replace irtty, because of irtty has some 
 *     problems that is hard to get around when we don't have control
 *     over the serial driver. This driver may also be used by FIR 
 *     drivers to handle SIR mode for them.
 *
 ********************************************************************/

#include <linux/module.h>

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/ioport.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/skbuff.h>
#include <linux/serial_reg.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/spinlock.h>
#include <linux/rtnetlink.h>

#include <asm/system.h>
#include <asm/bitops.h>
#include <asm/io.h>

#include <net/irda/irda.h>
#include <net/irda/irmod.h>
#include <net/irda/wrapper.h>
#include <net/irda/irport.h>

#define IO_EXTENT 8

/* 
 * Currently you'll need to set these values using insmod like this:
 * insmod irport io=0x3e8 irq=11
 */
static unsigned int io[]  = { ~0, ~0, ~0, ~0 };
static unsigned int irq[] = { 0, 0, 0, 0 };

static unsigned int qos_mtt_bits = 0x03;

static struct irport_cb *dev_self[] = { NULL, NULL, NULL, NULL};
static char *driver_name = "irport";

static void irport_write_wakeup(struct irport_cb *self);
static int  irport_write(int iobase, int fifo_size, __u8 *buf, int len);
static void irport_receive(struct irport_cb *self);

static int  irport_net_init(struct net_device *dev);
static int  irport_net_ioctl(struct net_device *dev, struct ifreq *rq, 
			     int cmd);
static int  irport_is_receiving(struct irport_cb *self);
static int  irport_set_dtr_rts(struct net_device *dev, int dtr, int rts);
static int  irport_raw_write(struct net_device *dev, __u8 *buf, int len);
static struct net_device_stats *irport_net_get_stats(struct net_device *dev);
static int irport_change_speed_complete(struct irda_task *task);
static void irport_timeout(struct net_device *dev);

EXPORT_SYMBOL(irport_open);
EXPORT_SYMBOL(irport_close);
EXPORT_SYMBOL(irport_start);
EXPORT_SYMBOL(irport_stop);
EXPORT_SYMBOL(irport_interrupt);
EXPORT_SYMBOL(irport_hard_xmit);
EXPORT_SYMBOL(irport_timeout);
EXPORT_SYMBOL(irport_change_speed);
EXPORT_SYMBOL(irport_net_open);
EXPORT_SYMBOL(irport_net_close);

int __init irport_init(void)
{
 	int i;

 	for (i=0; (io[i] < 2000) && (i < 4); i++) {
 		int ioaddr = io[i];
 		if (check_region(ioaddr, IO_EXTENT))
 			continue;
 		if (irport_open(i, io[i], irq[i]) != NULL)
 			return 0;
 	}
	/* 
	 * Maybe something failed, but we can still be usable for FIR drivers 
	 */
 	return 0;
}

/*
 * Function irport_cleanup ()
 *
 *    Close all configured ports
 *
 */
#ifdef MODULE
static void irport_cleanup(void)
{
 	int i;

        IRDA_DEBUG( 4, "%s()\n", __FUNCTION__);

	for (i=0; i < 4; i++) {
 		if (dev_self[i])
 			irport_close(dev_self[i]);
 	}
}
#endif /* MODULE */

struct irport_cb *
irport_open(int i, unsigned int iobase, unsigned int irq)
{
	struct net_device *dev;
	struct irport_cb *self;
	void *ret;
	int err;

	IRDA_DEBUG(0, "%s()\n", __FUNCTION__);

	/*
	 *  Allocate new instance of the driver
	 */
	self = kmalloc(sizeof(struct irport_cb), GFP_KERNEL);
	if (!self) {
		ERROR("%s(), can't allocate memory for "
		      "control block!\n", __FUNCTION__);
		return NULL;
	}
	memset(self, 0, sizeof(struct irport_cb));
	spin_lock_init(&self->lock);

	/* Need to store self somewhere */
	dev_self[i] = self;
	self->priv = self;
	self->index = i;

	/* Initialize IO */
	self->io.sir_base  = iobase;
        self->io.sir_ext   = IO_EXTENT;
        self->io.irq       = irq;
        self->io.fifo_size = 16;

	/* Lock the port that we need */
	ret = request_region(self->io.sir_base, self->io.sir_ext, driver_name);
	if (!ret) { 
		IRDA_DEBUG(0, "%s(), can't get iobase of 0x%03x\n",
			__FUNCTION__, self->io.sir_base);
		return NULL;
	}

	/* Initialize QoS for this device */
	irda_init_max_qos_capabilies(&self->qos);
	
	self->qos.baud_rate.bits = IR_9600|IR_19200|IR_38400|IR_57600|
		IR_115200;

	self->qos.min_turn_time.bits = qos_mtt_bits;
	irda_qos_bits_to_value(&self->qos);
	
	self->flags = IFF_SIR|IFF_PIO;

	/* Specify how much memory we want */
	self->rx_buff.truesize = 4000; 
	self->tx_buff.truesize = 4000;
	
	/* Allocate memory if needed */
	if (self->rx_buff.truesize > 0) {
		self->rx_buff.head = (__u8 *) kmalloc(self->rx_buff.truesize,
						      GFP_KERNEL);
		if (self->rx_buff.head == NULL)
			return NULL;
		memset(self->rx_buff.head, 0, self->rx_buff.truesize);
	}
	if (self->tx_buff.truesize > 0) {
		self->tx_buff.head = (__u8 *) kmalloc(self->tx_buff.truesize, 
						      GFP_KERNEL);
		if (self->tx_buff.head == NULL) {
			kfree(self->rx_buff.head);
			return NULL;
		}
		memset(self->tx_buff.head, 0, self->tx_buff.truesize);
	}	
	self->rx_buff.in_frame = FALSE;
	self->rx_buff.state = OUTSIDE_FRAME;
	self->tx_buff.data = self->tx_buff.head;
	self->rx_buff.data = self->rx_buff.head;
	self->mode = IRDA_IRLAP;

	if (!(dev = dev_alloc("irda%d", &err))) {
		ERROR("%s(), dev_alloc() failed!\n", __FUNCTION__);
		return NULL;
	}
	self->netdev = dev;

	/* May be overridden by piggyback drivers */
 	dev->priv = (void *) self;
	self->interrupt    = irport_interrupt;
	self->change_speed = irport_change_speed;

	/* Override the network functions we need to use */
	dev->init            = irport_net_init;
	dev->hard_start_xmit = irport_hard_xmit;
	dev->tx_timeout	     = irport_timeout;
	dev->watchdog_timeo  = HZ;  /* Allow time enough for speed change */
	dev->open            = irport_net_open;
	dev->stop            = irport_net_close;
	dev->get_stats	     = irport_net_get_stats;
	dev->do_ioctl        = irport_net_ioctl;

	/* Make ifconfig display some details */
	dev->base_addr = iobase;
	dev->irq = irq;

	rtnl_lock();
	err = register_netdevice(dev);
	rtnl_unlock();
	if (err) {
		ERROR("%s(), register_netdev() failed!\n", __FUNCTION__);
		return NULL;
	}
	MESSAGE("IrDA: Registered device %s\n", dev->name);

	return self;
}

int irport_close(struct irport_cb *self)
{
	ASSERT(self != NULL, return -1;);

	/* We are not using any dongle anymore! */
	if (self->dongle)
		irda_device_dongle_cleanup(self->dongle);
	self->dongle = NULL;
	
	/* Remove netdevice */
	if (self->netdev) {
		rtnl_lock();
		unregister_netdevice(self->netdev);
		rtnl_unlock();
	}

	/* Release the IO-port that this driver is using */
	IRDA_DEBUG(0 , "%s(), Releasing Region %03x\n", 
		__FUNCTION__, self->io.sir_base);
	release_region(self->io.sir_base, self->io.sir_ext);

	if (self->tx_buff.head)
		kfree(self->tx_buff.head);
	
	if (self->rx_buff.head)
		kfree(self->rx_buff.head);
	
	/* Remove ourselves */
	dev_self[self->index] = NULL;
	kfree(self);
	
	return 0;
}

void irport_start(struct irport_cb *self)
{
	unsigned long flags;
	int iobase;

	iobase = self->io.sir_base;

	irport_stop(self);
	
	spin_lock_irqsave(&self->lock, flags);

	/* Initialize UART */
	outb(UART_LCR_WLEN8, iobase+UART_LCR);  /* Reset DLAB */
	outb((UART_MCR_DTR | UART_MCR_RTS | UART_MCR_OUT2), iobase+UART_MCR);
	
	/* Turn on interrups */
	outb(UART_IER_RLSI | UART_IER_RDI |UART_IER_THRI, iobase+UART_IER);

	spin_unlock_irqrestore(&self->lock, flags);
}

void irport_stop(struct irport_cb *self)
{
	unsigned long flags;
	int iobase;

	iobase = self->io.sir_base;

	spin_lock_irqsave(&self->lock, flags);

	/* Reset UART */
	outb(0, iobase+UART_MCR);
	
	/* Turn off interrupts */
	outb(0, iobase+UART_IER);

	spin_unlock_irqrestore(&self->lock, flags);
}

/*
 * Function irport_probe (void)
 *
 *    Start IO port 
 *
 */
int irport_probe(int iobase)
{
	IRDA_DEBUG(4, "%s(), iobase=%#x\n", __FUNCTION__, iobase);

	return 0;
}

/*
 * Function irport_change_speed (self, speed)
 *
 *    Set speed of IrDA port to specified baudrate
 *
 */
void irport_change_speed(void *priv, __u32 speed)
{
	struct irport_cb *self = (struct irport_cb *) priv;
	unsigned long flags;
	int iobase; 
	int fcr;    /* FIFO control reg */
	int lcr;    /* Line control reg */
	int divisor;

	IRDA_DEBUG(0, "%s(), Setting speed to: %d\n",
		__FUNCTION__, speed);

	ASSERT(self != NULL, return;);

	iobase = self->io.sir_base;
	
	/* Update accounting for new speed */
	self->io.speed = speed;

	spin_lock_irqsave(&self->lock, flags);

	/* Turn off interrupts */
	outb(0, iobase+UART_IER); 

	divisor = SPEED_MAX/speed;
	
	fcr = UART_FCR_ENABLE_FIFO;

	/* 
	 * Use trigger level 1 to avoid 3 ms. timeout delay at 9600 bps, and
	 * almost 1,7 ms at 19200 bps. At speeds above that we can just forget
	 * about this timeout since it will always be fast enough. 
	 */
	if (self->io.speed < 38400)
		fcr |= UART_FCR_TRIGGER_1;
	else 
		fcr |= UART_FCR_TRIGGER_14;
        
	/* IrDA ports use 8N1 */
	lcr = UART_LCR_WLEN8;
	
	outb(UART_LCR_DLAB | lcr, iobase+UART_LCR); /* Set DLAB */
	outb(divisor & 0xff,      iobase+UART_DLL); /* Set speed */
	outb(divisor >> 8,	  iobase+UART_DLM);
	outb(lcr,		  iobase+UART_LCR); /* Set 8N1	*/
	outb(fcr,		  iobase+UART_FCR); /* Enable FIFO's */

	/* Turn on interrups */
	outb(/*UART_IER_RLSI|*/UART_IER_RDI/*|UART_IER_THRI*/, iobase+UART_IER);

	spin_unlock_irqrestore(&self->lock, flags);
}

/*
 * Function __irport_change_speed (instance, state, param)
 *
 *    State machine for changing speed of the device. We do it this way since
 *    we cannot use schedule_timeout() when we are in interrupt context
 */
int __irport_change_speed(struct irda_task *task)
{
	struct irport_cb *self;
	__u32 speed = (__u32) task->param;
	int ret = 0;

	IRDA_DEBUG(2, "%s(), <%ld>\n", __FUNCTION__, jiffies); 

	self = (struct irport_cb *) task->instance;

	ASSERT(self != NULL, return -1;);

	switch (task->state) {
	case IRDA_TASK_INIT:
	case IRDA_TASK_WAIT:
		/* Are we ready to change speed yet? */
		if (self->tx_buff.len > 0) {
			task->state = IRDA_TASK_WAIT;

			/* Try again later */
			ret = MSECS_TO_JIFFIES(20);
			break;
		}

		if (self->dongle)
			irda_task_next_state(task, IRDA_TASK_CHILD_INIT);
		else
			irda_task_next_state(task, IRDA_TASK_CHILD_DONE);
		break;
	case IRDA_TASK_CHILD_INIT:
		/* Go to default speed */
		self->change_speed(self->priv, 9600);

		/* Change speed of dongle */
		if (irda_task_execute(self->dongle,
				      self->dongle->issue->change_speed, 
				      NULL, task, (void *) speed))
		{
			/* Dongle need more time to change its speed */
			irda_task_next_state(task, IRDA_TASK_CHILD_WAIT);

			/* Give dongle 1 sec to finish */
			ret = MSECS_TO_JIFFIES(1000);
		} else
			/* Child finished immediately */
			irda_task_next_state(task, IRDA_TASK_CHILD_DONE);
		break;
	case IRDA_TASK_CHILD_WAIT:
		WARNING("%s(), changing speed of dongle timed out!\n",  __FUNCTION__);
		ret = -1;		
		break;
	case IRDA_TASK_CHILD_DONE:
		/* Finally we are ready to change the speed */
		self->change_speed(self->priv, speed);
		
		irda_task_next_state(task, IRDA_TASK_DONE);
		break;
	default:
		ERROR("%s(), unknown state %d\n",  __FUNCTION__, task->state);
		irda_task_next_state(task, IRDA_TASK_DONE);
		ret = -1;
		break;
	}	
	return ret;
}

/*
 * Function irport_write_wakeup (tty)
 *
 *    Called by the driver when there's room for more data.  If we have
 *    more packets to send, we send them here.
 *
 */
static void irport_write_wakeup(struct irport_cb *self)
{
	int actual = 0;
	int iobase;
	int fcr;

	ASSERT(self != NULL, return;);

	IRDA_DEBUG(4, "%s()\n", __FUNCTION__);

	iobase = self->io.sir_base;

	/* Finished with frame?  */
	if (self->tx_buff.len > 0)  {
		/* Write data left in transmit buffer */
		actual = irport_write(iobase, self->io.fifo_size, 
				      self->tx_buff.data, self->tx_buff.len);
		self->tx_buff.data += actual;
		self->tx_buff.len  -= actual;
	} else {
		/* 
		 *  Now serial buffer is almost free & we can start 
		 *  transmission of another packet. But first we must check
		 *  if we need to change the speed of the hardware
		 */
		if (self->new_speed) {
			IRDA_DEBUG(5, "%s(), Changing speed!\n",  __FUNCTION__);
			irda_task_execute(self, __irport_change_speed, 
					  irport_change_speed_complete, 
					  NULL, (void *) self->new_speed);
			self->new_speed = 0;
		} else {
			/* Tell network layer that we want more frames */
			netif_wake_queue(self->netdev);
		}
		self->stats.tx_packets++;

		/* 
		 * Reset Rx FIFO to make sure that all reflected transmit data
		 * is discarded. This is needed for half duplex operation
		 */
		fcr = UART_FCR_ENABLE_FIFO | UART_FCR_CLEAR_RCVR;
		if (self->io.speed < 38400)
			fcr |= UART_FCR_TRIGGER_1;
		else 
			fcr |= UART_FCR_TRIGGER_14;

		outb(fcr, iobase+UART_FCR);

		/* Turn on receive interrupts */
		outb(UART_IER_RDI, iobase+UART_IER);
	}
}

/*
 * Function irport_write (driver)
 *
 *    Fill Tx FIFO with transmit data
 *
 */
static int irport_write(int iobase, int fifo_size, __u8 *buf, int len)
{
	int actual = 0;

	/* Tx FIFO should be empty! */
	if (!(inb(iobase+UART_LSR) & UART_LSR_THRE)) {
		IRDA_DEBUG(0, "%s(), failed, fifo not empty!\n",  __FUNCTION__);
		return 0;
	}
        
	/* Fill FIFO with current frame */
	while ((fifo_size-- > 0) && (actual < len)) {
		/* Transmit next byte */
		outb(buf[actual], iobase+UART_TX);

		actual++;
	}
        
	return actual;
}

/*
 * Function irport_change_speed_complete (task)
 *
 *    Called when the change speed operation completes
 *
 */
static int irport_change_speed_complete(struct irda_task *task)
{
	struct irport_cb *self;

	IRDA_DEBUG(0, "%s()\n", __FUNCTION__);

	self = (struct irport_cb *) task->instance;

	ASSERT(self != NULL, return -1;);
	ASSERT(self->netdev != NULL, return -1;);

	/* Finished changing speed, so we are not busy any longer */
	/* Signal network layer so it can try to send the frame */

	netif_wake_queue(self->netdev);
	
	return 0;
}

/*
 * Function irport_timeout (struct net_device *dev)
 *
 *    The networking layer thinks we timed out.
 *
 */

static void irport_timeout(struct net_device *dev)
{
	struct irport_cb *self;
	int iobase;

	self = (struct irport_cb *) dev->priv;
	iobase = self->io.sir_base;
	
	WARNING("%s: transmit timed out\n", dev->name);
	irport_start(self);
	self->change_speed(self->priv, self->io.speed);
	dev->trans_start = jiffies;
	netif_wake_queue(dev);
}
 
/*
 * Function irport_hard_start_xmit (struct sk_buff *skb, struct net_device *dev)
 *
 *    Transmits the current frame until FIFO is full, then
 *    waits until the next transmitt interrupt, and continues until the
 *    frame is transmitted.
 */
int irport_hard_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct irport_cb *self;
	unsigned long flags;
	int iobase;
	s32 speed;

	IRDA_DEBUG(0, "%s()\n", __FUNCTION__);

	ASSERT(dev != NULL, return 0;);
	
	self = (struct irport_cb *) dev->priv;
	ASSERT(self != NULL, return 0;);

	iobase = self->io.sir_base;

	netif_stop_queue(dev);
	
	/* Check if we need to change the speed */
	speed = irda_get_next_speed(skb);
	if ((speed != self->io.speed) && (speed != -1)) {
		/* Check for empty frame */
		if (!skb->len) {
			irda_task_execute(self, __irport_change_speed, 
					  irport_change_speed_complete, 
					  NULL, (void *) speed);
			dev_kfree_skb(skb);
			return 0;
		} else
			self->new_speed = speed;
	}

	spin_lock_irqsave(&self->lock, flags);

	/* Init tx buffer */
	self->tx_buff.data = self->tx_buff.head;

        /* Copy skb to tx_buff while wrapping, stuffing and making CRC */
	self->tx_buff.len = async_wrap_skb(skb, self->tx_buff.data, 
					   self->tx_buff.truesize);
	
	self->stats.tx_bytes += self->tx_buff.len;

	/* Turn on transmit finished interrupt. Will fire immediately!  */
	outb(UART_IER_THRI, iobase+UART_IER); 

	spin_unlock_irqrestore(&self->lock, flags);

	dev_kfree_skb(skb);
	
	return 0;
}
        
/*
 * Function irport_receive (self)
 *
 *    Receive one frame from the infrared port
 *
 */
static void irport_receive(struct irport_cb *self) 
{
	int boguscount = 0;
	int iobase;

	ASSERT(self != NULL, return;);

	iobase = self->io.sir_base;

	/*  
	 * Receive all characters in Rx FIFO, unwrap and unstuff them. 
         * async_unwrap_char will deliver all found frames  
	 */
	do {
		async_unwrap_char(self->netdev, &self->stats, &self->rx_buff, 
				  inb(iobase+UART_RX));

		/* Make sure we don't stay here to long */
		if (boguscount++ > 32) {
			IRDA_DEBUG(2, "%s(), breaking!\n",  __FUNCTION__);
			break;
		}
	} while (inb(iobase+UART_LSR) & UART_LSR_DR);	
}

/*
 * Function irport_interrupt (irq, dev_id, regs)
 *
 *    Interrupt handler
 */
void irport_interrupt(int irq, void *dev_id, struct pt_regs *regs) 
{
	struct net_device *dev = (struct net_device *) dev_id;
	struct irport_cb *self;
	int boguscount = 0;
	int iobase;
	int iir, lsr;

	if (!dev) {
		WARNING("%s() irq %d for unknown device.\n",  __FUNCTION__, irq);
		return;
	}
	self = (struct irport_cb *) dev->priv;

	spin_lock(&self->lock);

	iobase = self->io.sir_base;

	iir = inb(iobase+UART_IIR) & UART_IIR_ID;
	while (iir) {
		/* Clear interrupt */
		lsr = inb(iobase+UART_LSR);

		IRDA_DEBUG(4, "%s(), iir=%02x, lsr=%02x, iobase=%#x\n", 
			 __FUNCTION__, iir, lsr, iobase);

		switch (iir) {
		case UART_IIR_RLSI:
			IRDA_DEBUG(2, "%s(), RLSI\n",  __FUNCTION__);
			break;
		case UART_IIR_RDI:
			/* Receive interrupt */
			irport_receive(self);
			break;
		case UART_IIR_THRI:
			if (lsr & UART_LSR_THRE)
				/* Transmitter ready for data */
				irport_write_wakeup(self);
			break;
		default:
			IRDA_DEBUG(0, "%s(), unhandled IIR=%#x\n",  __FUNCTION__, iir);
			break;
		} 
		
		/* Make sure we don't stay here to long */
		if (boguscount++ > 100)
			break;

 	        iir = inb(iobase + UART_IIR) & UART_IIR_ID;
	}
	spin_unlock(&self->lock);
}

static int irport_net_init(struct net_device *dev)
{
	/* Set up to be a normal IrDA network device driver */
	irda_device_setup(dev);

	/* Insert overrides below this line! */

	return 0;
}

/*
 * Function irport_net_open (dev)
 *
 *    Network device is taken up. Usually this is done by "ifconfig irda0 up" 
 *   
 */
int irport_net_open(struct net_device *dev)
{
	struct irport_cb *self;
	int iobase;
	char hwname[16];

	IRDA_DEBUG(0, "%s()\n", __FUNCTION__);
	
	ASSERT(dev != NULL, return -1;);
	self = (struct irport_cb *) dev->priv;

	iobase = self->io.sir_base;

	if (request_irq(self->io.irq, self->interrupt, 0, dev->name, 
			(void *) dev)) {
		IRDA_DEBUG(0, "%s(), unable to allocate irq=%d\n",
			 __FUNCTION__, self->io.irq);
		return -EAGAIN;
	}

	irport_start(self);


	/* Give self a hardware name */
	sprintf(hwname, "SIR @ 0x%03x", self->io.sir_base);

	/* 
	 * Open new IrLAP layer instance, now that everything should be
	 * initialized properly 
	 */
	self->irlap = irlap_open(dev, &self->qos, hwname);

	/* FIXME: change speed of dongle */
	/* Ready to play! */

	netif_start_queue(dev);
	
	MOD_INC_USE_COUNT;

	return 0;
}

/*
 * Function irport_net_close (self)
 *
 *    Network device is taken down. Usually this is done by 
 *    "ifconfig irda0 down" 
 */
int irport_net_close(struct net_device *dev)
{
	struct irport_cb *self;
	int iobase;

	IRDA_DEBUG(4, "%s()\n", __FUNCTION__);

	ASSERT(dev != NULL, return -1;);
	self = (struct irport_cb *) dev->priv;

	ASSERT(self != NULL, return -1;);

	iobase = self->io.sir_base;

	/* Stop device */
	netif_stop_queue(dev);
	
	/* Stop and remove instance of IrLAP */
	if (self->irlap)
		irlap_close(self->irlap);
	self->irlap = NULL;

	irport_stop(self);

	free_irq(self->io.irq, dev);

	MOD_DEC_USE_COUNT;

	return 0;
}

/*
 * Function irport_wait_until_sent (self)
 *
 *    Delay exectution until finished transmitting
 *
 */
#if 0
void irport_wait_until_sent(struct irport_cb *self)
{
	int iobase;

	iobase = self->io.sir_base;

	/* Wait until Tx FIFO is empty */
	while (!(inb(iobase+UART_LSR) & UART_LSR_THRE)) {
		IRDA_DEBUG(2, "%s(), waiting!\n",  __FUNCTION__);
		current->state = TASK_INTERRUPTIBLE;
		schedule_timeout(MSECS_TO_JIFFIES(60));
	}
}
#endif

/*
 * Function irport_is_receiving (self)
 *
 *    Returns true is we are currently receiving data
 *
 */
static int irport_is_receiving(struct irport_cb *self)
{
	return (self->rx_buff.state != OUTSIDE_FRAME);
}

/*
 * Function irport_set_dtr_rts (tty, dtr, rts)
 *
 *    This function can be used by dongles etc. to set or reset the status
 *    of the dtr and rts lines
 */
static int irport_set_dtr_rts(struct net_device *dev, int dtr, int rts)
{
	struct irport_cb *self = dev->priv;
	int iobase;

	ASSERT(self != NULL, return -1;);

	iobase = self->io.sir_base;

	if (dtr)
		dtr = UART_MCR_DTR;
	if (rts)
		rts = UART_MCR_RTS;

	outb(dtr|rts|UART_MCR_OUT2, iobase+UART_MCR);

	return 0;
}

static int irport_raw_write(struct net_device *dev, __u8 *buf, int len)
{
	struct irport_cb *self = (struct irport_cb *) dev->priv;
	int actual = 0;
	int iobase;

	ASSERT(self != NULL, return -1;);

	iobase = self->io.sir_base;

	/* Tx FIFO should be empty! */
	if (!(inb(iobase+UART_LSR) & UART_LSR_THRE)) {
		IRDA_DEBUG( 0, "%s(), failed, fifo not empty!\n",  __FUNCTION__);
		return -1;
	}
        
	/* Fill FIFO with current frame */
	while (actual < len) {
		/* Transmit next byte */
		outb(buf[actual], iobase+UART_TX);
		actual++;
	}

	return actual;
}

/*
 * Function irport_net_ioctl (dev, rq, cmd)
 *
 *    Process IOCTL commands for this device
 *
 */
static int irport_net_ioctl(struct net_device *dev, struct ifreq *rq, int cmd)
{
	struct if_irda_req *irq = (struct if_irda_req *) rq;
	struct irport_cb *self;
	dongle_t *dongle;
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
		if (!capable(CAP_NET_ADMIN))
			ret = -EPERM;
                else
			irda_task_execute(self, __irport_change_speed, NULL, 
					  NULL, (void *) irq->ifr_baudrate);
		break;
	case SIOCSDONGLE: /* Set dongle */
		if (!capable(CAP_NET_ADMIN)) {
			ret = -EPERM;
			break;
		}

		/* Initialize dongle */
		dongle = irda_device_dongle_init(dev, irq->ifr_dongle);
		if (!dongle)
			break;
		
		dongle->set_mode    = NULL;
		dongle->read        = NULL;
		dongle->write       = irport_raw_write;
		dongle->set_dtr_rts = irport_set_dtr_rts;
		
		self->dongle = dongle;

		/* Now initialize the dongle!  */
		dongle->issue->open(dongle, &self->qos);
		
		/* Reset dongle */
		irda_task_execute(dongle, dongle->issue->reset, NULL, NULL, 
				  NULL);	
		break;
	case SIOCSMEDIABUSY: /* Set media busy */
		if (!capable(CAP_NET_ADMIN)) {
			ret = -EPERM;
			break;
		}

		irda_device_set_media_busy(self->netdev, TRUE);
		break;
	case SIOCGRECEIVING: /* Check if we are receiving right now */
		irq->ifr_receiving = irport_is_receiving(self);
		break;
	case SIOCSDTRRTS:
		if (!capable(CAP_NET_ADMIN)) {
			ret = -EPERM;
			break;
		}

		irport_set_dtr_rts(dev, irq->ifr_dtr, irq->ifr_rts);
		break;
	default:
		ret = -EOPNOTSUPP;
	}
	
	restore_flags(flags);
	
	return ret;
}

static struct net_device_stats *irport_net_get_stats(struct net_device *dev)
{
	struct irport_cb *self = (struct irport_cb *) dev->priv;
	
	return &self->stats;
}

#ifdef MODULE
MODULE_PARM(io, "1-4i");
MODULE_PARM_DESC(io, "Base I/O addresses");
MODULE_PARM(irq, "1-4i");
MODULE_PARM_DESC(irq, "IRQ lines");

MODULE_AUTHOR("Dag Brattli <dagb@cs.uit.no>");
MODULE_DESCRIPTION("Half duplex serial driver for IrDA SIR mode");
MODULE_LICENSE("GPL");


void cleanup_module(void)
{
	irport_cleanup();
}

int init_module(void)
{
	return irport_init();
}
#endif /* MODULE */

