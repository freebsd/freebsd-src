/* 
 * Driver for PS/2 mouse on IOMD interface
 */
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/ptrace.h>
#include <linux/signal.h>
#include <linux/timer.h>
#include <linux/random.h>
#include <linux/ctype.h>
#include <linux/kbd_ll.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/poll.h>
#include <linux/miscdevice.h>

#include <asm/bitops.h>
#include <asm/irq.h>
#include <asm/hardware.h>
#include <asm/io.h>
#include <asm/hardware/iomd.h>
#include <asm/system.h>
#include <asm/uaccess.h>

/*
 *	PS/2 Auxiliary Device
 */

static struct aux_queue *queue;	/* Mouse data buffer. */
static int aux_count = 0;
/* used when we send commands to the mouse that expect an ACK. */
static unsigned char mouse_reply_expected = 0;

#define MAX_RETRIES	60		/* some aux operations take long time*/

/*
 *	Mouse Commands
 */

#define AUX_SET_RES		0xE8	/* Set resolution */
#define AUX_SET_SCALE11		0xE6	/* Set 1:1 scaling */
#define AUX_SET_SCALE21		0xE7	/* Set 2:1 scaling */
#define AUX_GET_SCALE		0xE9	/* Get scaling factor */
#define AUX_SET_STREAM		0xEA	/* Set stream mode */
#define AUX_SET_SAMPLE		0xF3	/* Set sample rate */
#define AUX_ENABLE_DEV		0xF4	/* Enable aux device */
#define AUX_DISABLE_DEV		0xF5	/* Disable aux device */
#define AUX_RESET		0xFF	/* Reset aux device */
#define AUX_ACK			0xFA	/* Command byte ACK. */

#define AUX_BUF_SIZE		2048	/* This might be better divisible by
					   three to make overruns stay in sync
					   but then the read function would 
					   need a lock etc - ick */

struct aux_queue {
	unsigned long head;
	unsigned long tail;
	wait_queue_head_t proc_list;
	struct fasync_struct *fasync;
	unsigned char buf[AUX_BUF_SIZE];
};

/*
 * Send a byte to the mouse.
 */
static void aux_write_dev(int val)
{
	while (!(iomd_readb(IOMD_MSECTL) & 0x80));
	iomd_writeb(val, IOMD_MSEDAT);
}

/*
 * Send a byte to the mouse & handle returned ack
 */
static void aux_write_ack(int val)
{
	while (!(iomd_readb(IOMD_MSECTL) & 0x80));
	iomd_writeb(val, IOMD_MSEDAT);

	/* we expect an ACK in response. */
	mouse_reply_expected++;
}

static unsigned char get_from_queue(void)
{
	unsigned char result;

	result = queue->buf[queue->tail];
	queue->tail = (queue->tail + 1) & (AUX_BUF_SIZE-1);
	return result;
}

static void psaux_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	int val = iomd_readb(IOMD_MSEDAT);

	if (mouse_reply_expected) {
		if (val == AUX_ACK) {
			mouse_reply_expected--;
			return;
		}
		mouse_reply_expected = 0;
	}

	add_mouse_randomness(val);
	if (aux_count) {
		int head = queue->head;

		queue->buf[head] = val;
		head = (head + 1) & (AUX_BUF_SIZE-1);
		if (head != queue->tail) {
			queue->head = head;
			kill_fasync(&queue->fasync, SIGIO, POLL_IN);
			wake_up_interruptible(&queue->proc_list);
		}
	}
}

static inline int queue_empty(void)
{
	return queue->head == queue->tail;
}

static int fasync_aux(int fd, struct file *filp, int on)
{
	int retval;

	retval = fasync_helper(fd, filp, on, &queue->fasync);
	if (retval < 0)
		return retval;
	return 0;
}


/*
 * Random magic cookie for the aux device
 */
#define AUX_DEV ((void *)queue)

static int release_aux(struct inode * inode, struct file * file)
{
	fasync_aux(-1, file, 0);
	if (--aux_count)
		return 0;
	free_irq(IRQ_MOUSERX, AUX_DEV);
	return 0;
}

/*
 * Install interrupt handler.
 * Enable auxiliary device.
 */

static int open_aux(struct inode * inode, struct file * file)
{
	if (aux_count++)
		return 0;

	queue->head = queue->tail = 0;		/* Flush input queue */
	if (request_irq(IRQ_MOUSERX, psaux_interrupt, SA_SHIRQ, "ps/2 mouse", 
			AUX_DEV)) {
		aux_count--;
		return -EBUSY;
	}

	aux_write_ack(AUX_ENABLE_DEV); /* Enable aux device */

	return 0;
}

/*
 * Put bytes from input queue to buffer.
 */

static ssize_t read_aux(struct file * file, char * buffer,
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
		current->state = TASK_INTERRUPTIBLE;
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

/*
 * Write to the aux device.
 */

static ssize_t write_aux(struct file * file, const char * buffer,
			 size_t count, loff_t *ppos)
{
	ssize_t retval = 0;

	if (count) {
		ssize_t written = 0;

		if (count > 32)
			count = 32; /* Limit to 32 bytes. */
		do {
			char c;
			get_user(c, buffer++);
			aux_write_dev(c);
			written++;
		} while (--count);
		retval = -EIO;
		if (written) {
			retval = written;
			file->f_dentry->d_inode->i_mtime = CURRENT_TIME;
		}
	}

	return retval;
}

static unsigned int aux_poll(struct file *file, poll_table * wait)
{
	poll_wait(file, &queue->proc_list, wait);
	if (!queue_empty())
		return POLLIN | POLLRDNORM;
	return 0;
}

struct file_operations psaux_fops = {
	read:		read_aux,
	write:		write_aux,
	poll:		aux_poll,
	open:		open_aux,
	release:	release_aux,
	fasync:		fasync_aux,
};

/*
 * Initialize driver.
 */
static struct miscdevice psaux_mouse = {
	PSMOUSE_MINOR, "psaux", &psaux_fops
};

int __init psaux_init(void)
{
	/* Reset the mouse state machine. */
	iomd_writeb(0, IOMD_MSECTL);
	iomd_writeb(8, IOMD_MSECTL);
  
	queue = (struct aux_queue *) kmalloc(sizeof(*queue), GFP_KERNEL);
	if (queue == NULL)
		return -ENOMEM;

	if (misc_register(&psaux_mouse)) {
		kfree(queue);
		return -ENODEV;
	}

	memset(queue, 0, sizeof(*queue));
	queue->head = queue->tail = 0;
	init_waitqueue_head(&queue->proc_list);

	aux_write_ack(AUX_SET_SAMPLE);
	aux_write_ack(100);			/* 100 samples/sec */
	aux_write_ack(AUX_SET_RES);
	aux_write_ack(3);			/* 8 counts per mm */
	aux_write_ack(AUX_SET_SCALE21);		/* 2:1 scaling */

	return 0;
}

module_init(psaux_init);

