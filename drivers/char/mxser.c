/*****************************************************************************/
/*
 *          mxser.c  -- MOXA Smartio family multiport serial driver.
 *
 *      Copyright (C) 1999-2000  Moxa Technologies (support@moxa.com.tw).
 *
 *      This code is loosely based on the Linux serial driver, written by
 *      Linus Torvalds, Theodore T'so and others.
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*
 *    MOXA Smartio Family Serial Driver
 *
 *      Copyright (C) 1999,2000  Moxa Technologies Co., LTD.
 *
 *      for             : LINUX 2.0.X, 2.2.X, 2.4.X
 *      date            : 2001/05/01
 *      version         : 1.2 
 *      
 *    Fixes for C104H/PCI by Tim Hockin <thockin@sun.com>
 *    Added support for: C102, CI-132, CI-134, CP-132, CP-114, CT-114 cards
 *                        by Damian Wrobel <dwrobel@ertel.com.pl>
 *
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/errno.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/serial.h>
#include <linux/serial_reg.h>
#include <linux/major.h>
#include <linux/string.h>
#include <linux/fcntl.h>
#include <linux/ptrace.h>
#include <linux/ioport.h>
#include <linux/mm.h>
#include <linux/smp_lock.h>
#include <linux/pci.h>

#include <asm/system.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/segment.h>
#include <asm/bitops.h>
#include <asm/uaccess.h>

#define		MXSER_VERSION			"1.2.1"

#define		MXSERMAJOR	 	174
#define		MXSERCUMAJOR		175


#define	MXSER_EVENT_TXLOW	 1
#define	MXSER_EVENT_HANGUP	 2


#define 	SERIAL_DO_RESTART

#define 	MXSER_BOARDS		4	/* Max. boards */
#define 	MXSER_PORTS		32	/* Max. ports */
#define 	MXSER_PORTS_PER_BOARD	8	/* Max. ports per board */
#define 	MXSER_ISR_PASS_LIMIT	256

#define		MXSER_ERR_IOADDR	-1
#define		MXSER_ERR_IRQ		-2
#define		MXSER_ERR_IRQ_CONFLIT	-3
#define		MXSER_ERR_VECTOR	-4

#define 	SERIAL_TYPE_NORMAL	1
#define 	SERIAL_TYPE_CALLOUT	2

#define 	WAKEUP_CHARS		256

#define 	UART_MCR_AFE		0x20
#define 	UART_LSR_SPECIAL	0x1E

#define PORTNO(x)		(MINOR((x)->device) - (x)->driver.minor_start)

#define RELEVANT_IFLAG(iflag)	(iflag & (IGNBRK|BRKINT|IGNPAR|PARMRK|INPCK))

#define IRQ_T(info) ((info->flags & ASYNC_SHARE_IRQ) ? SA_SHIRQ : SA_INTERRUPT)

#ifndef MIN
#define MIN(a,b)	((a) < (b) ? (a) : (b))
#endif

/*
 *    Define the Moxa PCI vendor and device IDs.
 */

#ifndef	PCI_VENDOR_ID_MOXA
#define	PCI_VENDOR_ID_MOXA	0x1393
#endif
#ifndef PCI_DEVICE_ID_C168
#define PCI_DEVICE_ID_C168	0x1680
#endif
#ifndef PCI_DEVICE_ID_C104
#define PCI_DEVICE_ID_C104	0x1040
#endif
#ifndef PCI_DEVICE_ID_CP132
#define PCI_DEVICE_ID_CP132	0x1320
#endif
#ifndef PCI_DEVICE_ID_CP114
#define PCI_DEVICE_ID_CP114	0x1141
#endif
#ifndef PCI_DEVICE_ID_CT114
#define PCI_DEVICE_ID_CT114	0x1140
#endif

#define C168_ASIC_ID    1
#define C104_ASIC_ID    2
#define CI134_ASIC_ID   3
#define CI132_ASIC_ID   4
#define CI104J_ASIC_ID  5
#define C102_ASIC_ID	0xB

enum {
	MXSER_BOARD_C168_ISA = 0,
	MXSER_BOARD_C104_ISA,
	MXSER_BOARD_CI104J,
	MXSER_BOARD_C168_PCI,
	MXSER_BOARD_C104_PCI,
	MXSER_BOARD_C102_ISA,
	MXSER_BOARD_CI132,
	MXSER_BOARD_CI134,
	MXSER_BOARD_CP132_PCI,
	MXSER_BOARD_CP114_PCI,
	MXSER_BOARD_CT114_PCI
};

static char *mxser_brdname[] =
{
	"C168 series",
	"C104 series",
	"CI-104J series",
	"C168H/PCI series",
	"C104H/PCI series",
	"C102 series",
	"CI-132 series",
	"CI-134 series",
	"CP-132 series",
	"CP-114 series",
	"CT-114 series"
};

static int mxser_numports[] =
{
	8,
	4,
	4,
	8,
	4,
	2,
	2,
	4,
	2,
	4,
	4
};

/*
 *    MOXA ioctls
 */
#define 	MOXA		0x400
#define 	MOXA_GETDATACOUNT     (MOXA + 23)
#define		MOXA_GET_CONF         (MOXA + 35)
#define 	MOXA_DIAGNOSE         (MOXA + 50)
#define 	MOXA_CHKPORTENABLE    (MOXA + 60)
#define 	MOXA_HighSpeedOn      (MOXA + 61)
#define         MOXA_GET_MAJOR        (MOXA + 63)
#define         MOXA_GET_CUMAJOR      (MOXA + 64)
#define         MOXA_GETMSTATUS       (MOXA + 65)

static struct pci_device_id mxser_pcibrds[] = {
	{ PCI_VENDOR_ID_MOXA, PCI_DEVICE_ID_C168, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 
	  MXSER_BOARD_C168_PCI },
	{ PCI_VENDOR_ID_MOXA, PCI_DEVICE_ID_C104, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 
	  MXSER_BOARD_C104_PCI },
	{ PCI_VENDOR_ID_MOXA, PCI_DEVICE_ID_CP132, PCI_ANY_ID, PCI_ANY_ID, 0, 0,
	  MXSER_BOARD_CP132_PCI },
	{ PCI_VENDOR_ID_MOXA, PCI_DEVICE_ID_CP114, PCI_ANY_ID, PCI_ANY_ID, 0, 0,
	  MXSER_BOARD_CP114_PCI },
	{ PCI_VENDOR_ID_MOXA, PCI_DEVICE_ID_CT114, PCI_ANY_ID, PCI_ANY_ID, 0, 0,
	  MXSER_BOARD_CT114_PCI },
	{ 0 }
};
MODULE_DEVICE_TABLE(pci, mxser_pcibrds);

static int ioaddr[MXSER_BOARDS];
static int ttymajor = MXSERMAJOR;
static int calloutmajor = MXSERCUMAJOR;
static int verbose;

/* Variables for insmod */

MODULE_AUTHOR("William Chen");
MODULE_DESCRIPTION("MOXA Smartio Family Multiport Board Device Driver");
MODULE_LICENSE("GPL");
MODULE_PARM(ioaddr, "1-4i");
MODULE_PARM(ttymajor, "i");
MODULE_PARM(calloutmajor, "i");
MODULE_PARM(verbose, "i");

EXPORT_NO_SYMBOLS;

struct mxser_hwconf {
	int board_type;
	int ports;
	int irq;
	int vector;
	int vector_mask;
	int uart_type;
	int ioaddr[MXSER_PORTS_PER_BOARD];
	int baud_base[MXSER_PORTS_PER_BOARD];
	struct pci_dev *pdev;
};

struct mxser_struct {
	int port;
	int base;		/* port base address */
	int irq;		/* port using irq no. */
	int vector;		/* port irq vector */
	int vectormask;		/* port vector mask */
	int rx_trigger;		/* Rx fifo trigger level */
	int baud_base;		/* max. speed */
	int flags;		/* defined in tty.h */
	int type;		/* UART type */
	struct tty_struct *tty;
	int read_status_mask;
	int ignore_status_mask;
	int xmit_fifo_size;
	int custom_divisor;
	int x_char;		/* xon/xoff character */
	int close_delay;
	unsigned short closing_wait;
	int IER;		/* Interrupt Enable Register */
	int MCR;		/* Modem control register */
	unsigned long event;
	int count;		/* # of fd on device */
	int blocked_open;	/* # of blocked opens */
	long session;		/* Session of opening process */
	long pgrp;		/* pgrp of opening process */
	unsigned char *xmit_buf;
	int xmit_head;
	int xmit_tail;
	int xmit_cnt;
	struct tq_struct tqueue;
	struct termios normal_termios;
	struct termios callout_termios;
	wait_queue_head_t open_wait;
	wait_queue_head_t close_wait;
	wait_queue_head_t delta_msr_wait;
	struct async_icount icount;	/* kernel counters for the 4 input interrupts */
};

struct mxser_log {
	int tick;
	int rxcnt[MXSER_PORTS];
	int txcnt[MXSER_PORTS];
};

struct mxser_mstatus {
	tcflag_t cflag;
	int cts;
	int dsr;
	int ri;
	int dcd;
};

static struct mxser_mstatus GMStatus[MXSER_PORTS];

static int mxserBoardCAP[MXSER_BOARDS] =
{
	0, 0, 0, 0
       /*  0x180, 0x280, 0x200, 0x320   */
};


static struct tty_driver mxvar_sdriver, mxvar_cdriver;
static int mxvar_refcount;
static struct mxser_struct mxvar_table[MXSER_PORTS];
static struct tty_struct *mxvar_tty[MXSER_PORTS + 1];
static struct termios *mxvar_termios[MXSER_PORTS + 1];
static struct termios *mxvar_termios_locked[MXSER_PORTS + 1];
static struct mxser_log mxvar_log;
static int mxvar_diagflag;
/*
 * mxvar_tmp_buf is used as a temporary buffer by serial_write. We need
 * to lock it in case the memcpy_fromfs blocks while swapping in a page,
 * and some other program tries to do a serial write at the same time.
 * Since the lock will only come under contention when the system is
 * swapping and available memory is low, it makes sense to share one
 * buffer across all the serial ports, since it significantly saves
 * memory if large numbers of serial ports are open.
 */
static unsigned char *mxvar_tmp_buf;
static struct semaphore mxvar_tmp_buf_sem;

/*
 * This is used to figure out the divisor speeds and the timeouts
 */
static int mxvar_baud_table[] =
{
	0, 50, 75, 110, 134, 150, 200, 300, 600, 1200, 1800, 2400, 4800,
	9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600, 0};

struct mxser_hwconf mxsercfg[MXSER_BOARDS];

/*
 * static functions:
 */

#ifdef MODULE
int init_module(void);
void cleanup_module(void);
#endif

static void mxser_getcfg(int board, struct mxser_hwconf *hwconf);
int mxser_init(void);
static int mxser_get_ISA_conf(int, struct mxser_hwconf *);
static int mxser_get_PCI_conf(struct pci_dev *, int, struct mxser_hwconf *);
static void mxser_do_softint(void *);
static int mxser_open(struct tty_struct *, struct file *);
static void mxser_close(struct tty_struct *, struct file *);
static int mxser_write(struct tty_struct *, int, const unsigned char *, int);
static int mxser_write_room(struct tty_struct *);
static void mxser_flush_buffer(struct tty_struct *);
static int mxser_chars_in_buffer(struct tty_struct *);
static void mxser_flush_chars(struct tty_struct *);
static void mxser_put_char(struct tty_struct *, unsigned char);
static int mxser_ioctl(struct tty_struct *, struct file *, uint, ulong);
static int mxser_ioctl_special(unsigned int, unsigned long);
static void mxser_throttle(struct tty_struct *);
static void mxser_unthrottle(struct tty_struct *);
static void mxser_set_termios(struct tty_struct *, struct termios *);
static void mxser_stop(struct tty_struct *);
static void mxser_start(struct tty_struct *);
static void mxser_hangup(struct tty_struct *);
static void mxser_interrupt(int, void *, struct pt_regs *);
static inline void mxser_receive_chars(struct mxser_struct *, int *);
static inline void mxser_transmit_chars(struct mxser_struct *);
static inline void mxser_check_modem_status(struct mxser_struct *, int);
static int mxser_block_til_ready(struct tty_struct *, struct file *, struct mxser_struct *);
static int mxser_startup(struct mxser_struct *);
static void mxser_shutdown(struct mxser_struct *);
static int mxser_change_speed(struct mxser_struct *, struct termios *old_termios);
static int mxser_get_serial_info(struct mxser_struct *, struct serial_struct *);
static int mxser_set_serial_info(struct mxser_struct *, struct serial_struct *);
static int mxser_get_lsr_info(struct mxser_struct *, unsigned int *);
static void mxser_send_break(struct mxser_struct *, int);
static int mxser_get_modem_info(struct mxser_struct *, unsigned int *);
static int mxser_set_modem_info(struct mxser_struct *, unsigned int, unsigned int *);

/*
 * The MOXA C168/C104 serial driver boot-time initialization code!
 */


#ifdef MODULE
int init_module(void)
{
	int ret;

	if (verbose)
		printk("Loading module mxser ...\n");
	ret = mxser_init();
	if (verbose)
		printk("Done.\n");
	return (ret);
}

void cleanup_module(void)
{
	int i, err = 0;


	if (verbose)
		printk("Unloading module mxser ...\n");
	if ((err |= tty_unregister_driver(&mxvar_cdriver)))
		printk("Couldn't unregister MOXA Smartio family callout driver\n");
	if ((err |= tty_unregister_driver(&mxvar_sdriver)))
		printk("Couldn't unregister MOXA Smartio family serial driver\n");

	for (i = 0; i < MXSER_BOARDS; i++) {
		if (mxsercfg[i].board_type == -1)
			continue;
		else {
			free_irq(mxsercfg[i].irq, &mxvar_table[i * MXSER_PORTS_PER_BOARD]);
		}
	}

	if (verbose)
		printk("Done.\n");

}
#endif


int mxser_initbrd(int board, struct mxser_hwconf *hwconf)
{
	struct mxser_struct *info;
	unsigned long flags;
	int retval;
	int i, n;

	init_MUTEX(&mxvar_tmp_buf_sem);
	
	n = board * MXSER_PORTS_PER_BOARD;
	info = &mxvar_table[n];
	for (i = 0; i < hwconf->ports; i++, n++, info++) {
		if (verbose) {
			printk("        ttyM%d/cum%d at 0x%04x ", n, n, hwconf->ioaddr[i]);
			if (hwconf->baud_base[i] == 115200)
				printk(" max. baud rate up to 115200 bps.\n");
			else
				printk(" max. baud rate up to 921600 bps.\n");
		}
		info->port = n;
		info->base = hwconf->ioaddr[i];
		info->irq = hwconf->irq;
		info->vector = hwconf->vector;
		info->vectormask = hwconf->vector_mask;
		info->rx_trigger = 14;
		info->baud_base = hwconf->baud_base[i];
		info->flags = ASYNC_SHARE_IRQ;
		info->type = hwconf->uart_type;
		if ((info->type == PORT_16450) || (info->type == PORT_8250))
			info->xmit_fifo_size = 1;
		else
			info->xmit_fifo_size = 16;
		info->custom_divisor = hwconf->baud_base[i] * 16;
		info->close_delay = 5 * HZ / 10;
		info->closing_wait = 30 * HZ;
		info->tqueue.routine = mxser_do_softint;
		info->tqueue.data = info;
		info->callout_termios = mxvar_cdriver.init_termios;
		info->normal_termios = mxvar_sdriver.init_termios;
		init_waitqueue_head(&info->open_wait);
		init_waitqueue_head(&info->close_wait);
		init_waitqueue_head(&info->delta_msr_wait);
	}

	/*
	 * Allocate the IRQ if necessary
	 */
	save_flags(flags);

	n = board * MXSER_PORTS_PER_BOARD;
	info = &mxvar_table[n];

	cli();
	retval = request_irq(hwconf->irq, mxser_interrupt, IRQ_T(info),
			     "mxser", info);
	if (retval) {
		restore_flags(flags);
		printk("Board %d: %s", board, mxser_brdname[hwconf->board_type]);
		printk("  Request irq fail,IRQ (%d) may be conflit with another device.\n", info->irq);
		return (retval);
	}
	restore_flags(flags);

	return 0;
}


static void mxser_getcfg(int board, struct mxser_hwconf *hwconf)
{
	mxsercfg[board] = *hwconf;
}

static int mxser_get_PCI_conf(struct pci_dev *pdev, int board_type, struct mxser_hwconf *hwconf)
{
	int i;
	unsigned int ioaddress;

	hwconf->board_type = board_type;
	hwconf->ports = mxser_numports[board_type];
	ioaddress = pci_resource_start (pdev, 2);
	for (i = 0; i < hwconf->ports; i++)
		hwconf->ioaddr[i] = ioaddress + 8 * i;

	ioaddress = pci_resource_start (pdev, 3);
	hwconf->vector = ioaddress;

	hwconf->irq = pdev->irq;

	hwconf->uart_type = PORT_16550A;
	hwconf->vector_mask = 0;
	for (i = 0; i < hwconf->ports; i++) {
		hwconf->vector_mask |= (1 << i);
		hwconf->baud_base[i] = 921600;
	}
	return (0);
}

int mxser_init(void)
{
	int i, m, retval, b;
	int n, index;
	int ret1, ret2;
	struct mxser_hwconf hwconf;

	printk("MOXA Smartio family driver version %s\n", MXSER_VERSION);

	/* Initialize the tty_driver structure */

	memset(&mxvar_sdriver, 0, sizeof(struct tty_driver));
	mxvar_sdriver.magic = TTY_DRIVER_MAGIC;
	mxvar_sdriver.name = "ttyM";
	mxvar_sdriver.major = ttymajor;
	mxvar_sdriver.minor_start = 0;
	mxvar_sdriver.num = MXSER_PORTS + 1;
	mxvar_sdriver.type = TTY_DRIVER_TYPE_SERIAL;
	mxvar_sdriver.subtype = SERIAL_TYPE_NORMAL;
	mxvar_sdriver.init_termios = tty_std_termios;
	mxvar_sdriver.init_termios.c_cflag = B9600 | CS8 | CREAD | HUPCL | CLOCAL;
	mxvar_sdriver.flags = TTY_DRIVER_REAL_RAW;
	mxvar_sdriver.refcount = &mxvar_refcount;
	mxvar_sdriver.table = mxvar_tty;
	mxvar_sdriver.termios = mxvar_termios;
	mxvar_sdriver.termios_locked = mxvar_termios_locked;

	mxvar_sdriver.open = mxser_open;
	mxvar_sdriver.close = mxser_close;
	mxvar_sdriver.write = mxser_write;
	mxvar_sdriver.put_char = mxser_put_char;
	mxvar_sdriver.flush_chars = mxser_flush_chars;
	mxvar_sdriver.write_room = mxser_write_room;
	mxvar_sdriver.chars_in_buffer = mxser_chars_in_buffer;
	mxvar_sdriver.flush_buffer = mxser_flush_buffer;
	mxvar_sdriver.ioctl = mxser_ioctl;
	mxvar_sdriver.throttle = mxser_throttle;
	mxvar_sdriver.unthrottle = mxser_unthrottle;
	mxvar_sdriver.set_termios = mxser_set_termios;
	mxvar_sdriver.stop = mxser_stop;
	mxvar_sdriver.start = mxser_start;
	mxvar_sdriver.hangup = mxser_hangup;

	/*
	 * The callout device is just like normal device except for
	 * major number and the subtype code.
	 */
	mxvar_cdriver = mxvar_sdriver;
	mxvar_cdriver.name = "cum";
	mxvar_cdriver.major = calloutmajor;
	mxvar_cdriver.subtype = SERIAL_TYPE_CALLOUT;

	printk("Tty devices major number = %d, callout devices major number = %d\n", ttymajor, calloutmajor);

	mxvar_diagflag = 0;
	memset(mxvar_table, 0, MXSER_PORTS * sizeof(struct mxser_struct));
	memset(&mxvar_log, 0, sizeof(struct mxser_log));


	m = 0;
	/* Start finding ISA boards here */
	for (b = 0; b < MXSER_BOARDS && m < MXSER_BOARDS; b++) {
		int cap;
		if (!(cap = mxserBoardCAP[b]))
			continue;

		retval = mxser_get_ISA_conf(cap, &hwconf);

		if (retval != 0)
			printk("Found MOXA %s board (CAP=0x%x)\n",
			       mxser_brdname[hwconf.board_type],
			       ioaddr[b]);

		if (retval <= 0) {
			if (retval == MXSER_ERR_IRQ)
				printk("Invalid interrupt number,board not configured\n");
			else if (retval == MXSER_ERR_IRQ_CONFLIT)
				printk("Invalid interrupt number,board not configured\n");
			else if (retval == MXSER_ERR_VECTOR)
				printk("Invalid interrupt vector,board not configured\n");
			else if (retval == MXSER_ERR_IOADDR)
				printk("Invalid I/O address,board not configured\n");

			continue;
		}
		hwconf.pdev = NULL;

		if (mxser_initbrd(m, &hwconf) < 0)
			continue;

		mxser_getcfg(m, &hwconf);

		m++;
	}

	/* Start finding ISA boards from module arg */
	for (b = 0; b < MXSER_BOARDS && m < MXSER_BOARDS; b++) {
		int cap;
		if (!(cap = ioaddr[b]))
			continue;

		retval = mxser_get_ISA_conf(cap, &hwconf);

		if (retval != 0)
			printk("Found MOXA %s board (CAP=0x%x)\n",
			       mxser_brdname[hwconf.board_type],
			       ioaddr[b]);

		if (retval <= 0) {
			if (retval == MXSER_ERR_IRQ)
				printk("Invalid interrupt number,board not configured\n");
			else if (retval == MXSER_ERR_IRQ_CONFLIT)
				printk("Invalid interrupt number,board not configured\n");
			else if (retval == MXSER_ERR_VECTOR)
				printk("Invalid interrupt vector,board not configured\n");
			else if (retval == MXSER_ERR_IOADDR)
				printk("Invalid I/O address,board not configured\n");

			continue;
		}
		hwconf.pdev = NULL;

		if (mxser_initbrd(m, &hwconf) < 0)
			continue;

		mxser_getcfg(m, &hwconf);

		m++;
	}

	/* start finding PCI board here */

#ifdef CONFIG_PCI
	{
		struct pci_dev *pdev = NULL;

		n = (sizeof(mxser_pcibrds) / sizeof(mxser_pcibrds[0])) - 1;
		index = 0;
		for (b = 0; b < n; b++) {
			while ((pdev = pci_find_device(mxser_pcibrds[b].vendor, mxser_pcibrds[b].device, pdev)) != NULL)
			{
				if (pci_enable_device(pdev))
					continue;
				hwconf.pdev = pdev;
				printk("Found MOXA %s board(BusNo=%d,DevNo=%d)\n",
					mxser_brdname[mxser_pcibrds[b].driver_data],
				pdev->bus->number, PCI_SLOT(pdev->devfn));
				if (m >= MXSER_BOARDS) {
					printk("Too many Smartio family boards found (maximum %d),board not configured\n", MXSER_BOARDS);
				} else {
					retval = mxser_get_PCI_conf(pdev, mxser_pcibrds[b].driver_data, &hwconf);
					if (retval < 0) {
						if (retval == MXSER_ERR_IRQ)
							printk("Invalid interrupt number,board not configured\n");
						else if (retval == MXSER_ERR_IRQ_CONFLIT)
							printk("Invalid interrupt number,board not configured\n");	
						else if (retval == MXSER_ERR_VECTOR)
							printk("Invalid interrupt vector,board not configured\n");
						else if (retval == MXSER_ERR_IOADDR)
							printk("Invalid I/O address,board not configured\n");
						continue;
					}
					if (mxser_initbrd(m, &hwconf) < 0)
						continue;
					mxser_getcfg(m, &hwconf);
					m++;
				}
			}
		}
	}
#endif

	for (i = m; i < MXSER_BOARDS; i++) {
		mxsercfg[i].board_type = -1;
	}


	ret1 = 0;
	ret2 = 0;
	if (!(ret1 = tty_register_driver(&mxvar_sdriver))) {
		if (!(ret2 = tty_register_driver(&mxvar_cdriver))) {
			return 0;
		} else {
			tty_unregister_driver(&mxvar_sdriver);
			printk("Couldn't install MOXA Smartio family callout driver !\n");
		}
	} else
		printk("Couldn't install MOXA Smartio family driver !\n");


	if (ret1 || ret2) {
		for (i = 0; i < MXSER_BOARDS; i++) {
			if (mxsercfg[i].board_type == -1)
				continue;
			else {
				free_irq(mxsercfg[i].irq, &mxvar_table[i * MXSER_PORTS_PER_BOARD]);
			}
		}
		return -1;
	}
	return (0);
}

static void mxser_do_softint(void *private_)
{
	struct mxser_struct *info = (struct mxser_struct *) private_;
	struct tty_struct *tty;

	tty = info->tty;
	if (tty) {
		if (test_and_clear_bit(MXSER_EVENT_TXLOW, &info->event)) {
			if ((tty->flags & (1 << TTY_DO_WRITE_WAKEUP)) &&
			    tty->ldisc.write_wakeup)
				(tty->ldisc.write_wakeup) (tty);
			wake_up_interruptible(&tty->write_wait);
		}
		if (test_and_clear_bit(MXSER_EVENT_HANGUP, &info->event)) {
			tty_hangup(tty);	/* FIXME: module removal race here - AKPM */
		}
	}
	MOD_DEC_USE_COUNT;
}

/*
 * This routine is called whenever a serial port is opened.  It
 * enables interrupts for a serial port, linking in its async structure into
 * the IRQ chain.   It also performs the serial-specific
 * initialization for the tty structure.
 */

static int mxser_open(struct tty_struct *tty, struct file *filp)
{
	struct mxser_struct *info;
	int retval, line;
	unsigned long page;

	line = PORTNO(tty);
	if (line == MXSER_PORTS)
		return (0);
	if ((line < 0) || (line > MXSER_PORTS))
		return (-ENODEV);
	info = mxvar_table + line;
	if (!info->base)
		return (-ENODEV);

	info->count++;
	tty->driver_data = info;
	info->tty = tty;

	if (!mxvar_tmp_buf) {
		page = get_free_page(GFP_KERNEL);
		if (!page)
			return (-ENOMEM);
		if (mxvar_tmp_buf)
			free_page(page);
		else
			mxvar_tmp_buf = (unsigned char *) page;
	}
	/*
	 * Start up serial port
	 */
	retval = mxser_startup(info);
	if (retval)
		return (retval);

	retval = mxser_block_til_ready(tty, filp, info);
	if (retval)
		return (retval);

	if ((info->count == 1) && (info->flags & ASYNC_SPLIT_TERMIOS)) {
		if (tty->driver.subtype == SERIAL_TYPE_NORMAL)
			*tty->termios = info->normal_termios;
		else
			*tty->termios = info->callout_termios;
		mxser_change_speed(info, 0);
	}
	info->session = current->session;
	info->pgrp = current->pgrp;

	MOD_INC_USE_COUNT;

	return (0);
}

/*
 * This routine is called when the serial port gets closed.  First, we
 * wait for the last remaining data to be sent.  Then, we unlink its
 * async structure from the interrupt chain if necessary, and we free
 * that IRQ if nothing is left in the chain.
 */

static void mxser_close(struct tty_struct *tty, struct file *filp)
{
	struct mxser_struct *info = (struct mxser_struct *) tty->driver_data;
	unsigned long flags;
	unsigned long timeout;

	if (PORTNO(tty) == MXSER_PORTS)
		return;
	if (!info)
		return;

	save_flags(flags);
	cli();

	if (tty_hung_up_p(filp)) {
		restore_flags(flags);
		MOD_DEC_USE_COUNT;
		return;
	}
	if ((tty->count == 1) && (info->count != 1)) {
		/*
		 * Uh, oh.        tty->count is 1, which means that the tty
		 * structure will be freed.  Info->count should always
		 * be one in these conditions.  If it's greater than
		 * one, we've got real problems, since it means the
		 * serial port won't be shutdown.
		 */
		printk("mxser_close: bad serial port count; tty->count is 1, "
		       "info->count is %d\n", info->count);
		info->count = 1;
	}
	if (--info->count < 0) {
		printk("mxser_close: bad serial port count for ttys%d: %d\n",
		       info->port, info->count);
		info->count = 0;
	}
	if (info->count) {
		restore_flags(flags);
		MOD_DEC_USE_COUNT;
		return;
	}
	info->flags |= ASYNC_CLOSING;
	/*
	 * Save the termios structure, since this port may have
	 * separate termios for callout and dialin.
	 */
	if (info->flags & ASYNC_NORMAL_ACTIVE)
		info->normal_termios = *tty->termios;
	if (info->flags & ASYNC_CALLOUT_ACTIVE)
		info->callout_termios = *tty->termios;
	/*
	 * Now we wait for the transmit buffer to clear; and we notify
	 * the line discipline to only process XON/XOFF characters.
	 */
	tty->closing = 1;
	if (info->closing_wait != ASYNC_CLOSING_WAIT_NONE)
		tty_wait_until_sent(tty, info->closing_wait);
	/*
	 * At this point we stop accepting input.  To do this, we
	 * disable the receive line status interrupts, and tell the
	 * interrupt driver to stop checking the data ready bit in the
	 * line status register.
	 */
	info->IER &= ~UART_IER_RLSI;
	/* by William
	   info->read_status_mask &= ~UART_LSR_DR;
	 */
	if (info->flags & ASYNC_INITIALIZED) {
		outb(info->IER, info->base + UART_IER);
		/*
		 * Before we drop DTR, make sure the UART transmitter
		 * has completely drained; this is especially
		 * important if there is a transmit FIFO!
		 */
		timeout = jiffies + HZ;
		while (!(inb(info->base + UART_LSR) & UART_LSR_TEMT)) {
			set_current_state(TASK_INTERRUPTIBLE);
			schedule_timeout(5);
			if (jiffies > timeout)
				break;
		}
	}
	mxser_shutdown(info);
	if (tty->driver.flush_buffer)
		tty->driver.flush_buffer(tty);
	if (tty->ldisc.flush_buffer)
		tty->ldisc.flush_buffer(tty);
	tty->closing = 0;
	info->event = 0;
	info->tty = 0;
	if (info->blocked_open) {
		if (info->close_delay) {
			set_current_state(TASK_INTERRUPTIBLE);
			schedule_timeout(info->close_delay);
		}
		wake_up_interruptible(&info->open_wait);
	}
	info->flags &= ~(ASYNC_NORMAL_ACTIVE | ASYNC_CALLOUT_ACTIVE |
			 ASYNC_CLOSING);
	wake_up_interruptible(&info->close_wait);
	restore_flags(flags);

	MOD_DEC_USE_COUNT;
}

static int mxser_write(struct tty_struct *tty, int from_user,
		       const unsigned char *buf, int count)
{
	int c, total = 0;
	struct mxser_struct *info = (struct mxser_struct *) tty->driver_data;
	unsigned long flags;

	if (!tty || !info->xmit_buf || !mxvar_tmp_buf)
		return (0);

	save_flags(flags);
	if (from_user) {
		down(&mxvar_tmp_buf_sem);
		while (1) {
			c = MIN(count, MIN(SERIAL_XMIT_SIZE - info->xmit_cnt - 1,
					   SERIAL_XMIT_SIZE - info->xmit_head));
			if (c <= 0)
				break;

			c -= copy_from_user(mxvar_tmp_buf, buf, c);
			if (!c) {
				if (!total)
					total = -EFAULT;
				break;
			}

			cli();
			c = MIN(c, MIN(SERIAL_XMIT_SIZE - info->xmit_cnt - 1,
				       SERIAL_XMIT_SIZE - info->xmit_head));
			memcpy(info->xmit_buf + info->xmit_head, mxvar_tmp_buf, c);
			info->xmit_head = (info->xmit_head + c) & (SERIAL_XMIT_SIZE - 1);
			info->xmit_cnt += c;
			restore_flags(flags);

			buf += c;
			count -= c;
			total += c;
		}
		up(&mxvar_tmp_buf_sem);
	} else {
		while (1) {
			cli();
			c = MIN(count, MIN(SERIAL_XMIT_SIZE - info->xmit_cnt - 1,
					   SERIAL_XMIT_SIZE - info->xmit_head));
			if (c <= 0) {
				restore_flags(flags);
				break;
			}

			memcpy(info->xmit_buf + info->xmit_head, buf, c);
			info->xmit_head = (info->xmit_head + c) & (SERIAL_XMIT_SIZE - 1);
			info->xmit_cnt += c;
			restore_flags(flags);

			buf += c;
			count -= c;
			total += c;
		}
	}

	cli();
	if (info->xmit_cnt && !tty->stopped && !tty->hw_stopped &&
	    !(info->IER & UART_IER_THRI)) {
		info->IER |= UART_IER_THRI;
		outb(info->IER, info->base + UART_IER);
	}
	restore_flags(flags);
	return (total);
}

static void mxser_put_char(struct tty_struct *tty, unsigned char ch)
{
	struct mxser_struct *info = (struct mxser_struct *) tty->driver_data;
	unsigned long flags;

	if (!tty || !info->xmit_buf)
		return;

	save_flags(flags);
	cli();
	if (info->xmit_cnt >= SERIAL_XMIT_SIZE - 1) {
		restore_flags(flags);
		return;
	}
	info->xmit_buf[info->xmit_head++] = ch;
	info->xmit_head &= SERIAL_XMIT_SIZE - 1;
	info->xmit_cnt++;
	/********************************************** why ??? ***********
	if ( !tty->stopped && !tty->hw_stopped &&
	     !(info->IER & UART_IER_THRI) ) {
	    info->IER |= UART_IER_THRI;
	    outb(info->IER, info->base + UART_IER);
	}
	*****************************************************************/
	restore_flags(flags);
}

static void mxser_flush_chars(struct tty_struct *tty)
{
	struct mxser_struct *info = (struct mxser_struct *) tty->driver_data;
	unsigned long flags;

	if (info->xmit_cnt <= 0 || tty->stopped || tty->hw_stopped ||
	    !info->xmit_buf)
		return;

	save_flags(flags);
	cli();
	info->IER |= UART_IER_THRI;
	outb(info->IER, info->base + UART_IER);
	restore_flags(flags);
}

static int mxser_write_room(struct tty_struct *tty)
{
	struct mxser_struct *info = (struct mxser_struct *) tty->driver_data;
	int ret;

	ret = SERIAL_XMIT_SIZE - info->xmit_cnt - 1;
	if (ret < 0)
		ret = 0;
	return (ret);
}

static int mxser_chars_in_buffer(struct tty_struct *tty)
{
	struct mxser_struct *info = (struct mxser_struct *) tty->driver_data;

	return (info->xmit_cnt);
}

static void mxser_flush_buffer(struct tty_struct *tty)
{
	struct mxser_struct *info = (struct mxser_struct *) tty->driver_data;
	unsigned long flags;

	save_flags(flags);
	cli();
	info->xmit_cnt = info->xmit_head = info->xmit_tail = 0;
	restore_flags(flags);
	wake_up_interruptible(&tty->write_wait);
	if ((tty->flags & (1 << TTY_DO_WRITE_WAKEUP)) &&
	    tty->ldisc.write_wakeup)
		(tty->ldisc.write_wakeup) (tty);
}

static int mxser_ioctl(struct tty_struct *tty, struct file *file,
		       unsigned int cmd, unsigned long arg)
{
	unsigned long flags;
	struct mxser_struct *info = (struct mxser_struct *) tty->driver_data;
	int retval;
	struct async_icount cprev, cnow;	/* kernel counter temps */
	struct serial_icounter_struct *p_cuser;		/* user space */
	unsigned long templ;

	if (PORTNO(tty) == MXSER_PORTS)
		return (mxser_ioctl_special(cmd, arg));
	if ((cmd != TIOCGSERIAL) && (cmd != TIOCMIWAIT) &&
	    (cmd != TIOCGICOUNT)) {
		if (tty->flags & (1 << TTY_IO_ERROR))
			return (-EIO);
	}
	switch (cmd) {
	case TCSBRK:		/* SVID version: non-zero arg --> no break */
		retval = tty_check_change(tty);
		if (retval)
			return (retval);
		tty_wait_until_sent(tty, 0);
		if (!arg)
			mxser_send_break(info, HZ / 4);		/* 1/4 second */
		return (0);
	case TCSBRKP:		/* support for POSIX tcsendbreak() */
		retval = tty_check_change(tty);
		if (retval)
			return (retval);
		tty_wait_until_sent(tty, 0);
		mxser_send_break(info, arg ? arg * (HZ / 10) : HZ / 4);
		return (0);
	case TIOCGSOFTCAR:
		return put_user(C_CLOCAL(tty) ? 1 : 0, (unsigned long *) arg);
	case TIOCSSOFTCAR:
		if(get_user(templ, (unsigned long *) arg))
			return -EFAULT;
		arg = templ;
		tty->termios->c_cflag = ((tty->termios->c_cflag & ~CLOCAL) |
					 (arg ? CLOCAL : 0));
		return (0);
	case TIOCMGET:
		return (mxser_get_modem_info(info, (unsigned int *) arg));
	case TIOCMBIS:
	case TIOCMBIC:
	case TIOCMSET:
		return (mxser_set_modem_info(info, cmd, (unsigned int *) arg));
	case TIOCGSERIAL:
		return (mxser_get_serial_info(info, (struct serial_struct *) arg));
	case TIOCSSERIAL:
		return (mxser_set_serial_info(info, (struct serial_struct *) arg));
	case TIOCSERGETLSR:	/* Get line status register */
		return (mxser_get_lsr_info(info, (unsigned int *) arg));
		/*
		 * Wait for any of the 4 modem inputs (DCD,RI,DSR,CTS) to change
		 * - mask passed in arg for lines of interest
		 *   (use |'ed TIOCM_RNG/DSR/CD/CTS for masking)
		 * Caller should use TIOCGICOUNT to see which one it was
		 */
	case TIOCMIWAIT:
		save_flags(flags);
		cli();
		cprev = info->icount;	/* note the counters on entry */
		restore_flags(flags);
		while (1) {
			interruptible_sleep_on(&info->delta_msr_wait);
			/* see if a signal did it */
			if (signal_pending(current))
				return (-ERESTARTSYS);
			save_flags(flags);
			cli();
			cnow = info->icount;	/* atomic copy */
			restore_flags(flags);
			if (cnow.rng == cprev.rng && cnow.dsr == cprev.dsr &&
			  cnow.dcd == cprev.dcd && cnow.cts == cprev.cts)
				return (-EIO);	/* no change => error */
			if (((arg & TIOCM_RNG) && (cnow.rng != cprev.rng)) ||
			((arg & TIOCM_DSR) && (cnow.dsr != cprev.dsr)) ||
			 ((arg & TIOCM_CD) && (cnow.dcd != cprev.dcd)) ||
			((arg & TIOCM_CTS) && (cnow.cts != cprev.cts))) {
				return (0);
			}
			cprev = cnow;
		}
		/* NOTREACHED */
		/*
		 * Get counter of input serial line interrupts (DCD,RI,DSR,CTS)
		 * Return: write counters to the user passed counter struct
		 * NB: both 1->0 and 0->1 transitions are counted except for
		 *     RI where only 0->1 is counted.
		 */
	case TIOCGICOUNT:
		save_flags(flags);
		cli();
		cnow = info->icount;
		restore_flags(flags);
		p_cuser = (struct serial_icounter_struct *) arg;
		if(put_user(cnow.cts, &p_cuser->cts))
			return -EFAULT;
		if(put_user(cnow.dsr, &p_cuser->dsr))
			return -EFAULT;
		if(put_user(cnow.rng, &p_cuser->rng))
			return -EFAULT;
		return put_user(cnow.dcd, &p_cuser->dcd);
	case MOXA_HighSpeedOn:
		return put_user(info->baud_base != 115200 ? 1 : 0, (int *) arg);
	default:
		return (-ENOIOCTLCMD);
	}
	return (0);
}

static int mxser_ioctl_special(unsigned int cmd, unsigned long arg)
{
	int i, result, status;

	switch (cmd) {
	case MOXA_GET_CONF:
		if(copy_to_user((struct mxser_hwconf *) arg, mxsercfg,
			     sizeof(struct mxser_hwconf) * 4))
			     	return -EFAULT;
		return 0;
	case MOXA_GET_MAJOR:
		if(copy_to_user((int *) arg, &ttymajor, sizeof(int)))
			return -EFAULT;
		return 0;

	case MOXA_GET_CUMAJOR:
		if(copy_to_user((int *) arg, &calloutmajor, sizeof(int)))
			return -EFAULT;
		return 0;

	case MOXA_CHKPORTENABLE:
		result = 0;
		for (i = 0; i < MXSER_PORTS; i++) {
			if (mxvar_table[i].base)
				result |= (1 << i);
		}
		return put_user(result, (unsigned long *) arg);
	case MOXA_GETDATACOUNT:
		if(copy_to_user((struct mxser_log *) arg, &mxvar_log, sizeof(mxvar_log)))
			return -EFAULT;
		return (0);
	case MOXA_GETMSTATUS:
		for (i = 0; i < MXSER_PORTS; i++) {
			GMStatus[i].ri = 0;
			if (!mxvar_table[i].base) {
				GMStatus[i].dcd = 0;
				GMStatus[i].dsr = 0;
				GMStatus[i].cts = 0;
				continue;
			}
			if (!mxvar_table[i].tty || !mxvar_table[i].tty->termios)
				GMStatus[i].cflag = mxvar_table[i].normal_termios.c_cflag;
			else
				GMStatus[i].cflag = mxvar_table[i].tty->termios->c_cflag;

			status = inb(mxvar_table[i].base + UART_MSR);
			if (status & 0x80 /*UART_MSR_DCD */ )
				GMStatus[i].dcd = 1;
			else
				GMStatus[i].dcd = 0;

			if (status & 0x20 /*UART_MSR_DSR */ )
				GMStatus[i].dsr = 1;
			else
				GMStatus[i].dsr = 0;


			if (status & 0x10 /*UART_MSR_CTS */ )
				GMStatus[i].cts = 1;
			else
				GMStatus[i].cts = 0;
		}
		if(copy_to_user((struct mxser_mstatus *) arg, GMStatus,
			     sizeof(struct mxser_mstatus) * MXSER_PORTS))
			return -EFAULT;
		return 0;
	default:
		return (-ENOIOCTLCMD);
	}
	return (0);
}

/*
 * This routine is called by the upper-layer tty layer to signal that
 * incoming characters should be throttled.
 */
static void mxser_throttle(struct tty_struct *tty)
{
	struct mxser_struct *info = (struct mxser_struct *) tty->driver_data;
	unsigned long flags;

	if (I_IXOFF(tty)) {
		info->x_char = STOP_CHAR(tty);
		save_flags(flags);
		cli();
		outb(info->IER, 0);
		info->IER |= UART_IER_THRI;
		outb(info->IER, info->base + UART_IER);		/* force Tx interrupt */
		restore_flags(flags);
	}
	if (info->tty->termios->c_cflag & CRTSCTS) {
		info->MCR &= ~UART_MCR_RTS;
		save_flags(flags);
		cli();
		outb(info->MCR, info->base + UART_MCR);
		restore_flags(flags);
	}
}

static void mxser_unthrottle(struct tty_struct *tty)
{
	struct mxser_struct *info = (struct mxser_struct *) tty->driver_data;
	unsigned long flags;

	if (I_IXOFF(tty)) {
		if (info->x_char)
			info->x_char = 0;
		else {
			info->x_char = START_CHAR(tty);
			save_flags(flags);
			cli();
			outb(info->IER, 0);
			info->IER |= UART_IER_THRI;	/* force Tx interrupt */
			outb(info->IER, info->base + UART_IER);
			restore_flags(flags);
		}
	}
	if (info->tty->termios->c_cflag & CRTSCTS) {
		info->MCR |= UART_MCR_RTS;
		save_flags(flags);
		cli();
		outb(info->MCR, info->base + UART_MCR);
		restore_flags(flags);
	}
}

static void mxser_set_termios(struct tty_struct *tty,
			      struct termios *old_termios)
{
	struct mxser_struct *info = (struct mxser_struct *) tty->driver_data;

/* 8-2-99 by William
   if ( (tty->termios->c_cflag == old_termios->c_cflag) &&
   (RELEVANT_IFLAG(tty->termios->c_iflag) ==
   RELEVANT_IFLAG(old_termios->c_iflag)) )
   return;

   mxser_change_speed(info, old_termios);

   if ( (old_termios->c_cflag & CRTSCTS) &&
   !(tty->termios->c_cflag & CRTSCTS) ) {
   tty->hw_stopped = 0;
   mxser_start(tty);
   }
 */
	if ((tty->termios->c_cflag != old_termios->c_cflag) ||
	    (RELEVANT_IFLAG(tty->termios->c_iflag) !=
	     RELEVANT_IFLAG(old_termios->c_iflag))) {

		mxser_change_speed(info, old_termios);

		if ((old_termios->c_cflag & CRTSCTS) &&
		    !(tty->termios->c_cflag & CRTSCTS)) {
			tty->hw_stopped = 0;
			mxser_start(tty);
		}
	}
/* Handle sw stopped */
	if ((old_termios->c_iflag & IXON) &&
	    !(tty->termios->c_iflag & IXON)) {
		tty->stopped = 0;
		mxser_start(tty);
	}
}

/*
 * mxser_stop() and mxser_start()
 *
 * This routines are called before setting or resetting tty->stopped.
 * They enable or disable transmitter interrupts, as necessary.
 */
static void mxser_stop(struct tty_struct *tty)
{
	struct mxser_struct *info = (struct mxser_struct *) tty->driver_data;
	unsigned long flags;

	save_flags(flags);
	cli();
	if (info->IER & UART_IER_THRI) {
		info->IER &= ~UART_IER_THRI;
		outb(info->IER, info->base + UART_IER);
	}
	restore_flags(flags);
}

static void mxser_start(struct tty_struct *tty)
{
	struct mxser_struct *info = (struct mxser_struct *) tty->driver_data;
	unsigned long flags;

	save_flags(flags);
	cli();
	if (info->xmit_cnt && info->xmit_buf &&
	    !(info->IER & UART_IER_THRI)) {
		info->IER |= UART_IER_THRI;
		outb(info->IER, info->base + UART_IER);
	}
	restore_flags(flags);
}

/*
 * This routine is called by tty_hangup() when a hangup is signaled.
 */
void mxser_hangup(struct tty_struct *tty)
{
	struct mxser_struct *info = (struct mxser_struct *) tty->driver_data;

	mxser_flush_buffer(tty);
	mxser_shutdown(info);
	info->event = 0;
	info->count = 0;
	info->flags &= ~(ASYNC_NORMAL_ACTIVE | ASYNC_CALLOUT_ACTIVE);
	info->tty = 0;
	wake_up_interruptible(&info->open_wait);
}

/*
 * This is the serial driver's generic interrupt routine
 */
static void mxser_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	int status, i;
	struct mxser_struct *info;
	struct mxser_struct *port;
	int max, irqbits, bits, msr;
	int pass_counter = 0;

	port = 0;
	for (i = 0; i < MXSER_BOARDS; i++) {
		if (dev_id == &(mxvar_table[i * MXSER_PORTS_PER_BOARD])) {
			port = dev_id;
			break;
		}
	}

	if (i == MXSER_BOARDS)
		return;
	if (port == 0)
		return;
	max = mxser_numports[mxsercfg[i].board_type];

	while (1) {
		irqbits = inb(port->vector) & port->vectormask;
		if (irqbits == port->vectormask)
			break;
		for (i = 0, bits = 1; i < max; i++, irqbits |= bits, bits <<= 1) {
			if (irqbits == port->vectormask)
				break;
			if (bits & irqbits)
				continue;
			info = port + i;
			if (!info->tty ||
			  (inb(info->base + UART_IIR) & UART_IIR_NO_INT))
				continue;
			status = inb(info->base + UART_LSR) & info->read_status_mask;
			if (status & UART_LSR_DR)
				mxser_receive_chars(info, &status);
			msr = inb(info->base + UART_MSR);
			if (msr & UART_MSR_ANY_DELTA)
				mxser_check_modem_status(info, msr);
			if (status & UART_LSR_THRE) {
/* 8-2-99 by William
   if ( info->x_char || (info->xmit_cnt > 0) )
 */
				mxser_transmit_chars(info);
			}
		}
		if (pass_counter++ > MXSER_ISR_PASS_LIMIT) {
#if 0
			printk("MOXA Smartio/Indusrtio family driver interrupt loop break\n");
#endif
			break;	/* Prevent infinite loops */
		}
	}
}

static inline void mxser_receive_chars(struct mxser_struct *info,
					 int *status)
{
	struct tty_struct *tty = info->tty;
	unsigned char ch;
	int ignored = 0;
	int cnt = 0;

	do {
		ch = inb(info->base + UART_RX);
		if (*status & info->ignore_status_mask) {
			if (++ignored > 100)
				break;
		} else {
			if (tty->flip.count >= TTY_FLIPBUF_SIZE)
				break;
			tty->flip.count++;
			if (*status & UART_LSR_SPECIAL) {
				if (*status & UART_LSR_BI) {
					*tty->flip.flag_buf_ptr++ = TTY_BREAK;
					if (info->flags & ASYNC_SAK)
						do_SAK(tty);
				} else if (*status & UART_LSR_PE) {
					*tty->flip.flag_buf_ptr++ = TTY_PARITY;
				} else if (*status & UART_LSR_FE) {
					*tty->flip.flag_buf_ptr++ = TTY_FRAME;
				} else if (*status & UART_LSR_OE) {
					*tty->flip.flag_buf_ptr++ = TTY_OVERRUN;
				} else
					*tty->flip.flag_buf_ptr++ = 0;
			} else
				*tty->flip.flag_buf_ptr++ = 0;
			*tty->flip.char_buf_ptr++ = ch;
			cnt++;
		}
		*status = inb(info->base + UART_LSR) & info->read_status_mask;
	} while (*status & UART_LSR_DR);
	mxvar_log.rxcnt[info->port] += cnt;
	queue_task(&tty->flip.tqueue, &tq_timer);

}

static inline void mxser_transmit_chars(struct mxser_struct *info)
{
	int count, cnt;

	if (info->x_char) {
		outb(info->x_char, info->base + UART_TX);
		info->x_char = 0;
		mxvar_log.txcnt[info->port]++;
		return;
	}
	if ((info->xmit_cnt <= 0) || info->tty->stopped ||
	    info->tty->hw_stopped) {
		info->IER &= ~UART_IER_THRI;
		outb(info->IER, info->base + UART_IER);
		return;
	}
	cnt = info->xmit_cnt;
	count = info->xmit_fifo_size;
	do {
		outb(info->xmit_buf[info->xmit_tail++], info->base + UART_TX);
		info->xmit_tail = info->xmit_tail & (SERIAL_XMIT_SIZE - 1);
		if (--info->xmit_cnt <= 0)
			break;
	} while (--count > 0);
	mxvar_log.txcnt[info->port] += (cnt - info->xmit_cnt);

	if (info->xmit_cnt < WAKEUP_CHARS) {
		set_bit(MXSER_EVENT_TXLOW, &info->event);
		MOD_INC_USE_COUNT;
		if (schedule_task(&info->tqueue) == 0)
		    MOD_DEC_USE_COUNT;
	}
	if (info->xmit_cnt <= 0) {
		info->IER &= ~UART_IER_THRI;
		outb(info->IER, info->base + UART_IER);
	}
}

static inline void mxser_check_modem_status(struct mxser_struct *info,
					      int status)
{

	/* update input line counters */
	if (status & UART_MSR_TERI)
		info->icount.rng++;
	if (status & UART_MSR_DDSR)
		info->icount.dsr++;
	if (status & UART_MSR_DDCD)
		info->icount.dcd++;
	if (status & UART_MSR_DCTS)
		info->icount.cts++;
	wake_up_interruptible(&info->delta_msr_wait);

	if ((info->flags & ASYNC_CHECK_CD) && (status & UART_MSR_DDCD)) {
		if (status & UART_MSR_DCD)
			wake_up_interruptible(&info->open_wait);
		else if (!((info->flags & ASYNC_CALLOUT_ACTIVE) &&
			   (info->flags & ASYNC_CALLOUT_NOHUP)))
			set_bit(MXSER_EVENT_HANGUP, &info->event);
		MOD_INC_USE_COUNT;
		if (schedule_task(&info->tqueue) == 0)
		    MOD_DEC_USE_COUNT;
	}
	if (info->flags & ASYNC_CTS_FLOW) {
		if (info->tty->hw_stopped) {
			if (status & UART_MSR_CTS) {
				info->tty->hw_stopped = 0;
				info->IER |= UART_IER_THRI;
				outb(info->IER, info->base + UART_IER);

				set_bit(MXSER_EVENT_TXLOW, &info->event);
				MOD_INC_USE_COUNT;
				if (schedule_task(&info->tqueue) == 0)
					MOD_DEC_USE_COUNT;
			}
		} else {
			if (!(status & UART_MSR_CTS)) {
				info->tty->hw_stopped = 1;
				info->IER &= ~UART_IER_THRI;
				outb(info->IER, info->base + UART_IER);
			}
		}
	}
}

static int mxser_block_til_ready(struct tty_struct *tty, struct file *filp,
				 struct mxser_struct *info)
{
	DECLARE_WAITQUEUE(wait, current);
	unsigned long flags;
	int retval;
	int do_clocal = 0;

	/*
	 * If the device is in the middle of being closed, then block
	 * until it's done, and then try again.
	 */
	if (tty_hung_up_p(filp) || (info->flags & ASYNC_CLOSING)) {
		if (info->flags & ASYNC_CLOSING)
			interruptible_sleep_on(&info->close_wait);
#ifdef SERIAL_DO_RESTART
		if (info->flags & ASYNC_HUP_NOTIFY)
			return (-EAGAIN);
		else
			return (-ERESTARTSYS);
#else
		return (-EAGAIN);
#endif
	}
	/*
	 * If this is a callout device, then just make sure the normal
	 * device isn't being used.
	 */
	if (tty->driver.subtype == SERIAL_TYPE_CALLOUT) {
		if (info->flags & ASYNC_NORMAL_ACTIVE)
			return (-EBUSY);
		if ((info->flags & ASYNC_CALLOUT_ACTIVE) &&
		    (info->flags & ASYNC_SESSION_LOCKOUT) &&
		    (info->session != current->session))
			return (-EBUSY);
		if ((info->flags & ASYNC_CALLOUT_ACTIVE) &&
		    (info->flags & ASYNC_PGRP_LOCKOUT) &&
		    (info->pgrp != current->pgrp))
			return (-EBUSY);
		info->flags |= ASYNC_CALLOUT_ACTIVE;
		return (0);
	}
	/*
	 * If non-blocking mode is set, or the port is not enabled,
	 * then make the check up front and then exit.
	 */
	if ((filp->f_flags & O_NONBLOCK) ||
	    (tty->flags & (1 << TTY_IO_ERROR))) {
		if (info->flags & ASYNC_CALLOUT_ACTIVE)
			return (-EBUSY);
		info->flags |= ASYNC_NORMAL_ACTIVE;
		return (0);
	}
	if (info->flags & ASYNC_CALLOUT_ACTIVE) {
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
	 * mxser_close() knows when to free things.  We restore it upon
	 * exit, either normal or abnormal.
	 */
	retval = 0;
	add_wait_queue(&info->open_wait, &wait);
	save_flags(flags);
	cli();
	if (!tty_hung_up_p(filp))
		info->count--;
	restore_flags(flags);
	info->blocked_open++;
	while (1) {
		save_flags(flags);
		cli();
		if (!(info->flags & ASYNC_CALLOUT_ACTIVE))
			outb(inb(info->base + UART_MCR) | UART_MCR_DTR | UART_MCR_RTS,
			     info->base + UART_MCR);
		restore_flags(flags);
		set_current_state(TASK_INTERRUPTIBLE);
		if (tty_hung_up_p(filp) || !(info->flags & ASYNC_INITIALIZED)) {
#ifdef SERIAL_DO_RESTART
			if (info->flags & ASYNC_HUP_NOTIFY)
				retval = -EAGAIN;
			else
				retval = -ERESTARTSYS;
#else
			retval = -EAGAIN;
#endif
			break;
		}
		if (!(info->flags & ASYNC_CALLOUT_ACTIVE) &&
		    !(info->flags & ASYNC_CLOSING) &&
		    (do_clocal || (inb(info->base + UART_MSR) & UART_MSR_DCD)))
			break;
		if (signal_pending(current)) {
			retval = -ERESTARTSYS;
			break;
		}
		schedule();
	}
	set_current_state(TASK_RUNNING);
	remove_wait_queue(&info->open_wait, &wait);
	if (!tty_hung_up_p(filp))
		info->count++;
	info->blocked_open--;
	if (retval)
		return (retval);
	info->flags |= ASYNC_NORMAL_ACTIVE;
	return (0);
}

static int mxser_startup(struct mxser_struct *info)
{
	unsigned long flags;
	unsigned long page;

	page = get_free_page(GFP_KERNEL);
	if (!page)
		return (-ENOMEM);

	save_flags(flags);
	cli();

	if (info->flags & ASYNC_INITIALIZED) {
		free_page(page);
		restore_flags(flags);
		return (0);
	}
	if (!info->base || !info->type) {
		if (info->tty)
			set_bit(TTY_IO_ERROR, &info->tty->flags);
		free_page(page);
		restore_flags(flags);
		return (0);
	}
	if (info->xmit_buf)
		free_page(page);
	else
		info->xmit_buf = (unsigned char *) page;

	/*
	 * Clear the FIFO buffers and disable them
	 * (they will be reenabled in mxser_change_speed())
	 */
	if (info->xmit_fifo_size == 16)
		outb((UART_FCR_CLEAR_RCVR | UART_FCR_CLEAR_XMIT),
		     info->base + UART_FCR);

	/*
	 * At this point there's no way the LSR could still be 0xFF;
	 * if it is, then bail out, because there's likely no UART
	 * here.
	 */
	if (inb(info->base + UART_LSR) == 0xff) {
		restore_flags(flags);
		if (capable(CAP_SYS_ADMIN)) {
			if (info->tty)
				set_bit(TTY_IO_ERROR, &info->tty->flags);
			return (0);
		} else
			return (-ENODEV);
	}
	/*
	 * Clear the interrupt registers.
	 */
	(void) inb(info->base + UART_LSR);
	(void) inb(info->base + UART_RX);
	(void) inb(info->base + UART_IIR);
	(void) inb(info->base + UART_MSR);

	/*
	 * Now, initialize the UART
	 */
	outb(UART_LCR_WLEN8, info->base + UART_LCR);	/* reset DLAB */
	info->MCR = UART_MCR_DTR | UART_MCR_RTS;
	outb(info->MCR, info->base + UART_MCR);

	/*
	 * Finally, enable interrupts
	 */
	info->IER = UART_IER_MSI | UART_IER_RLSI | UART_IER_RDI;
	outb(info->IER, info->base + UART_IER);		/* enable interrupts */

	/*
	 * And clear the interrupt registers again for luck.
	 */
	(void) inb(info->base + UART_LSR);
	(void) inb(info->base + UART_RX);
	(void) inb(info->base + UART_IIR);
	(void) inb(info->base + UART_MSR);

	if (info->tty)
		test_and_clear_bit(TTY_IO_ERROR, &info->tty->flags);

	info->xmit_cnt = info->xmit_head = info->xmit_tail = 0;

	/*
	 * and set the speed of the serial port
	 */
	mxser_change_speed(info, 0);

	info->flags |= ASYNC_INITIALIZED;
	restore_flags(flags);
	return (0);
}

/*
 * This routine will shutdown a serial port; interrupts maybe disabled, and
 * DTR is dropped if the hangup on close termio flag is on.
 */
static void mxser_shutdown(struct mxser_struct *info)
{
	unsigned long flags;

	if (!(info->flags & ASYNC_INITIALIZED))
		return;

	save_flags(flags);
	cli();			/* Disable interrupts */

	/*
	 * clear delta_msr_wait queue to avoid mem leaks: we may free the irq
	 * here so the queue might never be waken up
	 */
	wake_up_interruptible(&info->delta_msr_wait);

	/*
	 * Free the IRQ, if necessary
	 */
	if (info->xmit_buf) {
		free_page((unsigned long) info->xmit_buf);
		info->xmit_buf = 0;
	}
	info->IER = 0;
	outb(0x00, info->base + UART_IER);	/* disable all intrs */

	if (!info->tty || (info->tty->termios->c_cflag & HUPCL))
		info->MCR &= ~(UART_MCR_DTR | UART_MCR_RTS);
	outb(info->MCR, info->base + UART_MCR);

	/* clear Rx/Tx FIFO's */
	outb((UART_FCR_CLEAR_RCVR | UART_FCR_CLEAR_XMIT), info->base + UART_FCR);
	/* read data port to reset things */
	(void) inb(info->base + UART_RX);

	if (info->tty)
		set_bit(TTY_IO_ERROR, &info->tty->flags);

	info->flags &= ~ASYNC_INITIALIZED;
	restore_flags(flags);
}

/*
 * This routine is called to set the UART divisor registers to match
 * the specified baud rate for a serial port.
 */
static int mxser_change_speed(struct mxser_struct *info,
			      struct termios *old_termios)
{
	int quot = 0;
	unsigned cflag, cval, fcr;
	int i;
	int ret = 0;
	unsigned long flags;

	if (!info->tty || !info->tty->termios)
		return ret;
	cflag = info->tty->termios->c_cflag;
	if (!(info->base))
		return ret;

#ifndef B921600
#define B921600 (B460800 +1)
#endif
	switch (cflag & (CBAUD | CBAUDEX)) {
	case B921600:
		i = 20;
		break;
	case B460800:
		i = 19;
		break;
	case B230400:
		i = 18;
		break;
	case B115200:
		i = 17;
		break;
	case B57600:
		i = 16;
		break;
	case B38400:
		i = 15;
		break;
	case B19200:
		i = 14;
		break;
	case B9600:
		i = 13;
		break;
	case B4800:
		i = 12;
		break;
	case B2400:
		i = 11;
		break;
	case B1800:
		i = 10;
		break;
	case B1200:
		i = 9;
		break;
	case B600:
		i = 8;
		break;
	case B300:
		i = 7;
		break;
	case B200:
		i = 6;
		break;
	case B150:
		i = 5;
		break;
	case B134:
		i = 4;
		break;
	case B110:
		i = 3;
		break;
	case B75:
		i = 2;
		break;
	case B50:
		i = 1;
		break;
	default:
		i = 0;
		break;
	}

	if (i == 15) {
		if ((info->flags & ASYNC_SPD_MASK) == ASYNC_SPD_HI)
			i = 16;	/* 57600 bps */
		if ((info->flags & ASYNC_SPD_MASK) == ASYNC_SPD_VHI)
			i = 17;	/* 115200 bps */

#ifdef ASYNC_SPD_SHI
		if ((info->flags & ASYNC_SPD_MASK) == ASYNC_SPD_SHI)
			i = 18;
#endif

#ifdef ASYNC_SPD_WARP
		if ((info->flags & ASYNC_SPD_MASK) == ASYNC_SPD_WARP)
			i = 19;
#endif
	}
	if (mxvar_baud_table[i] == 134) {
		quot = (2 * info->baud_base / 269);
	} else if (mxvar_baud_table[i]) {
		quot = info->baud_base / mxvar_baud_table[i];
		if (!quot && old_termios) {
			/* re-calculate */
			info->tty->termios->c_cflag &= ~CBAUD;
			info->tty->termios->c_cflag |= (old_termios->c_cflag & CBAUD);
			switch (info->tty->termios->c_cflag & (CBAUD | CBAUDEX)) {
			case B921600:
				i = 20;
				break;
			case B460800:
				i = 19;
				break;
			case B230400:
				i = 18;
				break;
			case B115200:
				i = 17;
				break;
			case B57600:
				i = 16;
				break;
			case B38400:
				i = 15;
				break;
			case B19200:
				i = 14;
				break;
			case B9600:
				i = 13;
				break;
			case B4800:
				i = 12;
				break;
			case B2400:
				i = 11;
				break;
			case B1800:
				i = 10;
				break;
			case B1200:
				i = 9;
				break;
			case B600:
				i = 8;
				break;
			case B300:
				i = 7;
				break;
			case B200:
				i = 6;
				break;
			case B150:
				i = 5;
				break;
			case B134:
				i = 4;
				break;
			case B110:
				i = 3;
				break;
			case B75:
				i = 2;
				break;
			case B50:
				i = 1;
				break;
			default:
				i = 0;
				break;
			}
			if (i == 15) {
				if ((info->flags & ASYNC_SPD_MASK) == ASYNC_SPD_HI)
					i = 16;		/* 57600 bps */
				if ((info->flags & ASYNC_SPD_MASK) == ASYNC_SPD_VHI)
					i = 17;		/* 115200 bps */
#ifdef ASYNC_SPD_SHI
				if ((info->flags & ASYNC_SPD_MASK) == ASYNC_SPD_SHI)
					i = 18;
#endif
#ifdef ASYNC_SPD_WARP
				if ((info->flags & ASYNC_SPD_MASK) == ASYNC_SPD_WARP)
					i = 19;
#endif
			}
			if (mxvar_baud_table[i] == 134) {
				quot = (2 * info->baud_base / 269);
			} else if (mxvar_baud_table[i]) {
				quot = info->baud_base / mxvar_baud_table[i];
				if (quot == 0)
					quot = 1;
			} else {
				quot = 0;
			}
		} else if (quot == 0)
			quot = 1;
	} else {
		quot = 0;
	}

	if (quot) {
		info->MCR |= UART_MCR_DTR;
		save_flags(flags);
		cli();
		outb(info->MCR, info->base + UART_MCR);
		restore_flags(flags);
	} else {
		info->MCR &= ~UART_MCR_DTR;
		save_flags(flags);
		cli();
		outb(info->MCR, info->base + UART_MCR);
		restore_flags(flags);
		return ret;
	}
	/* byte size and parity */
	switch (cflag & CSIZE) {
	case CS5:
		cval = 0x00;
		break;
	case CS6:
		cval = 0x01;
		break;
	case CS7:
		cval = 0x02;
		break;
	case CS8:
		cval = 0x03;
		break;
	default:
		cval = 0x00;
		break;		/* too keep GCC shut... */
	}
	if (cflag & CSTOPB)
		cval |= 0x04;
	if (cflag & PARENB)
		cval |= UART_LCR_PARITY;
	if (!(cflag & PARODD))
		cval |= UART_LCR_EPAR;
	if ((info->type == PORT_8250) || (info->type == PORT_16450)) {
		fcr = 0;
	} else {
		fcr = UART_FCR_ENABLE_FIFO;
		switch (info->rx_trigger) {
		case 1:
			fcr |= UART_FCR_TRIGGER_1;
			break;
		case 4:
			fcr |= UART_FCR_TRIGGER_4;
			break;
		case 8:
			fcr |= UART_FCR_TRIGGER_8;
			break;
		default:
			fcr |= UART_FCR_TRIGGER_14;
		}
	}

	/* CTS flow control flag and modem status interrupts */
	info->IER &= ~UART_IER_MSI;
	info->MCR &= ~UART_MCR_AFE;
	if (cflag & CRTSCTS) {
		info->flags |= ASYNC_CTS_FLOW;
		info->IER |= UART_IER_MSI;
		if (info->type == PORT_16550A)
			info->MCR |= UART_MCR_AFE;
	} else {
		info->flags &= ~ASYNC_CTS_FLOW;
	}
	outb(info->MCR, info->base + UART_MCR);
	if (cflag & CLOCAL)
		info->flags &= ~ASYNC_CHECK_CD;
	else {
		info->flags |= ASYNC_CHECK_CD;
		info->IER |= UART_IER_MSI;
	}
	outb(info->IER, info->base + UART_IER);

	/*
	 * Set up parity check flag
	 */
	info->read_status_mask = UART_LSR_OE | UART_LSR_THRE | UART_LSR_DR;
	if (I_INPCK(info->tty))
		info->read_status_mask |= UART_LSR_FE | UART_LSR_PE;
	if (I_BRKINT(info->tty) || I_PARMRK(info->tty))
		info->read_status_mask |= UART_LSR_BI;

	info->ignore_status_mask = 0;
#if 0
	/* This should be safe, but for some broken bits of hardware... */
	if (I_IGNPAR(info->tty)) {
		info->ignore_status_mask |= UART_LSR_PE | UART_LSR_FE;
		info->read_status_mask |= UART_LSR_PE | UART_LSR_FE;
	}
#endif
	if (I_IGNBRK(info->tty)) {
		info->ignore_status_mask |= UART_LSR_BI;
		info->read_status_mask |= UART_LSR_BI;
		/*
		 * If we're ignore parity and break indicators, ignore
		 * overruns too.  (For real raw support).
		 */
		if (I_IGNPAR(info->tty)) {
			info->ignore_status_mask |= UART_LSR_OE | UART_LSR_PE | UART_LSR_FE;
			info->read_status_mask |= UART_LSR_OE | UART_LSR_PE | UART_LSR_FE;
		}
	}
	save_flags(flags);
	cli();
	outb(cval | UART_LCR_DLAB, info->base + UART_LCR);	/* set DLAB */
	outb(quot & 0xff, info->base + UART_DLL);	/* LS of divisor */
	outb(quot >> 8, info->base + UART_DLM);		/* MS of divisor */
	outb(cval, info->base + UART_LCR);	/* reset DLAB */
	outb(fcr, info->base + UART_FCR);	/* set fcr */
	restore_flags(flags);

	return ret;
}

/*
 * ------------------------------------------------------------
 * friends of mxser_ioctl()
 * ------------------------------------------------------------
 */
static int mxser_get_serial_info(struct mxser_struct *info,
				 struct serial_struct *retinfo)
{
	struct serial_struct tmp;

	if (!retinfo)
		return (-EFAULT);
	memset(&tmp, 0, sizeof(tmp));
	tmp.type = info->type;
	tmp.line = info->port;
	tmp.port = info->base;
	tmp.irq = info->irq;
	tmp.flags = info->flags;
	tmp.baud_base = info->baud_base;
	tmp.close_delay = info->close_delay;
	tmp.closing_wait = info->closing_wait;
	tmp.custom_divisor = info->custom_divisor;
	tmp.hub6 = 0;
	return copy_to_user(retinfo, &tmp, sizeof(*retinfo)) ? -EFAULT : 0;
}

static int mxser_set_serial_info(struct mxser_struct *info,
				 struct serial_struct *new_info)
{
	struct serial_struct new_serial;
	unsigned int flags;
	int retval = 0;

	if (!new_info || !info->base)
		return (-EFAULT);
	if (copy_from_user(&new_serial, new_info, sizeof(new_serial)))
		return -EFAULT;

	if ((new_serial.irq != info->irq) ||
	    (new_serial.port != info->base) ||
	    (new_serial.type != info->type) ||
	    (new_serial.custom_divisor != info->custom_divisor) ||
	    (new_serial.baud_base != info->baud_base))
		return (-EPERM);

	flags = info->flags & ASYNC_SPD_MASK;

	if (!suser()) {
		if ((new_serial.baud_base != info->baud_base) ||
		    (new_serial.close_delay != info->close_delay) ||
		    ((new_serial.flags & ~ASYNC_USR_MASK) !=
		     (info->flags & ~ASYNC_USR_MASK)))
			return (-EPERM);
		info->flags = ((info->flags & ~ASYNC_USR_MASK) |
			       (new_serial.flags & ASYNC_USR_MASK));
	} else {
		/*
		 * OK, past this point, all the error checking has been done.
		 * At this point, we start making changes.....
		 */
		info->flags = ((info->flags & ~ASYNC_FLAGS) |
			       (new_serial.flags & ASYNC_FLAGS));
		info->close_delay = new_serial.close_delay * HZ / 100;
		info->closing_wait = new_serial.closing_wait * HZ / 100;
	}

	if (info->flags & ASYNC_INITIALIZED) {
		if (flags != (info->flags & ASYNC_SPD_MASK)) {
			mxser_change_speed(info, 0);
		}
	} else
		retval = mxser_startup(info);
	return (retval);
}

/*
 * mxser_get_lsr_info - get line status register info
 *
 * Purpose: Let user call ioctl() to get info when the UART physically
 *          is emptied.  On bus types like RS485, the transmitter must
 *          release the bus after transmitting. This must be done when
 *          the transmit shift register is empty, not be done when the
 *          transmit holding register is empty.  This functionality
 *          allows an RS485 driver to be written in user space.
 */
static int mxser_get_lsr_info(struct mxser_struct *info, unsigned int *value)
{
	unsigned char status;
	unsigned int result;
	unsigned long flags;

	save_flags(flags);
	cli();
	status = inb(info->base + UART_LSR);
	restore_flags(flags);
	result = ((status & UART_LSR_TEMT) ? TIOCSER_TEMT : 0);
	return put_user(result, value);
}

/*
 * This routine sends a break character out the serial port.
 */
static void mxser_send_break(struct mxser_struct *info, int duration)
{
	unsigned long flags;
	if (!info->base)
		return;
	set_current_state(TASK_INTERRUPTIBLE);
	save_flags(flags);
	cli();
	outb(inb(info->base + UART_LCR) | UART_LCR_SBC, info->base + UART_LCR);
	schedule_timeout(duration);
	outb(inb(info->base + UART_LCR) & ~UART_LCR_SBC, info->base + UART_LCR);
	restore_flags(flags);
}

static int mxser_get_modem_info(struct mxser_struct *info,
				unsigned int *value)
{
	unsigned char control, status;
	unsigned int result;
	unsigned long flags;

	control = info->MCR;
	save_flags(flags);
	cli();
	status = inb(info->base + UART_MSR);
	if (status & UART_MSR_ANY_DELTA)
		mxser_check_modem_status(info, status);
	restore_flags(flags);
	result = ((control & UART_MCR_RTS) ? TIOCM_RTS : 0) |
	    ((control & UART_MCR_DTR) ? TIOCM_DTR : 0) |
	    ((status & UART_MSR_DCD) ? TIOCM_CAR : 0) |
	    ((status & UART_MSR_RI) ? TIOCM_RNG : 0) |
	    ((status & UART_MSR_DSR) ? TIOCM_DSR : 0) |
	    ((status & UART_MSR_CTS) ? TIOCM_CTS : 0);
	return put_user(result, value);
}

static int mxser_set_modem_info(struct mxser_struct *info, unsigned int cmd,
				unsigned int *value)
{
	unsigned int arg;
	unsigned long flags;

	if(get_user(arg, value))
		return -EFAULT;
	switch (cmd) {
	case TIOCMBIS:
		if (arg & TIOCM_RTS)
			info->MCR |= UART_MCR_RTS;
		if (arg & TIOCM_DTR)
			info->MCR |= UART_MCR_DTR;
		break;
	case TIOCMBIC:
		if (arg & TIOCM_RTS)
			info->MCR &= ~UART_MCR_RTS;
		if (arg & TIOCM_DTR)
			info->MCR &= ~UART_MCR_DTR;
		break;
	case TIOCMSET:
		info->MCR = ((info->MCR & ~(UART_MCR_RTS | UART_MCR_DTR)) |
			     ((arg & TIOCM_RTS) ? UART_MCR_RTS : 0) |
			     ((arg & TIOCM_DTR) ? UART_MCR_DTR : 0));
		break;
	default:
		return (-EINVAL);
	}
	save_flags(flags);
	cli();
	outb(info->MCR, info->base + UART_MCR);
	restore_flags(flags);
	return (0);
}

static int mxser_read_register(int, unsigned short *);
static int mxser_program_mode(int);
static void mxser_normal_mode(int);

static int mxser_get_ISA_conf(int cap, struct mxser_hwconf *hwconf)
{
	int id, i, bits;
	unsigned short regs[16], irq;
	unsigned char scratch, scratch2;

	id = mxser_read_register(cap, regs);
	if (id == C168_ASIC_ID)
		hwconf->board_type = MXSER_BOARD_C168_ISA;
	else if (id == C104_ASIC_ID)
		hwconf->board_type = MXSER_BOARD_C104_ISA;
	else if (id == C102_ASIC_ID)
		hwconf->board_type = MXSER_BOARD_C102_ISA;
	else if (id == CI132_ASIC_ID)
		hwconf->board_type = MXSER_BOARD_CI132;
	else if (id == CI134_ASIC_ID)
		hwconf->board_type = MXSER_BOARD_CI134;
	else if (id == CI104J_ASIC_ID)
		hwconf->board_type = MXSER_BOARD_CI104J;
	else
		return (0);
	irq = regs[9] & 0x0F;
	irq = irq | (irq << 4);
	irq = irq | (irq << 8);
	if ((irq != regs[9]) || ((id == 1) && (irq != regs[10]))) {
		return (MXSER_ERR_IRQ_CONFLIT);
	}
	if (!irq) {
		return (MXSER_ERR_IRQ);
	}
	for (i = 0; i < 8; i++)
		hwconf->ioaddr[i] = (int) regs[i + 1] & 0xFFF8;
	hwconf->irq = (int) (irq & 0x0F);
	if ((regs[12] & 0x80) == 0) {
		return (MXSER_ERR_VECTOR);
	}
	hwconf->vector = (int) regs[11];	/* interrupt vector */
	if (id == 1)
		hwconf->vector_mask = 0x00FF;
	else
		hwconf->vector_mask = 0x000F;
	for (i = 7, bits = 0x0100; i >= 0; i--, bits <<= 1) {
		if (regs[12] & bits)
			hwconf->baud_base[i] = 921600;
		else
			hwconf->baud_base[i] = 115200;
	}
	scratch2 = inb(cap + UART_LCR) & (~UART_LCR_DLAB);
	outb(scratch2 | UART_LCR_DLAB, cap + UART_LCR);
	outb(0, cap + UART_EFR);	/* EFR is the same as FCR */
	outb(scratch2, cap + UART_LCR);
	outb(UART_FCR_ENABLE_FIFO, cap + UART_FCR);
	scratch = inb(cap + UART_IIR);
	if (scratch & 0xC0)
		hwconf->uart_type = PORT_16550A;
	else
		hwconf->uart_type = PORT_16450;
	if (id == 1)
		hwconf->ports = 8;
	else
		hwconf->ports = 4;
	return (hwconf->ports);
}

#define CHIP_SK 	0x01	/* Serial Data Clock  in Eprom */
#define CHIP_DO 	0x02	/* Serial Data Output in Eprom */
#define CHIP_CS 	0x04	/* Serial Chip Select in Eprom */
#define CHIP_DI 	0x08	/* Serial Data Input  in Eprom */
#define EN_CCMD 	0x000	/* Chip's command register     */
#define EN0_RSARLO	0x008	/* Remote start address reg 0  */
#define EN0_RSARHI	0x009	/* Remote start address reg 1  */
#define EN0_RCNTLO	0x00A	/* Remote byte count reg WR    */
#define EN0_RCNTHI	0x00B	/* Remote byte count reg WR    */
#define EN0_DCFG	0x00E	/* Data configuration reg WR   */
#define EN0_PORT	0x010	/* Rcv missed frame error counter RD */
#define ENC_PAGE0	0x000	/* Select page 0 of chip registers   */
#define ENC_PAGE3	0x0C0	/* Select page 3 of chip registers   */
static int mxser_read_register(int port, unsigned short *regs)
{
	int i, k, value, id;
	unsigned int j;

	id = mxser_program_mode(port);
	if (id < 0)
		return (id);
	for (i = 0; i < 14; i++) {
		k = (i & 0x3F) | 0x180;
		for (j = 0x100; j > 0; j >>= 1) {
			outb(CHIP_CS, port);
			if (k & j) {
				outb(CHIP_CS | CHIP_DO, port);
				outb(CHIP_CS | CHIP_DO | CHIP_SK, port);	/* A? bit of read */
			} else {
				outb(CHIP_CS, port);
				outb(CHIP_CS | CHIP_SK, port);	/* A? bit of read */
			}
		}
		(void) inb(port);
		value = 0;
		for (k = 0, j = 0x8000; k < 16; k++, j >>= 1) {
			outb(CHIP_CS, port);
			outb(CHIP_CS | CHIP_SK, port);
			if (inb(port) & CHIP_DI)
				value |= j;
		}
		regs[i] = value;
		outb(0, port);
	}
	mxser_normal_mode(port);
	return (id);
}

static int mxser_program_mode(int port)
{
	int id, i, j, n;
	unsigned long flags;

	save_flags(flags);
	cli();
	outb(0, port);
	outb(0, port);
	outb(0, port);
	(void) inb(port);
	(void) inb(port);
	outb(0, port);
	(void) inb(port);
	restore_flags(flags);
	id = inb(port + 1) & 0x1F;
	if ((id != C168_ASIC_ID) && (id != C104_ASIC_ID) && (id != CI104J_ASIC_ID) &&
		(id != C102_ASIC_ID) &&	(id != CI132_ASIC_ID) && (id != CI134_ASIC_ID))
		return (-1);
	for (i = 0, j = 0; i < 4; i++) {
		n = inb(port + 2);
		if (n == 'M') {
			j = 1;
		} else if ((j == 1) && (n == 1)) {
			j = 2;
			break;
		} else
			j = 0;
	}
	if (j != 2)
		id = -2;
	return (id);
}

static void mxser_normal_mode(int port)
{
	int i, n;

	outb(0xA5, port + 1);
	outb(0x80, port + 3);
	outb(12, port + 0);	/* 9600 bps */
	outb(0, port + 1);
	outb(0x03, port + 3);	/* 8 data bits */
	outb(0x13, port + 4);	/* loop back mode */
	for (i = 0; i < 16; i++) {
		n = inb(port + 5);
		if ((n & 0x61) == 0x60)
			break;
		if ((n & 1) == 1)
			(void) inb(port);
	}
	outb(0x00, port + 4);
}
