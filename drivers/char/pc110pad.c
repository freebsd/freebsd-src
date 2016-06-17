/*
 *	Linux driver for the PC110 pad
 */
 
/**
 * 	DOC: PC110 Digitizer Hardware
 *
 *	The pad provides triples of data. The first byte has
 *	0x80=bit 8 X, 0x01=bit 7 X, 0x08=bit 8 Y, 0x01=still down
 *	The second byte is bits 0-6 X
 *	The third is bits 0-6 Y
 *
 *	This is read internally and used to synthesize a stream of
 *	triples in the form expected from a PS/2 device. Specialist
 *	applications can choose to obtain the pad data in other formats
 *	including a debugging mode.
 *
 *	It would be good to add a joystick driver mode to this pad so
 *	that doom and other game playing are better. One possible approach
 *	would be to deactive the mouse mode while the joystick port is opened.
 */
 
/*
 *	History
 *
 *	0.0 1997-05-16 Alan Cox <alan@redhat.com> - Pad reader
 *	0.1 1997-05-19 Robin O'Leary <robin@acm.org> - PS/2 emulation
 *	0.2 1997-06-03 Robin O'Leary <robin@acm.org> - tap gesture
 *	0.3 1997-06-27 Alan Cox <alan@redhat.com> - 2.1 commit
 *	0.4 1997-11-09 Alan Cox <alan@redhat.com> - Single Unix VFS API changes
 *	0.5 2000-02-10 Alan Cox <alan@redhat.com> - 2.3.x cleanup, documentation
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/signal.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/miscdevice.h>
#include <linux/ptrace.h>
#include <linux/poll.h>
#include <linux/ioport.h>
#include <linux/interrupt.h>
#include <linux/smp_lock.h>
#include <linux/init.h>

#include <asm/signal.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/semaphore.h>
#include <linux/spinlock.h>
#include <asm/uaccess.h>

#include "pc110pad.h"


static struct pc110pad_params default_params = {
	mode:			PC110PAD_PS2,
	bounce_interval:	50 MS,
	tap_interval:		200 MS,
	irq:			10,
	io:			0x15E0,
};

static struct pc110pad_params current_params;


/* driver/filesystem interface management */
static wait_queue_head_t queue;
static struct fasync_struct *asyncptr;
static int active;	/* number of concurrent open()s */
static struct semaphore reader_lock;

/**
 *	wake_readers:
 *
 *	Take care of letting any waiting processes know that
 *	now would be a good time to do a read().  Called
 *	whenever a state transition occurs, real or synthetic. Also
 *	issue any SIGIO's to programs that use SIGIO on mice (eg
 *	Executor)
 */
 
static void wake_readers(void)
{
	wake_up_interruptible(&queue);
	kill_fasync(&asyncptr, SIGIO, POLL_IN);
}


/*****************************************************************************/
/*
 * Deal with the messy business of synthesizing button tap and drag
 * events.
 *
 * Exports:
 *	notify_pad_up_down()
 *		Must be called whenever debounced pad up/down state changes.
 *	button_pending
 *		Flag is set whenever read_button() has new values
 *		to return.
 *	read_button()
 *		Obtains the current synthetic mouse button state.
 */

/*
 * These keep track of up/down transitions needed to generate the
 * synthetic mouse button events.  While recent_transition is set,
 * up/down events cause transition_count to increment.  tap_timer
 * turns off the recent_transition flag and may cause some synthetic
 * up/down mouse events to be created by incrementing synthesize_tap.
 */
 
static int button_pending;
static int recent_transition;
static int transition_count;
static int synthesize_tap;
static void tap_timeout(unsigned long data);
static struct timer_list tap_timer = { function: tap_timeout };


/**
 * tap_timeout:
 * @data: Unused
 *
 * This callback goes off a short time after an up/down transition;
 * before it goes off, transitions will be considered part of a
 * single PS/2 event and counted in transition_count.  Once the
 * timeout occurs the recent_transition flag is cleared and
 * any synthetic mouse up/down events are generated.
 */
 
static void tap_timeout(unsigned long data)
{
	if(!recent_transition)
	{
		printk(KERN_ERR "pc110pad: tap_timeout but no recent transition!\n");
	}
	if( transition_count==2 || transition_count==4 || transition_count==6 )
	{
		synthesize_tap+=transition_count;
		button_pending = 1;
		wake_readers();
	}
	recent_transition=0;
}


/**
 * notify_pad_up_down:
 *
 * Called by the raw pad read routines when a (debounced) up/down
 * transition is detected.
 */
 
void notify_pad_up_down(void)
{
	if(recent_transition)
	{
		transition_count++;
	}
	else
	{
		transition_count=1;
		recent_transition=1;
	}
	mod_timer(&tap_timer, jiffies + current_params.tap_interval);

	/* changes to transition_count can cause reported button to change */
	button_pending = 1;
	wake_readers();
}

/**
 *	read_button:
 *	@b: pointer to the button status.
 *
 *	The actual button state depends on what we are seeing. We have to check
 *	for the tap gesture and also for dragging.
 */

static void read_button(int *b)
{
	if(synthesize_tap)
	{
		*b=--synthesize_tap & 1;
	}
	else
	{
		*b=(!recent_transition && transition_count==3);	/* drag */
	}
	button_pending=(synthesize_tap>0);
}


/*****************************************************************************/
/*
 * Read pad absolute co-ordinates and debounced up/down state.
 *
 * Exports:
 *	pad_irq()
 *		Function to be called whenever the pad signals
 *		that it has new data available.
 *	read_raw_pad()
 *		Returns the most current pad state.
 *	xy_pending
 *		Flag is set whenever read_raw_pad() has new values
 *		to return.
 * Imports:
 *	wake_readers()
 *		Called when movement occurs.
 *	notify_pad_up_down()
 *		Called when debounced up/down status changes.
 */

/*
 * These are up/down state and absolute co-ords read directly from pad 
 */

static int raw_data[3];
static int raw_data_count;
static int raw_x, raw_y;	/* most recent absolute co-ords read */
static int raw_down;		/* raw up/down state */
static int debounced_down;	/* up/down state after debounce processing */
static enum { NO_BOUNCE, JUST_GONE_UP, JUST_GONE_DOWN } bounce=NO_BOUNCE;
				/* set just after an up/down transition */
static int xy_pending;	/* set if new data have not yet been read */

/* 
 * Timer goes off a short while after an up/down transition and copies
 * the value of raw_down to debounced_down.
 */
 
static void bounce_timeout(unsigned long data);
static struct timer_list bounce_timer = { function: bounce_timeout };



/**
 * bounce_timeout:
 * @data: Unused
 *
 * No further up/down transitions happened within the
 * bounce period, so treat this as a genuine transition.
 */

static void bounce_timeout(unsigned long data)
{
	switch(bounce)
	{
		case NO_BOUNCE:
		{
			/*
			 * Strange; the timer callback should only go off if
			 * we were expecting to do bounce processing!
			 */
			printk(KERN_WARNING "pc110pad, bounce_timeout: bounce flag not set!\n");
			break;
		}
		case JUST_GONE_UP:
		{
			/*
			 * The last up we spotted really was an up, so set
			 * debounced state the same as raw state.
			 */
			bounce=NO_BOUNCE;
			if(debounced_down==raw_down)
			{
				printk(KERN_WARNING "pc110pad, bounce_timeout: raw already debounced!\n");
			}
			debounced_down=raw_down;

			notify_pad_up_down();
			break;
		}
		case JUST_GONE_DOWN:
		{
			/*
			 * We don't debounce down events, but we still time
			 * out soon after one occurs so we can avoid the (x,y)
			 * skittering that sometimes happens.
			 */
			bounce=NO_BOUNCE;
			break;
		}
	}
}


/**
 * pad_irq:
 * @irq: Interrupt number
 * @ptr: Unused
 * @regs: Unused
 *
 * Callback when pad's irq goes off; copies values in to raw_* globals;
 * initiates debounce processing. This isn't SMP safe however there are
 * no SMP machines with a PC110 touchpad on them.
 */
 
static void pad_irq(int irq, void *ptr, struct pt_regs *regs)
{

	/* Obtain byte from pad and prime for next byte */
	{
		int value=inb_p(current_params.io);
		int handshake=inb_p(current_params.io+2);
		outb_p(handshake | 1, current_params.io+2);
		outb_p(handshake &~1, current_params.io+2);
		inb_p(0x64);

		raw_data[raw_data_count++]=value;
	}

	if(raw_data_count==3)
	{
		int new_down=raw_data[0]&0x01;
		int new_x=raw_data[1];
		int new_y=raw_data[2];
		if(raw_data[0]&0x10) new_x+=128;
		if(raw_data[0]&0x80) new_x+=256;
		if(raw_data[0]&0x08) new_y+=128;

		if( (raw_x!=new_x) || (raw_y!=new_y) )
		{
			raw_x=new_x;
			raw_y=new_y;
			xy_pending=1;
		}

		if(new_down != raw_down)
		{
			/* Down state has changed.  raw_down always holds
			 * the most recently observed state.
			 */
			raw_down=new_down;

			/* Forget any earlier bounce processing */
			if(bounce)
			{
				del_timer(&bounce_timer);
				bounce=NO_BOUNCE;
			}

			if(new_down)
			{
				if(debounced_down)
				{
					/* pad gone down, but we were reporting
					 * it down anyway because we suspected
					 * (correctly) that the last up was just
					 * a bounce
					 */
				}
				else
				{
					bounce=JUST_GONE_DOWN;
					mod_timer(&bounce_timer,
						jiffies+current_params.bounce_interval);
					/* start new stroke/tap */
					debounced_down=new_down;
					notify_pad_up_down();
				}
			}
			else /* just gone up */
			{
				if(recent_transition)
				{
					/* early bounces are probably part of
					 * a multi-tap gesture, so process
					 * immediately
					 */
					debounced_down=new_down;
					notify_pad_up_down();
				}
				else
				{
					/* don't trust it yet */
					bounce=JUST_GONE_UP;
					mod_timer(&bounce_timer,
						jiffies+current_params.bounce_interval);
				}
			}
		}
		wake_readers();
		raw_data_count=0;
	}
}

/**
 *	read_raw_pad:
 *	@down: set if the pen is down
 *	@debounced: set if the debounced pen position is down
 *	@x: X position
 *	@y: Y position
 *
 *	Retrieve the data saved by the interrupt handler and indicate we
 *	have no more pending XY to do. 
 *
 *	FIXME: We should switch to a spinlock for this.
 */

static void read_raw_pad(int *down, int *debounced, int *x, int *y)
{
	disable_irq(current_params.irq);
	{
		*down=raw_down;
		*debounced=debounced_down;
		*x=raw_x;
		*y=raw_y;
		xy_pending = 0;
	}
	enable_irq(current_params.irq);
}

/*****************************************************************************/
/*
 * Filesystem interface
 */

/* 
 * Read returns byte triples, so we need to keep track of
 * how much of a triple has been read.  This is shared across
 * all processes which have this device open---not that anything
 * will make much sense in that case.
 */
static int read_bytes[3];
static int read_byte_count;

/**
 *	sample_raw:
 *	@d: sample buffer
 *
 *	Retrieve a triple of sample data. 
 */


static void sample_raw(int d[3])
{
	d[0]=raw_data[0];
	d[1]=raw_data[1];
	d[2]=raw_data[2];
}

/**
 *	sample_rare:
 *	@d: sample buffer
 *
 *	Retrieve a triple of sample data and sanitize it. We do the needed
 *	scaling and masking to get the current status.
 */


static void sample_rare(int d[3])
{
	int thisd, thisdd, thisx, thisy;

	read_raw_pad(&thisd, &thisdd, &thisx, &thisy);

	d[0]=(thisd?0x80:0)
	   | (thisx/256)<<4
	   | (thisdd?0x08:0)
	   | (thisy/256)
	;
	d[1]=thisx%256;
	d[2]=thisy%256;
}

/**
 *	sample_debug:
 *	@d: sample buffer
 *
 *	Retrieve a triple of sample data and mix it up with the state 
 *	information in the gesture parser. Not useful for normal users but
 *	handy when debugging
 */

static void sample_debug(int d[3])
{
	int thisd, thisdd, thisx, thisy;
	int b;
	unsigned long flags;
	
	save_flags(flags);
	cli();
	read_raw_pad(&thisd, &thisdd, &thisx, &thisy);
	d[0]=(thisd?0x80:0) | (thisdd?0x40:0) | bounce;
	d[1]=(recent_transition?0x80:0)+transition_count;
	read_button(&b);
	d[2]=(synthesize_tap<<4) | (b?0x01:0);
	restore_flags(flags);
}

/**
 *	sample_ps2:
 *	@d: sample buffer
 *
 *	Retrieve a triple of sample data and turn the debounced tap and
 *	stroke information into what appears to be a PS/2 mouse. This means
 *	the PC110 pad needs no funny application side support.
 */


static void sample_ps2(int d[3])
{
	static int lastx, lasty, lastd;

	int thisd, thisdd, thisx, thisy;
	int dx, dy, b;

	/*
	 * Obtain the current mouse parameters and limit as appropriate for
	 * the return data format.  Interrupts are only disabled while 
	 * obtaining the parameters, NOT during the puts_fs_byte() calls,
	 * so paging in put_user() does not affect mouse tracking.
	 */
	read_raw_pad(&thisd, &thisdd, &thisx, &thisy);
	read_button(&b);

	/* Now compare with previous readings.  Note that we use the
	 * raw down flag rather than the debounced one.
	 */
	if( (thisd && !lastd) /* new stroke */
	 || (bounce!=NO_BOUNCE) )
	{
		dx=0;
		dy=0;
	}
	else
	{
		dx =  (thisx-lastx);
		dy = -(thisy-lasty);
	}
	lastx=thisx;
	lasty=thisy;
	lastd=thisd;

/*
	d[0]= ((dy<0)?0x20:0)
	    | ((dx<0)?0x10:0)
	    | 0x08
	    | (b? 0x01:0x00)
	;
*/
	d[0]= ((dy<0)?0x20:0)
	    | ((dx<0)?0x10:0)
	    | (b? 0x00:0x08)
	;
	d[1]=dx;
	d[2]=dy;
}


/**
 *	fasync_pad:
 *	@fd:	file number for the file 
 *	@filp:	file handle
 *	@on:	1 to add, 0 to remove a notifier
 *
 *	Update the queue of asynchronous event notifiers. We can use the
 *	same helper the mice do and that does almost everything we need.
 */
 
static int fasync_pad(int fd, struct file *filp, int on)
{
	int retval;

	retval = fasync_helper(fd, filp, on, &asyncptr);
	if (retval < 0)
		return retval;
	return 0;
}


/**
 *	close_pad:
 *	@inode: inode of pad
 *	@file: file handle to pad
 *
 *	Close access to the pad. We turn the pad power off if this is the
 *	last user of the pad. I've not actually measured the power draw but
 *	the DOS driver is careful to do this so we follow suit.
 */
 
static int close_pad(struct inode * inode, struct file * file)
{
	lock_kernel();
	fasync_pad(-1, file, 0);
	if (!--active)
		outb(0x30, current_params.io+2);  /* switch off digitiser */
	unlock_kernel();
	return 0;
}


/**
 *	open_pad:
 *	@inode: inode of pad
 *	@file: file handle to pad
 *
 *	Open access to the pad. We turn the pad off first (we turned it off
 *	on close but if this is the first open after a crash the state is
 *	indeterminate). The device has a small fifo so we empty that before
 *	we kick it back into action.
 */
 
static int open_pad(struct inode * inode, struct file * file)
{
	unsigned long flags;
	
	if (active++)
		return 0;

	save_flags(flags);
	cli();
	outb(0x30, current_params.io+2);	/* switch off digitiser */
	pad_irq(0,0,0);		/* read to flush any pending bytes */
	pad_irq(0,0,0);		/* read to flush any pending bytes */
	pad_irq(0,0,0);		/* read to flush any pending bytes */
	outb(0x38, current_params.io+2);	/* switch on digitiser */
	current_params = default_params;
	raw_data_count=0;		/* re-sync input byte counter */
	read_byte_count=0;		/* re-sync output byte counter */
	button_pending=0;
	recent_transition=0;
	transition_count=0;
	synthesize_tap=0;
	del_timer(&bounce_timer);
	del_timer(&tap_timer);
	restore_flags(flags);

	return 0;
}


/**
 *	write_pad:
 *	@file: File handle to the pad
 *	@buffer: Unused
 *	@count: Unused
 *	@ppos: Unused
 *
 *	Writes are disallowed. A true PS/2 mouse lets you write stuff. Everyone
 *	seems happy with this and not faking the write modes.
 */

static ssize_t write_pad(struct file * file, const char * buffer, size_t count, loff_t *ppos)
{
	return -EINVAL;
}


/*
 *	new_sample:
 *	@d: sample buffer
 *
 *	Fetch a new sample according the current mouse mode the pad is 
 *	using.
 */
 
void new_sample(int d[3])
{
	switch(current_params.mode)
	{
		case PC110PAD_RAW:	sample_raw(d);		break;
		case PC110PAD_RARE:	sample_rare(d);		break;
		case PC110PAD_DEBUG:	sample_debug(d);	break;
		case PC110PAD_PS2:	sample_ps2(d);		break;
	}
}


/**
 * read_pad:
 * @file: File handle to pad
 * @buffer: Target for the mouse data
 * @count: Buffer length
 * @ppos: Offset (unused)
 *
 * Read data from the pad. We use the reader_lock to avoid mess when there are
 * two readers. This shouldnt be happening anyway but we play safe.
 */
 
static ssize_t read_pad(struct file * file, char * buffer, size_t count, loff_t *ppos)
{
	int r;

	down(&reader_lock);
	for(r=0; r<count; r++)
	{
		if(!read_byte_count)
			new_sample(read_bytes);
		if(put_user(read_bytes[read_byte_count], buffer+r))
		{
			r = -EFAULT;
			break;
		}
		read_byte_count = (read_byte_count+1)%3;
	}
	up(&reader_lock);
	return r;
}


/**
 * pad_poll:
 * @file: File of the pad device
 * @wait: Poll table
 *
 * The pad is ready to read if there is a button or any position change
 * pending in the queue. The reading and interrupt routines maintain the
 * required state for us and do needed wakeups.
 */

static unsigned int pad_poll(struct file *file, poll_table * wait)
{
	poll_wait(file, &queue, wait);
    	if(button_pending || xy_pending)
		return POLLIN | POLLRDNORM;
	return 0;
}


/**
 *	pad_ioctl;
 *	@inode: Inode of the pad
 *	@file: File handle to the pad
 *	@cmd: Ioctl command
 *	@arg: Argument pointer
 *
 *	The PC110 pad supports two ioctls both of which use the pc110pad_params
 *	structure. GETP queries the current pad status. SETP changes the pad
 *	configuration. Changing configuration during normal mouse operations
 *	may give momentarily odd results as things like tap gesture state
 *	may be lost.
 */
 
static int pad_ioctl(struct inode *inode, struct file * file,
	unsigned int cmd, unsigned long arg)
{
	struct pc110pad_params new;

	if (!inode)
		return -EINVAL;
		
	switch (cmd) {
	case PC110PADIOCGETP:
		new = current_params;
		if(copy_to_user((void *)arg, &new, sizeof(new)))
			return -EFAULT;
		return 0;

	case PC110PADIOCSETP:
		if(copy_from_user(&new, (void *)arg, sizeof(new)))
			return -EFAULT;

		if( (new.mode<PC110PAD_RAW)
		 || (new.mode>PC110PAD_PS2)
		 || (new.bounce_interval<0)
		 || (new.tap_interval<0)
		)
			return -EINVAL;

		current_params.mode	= new.mode;
		current_params.bounce_interval	= new.bounce_interval;
		current_params.tap_interval	= new.tap_interval;
		return 0;
	}
	return -ENOTTY;
}


static struct file_operations pad_fops = {
	owner:		THIS_MODULE,
	read:		read_pad,
	write:		write_pad,
	poll:		pad_poll,
	ioctl:		pad_ioctl,
	open:		open_pad,
	release:	close_pad,
	fasync:		fasync_pad,
};


static struct miscdevice pc110_pad = {
	minor:		PC110PAD_MINOR,
	name:		"pc110 pad",
	fops:		&pad_fops,
};


/**
 *	pc110pad_init_driver:
 *
 *	We configure the pad with the default parameters (that is PS/2 
 *	emulation mode. We then claim the needed I/O and interrupt resources.
 *	Finally as a matter of paranoia we turn the pad off until we are
 *	asked to open it by an application.
 */

static char banner[] __initdata = KERN_INFO "PC110 digitizer pad at 0x%X, irq %d.\n";

static int __init pc110pad_init_driver(void)
{
	init_MUTEX(&reader_lock);
	current_params = default_params;

	if (request_irq(current_params.irq, pad_irq, 0, "pc110pad", 0)) {
		printk(KERN_ERR "pc110pad: Unable to get IRQ.\n");
		return -EBUSY;
	}
	if (!request_region(current_params.io, 4, "pc110pad"))	{
		printk(KERN_ERR "pc110pad: I/O area in use.\n");
		free_irq(current_params.irq,0);
		return -EBUSY;
	}
	init_waitqueue_head(&queue);
	printk(banner, current_params.io, current_params.irq);
	misc_register(&pc110_pad);
	outb(0x30, current_params.io+2);	/* switch off digitiser */
	return 0;
}

/*
 *	pc110pad_exit_driver:
 *
 *	Free the resources we acquired when the module was loaded. We also
 *	turn the pad off to be sure we don't leave it using power.
 */

static void __exit pc110pad_exit_driver(void)
{
	outb(0x30, current_params.io+2);	/* switch off digitiser */
	if (current_params.irq)
		free_irq(current_params.irq, 0);
	current_params.irq = 0;
	release_region(current_params.io, 4);
	misc_deregister(&pc110_pad);
}

module_init(pc110pad_init_driver);
module_exit(pc110pad_exit_driver);

MODULE_AUTHOR("Alan Cox, Robin O'Leary");
MODULE_DESCRIPTION("Driver for the touchpad on the IBM PC110 palmtop");
MODULE_LICENSE("GPL");

EXPORT_NO_SYMBOLS;
