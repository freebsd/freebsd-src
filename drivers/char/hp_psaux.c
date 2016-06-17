/*
 *      LASI PS/2 keyboard/psaux driver for HP-PARISC workstations
 *      
 *      (c) Copyright 1999 The Puffin Group Inc.
 *      by Alex deVries <adevries@thepuffingroup.com>
 *	Copyright 1999, 2000 Philipp Rumpf <prumpf@tux.org>
 *
 *	2000/10/26	Debacker Xavier (debackex@esiee.fr)
 *	implemented the psaux and controlled the mouse scancode based on pc_keyb.c
 *			Marteau Thomas (marteaut@esiee.fr)
 *	fixed leds control
 *
 *	2001/12/17	Marteau Thomas (marteaut@esiee.fr)
 *	get nice initialisation procedure
 */

#include <linux/config.h>

#include <linux/types.h>
#include <linux/ptrace.h>	/* interrupt.h wants struct pt_regs defined */
#include <linux/interrupt.h>
#include <linux/sched.h>	/* for request_irq/free_irq */
#include <linux/ioport.h>
#include <linux/kernel.h>
#include <linux/wait.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/pc_keyb.h>
#include <linux/kbd_kern.h>

/* mouse includes */
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/random.h>
#include <linux/spinlock.h>
#include <linux/smp_lock.h>
#include <linux/poll.h>

#include <asm/hardware.h>
#include <asm/keyboard.h>
#include <asm/gsc.h>
#include <asm/uaccess.h>

/* HP specific LASI PS/2 keyboard and psaux constants */
#define	AUX_REPLY_ACK	0xFA	/* Command byte ACK. */
#define	AUX_RESEND	0xFE	/* Sent by the keyb. Asking for resending the last command. */
#define	AUX_RECONNECT	0xAA	/* scancode when ps2 device is plugged (back) in */

#define	LASI_PSAUX_OFFSET 0x0100 /* offset from keyboard to psaux port */

#define	LASI_ID		0x00	/* ID and reset port offsets */
#define	LASI_RESET	0x00
#define	LASI_RCVDATA	0x04	/* receive and transmit port offsets */
#define	LASI_XMTDATA	0x04
#define	LASI_CONTROL	0x08	/* see: control register bits */
#define	LASI_STATUS	0x0C	/* see: status register bits */

/* control register bits */
#define LASI_CTRL_ENBL	0x01	/* enable interface */
#define LASI_CTRL_LPBXR	0x02	/* loopback operation */
#define LASI_CTRL_DIAG	0x20	/* directly control clock/data line */
#define LASI_CTRL_DATDIR 0x40	/* data line direct control */
#define LASI_CTRL_CLKDIR 0x80	/* clock line direct control */

/* status register bits */
#define LASI_STAT_RBNE	0x01
#define LASI_STAT_TBNE	0x02
#define LASI_STAT_TERR	0x04
#define LASI_STAT_PERR	0x08
#define LASI_STAT_CMPINTR 0x10
#define LASI_STAT_DATSHD 0x40
#define LASI_STAT_CLKSHD 0x80

static spinlock_t	kbd_controller_lock = SPIN_LOCK_UNLOCKED;
static unsigned long lasikbd_hpa;

static volatile int cmd_status;

static inline u8 read_input(unsigned long hpa)
{
	return gsc_readb(hpa+LASI_RCVDATA);
}

static inline u8 read_control(unsigned long hpa)
{
        return gsc_readb(hpa+LASI_CONTROL);
}

static inline void write_control(u8 val, unsigned long hpa)
{
	gsc_writeb(val, hpa+LASI_CONTROL);
}

static inline u8 read_status(unsigned long hpa)
{
        return gsc_readb(hpa+LASI_STATUS);
}

/* XXX should this grab the spinlock? */

static int write_output(u8 val, unsigned long hpa)
{
	int wait = 250;

	while (read_status(hpa) & LASI_STAT_TBNE) {
		if (!--wait) {
			return 0;
		}
		mdelay(1);
	}
	gsc_writeb(val, hpa+LASI_XMTDATA);

	return 1;
}

/* XXX should this grab the spinlock? */

static u8 wait_input(unsigned long hpa)
{
	int wait = 250;

	while (!(read_status(hpa) & LASI_STAT_RBNE)) {
		if (!--wait) {
			return 0;
		}
		mdelay(1);
	}
	return read_input(hpa);      
}

/* This function is the PA-RISC adaptation of i386 source */

static inline int aux_write_ack(u8 val)
{
      return write_output(val, lasikbd_hpa+LASI_PSAUX_OFFSET);
}

/* This is wrong, should do something like the pc driver, which sends
 * the command up to 3 times at 1 second intervals, checking once
 * per millisecond for an acknowledge.
 */

static void lasikbd_leds(unsigned char leds)
{
	int loop = 1000;

	if (!lasikbd_hpa)
		return;

	cmd_status=2;
	while (cmd_status!=0 && --loop > 0) {
		write_output(KBD_CMD_SET_LEDS, lasikbd_hpa);
		mdelay(5); 
	}
   
	cmd_status=2;
	while (cmd_status!=0 && --loop > 0) {
		write_output(leds, lasikbd_hpa);
		mdelay(5);
	}

	cmd_status=2;
	while (cmd_status!=0 && --loop > 0) {
	   write_output(KBD_CMD_ENABLE, lasikbd_hpa);   
	   mdelay(5);
	}
	if (loop <= 0)
		printk("lasikbd_leds: timeout\n");
}

#if 0
/* this might become useful again at some point.  not now  -prumpf */
int lasi_ps2_test(void *hpa)
{
	u8 control,c;
	int i, ret = 0;

	control = read_control(hpa);
	write_control(control | LASI_CTRL_LPBXR | LASI_CTRL_ENBL, hpa);

	for (i=0; i<256; i++) {
		write_output(i, hpa);

		while (!(read_status(hpa) & LASI_STAT_RBNE))
		    /* just wait */;
		    
		c = read_input(hpa);
		if (c != i)
			ret--;
	}

	write_control(control, hpa);

	return ret;
}
#endif 

static int init_keyb(unsigned long hpa)
{
	int res = 0;
	unsigned long flags;

	spin_lock_irqsave(&kbd_controller_lock, flags);

	if (write_output(KBD_CMD_SET_LEDS, hpa) &&
			wait_input(hpa) == AUX_REPLY_ACK &&
			write_output(0, hpa) &&
			wait_input(hpa) == AUX_REPLY_ACK &&
			write_output(KBD_CMD_ENABLE, hpa) &&
			wait_input(hpa) == AUX_REPLY_ACK)
		res = 1;

	spin_unlock_irqrestore(&kbd_controller_lock, flags);

	return res;
}


static void __init lasi_ps2_reset(unsigned long hpa)
{
	u8 control;

	/* reset the interface */
	gsc_writeb(0xff, hpa+LASI_RESET);
	gsc_writeb(0x0 , hpa+LASI_RESET);		

	/* enable it */
	control = read_control(hpa);
	write_control(control | LASI_CTRL_ENBL, hpa);
}

/* Greatly inspired by pc_keyb.c */

/*
 * Wait for keyboard controller input buffer to drain.
 *
 * Don't use 'jiffies' so that we don't depend on
 * interrupts..
 *
 * Quote from PS/2 System Reference Manual:
 *
 * "Address hex 0060 and address hex 0064 should be written only when
 * the input-buffer-full bit and output-buffer-full bit in the
 * Controller Status register are set 0."
 */
#ifdef CONFIG_PSMOUSE

static struct aux_queue	*queue;
static unsigned char	mouse_reply_expected;
static int 		aux_count;

static int fasync_aux(int fd, struct file *filp, int on)
{
	int retval;
	
	retval = fasync_helper(fd, filp, on, &queue->fasync);
	if (retval < 0)
		return retval;
	
	return 0;
}



static inline void handle_mouse_scancode(unsigned char scancode)
{
	if (mouse_reply_expected) {
		if (scancode == AUX_REPLY_ACK) {
			mouse_reply_expected--;
			return;
		}
		mouse_reply_expected = 0;
	}
	else if (scancode == AUX_RECONNECT) {
		queue->head = queue->tail = 0;  /* Flush input queue */
		return;
	}

	add_mouse_randomness(scancode);
	if (aux_count) {
		int head = queue->head;
				
		queue->buf[head] = scancode;
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

static unsigned char get_from_queue(void)
{
	unsigned char result;
	unsigned long flags;

	spin_lock_irqsave(&kbd_controller_lock, flags);
	result = queue->buf[queue->tail];
	queue->tail = (queue->tail + 1) & (AUX_BUF_SIZE-1);
	spin_unlock_irqrestore(&kbd_controller_lock, flags);

	return result;
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


static int open_aux(struct inode * inode, struct file * file)
{
	if (aux_count++) 
		return 0;

	queue->head = queue->tail = 0;	/* Flush input queue */
	aux_count = 1;
	aux_write_ack(AUX_ENABLE_DEV);	/* Enable aux device */
	
	return 0;
}


/* No kernel lock held - fine */
static unsigned int aux_poll(struct file *file, poll_table * wait)
{

	poll_wait(file, &queue->proc_list, wait);
	if (!queue_empty())
		return POLLIN | POLLRDNORM;
	return 0;
}


static int release_aux(struct inode * inode, struct file * file)
{
	lock_kernel();
	fasync_aux(-1, file, 0);
	if (--aux_count) {
		unlock_kernel();
		return 0;
	}
	unlock_kernel();
	return 0;
}

static struct file_operations psaux_fops = {
	read:		read_aux,
	write:		write_aux,
	poll:		aux_poll,
	open:		open_aux,
	release:	release_aux,
	fasync:		fasync_aux,
};

static struct miscdevice psaux_mouse = {
	minor:		PSMOUSE_MINOR,
	name:		"psaux",
	fops:		&psaux_fops,
};

#endif /* CONFIG_PSMOUSE */


/* This function is looking at the PS2 controller and empty the two buffers */

static u8 handle_lasikbd_event(unsigned long hpa)
{
        u8 status_keyb,status_mouse,scancode,id;
        extern void handle_at_scancode(int); /* in drivers/char/keyb_at.c */
        
        /* Mask to get the base address of the PS/2 controller */
        id = gsc_readb(hpa+LASI_ID) & 0x0f;
        
        if (id==1) 
		hpa -= LASI_PSAUX_OFFSET; 
	
        status_keyb = read_status(hpa);
        status_mouse = read_status(hpa+LASI_PSAUX_OFFSET);

        while ((status_keyb|status_mouse) & LASI_STAT_RBNE){
           
		while (status_keyb & LASI_STAT_RBNE) {
	      
		scancode = read_input(hpa);

	        /* XXX don't know if this is a valid fix, but filtering
	         * 0xfa avoids 'unknown scancode' errors on, eg, capslock
	         * on some keyboards.
	         */
	      	      
		if (scancode == AUX_REPLY_ACK) 
			cmd_status=0;
			
		else if (scancode == AUX_RESEND)
			cmd_status=1;
		else 
			handle_at_scancode(scancode); 
	      
		status_keyb =read_status(hpa);
		}
	   
#ifdef CONFIG_PSMOUSE
		while (status_mouse & LASI_STAT_RBNE) {
			scancode = read_input(hpa+LASI_PSAUX_OFFSET);
			handle_mouse_scancode(scancode);
			status_mouse = read_status(hpa+LASI_PSAUX_OFFSET);
		}
		status_mouse = read_status(hpa+LASI_PSAUX_OFFSET);
#endif /* CONFIG_PSMOUSE */
		status_keyb = read_status(hpa);
        }

        tasklet_schedule(&keyboard_tasklet);
        return (status_keyb|status_mouse);
}
	
extern struct pt_regs *kbd_pt_regs;

static void lasikbd_interrupt(int irq, void *dev, struct pt_regs *regs)
{
	kbd_pt_regs = regs;
	handle_lasikbd_event((unsigned long) dev);
}

extern int pckbd_translate(unsigned char, unsigned char *, char);
extern int pckbd_setkeycode(unsigned int, unsigned int);
extern int pckbd_getkeycode(unsigned int);

static struct kbd_ops gsc_ps2_kbd_ops = {
	setkeycode:     pckbd_setkeycode,
        getkeycode:     pckbd_getkeycode,
        translate:	pckbd_translate,
	leds:		lasikbd_leds,
#ifdef CONFIG_MAGIC_SYSRQ
	sysrq_key:	0x54,
	sysrq_xlate:	hp_ps2kbd_sysrq_xlate,
#endif
};



#if 1
/* XXX: HACK !!!
 * remove this function and the call in hil_kbd.c 
 * if hp_psaux.c/hp_keyb.c is converted to the input layer... */
int register_ps2_keybfuncs(void)
{
	gsc_ps2_kbd_ops.leds = NULL;
	register_kbd_ops(&gsc_ps2_kbd_ops);
}
EXPORT_SYMBOL(register_ps2_keybfuncs);
#endif


static int __init
lasi_ps2_register(struct parisc_device *dev)
{
	unsigned long hpa = dev->hpa;
	char *name;
	int device_found = 0;
	u8 id;

	id = gsc_readb(hpa+LASI_ID) & 0x0f;

	switch (id) {
	case 0:
		name = "keyboard";
		lasikbd_hpa = hpa;	/* save "hpa" for lasikbd_leds() */
		break;
	case 1:
		name = "psaux";
		break;
	default:
		printk(KERN_WARNING "%s: Unknown PS/2 port (id=%d) - ignored.\n",
			__FUNCTION__, id );
		return 0;
	}
	
	/* reset the PS/2 port */
	lasi_ps2_reset(hpa);

	switch (id) {
	case 0:	
	        device_found = init_keyb(hpa);
		if (device_found) register_kbd_ops(&gsc_ps2_kbd_ops);
		break;
	case 1:
#ifdef CONFIG_PSMOUSE
		queue = (struct aux_queue *) kmalloc(sizeof(*queue), GFP_KERNEL);
		if (!queue)
			return -ENOMEM;

		memset(queue, 0, sizeof(*queue));
		queue->head = queue->tail = 0;
		init_waitqueue_head(&queue->proc_list);
		
		misc_register(&psaux_mouse);

		aux_write_ack(AUX_ENABLE_DEV);
		/* try it a second time, this will give status if the device is
		 * available */
		device_found = aux_write_ack(AUX_ENABLE_DEV);
		break;
#else
		/* return without printing any unnecessary and misleading info */
		return 0;	
#endif
	} /* of case */
	
	if (device_found) {
	/* Here we claim only if we have a device attached */   
		/* allocate the irq and memory region for that device */
		if (!dev->irq)
	 	return -ENODEV;
	   
	  	if (request_irq(dev->irq, lasikbd_interrupt, 0, name, (void *)hpa))
	  	return -ENODEV;
	   
	  	if (!request_mem_region(hpa, LASI_STATUS + 4, name))
	  	return -ENODEV;
	}
	
	printk(KERN_INFO "PS/2 %s port at 0x%08lx (irq %d) found, "
			 "%sdevice attached.\n",
			name, hpa, dev->irq, device_found ? "":"no ");

	return 0;
}

static struct parisc_device_id lasi_psaux_tbl[] = {
	{ HPHW_FIO, HVERSION_REV_ANY_ID, HVERSION_ANY_ID, 0x00084 },
	{ 0, } /* 0 terminated list */
};

MODULE_DEVICE_TABLE(parisc, lasi_psaux_tbl);

static struct parisc_driver lasi_psaux_driver = {
	name:		"Lasi psaux",
	id_table:	lasi_psaux_tbl,
	probe:		lasi_ps2_register,
};

static int __init gsc_ps2_init(void) 
{
	return register_parisc_driver(&lasi_psaux_driver);
}

module_init(gsc_ps2_init);
