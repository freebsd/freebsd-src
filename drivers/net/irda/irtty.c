/*********************************************************************
 *                
 * Filename:      irtty.c
 * Version:       1.1
 * Description:   IrDA line discipline implementation
 * Status:        Experimental.
 * Author:        Dag Brattli <dagb@cs.uit.no>
 * Created at:    Tue Dec  9 21:18:38 1997
 * Modified at:   Sat Mar 11 07:43:30 2000
 * Modified by:   Dag Brattli <dagb@cs.uit.no>
 * Sources:       slip.c by Laurence Culhane,   <loz@holmes.demon.co.uk>
 *                          Fred N. van Kempen, <waltje@uwalt.nl.mugnet.org>
 * 
 *     Copyright (c) 1998-2000 Dag Brattli, All Rights Reserved.
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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/tty.h>
#include <linux/init.h>
#include <linux/skbuff.h>
#include <linux/if_arp.h>
#include <linux/rtnetlink.h>

#include <asm/segment.h>
#include <asm/uaccess.h>

#include <net/irda/irda.h>
#include <net/irda/irtty.h>
#include <net/irda/wrapper.h>
#include <net/irda/timer.h>
#include <net/irda/irda_device.h>

static hashbin_t *irtty = NULL;
static struct tty_ldisc irda_ldisc;

static int qos_mtt_bits = 0x03;      /* 5 ms or more */

/* Network device fuction prototypes */
static int  irtty_hard_xmit(struct sk_buff *skb, struct net_device *dev);
static int  irtty_net_init(struct net_device *dev);
static int  irtty_net_open(struct net_device *dev);
static int  irtty_net_close(struct net_device *dev);
static int  irtty_net_ioctl(struct net_device *dev, struct ifreq *rq, int cmd);
static struct net_device_stats *irtty_net_get_stats(struct net_device *dev);

/* Line discipline function prototypes */
static int  irtty_open(struct tty_struct *tty);
static void irtty_close(struct tty_struct *tty);
static int  irtty_ioctl(struct tty_struct *, void *, int, void *);
static int  irtty_receive_room(struct tty_struct *tty);
static void irtty_write_wakeup(struct tty_struct *tty);
static void irtty_receive_buf(struct tty_struct *, const unsigned char *, 
			      char *, int);

/* IrDA specific function protoctypes */
static int  irtty_is_receiving(struct irtty_cb *self);
static int  irtty_set_dtr_rts(struct net_device *dev, int dtr, int rts);
static int  irtty_raw_write(struct net_device *dev, __u8 *buf, int len);
static int  irtty_raw_read(struct net_device *dev, __u8 *buf, int len);
static int  irtty_set_mode(struct net_device *dev, int mode);
static int  irtty_change_speed(struct irda_task *task);

char *driver_name = "irtty";

int __init irtty_init(void)
{
	int status;
	
	irtty = hashbin_new( HB_LOCAL);
	if ( irtty == NULL) {
		printk( KERN_WARNING "IrDA: Can't allocate irtty hashbin!\n");
		return -ENOMEM;
	}

	/* Fill in our line protocol discipline, and register it */
	memset(&irda_ldisc, 0, sizeof( irda_ldisc));

	irda_ldisc.magic = TTY_LDISC_MAGIC;
 	irda_ldisc.name  = "irda";
	irda_ldisc.flags = 0;
	irda_ldisc.open  = irtty_open;
	irda_ldisc.close = irtty_close;
	irda_ldisc.read  = NULL;
	irda_ldisc.write = NULL;
	irda_ldisc.ioctl = (int (*)(struct tty_struct *, struct file *,
				    unsigned int, unsigned long)) irtty_ioctl;
 	irda_ldisc.poll  = NULL;
	irda_ldisc.receive_buf  = irtty_receive_buf;
	irda_ldisc.receive_room = irtty_receive_room;
	irda_ldisc.write_wakeup = irtty_write_wakeup;
	
	if ((status = tty_register_ldisc(N_IRDA, &irda_ldisc)) != 0) {
		ERROR("IrDA: can't register line discipline (err = %d)\n", 
		      status);
	}
	
	return status;
}

/* 
 *  Function irtty_cleanup ( )
 *
 *    Called when the irda module is removed. Here we remove all instances
 *    of the driver, and the master array.
 */
#ifdef MODULE
static void irtty_cleanup(void) 
{
	int ret;
	
	/* Unregister tty line-discipline */
	if ((ret = tty_register_ldisc(N_IRDA, NULL))) {
		ERROR("%s(), can't unregister line discipline (err = %d)\n",
			__FUNCTION__, ret);
	}

	/*
	 *  The TTY should care of deallocating the instances by using the
	 *  callback to irtty_close(), therefore we do give any deallocation
	 *  function to hashbin_destroy().
	 */
	hashbin_delete(irtty, NULL);
}
#endif /* MODULE */

/* 
 *  Function irtty_open(tty)
 *
 *    This function is called by the TTY module when the IrDA line
 *    discipline is called for.  Because we are sure the tty line exists,
 *    we only have to link it to a free IrDA channel.  
 */
static int irtty_open(struct tty_struct *tty) 
{
	struct net_device *dev;
	struct irtty_cb *self;
	char name[16];
	int err;
	
	ASSERT(tty != NULL, return -EEXIST;);

	/* First make sure we're not already connected. */
	self = (struct irtty_cb *) tty->disc_data;

	if (self != NULL && self->magic == IRTTY_MAGIC)
		return -EEXIST;
	
	/*
	 *  Allocate new instance of the driver
	 */
	self = kmalloc(sizeof(struct irtty_cb), GFP_KERNEL);
	if (self == NULL) {
		printk(KERN_ERR "IrDA: Can't allocate memory for "
		       "IrDA control block!\n");
		return -ENOMEM;
	}
	memset(self, 0, sizeof(struct irtty_cb));
	
	self->tty = tty;
	tty->disc_data = self;

	/* Give self a name */
	sprintf(name, "%s%d", tty->driver.name,
		MINOR(tty->device) - tty->driver.minor_start +
		tty->driver.name_base);

	hashbin_insert(irtty, (irda_queue_t *) self, (int) self, NULL);

	if (tty->driver.flush_buffer)
		tty->driver.flush_buffer(tty);
	
	if (tty->ldisc.flush_buffer)
		tty->ldisc.flush_buffer(tty);
	
	self->magic = IRTTY_MAGIC;
	self->mode = IRDA_IRLAP;

	/* 
	 *  Initialize QoS capabilities, we fill in all the stuff that
	 *  we support. Be careful not to place any restrictions on values
	 *  that are not device dependent (such as link disconnect time) so
	 *  this parameter can be set by IrLAP (or the user) instead. DB
	 */
	irda_init_max_qos_capabilies(&self->qos);

	/* The only value we must override it the baudrate */
	self->qos.baud_rate.bits = IR_9600|IR_19200|IR_38400|IR_57600|
		IR_115200;
	self->qos.min_turn_time.bits = qos_mtt_bits;
	self->flags = IFF_SIR | IFF_PIO;
	irda_qos_bits_to_value(&self->qos);

	/* Specify how much memory we want */
	self->rx_buff.truesize = 4000; 
	self->tx_buff.truesize = 4000;

	/* Allocate memory if needed */
	if (self->rx_buff.truesize > 0) {
		self->rx_buff.head = (__u8 *) kmalloc(self->rx_buff.truesize,
						      GFP_KERNEL);
		if (self->rx_buff.head == NULL)
			return -ENOMEM;
		memset(self->rx_buff.head, 0, self->rx_buff.truesize);
	}
	if (self->tx_buff.truesize > 0) {
		self->tx_buff.head = (__u8 *) kmalloc(self->tx_buff.truesize, 
						      GFP_KERNEL);
		if (self->tx_buff.head == NULL) {
			kfree(self->rx_buff.head);
			return -ENOMEM;
		}
		memset(self->tx_buff.head, 0, self->tx_buff.truesize);
	}
	
	self->rx_buff.in_frame = FALSE;
	self->rx_buff.state = OUTSIDE_FRAME;
	self->tx_buff.data = self->tx_buff.head;
	self->rx_buff.data = self->rx_buff.head;
	
	if (!(dev = dev_alloc("irda%d", &err))) {
		ERROR("%s(), dev_alloc() failed!\n", __FUNCTION__);
		return -ENOMEM;
	}

	dev->priv = (void *) self;
	self->netdev = dev;

	/* Override the network functions we need to use */
	dev->init            = irtty_net_init;
	dev->hard_start_xmit = irtty_hard_xmit;
	dev->open            = irtty_net_open;
	dev->stop            = irtty_net_close;
	dev->get_stats	     = irtty_net_get_stats;
	dev->do_ioctl        = irtty_net_ioctl;

	rtnl_lock();
	err = register_netdevice(dev);
	rtnl_unlock();
	if (err) {
		ERROR("%s(), register_netdev() failed!\n", __FUNCTION__);
		return -1;
	}

	MESSAGE("IrDA: Registered device %s\n", dev->name);

	MOD_INC_USE_COUNT;

	return 0;
}

/* 
 *  Function irtty_close (tty)
 *
 *    Close down a IrDA channel. This means flushing out any pending queues,
 *    and then restoring the TTY line discipline to what it was before it got
 *    hooked to IrDA (which usually is TTY again).  
 */
static void irtty_close(struct tty_struct *tty) 
{
	struct irtty_cb *self = (struct irtty_cb *) tty->disc_data;
	
	/* First make sure we're connected. */
	ASSERT(self != NULL, return;);
	ASSERT(self->magic == IRTTY_MAGIC, return;);
	
	/* Stop tty */
	tty->flags &= ~(1 << TTY_DO_WRITE_WAKEUP);
	tty->disc_data = 0;
	
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
	
	/* Remove speed changing task if any */
	if (self->task)
		irda_task_delete(self->task);

	self->tty = NULL;
	self->magic = 0;
	
	self = hashbin_remove(irtty, (int) self, NULL);

	if (self->tx_buff.head)
		kfree(self->tx_buff.head);
	
	if (self->rx_buff.head)
		kfree(self->rx_buff.head);
	
	kfree(self);
	
 	MOD_DEC_USE_COUNT;
}

/*
 * Function irtty_stop_receiver (self, stop)
 *
 *    
 *
 */
static void irtty_stop_receiver(struct irtty_cb *self, int stop)
{
	struct termios old_termios;
	int cflag;

	old_termios = *(self->tty->termios);
	cflag = self->tty->termios->c_cflag;
	
	if (stop)
		cflag &= ~CREAD;
	else
		cflag |= CREAD;

	self->tty->termios->c_cflag = cflag;
	self->tty->driver.set_termios(self->tty, &old_termios);
}

/* 
 *  Function irtty_do_change_speed (self, speed)
 *
 *    Change the speed of the serial port.
 */
static void __irtty_change_speed(struct irtty_cb *self, __u32 speed)
{
        struct termios old_termios;
	int cflag;

	ASSERT(self != NULL, return;);
	ASSERT(self->magic == IRTTY_MAGIC, return;);

	old_termios = *(self->tty->termios);
	cflag = self->tty->termios->c_cflag;

	cflag &= ~CBAUD;

	IRDA_DEBUG(2, "%s(), Setting speed to %d\n", __FUNCTION__, speed);

	switch (speed) {
	case 1200:
		cflag |= B1200;
		break;
	case 2400:
		cflag |= B2400;
		break;
	case 4800:
		cflag |= B4800;
		break;
	case 19200:
		cflag |= B19200;
		break;
	case 38400:
		cflag |= B38400;
		break;
	case 57600:
		cflag |= B57600;
		break;
	case 115200:
		cflag |= B115200;
		break;
	case 9600:
	default:
		cflag |= B9600;
		break;
	}	

	self->tty->termios->c_cflag = cflag;
	self->tty->driver.set_termios(self->tty, &old_termios);

	self->io.speed = speed;
}

/*
 * Function irtty_change_speed (instance, state, param)
 *
 *    State machine for changing speed of the device. We do it this way since
 *    we cannot use schedule_timeout() when we are in interrupt context
 */
static int irtty_change_speed(struct irda_task *task)
{
	struct irtty_cb *self;
	__u32 speed = (__u32) task->param;
	int ret = 0;

	IRDA_DEBUG(2, "%s(), <%ld>\n", __FUNCTION__, jiffies); 

	self = (struct irtty_cb *) task->instance;
	ASSERT(self != NULL, return -1;);

	/* Check if busy */
	if (self->task && self->task != task) {
		IRDA_DEBUG(0, "%s(), busy!\n", __FUNCTION__);
		return MSECS_TO_JIFFIES(10);
	} else
		self->task = task;

	switch (task->state) {
	case IRDA_TASK_INIT:
		/* 
		 * Make sure all data is sent before changing the speed of the
		 * serial port.
		 */
		if (self->tty->driver.chars_in_buffer(self->tty)) {
			/* Keep state, and try again later */
			ret = MSECS_TO_JIFFIES(10);
			break;
		} else {
			/* Transmit buffer is now empty, but it may still
			 * take over 13 ms for the FIFO to become empty, so
			 * wait some more to be sure all data is sent
			 */
			irda_task_next_state(task, IRDA_TASK_WAIT);
			ret = MSECS_TO_JIFFIES(13);
		}
	case IRDA_TASK_WAIT:
		if (self->dongle)
			irda_task_next_state(task, IRDA_TASK_CHILD_INIT);
		else
			irda_task_next_state(task, IRDA_TASK_CHILD_DONE);
		break;
	case IRDA_TASK_CHILD_INIT:
		/* Go to default speed */
		__irtty_change_speed(self, 9600);

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
		WARNING("%s(), changing speed of dongle timed out!\n", __FUNCTION__);
		ret = -1;		
		break;
	case IRDA_TASK_CHILD_DONE:
		/* Finally we are ready to change the speed */
		__irtty_change_speed(self, speed);
		
		irda_task_next_state(task, IRDA_TASK_DONE);
		self->task = NULL;
		break;
	default:
		ERROR("%s(), unknown state %d\n", __FUNCTION__, task->state);
		irda_task_next_state(task, IRDA_TASK_DONE);
		self->task = NULL;
		ret = -1;
		break;
	}	
	return ret;
}

/*
 * Function irtty_ioctl (tty, file, cmd, arg)
 *
 *     The Swiss army knife of system calls :-)
 *
 */
static int irtty_ioctl(struct tty_struct *tty, void *file, int cmd, void *arg)
{
	dongle_t *dongle;
	struct irtty_info info;
	struct irtty_cb *self;
	int size = _IOC_SIZE(cmd);
	int err = 0;

	self = (struct irtty_cb *) tty->disc_data;

	ASSERT(self != NULL, return -ENODEV;);
	ASSERT(self->magic == IRTTY_MAGIC, return -EBADR;);

	if (_IOC_DIR(cmd) & _IOC_READ)
		err = verify_area(VERIFY_WRITE, (void *) arg, size);
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
		err = verify_area(VERIFY_READ, (void *) arg, size);
	if (err)
		return err;
	
	switch (cmd) {
	case TCGETS:
	case TCGETA:
		return n_tty_ioctl(tty, (struct file *) file, cmd, 
				   (unsigned long) arg);
		break;
	case IRTTY_IOCTDONGLE:
		/* Initialize dongle */
		dongle = irda_device_dongle_init(self->netdev, (int) arg);
		if (!dongle)
			break;
		
		/* Initialize callbacks */
		dongle->set_mode    = irtty_set_mode;
		dongle->read        = irtty_raw_read;
		dongle->write       = irtty_raw_write;
		dongle->set_dtr_rts = irtty_set_dtr_rts;
		
		/* Bind dongle */
		self->dongle = dongle;
		
		/* Now initialize the dongle!  */
		dongle->issue->open(dongle, &self->qos);
		
		/* Reset dongle */
		irda_task_execute(dongle, dongle->issue->reset, NULL, NULL, 
				  NULL);		
		break;
	case IRTTY_IOCGET:
		ASSERT(self->netdev != NULL, return -1;);

		memset(&info, 0, sizeof(struct irtty_info)); 
		strncpy(info.name, self->netdev->name, 5);

		if (copy_to_user(arg, &info, sizeof(struct irtty_info)))
			return -EFAULT;
		break;
	default:
		return -ENOIOCTLCMD;
	}
	return 0;
}

/* 
 *  Function irtty_receive_buf( tty, cp, count)
 *
 *    Handle the 'receiver data ready' interrupt.  This function is called
 *    by the 'tty_io' module in the kernel when a block of IrDA data has
 *    been received, which can now be decapsulated and delivered for
 *    further processing 
 */
static void irtty_receive_buf(struct tty_struct *tty, const unsigned char *cp,
			      char *fp, int count) 
{
	struct irtty_cb *self = (struct irtty_cb *) tty->disc_data;

	if (!self || !self->netdev) {
		IRDA_DEBUG(0, "%s(), not ready yet!\n", __FUNCTION__);
		return;
	}

	/* Read the characters out of the buffer */
 	while (count--) {
		/* 
		 *  Characters received with a parity error, etc?
		 */
 		if (fp && *fp++) { 
			IRDA_DEBUG(0, "Framing or parity error!\n");
			irda_device_set_media_busy(self->netdev, TRUE);
			
 			cp++;
 			continue;
 		}
		
		switch (self->mode) {
		case IRDA_IRLAP:
			/* Unwrap and destuff one byte */
			async_unwrap_char(self->netdev, &self->stats, 
					  &self->rx_buff, *cp++);
			break;
		case IRDA_RAW:
			/* What should we do when the buffer is full? */
			if (self->rx_buff.len == self->rx_buff.truesize)
				self->rx_buff.len = 0;
			
			self->rx_buff.data[self->rx_buff.len++] = *cp++;
			break;
		default:
			break;
		}
	}
}

/*
 * Function irtty_change_speed_complete (task)
 *
 *    Called when the change speed operation completes
 *
 */
static int irtty_change_speed_complete(struct irda_task *task)
{
	struct irtty_cb *self;

	IRDA_DEBUG(2, "%s()\n", __FUNCTION__);

	self = (struct irtty_cb *) task->instance;

	ASSERT(self != NULL, return -1;);
	ASSERT(self->netdev != NULL, return -1;);

	/* Finished changing speed, so we are not busy any longer */
	/* Signal network layer so it can try to send the frame */
	netif_wake_queue(self->netdev);
	
	return 0;
}

/*
 * Function irtty_hard_xmit (skb, dev)
 *
 *    Transmit frame
 *
 */
static int irtty_hard_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct irtty_cb *self;
	int actual = 0;
	__s32 speed;

	self = (struct irtty_cb *) dev->priv;
	ASSERT(self != NULL, return 0;);

	/* Lock transmit buffer */
	netif_stop_queue(dev);
	
	/* Check if we need to change the speed */
	speed = irda_get_next_speed(skb);
	if ((speed != self->io.speed) && (speed != -1)) {
		/* Check for empty frame */
		if (!skb->len) {
			irda_task_execute(self, irtty_change_speed, 
					  irtty_change_speed_complete, 
					  NULL, (void *) speed);
			dev_kfree_skb(skb);
			return 0;
		} else
			self->new_speed = speed;
	}

	/* Init tx buffer*/
	self->tx_buff.data = self->tx_buff.head;
	
        /* Copy skb to tx_buff while wrapping, stuffing and making CRC */
        self->tx_buff.len = async_wrap_skb(skb, self->tx_buff.data, 
					   self->tx_buff.truesize); 

	self->tty->flags |= (1 << TTY_DO_WRITE_WAKEUP);

	dev->trans_start = jiffies;
	self->stats.tx_bytes += self->tx_buff.len;

	if (self->tty->driver.write)
		actual = self->tty->driver.write(self->tty, 0, 
						 self->tx_buff.data, 
						 self->tx_buff.len);
	/* Hide the part we just transmitted */
	self->tx_buff.data += actual;
	self->tx_buff.len -= actual;

	dev_kfree_skb(skb);

	return 0;
}

/*
 * Function irtty_receive_room (tty)
 *
 *    Used by the TTY to find out how much data we can receive at a time
 * 
*/
static int irtty_receive_room(struct tty_struct *tty) 
{
	IRDA_DEBUG(0, "%s()\n", __FUNCTION__);
	return 65536;  /* We can handle an infinite amount of data. :-) */
}

/*
 * Function irtty_write_wakeup (tty)
 *
 *    Called by the driver when there's room for more data.  If we have
 *    more packets to send, we send them here.
 *
 */
static void irtty_write_wakeup(struct tty_struct *tty) 
{
	struct irtty_cb *self = (struct irtty_cb *) tty->disc_data;
	int actual = 0;
	
	/* 
	 *  First make sure we're connected. 
	 */
	ASSERT(self != NULL, return;);
	ASSERT(self->magic == IRTTY_MAGIC, return;);

	/* Finished with frame?  */
	if (self->tx_buff.len > 0)  {
		/* Write data left in transmit buffer */
		actual = tty->driver.write(tty, 0, self->tx_buff.data, 
					   self->tx_buff.len);

		self->tx_buff.data += actual;
		self->tx_buff.len  -= actual;
	} else {		
		/* 
		 *  Now serial buffer is almost free & we can start 
		 *  transmission of another packet 
		 */
		IRDA_DEBUG(5, "%s(), finished with frame!\n", __FUNCTION__);
		
		self->stats.tx_packets++;		      

		tty->flags &= ~(1 << TTY_DO_WRITE_WAKEUP);

		if (self->new_speed) {
			IRDA_DEBUG(5, "%s(), Changing speed!\n", __FUNCTION__);
			irda_task_execute(self, irtty_change_speed, 
					  irtty_change_speed_complete, 
					  NULL, (void *) self->new_speed);
			self->new_speed = 0;
		} else {
			/* Tell network layer that we want more frames */
			netif_wake_queue(self->netdev);
		}
	}
}

/*
 * Function irtty_is_receiving (self)
 *
 *    Return TRUE is we are currently receiving a frame
 *
 */
static int irtty_is_receiving(struct irtty_cb *self)
{
	return (self->rx_buff.state != OUTSIDE_FRAME);
}

/*
 * Function irtty_set_dtr_rts (tty, dtr, rts)
 *
 *    This function can be used by dongles etc. to set or reset the status
 *    of the dtr and rts lines
 */
static int irtty_set_dtr_rts(struct net_device *dev, int dtr, int rts)
{
	struct irtty_cb *self;
	struct tty_struct *tty;
	mm_segment_t fs;
	int arg = TIOCM_MODEM_BITS;

	self = (struct irtty_cb *) dev->priv;
	tty = self->tty;

	if (rts)
		arg |= TIOCM_RTS;
	if (dtr)
		arg |= TIOCM_DTR;

	/*
	 *  The ioctl() function, or actually set_modem_info() in serial.c
	 *  expects a pointer to the argument in user space. To hack us
	 *  around this, we use the set_fs() function to fool the routines 
	 *  that check if they are called from user space. We also need 
	 *  to send a pointer to the argument so get_user() gets happy. DB.
	 */

	fs = get_fs();
	set_fs(get_ds());
	
	if (tty->driver.ioctl(tty, NULL, TIOCMSET, (unsigned long) &arg)) { 
		IRDA_DEBUG(2, "%s(), error doing ioctl!\n", __FUNCTION__);
	}
	set_fs(fs);

	return 0;
}

/*
 * Function irtty_set_mode (self, status)
 *
 *    For the airport dongle, we need support for reading raw characters
 *    from the IrDA device. This function switches between those modes. 
 *    FALSE is the default mode, and will then treat incoming data as IrDA 
 *    packets.
 */
int irtty_set_mode(struct net_device *dev, int mode)
{
	struct irtty_cb *self;

	self = (struct irtty_cb *) dev->priv;

	ASSERT(self != NULL, return -1;);

	IRDA_DEBUG(2, "%s(), mode=%s\n", __FUNCTION__, infrared_mode[mode]);
	
	/* save status for driver */
	self->mode = mode;
	
	/* reset the buffer state */
	self->rx_buff.data = self->rx_buff.head;
	self->rx_buff.len = 0;
	self->rx_buff.state = OUTSIDE_FRAME;

	return 0;
}

/*
 * Function irtty_raw_read (self, buf, len)
 *
 *    Receive incoming data. This function sleeps, so it must only be
 *    called with a process context. Timeout is currently defined to be
 *    a multiple of 10 ms.
 */
static int irtty_raw_read(struct net_device *dev, __u8 *buf, int len)
{
	struct irtty_cb *self;
	int count;

	self = (struct irtty_cb *) dev->priv;

	ASSERT(self != NULL, return 0;);
	ASSERT(self->magic == IRTTY_MAGIC, return 0;);

	return 0;
#if 0
	buf = self->rx_buff.data;

	/* Wait for the requested amount of data to arrive */
	while (len < self->rx_buff.len) {
		current->state = TASK_INTERRUPTIBLE;
		schedule_timeout(MSECS_TO_JIFFIES(10));

		if (!timeout--)
			break;
	}
	
	count = self->rx_buff.len < len ? self->rx_buff.len : len;

	/* 
	 * Reset the state, this mean that a raw read is sort of a 
	 * datagram read, and _not_ a stream style read. Be aware of the
	 * difference. Implementing it the other way will just be painful ;-)
	 */
	self->rx_buff.data = self->rx_buff.head;
	self->rx_buff.len = 0;
	self->rx_buff.state = OUTSIDE_FRAME;
#endif
	/* Return the amount we were able to get */
	return count;
}

static int irtty_raw_write(struct net_device *dev, __u8 *buf, int len)
{
	struct irtty_cb *self;
	int actual = 0;

	self = (struct irtty_cb *) dev->priv;

	ASSERT(self != NULL, return 0;);
	ASSERT(self->magic == IRTTY_MAGIC, return 0;);

	if (self->tty->driver.write)
		actual = self->tty->driver.write(self->tty, 0, buf, len);

	return actual;
}

static int irtty_net_init(struct net_device *dev)
{
	/* Set up to be a normal IrDA network device driver */
	irda_device_setup(dev);

	/* Insert overrides below this line! */

	return 0;
}

static int irtty_net_open(struct net_device *dev)
{
	struct irtty_cb *self = (struct irtty_cb *) dev->priv;
	struct tty_struct *tty = self->tty;
	char hwname[16];

	ASSERT(self != NULL, return -1;);
	ASSERT(self->magic == IRTTY_MAGIC, return -1;);

	IRDA_DEBUG(0, "%s()\n", __FUNCTION__);
	
	/* Ready to play! */
	netif_start_queue(dev);
	
	/* Make sure we can receive more data */
	irtty_stop_receiver(self, FALSE);

	/* Give self a hardware name */
	sprintf(hwname, "%s%d", tty->driver.name,
		MINOR(tty->device) - tty->driver.minor_start +
		tty->driver.name_base);

	/* 
	 * Open new IrLAP layer instance, now that everything should be
	 * initialized properly 
	 */
	self->irlap = irlap_open(dev, &self->qos, hwname);

	MOD_INC_USE_COUNT;

	return 0;
}

static int irtty_net_close(struct net_device *dev)
{
	struct irtty_cb *self = (struct irtty_cb *) dev->priv;

	ASSERT(self != NULL, return -1;);
	ASSERT(self->magic == IRTTY_MAGIC, return -1;);

	/* Make sure we don't receive more data */
	irtty_stop_receiver(self, TRUE);

	/* Stop device */
	netif_stop_queue(dev);
	
	/* Stop and remove instance of IrLAP */
	if (self->irlap)
		irlap_close(self->irlap);
	self->irlap = NULL;

	MOD_DEC_USE_COUNT;

	return 0;
}

/*
 * Function irtty_net_ioctl (dev, rq, cmd)
 *
 *    Process IOCTL commands for this device
 *
 */
static int irtty_net_ioctl(struct net_device *dev, struct ifreq *rq, int cmd)
{
	struct if_irda_req *irq = (struct if_irda_req *) rq;
	struct irtty_cb *self;
	dongle_t *dongle;
	unsigned long flags;
	int ret = 0;

	ASSERT(dev != NULL, return -1;);

	self = dev->priv;

	ASSERT(self != NULL, return -1;);
	ASSERT(self->magic == IRTTY_MAGIC, return -1;);

	IRDA_DEBUG(3, "%s(), %s, (cmd=0x%X)\n", __FUNCTION__, dev->name, cmd);
	
	/* Locking :
	 * irda_device_dongle_init() can't be locked.
	 * irda_task_execute() doesn't need to be locked (but
	 * irtty_change_speed() should protect itself).
	 * As this driver doesn't have spinlock protection, keep
	 * old fashion locking :-(
	 * Jean II
	 */
	
	switch (cmd) {
	case SIOCSBANDWIDTH: /* Set bandwidth */
		if (!capable(CAP_NET_ADMIN))
			ret = -EPERM;
		else
			irda_task_execute(self, irtty_change_speed, NULL, NULL, 
					  (void *) irq->ifr_baudrate);
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
		
		dongle->set_mode    = irtty_set_mode;
		dongle->read        = irtty_raw_read;
		dongle->write       = irtty_raw_write;
		dongle->set_dtr_rts = irtty_set_dtr_rts;
		
		/* Now initialize the dongle!
		 * Safe to do unlocked : self->dongle is still NULL. */ 
		dongle->issue->open(dongle, &self->qos);
		
		/* Reset dongle */
		irda_task_execute(dongle, dongle->issue->reset, NULL, NULL, 
				  NULL);	

		/* Make dongle available to driver only now to avoid
		 * race conditions - Jean II */
		self->dongle = dongle;
		break;
	case SIOCSMEDIABUSY: /* Set media busy */
		if (!capable(CAP_NET_ADMIN))
			ret = -EPERM;
		else
			irda_device_set_media_busy(self->netdev, TRUE);
		break;
	case SIOCGRECEIVING: /* Check if we are receiving right now */
		irq->ifr_receiving = irtty_is_receiving(self);
		break;
	case SIOCSDTRRTS:
		if (!capable(CAP_NET_ADMIN))
			ret = -EPERM;
		else {
			save_flags(flags);
			cli();
			irtty_set_dtr_rts(dev, irq->ifr_dtr, irq->ifr_rts);
			restore_flags(flags);
		}
		break;
	case SIOCSMODE:
		if (!capable(CAP_NET_ADMIN))
			ret = -EPERM;
		else {
			save_flags(flags);
			cli();
			irtty_set_mode(dev, irq->ifr_mode);
			restore_flags(flags);
		}
		break;
	default:
		ret = -EOPNOTSUPP;
	}
	
	return ret;
}

static struct net_device_stats *irtty_net_get_stats(struct net_device *dev)
{
	struct irtty_cb *self = (struct irtty_cb *) dev->priv;

	return &self->stats;
}

#ifdef MODULE

MODULE_AUTHOR("Dag Brattli <dagb@cs.uit.no>");
MODULE_DESCRIPTION("IrDA TTY device driver");
MODULE_LICENSE("GPL");


MODULE_PARM(qos_mtt_bits, "i");
MODULE_PARM_DESC(qos_mtt_bits, "Minimum Turn Time");

/*
 * Function init_module (void)
 *
 *    Initialize IrTTY module
 *
 */
int init_module(void)
{
	return irtty_init();
}

/*
 * Function cleanup_module (void)
 *
 *    Cleanup IrTTY module
 *
 */
void cleanup_module(void)
{
	irtty_cleanup();
}

#endif /* MODULE */
