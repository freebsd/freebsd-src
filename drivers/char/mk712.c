/* -*- c -*- --------------------------------------------------------- *
 *
 * linux/drivers/char/mk712.c
 *
 * Copyright 1999-2002 Transmeta Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * This driver supports the MK712 touch screen.
 * based on busmouse.c, pc_keyb.c, and other mouse drivers
 *
 * 1999-12-18: original version, Daniel Quinlan
 * 1999-12-19: added anti-jitter code, report pen-up events, fixed mk712_poll
 *             to use queue_empty, Nathan Laredo
 * 1999-12-20: improved random point rejection, Nathan Laredo
 * 2000-01-05: checked in new anti-jitter code, changed mouse protocol, fixed
 *             queue code, added module options, other fixes, Daniel Quinlan
 * 2002-03-15: Clean up for kernel merge <alan@redhat.com>
 *	       Fixed multi open race, fixed memory checks, fixed resource
 *	       allocation, fixed close/powerdown bug, switched to new init
 *
 * ------------------------------------------------------------------------- */

#include <linux/module.h>

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/signal.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/poll.h>
#include <linux/miscdevice.h>
#include <linux/random.h>
#include <linux/delay.h>
#include <linux/ioport.h>
#include <linux/slab.h>
#include <linux/interrupt.h>

#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/system.h>
#include <asm/irq.h>

#define DEBUG(x)	x
#define SQUARE(x)	((x)*(x))

#define MK712_DEFAULT_IO 0x260		/* demo board: 0x200, 0x208, 0x300 */
#define MK712_DEFAULT_IRQ 10		/* demo board: 10, 12, 14 or 15  */

/* eight 8-bit registers */
#define MK712_STATUS_LOW 0	/* READ */
#define MK712_STATUS_HIGH 1	/* READ */
#define MK712_X_LOW 2		/* READ */
#define MK712_X_HIGH 3		/* READ */
#define MK712_Y_LOW 4		/* READ */
#define MK712_Y_HIGH 5		/* READ */
#define MK712_CONTROL 6		/* R/W */
#define MK712_RATE 7		/* R/W */

/* status */
#define	MK712_STATUS_TOUCH 0x10
#define	MK712_CONVERSION_COMPLETE 0x80

#define MK712_ENABLE_INT			0x01 /* enable interrupts */
#define MK712_INT_ON_CONVERSION_COMPLETE	0x02 /* if bit 0 = 1 */
#define MK712_INT_ON_CHANGE_IN_TOUCH_STATUS_A	0x04 /* if bit 0 = 1 */
#define MK712_INT_ON_CHANGE_IN_TOUCH_STATUS_B	0x08 /* if bit 0 = 1 */
#define MK712_ENABLE_PERIODIC_CONVERSIONS	0x10
#define MK712_READ_ONE_POINT			0x20
#define MK712_POWERDOWN_A			0x40
#define MK712_POWERDOWN_B			0x80

#define MK712_BUF_SIZE 256      /* a page */

struct mk712_packet {
        unsigned int header;
        unsigned int x;
        unsigned int y;
        unsigned int reserved;
};

struct mk712_queue {
	unsigned long head;
	unsigned long tail;
	wait_queue_head_t proc_list;
	struct fasync_struct *fasync;
	struct mk712_packet buf[256];
};

#ifdef MODULE
static int io = 0;
static int irq = 0;
#endif
static int mk712_io = MK712_DEFAULT_IO;
static int mk712_irq = MK712_DEFAULT_IRQ;
static int mk712_users = 0;
static spinlock_t mk712_lock = SPIN_LOCK_UNLOCKED;
static struct mk712_queue *queue; /* mouse data buffer */

static struct mk712_packet get_from_queue(void)
{
	struct mk712_packet result;
	unsigned long flags;

	spin_lock_irqsave(&mk712_lock, flags);
	result = queue->buf[queue->tail];
	queue->tail = (queue->tail + 1) & (MK712_BUF_SIZE-1);
	spin_unlock_irqrestore(&mk712_lock, flags);
	return result;
}

static inline int queue_empty(void)
{
	return queue->head == queue->tail;
}

static int mk712_fasync(int fd, struct file *filp, int on)
{
	int retval;

	retval = fasync_helper(fd, filp, on, &queue->fasync);
	if (retval < 0)
		return retval;
	return 0;
}

static void mk712_output_packet(struct mk712_packet data)
{
        int head = queue->head;

        queue->buf[head] = data;
        head = (head + 1) & (MK712_BUF_SIZE-1);
        if (head != queue->tail) {
                queue->head = head;
                kill_fasync(&queue->fasync, SIGIO, POLL_IN);
                wake_up_interruptible(&queue->proc_list);
        }
}

static int points = 0;          /* number of stored points */
static int output_point = 0;    /* did I output a point since last release? */

static void mk712_output_point(int x, int y)
{
        struct mk712_packet t;

        t.header = 0;
        t.x = x;
        t.y = y;
        t.reserved = 0;

        mk712_output_packet(t);
        output_point = 1;
}

static void mk712_store_point(int x_new, int y_new)
{
        static int x[3], y[3];
        int x_out, y_out;

        x[points] = x_new;
        y[points] = y_new;

        if (points == 1 && abs(x[0] - x[1]) < 88 && abs(y[0] - y[1]) < 88)
	{
		x_out = (x[0] + x[1]) >> 1;
                y_out = (y[0] + y[1]) >> 1;
                mk712_output_point(x_out, y_out);
        }

        if (points == 2) {
                if ((abs(x[1] - x[2]) < 88 && abs(y[1] - y[2]) < 88) &&
                    (abs(x[0] - x[1]) < 88 && abs(y[0] - y[1]) < 88))
                {
                        x_out = (x[0] + x[1] + x[2]) / 3;
                        y_out = (y[0] + y[1] + y[2]) / 3;
                        mk712_output_point(x_out, y_out);
                }
                else if (abs(x[1] - x[2]) < 88 && abs(y[1] - y[2]) < 88)
                {
                        x_out = (x[1] + x[2]) >> 1;
                        y_out = (y[1] + y[2]) >> 1;
                        mk712_output_point(x_out, y_out);
                }
                else
                {
                        int x_avg, y_avg, d0, d1, d2;

                        x_avg = (x[0] + x[1] + x[2]) / 3;
                        y_avg = (y[0] + y[1] + y[2]) / 3;

                        d0 = SQUARE(x[0] - x_avg) + SQUARE(y[0] - y_avg);
                        d1 = SQUARE(x[1] - x_avg) + SQUARE(y[1] - y_avg);
                        d2 = SQUARE(x[2] - x_avg) + SQUARE(y[2] - y_avg);

                        if (d2 > d1 && d2 > d0)
			{
                                x_out = (x[0] + x[1]) >> 1;
                                y_out = (y[0] + y[1]) >> 1;
                        }
                        if (d1 > d0 && d1 > d2)
			{
                                x_out = (x[0] + x[2]) >> 1;
                                y_out = (y[0] + y[2]) >> 1;
                        }
                        else
                        {
                                x_out = (x[1] + x[2]) >> 1;
                                y_out = (y[1] + y[2]) >> 1;
                        }

                        mk712_output_point(x_out, y_out);

                        x[0] = x[1];
                        x[1] = x[2];
                        y[0] = y[1];
                        y[1] = y[2];
                }
        }
        else
	{
                points++;
        }
}

static void mk712_release_event(void)
{
        struct mk712_packet t;

        if (!output_point) {
                points = 0;
                return;
        }
        output_point = 0;

        t.header = 1;
        t.x = t.y = t.reserved = 0;

        mk712_output_packet(t);
        points = 0;
}

#define MK712_FILTER
static void mk712_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	unsigned short x;
	unsigned short y;
        unsigned char status;
	unsigned long flags;
#ifdef MK712_FILTER
        static int drop_next = 1;
#endif

	spin_lock_irqsave(&mk712_lock, flags);

        status = inb(mk712_io + MK712_STATUS_LOW);

	if (!(status & MK712_CONVERSION_COMPLETE)) {
#ifdef MK712_FILTER
                drop_next = 1;
#endif
		return;
	}
	if (!(status & MK712_STATUS_TOUCH))	/* release event */
	{
#ifdef MK712_FILTER
                drop_next = 1;
#endif
                mk712_release_event();

                spin_unlock_irqrestore(&mk712_lock, flags);
                wake_up_interruptible(&queue->proc_list);

		return;
	}

        x = inw(mk712_io + MK712_X_LOW) & 0x0fff;
        y = inw(mk712_io + MK712_Y_LOW) & 0x0fff;

#ifdef MK712_FILTER
        if (drop_next)
        {
                drop_next = 0;

                spin_unlock_irqrestore(&mk712_lock, flags);
                wake_up_interruptible(&queue->proc_list);

                return;
        }
#endif

        x = inw(mk712_io + MK712_X_LOW) & 0x0fff;
	y = inw(mk712_io + MK712_Y_LOW) & 0x0fff;

        mk712_store_point(x, y);

        spin_unlock_irqrestore(&mk712_lock, flags);
        wake_up_interruptible(&queue->proc_list);
}

static int mk712_open(struct inode *inode, struct file *file) 
{
	unsigned char control;
	unsigned long flags;

	control = 0;

	spin_lock_irqsave(&mk712_lock, flags);
	if(!mk712_users++)
	{
		outb(0, mk712_io + MK712_CONTROL);

		control |= (MK712_ENABLE_INT |
	                    MK712_INT_ON_CONVERSION_COMPLETE |
	                    MK712_INT_ON_CHANGE_IN_TOUCH_STATUS_B |
	                    MK712_ENABLE_PERIODIC_CONVERSIONS |
	                    MK712_POWERDOWN_A);
		outb(control, mk712_io + MK712_CONTROL);

	        outb(10, mk712_io + MK712_RATE); /* default count = 10 */

		queue->head = queue->tail = 0;          /* Flush input queue */
	}
	spin_unlock_irqrestore(&mk712_lock, flags);
	return 0;
}

static int mk712_close(struct inode * inode, struct file * file) {
        /* power down controller */
        unsigned long flags;
        spin_lock_irqsave(&mk712_lock, flags);
	if(--mk712_users==0)
		outb(0, mk712_io + MK712_CONTROL);
	spin_unlock_irqrestore(&mk712_lock, flags);
	return 0;
}

static unsigned int mk712_poll(struct file *file, poll_table *wait)
{
	poll_wait(file, &queue->proc_list, wait);
	if(!queue_empty())
		return POLLIN | POLLRDNORM;
	return 0;
}

static int mk712_ioctl(struct inode *inode, struct file * file,
	unsigned int cmd, unsigned long arg)
{
	if (!inode)
		BUG();
	return -ENOTTY;
}


static ssize_t mk712_read(struct file *file, char *buffer,
			  size_t count, loff_t *pos)
{
	DECLARE_WAITQUEUE(wait, current);
	ssize_t bytes_read = 0;
	struct mk712_packet p;

	/* wait for an event */
	if (queue_empty()) {
		if (file->f_flags & O_NONBLOCK)
			return -EAGAIN;
		add_wait_queue(&queue->proc_list, &wait);
repeat:
		set_current_state(TASK_INTERRUPTIBLE);
		if (queue_empty() && !signal_pending(current)) {
			schedule();
			goto repeat;
		}
		current->state = TASK_RUNNING;
		remove_wait_queue(&queue->proc_list, &wait);
	}

	while (bytes_read < count && !queue_empty()) {
		p = get_from_queue();
		if (copy_to_user (buffer+bytes_read, (void *) &p, sizeof(p)))
		{
			bytes_read = -EFAULT;
			break;
		}
		bytes_read += sizeof(p);
	}

        if (bytes_read > 0)
        {
                file->f_dentry->d_inode->i_atime = CURRENT_TIME;
                return bytes_read;
        }

	if (signal_pending(current))
		return -ERESTARTSYS;

	return bytes_read;
}

static ssize_t mk712_write(struct file *file, const char *buffer, size_t count,
			   loff_t *ppos)
{
	return -EINVAL;
}

struct file_operations mk712_fops = {
	owner: THIS_MODULE,
	read: mk712_read,
	write: mk712_write,
	poll: mk712_poll,
	ioctl: mk712_ioctl,
	open: mk712_open,
	release: mk712_close,
	fasync: mk712_fasync,
};

static struct miscdevice mk712_touchscreen = {
	MK712_MINOR, "mk712_touchscreen", &mk712_fops
};

int __init mk712_init(void)
{
#ifdef MODULE
        if (io)
                mk712_io = io;
        if (irq)
                mk712_irq = irq;
#endif

	if(!request_region(mk712_io, 8, "mk712_touchscreen"))
	{
		printk("mk712: unable to get IO region\n");
		return -ENODEV;
	}

	/* set up wait queue */
	queue = (struct mk712_queue *) kmalloc(sizeof(*queue), GFP_KERNEL);
	if(queue == NULL)
	{
		release_region(mk712_io, 8);
		return -ENOMEM;
	}
	memset(queue, 0, sizeof(*queue));
	queue->head = queue->tail = 0;
	init_waitqueue_head(&queue->proc_list);

        /* The MK712 is ISA and hard-coded to a particular IRQ, so the
           driver should keep the IRQ as long as it is loaded. */
	if(request_irq(mk712_irq, mk712_interrupt, 0, "mk712_touchscreen",
		       queue))
	{
		printk("mk712: unable to get IRQ\n");
		release_region(mk712_io, 8);
		kfree(queue);
		return -EBUSY;
	}

        /* register misc device */
	if(misc_register(&mk712_touchscreen)<0)
	{
		release_region(mk712_io, 8);
		kfree(queue);
		free_irq(mk712_irq, queue);
		return -ENODEV;
	}
	return 0;
}

static void __exit mk712_exit(void)
{
	misc_deregister(&mk712_touchscreen);
	release_region(mk712_io, 8);
	free_irq(mk712_irq, queue);
	kfree(queue);
	printk(KERN_INFO "mk712 touchscreen uninstalled\n");
}

MODULE_AUTHOR("Daniel Quinlan");
MODULE_DESCRIPTION("MK712 touch screen driver");
MODULE_PARM(io, "i");
MODULE_PARM_DESC(io, "I/O base address of MK712 touch screen controller");
MODULE_PARM(irq, "i");
MODULE_PARM_DESC(irq, "IRQ of MK712 touch screen controller");
MODULE_LICENSE("GPL");

module_init(mk712_init);
module_exit(mk712_exit);

/*
 * Local variables:
 * c-file-style: "linux"
 * End:
 */
