/*
 * linux/drivers/char/qpmouse.c
 *
 * Driver for a 82C710 C&T mouse interface chip.
 *
 * Based on the PS/2 driver by Johan Myreen.
 *
 * Corrections in device setup for some laptop mice & trackballs.
 * 02Feb93  (troyer@saifr00.cfsat.Honeywell.COM,mch@wimsey.bc.ca)
 *
 * Modified by Johan Myreen (jem@iki.fi) 04Aug93
 *   to include support for QuickPort mouse.
 *
 * Changed references to "QuickPort" with "82C710" since "QuickPort"
 * is not what this driver is all about -- QuickPort is just a
 * connector type, and this driver is for the mouse port on the Chips
 * & Technologies 82C710 interface chip. 15Nov93 jem@iki.fi
 *
 * Added support for SIGIO. 28Jul95 jem@iki.fi
 *
 * Rearranged SIGIO support to use code from tty_io.  9Sept95 ctm@ardi.com
 *
 * Modularised 8-Sep-95 Philip Blundell <pjb27@cam.ac.uk>
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/fcntl.h>
#include <linux/errno.h>
#include <linux/timer.h>
#include <linux/slab.h>
#include <linux/miscdevice.h>
#include <linux/random.h>
#include <linux/poll.h>
#include <linux/init.h>
#include <linux/smp_lock.h>

#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/system.h>
#include <asm/semaphore.h>

#include <linux/pc_keyb.h>		/* mouse enable command.. */


/*
 * We use the same minor number as the PS/2 mouse for (bad) historical
 * reasons..
 */
#define PSMOUSE_MINOR      1	       		/* Minor device # for this mouse */
#define QP_BUF_SIZE	2048

struct qp_queue {
	unsigned long head;
	unsigned long tail;
	wait_queue_head_t proc_list;
	struct fasync_struct *fasync;
	unsigned char buf[QP_BUF_SIZE];
};

static struct qp_queue *queue;

static unsigned int get_from_queue(void)
{
	unsigned int result;
	unsigned long flags;

	save_flags(flags);
	cli();
	result = queue->buf[queue->tail];
	queue->tail = (queue->tail + 1) & (QP_BUF_SIZE-1);
	restore_flags(flags);
	return result;
}


static inline int queue_empty(void)
{
	return queue->head == queue->tail;
}

static int fasync_qp(int fd, struct file *filp, int on)
{
	int retval;

	retval = fasync_helper(fd, filp, on, &queue->fasync);
	if (retval < 0)
		return retval;
	return 0;
}

/*
 *	82C710 Interface
 */

#define QP_DATA         0x310		/* Data Port I/O Address */
#define QP_STATUS       0x311		/* Status Port I/O Address */

#define QP_DEV_IDLE     0x01		/* Device Idle */
#define QP_RX_FULL      0x02		/* Device Char received */
#define QP_TX_IDLE      0x04		/* Device XMIT Idle */
#define QP_RESET        0x08		/* Device Reset */
#define QP_INTS_ON      0x10		/* Device Interrupt On */
#define QP_ERROR_FLAG   0x20		/* Device Error */
#define QP_CLEAR        0x40		/* Device Clear */
#define QP_ENABLE       0x80		/* Device Enable */

#define QP_IRQ          12

static int qp_present;
static int qp_count;
static int qp_data = QP_DATA;
static int qp_status = QP_STATUS;

static int poll_qp_status(void);
static int probe_qp(void);

/*
 * Interrupt handler for the 82C710 mouse port. A character
 * is waiting in the 82C710.
 */

static void qp_interrupt(int cpl, void *dev_id, struct pt_regs * regs)
{
	int head = queue->head;
	int maxhead = (queue->tail-1) & (QP_BUF_SIZE-1);

	add_mouse_randomness(queue->buf[head] = inb(qp_data));
	if (head != maxhead) {
		head++;
		head &= QP_BUF_SIZE-1;
	}
	queue->head = head;
	kill_fasync(&queue->fasync, SIGIO, POLL_IN);
	wake_up_interruptible(&queue->proc_list);
}

static int release_qp(struct inode * inode, struct file * file)
{
	unsigned char status;

	lock_kernel();
	fasync_qp(-1, file, 0);
	if (!--qp_count) {
		if (!poll_qp_status())
			printk(KERN_WARNING "Warning: Mouse device busy in release_qp()\n");
		status = inb_p(qp_status);
		outb_p(status & ~(QP_ENABLE|QP_INTS_ON), qp_status);
		if (!poll_qp_status())
			printk(KERN_WARNING "Warning: Mouse device busy in release_qp()\n");
		free_irq(QP_IRQ, NULL);
	}
	unlock_kernel();
	return 0;
}

/*
 * Install interrupt handler.
 * Enable the device, enable interrupts. 
 */

static int open_qp(struct inode * inode, struct file * file)
{
	unsigned char status;

	if (!qp_present)
		return -EINVAL;

	if (qp_count++)
		return 0;

	if (request_irq(QP_IRQ, qp_interrupt, 0, "PS/2 Mouse", NULL)) {
		qp_count--;
		return -EBUSY;
	}

	status = inb_p(qp_status);
	status |= (QP_ENABLE|QP_RESET);
	outb_p(status, qp_status);
	status &= ~(QP_RESET);
	outb_p(status, qp_status);

	queue->head = queue->tail = 0;          /* Flush input queue */
	status |= QP_INTS_ON;
	outb_p(status, qp_status);              /* Enable interrupts */

	while (!poll_qp_status()) {
		printk(KERN_ERR "Error: Mouse device busy in open_qp()\n");
		qp_count--;
		status &= ~(QP_ENABLE|QP_INTS_ON);
		outb_p(status, qp_status);
		free_irq(QP_IRQ, NULL);
		return -EBUSY;
	}

	outb_p(AUX_ENABLE_DEV, qp_data);	/* Wake up mouse */
	return 0;
}

/*
 * Write to the 82C710 mouse device.
 */

static ssize_t write_qp(struct file * file, const char * buffer,
			size_t count, loff_t *ppos)
{
	ssize_t i = count;

	while (i--) {
		char c;
		if (!poll_qp_status())
			return -EIO;
		get_user(c, buffer++);
		outb_p(c, qp_data);
	}
	file->f_dentry->d_inode->i_mtime = CURRENT_TIME;
	return count;
}

static unsigned int poll_qp(struct file *file, poll_table * wait)
{
	poll_wait(file, &queue->proc_list, wait);
	if (!queue_empty())
		return POLLIN | POLLRDNORM;
	return 0;
}

/*
 * Wait for device to send output char and flush any input char.
 */

#define MAX_RETRIES (60)

static int poll_qp_status(void)
{
	int retries=0;

	while ((inb(qp_status)&(QP_RX_FULL|QP_TX_IDLE|QP_DEV_IDLE))
		       != (QP_DEV_IDLE|QP_TX_IDLE)
		       && retries < MAX_RETRIES) {

		if (inb_p(qp_status)&(QP_RX_FULL))
			inb_p(qp_data);
		current->state = TASK_INTERRUPTIBLE;
		schedule_timeout((5*HZ + 99) / 100);
		retries++;
	}
	return !(retries==MAX_RETRIES);
}

/*
 * Put bytes from input queue to buffer.
 */

static ssize_t read_qp(struct file * file, char * buffer,
			size_t count, loff_t *ppos)
{
	DECLARE_WAITQUEUE(wait, current);
	ssize_t i = count;
	unsigned char c;

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
	while (i > 0 && !queue_empty()) {
		c = get_from_queue();
		put_user(c, buffer++);
		i--;
	}
	if (count-i) {
		file->f_dentry->d_inode->i_atime = CURRENT_TIME;
		return count-i;
	}
	if (signal_pending(current))
		return -ERESTARTSYS;
	return 0;
}

struct file_operations qp_fops = {
	owner:		THIS_MODULE,
	read:		read_qp,
	write:		write_qp,
	poll:		poll_qp,
	open:		open_qp,
	release:	release_qp,
	fasync:		fasync_qp,
};

/*
 * Initialize driver.
 */
static struct miscdevice qp_mouse = {
	minor:		PSMOUSE_MINOR,
	name:		"QPmouse",
	fops:		&qp_fops,
};

/*
 * Function to read register in 82C710.
 */

static inline unsigned char read_710(unsigned char index)
{
	outb_p(index, 0x390);			/* Write index */
	return inb_p(0x391);			/* Read the data */
}


/*
 * See if we can find a 82C710 device. Read mouse address.
 */

static int __init probe_qp(void)
{
	outb_p(0x55, 0x2fa);			/* Any value except 9, ff or 36 */
	outb_p(0xaa, 0x3fa);			/* Inverse of 55 */
	outb_p(0x36, 0x3fa);			/* Address the chip */
	outb_p(0xe4, 0x3fa);			/* 390/4; 390 = config address */
	outb_p(0x1b, 0x2fa);			/* Inverse of e4 */
	if (read_710(0x0f) != 0xe4)		/* Config address found? */
	  return 0;				/* No: no 82C710 here */
	qp_data = read_710(0x0d)*4;		/* Get mouse I/O address */
	qp_status = qp_data+1;
	outb_p(0x0f, 0x390);
	outb_p(0x0f, 0x391);			/* Close config mode */
	return 1;
}

static char msg_banner[] __initdata = KERN_INFO "82C710 type pointing device detected -- driver installed.\n";
static char msg_nomem[]  __initdata = KERN_ERR "qpmouse: no queue memory.\n";

static int __init qpmouse_init_driver(void)
{
	if (!probe_qp())
		return -EIO;

	printk(msg_banner);

/*	printk("82C710 address = %x (should be 0x310)\n", qp_data); */
	queue = (struct qp_queue *) kmalloc(sizeof(*queue), GFP_KERNEL);
	if (queue == NULL) {
		printk(msg_nomem);
		return -ENOMEM;
	}
	qp_present = 1;
	misc_register(&qp_mouse);
	memset(queue, 0, sizeof(*queue));
	queue->head = queue->tail = 0;
	init_waitqueue_head(&queue->proc_list);
	return 0;
}

static void __exit qpmouse_exit_driver(void)
{
	misc_deregister(&qp_mouse);
	kfree(queue);
}

module_init(qpmouse_init_driver);
module_exit(qpmouse_exit_driver);


MODULE_LICENSE("GPL");
EXPORT_NO_SYMBOLS;
