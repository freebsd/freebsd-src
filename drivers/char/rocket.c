/*
 * Rocketport device driver for Linux
 *
 * Written by Theodore Ts'o, 1995, 1996, 1997.
 * 
 * Copyright (C) 1995, 1996, 1997 by Comtrol, Inc.
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*
 * Minor number schema:
 *
 * +-------------------------------+
 * | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
 * +---+-------+-------+-----------+
 * | C | Board |  AIOP | Port #    |
 * +---+-------+-------+-----------+
 *
 * C=0 implements normal POSIX tty.
 * C=1 is reserved for the callout device.
 * 
 * Normally, the user won't have to worry about the AIOP; as far as
 * the user is concerned, the lower 5 bits of the minor number address
 * the ports on a particular board (from 0 up to 32).
 */

/* Kernel includes */

#include <linux/config.h>
#include <linux/version.h>

#ifdef CONFIG_PCI
#define ENABLE_PCI
#endif

#include <linux/module.h>
#include <linux/errno.h>
#include <linux/major.h>
#include <linux/kernel.h>
#include <linux/signal.h>
#include <linux/slab.h>
#include <linux/mm.h>

#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/string.h>
#include <linux/fcntl.h>
#include <linux/ptrace.h>
#include <linux/ioport.h>
#ifdef ENABLE_PCI
#include <linux/pci.h>
#if (LINUX_VERSION_CODE < 0x020163) /* 2.1.99 */
#include <linux/bios32.h>
#endif
#endif
#if (LINUX_VERSION_CODE >= 131343) /* 2.1.15 -- XX get correct version */
#include <linux/init.h>
#endif
	
#include "rocket_int.h"
#ifdef LOCAL_ROCKET_H
#include "rocket.h"
#include "version.h"
#else
#include <linux/rocket.h>
#define ROCKET_VERSION "1.14c"
#define ROCKET_DATE "24-Aug-98"
#endif /* LOCAL_ROCKET_H */

#define ROCKET_PARANOIA_CHECK
#define ROCKET_SOFT_FLOW

#undef ROCKET_DEBUG_OPEN
#undef ROCKET_DEBUG_INTR
#undef ROCKET_DEBUG_WRITE
#undef ROCKET_DEBUG_FLOW
#undef ROCKET_DEBUG_THROTTLE
#undef ROCKET_DEBUG_WAIT_UNTIL_SENT
#undef ROCKET_DEBUG_RECEIVE
#undef ROCKET_DEBUG_HANGUP
	

/*   CAUTION!!!!!  The TIME_STAT Function relies on the Pentium 64 bit
 *    register.  For various reasons related to 1.2.13, the test for this
 *    register is omitted from this driver.  If you are going to enable
 *    this option, make sure you are running a Pentium CPU and that a
 *    cat of /proc/cpuinfo shows ability TS Counters as Yes.  Warning part
 *    done, don't cry to me if you enable this options and things won't
 *    work.  If it gives you any problems, then disable the option.  The code
 *    in this function is pretty straight forward, if it breaks on your
 *    CPU, there is probably something funny about your CPU.
 */

#undef TIME_STAT	/* For performing timing statistics on driver. */
			/* Produces printks, one every TIME_COUNTER loops, eats */
			/* some of your CPU time.  Good for testing or */
			/* other checking, otherwise, leave it undefed */
			/* Doug Ledford */
#define TIME_STAT_CPU 100      /* This needs to be set to your processor speed */
                               /* For example, 100Mhz CPU, set this to 100 */
#define TIME_COUNTER 180000    /* This is how many iterations to run before */
			      /* performing the printk statements.   */
			      /* 6000 = 1 minute, 360000 = 1 hour, etc. */
			      /* Since time_stat is long long, this */
			      /* Can be really high if you want :)  */
#undef TIME_STAT_VERBOSE   /* Undef this if you want a terse log message. */

#define _INLINE_ inline

static struct r_port *rp_table[MAX_RP_PORTS];
static struct tty_struct *rocket_table[MAX_RP_PORTS];
static unsigned int xmit_flags[NUM_BOARDS];
static struct termios *rocket_termios[MAX_RP_PORTS];
static struct termios *rocket_termios_locked[MAX_RP_PORTS];
static void rp_wait_until_sent(struct tty_struct *tty, int timeout);
static void rp_flush_buffer(struct tty_struct *tty);

static struct tty_driver rocket_driver, callout_driver;
static int rocket_refcount;

static int rp_num_ports_open;

static struct timer_list rocket_timer;

unsigned long board1;
unsigned long board2;
unsigned long board3;
unsigned long board4;
unsigned long controller;
unsigned long support_low_speed;
int rp_baud_base = 460800;
static unsigned long rcktpt_io_addr[NUM_BOARDS];
static int max_board;
#ifdef TIME_STAT
static unsigned long long time_stat;
static unsigned long time_stat_short;
static unsigned long time_stat_long;
static unsigned long time_counter;
#endif

#if ((LINUX_VERSION_CODE > 0x020111) && defined(MODULE))
MODULE_AUTHOR("Theodore Ts'o");
MODULE_DESCRIPTION("Comtrol Rocketport driver");
MODULE_LICENSE("GPL");
MODULE_PARM(board1,     "i");
MODULE_PARM_DESC(board1, "I/O port for (ISA) board #1");
MODULE_PARM(board2,     "i");
MODULE_PARM_DESC(board2, "I/O port for (ISA) board #2");
MODULE_PARM(board3,     "i");
MODULE_PARM_DESC(board3, "I/O port for (ISA) board #3");
MODULE_PARM(board4,     "i");
MODULE_PARM_DESC(board4, "I/O port for (ISA) board #4");
MODULE_PARM(controller, "i");
MODULE_PARM_DESC(controller, "I/O port for (ISA) rocketport controller");
MODULE_PARM(support_low_speed, "i");
MODULE_PARM_DESC(support_low_speed, "0 means support 50 baud, 1 means support 460400 baud");	
#endif

#if (LINUX_VERSION_CODE < 131336)
int copy_from_user(void *to, const void *from_user, unsigned long len)
{
	int	error;

	error = verify_area(VERIFY_READ, from_user, len);
	if (error)
		return len;
	memcpy_fromfs(to, from_user, len);
	return 0;
}

int copy_to_user(void *to_user, const void *from, unsigned long len)
{
	int	error;
	
	error = verify_area(VERIFY_WRITE, to_user, len);
	if (error)
		return len;
	memcpy_tofs(to_user, from, len);
	return 0;
}

static inline int signal_pending(struct task_struct *p)
{
	return (p->signal & ~p->blocked) != 0;
}

#else
#include <asm/uaccess.h>
#endif

/*
 * tmp_buf is used as a temporary buffer by rp_write.  We need to
 * lock it in case the memcpy_fromfs blocks while swapping in a page,
 * and some other program tries to do a serial write at the same time.
 * Since the lock will only come under contention when the system is
 * swapping and available memory is low, it makes sense to share one
 * buffer across all the serial ports, since it significantly saves
 * memory if large numbers of serial ports are open.
 */
static unsigned char *tmp_buf = 0;
static DECLARE_MUTEX(tmp_buf_sem);

static void rp_start(struct tty_struct *tty);

static inline int rocket_paranoia_check(struct r_port *info,
					kdev_t device, const char *routine)
{
#ifdef ROCKET_PARANOIA_CHECK
	static const char *badmagic =
		"Warning: bad magic number for rocketport struct (%d, %d) in %s\n";
	if (!info)
		return 1;
	if (info->magic != RPORT_MAGIC) {
		printk(badmagic, MAJOR(device), MINOR(device), routine);
		return 1;
	}
#endif
	return 0;
}

/*
 * Here begins the interrupt/polling routine for the Rocketport!
 */
static _INLINE_ void rp_do_receive(struct r_port *info, struct tty_struct *tty,
				   CHANNEL_t *cp, unsigned int ChanStatus)
{
	unsigned int CharNStat;
	int ToRecv, wRecv, space, count;
	unsigned char	*cbuf;
	char		*fbuf;
	
	ToRecv= sGetRxCnt(cp);
	space = 2*TTY_FLIPBUF_SIZE;
	cbuf = tty->flip.char_buf;
	fbuf = tty->flip.flag_buf;
	count = 0;
#ifdef ROCKET_DEBUG_INTR
	printk("rp_do_receive(%d, %d)...", ToRecv, space);
#endif
	if (ToRecv == 0 || (space <= 0))
		return;
	
	/*
	 * determine how many we can actually read in.  If we can't
	 * read any in then we have a software overrun condition.
	 */
	if (ToRecv > space)
		ToRecv = space;
	
	/*
	 * if status indicates there are errored characters in the
	 * FIFO, then enter status mode (a word in FIFO holds
	 * character and status).
	 */
	if (ChanStatus & (RXFOVERFL | RXBREAK | RXFRAME | RXPARITY)) {
		if (!(ChanStatus & STATMODE)) {
#ifdef ROCKET_DEBUG_RECEIVE
			printk("Entering STATMODE...");
#endif
			ChanStatus |= STATMODE;
			sEnRxStatusMode(cp);
		}
	}

	/* 
	 * if we previously entered status mode, then read down the
	 * FIFO one word at a time, pulling apart the character and
	 * the status.  Update error counters depending on status
	 */
	if (ChanStatus & STATMODE) {
#ifdef ROCKET_DEBUG_RECEIVE
		printk("Ignore %x, read %x...", info->ignore_status_mask,
		       info->read_status_mask);
#endif
		while (ToRecv) {
			CharNStat= sInW(sGetTxRxDataIO(cp));

#ifdef ROCKET_DEBUG_RECEIVE
			printk("%x...", CharNStat);
#endif

			if (CharNStat & STMBREAKH)
				CharNStat &= ~(STMFRAMEH | STMPARITYH);
			if (CharNStat & info->ignore_status_mask) {
				ToRecv--;
				continue;
			}
			CharNStat &= info->read_status_mask;
			if (CharNStat & STMBREAKH) {
				*fbuf++ = TTY_BREAK;
#if 0
				if (info->flags & ROCKET_SAK)
					do_SAK(tty);
#endif
			} else if (CharNStat & STMPARITYH)
				*fbuf++ = TTY_PARITY;
			else if (CharNStat & STMFRAMEH)
				*fbuf++ = TTY_FRAME;
			else if (CharNStat & STMRCVROVRH)
				*fbuf++ =TTY_OVERRUN;
			else
				*fbuf++ = 0;
			*cbuf++ = CharNStat & 0xff;
			count++;
			ToRecv--;
		}

		/*
		 * after we've emptied the FIFO in status mode, turn
		 * status mode back off
		 */
		if (sGetRxCnt(cp) == 0) {
#ifdef ROCKET_DEBUG_RECEIVE
			printk("Status mode off.\n");
#endif
			sDisRxStatusMode(cp);
		}
	} else {
		/*
		 * we aren't in status mode, so read down the FIFO two
		 * characters at time by doing repeated word IO
		 * transfer.
		 */
		wRecv= ToRecv >> 1;
		if (wRecv)
			sInStrW(sGetTxRxDataIO(cp), cbuf,
				wRecv);
		if (ToRecv & 1)
			cbuf[ToRecv-1] = sInB(sGetTxRxDataIO(cp));
		memset(fbuf, 0, ToRecv);
		cbuf += ToRecv;
		fbuf += ToRecv;
		count += ToRecv;
	}
	tty->ldisc.receive_buf(tty, tty->flip.char_buf,
			       tty->flip.flag_buf, count);
}

/*
 * This routine is called when a transmit interrupt is found.  It's
 * responsible for pushing data found in the transmit buffer out to
 * the serial card.
 */
static _INLINE_ void rp_do_transmit(struct r_port *info)
{
	int	c;
	CHANNEL_t *cp = &info->channel;
	struct tty_struct *tty;
	
#ifdef ROCKET_DEBUG_INTR
	printk("rp_do_transmit ");
#endif
	if (!info)
		return;
	if (!info->tty) {
		printk("rp: WARNING rp_do_transmit called with info->tty==NULL\n");
		xmit_flags[info->line >> 5] &= ~(1 << (info->line & 0x1f));
		return;
	}
	tty = info->tty;
	info->xmit_fifo_room = TXFIFO_SIZE - sGetTxCnt(cp);
	while (1) {
		if (tty->stopped || tty->hw_stopped)
			break;
		c = MIN(info->xmit_fifo_room,
			MIN(info->xmit_cnt,
			    XMIT_BUF_SIZE - info->xmit_tail));
		if (c <= 0 || info->xmit_fifo_room <= 0)
			break;
		sOutStrW(sGetTxRxDataIO(cp),
			 info->xmit_buf + info->xmit_tail, c/2);
		if (c & 1)
			sOutB(sGetTxRxDataIO(cp),
			      info->xmit_buf[info->xmit_tail + c -
					     1]);
		info->xmit_tail += c;
		info->xmit_tail &= XMIT_BUF_SIZE-1;
		info->xmit_cnt -= c;
		info->xmit_fifo_room -= c;
#ifdef ROCKET_DEBUG_INTR
		printk("tx %d chars...", c);
#endif
	}
	if (info->xmit_cnt == 0)
		xmit_flags[info->line >> 5] &= ~(1 << (info->line & 0x1f));
	if (info->xmit_cnt < WAKEUP_CHARS) {
		if ((tty->flags & (1 << TTY_DO_WRITE_WAKEUP)) &&
		    tty->ldisc.write_wakeup)
			(tty->ldisc.write_wakeup)(tty);
		wake_up_interruptible(&tty->write_wait);
	}
#ifdef ROCKET_DEBUG_INTR
	printk("(%d,%d,%d,%d)...", info->xmit_cnt, info->xmit_head,
	       info->xmit_tail, info->xmit_fifo_room);
#endif
}

/*
 * This function is called for each port which has signalled an
 * interrupt.  It checks what interrupts are pending and services
 * them. 
 */
static _INLINE_ void rp_handle_port(struct r_port *info)
{
	CHANNEL_t *cp;
	struct tty_struct *tty;
	unsigned int IntMask, ChanStatus;

	if (!info)
		return;
	if ( (info->flags & ROCKET_INITIALIZED) == 0 ) {
		printk("rp: WARNING: rp_handle_port called with info->flags & NOT_INIT\n");
		return;
	}
	if (!info->tty) {
		printk("rp: WARNING: rp_handle_port called with info->tty==NULL\n");
		return;
	}
	cp = &info->channel;
	tty = info->tty;

	IntMask = sGetChanIntID(cp) & info->intmask;
#ifdef ROCKET_DEBUG_INTR
	printk("rp_interrupt %02x...", IntMask);
#endif
	ChanStatus= sGetChanStatus(cp);
	if (IntMask & RXF_TRIG) {	/* Rx FIFO trigger level */
		rp_do_receive(info, tty, cp, ChanStatus);
	}
#if 0
	if (IntMask & SRC_INT) {	/* Special receive condition */
	}
#endif
	if (IntMask & DELTA_CD) {	/* CD change  */
#if (defined(ROCKET_DEBUG_OPEN) || defined(ROCKET_DEBUG_INTR) || \
     defined(ROCKET_DEBUG_HANGUP))
		printk("ttyR%d CD now %s...", info->line,
		       (ChanStatus & CD_ACT) ? "on" : "off");
#endif
		if (!(ChanStatus & CD_ACT) &&
		    info->cd_status &&
		    !((info->flags & ROCKET_CALLOUT_ACTIVE) &&
		      (info->flags & ROCKET_CALLOUT_NOHUP))) {
#ifdef ROCKET_DEBUG_HANGUP
			printk("CD drop, calling hangup.\n");
#endif
			tty_hangup(tty);
		}
		info->cd_status = (ChanStatus & CD_ACT) ? 1 : 0;
		wake_up_interruptible(&info->open_wait);
	}
#ifdef ROCKET_DEBUG_INTR
	if (IntMask & DELTA_CTS) {	/* CTS change */
		printk("CTS change...\n");
	}
	if (IntMask & DELTA_DSR) {	/* DSR change */
		printk("DSR change...\n");
	}
#endif
}

/*
 * The top level polling routine.
 */
static void rp_do_poll(unsigned long dummy)
{
	CONTROLLER_t *ctlp;
	int ctrl, aiop, ch, line;
	unsigned int xmitmask;
	unsigned char CtlMask, AiopMask;

#ifdef TIME_STAT
	unsigned long loop_time;
	unsigned long long time_stat_tmp=0, time_stat_tmp2=0;

	rdtscll(time_stat_tmp);
#endif /* TIME_STAT */

	for (ctrl=0; ctrl < max_board; ctrl++) {
		if (rcktpt_io_addr[ctrl] <= 0)
			continue;
		ctlp= sCtlNumToCtlPtr(ctrl);

#ifdef ENABLE_PCI
		if(ctlp->BusType == isPCI)
			CtlMask= sPCIGetControllerIntStatus(ctlp);
		else
#endif
			CtlMask= sGetControllerIntStatus(ctlp);
		for (aiop=0; CtlMask; CtlMask >>= 1, aiop++) {
			if (CtlMask & 1) {
				AiopMask= sGetAiopIntStatus(ctlp, aiop);
				for (ch=0; AiopMask; AiopMask >>= 1, ch++) {
					if (AiopMask & 1) {
						line = (ctrl << 5) | 
							(aiop << 3) | ch;
						rp_handle_port(rp_table[line]);
					}
				}
			}
		}
		xmitmask = xmit_flags[ctrl];
		for (line = ctrl << 5; xmitmask; xmitmask >>= 1, line++) {
			if (xmitmask & 1)
				rp_do_transmit(rp_table[line]);
		}
	}

	/*
	 * Reset the timer so we get called at the next clock tick.
	 */
	if (rp_num_ports_open) {
		mod_timer(&rocket_timer, jiffies + 1);
	}
#ifdef TIME_STAT
	rdtscll(time_stat_tmp2);
	time_stat_tmp2 -= time_stat_tmp;
	time_stat += time_stat_tmp2;
	if (time_counter == 0) 
		time_stat_short = time_stat_long = time_stat_tmp2;
	else {
		if ( time_stat_tmp2 < time_stat_short )
			time_stat_short = time_stat_tmp2;
		else if ( time_stat_tmp2 > time_stat_long )
			time_stat_long = time_stat_tmp2;
	}
	if ( ++time_counter == TIME_COUNTER ) {
		loop_time = (unsigned long) ( ((unsigned long)(time_stat >> 32) * ( (unsigned long)(0xffffffff)/(TIME_STAT_CPU * TIME_COUNTER) ) ) + ((unsigned long)time_stat/(TIME_STAT_CPU*TIME_COUNTER)));
#ifdef TIME_STAT_VERBOSE
		printk("rp_do_poll: Interrupt Timings\n");
		printk("     %5ld iterations; %ld us min,\n",
		       (long)TIME_COUNTER, (time_stat_short/TIME_STAT_CPU));
		printk("     %5ld us max, %ld us average per iteration.\n",
		       (time_stat_long/TIME_STAT_CPU), loop_time);
		printk("We want to use < 5,000 us for an iteration.\n");
#else /* TIME_STAT_VERBOSE */
		printk("rp: %ld loops: %ld min, %ld max, %ld us/loop.\n",
		       (long)TIME_COUNTER, (time_stat_short/TIME_STAT_CPU),
		       (time_stat_long/TIME_STAT_CPU), loop_time);
#endif /* TIME_STAT_VERBOSE */
		time_counter = time_stat = 0;
		time_stat_short = time_stat_long = 0;
	}
#endif /* TIME_STAT */
}
/*
 * Here ends the interrupt/polling routine.
 */


/*
 * This function initializes the r_port structure, as well as enabling
 * the port on the RocketPort board.
 */
static void init_r_port(int board, int aiop, int chan)
{
	struct r_port *info;
	int line;
	CONTROLLER_T *ctlp;
	CHANNEL_t	*cp;
	
	line = (board << 5) | (aiop << 3) | chan;

	ctlp= sCtlNumToCtlPtr(board);

	info = kmalloc(sizeof(struct r_port), GFP_KERNEL);
	if (!info) {
		printk("Couldn't allocate info struct for line #%d\n", line);
		return;
	}
	memset(info, 0, sizeof(struct r_port));
	
	info->magic = RPORT_MAGIC;
	info->line = line;
	info->ctlp = ctlp;
	info->board = board;
	info->aiop = aiop;
	info->chan = chan;
	info->closing_wait = 3000;
	info->close_delay = 50;
	info->callout_termios =callout_driver.init_termios;
	info->normal_termios = rocket_driver.init_termios;
	init_waitqueue_head(&info->open_wait);
	init_waitqueue_head(&info->close_wait);

	info->intmask = RXF_TRIG | TXFIFO_MT | SRC_INT | DELTA_CD |
		DELTA_CTS | DELTA_DSR;
	if (sInitChan(ctlp, &info->channel, aiop, chan) == 0) {
		printk("Rocketport sInitChan(%d, %d, %d) failed!\n",
		       board, aiop, chan);
		kfree(info);
		return;
	}
	cp = &info->channel;
	rp_table[line] = info;
}

#if (LINUX_VERSION_CODE < 131394) /* Linux 2.1.66 */
static int baud_table[] = {
	0, 50, 75, 110, 134, 150, 200, 300,
	600, 1200, 1800, 2400, 4800, 9600, 19200,
	38400, 57600, 115200, 230400, 460800, 0 };
#endif

/*
 * This routine configures a rocketport port so according to its
 * termio settings.
 */
static void configure_r_port(struct r_port *info)
{
	unsigned cflag;
	unsigned long 	flags;
	int	bits, baud;
#if (LINUX_VERSION_CODE < 131393) /* Linux 2.1.65 */
	int i;
#endif
	CHANNEL_t	*cp;
	
	if (!info->tty || !info->tty->termios)
		return;
	cp = &info->channel;
	cflag = info->tty->termios->c_cflag;

	/* Byte size and parity */
	if ((cflag & CSIZE) == CS8) {
		sSetData8(cp);
		bits = 10;
	} else {
		sSetData7(cp);
		bits = 9;
	}
        if (cflag & CSTOPB) {
		sSetStop2(cp);
		bits++;
	} else {
		sSetStop1(cp);
	}
	
	if (cflag & PARENB) {
		sEnParity(cp);
		bits++;
		if (cflag & PARODD) {
			sSetOddParity(cp);
		} else {
			sSetEvenParity(cp);
		}
	} else {
		sDisParity(cp);
	}
	
	/* baud rate */
#if (LINUX_VERSION_CODE < 131394) /* Linux 2.1.66 */
	i = cflag & CBAUD;
	if (i & CBAUDEX) {
		i &= ~CBAUDEX;
		if (i < 1 || i > 4) 
			info->tty->termios->c_cflag &= ~CBAUDEX;
		else
			i += 15;
	}
	if (i == 15) {
		if ((info->flags & ROCKET_SPD_MASK) == ROCKET_SPD_HI)
			i += 1;
		if ((info->flags & ROCKET_SPD_MASK) == ROCKET_SPD_VHI)
			i += 2;
		if ((info->flags & ROCKET_SPD_MASK) == ROCKET_SPD_SHI)
			i += 3;
		if ((info->flags & ROCKET_SPD_MASK) == ROCKET_SPD_WARP)
			i += 4;
	}
	baud = baud_table[i] ? baud_table[i] : 9600;
#else
	baud = tty_get_baud_rate(info->tty);
	if (!baud)
		baud = 9600;
#endif
	info->cps = baud / bits;
	sSetBaud(cp, (rp_baud_base/baud) - 1);
	
	if (cflag & CRTSCTS) {
		info->intmask |= DELTA_CTS;
		sEnCTSFlowCtl(cp);
	} else {
		info->intmask &= ~DELTA_CTS;
		sDisCTSFlowCtl(cp);
	}
	sSetRTS(&info->channel);
	if (cflag & CLOCAL)
		info->intmask &= ~DELTA_CD;
	else {
		save_flags(flags); cli();
		if (sGetChanStatus(cp) & CD_ACT)
			info->cd_status = 1;
		else
			info->cd_status = 0;
		info->intmask |= DELTA_CD;
		restore_flags(flags);
	}

	/*
	 * Handle software flow control in the board
	 */
#ifdef ROCKET_SOFT_FLOW
	if (I_IXON(info->tty)) {
		sEnTxSoftFlowCtl(cp);
		if (I_IXANY(info->tty)) {
			sEnIXANY(cp);
		} else {
			sDisIXANY(cp);
		}
		sSetTxXONChar(cp, START_CHAR(info->tty));
		sSetTxXOFFChar(cp, STOP_CHAR(info->tty));
	} else {
		sDisTxSoftFlowCtl(cp);
		sDisIXANY(cp);
		sClrTxXOFF(cp);
	}
#endif
	
	/*
	 * Set up ignore/read mask words
	 */
	info->read_status_mask = STMRCVROVRH | 0xFF;
	if (I_INPCK(info->tty))
		info->read_status_mask |= STMFRAMEH | STMPARITYH;
	if (I_BRKINT(info->tty) || I_PARMRK(info->tty))
		info->read_status_mask |= STMBREAKH;

	/*
	 * Characters to ignore
	 */
	info->ignore_status_mask = 0;
	if (I_IGNPAR(info->tty))
		info->ignore_status_mask |= STMFRAMEH | STMPARITYH;
	if (I_IGNBRK(info->tty)) {
		info->ignore_status_mask |= STMBREAKH;
		/*
		 * If we're ignoring parity and break indicators,
		 * ignore overruns too.  (For real raw support).
		 */
		if (I_IGNPAR(info->tty))
			info->ignore_status_mask |= STMRCVROVRH;
	}
}

static int block_til_ready(struct tty_struct *tty, struct file * filp,
			   struct r_port *info)
{
	DECLARE_WAITQUEUE(wait, current);
	int		retval;
	int		do_clocal = 0, extra_count = 0;
	unsigned long	flags;

	/*
	 * If the device is in the middle of being closed, then block
	 * until it's done, and then try again.
	 */
	if (tty_hung_up_p(filp))
		return ((info->flags & ROCKET_HUP_NOTIFY) ? 
			-EAGAIN : -ERESTARTSYS);
	if (info->flags & ROCKET_CLOSING) {
		interruptible_sleep_on(&info->close_wait);
		return ((info->flags & ROCKET_HUP_NOTIFY) ? 
			-EAGAIN : -ERESTARTSYS);
	}

	/*
	 * If this is a callout device, then just make sure the normal
	 * device isn't being used.
	 */
	if (tty->driver.subtype == SERIAL_TYPE_CALLOUT) {
		if (info->flags & ROCKET_NORMAL_ACTIVE)
			return -EBUSY;
		if ((info->flags & ROCKET_CALLOUT_ACTIVE) &&
		    (info->flags & ROCKET_SESSION_LOCKOUT) &&
		    (info->session != current->session))
		    return -EBUSY;
		if ((info->flags & ROCKET_CALLOUT_ACTIVE) &&
		    (info->flags & ROCKET_PGRP_LOCKOUT) &&
		    (info->pgrp != current->pgrp))
		    return -EBUSY;
		info->flags |= ROCKET_CALLOUT_ACTIVE;
		return 0;
	}
	
	/*
	 * If non-blocking mode is set, or the port is not enabled,
	 * then make the check up front and then exit.
	 */
	if ((filp->f_flags & O_NONBLOCK) ||
	    (tty->flags & (1 << TTY_IO_ERROR))) {
		if (info->flags & ROCKET_CALLOUT_ACTIVE)
			return -EBUSY;
		info->flags |= ROCKET_NORMAL_ACTIVE;
		return 0;
	}

	if (info->flags & ROCKET_CALLOUT_ACTIVE) {
		if (info->normal_termios.c_cflag & CLOCAL)
			do_clocal = 1;
	} else {
		if (tty->termios->c_cflag & CLOCAL)
			do_clocal = 1;
	}
	
	/*
	 * Block waiting for the carrier detect and the line to become
	 * free (i.e., not in use by the callout).  While we are in
	 * this loop, info->count is dropped by one, so that
	 * rp_close() knows when to free things.  We restore it upon
	 * exit, either normal or abnormal.
	 */
	retval = 0;
	add_wait_queue(&info->open_wait, &wait);
#ifdef ROCKET_DEBUG_OPEN
	printk("block_til_ready before block: ttyR%d, count = %d\n",
	       info->line, info->count);
#endif
	save_flags(flags); cli();
	if (!tty_hung_up_p(filp)) {
		extra_count = 1;
		info->count--;
	}
	restore_flags(flags);
	info->blocked_open++;
	while (1) {
		if (!(info->flags & ROCKET_CALLOUT_ACTIVE) &&
		    (tty->termios->c_cflag & CBAUD)) {
			sSetDTR(&info->channel);
			sSetRTS(&info->channel);
		}
		set_current_state(TASK_INTERRUPTIBLE);
		if (tty_hung_up_p(filp) ||
		    !(info->flags & ROCKET_INITIALIZED)) {
			if (info->flags & ROCKET_HUP_NOTIFY)
				retval = -EAGAIN;
			else
				retval = -ERESTARTSYS;	
			break;
		}
		if (!(info->flags & ROCKET_CALLOUT_ACTIVE) &&
		    !(info->flags & ROCKET_CLOSING) &&
		    (do_clocal || (sGetChanStatusLo(&info->channel) &
				   CD_ACT)))
			break;
		if (signal_pending(current)) {
			retval = -ERESTARTSYS;
			break;
		}
#ifdef ROCKET_DEBUG_OPEN
		printk("block_til_ready blocking: ttyR%d, count = %d, flags=0x%0x\n",
		       info->line, info->count, info->flags);
#endif
		schedule();
	}
	current->state = TASK_RUNNING;
	remove_wait_queue(&info->open_wait, &wait);
	cli();
	if (extra_count)
		info->count++;
	restore_flags(flags);
	info->blocked_open--;
#ifdef ROCKET_DEBUG_OPEN
	printk("block_til_ready after blocking: ttyR%d, count = %d\n",
	       info->line, info->count);
#endif
	if (retval)
		return retval;
	info->flags |= ROCKET_NORMAL_ACTIVE;
	return 0;
}	

/*
 * This routine is called whenever a rocketport board is opened.
 */
static int rp_open(struct tty_struct *tty, struct file * filp)
{
	struct r_port *info;
	int	line, retval;
	CHANNEL_t	*cp;
	unsigned long page;
	
	line = MINOR(tty->device) - tty->driver.minor_start;
	if ((line < 0) || (line >= MAX_RP_PORTS))
		return -ENODEV;
	if (!tmp_buf) {
		page = get_free_page(GFP_KERNEL);
		if (!page)
			return -ENOMEM;
		if (tmp_buf)
			free_page(page);
		else
			tmp_buf = (unsigned char *) page;
	}
	page = get_free_page(GFP_KERNEL);
	if (!page)
		return -ENOMEM;

	tty->driver_data = info = rp_table[line];
	
	if (info->flags & ROCKET_CLOSING) {
		interruptible_sleep_on(&info->close_wait);
		free_page(page);
		return ((info->flags & ROCKET_HUP_NOTIFY) ?
			-EAGAIN : -ERESTARTSYS);
	}
	
	/*
	 * We must not sleep from here until the port is marked fully
	 * in use.
	 */
	if (rp_table[line] == NULL) {
		tty->flags = (1 << TTY_IO_ERROR);
		free_page(page);
		return 0;
	}
	if (!info) {
		printk("rp_open: rp_table[%d] is NULL!\n", line);
		free_page(page);
		return -EIO;
	}
	if (info->xmit_buf)
		free_page(page);
	else
		info->xmit_buf = (unsigned char *) page;
	info->tty = tty;

	if (info->flags & ROCKET_CLOSING) {
		interruptible_sleep_on(&info->close_wait);
		return ((info->flags & ROCKET_HUP_NOTIFY) ? 
			-EAGAIN : -ERESTARTSYS);
	}

	if (info->count++ == 0) {
#ifdef MODULE
		MOD_INC_USE_COUNT;
#endif
		rp_num_ports_open++;
#ifdef ROCKET_DEBUG_OPEN
		printk("rocket mod++ = %d...", rp_num_ports_open);
#endif
	}
#ifdef ROCKET_DEBUG_OPEN
	printk("rp_open ttyR%d, count=%d\n", info->line, info->count);
#endif
	/*
	 * Info->count is now 1; so it's safe to sleep now.
	 */
	info->session = current->session;
	info->pgrp = current->pgrp;
	
	cp = &info->channel;
	sSetRxTrigger(cp, TRIG_1);
	if (sGetChanStatus(cp) & CD_ACT)
		info->cd_status = 1;
	else
		info->cd_status = 0;
	sDisRxStatusMode(cp);
	sFlushRxFIFO(cp);	
	sFlushTxFIFO(cp);	

	sEnInterrupts(cp, (TXINT_EN|MCINT_EN|RXINT_EN|SRCINT_EN|CHANINT_EN));
	sSetRxTrigger(cp, TRIG_1);

	sGetChanStatus(cp);
	sDisRxStatusMode(cp);
	sClrTxXOFF(cp);

	sDisCTSFlowCtl(cp);
	sDisTxSoftFlowCtl(cp);

	sEnRxFIFO(cp);
	sEnTransmit(cp);

	info->flags |= ROCKET_INITIALIZED;

#if (LINUX_VERSION_CODE >= 131394) /* Linux 2.1.66 */
	/*
	 * Set up the tty->alt_speed kludge
	 */
	if ((info->flags & ROCKET_SPD_MASK) == ROCKET_SPD_HI)
		info->tty->alt_speed = 57600;
	if ((info->flags & ROCKET_SPD_MASK) == ROCKET_SPD_VHI)
		info->tty->alt_speed = 115200;
	if ((info->flags & ROCKET_SPD_MASK) == ROCKET_SPD_SHI)
		info->tty->alt_speed = 230400;
	if ((info->flags & ROCKET_SPD_MASK) == ROCKET_SPD_WARP)
		info->tty->alt_speed = 460800;
#endif

	configure_r_port(info);
	if (tty->termios->c_cflag & CBAUD) {
		sSetDTR(cp);
		sSetRTS(cp);
	}
	
	mod_timer(&rocket_timer, jiffies + 1);

	retval = block_til_ready(tty, filp, info);
	if (retval) {
#ifdef ROCKET_DEBUG_OPEN
		printk("rp_open returning after block_til_ready with %d\n",
		       retval);
#endif
		return retval;
	}

	if ((info->count == 1) && (info->flags & ROCKET_SPLIT_TERMIOS)) {
		if (tty->driver.subtype == SERIAL_TYPE_NORMAL)
			*tty->termios = info->normal_termios;
		else 
			*tty->termios = info->callout_termios;
		configure_r_port(info);
	}

	return 0;
}

static void rp_close(struct tty_struct *tty, struct file * filp)
{
	struct r_port * info = (struct r_port *)tty->driver_data;
	unsigned long flags;
	int timeout;
	CHANNEL_t	*cp;

	if (rocket_paranoia_check(info, tty->device, "rp_close"))
		return;

#ifdef ROCKET_DEBUG_OPEN
	printk("rp_close ttyR%d, count = %d\n", info->line, info->count);
#endif
	
	save_flags(flags); cli();
	
	if (tty_hung_up_p(filp)) {
		restore_flags(flags);
		return;
	}
	if ((tty->count == 1) && (info->count != 1)) {
		/*
		 * Uh, oh.  tty->count is 1, which means that the tty
		 * structure will be freed.  Info->count should always
		 * be one in these conditions.  If it's greater than
		 * one, we've got real problems, since it means the
		 * serial port won't be shutdown.
		 */
		printk("rp_close: bad serial port count; tty->count is 1, "
		       "info->count is %d\n", info->count);
		info->count = 1;
	}
	if (--info->count < 0) {
		printk("rp_close: bad serial port count for ttyR%d: %d\n",
		       info->line, info->count);
		info->count = 0;
	}
	if (info->count) {
		restore_flags(flags);
		return;
	}
	info->flags |= ROCKET_CLOSING;
	/*
	 * Save the termios structure, since this port may have
	 * separate termios for callout and dialin.
	 */
	if (info->flags & ROCKET_NORMAL_ACTIVE)
		info->normal_termios = *tty->termios;
	if (info->flags & ROCKET_CALLOUT_ACTIVE)
		info->callout_termios = *tty->termios;
	
	cp = &info->channel;

	/*
	 * Notify the line discpline to only process XON/XOFF characters
	 */
	tty->closing = 1;

	/*
	 * If transmission was throttled by the application request,
	 * just flush the xmit buffer.
	 */
#if (LINUX_VERSION_CODE >= 131343)
	if (tty->flow_stopped)
		rp_flush_buffer(tty);
#endif

	/*
	 * Wait for the transmit buffer to clear
	 */
	if (info->closing_wait != ROCKET_CLOSING_WAIT_NONE)
		tty_wait_until_sent(tty, info->closing_wait);
	/*
	 * Before we drop DTR, make sure the UART transmitter
	 * has completely drained; this is especially
	 * important if there is a transmit FIFO!
	 */
	timeout = (sGetTxCnt(cp)+1) * HZ / info->cps;
	if (timeout == 0)
		timeout = 1;
	rp_wait_until_sent(tty, timeout);
	
	xmit_flags[info->line >> 5] &= ~(1 << (info->line & 0x1f));
	sDisTransmit(cp);
	sDisInterrupts(cp, (TXINT_EN|MCINT_EN|RXINT_EN|SRCINT_EN|CHANINT_EN));
	sDisCTSFlowCtl(cp);
	sDisTxSoftFlowCtl(cp);
	sClrTxXOFF(cp);
	sFlushRxFIFO(cp);	
	sFlushTxFIFO(cp);
	sClrRTS(cp);
	if (C_HUPCL(tty)) {
		sClrDTR(cp);
	}
	if (tty->driver.flush_buffer)
		tty->driver.flush_buffer(tty);
	if (tty->ldisc.flush_buffer)
		tty->ldisc.flush_buffer(tty);

	xmit_flags[info->line >> 5] &= ~(1 << (info->line & 0x1f));
	if (info->blocked_open) {
		if (info->close_delay) {
			current->state = TASK_INTERRUPTIBLE;
			schedule_timeout(info->close_delay);
		}
		wake_up_interruptible(&info->open_wait);
	} else {
		if (info->xmit_buf) {
			free_page((unsigned long) info->xmit_buf);
			info->xmit_buf = 0;
		}
	}
	info->flags &= ~(ROCKET_INITIALIZED | ROCKET_CLOSING |
			 ROCKET_CALLOUT_ACTIVE | ROCKET_NORMAL_ACTIVE);
	tty->closing = 0;
	wake_up_interruptible(&info->close_wait);
	
#ifdef MODULE
	MOD_DEC_USE_COUNT;
#endif
	rp_num_ports_open--;
#ifdef ROCKET_DEBUG_OPEN
	printk("rocket mod-- = %d...", rp_num_ports_open);
#endif
	restore_flags(flags);
	
#ifdef ROCKET_DEBUG_OPEN
	printk("rp_close ttyR%d complete shutdown\n", info->line);
#endif
	
}

static void rp_set_termios(struct tty_struct *tty, struct termios *old_termios)
{
	struct r_port * info = (struct r_port *)tty->driver_data;
	CHANNEL_t *cp;
	unsigned cflag;
	

	if (rocket_paranoia_check(info, tty->device, "rp_set_termios"))
		return;

	cflag = tty->termios->c_cflag;

	if (cflag == old_termios->c_cflag)
		return;

	/*
	 * This driver doesn't support CS5 or CS6
	 */
	if (((cflag & CSIZE) == CS5) ||
	    ((cflag & CSIZE) == CS6))
		tty->termios->c_cflag = ((cflag & ~CSIZE) |
					 (old_termios->c_cflag & CSIZE));

	configure_r_port(info);

	cp = &info->channel;

	/* Handle transition to B0 status */
	if ((old_termios->c_cflag & CBAUD) &&
	    !(tty->termios->c_cflag & CBAUD)) {
		sClrDTR(cp);
		sClrRTS(cp);
	}
	
	/* Handle transition away from B0 status */
	if (!(old_termios->c_cflag & CBAUD) &&
	    (tty->termios->c_cflag & CBAUD)) {
		if (!tty->hw_stopped ||
		    !(tty->termios->c_cflag & CRTSCTS)) {
			sSetRTS(cp);
		}
		sSetDTR(cp);
	}
	
	if ((old_termios->c_cflag & CRTSCTS) &&
	    !(tty->termios->c_cflag & CRTSCTS)) {
		tty->hw_stopped = 0;
		rp_start(tty);
	}
}

/*
 * Here are the routines used by rp_ioctl
 */
#if (LINUX_VERSION_CODE < 131394) /* Linux 2.1.66 */
static void send_break(	struct r_port * info, int duration)
{
	current->state = TASK_INTERRUPTIBLE;
	cli();
	sSendBreak(&info->channel);
	schedule_timeout(duration);
	sClrBreak(&info->channel);
	sti();
}
#else
static void rp_break(struct tty_struct *tty, int break_state)
{
	struct r_port * info = (struct r_port *)tty->driver_data;
	unsigned long flags;
	
	if (rocket_paranoia_check(info, tty->device, "rp_break"))
		return;

	save_flags(flags); cli();
	if (break_state == -1) {
		sSendBreak(&info->channel);
	} else {
		sClrBreak(&info->channel);
	}
	restore_flags(flags);
}
#endif

static int get_modem_info(struct r_port * info, unsigned int *value)
{
	unsigned int control, result, ChanStatus;

	ChanStatus = sGetChanStatusLo(&info->channel);
	
	control = info->channel.TxControl[3];
	result =  ((control & SET_RTS) ? TIOCM_RTS : 0)
		| ((control & SET_DTR) ? TIOCM_DTR : 0)
		| ((ChanStatus  & CD_ACT) ? TIOCM_CAR : 0)
			/* TIOCM_RNG not supported */
		| ((ChanStatus  & DSR_ACT) ? TIOCM_DSR : 0)
		| ((ChanStatus  & CTS_ACT) ? TIOCM_CTS : 0);

	if (copy_to_user(value, &result, sizeof(int)))
		return -EFAULT;
	return 0;
}

static int set_modem_info(struct r_port * info, unsigned int cmd,
			  unsigned int *value)
{
	unsigned int arg;

	if (copy_from_user(&arg, value, sizeof(int)))
		return -EFAULT;

	switch (cmd) {
	case TIOCMBIS: 
		if (arg & TIOCM_RTS)
			info->channel.TxControl[3] |= SET_RTS;
		if (arg & TIOCM_DTR)
			info->channel.TxControl[3] |= SET_DTR;
		break;
	case TIOCMBIC:
		if (arg & TIOCM_RTS)
			info->channel.TxControl[3] &= ~SET_RTS;
		if (arg & TIOCM_DTR)
			info->channel.TxControl[3] &= ~SET_DTR;
		break;
	case TIOCMSET:
		info->channel.TxControl[3] =
			((info->channel.TxControl[3] & ~(SET_RTS | SET_DTR))
			 | ((arg & TIOCM_RTS) ? SET_RTS : 0)
			 | ((arg & TIOCM_DTR) ? SET_DTR : 0));
		break;
	default:
		return -EINVAL;
	}

	sOutDW(info->channel.IndexAddr,
	       *(DWord_t *) &(info->channel.TxControl[0]));
	
	return 0;
}

static int get_config(struct r_port * info, struct rocket_config * retinfo)
{
	struct rocket_config tmp;
  
	if (!retinfo)
		return -EFAULT;
	memset(&tmp, 0, sizeof(tmp));
	tmp.line = info->line;
	tmp.flags = info->flags;
	tmp.close_delay = info->close_delay;
	tmp.closing_wait = info->closing_wait;
	tmp.port = rcktpt_io_addr[(info->line >> 5) & 3];
	
	if (copy_to_user(retinfo,&tmp,sizeof(*retinfo)))
		return -EFAULT;
	return 0;
}

static int set_config(struct r_port * info, struct rocket_config * new_info)
{
	struct rocket_config new_serial;

	if (copy_from_user(&new_serial, new_info, sizeof(new_serial)))
		return -EFAULT;

#ifdef CAP_SYS_ADMIN
	if (!capable(CAP_SYS_ADMIN))
#else
	if (!suser())
#endif
	{
		if ((new_serial.flags & ~ROCKET_USR_MASK) !=
		    (info->flags & ~ROCKET_USR_MASK))
			return -EPERM;
		info->flags = ((info->flags & ~ROCKET_USR_MASK) |
			       (new_serial.flags & ROCKET_USR_MASK));
		configure_r_port(info);
		return 0;
	}
	
	info->flags = ((info->flags & ~ROCKET_FLAGS) |
			(new_serial.flags & ROCKET_FLAGS));
	info->close_delay = new_serial.close_delay;
	info->closing_wait = new_serial.closing_wait;

#if (LINUX_VERSION_CODE >= 131394) /* Linux 2.1.66 */
	if ((info->flags & ROCKET_SPD_MASK) == ROCKET_SPD_HI)
		info->tty->alt_speed = 57600;
	if ((info->flags & ROCKET_SPD_MASK) == ROCKET_SPD_VHI)
		info->tty->alt_speed = 115200;
	if ((info->flags & ROCKET_SPD_MASK) == ROCKET_SPD_SHI)
		info->tty->alt_speed = 230400;
	if ((info->flags & ROCKET_SPD_MASK) == ROCKET_SPD_WARP)
		info->tty->alt_speed = 460800;
#endif
	
	configure_r_port(info);
	return 0;
}

static int get_ports(struct r_port * info, struct rocket_ports * retports)
{
	struct rocket_ports tmp;
	int	board, port, index;
  
	if (!retports)
		return -EFAULT;
	memset(&tmp, 0, sizeof(tmp));
	tmp.tty_major = rocket_driver.major;
	tmp.callout_major = callout_driver.major;
	for (board = 0; board < 4; board++) {
		index = board << 5;
		for (port = 0; port < 32; port++, index++) {
			if (rp_table[index])
				tmp.port_bitmap[board] |= 1 << port;
		}
	}
	if (copy_to_user(retports,&tmp,sizeof(*retports)))
		return -EFAULT;
	return 0;
}

static int rp_ioctl(struct tty_struct *tty, struct file * file,
		    unsigned int cmd, unsigned long arg)
{
	struct r_port * info = (struct r_port *)tty->driver_data;
#if (LINUX_VERSION_CODE < 131394) /* Linux 2.1.66 */
	int retval, tmp;
#endif

	if (cmd != RCKP_GET_PORTS &&
	    rocket_paranoia_check(info, tty->device, "rp_ioctl"))
		return -ENODEV;

	switch (cmd) {
#if (LINUX_VERSION_CODE < 131394) /* Linux 2.1.66 */
		case TCSBRK:	/* SVID version: non-zero arg --> no break */
			retval = tty_check_change(tty);
			if (retval)
				return retval;
			tty_wait_until_sent(tty, 0);
			if (signal_pending(current))
				return -EINTR;
			if (!arg) {
				send_break(info, HZ/4);	/* 1/4 second */
				if (signal_pending(current))
					return -EINTR;
			}
			return 0;
		case TCSBRKP:	/* support for POSIX tcsendbreak() */
			retval = tty_check_change(tty);
			if (retval)
				return retval;
			tty_wait_until_sent(tty, 0);
			if (signal_pending(current))
				return -EINTR;
			send_break(info, arg ? arg*(HZ/10) : HZ/4);
			if (signal_pending(current))
				return -EINTR;
			return 0;
		case TIOCGSOFTCAR:
			tmp = C_CLOCAL(tty) ? 1 : 0;
			if (copy_to_user((void *)arg, &tmp, sizeof(int)))
				return -EFAULT;
			return 0;
		case TIOCSSOFTCAR:
			if (copy_from_user(&tmp, (void *)arg, sizeof(int)))
				return -EFAULT;

			tty->termios->c_cflag =
				((tty->termios->c_cflag & ~CLOCAL) |
				 (tmp ? CLOCAL : 0));
			return 0;
#endif
		case TIOCMGET:
			return get_modem_info(info, (unsigned int *) arg);
		case TIOCMBIS:
		case TIOCMBIC:
		case TIOCMSET:
			return set_modem_info(info, cmd, (unsigned int *) arg);
		case RCKP_GET_STRUCT:
			if (copy_to_user((void *) arg, info,
					 sizeof(struct r_port)))
				return -EFAULT;
			return 0;

		case RCKP_GET_CONFIG:
			return get_config(info, (struct rocket_config *) arg);
		case RCKP_SET_CONFIG:
			return set_config(info, (struct rocket_config *) arg);
			
		case RCKP_GET_PORTS:
			return get_ports(info, (struct rocket_ports *) arg);
		default:
			return -ENOIOCTLCMD;
		}
	return 0;
}

#if (defined(ROCKET_DEBUG_FLOW) || defined(ROCKET_DEBUG_THROTTLE))
static char *rp_tty_name(struct tty_struct *tty, char *buf)
{
	if (tty)
		sprintf(buf, "%s%d", tty->driver.name,
			MINOR(tty->device) - tty->driver.minor_start +
			tty->driver.name_base);
	else
		strcpy(buf, "NULL tty");
	return buf;
}
#endif

static void rp_send_xchar(struct tty_struct *tty, char ch)
{
	struct r_port *info = (struct r_port *)tty->driver_data;
	CHANNEL_t *cp;

	if (rocket_paranoia_check(info, tty->device, "rp_send_xchar"))
		return;

	cp = &info->channel;
	if (sGetTxCnt(cp)) 
		sWriteTxPrioByte(cp, ch);
	else
		sWriteTxByte(sGetTxRxDataIO(cp), ch);
}

static void rp_throttle(struct tty_struct * tty)
{
	struct r_port *info = (struct r_port *)tty->driver_data;
	CHANNEL_t *cp;
#ifdef ROCKET_DEBUG_THROTTLE
	char	buf[64];
	
	printk("throttle %s: %d....\n", rp_tty_name(tty, buf),
	       tty->ldisc.chars_in_buffer(tty));
#endif

	if (rocket_paranoia_check(info, tty->device, "rp_throttle"))
		return;

	cp = &info->channel;
	if (I_IXOFF(tty))
		rp_send_xchar(tty, STOP_CHAR(tty));
	
	sClrRTS(&info->channel);
}

static void rp_unthrottle(struct tty_struct * tty)
{
	struct r_port *info = (struct r_port *)tty->driver_data;
	CHANNEL_t *cp;
#ifdef ROCKET_DEBUG_THROTTLE
	char	buf[64];
	
	printk("unthrottle %s: %d....\n", rp_tty_name(tty, buf),
	       tty->ldisc.chars_in_buffer(tty));
#endif

	if (rocket_paranoia_check(info, tty->device, "rp_throttle"))
		return;

	cp = &info->channel;
	if (I_IXOFF(tty))
		rp_send_xchar(tty, START_CHAR(tty));

	sSetRTS(&info->channel);
}

/*
 * ------------------------------------------------------------
 * rp_stop() and rp_start()
 *
 * This routines are called before setting or resetting tty->stopped.
 * They enable or disable transmitter interrupts, as necessary.
 * ------------------------------------------------------------
 */
static void rp_stop(struct tty_struct *tty)
{
	struct r_port * info = (struct r_port *)tty->driver_data;
#ifdef ROCKET_DEBUG_FLOW
	char	buf[64];
	
	printk("stop %s: %d %d....\n", rp_tty_name(tty, buf),
	       info->xmit_cnt, info->xmit_fifo_room);
#endif

	if (rocket_paranoia_check(info, tty->device, "rp_stop"))
		return;

	if (sGetTxCnt(&info->channel))
		sDisTransmit(&info->channel);
}

static void rp_start(struct tty_struct *tty)
{
	struct r_port * info = (struct r_port *)tty->driver_data;
#ifdef ROCKET_DEBUG_FLOW
	char	buf[64];
	
	printk("start %s: %d %d....\n", rp_tty_name(tty, buf),
	       info->xmit_cnt, info->xmit_fifo_room);
#endif

	if (rocket_paranoia_check(info, tty->device, "rp_stop"))
		return;

	sEnTransmit(&info->channel);
	xmit_flags[info->line >> 5] |= (1 << (info->line & 0x1f));
}

/*
 * rp_wait_until_sent() --- wait until the transmitter is empty
 */
static void rp_wait_until_sent(struct tty_struct *tty, int timeout)
{
	struct r_port *info = (struct r_port *)tty->driver_data;
	CHANNEL_t *cp;
	unsigned long orig_jiffies;
	int check_time, exit_time;
	int txcnt;
	
	if (rocket_paranoia_check(info, tty->device, "rp_wait_until_sent"))
		return;

	cp = &info->channel;

	orig_jiffies = jiffies;
#ifdef ROCKET_DEBUG_WAIT_UNTIL_SENT
	printk("In RP_wait_until_sent(%d) (jiff=%lu)...", timeout, jiffies);
	printk("cps=%d...", info->cps);
#endif
	while (1) {
		txcnt = sGetTxCnt(cp);
		if (!txcnt) {
			if (sGetChanStatusLo(cp) & TXSHRMT)
				break;
			check_time = (HZ / info->cps) / 5;
		} else
			check_time = HZ * txcnt / info->cps;
		if (timeout) {
			exit_time = orig_jiffies + timeout - jiffies;
			if (exit_time <= 0)
				break;
			if (exit_time < check_time)
				check_time = exit_time;
		}
		if (check_time == 0)
			check_time = 1;
#ifdef ROCKET_DEBUG_WAIT_UNTIL_SENT
		printk("txcnt = %d (jiff=%lu,check=%d)...", txcnt,
		       jiffies, check_time);
#endif
		current->state = TASK_INTERRUPTIBLE;
		schedule_timeout(check_time);
		if (signal_pending(current))
			break;
	}
	current->state = TASK_RUNNING;
#ifdef ROCKET_DEBUG_WAIT_UNTIL_SENT
	printk("txcnt = %d (jiff=%lu)...done\n", txcnt, jiffies);
#endif
}

/*
 * rp_hangup() --- called by tty_hangup() when a hangup is signaled.
 */
static void rp_hangup(struct tty_struct *tty)
{
	CHANNEL_t	*cp;
	struct r_port * info = (struct r_port *)tty->driver_data;
	
	if (rocket_paranoia_check(info, tty->device, "rp_hangup"))
		return;

#if (defined(ROCKET_DEBUG_OPEN) || defined(ROCKET_DEBUG_HANGUP))
	printk("rp_hangup of ttyR%d...", info->line);
#endif
	/*
	 * If the port is in the process of being closed, just force
	 * the transmit buffer to be empty, and let rp_close handle
	 * the clean up.
	 */
	if (info->flags & ROCKET_CLOSING) {
		cli();
		info->xmit_cnt = info->xmit_head = info->xmit_tail = 0;
		sti();
		wake_up_interruptible(&tty->write_wait);
		return;
	}
	if (info->count) {
#ifdef MODULE
		MOD_DEC_USE_COUNT;
#endif
		rp_num_ports_open--;
	}
	
	xmit_flags[info->line >> 5] &= ~(1 << (info->line & 0x1f));
	info->count = 0;
	info->flags &= ~(ROCKET_NORMAL_ACTIVE|ROCKET_CALLOUT_ACTIVE);
	info->tty = 0;

	cp = &info->channel;
	sDisRxFIFO(cp);
	sDisTransmit(cp);
	sDisInterrupts(cp, (TXINT_EN|MCINT_EN|RXINT_EN|SRCINT_EN|CHANINT_EN));
	sDisCTSFlowCtl(cp);
	sDisTxSoftFlowCtl(cp);
	sClrTxXOFF(cp);
	info->flags &= ~ROCKET_INITIALIZED;
	
	wake_up_interruptible(&info->open_wait);
}

/*
 * The Rocketport write routines.  The Rocketport driver uses a
 * double-buffering strategy, with the twist that if the in-memory CPU
 * buffer is empty, and there's space in the transmit FIFO, the
 * writing routines will write directly to transmit FIFO.
 *
 * This gets a little tricky, but I'm pretty sure I got it all right.
 */
static void rp_put_char(struct tty_struct *tty, unsigned char ch)
{
	struct r_port * info = (struct r_port *)tty->driver_data;
	CHANNEL_t	*cp;

	if (rocket_paranoia_check(info, tty->device, "rp_put_char"))
		return;

#ifdef ROCKET_DEBUG_WRITE
	printk("rp_put_char %c...", ch);
#endif
	
	cp = &info->channel;

	if (!tty->stopped && !tty->hw_stopped && info->xmit_fifo_room == 0)
		info->xmit_fifo_room = TXFIFO_SIZE - sGetTxCnt(cp);

	if (tty->stopped || tty->hw_stopped ||
	    info->xmit_fifo_room == 0 || info->xmit_cnt != 0) {
		info->xmit_buf[info->xmit_head++] = ch;
		info->xmit_head &= XMIT_BUF_SIZE-1;
		info->xmit_cnt++;
		xmit_flags[info->line >> 5] |= (1 << (info->line & 0x1f));
	} else {
		sOutB(sGetTxRxDataIO(cp), ch);
		info->xmit_fifo_room--;
	}
}

static int rp_write(struct tty_struct * tty, int from_user,
		    const unsigned char *buf, int count)
{
	struct r_port * info = (struct r_port *)tty->driver_data;
	CHANNEL_t	*cp;
	const unsigned char	*b;
	int		c, retval = 0;
	unsigned long	flags;

	if (count <= 0 || rocket_paranoia_check(info, tty->device, "rp_write"))
		return 0;

#ifdef ROCKET_DEBUG_WRITE
	printk("rp_write %d chars...", count);
#endif
	cp = &info->channel;

	if (!tty->stopped && !tty->hw_stopped && info->xmit_fifo_room == 0)
		info->xmit_fifo_room = TXFIFO_SIZE - sGetTxCnt(cp);

	if (!tty->stopped && !tty->hw_stopped && info->xmit_cnt == 0
	    && info->xmit_fifo_room >= 0) {
		c = MIN(count, info->xmit_fifo_room);
		b = buf;
		if (from_user) {
			down(&tmp_buf_sem);
			c -= copy_from_user(tmp_buf, buf, c);
			b = tmp_buf;
			up(&tmp_buf_sem);
			/* In case we got pre-empted */
			if (!c) {
				retval = -EFAULT;
				goto end;
			}
			if (info->tty == 0)
				goto end;
			c = MIN(c, info->xmit_fifo_room);
		}
		sOutStrW(sGetTxRxDataIO(cp), b, c/2);
		if (c & 1)
			sOutB(sGetTxRxDataIO(cp), b[c-1]);
		retval += c;
		buf += c;
		count -= c;
		info->xmit_fifo_room -= c;
	}
	if (!count)
		goto end;
	
	save_flags(flags);
	while (1) {
		if (info->tty == 0) {
			restore_flags(flags);
			goto end;
		}
		c = MIN(count, MIN(XMIT_BUF_SIZE - info->xmit_cnt - 1,
				   XMIT_BUF_SIZE - info->xmit_head));
		if (c <= 0)
			break;

		b = buf;
		if (from_user) {
			down(&tmp_buf_sem);
			c -= copy_from_user(tmp_buf, buf, c);
			b = tmp_buf;
			up(&tmp_buf_sem);
			if (!c) {
				if (retval == 0)
					retval = -EFAULT;
				goto end_intr;
			}
			/* In case we got pre-empted */
			if (info->tty == 0)
				goto end_intr;
		}
		cli();
		c = MIN(c, MIN(XMIT_BUF_SIZE - info->xmit_cnt - 1,
			       XMIT_BUF_SIZE - info->xmit_head));
		memcpy(info->xmit_buf + info->xmit_head, b, c);
		info->xmit_head = (info->xmit_head + c) & (XMIT_BUF_SIZE-1);
		info->xmit_cnt += c;
		restore_flags(flags);
		buf += c;
		count -= c;
		retval += c;
	}
end_intr:
	if ((retval > 0) && !tty->stopped && !tty->hw_stopped)
		xmit_flags[info->line >> 5] |= (1 << (info->line & 0x1f));
	restore_flags(flags);
end:
	if (info->xmit_cnt < WAKEUP_CHARS) {
		if ((tty->flags & (1 << TTY_DO_WRITE_WAKEUP)) &&
		    tty->ldisc.write_wakeup)
			(tty->ldisc.write_wakeup)(tty);
		wake_up_interruptible(&tty->write_wait);
	}
	return retval;
}

/*
 * Return the number of characters that can be sent.  We estimate
 * only using the in-memory transmit buffer only, and ignore the
 * potential space in the transmit FIFO.
 */
static int rp_write_room(struct tty_struct *tty)
{
	struct r_port * info = (struct r_port *)tty->driver_data;
	int	ret;

	if (rocket_paranoia_check(info, tty->device, "rp_write_room"))
		return 0;

	ret = XMIT_BUF_SIZE - info->xmit_cnt - 1;
	if (ret < 0)
		ret = 0;
#ifdef ROCKET_DEBUG_WRITE
	printk("rp_write_room returns %d...", ret);
#endif
	return ret;
}

/*
 * Return the number of characters in the buffer.  Again, this only
 * counts those characters in the in-memory transmit buffer.
 */
static int rp_chars_in_buffer(struct tty_struct *tty)
{
	struct r_port * info = (struct r_port *)tty->driver_data;
	CHANNEL_t	*cp;

	if (rocket_paranoia_check(info, tty->device, "rp_chars_in_buffer"))
		return 0;

	cp = &info->channel;

#ifdef ROCKET_DEBUG_WRITE
	printk("rp_chars_in_buffer returns %d...", info->xmit_cnt);
#endif
	return info->xmit_cnt;
}

static void rp_flush_buffer(struct tty_struct *tty)
{
	struct r_port * info = (struct r_port *)tty->driver_data;
	CHANNEL_t	*cp;

	if (rocket_paranoia_check(info, tty->device, "rp_flush_buffer"))
		return;

	cli();
	info->xmit_cnt = info->xmit_head = info->xmit_tail = 0;
	sti();
	wake_up_interruptible(&tty->write_wait);
	if ((tty->flags & (1 << TTY_DO_WRITE_WAKEUP)) &&
	    tty->ldisc.write_wakeup)
		(tty->ldisc.write_wakeup)(tty);
	
	cp = &info->channel;
	
	sFlushTxFIFO(cp);
}

#ifdef ENABLE_PCI
#if (LINUX_VERSION_CODE < 0x020163) /* 2.1.99 */
/* For compatibility */
static struct pci_dev *pci_find_slot(unsigned char bus,
				     unsigned char device_fn)
{
	unsigned short		vendor_id, device_id;
	int			ret, error;
	static struct pci_dev	ret_struct;
	
	error = pcibios_read_config_word(bus, device_fn, PCI_VENDOR_ID,
		&vendor_id);
	ret = pcibios_read_config_word(bus, device_fn, PCI_DEVICE_ID,
		&device_id);
	if (error == 0)
		error = ret;

	if (error) {
		printk("PCI RocketPort error: %s not initializing due to error"
		       "reading configuration space\n",
		       pcibios_strerror(error));
		return(0);
	}

	memset(&ret_struct, 0, sizeof(ret_struct));
	ret_struct.device = device_id;

	return &ret_struct;
}
#endif
     
int __init register_PCI(int i, unsigned int bus, unsigned int device_fn)
{
	int	num_aiops, aiop, max_num_aiops, num_chan, chan;
	unsigned int	aiopio[MAX_AIOPS_PER_BOARD];
	char *str;
	CONTROLLER_t	*ctlp;
	struct pci_dev *dev = pci_find_slot(bus, device_fn);
#if (LINUX_VERSION_CODE < 0x020163) /* 2.1.99 */
	int	ret;
	unsigned int port;
#endif

	if (!dev)
		return 0;

	if (pci_enable_device(dev))
		return 0;

	rcktpt_io_addr[i] = pci_resource_start (dev, 0);
	switch(dev->device) {
	case PCI_DEVICE_ID_RP4QUAD:
		str = "Quadcable";
		max_num_aiops = 1;
		break;
	case PCI_DEVICE_ID_RP8OCTA:
		str = "Octacable";
		max_num_aiops = 1;
		break;
	case PCI_DEVICE_ID_RP8INTF:
		str = "8";
		max_num_aiops = 1;
		break;
	case PCI_DEVICE_ID_RP8J:
		str = "8J";
		max_num_aiops = 1;
		break;
	case PCI_DEVICE_ID_RP4J:
		str = "4J";
		max_num_aiops = 1;
		break;
	case PCI_DEVICE_ID_RP16INTF:
		str = "16";
		max_num_aiops = 2;
		break;
	case PCI_DEVICE_ID_RP32INTF:
		str = "32";
		max_num_aiops = 4;
		break;
	case PCI_DEVICE_ID_RPP4:
		str = "Plus Quadcable";
		max_num_aiops = 1;
		break;
	case PCI_DEVICE_ID_RPP8:
		str = "Plus Octacable";
		max_num_aiops = 1;
		break;
	case PCI_DEVICE_ID_RP8M:
		str = "8-port Modem";
		max_num_aiops = 1;
		break;
	default:
		str = "(unknown/unsupported)";
		max_num_aiops = 0;
		break;
	}
	for(aiop=0;aiop < max_num_aiops;aiop++)
		aiopio[aiop] = rcktpt_io_addr[i] + (aiop * 0x40);
	ctlp = sCtlNumToCtlPtr(i);
	num_aiops = sPCIInitController(ctlp, i,
					aiopio, max_num_aiops, 0,
					FREQ_DIS, 0);
	printk("Rocketport controller #%d found at %02x:%02x, "
	       "%d AIOP(s) (PCI Rocketport %s)\n", i, bus, device_fn,
	       num_aiops, str);
	if(num_aiops <= 0) {
		rcktpt_io_addr[i] = 0;
		return(0);
	}
	for(aiop = 0;aiop < num_aiops; aiop++) {
		sResetAiopByNum(ctlp, aiop);
		sEnAiop(ctlp, aiop);
		num_chan = sGetAiopNumChan(ctlp, aiop);
		for(chan=0;chan < num_chan; chan++)
			init_r_port(i, aiop, chan);
	}
	return(1);
}

static int __init init_PCI(int boards_found)
{
	unsigned char	bus, device_fn;
	int	i, count = 0;

	for(i=0; i < (NUM_BOARDS - boards_found); i++) {
		if (!pcibios_find_device(PCI_VENDOR_ID_RP,
			PCI_DEVICE_ID_RP4QUAD, i, &bus, &device_fn)) 
			if (register_PCI(count+boards_found, bus, device_fn))
				count++;
		if (!pcibios_find_device(PCI_VENDOR_ID_RP,
			PCI_DEVICE_ID_RP8J, i, &bus, &device_fn)) 
			if (register_PCI(count+boards_found, bus, device_fn))
				count++;
		if (!pcibios_find_device(PCI_VENDOR_ID_RP,
			PCI_DEVICE_ID_RP4J, i, &bus, &device_fn)) 
			if (register_PCI(count+boards_found, bus, device_fn))
				count++;
		if(!pcibios_find_device(PCI_VENDOR_ID_RP,
			PCI_DEVICE_ID_RP8OCTA, i, &bus, &device_fn)) 
			if(register_PCI(count+boards_found, bus, device_fn))
				count++;
		if(!pcibios_find_device(PCI_VENDOR_ID_RP,
			PCI_DEVICE_ID_RP8INTF, i, &bus, &device_fn)) 
			if(register_PCI(count+boards_found, bus, device_fn))
				count++;
		if(!pcibios_find_device(PCI_VENDOR_ID_RP,
			PCI_DEVICE_ID_RP16INTF, i, &bus, &device_fn)) 
			if(register_PCI(count+boards_found, bus, device_fn))
				count++;
		if(!pcibios_find_device(PCI_VENDOR_ID_RP,
			PCI_DEVICE_ID_RP32INTF, i, &bus, &device_fn)) 
			if(register_PCI(count+boards_found, bus, device_fn))
				count++;
		if(!pcibios_find_device(PCI_VENDOR_ID_RP,
			PCI_DEVICE_ID_RP4QUAD, i, &bus, &device_fn)) 
			if(register_PCI(count+boards_found, bus, device_fn))
				count++;
		if(!pcibios_find_device(PCI_VENDOR_ID_RP,
			PCI_DEVICE_ID_RP8J, i, &bus, &device_fn)) 
			if(register_PCI(count+boards_found, bus, device_fn))
				count++;
		if(!pcibios_find_device(PCI_VENDOR_ID_RP,
			PCI_DEVICE_ID_RP4J, i, &bus, &device_fn)) 
			if(register_PCI(count+boards_found, bus, device_fn))
				count++;
		if(!pcibios_find_device(PCI_VENDOR_ID_RP,
			PCI_DEVICE_ID_RPP4, i, &bus, &device_fn)) 
			if(register_PCI(count+boards_found, bus, device_fn))
				count++;
		if(!pcibios_find_device(PCI_VENDOR_ID_RP,
			PCI_DEVICE_ID_RPP8, i, &bus, &device_fn)) 
			if(register_PCI(count+boards_found, bus, device_fn))
				count++;
		if(!pcibios_find_device(PCI_VENDOR_ID_RP,
			PCI_DEVICE_ID_RP8M, i, &bus, &device_fn)) 
			if(register_PCI(count+boards_found, bus, device_fn))
				count++;
	}
	return(count);
}
#endif

static int __init init_ISA(int i, int *reserved_controller)
{
	int	num_aiops, num_chan;
	int	aiop, chan;
	unsigned int	aiopio[MAX_AIOPS_PER_BOARD];	
	CONTROLLER_t	*ctlp;

	if (rcktpt_io_addr[i] == 0)
		return(0);

	if (check_region(rcktpt_io_addr[i],64)) {
		printk("RocketPort board address 0x%lx in use...\n",
			rcktpt_io_addr[i]);
		rcktpt_io_addr[i] = 0;
		return(0);
	}
	
	for (aiop=0; aiop<MAX_AIOPS_PER_BOARD; aiop++)
		aiopio[aiop]= rcktpt_io_addr[i] + (aiop * 0x400);
	ctlp= sCtlNumToCtlPtr(i);
	num_aiops = sInitController(ctlp, i, controller + (i*0x400),
				    aiopio, MAX_AIOPS_PER_BOARD, 0,
				    FREQ_DIS, 0);
	if (num_aiops <= 0) {
		rcktpt_io_addr[i] = 0;
		return(0);
	}
	for (aiop = 0; aiop < num_aiops; aiop++) {
		sResetAiopByNum(ctlp, aiop);
		sEnAiop(ctlp, aiop);
		num_chan = sGetAiopNumChan(ctlp,aiop);
		for (chan=0; chan < num_chan; chan++)
			init_r_port(i, aiop, chan);
	}
	printk("Rocketport controller #%d found at 0x%lx, "
	       "%d AIOPs\n", i, rcktpt_io_addr[i],
	       num_aiops);
	if (rcktpt_io_addr[i] + 0x40 == controller) {
		*reserved_controller = 1;
		request_region(rcktpt_io_addr[i], 68,
				       "Comtrol Rocketport");
	} else {
		request_region(rcktpt_io_addr[i], 64,
			       "Comtrol Rocketport");
	}
	return(1);
}


/*
 * The module "startup" routine; it's run when the module is loaded.
 */
int __init rp_init(void)
{
	int i, retval, pci_boards_found, isa_boards_found;
	int	reserved_controller = 0;

	printk("Rocketport device driver module, version %s, %s\n",
	       ROCKET_VERSION, ROCKET_DATE);

	/*
	 * Set up the timer channel.  If it is already in use by
	 * some other driver, give up.
	 */
	if (rocket_timer.function) {
		printk("rocket.o: Timer already in use!\n");
		return -EBUSY;
	}
	init_timer(&rocket_timer);
	rocket_timer.function = rp_do_poll;
	
	/*
	 * Initialize the array of pointers to our own internal state
	 * structures.
	 */
	memset(rp_table, 0, sizeof(rp_table));
	memset(xmit_flags, 0, sizeof(xmit_flags));

	if (board1 == 0)
		board1 = 0x180;
	if (controller == 0)
		controller = board1 + 0x40;

	if (check_region(controller, 4)) {
		printk("Controller IO addresses in use, unloading driver.\n");
		return -EBUSY;
	}
	
	rcktpt_io_addr[0] = board1;
	rcktpt_io_addr[1] = board2;
	rcktpt_io_addr[2] = board3;
	rcktpt_io_addr[3] = board4;

	/*
	 * If support_low_speed is set, use the slow clock prescale,
	 * which supports 50 bps
	 */
	if (support_low_speed) {
		sClockPrescale = 0x19;	/* mod 9 (divide by 10) prescale */
		rp_baud_base = 230400;
	} else {
		sClockPrescale = 0x14; /* mod 4 (devide by 5) prescale */
		rp_baud_base = 460800;
	}
	
	/*
	 * OK, let's probe each of the controllers looking for boards.
	 */
	isa_boards_found = 0;
	pci_boards_found = 0;
	for (i=0; i < NUM_BOARDS; i++) {
		if(init_ISA(i, &reserved_controller))
			isa_boards_found++;
	}
#ifdef ENABLE_PCI
	if (pcibios_present()) {
		if(isa_boards_found < NUM_BOARDS)
			pci_boards_found = init_PCI(isa_boards_found);
	} else {
		printk("No PCI BIOS found\n");
	}
#endif
	max_board = pci_boards_found + isa_boards_found;
	
	if (max_board == 0) {
		printk("No rocketport ports found; unloading driver.\n");
		rocket_timer.function = 0;
		return -ENODEV;
	}

	if (reserved_controller == 0)
		request_region(controller, 4, "Comtrol Rocketport");

	/*
	 * Set up the tty driver structure and then register this
	 * driver with the tty layer.
	 */
	memset(&rocket_driver, 0, sizeof(struct tty_driver));
	rocket_driver.magic = TTY_DRIVER_MAGIC;
#ifdef CONFIG_DEVFS_FS
	rocket_driver.name = "tts/R%d";
#else
	rocket_driver.name = "ttyR";
#endif
	rocket_driver.major = TTY_ROCKET_MAJOR;
	rocket_driver.minor_start = 0;
	rocket_driver.num = MAX_RP_PORTS;
	rocket_driver.type = TTY_DRIVER_TYPE_SERIAL;
	rocket_driver.subtype = SERIAL_TYPE_NORMAL;
	rocket_driver.init_termios = tty_std_termios;
	rocket_driver.init_termios.c_cflag =
		B9600 | CS8 | CREAD | HUPCL | CLOCAL;
	rocket_driver.flags = TTY_DRIVER_REAL_RAW;
	rocket_driver.refcount = &rocket_refcount;
	rocket_driver.table = rocket_table;
	rocket_driver.termios = rocket_termios;
	rocket_driver.termios_locked = rocket_termios_locked;

	rocket_driver.open = rp_open;
	rocket_driver.close = rp_close;
	rocket_driver.write = rp_write;
	rocket_driver.put_char = rp_put_char;
	rocket_driver.write_room = rp_write_room;
	rocket_driver.chars_in_buffer = rp_chars_in_buffer;
	rocket_driver.flush_buffer = rp_flush_buffer;
	rocket_driver.ioctl = rp_ioctl;
	rocket_driver.throttle = rp_throttle;
	rocket_driver.unthrottle = rp_unthrottle;
	rocket_driver.set_termios = rp_set_termios;
	rocket_driver.stop = rp_stop;
	rocket_driver.start = rp_start;
	rocket_driver.hangup = rp_hangup;
#if (LINUX_VERSION_CODE >= 131394) /* Linux 2.1.66 */
	rocket_driver.break_ctl = rp_break;
#endif
#if (LINUX_VERSION_CODE >= 131343)
	rocket_driver.send_xchar = rp_send_xchar;
	rocket_driver.wait_until_sent = rp_wait_until_sent;
#endif

	/*
	 * The callout device is just like normal device except for
	 * the minor number and the subtype code.
	 */
	callout_driver = rocket_driver;
#ifdef CONFIG_DEVFS_FS
	callout_driver.name = "cua/R%d";
#else
	callout_driver.name = "cur";
#endif
	callout_driver.major = CUA_ROCKET_MAJOR;
	callout_driver.minor_start = 0;
	callout_driver.subtype = SERIAL_TYPE_CALLOUT;
	
	retval = tty_register_driver(&callout_driver);
	if (retval < 0) {
		printk("Couldn't install Rocketport callout driver "
		       "(error %d)\n", -retval);
		return -1;
	}

	retval = tty_register_driver(&rocket_driver);
	if (retval < 0) {
		printk("Couldn't install tty Rocketport driver "
		       "(error %d)\n", -retval);
		return -1;
	}
#ifdef ROCKET_DEBUG_OPEN
	printk("Rocketport driver is major %d, callout is %d\n",
	       rocket_driver.major, callout_driver.major);
#endif

	return 0;
}

#ifdef MODULE
int init_module(void)
{
	return rp_init();
}

void
cleanup_module( void) {
	int	retval;
	int	i;
	int	released_controller = 0;

	del_timer_sync(&rocket_timer);

	retval = tty_unregister_driver(&callout_driver);
	if (retval) {
		printk("Error %d while trying to unregister "
		       "rocketport callout driver\n", -retval);
	}
	retval = tty_unregister_driver(&rocket_driver);
	if (retval) {
		printk("Error %d while trying to unregister "
		       "rocketport driver\n", -retval);
	}
	for (i = 0; i < MAX_RP_PORTS; i++) {
		if (rp_table[i])
			kfree(rp_table[i]);
	}
	for (i=0; i < NUM_BOARDS; i++) {
		if (rcktpt_io_addr[i] <= 0)
			continue;
		if (rcktpt_io_addr[i] + 0x40 == controller) {
			released_controller++;
			release_region(rcktpt_io_addr[i], 68);
		} else
			release_region(rcktpt_io_addr[i], 64);
		if (released_controller == 0)
			release_region(controller, 4);
	}
	if (tmp_buf)
		free_page((unsigned long) tmp_buf);
	rocket_timer.function = 0;
}
#endif

/***********************************************************************
		Copyright 1994 Comtrol Corporation.
			All Rights Reserved.

The following source code is subject to Comtrol Corporation's
Developer's License Agreement.

This source code is protected by United States copyright law and 
international copyright treaties.

This source code may only be used to develop software products that
will operate with Comtrol brand hardware.

You may not reproduce nor distribute this source code in its original
form but must produce a derivative work which includes portions of
this source code only.

The portions of this source code which you use in your derivative
work must bear Comtrol's copyright notice:

		Copyright 1994 Comtrol Corporation.

***********************************************************************/

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

static Byte_t RData[RDATASIZE] =
{
   0x00, 0x09, 0xf6, 0x82,
   0x02, 0x09, 0x86, 0xfb,
   0x04, 0x09, 0x00, 0x0a,
   0x06, 0x09, 0x01, 0x0a,
   0x08, 0x09, 0x8a, 0x13,
   0x0a, 0x09, 0xc5, 0x11,
   0x0c, 0x09, 0x86, 0x85,
   0x0e, 0x09, 0x20, 0x0a,
   0x10, 0x09, 0x21, 0x0a,
   0x12, 0x09, 0x41, 0xff,
   0x14, 0x09, 0x82, 0x00,
   0x16, 0x09, 0x82, 0x7b,
   0x18, 0x09, 0x8a, 0x7d,
   0x1a, 0x09, 0x88, 0x81,
   0x1c, 0x09, 0x86, 0x7a,
   0x1e, 0x09, 0x84, 0x81,
   0x20, 0x09, 0x82, 0x7c,
   0x22, 0x09, 0x0a, 0x0a 
};

static Byte_t RRegData[RREGDATASIZE]=
{
   0x00, 0x09, 0xf6, 0x82,             /* 00: Stop Rx processor */
   0x08, 0x09, 0x8a, 0x13,             /* 04: Tx software flow control */
   0x0a, 0x09, 0xc5, 0x11,             /* 08: XON char */
   0x0c, 0x09, 0x86, 0x85,             /* 0c: XANY */
   0x12, 0x09, 0x41, 0xff,             /* 10: Rx mask char */
   0x14, 0x09, 0x82, 0x00,             /* 14: Compare/Ignore #0 */
   0x16, 0x09, 0x82, 0x7b,             /* 18: Compare #1 */
   0x18, 0x09, 0x8a, 0x7d,             /* 1c: Compare #2 */
   0x1a, 0x09, 0x88, 0x81,             /* 20: Interrupt #1 */
   0x1c, 0x09, 0x86, 0x7a,             /* 24: Ignore/Replace #1 */
   0x1e, 0x09, 0x84, 0x81,             /* 28: Interrupt #2 */
   0x20, 0x09, 0x82, 0x7c,             /* 2c: Ignore/Replace #2 */
   0x22, 0x09, 0x0a, 0x0a              /* 30: Rx FIFO Enable */
};

CONTROLLER_T sController[CTL_SIZE] =
{
   {-1,-1,0,0,0,0,0,0,0,0,0,{0,0,0,0},{0,0,0,0},{-1,-1,-1,-1},{0,0,0,0}},
   {-1,-1,0,0,0,0,0,0,0,0,0,{0,0,0,0},{0,0,0,0},{-1,-1,-1,-1},{0,0,0,0}},
   {-1,-1,0,0,0,0,0,0,0,0,0,{0,0,0,0},{0,0,0,0},{-1,-1,-1,-1},{0,0,0,0}},
   {-1,-1,0,0,0,0,0,0,0,0,0,{0,0,0,0},{0,0,0,0},{-1,-1,-1,-1},{0,0,0,0}}
};

#if 0
/* IRQ number to MUDBAC register 2 mapping */
Byte_t sIRQMap[16] =
{
   0,0,0,0x10,0x20,0x30,0,0,0,0x40,0x50,0x60,0x70,0,0,0x80
};
#endif

Byte_t sBitMapClrTbl[8] =
{
   0xfe,0xfd,0xfb,0xf7,0xef,0xdf,0xbf,0x7f
};

Byte_t sBitMapSetTbl[8] =
{
   0x01,0x02,0x04,0x08,0x10,0x20,0x40,0x80
};

int sClockPrescale = 0x14;

/***************************************************************************
Function: sInitController
Purpose:  Initialization of controller global registers and controller
          structure.
Call:     sInitController(CtlP,CtlNum,MudbacIO,AiopIOList,AiopIOListSize,
                          IRQNum,Frequency,PeriodicOnly)
          CONTROLLER_T *CtlP; Ptr to controller structure
          int CtlNum; Controller number
          ByteIO_t MudbacIO; Mudbac base I/O address.
          ByteIO_t *AiopIOList; List of I/O addresses for each AIOP.
             This list must be in the order the AIOPs will be found on the
             controller.  Once an AIOP in the list is not found, it is
             assumed that there are no more AIOPs on the controller.
          int AiopIOListSize; Number of addresses in AiopIOList
          int IRQNum; Interrupt Request number.  Can be any of the following:
                         0: Disable global interrupts
                         3: IRQ 3
                         4: IRQ 4
                         5: IRQ 5
                         9: IRQ 9
                         10: IRQ 10
                         11: IRQ 11
                         12: IRQ 12
                         15: IRQ 15
          Byte_t Frequency: A flag identifying the frequency
                   of the periodic interrupt, can be any one of the following:
                      FREQ_DIS - periodic interrupt disabled
                      FREQ_137HZ - 137 Hertz
                      FREQ_69HZ - 69 Hertz
                      FREQ_34HZ - 34 Hertz
                      FREQ_17HZ - 17 Hertz
                      FREQ_9HZ - 9 Hertz
                      FREQ_4HZ - 4 Hertz
                   If IRQNum is set to 0 the Frequency parameter is
                   overidden, it is forced to a value of FREQ_DIS.
          int PeriodicOnly: TRUE if all interrupts except the periodic
                               interrupt are to be blocked.
                            FALSE is both the periodic interrupt and
                               other channel interrupts are allowed.
                            If IRQNum is set to 0 the PeriodicOnly parameter is
                               overidden, it is forced to a value of FALSE.
Return:   int: Number of AIOPs on the controller, or CTLID_NULL if controller
               initialization failed.

Comments:
          If periodic interrupts are to be disabled but AIOP interrupts
          are allowed, set Frequency to FREQ_DIS and PeriodicOnly to FALSE.

          If interrupts are to be completely disabled set IRQNum to 0.

          Setting Frequency to FREQ_DIS and PeriodicOnly to TRUE is an
          invalid combination.

          This function performs initialization of global interrupt modes,
          but it does not actually enable global interrupts.  To enable
          and disable global interrupts use functions sEnGlobalInt() and
          sDisGlobalInt().  Enabling of global interrupts is normally not
          done until all other initializations are complete.

          Even if interrupts are globally enabled, they must also be
          individually enabled for each channel that is to generate
          interrupts.

Warnings: No range checking on any of the parameters is done.

          No context switches are allowed while executing this function.

          After this function all AIOPs on the controller are disabled,
          they can be enabled with sEnAiop().
*/
int sInitController(	CONTROLLER_T *CtlP,
			int CtlNum,
			ByteIO_t MudbacIO,
			ByteIO_t *AiopIOList,
			int AiopIOListSize,
			int IRQNum,
			Byte_t Frequency,
			int PeriodicOnly)
{
	int		i;
	ByteIO_t	io;

   CtlP->CtlNum = CtlNum;
   CtlP->CtlID = CTLID_0001;        /* controller release 1 */
   CtlP->BusType = isISA;     
   CtlP->MBaseIO = MudbacIO;
   CtlP->MReg1IO = MudbacIO + 1;
   CtlP->MReg2IO = MudbacIO + 2;
   CtlP->MReg3IO = MudbacIO + 3;
#if 1
   CtlP->MReg2 = 0;                 /* interrupt disable */
   CtlP->MReg3 = 0;                 /* no periodic interrupts */
#else
   if(sIRQMap[IRQNum] == 0)            /* interrupts globally disabled */
   {
      CtlP->MReg2 = 0;                 /* interrupt disable */
      CtlP->MReg3 = 0;                 /* no periodic interrupts */
   }
   else
   {
      CtlP->MReg2 = sIRQMap[IRQNum];   /* set IRQ number */
      CtlP->MReg3 = Frequency;         /* set frequency */
      if(PeriodicOnly)                 /* periodic interrupt only */
      {
         CtlP->MReg3 |= PERIODIC_ONLY;
      }
   }
#endif
   sOutB(CtlP->MReg2IO,CtlP->MReg2);
   sOutB(CtlP->MReg3IO,CtlP->MReg3);
   sControllerEOI(CtlP);               /* clear EOI if warm init */
   /* Init AIOPs */
   CtlP->NumAiop = 0;
   for(i=0; i < AiopIOListSize; i++)
   {
      io = AiopIOList[i];
      CtlP->AiopIO[i] = (WordIO_t)io;
      CtlP->AiopIntChanIO[i] = io + _INT_CHAN;
      sOutB(CtlP->MReg2IO,CtlP->MReg2 | (i & 0x03)); /* AIOP index */
      sOutB(MudbacIO,(Byte_t)(io >> 6));	/* set up AIOP I/O in MUDBAC */
      sEnAiop(CtlP,i);                         /* enable the AIOP */

      CtlP->AiopID[i] = sReadAiopID(io);       /* read AIOP ID */
      if(CtlP->AiopID[i] == AIOPID_NULL)       /* if AIOP does not exist */
      {
         sDisAiop(CtlP,i);                     /* disable AIOP */
         break;                                /* done looking for AIOPs */
      }

      CtlP->AiopNumChan[i] = sReadAiopNumChan((WordIO_t)io); /* num channels in AIOP */
      sOutW((WordIO_t)io + _INDX_ADDR,_CLK_PRE);      /* clock prescaler */
      sOutB(io + _INDX_DATA,sClockPrescale);
      CtlP->NumAiop++;                         /* bump count of AIOPs */
      sDisAiop(CtlP,i);                        /* disable AIOP */
   }

   if(CtlP->NumAiop == 0)
      return(-1);
   else
      return(CtlP->NumAiop);
}

/***************************************************************************
Function: sPCIInitController
Purpose:  Initialization of controller global registers and controller
          structure.
Call:     sPCIInitController(CtlP,CtlNum,AiopIOList,AiopIOListSize,
                          IRQNum,Frequency,PeriodicOnly)
          CONTROLLER_T *CtlP; Ptr to controller structure
          int CtlNum; Controller number
          ByteIO_t *AiopIOList; List of I/O addresses for each AIOP.
             This list must be in the order the AIOPs will be found on the
             controller.  Once an AIOP in the list is not found, it is
             assumed that there are no more AIOPs on the controller.
          int AiopIOListSize; Number of addresses in AiopIOList
          int IRQNum; Interrupt Request number.  Can be any of the following:
                         0: Disable global interrupts
                         3: IRQ 3
                         4: IRQ 4
                         5: IRQ 5
                         9: IRQ 9
                         10: IRQ 10
                         11: IRQ 11
                         12: IRQ 12
                         15: IRQ 15
          Byte_t Frequency: A flag identifying the frequency
                   of the periodic interrupt, can be any one of the following:
                      FREQ_DIS - periodic interrupt disabled
                      FREQ_137HZ - 137 Hertz
                      FREQ_69HZ - 69 Hertz
                      FREQ_34HZ - 34 Hertz
                      FREQ_17HZ - 17 Hertz
                      FREQ_9HZ - 9 Hertz
                      FREQ_4HZ - 4 Hertz
                   If IRQNum is set to 0 the Frequency parameter is
                   overidden, it is forced to a value of FREQ_DIS.
          int PeriodicOnly: TRUE if all interrupts except the periodic
                               interrupt are to be blocked.
                            FALSE is both the periodic interrupt and
                               other channel interrupts are allowed.
                            If IRQNum is set to 0 the PeriodicOnly parameter is
                               overidden, it is forced to a value of FALSE.
Return:   int: Number of AIOPs on the controller, or CTLID_NULL if controller
               initialization failed.

Comments:
          If periodic interrupts are to be disabled but AIOP interrupts
          are allowed, set Frequency to FREQ_DIS and PeriodicOnly to FALSE.

          If interrupts are to be completely disabled set IRQNum to 0.

          Setting Frequency to FREQ_DIS and PeriodicOnly to TRUE is an
          invalid combination.

          This function performs initialization of global interrupt modes,
          but it does not actually enable global interrupts.  To enable
          and disable global interrupts use functions sEnGlobalInt() and
          sDisGlobalInt().  Enabling of global interrupts is normally not
          done until all other initializations are complete.

          Even if interrupts are globally enabled, they must also be
          individually enabled for each channel that is to generate
          interrupts.

Warnings: No range checking on any of the parameters is done.

          No context switches are allowed while executing this function.

          After this function all AIOPs on the controller are disabled,
          they can be enabled with sEnAiop().
*/
int sPCIInitController(	CONTROLLER_T *CtlP,
			int CtlNum,
			ByteIO_t *AiopIOList,
			int AiopIOListSize,
			int IRQNum,
			Byte_t Frequency,
			int PeriodicOnly)
{
	int		i;
	ByteIO_t	io;

   CtlP->CtlNum = CtlNum;
   CtlP->CtlID = CTLID_0001;        /* controller release 1 */
   CtlP->BusType = isPCI;        /* controller release 1 */

   CtlP->PCIIO = (WordIO_t)((ByteIO_t)AiopIOList[0] + _PCI_INT_FUNC);

   sPCIControllerEOI(CtlP);               /* clear EOI if warm init */
   /* Init AIOPs */
   CtlP->NumAiop = 0;
   for(i=0; i < AiopIOListSize; i++)
   {
      io = AiopIOList[i];
      CtlP->AiopIO[i] = (WordIO_t)io;
      CtlP->AiopIntChanIO[i] = io + _INT_CHAN;

      CtlP->AiopID[i] = sReadAiopID(io);       /* read AIOP ID */
      if(CtlP->AiopID[i] == AIOPID_NULL)       /* if AIOP does not exist */
         break;                                /* done looking for AIOPs */

      CtlP->AiopNumChan[i] = sReadAiopNumChan((WordIO_t)io); /* num channels in AIOP */
      sOutW((WordIO_t)io + _INDX_ADDR,_CLK_PRE);      /* clock prescaler */
      sOutB(io + _INDX_DATA,sClockPrescale);
      CtlP->NumAiop++;                         /* bump count of AIOPs */
   }

   if(CtlP->NumAiop == 0)
      return(-1);
   else
      return(CtlP->NumAiop);
}

/***************************************************************************
Function: sReadAiopID
Purpose:  Read the AIOP idenfication number directly from an AIOP.
Call:     sReadAiopID(io)
          ByteIO_t io: AIOP base I/O address
Return:   int: Flag AIOPID_XXXX if a valid AIOP is found, where X
                 is replace by an identifying number.
          Flag AIOPID_NULL if no valid AIOP is found
Warnings: No context switches are allowed while executing this function.

*/
int sReadAiopID(ByteIO_t io)
{
   Byte_t AiopID;               /* ID byte from AIOP */

   sOutB(io + _CMD_REG,RESET_ALL);     /* reset AIOP */
   sOutB(io + _CMD_REG,0x0);
   AiopID = sInB(io + _CHN_STAT0) & 0x07;
   if(AiopID == 0x06)
      return(1);
   else                                /* AIOP does not exist */
      return(-1);
}

/***************************************************************************
Function: sReadAiopNumChan
Purpose:  Read the number of channels available in an AIOP directly from
          an AIOP.
Call:     sReadAiopNumChan(io)
          WordIO_t io: AIOP base I/O address
Return:   int: The number of channels available
Comments: The number of channels is determined by write/reads from identical
          offsets within the SRAM address spaces for channels 0 and 4.
          If the channel 4 space is mirrored to channel 0 it is a 4 channel
          AIOP, otherwise it is an 8 channel.
Warnings: No context switches are allowed while executing this function.
*/
int sReadAiopNumChan(WordIO_t io)
{
   Word_t x;

   sOutDW((DWordIO_t)io + _INDX_ADDR,0x12340000L); /* write to chan 0 SRAM */
   sOutW(io + _INDX_ADDR,0);       /* read from SRAM, chan 0 */
   x = sInW(io + _INDX_DATA);
   sOutW(io + _INDX_ADDR,0x4000);  /* read from SRAM, chan 4 */
   if(x != sInW(io + _INDX_DATA))  /* if different must be 8 chan */
      return(8);
   else
      return(4);
}

/***************************************************************************
Function: sInitChan
Purpose:  Initialization of a channel and channel structure
Call:     sInitChan(CtlP,ChP,AiopNum,ChanNum)
          CONTROLLER_T *CtlP; Ptr to controller structure
          CHANNEL_T *ChP; Ptr to channel structure
          int AiopNum; AIOP number within controller
          int ChanNum; Channel number within AIOP
Return:   int: TRUE if initialization succeeded, FALSE if it fails because channel
               number exceeds number of channels available in AIOP.
Comments: This function must be called before a channel can be used.
Warnings: No range checking on any of the parameters is done.

          No context switches are allowed while executing this function.
*/
int sInitChan(	CONTROLLER_T *CtlP,
		CHANNEL_T *ChP,
		int AiopNum,
		int ChanNum)
{
   int i;
   WordIO_t AiopIO;
   WordIO_t ChIOOff;
   Byte_t *ChR;
   Word_t ChOff;
   static Byte_t R[4];
   int brd9600;

   if(ChanNum >= CtlP->AiopNumChan[AiopNum])
      return(FALSE);                   /* exceeds num chans in AIOP */

   /* Channel, AIOP, and controller identifiers */
   ChP->CtlP = CtlP;
   ChP->ChanID = CtlP->AiopID[AiopNum];
   ChP->AiopNum = AiopNum;
   ChP->ChanNum = ChanNum;

   /* Global direct addresses */
   AiopIO = CtlP->AiopIO[AiopNum];
   ChP->Cmd = (ByteIO_t)AiopIO + _CMD_REG;
   ChP->IntChan = (ByteIO_t)AiopIO + _INT_CHAN;
   ChP->IntMask = (ByteIO_t)AiopIO + _INT_MASK;
   ChP->IndexAddr = (DWordIO_t)AiopIO + _INDX_ADDR;
   ChP->IndexData = AiopIO + _INDX_DATA;

   /* Channel direct addresses */
   ChIOOff = AiopIO + ChP->ChanNum * 2;
   ChP->TxRxData = ChIOOff + _TD0;
   ChP->ChanStat = ChIOOff + _CHN_STAT0;
   ChP->TxRxCount = ChIOOff + _FIFO_CNT0;
   ChP->IntID = (ByteIO_t)AiopIO + ChP->ChanNum + _INT_ID0;

   /* Initialize the channel from the RData array */
   for(i=0; i < RDATASIZE; i+=4)
   {
      R[0] = RData[i];
      R[1] = RData[i+1] + 0x10 * ChanNum;
      R[2] = RData[i+2];
      R[3] = RData[i+3];
      sOutDW(ChP->IndexAddr,*((DWord_t *)&R[0]));
   }

   ChR = ChP->R;
   for(i=0; i < RREGDATASIZE; i+=4)
   {
      ChR[i] = RRegData[i];
      ChR[i+1] = RRegData[i+1] + 0x10 * ChanNum;
      ChR[i+2] = RRegData[i+2];
      ChR[i+3] = RRegData[i+3];
   }

   /* Indexed registers */
   ChOff = (Word_t)ChanNum * 0x1000;

   if (sClockPrescale == 0x14)
	   brd9600 = 47;
   else
	   brd9600 = 23;

   ChP->BaudDiv[0] = (Byte_t)(ChOff + _BAUD);
   ChP->BaudDiv[1] = (Byte_t)((ChOff + _BAUD) >> 8);
   ChP->BaudDiv[2] = (Byte_t)brd9600;
   ChP->BaudDiv[3] = (Byte_t)(brd9600 >> 8);
   sOutDW(ChP->IndexAddr,*(DWord_t *)&ChP->BaudDiv[0]);

   ChP->TxControl[0] = (Byte_t)(ChOff + _TX_CTRL);
   ChP->TxControl[1] = (Byte_t)((ChOff + _TX_CTRL) >> 8);
   ChP->TxControl[2] = 0;
   ChP->TxControl[3] = 0;
   sOutDW(ChP->IndexAddr,*(DWord_t *)&ChP->TxControl[0]);

   ChP->RxControl[0] = (Byte_t)(ChOff + _RX_CTRL);
   ChP->RxControl[1] = (Byte_t)((ChOff + _RX_CTRL) >> 8);
   ChP->RxControl[2] = 0;
   ChP->RxControl[3] = 0;
   sOutDW(ChP->IndexAddr,*(DWord_t *)&ChP->RxControl[0]);

   ChP->TxEnables[0] = (Byte_t)(ChOff + _TX_ENBLS);
   ChP->TxEnables[1] = (Byte_t)((ChOff + _TX_ENBLS) >> 8);
   ChP->TxEnables[2] = 0;
   ChP->TxEnables[3] = 0;
   sOutDW(ChP->IndexAddr,*(DWord_t *)&ChP->TxEnables[0]);

   ChP->TxCompare[0] = (Byte_t)(ChOff + _TXCMP1);
   ChP->TxCompare[1] = (Byte_t)((ChOff + _TXCMP1) >> 8);
   ChP->TxCompare[2] = 0;
   ChP->TxCompare[3] = 0;
   sOutDW(ChP->IndexAddr,*(DWord_t *)&ChP->TxCompare[0]);

   ChP->TxReplace1[0] = (Byte_t)(ChOff + _TXREP1B1);
   ChP->TxReplace1[1] = (Byte_t)((ChOff + _TXREP1B1) >> 8);
   ChP->TxReplace1[2] = 0;
   ChP->TxReplace1[3] = 0;
   sOutDW(ChP->IndexAddr,*(DWord_t *)&ChP->TxReplace1[0]);

   ChP->TxReplace2[0] = (Byte_t)(ChOff + _TXREP2);
   ChP->TxReplace2[1] = (Byte_t)((ChOff + _TXREP2) >> 8);
   ChP->TxReplace2[2] = 0;
   ChP->TxReplace2[3] = 0;
   sOutDW(ChP->IndexAddr,*(DWord_t *)&ChP->TxReplace2[0]);

   ChP->TxFIFOPtrs = ChOff + _TXF_OUTP;
   ChP->TxFIFO = ChOff + _TX_FIFO;

   sOutB(ChP->Cmd,(Byte_t)ChanNum | RESTXFCNT); /* apply reset Tx FIFO count */
   sOutB(ChP->Cmd,(Byte_t)ChanNum);  /* remove reset Tx FIFO count */
   sOutW((WordIO_t)ChP->IndexAddr,ChP->TxFIFOPtrs); /* clear Tx in/out ptrs */
   sOutW(ChP->IndexData,0);
   ChP->RxFIFOPtrs = ChOff + _RXF_OUTP;
   ChP->RxFIFO = ChOff + _RX_FIFO;

   sOutB(ChP->Cmd,(Byte_t)ChanNum | RESRXFCNT); /* apply reset Rx FIFO count */
   sOutB(ChP->Cmd,(Byte_t)ChanNum);  /* remove reset Rx FIFO count */
   sOutW((WordIO_t)ChP->IndexAddr,ChP->RxFIFOPtrs); /* clear Rx out ptr */
   sOutW(ChP->IndexData,0);
   sOutW((WordIO_t)ChP->IndexAddr,ChP->RxFIFOPtrs + 2); /* clear Rx in ptr */
   sOutW(ChP->IndexData,0);
   ChP->TxPrioCnt = ChOff + _TXP_CNT;
   sOutW((WordIO_t)ChP->IndexAddr,ChP->TxPrioCnt);
   sOutB(ChP->IndexData,0);
   ChP->TxPrioPtr = ChOff + _TXP_PNTR;
   sOutW((WordIO_t)ChP->IndexAddr,ChP->TxPrioPtr);
   sOutB(ChP->IndexData,0);
   ChP->TxPrioBuf = ChOff + _TXP_BUF;
   sEnRxProcessor(ChP);                /* start the Rx processor */

   return(TRUE);
}

/***************************************************************************
Function: sStopRxProcessor
Purpose:  Stop the receive processor from processing a channel.
Call:     sStopRxProcessor(ChP)
          CHANNEL_T *ChP; Ptr to channel structure

Comments: The receive processor can be started again with sStartRxProcessor().
          This function causes the receive processor to skip over the
          stopped channel.  It does not stop it from processing other channels.

Warnings: No context switches are allowed while executing this function.

          Do not leave the receive processor stopped for more than one
          character time.

          After calling this function a delay of 4 uS is required to ensure
          that the receive processor is no longer processing this channel.
*/
void sStopRxProcessor(CHANNEL_T *ChP)
{
   Byte_t R[4];

   R[0] = ChP->R[0];
   R[1] = ChP->R[1];
   R[2] = 0x0a;
   R[3] = ChP->R[3];
   sOutDW(ChP->IndexAddr,*(DWord_t *)&R[0]);
}

/***************************************************************************
Function: sFlushRxFIFO
Purpose:  Flush the Rx FIFO
Call:     sFlushRxFIFO(ChP)
          CHANNEL_T *ChP; Ptr to channel structure
Return:   void
Comments: To prevent data from being enqueued or dequeued in the Tx FIFO
          while it is being flushed the receive processor is stopped
          and the transmitter is disabled.  After these operations a
          4 uS delay is done before clearing the pointers to allow
          the receive processor to stop.  These items are handled inside
          this function.
Warnings: No context switches are allowed while executing this function.
*/
void sFlushRxFIFO(CHANNEL_T *ChP)
{
   int i;
   Byte_t Ch;                   /* channel number within AIOP */
   int RxFIFOEnabled;                  /* TRUE if Rx FIFO enabled */

   if(sGetRxCnt(ChP) == 0)             /* Rx FIFO empty */
      return;                          /* don't need to flush */

   RxFIFOEnabled = FALSE;
   if(ChP->R[0x32] == 0x08) /* Rx FIFO is enabled */
   {
      RxFIFOEnabled = TRUE;
      sDisRxFIFO(ChP);                 /* disable it */
      for(i=0; i < 2000/200; i++)	/* delay 2 uS to allow proc to disable FIFO*/
         sInB(ChP->IntChan);		/* depends on bus i/o timing */
   }
   sGetChanStatus(ChP);          /* clear any pending Rx errors in chan stat */
   Ch = (Byte_t)sGetChanNum(ChP);
   sOutB(ChP->Cmd,Ch | RESRXFCNT);     /* apply reset Rx FIFO count */
   sOutB(ChP->Cmd,Ch);                 /* remove reset Rx FIFO count */
   sOutW((WordIO_t)ChP->IndexAddr,ChP->RxFIFOPtrs); /* clear Rx out ptr */
   sOutW(ChP->IndexData,0);
   sOutW((WordIO_t)ChP->IndexAddr,ChP->RxFIFOPtrs + 2); /* clear Rx in ptr */
   sOutW(ChP->IndexData,0);
   if(RxFIFOEnabled)
      sEnRxFIFO(ChP);                  /* enable Rx FIFO */
}

/***************************************************************************
Function: sFlushTxFIFO
Purpose:  Flush the Tx FIFO
Call:     sFlushTxFIFO(ChP)
          CHANNEL_T *ChP; Ptr to channel structure
Return:   void
Comments: To prevent data from being enqueued or dequeued in the Tx FIFO
          while it is being flushed the receive processor is stopped
          and the transmitter is disabled.  After these operations a
          4 uS delay is done before clearing the pointers to allow
          the receive processor to stop.  These items are handled inside
          this function.
Warnings: No context switches are allowed while executing this function.
*/
void sFlushTxFIFO(CHANNEL_T *ChP)
{
   int i;
   Byte_t Ch;                   /* channel number within AIOP */
   int TxEnabled;                      /* TRUE if transmitter enabled */

   if(sGetTxCnt(ChP) == 0)             /* Tx FIFO empty */
      return;                          /* don't need to flush */

   TxEnabled = FALSE;
   if(ChP->TxControl[3] & TX_ENABLE)
   {
      TxEnabled = TRUE;
      sDisTransmit(ChP);               /* disable transmitter */
   }
   sStopRxProcessor(ChP);              /* stop Rx processor */
   for(i = 0; i < 4000/200; i++)         /* delay 4 uS to allow proc to stop */
      sInB(ChP->IntChan);	/* depends on bus i/o timing */
   Ch = (Byte_t)sGetChanNum(ChP);
   sOutB(ChP->Cmd,Ch | RESTXFCNT);     /* apply reset Tx FIFO count */
   sOutB(ChP->Cmd,Ch);                 /* remove reset Tx FIFO count */
   sOutW((WordIO_t)ChP->IndexAddr,ChP->TxFIFOPtrs); /* clear Tx in/out ptrs */
   sOutW(ChP->IndexData,0);
   if(TxEnabled)
      sEnTransmit(ChP);                /* enable transmitter */
   sStartRxProcessor(ChP);             /* restart Rx processor */
}

/***************************************************************************
Function: sWriteTxPrioByte
Purpose:  Write a byte of priority transmit data to a channel
Call:     sWriteTxPrioByte(ChP,Data)
          CHANNEL_T *ChP; Ptr to channel structure
          Byte_t Data; The transmit data byte

Return:   int: 1 if the bytes is successfully written, otherwise 0.

Comments: The priority byte is transmitted before any data in the Tx FIFO.

Warnings: No context switches are allowed while executing this function.
*/
int sWriteTxPrioByte(CHANNEL_T *ChP, Byte_t Data)
{
   Byte_t DWBuf[4];             /* buffer for double word writes */
   Word_t *WordPtr;          /* must be far because Win SS != DS */
   register DWordIO_t IndexAddr;

   if(sGetTxCnt(ChP) > 1)              /* write it to Tx priority buffer */
   {
      IndexAddr = ChP->IndexAddr;
      sOutW((WordIO_t)IndexAddr,ChP->TxPrioCnt); /* get priority buffer status */
      if(sInB((ByteIO_t)ChP->IndexData) & PRI_PEND) /* priority buffer busy */
         return(0);                    /* nothing sent */

      WordPtr = (Word_t *)(&DWBuf[0]);
      *WordPtr = ChP->TxPrioBuf;       /* data byte address */

      DWBuf[2] = Data;                 /* data byte value */
      sOutDW(IndexAddr,*((DWord_t *)(&DWBuf[0]))); /* write it out */

      *WordPtr = ChP->TxPrioCnt;       /* Tx priority count address */

      DWBuf[2] = PRI_PEND + 1;         /* indicate 1 byte pending */
      DWBuf[3] = 0;                    /* priority buffer pointer */
      sOutDW(IndexAddr,*((DWord_t *)(&DWBuf[0]))); /* write it out */
   }
   else                                /* write it to Tx FIFO */
   {
      sWriteTxByte(sGetTxRxDataIO(ChP),Data);
   }
   return(1);                          /* 1 byte sent */
}

/***************************************************************************
Function: sEnInterrupts
Purpose:  Enable one or more interrupts for a channel
Call:     sEnInterrupts(ChP,Flags)
          CHANNEL_T *ChP; Ptr to channel structure
          Word_t Flags: Interrupt enable flags, can be any combination
             of the following flags:
                TXINT_EN:   Interrupt on Tx FIFO empty
                RXINT_EN:   Interrupt on Rx FIFO at trigger level (see
                            sSetRxTrigger())
                SRCINT_EN:  Interrupt on SRC (Special Rx Condition)
                MCINT_EN:   Interrupt on modem input change
                CHANINT_EN: Allow channel interrupt signal to the AIOP's
                            Interrupt Channel Register.
Return:   void
Comments: If an interrupt enable flag is set in Flags, that interrupt will be
          enabled.  If an interrupt enable flag is not set in Flags, that
          interrupt will not be changed.  Interrupts can be disabled with
          function sDisInterrupts().

          This function sets the appropriate bit for the channel in the AIOP's
          Interrupt Mask Register if the CHANINT_EN flag is set.  This allows
          this channel's bit to be set in the AIOP's Interrupt Channel Register.

          Interrupts must also be globally enabled before channel interrupts
          will be passed on to the host.  This is done with function
          sEnGlobalInt().

          In some cases it may be desirable to disable interrupts globally but
          enable channel interrupts.  This would allow the global interrupt
          status register to be used to determine which AIOPs need service.
*/
void sEnInterrupts(CHANNEL_T *ChP,Word_t Flags)
{
   Byte_t Mask;                 /* Interrupt Mask Register */

   ChP->RxControl[2] |=
      ((Byte_t)Flags & (RXINT_EN | SRCINT_EN | MCINT_EN));

   sOutDW(ChP->IndexAddr,*(DWord_t *)&ChP->RxControl[0]);

   ChP->TxControl[2] |= ((Byte_t)Flags & TXINT_EN);

   sOutDW(ChP->IndexAddr,*(DWord_t *)&ChP->TxControl[0]);

   if(Flags & CHANINT_EN)
   {
      Mask = sInB(ChP->IntMask) | sBitMapSetTbl[ChP->ChanNum];
      sOutB(ChP->IntMask,Mask);
   }
}

/***************************************************************************
Function: sDisInterrupts
Purpose:  Disable one or more interrupts for a channel
Call:     sDisInterrupts(ChP,Flags)
          CHANNEL_T *ChP; Ptr to channel structure
          Word_t Flags: Interrupt flags, can be any combination
             of the following flags:
                TXINT_EN:   Interrupt on Tx FIFO empty
                RXINT_EN:   Interrupt on Rx FIFO at trigger level (see
                            sSetRxTrigger())
                SRCINT_EN:  Interrupt on SRC (Special Rx Condition)
                MCINT_EN:   Interrupt on modem input change
                CHANINT_EN: Disable channel interrupt signal to the
                            AIOP's Interrupt Channel Register.
Return:   void
Comments: If an interrupt flag is set in Flags, that interrupt will be
          disabled.  If an interrupt flag is not set in Flags, that
          interrupt will not be changed.  Interrupts can be enabled with
          function sEnInterrupts().

          This function clears the appropriate bit for the channel in the AIOP's
          Interrupt Mask Register if the CHANINT_EN flag is set.  This blocks
          this channel's bit from being set in the AIOP's Interrupt Channel
          Register.
*/
void sDisInterrupts(CHANNEL_T *ChP,Word_t Flags)
{
   Byte_t Mask;                 /* Interrupt Mask Register */

   ChP->RxControl[2] &=
         ~((Byte_t)Flags & (RXINT_EN | SRCINT_EN | MCINT_EN));
   sOutDW(ChP->IndexAddr,*(DWord_t *)&ChP->RxControl[0]);
   ChP->TxControl[2] &= ~((Byte_t)Flags & TXINT_EN);
   sOutDW(ChP->IndexAddr,*(DWord_t *)&ChP->TxControl[0]);

   if(Flags & CHANINT_EN)
   {
      Mask = sInB(ChP->IntMask) & sBitMapClrTbl[ChP->ChanNum];
      sOutB(ChP->IntMask,Mask);
   }
}
