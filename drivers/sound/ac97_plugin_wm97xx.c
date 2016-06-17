/*
 * ac97_plugin_wm97xx.c  --  Touch screen driver for Wolfson WM9705 and WM9712
 *                           AC97 Codecs.
 *
 * Copyright 2003 Wolfson Microelectronics PLC.
 * Author: Liam Girdwood
 *         liam.girdwood@wolfsonmicro.com or linux@wolfsonmicro.com
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 *  THIS  SOFTWARE  IS PROVIDED   ``AS  IS'' AND   ANY  EXPRESS OR IMPLIED
 *  WARRANTIES,   INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 *  NO  EVENT  SHALL   THE AUTHOR  BE    LIABLE FOR ANY   DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 *  NOT LIMITED   TO, PROCUREMENT OF  SUBSTITUTE GOODS  OR SERVICES; LOSS OF
 *  USE, DATA,  OR PROFITS; OR  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 *  ANY THEORY OF LIABILITY, WHETHER IN  CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 *  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  You should have received a copy of the  GNU General Public License along
 *  with this program; if not, write  to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * Notes:
 *
 *  Features:
 *       - supports WM9705, WM9712
 *       - polling mode
 *       - coordinate polling
 *       - adjustable rpu/dpp settings
 *       - adjustable pressure current
 *       - adjustable sample settle delay
 *       - 4 and 5 wire touchscreens (5 wire is WM9712 only)
 *       - pen down detection
 *       - battery monitor
 *       - sample AUX adc's
 *       - power management
 *       - direct AC97 IO from userspace (#define WM97XX_TS_DEBUG)
 *
 *  TODO:
 *       - continuous mode
 *       - adjustable sample rate
 *       - AUX adc in coordinate / continous modes
 *	 - Official device identifier or misc device ?
 *
 *  Revision history
 *    7th May 2003   Initial version.
 *    6th June 2003  Added non module support and AC97 registration.
 *   18th June 2003  Added AUX adc sampling. 
 *   23rd June 2003  Did some minimal reformatting, fixed a couple of
 *		     locking bugs and noted a race to fix.
 *   24th June 2003  Added power management and fixed race condition.
 */

#include <linux/module.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/poll.h>
#include <linux/string.h>
#include <linux/proc_fs.h>
#include <linux/miscdevice.h>
#include <linux/pm.h>
#include <linux/wm97xx.h>       /* WM97xx registers and bits */
#include <asm/uaccess.h>        /* get_user,copy_to_user */
#include <asm/io.h>

#define TS_NAME "ac97_plugin_wm97xx"
#define TS_MINOR 16
#define WM_TS_VERSION "0.6"
#define AC97_NUM_REG 64


/*
 * Debug
 */
 
#define PFX TS_NAME
#define WM97XX_TS_DEBUG 0

#ifdef WM97XX_TS_DEBUG
#define dbg(format, arg...) printk(KERN_DEBUG PFX ": " format "\n" , ## arg)
#else
#define dbg(format, arg...) do {} while (0)
#endif
#define err(format, arg...) printk(KERN_ERR PFX ": " format "\n" , ## arg)
#define info(format, arg...) printk(KERN_INFO PFX ": " format "\n" , ## arg)
#define warn(format, arg...) printk(KERN_WARNING PFX ": " format "\n" , ## arg)

/*
 * Module parameters
 */
	
	
/*
 * Set the codec sample mode.
 *
 * The WM9712 can sample touchscreen data in 3 different operating
 * modes. i.e. polling, coordinate and continous.
 *
 * Polling:-     The driver polls the codec and issues 3 seperate commands
 *               over the AC97 link to read X,Y and pressure.
 * 
 * Coordinate: - The driver polls the codec and only issues 1 command over
 *               the AC97 link to read X,Y and pressure. This mode has
 *               strict timing requirements and may drop samples if 
 *               interrupted. However, it is less demanding on the AC97
 *               link. Note: this mode requires a larger delay than polling
 *               mode.
 *
 * Continuous:-  The codec automatically samples X,Y and pressure and then
 *               sends the data over the AC97 link in slots. This is the
 *               same method used by the codec when recording audio.
 *
 * Set mode = 0 for polling, 1 for coordinate and 2 for continuous.
 *            
 */
MODULE_PARM(mode,"i");
MODULE_PARM_DESC(mode, "Set WM97XX operation mode");
static int mode = 0;	
	
/*
 * WM9712 - Set internal pull up for pen detect. 
 * 
 * Pull up is in the range 1.02k (least sensitive) to 64k (most sensitive)
 * i.e. pull up resistance = 64k Ohms / rpu.
 * 
 * Adjust this value if you are having problems with pen detect not 
 * detecting any down events.
 */
MODULE_PARM(rpu,"i");
MODULE_PARM_DESC(rpu, "Set internal pull up resitor for pen detect.");
static int rpu = 0;	

/*
 * WM9705 - Pen detect comparator threshold. 
 * 
 * 0 to Vmid in 15 steps, 0 = use zero power comparator with Vmid threshold
 * i.e. 1 =  Vmid/15 threshold
 *      15 =  Vmid/1 threshold
 * 
 * Adjust this value if you are having problems with pen detect not 
 * detecting any down events.
 */
MODULE_PARM(pdd,"i");
MODULE_PARM_DESC(pdd, "Set pen detect comparator threshold");
static int pdd = 0;	
	
/*
 * Set current used for pressure measurement.
 *
 * Set pil = 2 to use 400uA 
 *     pil = 1 to use 200uA and
 *     pil = 0 to disable pressure measurement.
 *
 * This is used to increase the range of values returned by the adc
 * when measureing touchpanel pressure. 
 */
MODULE_PARM(pil,"i");
MODULE_PARM_DESC(pil, "Set current used for pressure measurement.");
static int pil = 0;

/*
 * WM9712 - Set five_wire = 1 to use a 5 wire touchscreen.
 * 
 * NOTE: Five wire mode does not allow for readback of pressure.
 */
MODULE_PARM(five_wire,"i");
MODULE_PARM_DESC(five_wire, "Set 5 wire touchscreen.");
static int five_wire = 0;	

/*
 * Set adc sample delay.
 * 
 * For accurate touchpanel measurements, some settling time may be
 * required between the switch matrix applying a voltage across the
 * touchpanel plate and the ADC sampling the signal.
 *
 * This delay can be set by setting delay = n, where n is the array
 * position of the delay in the array delay_table below.
 * Long delays > 1ms are supported for completeness, but are not
 * recommended.
 */
MODULE_PARM(delay,"i");
MODULE_PARM_DESC(delay, "Set adc sample delay.");
static int delay = 4;	


/* +++++++++++++ Lifted from include/linux/h3600_ts.h ++++++++++++++*/
typedef struct {
	unsigned short pressure;  // touch pressure
	unsigned short x;         // calibrated X
	unsigned short y;         // calibrated Y
	unsigned short millisecs; // timestamp of this event
} TS_EVENT;

typedef struct {
	int xscale;
	int xtrans;
	int yscale;
	int ytrans;
	int xyswap;
} TS_CAL;

/* Use 'f' as magic number */
#define IOC_MAGIC  'f'

#define TS_GET_RATE             _IO(IOC_MAGIC, 8)
#define TS_SET_RATE             _IO(IOC_MAGIC, 9)
#define TS_GET_CAL              _IOR(IOC_MAGIC, 10, TS_CAL)
#define TS_SET_CAL              _IOW(IOC_MAGIC, 11, TS_CAL)

/* +++++++++++++ Done lifted from include/linux/h3600_ts.h +++++++++*/

#define TS_GET_COMP1			_IOR(IOC_MAGIC, 12, short)
#define TS_GET_COMP2			_IOR(IOC_MAGIC, 13, short)
#define TS_GET_BMON			_IOR(IOC_MAGIC, 14, short)
#define TS_GET_WIPER			_IOR(IOC_MAGIC, 15, short)

#ifdef WM97XX_TS_DEBUG
/* debug get/set ac97 codec register ioctl's */
#define TS_GET_AC97_REG			_IOR(IOC_MAGIC, 20, short)
#define TS_SET_AC97_REG			_IOW(IOC_MAGIC, 21, short)
#define TS_SET_AC97_INDEX		_IOW(IOC_MAGIC, 22, short)
#endif

#define EVENT_BUFSIZE 128

typedef struct {
	TS_CAL cal;                       /* Calibration values */
	TS_EVENT event_buf[EVENT_BUFSIZE];/* The event queue */
	int nextIn, nextOut;
	int event_count;
	int is_wm9712:1;                  /* are we a WM912 or a WM9705 */
	int is_registered:1;              /* Is the driver AC97 registered */
	int line_pgal:5;
	int line_pgar:5;
	int phone_pga:5;
	int mic_pgal:5;
	int mic_pgar:5;
	int overruns;                     /* event buffer overruns */
	int adc_errs;                     /* sample read back errors */
#ifdef WM97XX_TS_DEBUG
	short ac97_index;
#endif
	struct fasync_struct *fasync;     /* asynch notification */
	struct timer_list acq_timer;      /* Timer for triggering acquisitions */
	wait_queue_head_t wait;           /* read wait queue */
	spinlock_t lock;
	struct ac97_codec *codec;
	struct proc_dir_entry *wm97xx_ts_ps;
#ifdef WM97XX_TS_DEBUG
	struct proc_dir_entry *wm97xx_debug_ts_ps;
#endif
	struct pm_dev * pm;
} wm97xx_ts_t;

static inline void poll_delay (void);
static int __init wm97xx_ts_init_module(void);
static int wm97xx_poll_read_adc (wm97xx_ts_t* ts, u16 adcsel, u16* sample);
static int wm97xx_coord_read_adc (wm97xx_ts_t* ts, u16* x, u16* y, 
                                  u16* pressure);
static inline int pendown (wm97xx_ts_t *ts);
static void wm97xx_acq_timer(unsigned long data);
static int wm97xx_fasync(int fd, struct file *filp, int mode);
static int wm97xx_ioctl(struct inode * inode, struct file *filp,
	                    unsigned int cmd, unsigned long arg);
static unsigned int wm97xx_poll(struct file * filp, poll_table * wait);
static ssize_t wm97xx_read(struct file * filp, char * buf, size_t count, 
	                       loff_t * l);
static int wm97xx_open(struct inode * inode, struct file * filp);
static int wm97xx_release(struct inode * inode, struct file * filp);
static void init_wm97xx_phy(void);
static int adc_get (wm97xx_ts_t *ts, unsigned short *value, int id);
static int wm97xx_probe(struct ac97_codec *codec, struct ac97_driver *driver);
static void wm97xx_remove(struct ac97_codec *codec,  struct ac97_driver *driver);
static void wm97xx_ts_cleanup_module(void);
static int wm97xx_pm_event(struct pm_dev *dev, pm_request_t rqst, void *data);
static void wm97xx_suspend(void);
static void wm97xx_resume(void);
static void wm9712_pga_save(wm97xx_ts_t* ts);
static void wm9712_pga_restore(wm97xx_ts_t* ts);

/* AC97 registration info */
static struct ac97_driver wm9705_driver = {
	codec_id: 0x574D4C05,
	codec_mask: 0xFFFFFFFF,
	name: "Wolfson WM9705 Touchscreen/BMON",
	probe:	wm97xx_probe,
	remove: __devexit_p(wm97xx_remove),
};

static struct ac97_driver wm9712_driver = {
	codec_id: 0x574D4C12,
	codec_mask: 0xFFFFFFFF,
	name: "Wolfson WM9712 Touchscreen/BMON",
	probe:	wm97xx_probe,
	remove: __devexit_p(wm97xx_remove),
};

/* we only support a single touchscreen */
static wm97xx_ts_t wm97xx_ts;

/*
 * ADC sample delay times in uS
 */
static const int delay_table[16] = {
	21,		// 1 AC97 Link frames
	42,		// 2
	84,		// 4
	167,		// 8
	333,		// 16
	667,		// 32
	1000,		// 48
	1333,		// 64
	2000,		// 96
	2667,		// 128
	3333,		// 160
	4000,		// 192
	4667,		// 224
	5333,		// 256
	6000,		// 288
	0 		// No delay, switch matrix always on
};

/*
 * Delay after issuing a POLL command.
 *
 * The delay is 3 AC97 link frames + the touchpanel settling delay
 */

static inline void poll_delay(void)
{ 
	int pdelay = 3 * AC97_LINK_FRAME + delay_table[delay];
	udelay (pdelay);
}


/*
 * sample the auxillary ADC's 
 */

static int adc_get(wm97xx_ts_t* ts, unsigned short * value, int id)
{
	short adcsel = 0;
	
	/* first find out our adcsel flag */
	if (ts->is_wm9712) {
		switch (id) {
			case TS_COMP1:
				adcsel = WM9712_ADCSEL_COMP1;
				break;
			case TS_COMP2:
				adcsel = WM9712_ADCSEL_COMP2;
				break;
			case TS_BMON:
				adcsel = WM9712_ADCSEL_BMON;
				break;
			case TS_WIPER:
				adcsel = WM9712_ADCSEL_WIPER;
				break;
		}
	} else {
		switch (id) {
			case TS_COMP1:
				adcsel = WM9705_ADCSEL_PCBEEP;
				break;
			case TS_COMP2:
				adcsel = WM9705_ADCSEL_PHONE;
				break;
			case TS_BMON:
				adcsel = WM9705_ADCSEL_BMON;
				break;
			case TS_WIPER:
				adcsel = WM9705_ADCSEL_AUX;
				break;
		}
	}
	
	/* now sample the adc */
	if (mode == 1) {
		/* coordinate mode - not currently available (TODO) */
			return 0;
	}
	else
	{
		/* polling mode */
		if (!wm97xx_poll_read_adc(ts, adcsel, value))
			return 0;	
	}
	
	return 1;
}


/*
 * Read a sample from the adc in polling mode.
 */
static int wm97xx_poll_read_adc (wm97xx_ts_t* ts, u16 adcsel, u16* sample)
{
	u16 dig1;
	int timeout = 5 * delay;

	/* set up digitiser */
	dig1 = ts->codec->codec_read(ts->codec, AC97_WM97XX_DIGITISER1); 
	dig1&=0x0fff;
	ts->codec->codec_write(ts->codec, AC97_WM97XX_DIGITISER1, dig1 | adcsel |
		WM97XX_POLL); 

	/* wait 3 AC97 time slots + delay for conversion */
	poll_delay();

	/* wait for POLL to go low */
	while ((ts->codec->codec_read(ts->codec, AC97_WM97XX_DIGITISER1) & WM97XX_POLL) && timeout) { 
		udelay(AC97_LINK_FRAME);
		timeout--;	
	}
	if (timeout > 0)
		*sample = ts->codec->codec_read(ts->codec, AC97_WM97XX_DIGITISER_RD);
	else {
		ts->adc_errs++;
		err ("adc sample timeout");
		return 0;
	}
	
	/* check we have correct sample */
	if ((*sample & 0x7000) != adcsel ) { 
		err ("adc wrong sample, read %x got %x", adcsel, *sample & 0x7000);
		return 0;
	}
	return 1;
}

/*
 * Read a sample from the adc in coordinate mode.
 */
static int wm97xx_coord_read_adc(wm97xx_ts_t* ts, u16* x, u16* y, u16* pressure)
{
	u16 dig1;
	int timeout = 5 * delay;

	/* set up digitiser */
	dig1 = ts->codec->codec_read(ts->codec, AC97_WM97XX_DIGITISER1); 
	dig1&=0x0fff;
	ts->codec->codec_write(ts->codec, AC97_WM97XX_DIGITISER1, dig1 | WM97XX_ADCSEL_PRES |
		WM97XX_POLL); 

	/* wait 3 AC97 time slots + delay for conversion */
	poll_delay();
	
	/* read X then wait for 1 AC97 link frame + settling delay */
	*x = ts->codec->codec_read(ts->codec, AC97_WM97XX_DIGITISER_RD);
	udelay (AC97_LINK_FRAME + delay_table[delay]);

	/* read Y */
	*y = ts->codec->codec_read(ts->codec, AC97_WM97XX_DIGITISER_RD);
	
	/* wait for POLL to go low and then read pressure */
	while ((ts->codec->codec_read(ts->codec, AC97_WM97XX_DIGITISER1) & WM97XX_POLL)&& timeout) {
			udelay(AC97_LINK_FRAME);
			timeout--;
	}
	if (timeout > 0)		
		*pressure = ts->codec->codec_read(ts->codec, AC97_WM97XX_DIGITISER_RD);
	else {
		ts->adc_errs++;
		err ("adc sample timeout");
		return 0;
	}
	
	/* check we have correct samples */
	if (((*x & 0x7000) == 0x1000) && ((*y & 0x7000) == 0x2000) && 
		((*pressure & 0x7000) == 0x3000)) { 
		return 1;
	} else {
		ts->adc_errs++;
		err ("adc got wrong samples, got x 0x%x y 0x%x pressure 0x%x", *x, *y, *pressure);
		return 0;
	}
}

/*
 * Is the pen down ?
 */
static inline int pendown (wm97xx_ts_t *ts)
{
	return ts->codec->codec_read(ts->codec, AC97_WM97XX_DIGITISER_RD) & WM97XX_PEN_DOWN;
}

/*
 * X,Y coordinates and pressure aquisition function.
 * This function is run by a kernel timer and it's frequency between
 * calls is the touchscreen polling rate;
 */
 
static void wm97xx_acq_timer(unsigned long data)
{
	wm97xx_ts_t* ts = (wm97xx_ts_t*)data;
	unsigned long flags;
	long x,y;
	TS_EVENT event;
	
	spin_lock_irqsave(&ts->lock, flags);

	/* are we still registered ? */
	if (!ts->is_registered) {
		spin_unlock_irqrestore(&ts->lock, flags);
		return; /* we better stop then */
	}
	
	/* read coordinates if pen is down */
	if (!pendown(ts))
		goto acq_exit;
	
	if (mode == 1) {
		/* coordinate mode */
		if (!wm97xx_coord_read_adc(ts, (u16*)&x, (u16*)&y, &event.pressure))
			goto acq_exit;
	} else
	{
		/* polling mode */
		if (!wm97xx_poll_read_adc(ts, WM97XX_ADCSEL_X, (u16*)&x))
			goto acq_exit;
		if (!wm97xx_poll_read_adc(ts, WM97XX_ADCSEL_Y, (u16*)&y))
			goto acq_exit;
		
		/* only read pressure if we have to */
		if (!five_wire && pil) {
			if (!wm97xx_poll_read_adc(ts, WM97XX_ADCSEL_PRES, &event.pressure))
				goto acq_exit;
		}
		else
			event.pressure = 0;
	}
	/* timestamp this new event. */
	event.millisecs = jiffies;

	/* calibrate and remove unwanted bits from samples */
	event.pressure &= 0x0fff;
	
	x &= 0x00000fff;
	x = ((ts->cal.xscale * x) >> 8) + ts->cal.xtrans;
	event.x = (u16)x;
	
	y &= 0x00000fff;
	y = ((ts->cal.yscale * y) >> 8) + ts->cal.ytrans;
	event.y = (u16)y;
	
	/* add this event to the event queue */
	ts->event_buf[ts->nextIn++] = event;
	if (ts->nextIn == EVENT_BUFSIZE)
		ts->nextIn = 0;
	if (ts->event_count < EVENT_BUFSIZE) {
		ts->event_count++;
	} else {
		/* throw out the oldest event */
		if (++ts->nextOut == EVENT_BUFSIZE) {
			ts->nextOut = 0;
			ts->overruns++;
		}
	}

	/* async notify */
	if (ts->fasync)
		kill_fasync(&ts->fasync, SIGIO, POLL_IN);
	/* wake up any read call */
	if (waitqueue_active(&ts->wait))
		wake_up_interruptible(&ts->wait);

	/* schedule next acquire */
acq_exit:
	ts->acq_timer.expires = jiffies + HZ / 100;
	add_timer(&ts->acq_timer);

	spin_unlock_irqrestore(&ts->lock, flags);
}
	
	
/* +++++++++++++ File operations ++++++++++++++*/

static int wm97xx_fasync(int fd, struct file *filp, int mode)
{
	wm97xx_ts_t* ts = (wm97xx_ts_t*)filp->private_data;
	return fasync_helper(fd, filp, mode, &ts->fasync);
}

static int wm97xx_ioctl(struct inode * inode, struct file *filp,
	     unsigned int cmd, unsigned long arg)
{
	unsigned short adc_value;
#ifdef WM97XX_TS_DEBUG
	short data;
#endif	
	wm97xx_ts_t* ts = (wm97xx_ts_t*)filp->private_data;

	switch(cmd) {
	case TS_GET_RATE:       /* TODO: what is this? */
		break;
	case TS_SET_RATE:       /* TODO: what is this? */
		break;
	case TS_GET_CAL:
		if(copy_to_user((char *)arg, (char *)&ts->cal, sizeof(TS_CAL)))
			return -EFAULT;
		break;
	case TS_SET_CAL:
		if(copy_from_user((char *)&ts->cal, (char *)arg, sizeof(TS_CAL)))
			return -EFAULT;
		break;
	case TS_GET_COMP1:
		if (adc_get(ts, &adc_value, TS_COMP1)) {
			if(copy_to_user((char *)arg, (char *)&adc_value, sizeof(adc_value)))
				return -EFAULT;
		}
		else
			return -EIO;
		break;
	case TS_GET_COMP2:
		if (adc_get(ts, &adc_value, TS_COMP2)) {
			if(copy_to_user((char *)arg, (char *)&adc_value, sizeof(adc_value)))
				return -EFAULT;
		}
		else
			return -EIO;
		break;
	case TS_GET_BMON:
		if (adc_get(ts, &adc_value, TS_BMON)) {
			if(copy_to_user((char *)arg, (char *)&adc_value, sizeof(adc_value)))
				return -EFAULT;
		}
		else
			return -EIO;
		break;
	case TS_GET_WIPER:
		if (adc_get(ts, &adc_value, TS_WIPER)) {
			if(copy_to_user((char *)arg, (char *)&adc_value, sizeof(adc_value)))
				return -EFAULT;
		}
		else
			return -EIO;
		break;
#ifdef WM97XX_TS_DEBUG
		/* debug get/set ac97 codec register ioctl's 
		 *
		 * This is direct IO to the codec registers - BE CAREFULL
		 */
	case TS_GET_AC97_REG: /* read from ac97 reg (index) */
		data = ts->codec->codec_read(ts->codec, ts->ac97_index);
		if(copy_to_user((char *)arg, (char *)&data, sizeof(data)))
			return -EFAULT;
		break;
	case TS_SET_AC97_REG: /* write to ac97 reg (index) */
		if(copy_from_user((char *)&data, (char *)arg, sizeof(data)))
			return -EFAULT;
		ts->codec->codec_write(ts->codec, ts->ac97_index, data);
		break;
	case TS_SET_AC97_INDEX: /* set ac97 reg index */
		if(copy_from_user((char *)&ts->ac97_index, (char *)arg, sizeof(ts->ac97_index)))
			return -EFAULT;
		break;
#endif
	default:
		return -EINVAL;
	}

	return 0;
}

static unsigned int wm97xx_poll(struct file * filp, poll_table * wait)
{
	wm97xx_ts_t *ts = (wm97xx_ts_t *)filp->private_data;
	poll_wait(filp, &ts->wait, wait);
	if (ts->event_count)
		return POLLIN | POLLRDNORM;
	return 0;
}

static ssize_t wm97xx_read(struct file *filp, char *buf, size_t count, loff_t *l)
{
	wm97xx_ts_t* ts = (wm97xx_ts_t*)filp->private_data;
	unsigned long flags;
	TS_EVENT event;
	int i;

	/* are we still registered with AC97 layer ? */
	spin_lock_irqsave(&ts->lock, flags);
	if (!ts->is_registered) {
		spin_unlock_irqrestore(&ts->lock, flags);
		return -ENXIO;
	}
	
	if (ts->event_count == 0) {
		if (filp->f_flags & O_NONBLOCK)
			return -EAGAIN;
		spin_unlock_irqrestore(&ts->lock, flags);

		wait_event_interruptible(ts->wait, ts->event_count != 0);
		
		/* are we still registered after sleep ? */
		spin_lock_irqsave(&ts->lock, flags);
		if (!ts->is_registered) {
			spin_unlock_irqrestore(&ts->lock, flags);
			return -ENXIO;
		}
		if (signal_pending(current))
			return -ERESTARTSYS;
	}
	
	for (i = count; i >= sizeof(TS_EVENT);
	    i -= sizeof(TS_EVENT), buf += sizeof(TS_EVENT)) {
		if (ts->event_count == 0)
			break;
		spin_lock_irqsave(&ts->lock, flags);
		event = ts->event_buf[ts->nextOut++];
		if (ts->nextOut == EVENT_BUFSIZE)
			ts->nextOut = 0;
		if (ts->event_count)
			ts->event_count--;
		spin_unlock_irqrestore(&ts->lock, flags);
		if(copy_to_user(buf, &event, sizeof(TS_EVENT)))
			return i != count  ? count - i : -EFAULT;
	}
	return count - i;
}


static int wm97xx_open(struct inode * inode, struct file * filp)
{
	wm97xx_ts_t* ts;
	unsigned long flags;
	u16 val;
	int minor = MINOR(inode->i_rdev);
	
	if (minor != TS_MINOR)
		return -ENODEV;
	
	filp->private_data = ts = &wm97xx_ts;

	spin_lock_irqsave(&ts->lock, flags);
	
	/* are we registered with AC97 layer ? */
	if (!ts->is_registered) {
		spin_unlock_irqrestore(&ts->lock, flags);
		return -ENXIO;
	}
	
	/* start digitiser */
	val = ts->codec->codec_read(ts->codec, AC97_WM97XX_DIGITISER2);
	ts->codec->codec_write(ts->codec, AC97_WM97XX_DIGITISER2, 
		val | WM97XX_PRP_DET_DIG);
	
	/* flush event queue */
	ts->nextIn = ts->nextOut = ts->event_count = 0;
	
	/* Set up timer. */
	init_timer(&ts->acq_timer);
	ts->acq_timer.function = wm97xx_acq_timer;
	ts->acq_timer.data = (unsigned long)ts;
	ts->acq_timer.expires = jiffies + HZ / 100;
	add_timer(&ts->acq_timer);

	spin_unlock_irqrestore(&ts->lock, flags);
	return 0;
}

static int wm97xx_release(struct inode * inode, struct file * filp)
{
	wm97xx_ts_t* ts = (wm97xx_ts_t*)filp->private_data;
	unsigned long flags;
	u16 val;
	
	wm97xx_fasync(-1, filp, 0);
	del_timer_sync(&ts->acq_timer);

	spin_lock_irqsave(&ts->lock, flags);
	
	/* stop digitiser */
	val = ts->codec->codec_read(ts->codec, AC97_WM97XX_DIGITISER2);
	ts->codec->codec_write(ts->codec, AC97_WM97XX_DIGITISER2, 
		val & ~WM97XX_PRP_DET_DIG);
	
	spin_unlock_irqrestore(&ts->lock, flags);
	return 0;
}

static struct file_operations ts_fops = {
	owner:		THIS_MODULE,
	read:           wm97xx_read,
	poll:           wm97xx_poll,
	ioctl:		wm97xx_ioctl,
	fasync:         wm97xx_fasync,
	open:		wm97xx_open,
	release:	wm97xx_release,
};

/* +++++++++++++ End File operations ++++++++++++++*/

#ifdef CONFIG_PROC_FS
static int wm97xx_read_proc (char *page, char **start, off_t off,
		    int count, int *eof, void *data)
{
	int len = 0, prpu;
	u16 dig1, dig2, digrd, adcsel, adcsrc, slt, prp, rev;
	unsigned long flags;
	char srev = ' ';
	
	wm97xx_ts_t* ts;

	if ((ts = data) == NULL)
		return -ENODEV;
	
	spin_lock_irqsave(&ts->lock, flags);
	if (!ts->is_registered) {
		spin_unlock_irqrestore(&ts->lock, flags);
		len += sprintf (page+len, "No device registered\n");
		return len;
	}

	dig1 = ts->codec->codec_read(ts->codec, AC97_WM97XX_DIGITISER1);
	dig2 = ts->codec->codec_read(ts->codec, AC97_WM97XX_DIGITISER2);
	digrd = ts->codec->codec_read(ts->codec, AC97_WM97XX_DIGITISER_RD);
	rev = (ts->codec->codec_read(ts->codec, AC97_WM9712_REV) & 0x000c) >> 2;

	spin_unlock_irqrestore(&ts->lock, flags);
	
	adcsel = dig1 & 0x7000;
	adcsrc = digrd & 0x7000;
	slt = (dig1 & 0x7) + 5;
	prp = dig2 & 0xc000;
	prpu = dig2 & 0x003f;

	/* driver version */
	len += sprintf (page+len, "Wolfson WM97xx Version %s\n", WM_TS_VERSION);
	
	/* what we are using */
	len += sprintf (page+len, "Using %s", ts->is_wm9712 ? "WM9712" : "WM9705");
	if (ts->is_wm9712) {
		switch (rev) {
			case 0x0:
				srev = 'A';
			break;
			case 0x1:
				srev = 'B';
			break;
			case 0x2:
				srev = 'D';
			break;
			case 0x3:
				srev = 'E';
			break;
		}
		len += sprintf (page+len, " silicon rev %c\n",srev);
	} else
		len += sprintf (page+len, "\n");
		
	/* WM97xx settings */
	len += sprintf (page+len, "Settings     :\n%s%s%s%s",
			dig1 & WM97XX_POLL ? " -sampling adc data(poll)\n" : "",
			adcsel ==  WM97XX_ADCSEL_X ? " -adc set to X coordinate\n" : "",
			adcsel ==  WM97XX_ADCSEL_Y ? " -adc set to Y coordinate\n" : "",
			adcsel ==  WM97XX_ADCSEL_PRES ? " -adc set to pressure\n" : "");
	if (ts->is_wm9712) {
		len += sprintf (page+len, "%s%s%s%s", 
			adcsel ==  WM9712_ADCSEL_COMP1 ? " -adc set to COMP1/AUX1\n" : "",
			adcsel ==  WM9712_ADCSEL_COMP2 ? " -adc set to COMP2/AUX2\n" : "",
			adcsel ==  WM9712_ADCSEL_BMON ? " -adc set to BMON\n" : "",
			adcsel ==  WM9712_ADCSEL_WIPER ? " -adc set to WIPER\n" : "");
		} else {
		len += sprintf (page+len, "%s%s%s%s",
			adcsel ==  WM9705_ADCSEL_PCBEEP ? " -adc set to PCBEEP\n" : "",
			adcsel ==  WM9705_ADCSEL_PHONE ? " -adc set to PHONE\n" : "",
			adcsel ==  WM9705_ADCSEL_BMON ? " -adc set to BMON\n" : "",
			adcsel ==  WM9705_ADCSEL_AUX ? " -adc set to AUX\n" : "");
		}
		
	len += sprintf (page+len, "%s%s%s%s%s%s",
			dig1 & WM97XX_COO ? " -coordinate sampling\n" : " -individual sampling\n",
			dig1 & WM97XX_CTC ? " -continuous mode\n" : " -polling mode\n",
			prp == WM97XX_PRP_DET ? " -pen detect enabled, no wake up\n" : "",
			prp == WM97XX_PRP_DETW ? " -pen detect enabled, wake up\n" : "",
			prp == WM97XX_PRP_DET_DIG ? " -pen digitiser and pen detect enabled\n" : "",
			dig1 & WM97XX_SLEN ? " -read back using slot " : " -read back using AC97\n");
	
	if ((dig1 & WM97XX_SLEN) && slt !=12)	
		len += sprintf(page+len, "%d\n", slt);
	len += sprintf (page+len, " -adc sample delay %d uSecs\n", delay_table[(dig1 & 0x00f0) >> 4]);
	
	if (ts->is_wm9712) {
		if (prpu)
			len += sprintf (page+len, " -rpu %d Ohms\n", 64000/ prpu);
		len += sprintf (page+len, " -pressure current %s uA\n", dig2 & WM9712_PIL ? "400" : "200");
		len += sprintf (page+len, " -using %s wire touchscreen mode", dig2 & WM9712_45W ? "5" : "4");
	} else {
		len += sprintf (page+len, " -pressure current %s uA\n", dig2 & WM9705_PIL ? "400" : "200");
		len += sprintf (page+len, " -%s impedance for PHONE and PCBEEP\n", dig2 & WM9705_PHIZ ? "high" : "low");
	}
	
	/* WM97xx digitiser read */
	len += sprintf(page+len, "\nADC data:\n%s%d\n%s%s\n",
		" -adc value (decimal) : ", digrd & 0x0fff,
		" -pen ", digrd & 0x8000 ? "Down" : "Up");
	if (ts->is_wm9712) {
		len += sprintf (page+len, "%s%s%s%s", 
			adcsrc ==  WM9712_ADCSEL_COMP1 ? " -adc value is COMP1/AUX1\n" : "",
			adcsrc ==  WM9712_ADCSEL_COMP2 ? " -adc value is COMP2/AUX2\n" : "",
			adcsrc ==  WM9712_ADCSEL_BMON ? " -adc value is BMON\n" : "",
			adcsrc ==  WM9712_ADCSEL_WIPER ? " -adc value is WIPER\n" : "");
		} else {
		len += sprintf (page+len, "%s%s%s%s",
			adcsrc ==  WM9705_ADCSEL_PCBEEP ? " -adc value is PCBEEP\n" : "",
			adcsrc ==  WM9705_ADCSEL_PHONE ? " -adc value is PHONE\n" : "",
			adcsrc ==  WM9705_ADCSEL_BMON ? " -adc value is BMON\n" : "",
			adcsrc ==  WM9705_ADCSEL_AUX ? " -adc value is AUX\n" : "");
		}
		
	/* register dump */
	len += sprintf(page+len, "\nRegisters:\n%s%x\n%s%x\n%s%x\n",
		" -digitiser 1    (0x76) : 0x", dig1,
		" -digitiser 2    (0x78) : 0x", dig2,
		" -digitiser read (0x7a) : 0x", digrd);
		
	/* errors */
	len += sprintf(page+len, "\nErrors:\n%s%d\n%s%d\n",
		" -buffer overruns ", ts->overruns,
		" -coordinate errors ", ts->adc_errs);
		
	return len;
}

#ifdef WM97XX_TS_DEBUG
/* dump all the AC97 register space */
static int wm_debug_read_proc (char *page, char **start, off_t off,
		    int count, int *eof, void *data)
{
	int len = 0, i;
	unsigned long flags;
	wm97xx_ts_t* ts;
	u16 reg[AC97_NUM_REG];

	if ((ts = data) == NULL)
		return -ENODEV;

	spin_lock_irqsave(&ts->lock, flags);
	if (!ts->is_registered) {
		spin_unlock_irqrestore(&ts->lock, flags);
		len += sprintf (page+len, "Not registered\n");
		return len;
	}
	
	for (i=0; i < AC97_NUM_REG; i++) {
		reg[i] = ts->codec->codec_read(ts->codec, i * 2);
	}
	spin_unlock_irqrestore(&ts->lock, flags);
	
	for (i=0; i < AC97_NUM_REG; i++) {
		len += sprintf (page+len, "0x%2.2x : 0x%4.4x\n",i * 2, reg[i]);
	}
		
	return len;
}
#endif

#endif

#ifdef CONFIG_PM
/* WM97xx Power Management
 * The WM9712 has extra powerdown states that are controlled in 
 * seperate registers from the AC97 power management.
 * We will only power down into the extra WM9712 states and leave 
 * the AC97 power management to the sound driver.
 */
static int wm97xx_pm_event(struct pm_dev *dev, pm_request_t rqst, void *data)
{
	switch(rqst) {
		case PM_SUSPEND:
			wm97xx_suspend();
			break;
		case PM_RESUME:
			wm97xx_resume();
			break;
	}
	return 0;
}

/*
 * Power down the codec
 */
static void wm97xx_suspend(void)
{
	wm97xx_ts_t* ts = &wm97xx_ts;
	u16 reg;
	unsigned long flags;
	
	/* are we registered */
	spin_lock_irqsave(&ts->lock, flags);
	if (!ts->is_registered) {
		spin_unlock_irqrestore(&ts->lock, flags);
		return;
	}
	
	/* wm9705 does not have extra PM */
	if (!ts->is_wm9712) {
		spin_unlock_irqrestore(&ts->lock, flags);
		return;
	}
	
	/* save and mute the PGA's */
	wm9712_pga_save(ts);
	
	reg = ts->codec->codec_read(ts->codec, AC97_PHONE_VOL);
	ts->codec->codec_write(ts->codec, AC97_PHONE_VOL, reg | 0x001f);
	
	reg = ts->codec->codec_read(ts->codec, AC97_MIC_VOL);
	ts->codec->codec_write(ts->codec, AC97_MIC_VOL, reg | 0x1f1f);
	
	reg = ts->codec->codec_read(ts->codec, AC97_LINEIN_VOL);
	ts->codec->codec_write(ts->codec, AC97_LINEIN_VOL, reg | 0x1f1f);
	
	/* power down, dont disable the AC link */
	ts->codec->codec_write(ts->codec, AC97_WM9712_POWER, WM9712_PD(14) | WM9712_PD(13) |
							WM9712_PD(12) | WM9712_PD(11) | WM9712_PD(10) |                    
							WM9712_PD(9) | WM9712_PD(8) | WM9712_PD(7) |
							WM9712_PD(6) | WM9712_PD(5) | WM9712_PD(4) |
							WM9712_PD(3) | WM9712_PD(2) | WM9712_PD(1) |
							WM9712_PD(0));
	
	spin_unlock_irqrestore(&ts->lock, flags);
}

/*
 * Power up the Codec
 */
static void wm97xx_resume(void)
{
	wm97xx_ts_t* ts = &wm97xx_ts;
	unsigned long flags;
	
	/* are we registered */
	spin_lock_irqsave(&ts->lock, flags);
	if (!ts->is_registered) {
		spin_unlock_irqrestore(&ts->lock, flags);
		return;
	}
	
	/* wm9705 does not have extra PM */
	if (!ts->is_wm9712) {
		spin_unlock_irqrestore(&ts->lock, flags);
		return;
	}

	/* power up */
	ts->codec->codec_write(ts->codec, AC97_WM9712_POWER, 0x0);
	
	/* restore PGA state */
	wm9712_pga_restore(ts);
	
	spin_unlock_irqrestore(&ts->lock, flags);
}


/* save state of wm9712 PGA's */
static void wm9712_pga_save(wm97xx_ts_t* ts)
{
	ts->phone_pga = ts->codec->codec_read(ts->codec, AC97_PHONE_VOL) & 0x001f;
	ts->line_pgal = ts->codec->codec_read(ts->codec, AC97_LINEIN_VOL) & 0x1f00;
	ts->line_pgar = ts->codec->codec_read(ts->codec, AC97_LINEIN_VOL) & 0x001f;
	ts->mic_pgal = ts->codec->codec_read(ts->codec, AC97_MIC_VOL) & 0x1f00;
	ts->mic_pgar = ts->codec->codec_read(ts->codec, AC97_MIC_VOL) & 0x001f;
}

/* restore state of wm9712 PGA's */
static void wm9712_pga_restore(wm97xx_ts_t* ts)
{
	u16 reg;
	
	reg = ts->codec->codec_read(ts->codec, AC97_PHONE_VOL);
	ts->codec->codec_write(ts->codec, AC97_PHONE_VOL, reg | ts->phone_pga);
	
	reg = ts->codec->codec_read(ts->codec, AC97_LINEIN_VOL);
	ts->codec->codec_write(ts->codec, AC97_LINEIN_VOL, reg | ts->line_pgar | (ts->line_pgal << 8));

	reg = ts->codec->codec_read(ts->codec, AC97_MIC_VOL);
	ts->codec->codec_write(ts->codec, AC97_MIC_VOL, reg | ts->mic_pgar | (ts->mic_pgal << 8));
}

#endif

/*
 * set up the physical settings of the device 
 */

static void init_wm97xx_phy(void)
{
	u16 dig1, dig2, aux, vid;
	wm97xx_ts_t *ts = &wm97xx_ts;

	/* default values */
	dig1 = WM97XX_DELAY(4) | WM97XX_SLT(6);
	if (ts->is_wm9712)
		dig2 = WM9712_RPU(1);
	else {
		dig2 = 0x0;
		
		/* 
		 * mute VIDEO and AUX as they share X and Y touchscreen 
		 * inputs on the WM9705 
		 */
		aux = ts->codec->codec_read(ts->codec, AC97_AUX_VOL);
		if (!(aux & 0x8000)) {
			info("muting AUX mixer as it shares X touchscreen coordinate");
			ts->codec->codec_write(ts->codec, AC97_AUX_VOL, 0x8000 | aux);
		}
		
		vid = ts->codec->codec_read(ts->codec, AC97_VIDEO_VOL);
		if (!(vid & 0x8000)) {
			info("muting VIDEO mixer as it shares Y touchscreen coordinate");
			ts->codec->codec_write(ts->codec, AC97_VIDEO_VOL, 0x8000 | vid);
		}
	}
	
	/* WM9712 rpu */
	if (ts->is_wm9712 && rpu) {
		dig2 &= 0xffc0;
		dig2 |= WM9712_RPU(rpu);
		info("setting pen detect pull-up to %d Ohms",64000 / rpu);
	}
	
	/* touchpanel pressure */
	if  (pil == 2) {
		if (ts->is_wm9712)
			dig2 |= WM9712_PIL;
		else
			dig2 |= WM9705_PIL;
		info("setting pressure measurement current to 400uA.");
	} else if (pil) 
		info ("setting pressure measurement current to 200uA.");
	
	/* WM9712 five wire */
	if (ts->is_wm9712 && five_wire) {
		dig2 |= WM9712_45W;
		info("setting 5-wire touchscreen mode.");
	}		
	
	/* sample settling delay */
	if (delay!=4) {
		if (delay < 0 || delay > 15) {
			info ("supplied delay out of range.");
			delay = 4;
		}
		dig1 &= 0xff0f;
		dig1 |= WM97XX_DELAY(delay);
		info("setting adc sample delay to %d u Secs.", delay_table[delay]);
	}
	
	/* coordinate mode */
	if (mode == 1) {
		dig1 |= WM97XX_COO;
		info("using coordinate mode");
	}		
	
	/* WM9705 pdd */
	if (pdd && !ts->is_wm9712) {
		dig2 |= (pdd & 0x000f);
		info("setting pdd to Vmid/%d", 1 - (pdd & 0x000f));
	}
	
	ts->codec->codec_write(ts->codec, AC97_WM97XX_DIGITISER1, dig1);
	ts->codec->codec_write(ts->codec, AC97_WM97XX_DIGITISER2, dig2); 
}


/*
 * Called by the audio codec initialisation to register
 * the touchscreen driver.
 */

static int wm97xx_probe(struct ac97_codec *codec, struct ac97_driver *driver)
{
	 unsigned long flags;
	u16 id1, id2;
	wm97xx_ts_t *ts = &wm97xx_ts;
		
	spin_lock_irqsave(&ts->lock, flags);
	
	/* we only support 1 touchscreen at the moment */
	if (ts->is_registered) {
		spin_unlock_irqrestore(&ts->lock, flags);
		return -1;
	}
	
	/* 
	 * We can only use a WM9705 or WM9712 that has been *first* initialised
	 * by the AC97 audio driver. This is because we have to use the audio 
	 * drivers codec read() and write() functions to sample the touchscreen	
	 *	
	 * If an initialsed WM97xx is found then get the codec read and write 
	 * functions.		 
	 */
	
	/* test for a WM9712 or a WM9705 */
	id1 = codec->codec_read(codec, AC97_VENDOR_ID1);
	id2 = codec->codec_read(codec, AC97_VENDOR_ID2);
	if (id1 == WM97XX_ID1 && id2 == WM9712_ID2) {
		ts->is_wm9712 = 1;
		info("registered a WM9712");
	} else if (id1 == WM97XX_ID1 && id2 == WM9705_ID2) {
		    ts->is_wm9712 = 0;
		    info("registered a WM9705");
	} else {
		err("could not find a WM97xx codec. Found a 0x%4x:0x%4x instead",
		    id1, id2);
		spin_unlock_irqrestore(&ts->lock, flags);
		return -1;
	}
	
	/* set up AC97 codec interface */
	ts->codec = codec;
	codec->driver_private = (void*)&ts;
	codec->codec_unregister = 0;
	
	/* set up physical characteristics */
	init_wm97xx_phy();
		
	ts->is_registered = 1;
	spin_unlock_irqrestore(&ts->lock, flags);
	return 0;
}

/* this is called by the audio driver when ac97_codec is unloaded */

static void wm97xx_remove(struct ac97_codec *codec, struct ac97_driver *driver)
{
	unsigned long flags;
	u16 dig1, dig2;
	wm97xx_ts_t *ts = codec->driver_private;
	
	spin_lock_irqsave(&ts->lock, flags);
			
	/* check that are registered */
	if (!ts->is_registered) {
		err("double unregister");
		spin_unlock_irqrestore(&ts->lock, flags);
		return;
	}
	
	ts->is_registered = 0;
	wake_up_interruptible(&ts->wait); /* So we see its gone */
	
	/* restore default digitiser values */
	dig1 = WM97XX_DELAY(4) | WM97XX_SLT(6);
	if (ts->is_wm9712)
		dig2 = WM9712_RPU(1);
	else 
		dig2 = 0x0;
		
	codec->codec_write(codec, AC97_WM97XX_DIGITISER1, dig1);
	codec->codec_write(codec, AC97_WM97XX_DIGITISER2, dig2); 
	ts->codec = NULL;
		
	spin_unlock_irqrestore(&ts->lock, flags);
}

static struct miscdevice wm97xx_misc = { 
	minor:	TS_MINOR,
	name:	"touchscreen/wm97xx",
	fops:	&ts_fops,
};

static int __init wm97xx_ts_init_module(void)
{
	wm97xx_ts_t* ts = &wm97xx_ts;
	int ret;
	char proc_str[64];
	
	info("Wolfson WM9705/WM9712 Touchscreen Controller");
	info("Version %s  liam.girdwood@wolfsonmicro.com", WM_TS_VERSION);
	
	memset(ts, 0, sizeof(wm97xx_ts_t));
	
	/* register our misc device */
	if ((ret = misc_register(&wm97xx_misc)) < 0) {
		err("can't register misc device");
		return ret;
	}
	
	init_waitqueue_head(&ts->wait);
	spin_lock_init(&ts->lock);
	
	// initial calibration values
	ts->cal.xscale = 256;
	ts->cal.xtrans = 0;
	ts->cal.yscale = 256;
	ts->cal.ytrans = 0;
	
	/* reset error counters */
	ts->overruns = 0;
	ts->adc_errs = 0;
	
	/* register with the AC97 layer */
	ac97_register_driver(&wm9705_driver);
	ac97_register_driver(&wm9712_driver);
	
#ifdef CONFIG_PROC_FS
	/* register proc interface */
	sprintf(proc_str, "driver/%s", TS_NAME);
	if ((ts->wm97xx_ts_ps = create_proc_read_entry (proc_str, 0, NULL,
					     wm97xx_read_proc, ts)) == 0)
		err("could not register proc interface /proc/%s", proc_str);
#ifdef WM97XX_TS_DEBUG
	if ((ts->wm97xx_debug_ts_ps = create_proc_read_entry ("driver/ac97_registers",
		0, NULL,wm_debug_read_proc, ts)) == 0)
		err("could not register proc interface /proc/driver/ac97_registers");
#endif
#endif
#ifdef CONFIG_PM
	if ((ts->pm = pm_register(PM_UNKNOWN_DEV, PM_SYS_UNKNOWN, wm97xx_pm_event)) == 0)
		err("could not register with power management");
#endif
	return 0;
}

static void wm97xx_ts_cleanup_module(void)
{
	wm97xx_ts_t* ts = &wm97xx_ts;

#ifdef CONFIG_PM
	pm_unregister (ts->pm);
#endif
	ac97_unregister_driver(&wm9705_driver);
	ac97_unregister_driver(&wm9712_driver);
	misc_deregister(&wm97xx_misc);
}

/* Module information */
MODULE_AUTHOR("Liam Girdwood, liam.girdwood@wolfsonmicro.com, www.wolfsonmicro.com");
MODULE_DESCRIPTION("WM9705/WM9712 Touch Screen / BMON Driver");
MODULE_LICENSE("GPL");

module_init(wm97xx_ts_init_module);
module_exit(wm97xx_ts_cleanup_module);

#ifndef MODULE

static int __init wm97xx_ts_setup(char *options)
{
	char *this_opt = options;

	if (!options || !*options)
		return 0;

	/* parse the options and check for out of range values */
	for(this_opt=strtok(options, ",");
	    this_opt; this_opt=strtok(NULL, ",")) {
		if (!strncmp(this_opt, "pil:", 4)) {
			this_opt+=4;
			pil = simple_strtol(this_opt, NULL, 0);
			if (pil < 0 || pil > 2)
				pil = 0;
			continue;
		}
		if (!strncmp(this_opt, "rpu:", 4)) {
			this_opt+=4;
			rpu = simple_strtol(this_opt, NULL, 0);
			if (rpu < 0 || rpu > 31)
				rpu = 0;
			continue;
		}
		if (!strncmp(this_opt, "pdd:", 4)) {
			this_opt+=4;
			pdd = simple_strtol(this_opt, NULL, 0);
			if (pdd < 0 || pdd > 15)
				pdd = 0;
			continue;
		}
		if (!strncmp(this_opt, "delay:", 6)) {
			this_opt+=6;
			delay = simple_strtol(this_opt, NULL, 0);
			if (delay < 0 || delay > 15)
				delay = 4;
			continue;
		}
		if (!strncmp(this_opt, "five_wire:", 10)) {
			this_opt+=10;
			five_wire = simple_strtol(this_opt, NULL, 0);
			if (five_wire < 0 || five_wire > 1)
				five_wire = 0;
			continue;
		}
		if (!strncmp(this_opt, "mode:", 5)) {
			this_opt+=5;
			mode = simple_strtol(this_opt, NULL, 0);
			if (mode < 0 || mode > 2)
				mode = 0;
			continue;
		}
	}
	return 1;
}

__setup("wm97xx_ts=", wm97xx_ts_setup);

#endif /* MODULE */
