/*
 *      au1000_ts.c  --  Touch screen driver for the Alchemy Au1000's
 *                       SSI Port 0 talking to the ADS7846 touch screen
 *                       controller.
 *
 * Copyright 2001 MontaVista Software Inc.
 * Author: MontaVista Software, Inc.
 *         	stevel@mvista.com or source@mvista.com
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
 *  Revision history
 *    06.27.2001  Initial version
 */

#include <linux/module.h>
#include <linux/version.h>

#include <linux/init.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/poll.h>
#include <linux/string.h>
#include <linux/ioport.h>       /* request_region */
#include <linux/interrupt.h>    /* mark_bh */
#include <asm/uaccess.h>        /* get_user,copy_to_user */
#include <asm/io.h>
#include <asm/au1000.h>

#define TS_NAME "au1000-ts"
#define TS_MAJOR 11

#define PFX TS_NAME
#define AU1000_TS_DEBUG 1

#ifdef AU1000_TS_DEBUG
#define dbg(format, arg...) printk(KERN_DEBUG PFX ": " format "\n" , ## arg)
#else
#define dbg(format, arg...) do {} while (0)
#endif
#define err(format, arg...) printk(KERN_ERR PFX ": " format "\n" , ## arg)
#define info(format, arg...) printk(KERN_INFO PFX ": " format "\n" , ## arg)
#define warn(format, arg...) printk(KERN_WARNING PFX ": " format "\n" , ## arg)


// SSI Status register bit defines
#define SSISTAT_BF    (1<<4)
#define SSISTAT_OF    (1<<3)
#define SSISTAT_UF    (1<<2)
#define SSISTAT_DONE  (1<<1)
#define SSISTAT_BUSY  (1<<0)

// SSI Interrupt Pending and Enable register bit defines
#define SSIINT_OI     (1<<3)
#define SSIINT_UI     (1<<2)
#define SSIINT_DI     (1<<1)

// SSI Address/Data register bit defines
#define SSIADAT_D         (1<<24)
#define SSIADAT_ADDR_BIT  16
#define SSIADAT_ADDR_MASK (0xff<<SSIADAT_ADDR_BIT)
#define SSIADAT_DATA_BIT  0
#define SSIADAT_DATA_MASK (0xfff<<SSIADAT_DATA_BIT)

// SSI Enable register bit defines
#define SSIEN_CD (1<<1)
#define SSIEN_E  (1<<0)

// SSI Config register bit defines
#define SSICFG_AO (1<<24)
#define SSICFG_DO (1<<23)
#define SSICFG_ALEN_BIT 20
#define SSICFG_ALEN_MASK (0x7<<SSICFG_ALEN_BIT)
#define SSICFG_DLEN_BIT 16
#define SSICFG_DLEN_MASK (0xf<<SSICFG_DLEN_BIT)
#define SSICFG_DD (1<<11)
#define SSICFG_AD (1<<10)
#define SSICFG_BM_BIT 8
#define SSICFG_BM_MASK (0x3<<SSICFG_BM_BIT)
#define SSICFG_CE (1<<7)
#define SSICFG_DP (1<<6)
#define SSICFG_DL (1<<5)
#define SSICFG_EP (1<<4)

// Bus Turnaround Selection
#define SCLK_HOLD_HIGH 0
#define SCLK_HOLD_LOW  1
#define SCLK_CYCLE     2

/*
 * Default config for SSI0:
 *
 *   - transmit MSBit first
 *   - expect MSBit first on data receive
 *   - address length 7 bits
 *   - expect data length 12 bits
 *   - do not disable Direction bit
 *   - do not disable Address bits
 *   - SCLK held low during bus turnaround
 *   - Address and Data bits clocked out on falling edge of SCLK
 *   - Direction bit high is a read, low is a write
 *   - Direction bit precedes Address bits
 *   - Active low enable signal
 */

#define DEFAULT_SSI_CONFIG \
    (SSICFG_AO | SSICFG_DO | (6<<SSICFG_ALEN_BIT) | (11<<SSICFG_DLEN_BIT) |\
    (SCLK_HOLD_LOW<<SSICFG_BM_BIT) | SSICFG_DP | SSICFG_EP)


// ADS7846 Control Byte bit defines
#define ADS7846_ADDR_BIT  4
#define ADS7846_ADDR_MASK (0x7<<ADS7846_ADDR_BIT)
#define   ADS7846_MEASURE_X  (0x5<<ADS7846_ADDR_BIT)
#define   ADS7846_MEASURE_Y  (0x1<<ADS7846_ADDR_BIT)
#define   ADS7846_MEASURE_Z1 (0x3<<ADS7846_ADDR_BIT)
#define   ADS7846_MEASURE_Z2 (0x4<<ADS7846_ADDR_BIT)
#define ADS7846_8BITS     (1<<3)
#define ADS7846_12BITS    0
#define ADS7846_SER       (1<<2)
#define ADS7846_DFR       0
#define ADS7846_PWR_BIT   0
#define   ADS7846_PD      0
#define   ADS7846_ADC_ON  (0x1<<ADS7846_PWR_BIT)
#define   ADS7846_REF_ON  (0x2<<ADS7846_PWR_BIT)
#define   ADS7846_REF_ADC_ON (0x3<<ADS7846_PWR_BIT)

#define MEASURE_12BIT_X \
    (ADS7846_MEASURE_X | ADS7846_12BITS | ADS7846_DFR | ADS7846_PD)
#define MEASURE_12BIT_Y \
    (ADS7846_MEASURE_Y | ADS7846_12BITS | ADS7846_DFR | ADS7846_PD)
#define MEASURE_12BIT_Z1 \
    (ADS7846_MEASURE_Z1 | ADS7846_12BITS | ADS7846_DFR | ADS7846_PD)
#define MEASURE_12BIT_Z2 \
    (ADS7846_MEASURE_Z2 | ADS7846_12BITS | ADS7846_DFR | ADS7846_PD)

typedef enum {
	IDLE = 0,
	ACQ_X,
	ACQ_Y,
	ACQ_Z1,
	ACQ_Z2
} acq_state_t;

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


#define EVENT_BUFSIZE 128

/*
 * Which pressure equation to use from ADS7846 datasheet.
 * The first equation requires knowing only the X plate
 * resistance, but needs 4 measurements (X, Y, Z1, Z2).
 * The second equation requires knowing both X and Y plate
 * resistance, but only needs 3 measurements (X, Y, Z1).
 * The second equation is preferred because of the shorter
 * acquisition time required.
 */
enum {
	PRESSURE_EQN_1 = 0,
	PRESSURE_EQN_2
};


/*
 * The touch screen's X and Y plate resistances, used by
 * pressure equations.
 */
#define DEFAULT_X_PLATE_OHMS 580
#define DEFAULT_Y_PLATE_OHMS 580

/*
 * Pen up/down pressure resistance thresholds.
 *
 * FIXME: these are bogus and will have to be found empirically.
 *
 * These are hysteresis points. If pen state is up and pressure
 * is greater than pen-down threshold, pen transitions to down.
 * If pen state is down and pressure is less than pen-up threshold,
 * pen transitions to up. If pressure is in-between, pen status
 * doesn't change.
 *
 * This wouldn't be needed if PENIRQ* from the ADS7846 were
 * routed to an interrupt line on the Au1000. This would issue
 * an interrupt when the panel is touched.
 */
#define DEFAULT_PENDOWN_THRESH_OHMS 100
#define DEFAULT_PENUP_THRESH_OHMS    80

typedef struct {
	int baudrate;
	u32 clkdiv;
	acq_state_t acq_state;            // State of acquisition state machine
	int x_raw, y_raw, z1_raw, z2_raw; // The current raw acquisition values
	TS_CAL cal;                       // Calibration values
	// The X and Y plate resistance, needed to calculate pressure
	int x_plate_ohms, y_plate_ohms;
	// pressure resistance at which pen is considered down/up
	int pendown_thresh_ohms;
	int penup_thresh_ohms;
	int pressure_eqn;                 // eqn to use for pressure calc
	int pendown;                      // 1 = pen is down, 0 = pen is up
	TS_EVENT event_buf[EVENT_BUFSIZE];// The event queue
	int nextIn, nextOut;
	int event_count;
	struct fasync_struct *fasync;     // asynch notification
	struct timer_list acq_timer;      // Timer for triggering acquisitions
	wait_queue_head_t wait;           // read wait queue
	spinlock_t lock;
	struct tq_struct chug_tq;
} au1000_ts_t;

static au1000_ts_t au1000_ts;


static inline u32
calc_clkdiv(int baud)
{
	u32 sys_busclk =
		(get_au1x00_speed() / (int)(inl(SYS_POWERCTRL)&0x03) + 2);
	return (sys_busclk / (2 * baud)) - 1;
}

static inline int
calc_baudrate(u32 clkdiv)
{
	u32 sys_busclk =
		(get_au1x00_speed() / (int)(inl(SYS_POWERCTRL)&0x03) + 2);
	return sys_busclk / (2 * (clkdiv + 1));
}


/*
 * This is a bottom-half handler that is scheduled after
 * raw X,Y,Z1,Z2 coordinates have been acquired, and does
 * the following:
 *
 *   - computes touch screen pressure resistance
 *   - if pressure is above a threshold considered to be pen-down:
 *         - compute calibrated X and Y coordinates
 *         - queue a new TS_EVENT
 *         - signal asynchronously and wake up any read
 */
static void
chug_raw_data(void* private)
{
	au1000_ts_t* ts = (au1000_ts_t*)private;
	TS_EVENT event;
	int Rt, Xcal, Ycal;
	unsigned long flags;

	// timestamp this new event.
	event.millisecs = jiffies;

	// Calculate touch pressure resistance
	if (ts->pressure_eqn == PRESSURE_EQN_2) {
		Rt = (ts->x_plate_ohms * ts->x_raw *
		      (4096 - ts->z1_raw)) / ts->z1_raw;
		Rt -= (ts->y_plate_ohms * ts->y_raw);
		Rt = (Rt + 2048) >> 12; // round up to nearest ohm
	} else {
		Rt = (ts->x_plate_ohms * ts->x_raw *
		      (ts->z2_raw - ts->z1_raw)) / ts->z1_raw;
		Rt = (Rt + 2048) >> 12; // round up to nearest ohm
	}

	// hysteresis
	if (!ts->pendown && Rt > ts->pendown_thresh_ohms)
		ts->pendown = 1;
	else if (ts->pendown && Rt < ts->penup_thresh_ohms)
		ts->pendown = 0;

	if (ts->pendown) {
		// Pen is down
		// Calculate calibrated X,Y
		Xcal = ((ts->cal.xscale * ts->x_raw) >> 8) + ts->cal.xtrans;
		Ycal = ((ts->cal.yscale * ts->y_raw) >> 8) + ts->cal.ytrans;

		event.x = (unsigned short)Xcal;
		event.y = (unsigned short)Ycal;
		event.pressure = (unsigned short)Rt;

		// add this event to the event queue
		spin_lock_irqsave(&ts->lock, flags);
		ts->event_buf[ts->nextIn++] = event;
		if (ts->nextIn == EVENT_BUFSIZE)
			ts->nextIn = 0;
		if (ts->event_count < EVENT_BUFSIZE) {
			ts->event_count++;
		} else {
			// throw out the oldest event
			if (++ts->nextOut == EVENT_BUFSIZE)
				ts->nextOut = 0;
		}
		spin_unlock_irqrestore(&ts->lock, flags);

		// async notify
		if (ts->fasync)
			kill_fasync(&ts->fasync, SIGIO, POLL_IN);
		// wake up any read call
		if (waitqueue_active(&ts->wait))
			wake_up_interruptible(&ts->wait);
	}
}


/*
 * Raw X,Y,pressure acquisition timer function. This triggers
 * the start of a new acquisition. Its duration between calls
 * is the touch screen polling rate.
 */
static void
au1000_acq_timer(unsigned long data)
{
	au1000_ts_t* ts = (au1000_ts_t*)data;
	unsigned long flags;

	spin_lock_irqsave(&ts->lock, flags);

	// start acquisition with X coordinate
	ts->acq_state = ACQ_X;
	// start me up
	outl(SSIADAT_D | (MEASURE_12BIT_X << SSIADAT_ADDR_BIT), SSI0_ADATA);

	// schedule next acquire
	ts->acq_timer.expires = jiffies + HZ / 100;
	add_timer(&ts->acq_timer);

	spin_unlock_irqrestore(&ts->lock, flags);
}

static void
ssi0_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	au1000_ts_t *ts = (au1000_ts_t*)dev_id;
	u32 stat, int_stat, data;

	spin_lock(&ts->lock);

	stat = inl(SSI0_STATUS);
	// clear sticky status bits
	outl(stat & (SSISTAT_OF|SSISTAT_UF|SSISTAT_DONE), SSI0_STATUS);

	int_stat = inl(SSI0_INT);
	// clear sticky intr status bits
	outl(int_stat & (SSIINT_OI|SSIINT_UI|SSIINT_DI), SSI0_INT);

	if ((int_stat & (SSIINT_OI|SSIINT_UI|SSIINT_DI)) != SSIINT_DI) {
		if (int_stat & SSIINT_OI)
			err("overflow");
		if (int_stat & SSIINT_UI)
			err("underflow");
		spin_unlock(&ts->lock);
		return;
	}

	data = inl(SSI0_ADATA) & SSIADAT_DATA_MASK;

	switch (ts->acq_state) {
	case IDLE:
		break;
	case ACQ_X:
		ts->x_raw = data;
		ts->acq_state = ACQ_Y;
		// trigger Y acq
		outl(SSIADAT_D | (MEASURE_12BIT_Y << SSIADAT_ADDR_BIT),
		     SSI0_ADATA);
		break;
	case ACQ_Y:
		ts->y_raw = data;
		ts->acq_state = ACQ_Z1;
		// trigger Z1 acq
		outl(SSIADAT_D | (MEASURE_12BIT_Z1 << SSIADAT_ADDR_BIT),
		     SSI0_ADATA);
		break;
	case ACQ_Z1:
		ts->z1_raw = data;
		if (ts->pressure_eqn == PRESSURE_EQN_2) {
			// don't acq Z2, using 2nd eqn for touch pressure
			ts->acq_state = IDLE;
			// got the raw stuff, now mark BH
			queue_task(&ts->chug_tq, &tq_immediate);
			mark_bh(IMMEDIATE_BH);
		} else {
			ts->acq_state = ACQ_Z2;
			// trigger Z2 acq
			outl(SSIADAT_D | (MEASURE_12BIT_Z2<<SSIADAT_ADDR_BIT),
			     SSI0_ADATA);
		}
		break;
	case ACQ_Z2:
		ts->z2_raw = data;
		ts->acq_state = IDLE;
		// got the raw stuff, now mark BH
		queue_task(&ts->chug_tq, &tq_immediate);
		mark_bh(IMMEDIATE_BH);
		break;
	}

	spin_unlock(&ts->lock);
}


/* +++++++++++++ File operations ++++++++++++++*/

static int
au1000_fasync(int fd, struct file *filp, int mode)
{
	au1000_ts_t* ts = (au1000_ts_t*)filp->private_data;
	return fasync_helper(fd, filp, mode, &ts->fasync);
}

static int
au1000_ioctl(struct inode * inode, struct file *filp,
	     unsigned int cmd, unsigned long arg)
{
	au1000_ts_t* ts = (au1000_ts_t*)filp->private_data;

	switch(cmd) {
	case TS_GET_RATE:       /* TODO: what is this? */
		break;
	case TS_SET_RATE:       /* TODO: what is this? */
		break;
	case TS_GET_CAL:
		copy_to_user((char *)arg, (char *)&ts->cal, sizeof(TS_CAL));
		break;
	case TS_SET_CAL:
		copy_from_user((char *)&ts->cal, (char *)arg, sizeof(TS_CAL));
		break;
	default:
		err("unknown cmd %04x", cmd);
		return -EINVAL;
	}

	return 0;
}

static unsigned int
au1000_poll(struct file * filp, poll_table * wait)
{
	au1000_ts_t* ts = (au1000_ts_t*)filp->private_data;
	poll_wait(filp, &ts->wait, wait);
	if (ts->event_count)
		return POLLIN | POLLRDNORM;
	return 0;
}

static ssize_t
au1000_read(struct file * filp, char * buf, size_t count, loff_t * l)
{
	au1000_ts_t* ts = (au1000_ts_t*)filp->private_data;
	unsigned long flags;
	TS_EVENT event;
	int i;

	if (ts->event_count == 0) {
		if (filp->f_flags & O_NONBLOCK)
			return -EAGAIN;
		interruptible_sleep_on(&ts->wait);
		if (signal_pending(current))
			return -ERESTARTSYS;
	}

	for (i = count;
	     i >= sizeof(TS_EVENT);
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
		copy_to_user(buf, &event, sizeof(TS_EVENT));
	}

	return count - i;
}


static int
au1000_open(struct inode * inode, struct file * filp)
{
	au1000_ts_t* ts;
	unsigned long flags;

	filp->private_data = ts = &au1000_ts;

	spin_lock_irqsave(&ts->lock, flags);

	// setup SSI0 config
	outl(DEFAULT_SSI_CONFIG, SSI0_CONFIG);

	// clear out SSI0 status bits
	outl(SSISTAT_OF|SSISTAT_UF|SSISTAT_DONE, SSI0_STATUS);
	// clear out SSI0 interrupt pending bits
	outl(SSIINT_OI|SSIINT_UI|SSIINT_DI, SSI0_INT);

	// enable SSI0 interrupts
	outl(SSIINT_OI|SSIINT_UI|SSIINT_DI, SSI0_INT_ENABLE);

	/*
	 * init bh handler that chugs the raw data (calibrates and
	 * calculates touch pressure).
	 */
	ts->chug_tq.routine = chug_raw_data;
	ts->chug_tq.data = ts;
	ts->pendown = 0; // pen up
	
	// flush event queue
	ts->nextIn = ts->nextOut = ts->event_count = 0;
	
	// Start acquisition timer function
	init_timer(&ts->acq_timer);
	ts->acq_timer.function = au1000_acq_timer;
	ts->acq_timer.data = (unsigned long)ts;
	ts->acq_timer.expires = jiffies + HZ / 100;
	add_timer(&ts->acq_timer);

	spin_unlock_irqrestore(&ts->lock, flags);
	MOD_INC_USE_COUNT;
	return 0;
}

static int
au1000_release(struct inode * inode, struct file * filp)
{
	au1000_ts_t* ts = (au1000_ts_t*)filp->private_data;
	unsigned long flags;
	
	au1000_fasync(-1, filp, 0);
	del_timer_sync(&ts->acq_timer);

	spin_lock_irqsave(&ts->lock, flags);
	// disable SSI0 interrupts
	outl(0, SSI0_INT_ENABLE);
	spin_unlock_irqrestore(&ts->lock, flags);

	MOD_DEC_USE_COUNT;
	return 0;
}


static struct file_operations ts_fops = {
	read:           au1000_read,
	poll:           au1000_poll,
	ioctl:		au1000_ioctl,
	fasync:         au1000_fasync,
	open:		au1000_open,
	release:	au1000_release,
};

/* +++++++++++++ End File operations ++++++++++++++*/


int __init
au1000ts_init_module(void)
{
	au1000_ts_t* ts = &au1000_ts;
	int ret;

	/* register our character device */
	if ((ret = register_chrdev(TS_MAJOR, TS_NAME, &ts_fops)) < 0) {
		err("can't get major number");
		return ret;
	}
	info("registered");

	memset(ts, 0, sizeof(au1000_ts_t));
	init_waitqueue_head(&ts->wait);
	spin_lock_init(&ts->lock);

	if (!request_region(virt_to_phys((void*)SSI0_STATUS), 0x100, TS_NAME)) {
		err("SSI0 ports in use");
		return -ENXIO;
	}

	if ((ret = request_irq(AU1000_SSI0_INT, ssi0_interrupt,
			       SA_SHIRQ | SA_INTERRUPT, TS_NAME, ts))) {
		err("could not get IRQ");
		return ret;
	}

	// initial calibration values
	ts->cal.xscale = -93;
	ts->cal.xtrans = 346;
	ts->cal.yscale = -64;
	ts->cal.ytrans = 251;

	// init pen up/down hysteresis points
	ts->pendown_thresh_ohms = DEFAULT_PENDOWN_THRESH_OHMS;
	ts->penup_thresh_ohms = DEFAULT_PENUP_THRESH_OHMS;
	ts->pressure_eqn = PRESSURE_EQN_2;
	// init X and Y plate resistances
	ts->x_plate_ohms = DEFAULT_X_PLATE_OHMS;
	ts->y_plate_ohms = DEFAULT_Y_PLATE_OHMS;

	// set GPIO to SSI0 function
	outl(inl(SYS_PINFUNC) & ~1, SYS_PINFUNC);
	
	// enable SSI0 clock and bring SSI0 out of reset
	outl(0, SSI0_CONTROL);
	udelay(1000);
	outl(SSIEN_E, SSI0_CONTROL);
	udelay(100);
	
	// FIXME: is this a working baudrate?
	ts->clkdiv = 0;
	ts->baudrate = calc_baudrate(ts->clkdiv);
	outl(ts->clkdiv, SSI0_CLKDIV);

	info("baudrate = %d Hz", ts->baudrate);
	
	return 0;
}

void
au1000ts_cleanup_module(void)
{
	// disable clocks and hold in reset
	outl(SSIEN_CD, SSI0_CONTROL);
	free_irq(AU1000_SSI0_INT, &au1000_ts);
	release_region(virt_to_phys((void*)SSI0_STATUS), 0x100);
	unregister_chrdev(TS_MAJOR, TS_NAME);
}

/* Module information */
MODULE_AUTHOR("Steve Longerbeam, stevel@mvista.com, www.mvista.com");
MODULE_DESCRIPTION("Au1000/ADS7846 Touch Screen Driver");

module_init(au1000ts_init_module);
module_exit(au1000ts_cleanup_module);
