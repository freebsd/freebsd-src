/* $Id: pcikbd.c,v 1.61 2001/08/18 09:40:46 davem Exp $
 * pcikbd.c: Ultra/AX PC keyboard support.
 *
 * Copyright (C) 1997  Eddie C. Dost  (ecd@skynet.be)
 * JavaStation support by Pete A. Zaitcev.
 *
 * This code is mainly put together from various places in
 * drivers/char, please refer to these sources for credits
 * to the original authors.
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/random.h>
#include <linux/miscdevice.h>
#include <linux/kbd_ll.h>
#include <linux/kbd_kern.h>
#include <linux/vt_kern.h>
#include <linux/delay.h>
#include <linux/spinlock.h>
#include <linux/smp_lock.h>
#include <linux/init.h>

#include <asm/ebus.h>
#if (defined(CONFIG_USB) || defined(CONFIG_USB_MODULE)) && defined(CONFIG_SPARC64)
#include <asm/isa.h>
#endif
#include <asm/oplib.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/uaccess.h>

/*
 * Different platforms provide different permutations of names.
 * AXi - kb_ps2, kdmouse.
 * MrCoffee - keyboard, mouse.
 * Espresso - keyboard, kdmouse.
 */
#define	PCI_KB_NAME1	"kb_ps2"
#define PCI_KB_NAME2	"keyboard"
#define PCI_MS_NAME1	"kdmouse"
#define PCI_MS_NAME2	"mouse"

#include "pcikbd.h"
#include "sunserial.h"

#ifndef __sparc_v9__
static int pcikbd_mrcoffee = 0;
#else
#define pcikbd_mrcoffee 0
extern void (*prom_keyboard)(void);
#endif

static unsigned long pcikbd_iobase = 0;
static unsigned int pcikbd_irq = 0;

/* used only by send_data - set by keyboard_interrupt */
static volatile unsigned char reply_expected = 0;
static volatile unsigned char acknowledge = 0;
static volatile unsigned char resend = 0;

static spinlock_t pcikbd_lock = SPIN_LOCK_UNLOCKED;

static void pcikbd_write(int address, int data);
static int pcikbd_wait_for_input(void);

unsigned char pckbd_read_mask = KBD_STAT_OBF;

extern int pcikbd_init(void);
extern void pci_compute_shiftstate(void);
extern int pci_setkeycode(unsigned int, unsigned int);
extern int pci_getkeycode(unsigned int);
extern void pci_setledstate(struct kbd_struct *, unsigned int);
extern unsigned char pci_getledstate(void);

#define pcikbd_inb(x)     inb(x)
#define pcikbd_outb(v,x)  outb(v,x)

/* Wait for keyboard controller input buffer to drain.
 * Must be invoked under the pcikbd_lock.
 */
static void kb_wait(void)
{
	unsigned long timeout = 250;

	do {
		if(!(pcikbd_inb(pcikbd_iobase + KBD_STATUS_REG) & KBD_STAT_IBF))
			return;
		mdelay(1);
	} while (--timeout);
}

/*
 * Translation of escaped scancodes to keycodes.
 * This is now user-settable.
 * The keycodes 1-88,96-111,119 are fairly standard, and
 * should probably not be changed - changing might confuse X.
 * X also interprets scancode 0x5d (KEY_Begin).
 *
 * For 1-88 keycode equals scancode.
 */

#define E0_KPENTER 96
#define E0_RCTRL   97
#define E0_KPSLASH 98
#define E0_PRSCR   99
#define E0_RALT    100
#define E0_BREAK   101  /* (control-pause) */
#define E0_HOME    102
#define E0_UP      103
#define E0_PGUP    104
#define E0_LEFT    105
#define E0_RIGHT   106
#define E0_END     107
#define E0_DOWN    108
#define E0_PGDN    109
#define E0_INS     110
#define E0_DEL     111

#define E1_PAUSE   119

/*
 * The keycodes below are randomly located in 89-95,112-118,120-127.
 * They could be thrown away (and all occurrences below replaced by 0),
 * but that would force many users to use the `setkeycodes' utility, where
 * they needed not before. It does not matter that there are duplicates, as
 * long as no duplication occurs for any single keyboard.
 */
#define SC_LIM 89

#define FOCUS_PF1 85           /* actual code! */
#define FOCUS_PF2 89
#define FOCUS_PF3 90
#define FOCUS_PF4 91
#define FOCUS_PF5 92
#define FOCUS_PF6 93
#define FOCUS_PF7 94
#define FOCUS_PF8 95
#define FOCUS_PF9 120
#define FOCUS_PF10 121
#define FOCUS_PF11 122
#define FOCUS_PF12 123

#define JAP_86     124
/* tfj@olivia.ping.dk:
 * The four keys are located over the numeric keypad, and are
 * labelled A1-A4. It's an rc930 keyboard, from
 * Regnecentralen/RC International, Now ICL.
 * Scancodes: 59, 5a, 5b, 5c.
 */
#define RGN1 124
#define RGN2 125
#define RGN3 126
#define RGN4 127

static unsigned char high_keys[128 - SC_LIM] = {
  RGN1, RGN2, RGN3, RGN4, 0, 0, 0,                   /* 0x59-0x5f */
  0, 0, 0, 0, 0, 0, 0, 0,                            /* 0x60-0x67 */
  0, 0, 0, 0, 0, FOCUS_PF11, 0, FOCUS_PF12,          /* 0x68-0x6f */
  0, 0, 0, FOCUS_PF2, FOCUS_PF9, 0, 0, FOCUS_PF3,    /* 0x70-0x77 */
  FOCUS_PF4, FOCUS_PF5, FOCUS_PF6, FOCUS_PF7,        /* 0x78-0x7b */
  FOCUS_PF8, JAP_86, FOCUS_PF10, 0                   /* 0x7c-0x7f */
};

/* BTC */
#define E0_MACRO   112
/* LK450 */
#define E0_F13     113
#define E0_F14     114
#define E0_HELP    115
#define E0_DO      116
#define E0_F17     117
#define E0_KPMINPLUS 118
/*
 * My OmniKey generates e0 4c for  the "OMNI" key and the
 * right alt key does nada. [kkoller@nyx10.cs.du.edu]
 */
#define E0_OK	124
/*
 * New microsoft keyboard is rumoured to have
 * e0 5b (left window button), e0 5c (right window button),
 * e0 5d (menu button). [or: LBANNER, RBANNER, RMENU]
 * [or: Windows_L, Windows_R, TaskMan]
 */
#define E0_MSLW	125
#define E0_MSRW	126
#define E0_MSTM	127

static unsigned char e0_keys[128] = {
  0, 0, 0, 0, 0, 0, 0, 0,			      /* 0x00-0x07 */
  0, 0, 0, 0, 0, 0, 0, 0,			      /* 0x08-0x0f */
  0, 0, 0, 0, 0, 0, 0, 0,			      /* 0x10-0x17 */
  0, 0, 0, 0, E0_KPENTER, E0_RCTRL, 0, 0,	      /* 0x18-0x1f */
  0, 0, 0, 0, 0, 0, 0, 0,			      /* 0x20-0x27 */
  0, 0, 0, 0, 0, 0, 0, 0,			      /* 0x28-0x2f */
  0, 0, 0, 0, 0, E0_KPSLASH, 0, E0_PRSCR,	      /* 0x30-0x37 */
  E0_RALT, 0, 0, 0, 0, E0_F13, E0_F14, E0_HELP,	      /* 0x38-0x3f */
  E0_DO, E0_F17, 0, 0, 0, 0, E0_BREAK, E0_HOME,	      /* 0x40-0x47 */
  E0_UP, E0_PGUP, 0, E0_LEFT, E0_OK, E0_RIGHT, E0_KPMINPLUS, E0_END,/* 0x48-0x4f */
  E0_DOWN, E0_PGDN, E0_INS, E0_DEL, 0, 0, 0, 0,	      /* 0x50-0x57 */
  0, 0, 0, E0_MSLW, E0_MSRW, E0_MSTM, 0, 0,	      /* 0x58-0x5f */
  0, 0, 0, 0, 0, 0, 0, 0,			      /* 0x60-0x67 */
  0, 0, 0, 0, 0, 0, 0, E0_MACRO,		      /* 0x68-0x6f */
  0, 0, 0, 0, 0, 0, 0, 0,			      /* 0x70-0x77 */
  0, 0, 0, 0, 0, 0, 0, 0			      /* 0x78-0x7f */
};

/* Simple translation table for the SysRq keys */

#ifdef CONFIG_MAGIC_SYSRQ
unsigned char pcikbd_sysrq_xlate[128] =
	"\000\0331234567890-=\177\t"			/* 0x00 - 0x0f */
	"qwertyuiop[]\r\000as"				/* 0x10 - 0x1f */
	"dfghjkl;'`\000\\zxcv"				/* 0x20 - 0x2f */
	"bnm,./\000*\000 \000\201\202\203\204\205"	/* 0x30 - 0x3f */
	"\206\207\210\211\212\000\000789-456+1"		/* 0x40 - 0x4f */
	"230\177\000\000\213\214\000\000\000\000\000\000\000\000\000\000" /* 0x50 - 0x5f */
	"\r\000/";					/* 0x60 - 0x6f */
#endif

#define DEFAULT_KEYB_REP_DELAY	250
#define DEFAULT_KEYB_REP_RATE	30	/* cps */

static struct kbd_repeat kbdrate = {
	DEFAULT_KEYB_REP_DELAY,
	DEFAULT_KEYB_REP_RATE
};

static unsigned char parse_kbd_rate(struct kbd_repeat *r);
static int write_kbd_rate(unsigned char r);

int pcikbd_setkeycode(unsigned int scancode, unsigned int keycode)
{
	if(scancode < SC_LIM || scancode > 255 || keycode > 127)
		return -EINVAL;
	if(scancode < 128)
		high_keys[scancode - SC_LIM] = keycode;
	else
		e0_keys[scancode - 128] = keycode;
	return 0;
}

int pcikbd_getkeycode(unsigned int scancode)
{
	return
		(scancode < SC_LIM || scancode > 255) ? -EINVAL :
		(scancode < 128) ? high_keys[scancode - SC_LIM] :
		e0_keys[scancode - 128];
}

static int do_acknowledge(unsigned char scancode)
{
	if(reply_expected) {
		if(scancode == KBD_REPLY_ACK) {
			acknowledge = 1;
			reply_expected = 0;
			return 0;
		} else if(scancode == KBD_REPLY_RESEND) {
			resend = 1;
			reply_expected = 0;
			return 0;
		}
	}
	return 1;
}

#ifdef __sparc_v9__
static void pcikbd_enter_prom(void)
{
	pcikbd_write(KBD_DATA_REG, KBD_CMD_DISABLE);
	if(pcikbd_wait_for_input() != KBD_REPLY_ACK)
		printk("Prom Enter: Disable keyboard: no ACK\n");

	/* Disable PC scancode translation */
	pcikbd_write(KBD_CNTL_REG, KBD_CCMD_WRITE_MODE);
	pcikbd_write(KBD_DATA_REG, KBD_MODE_SYS);
	pcikbd_write(KBD_DATA_REG, KBD_CMD_ENABLE);
	if (pcikbd_wait_for_input() != KBD_REPLY_ACK)
		printk("Prom Enter: Enable Keyboard: no ACK\n");
}
#endif

static void ctrl_break(void)
{
	extern int stop_a_enabled;
	unsigned long timeout;
	int status, data;
	int mode;

	if (!stop_a_enabled)
		return;

	pcikbd_write(KBD_DATA_REG, KBD_CMD_DISABLE);
	if(pcikbd_wait_for_input() != KBD_REPLY_ACK)
		printk("Prom Enter: Disable keyboard: no ACK\n");

	/* Save current mode register settings */
	pcikbd_write(KBD_CNTL_REG, KBD_CCMD_READ_MODE);
	if ((mode = pcikbd_wait_for_input()) == -1)
		printk("Prom Enter: Read Mode: no ACK\n");

	/* Disable PC scancode translation */
	pcikbd_write(KBD_CNTL_REG, KBD_CCMD_WRITE_MODE);
	pcikbd_write(KBD_DATA_REG, mode & ~(KBD_MODE_KCC));
	pcikbd_write(KBD_DATA_REG, KBD_CMD_ENABLE);
	if (pcikbd_wait_for_input() != KBD_REPLY_ACK)
		printk("Prom Enter: Enable Keyboard: no ACK\n");

	/* Drop into OBP.
	 * Note that we must flush the user windows
	 * first before giving up control.
	 */
        flush_user_windows();
	prom_cmdline();

	/* Read prom's key up event (use short timeout) */
	do {
		timeout = 10;
		do {
			mdelay(1);
			status = pcikbd_inb(pcikbd_iobase + KBD_STATUS_REG);
			if (!(status & KBD_STAT_OBF))
				continue;
			data = pcikbd_inb(pcikbd_iobase + KBD_DATA_REG);
			if (status & (KBD_STAT_GTO | KBD_STAT_PERR))
				continue;
			break;
		} while (--timeout > 0);
	} while (timeout > 0);

	/* Reenable PC scancode translation */
	pcikbd_write(KBD_DATA_REG, KBD_CMD_DISABLE);
	if(pcikbd_wait_for_input() != KBD_REPLY_ACK)
		printk("Prom Leave: Disable keyboard: no ACK\n");

	pcikbd_write(KBD_CNTL_REG, KBD_CCMD_WRITE_MODE);
	pcikbd_write(KBD_DATA_REG, mode);
	pcikbd_write(KBD_DATA_REG, KBD_CMD_ENABLE);
	if (pcikbd_wait_for_input() != KBD_REPLY_ACK)
		printk("Prom Leave: Enable Keyboard: no ACK\n");

	/* Reset keyboard rate */
	write_kbd_rate(parse_kbd_rate(&kbdrate));
}

int pcikbd_translate(unsigned char scancode, unsigned char *keycode,
		     char raw_mode)
{
	static int prev_scancode = 0;
	int down = scancode & 0x80 ? 0 : 1;

	if (scancode == 0xe0 || scancode == 0xe1) {
		prev_scancode = scancode;
		return 0;
	}
	if (scancode == 0x00 || scancode == 0xff) {
		prev_scancode = 0;
		return 0;
	}
	scancode &= 0x7f;
	if(prev_scancode) {
		if(prev_scancode != 0xe0) {
			if(prev_scancode == 0xe1 && scancode == 0x1d) {
				prev_scancode = 0x100;
				return 0;
			} else if(prev_scancode == 0x100 && scancode == 0x45) {
				*keycode = E1_PAUSE;
				prev_scancode = 0;
			} else {
				prev_scancode = 0;
				return 0;
			}
		} else {
			prev_scancode = 0;
			if(scancode == 0x2a || scancode == 0x36)
				return 0;
			if(e0_keys[scancode])
				*keycode = e0_keys[scancode];
			else
				return 0;
		}
	} else if(scancode >= SC_LIM) {
		*keycode = high_keys[scancode - SC_LIM];
		if(!*keycode)
			return 0;

	} else
		*keycode = scancode;

	if (*keycode == E0_BREAK) {
		if (down)
			return 0;

		/* Handle ctrl-break event */
		ctrl_break();

		/* Send ctrl up event to the keyboard driver */
		*keycode = 0x1d;
	}

	return 1;
}

char pcikbd_unexpected_up(unsigned char keycode)
{
	if(keycode >= SC_LIM || keycode == 85)
		return 0;
	else
		return 0200;
}

static void
pcikbd_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	unsigned long flags;
	unsigned char status;

	spin_lock_irqsave(&pcikbd_lock, flags);

	kbd_pt_regs = regs;
	status = pcikbd_inb(pcikbd_iobase + KBD_STATUS_REG);
	do {
		unsigned char scancode;

		if(status & pckbd_read_mask & KBD_STAT_MOUSE_OBF)
			break;
		scancode = pcikbd_inb(pcikbd_iobase + KBD_DATA_REG);
		if((status & KBD_STAT_OBF) && do_acknowledge(scancode))
			handle_scancode(scancode, !(scancode & 0x80));
		status = pcikbd_inb(pcikbd_iobase + KBD_STATUS_REG);
	} while(status & KBD_STAT_OBF);
	tasklet_schedule(&keyboard_tasklet);

	spin_unlock_irqrestore(&pcikbd_lock, flags);
}

static int send_data(unsigned char data)
{
	int retries = 3;
	unsigned long flags;

	do {
		unsigned long timeout = 1000;

		spin_lock_irqsave(&pcikbd_lock, flags);

		kb_wait();

		acknowledge = 0;
		resend = 0;
		reply_expected = 1;

		pcikbd_outb(data, pcikbd_iobase + KBD_DATA_REG);

		spin_unlock_irqrestore(&pcikbd_lock, flags);

		do {
			if (acknowledge)
				return 1;
			if (resend)
				break;
			mdelay(1);
		} while (--timeout);

		if (timeout == 0)
			break;

	} while (retries-- > 0);

	return 0;
}

void pcikbd_leds(unsigned char leds)
{
	if (!pcikbd_iobase)
		return;
	if (!send_data(KBD_CMD_SET_LEDS) || !send_data(leds))
		send_data(KBD_CMD_ENABLE);
}

static unsigned char parse_kbd_rate(struct kbd_repeat *r)
{
	static struct r2v {
		int rate;
		unsigned char val;
	} kbd_rates[]={	{ 5,  0x14 },
			{ 7,  0x10 },
			{ 10, 0x0c },
			{ 15, 0x08 },
			{ 20, 0x04 },
			{ 25, 0x02 },
			{ 30, 0x00 } };
	static struct d2v {
		int delay;
		unsigned char val;
	} kbd_delays[]={ { 250,  0 },
			 { 500,  1 },
			 { 750,  2 },
			 { 1000, 3 } };
	int rate = 0, delay = 0;

	if (r != NULL) {
		int i, new_rate = 30, new_delay = 250;
		if (r->rate <= 0)
			r->rate = kbdrate.rate;
		if (r->delay <= 0)
			r->delay = kbdrate.delay;

		for (i = 0; i < sizeof(kbd_rates) / sizeof(struct r2v); i++) {
			if (kbd_rates[i].rate == r->rate) {
				new_rate = kbd_rates[i].rate;
				rate = kbd_rates[i].val;
				break;
			}
		}
		for (i=0; i < sizeof(kbd_delays) / sizeof(struct d2v); i++) {
			if (kbd_delays[i].delay == r->delay) {
				new_delay = kbd_delays[i].delay;
				delay = kbd_delays[i].val;
				break;
			}
		}
		r->rate = new_rate;
		r->delay = new_delay;
	}
	return (delay << 5) | rate;
}

static int write_kbd_rate(unsigned char r)
{
	if (!send_data(KBD_CMD_SET_RATE) || !send_data(r)) {
		/* re-enable kbd if any errors */
		send_data(KBD_CMD_ENABLE);
		return 0;
	}

	return 1;
}

static int pcikbd_rate(struct kbd_repeat *rep)
{
	unsigned char r;
	struct kbd_repeat old_rep;

	if (rep == NULL)
		return -EINVAL;

	r = parse_kbd_rate(rep);
	memcpy(&old_rep, &kbdrate, sizeof(struct kbd_repeat));
	if (write_kbd_rate(r)) {
		memcpy(&kbdrate,rep,sizeof(struct kbd_repeat));
		memcpy(rep,&old_rep,sizeof(struct kbd_repeat));
		return 0;
	}

	return -EIO;
}

static int pcikbd_wait_for_input(void)
{
	int status, data;
	unsigned long timeout = 1000;

	do {
		mdelay(1);

		status = pcikbd_inb(pcikbd_iobase + KBD_STATUS_REG);
		if (!(status & KBD_STAT_OBF))
			continue;

		data = pcikbd_inb(pcikbd_iobase + KBD_DATA_REG);
		if (status & (KBD_STAT_GTO | KBD_STAT_PERR))
			continue;

		return (data & 0xff);

	} while (--timeout > 0);

	return -1;
}

static void pcikbd_write(int address, int data)
{
	int status;

	do {
		status = pcikbd_inb(pcikbd_iobase + KBD_STATUS_REG);
	} while (status & KBD_STAT_IBF);
	pcikbd_outb(data, pcikbd_iobase + address);
}

#ifdef __sparc_v9__

static unsigned long pcibeep_iobase = 0;

/* Timer routine to turn off the beep after the interval expires. */
static void pcikbd_kd_nosound(unsigned long __unused)
{
	if (pcibeep_iobase & 0x2UL)
		outb(0, pcibeep_iobase);
	else
		outl(0, pcibeep_iobase);
}

/*
 * Initiate a keyboard beep. If the frequency is zero, then we stop
 * the beep. Any other frequency will start a monotone beep. The beep
 * will be stopped by a timer after "ticks" jiffies. If ticks is 0,
 * then we do not start a timer.
 */
static void pcikbd_kd_mksound(unsigned int hz, unsigned int ticks)
{
	unsigned long flags;
	static struct timer_list sound_timer = { function: pcikbd_kd_nosound };

	save_flags(flags); cli();
	del_timer(&sound_timer);
	if (hz) {
		if (pcibeep_iobase & 0x2UL)
			outb(1, pcibeep_iobase);
		else
			outl(1, pcibeep_iobase);
		if (ticks) {
			sound_timer.expires = jiffies + ticks;
			add_timer(&sound_timer);
		}
	} else {
		if (pcibeep_iobase & 0x2UL)
			outb(0, pcibeep_iobase);
		else
			outl(0, pcibeep_iobase);
	}
	restore_flags(flags);
}

#if (defined(CONFIG_USB) || defined(CONFIG_USB_MODULE)) && defined(CONFIG_SPARC64)
static void isa_kd_nosound(unsigned long __unused)
{
	/* disable counter 2 */
	outb(inb(pcibeep_iobase + 0x61)&0xFC, pcibeep_iobase + 0x61);
	return;
}

static void isa_kd_mksound(unsigned int hz, unsigned int ticks)
{
	static struct timer_list sound_timer = { function: isa_kd_nosound };
	unsigned int count = 0;
	unsigned long flags;

	if (hz > 20 && hz < 32767)
		count = 1193180 / hz;
	
	save_flags(flags);
	cli();
	del_timer(&sound_timer);
	if (count) {
		/* enable counter 2 */
		outb(inb(pcibeep_iobase + 0x61)|3, pcibeep_iobase + 0x61);
		/* set command for counter 2, 2 byte write */
		outb(0xB6, pcibeep_iobase + 0x43);
		/* select desired HZ */
		outb(count & 0xff, pcibeep_iobase + 0x42);
		outb((count >> 8) & 0xff, pcibeep_iobase + 0x42);

		if (ticks) {
			sound_timer.expires = jiffies+ticks;
			add_timer(&sound_timer);
		}
	} else
		isa_kd_nosound(0);
	restore_flags(flags);
	return;
}
#endif

#endif

static void nop_kd_mksound(unsigned int hz, unsigned int ticks)
{
}

extern void (*kd_mksound)(unsigned int hz, unsigned int ticks);

static char * __init do_pcikbd_init_hw(void)
{

	while(pcikbd_wait_for_input() != -1)
		;

	pcikbd_write(KBD_CNTL_REG, KBD_CCMD_SELF_TEST);
	if(pcikbd_wait_for_input() != 0x55)
		return "Keyboard failed self test";

	pcikbd_write(KBD_CNTL_REG, KBD_CCMD_KBD_TEST);
	if(pcikbd_wait_for_input() != 0x00)
		return "Keyboard interface failed self test";

	pcikbd_write(KBD_CNTL_REG, KBD_CCMD_KBD_ENABLE);
	pcikbd_write(KBD_DATA_REG, KBD_CMD_RESET);
	if(pcikbd_wait_for_input() != KBD_REPLY_ACK)
		return "Keyboard reset failed, no ACK";
	if(pcikbd_wait_for_input() != KBD_REPLY_POR)
		return "Keyboard reset failed, no ACK";

	pcikbd_write(KBD_DATA_REG, KBD_CMD_DISABLE);
	if(pcikbd_wait_for_input() != KBD_REPLY_ACK)
		return "Disable keyboard: no ACK";

	pcikbd_write(KBD_CNTL_REG, KBD_CCMD_WRITE_MODE);
	pcikbd_write(KBD_DATA_REG,
		     (KBD_MODE_KBD_INT | KBD_MODE_SYS |
		      KBD_MODE_DISABLE_MOUSE | KBD_MODE_KCC));
	pcikbd_write(KBD_DATA_REG, KBD_CMD_ENABLE);
	if(pcikbd_wait_for_input() != KBD_REPLY_ACK)
		return "Enable keyboard: no ACK";

	write_kbd_rate(parse_kbd_rate(&kbdrate));

	return NULL; /* success */
}

void __init pcikbd_init_hw(void)
{
	struct linux_ebus *ebus;
	struct linux_ebus_device *edev;
	struct linux_ebus_child *child;
	char *msg;

	if (pcikbd_mrcoffee) {
		if ((pcikbd_iobase = (unsigned long) ioremap(0x71300060, 8)) == 0) {
			prom_printf("pcikbd_init_hw: cannot map\n");
			return;
		}
		pcikbd_irq = 13 | 0x20;
		if (request_irq(pcikbd_irq, &pcikbd_interrupt,
				SA_SHIRQ, "keyboard", (void *)pcikbd_iobase)) {
			printk("8042: cannot register IRQ %x\n", pcikbd_irq);
			return;
		}
		printk("8042(kbd): iobase[%x] irq[%x]\n",
		    (unsigned)pcikbd_iobase, pcikbd_irq);
	} else {
		for_each_ebus(ebus) {
			for_each_ebusdev(edev, ebus) {
				if(!strcmp(edev->prom_name, "8042")) {
					for_each_edevchild(edev, child) {
                                                if (strcmp(child->prom_name, PCI_KB_NAME1) == 0 ||
						    strcmp(child->prom_name, PCI_KB_NAME2) == 0)
							goto found;
					}
				}
			}
		}
#if defined(CONFIG_USB) || defined(CONFIG_USB_MODULE)
		/* We are being called for the sake of USB keyboard
		 * state initialization.  So we should check for beeper
		 * device in this case.
		 */
		edev = 0;
		for_each_ebus(ebus) {
			for_each_ebusdev(edev, ebus) {
				if (!strcmp(edev->prom_name, "beep")) {
					pcibeep_iobase = edev->resource[0].start;
					kd_mksound = pcikbd_kd_mksound;
					printk("8042(speaker): iobase[%016lx]\n", pcibeep_iobase);
					return;
				}
			}
		}

#ifdef CONFIG_SPARC64
		/* Maybe we have one inside the ALI southbridge? */
		{
			struct isa_bridge *isa_br;
			struct isa_device *isa_dev;
			for_each_isa(isa_br) {
				for_each_isadev(isa_dev, isa_br) {
					/* This is a hack, the 'dma' device node has
					 * the base of the I/O port space for that PBM
					 * as it's resource, so we use that. -DaveM
					 */
					if (!strcmp(isa_dev->prom_name, "dma")) {
						pcibeep_iobase = isa_dev->resource.start;
						kd_mksound = isa_kd_mksound;
						printk("isa(speaker): iobase[%016lx:%016lx]\n",
						       pcibeep_iobase + 0x42,
						       pcibeep_iobase + 0x61);
						return;
					}
				}
			}
		}
#endif

		/* No beeper found, ok complain. */
#endif
		printk("pcikbd_init_hw: no 8042 found\n");
		return;

found:
		pcikbd_iobase = child->resource[0].start;
		pcikbd_irq = child->irqs[0];
		if (request_irq(pcikbd_irq, &pcikbd_interrupt,
				SA_SHIRQ, "keyboard", (void *)pcikbd_iobase)) {
			printk("8042: cannot register IRQ %s\n",
			       __irq_itoa(pcikbd_irq));
			return;
		}

		printk("8042(kbd) at 0x%lx (irq %s)\n", pcikbd_iobase,
		       __irq_itoa(pcikbd_irq));
	}

	kd_mksound = nop_kd_mksound;
	kbd_rate = pcikbd_rate;

#ifdef __sparc_v9__
	edev = 0;
	for_each_ebus(ebus) {
		for_each_ebusdev(edev, ebus) {
			if(!strcmp(edev->prom_name, "beeper"))
				goto ebus_done;
		}
	}
ebus_done:

	/*
	 * XXX: my 3.1.3 PROM does not give me the beeper node for the audio
	 *      auxio register, though I know it is there... (ecd)
	 *
	 * JavaStations appear not to have beeper. --zaitcev
	 */
	if (!edev)
		pcibeep_iobase = (pcikbd_iobase & ~(0xffffff)) | 0x722000;
	else
		pcibeep_iobase = edev->resource[0].start;

	kd_mksound = pcikbd_kd_mksound;
	printk("8042(speaker): iobase[%016lx]%s\n", pcibeep_iobase,
	       edev ? "" : " (forced)");

	prom_keyboard = pcikbd_enter_prom;
#endif

	disable_irq(pcikbd_irq);
	msg = do_pcikbd_init_hw();
	enable_irq(pcikbd_irq);

	if(msg)
		printk("8042: keyboard init failure [%s]\n", msg);
}


/*
 * Here begins the Mouse Driver.
 */

static unsigned long pcimouse_iobase = 0;
static unsigned int pcimouse_irq;

#define AUX_BUF_SIZE	2048

struct aux_queue {
	unsigned long head;
	unsigned long tail;
	wait_queue_head_t proc_list;
	struct fasync_struct *fasync;
	unsigned char buf[AUX_BUF_SIZE];
};

static struct aux_queue *queue;
static int aux_count = 0;
static int aux_present = 0;

#define pcimouse_inb(x)     inb(x)
#define pcimouse_outb(v,x)  outb(v,x)

/*
 *	Shared subroutines
 */

static unsigned int get_from_queue(void)
{
	unsigned int result;
	unsigned long flags;

	spin_lock_irqsave(&pcikbd_lock, flags);
	result = queue->buf[queue->tail];
	queue->tail = (queue->tail + 1) & (AUX_BUF_SIZE-1);
	spin_unlock_irqrestore(&pcikbd_lock, flags);

	return result;
}


static inline int queue_empty(void)
{
	return queue->head == queue->tail;
}

static int aux_fasync(int fd, struct file *filp, int on)
{
	int retval;

	retval = fasync_helper(fd, filp, on, &queue->fasync);
	if (retval < 0)
		return retval;
	return 0;
}

/*
 *	PS/2 Aux Device
 */

#define AUX_INTS_OFF	(KBD_MODE_KCC | KBD_MODE_DISABLE_MOUSE | \
			 KBD_MODE_SYS | KBD_MODE_KBD_INT)

#define AUX_INTS_ON	(KBD_MODE_KCC | KBD_MODE_SYS | \
			 KBD_MODE_MOUSE_INT | KBD_MODE_KBD_INT)

#define MAX_RETRIES	60		/* some aux operations take long time*/

/*
 *	Status polling
 */

static int poll_aux_status(void)
{
	int retries = 0;

	while ((pcimouse_inb(pcimouse_iobase + KBD_STATUS_REG) &
		(KBD_STAT_IBF | KBD_STAT_OBF)) && retries < MAX_RETRIES) {
 		if ((pcimouse_inb(pcimouse_iobase + KBD_STATUS_REG) & AUX_STAT_OBF)
		    == AUX_STAT_OBF)
			pcimouse_inb(pcimouse_iobase + KBD_DATA_REG);
		mdelay(5);
		retries++;
	}

	return (retries < MAX_RETRIES);
}

/*
 * Write to aux device
 */

static void aux_write_dev(int val)
{
	poll_aux_status();
	pcimouse_outb(KBD_CCMD_WRITE_MOUSE, pcimouse_iobase + KBD_CNTL_REG);/* Write magic cookie */
	poll_aux_status();
	pcimouse_outb(val, pcimouse_iobase + KBD_DATA_REG);		 /* Write data */
	udelay(1);
}

/*
 * Write to device & handle returned ack
 */

static int __init aux_write_ack(int val)
{
	aux_write_dev(val);
	poll_aux_status();

	if ((pcimouse_inb(pcimouse_iobase + KBD_STATUS_REG) & AUX_STAT_OBF) == AUX_STAT_OBF)
		return (pcimouse_inb(pcimouse_iobase + KBD_DATA_REG));
	return 0;
}

/*
 * Write aux device command
 */

static void aux_write_cmd(int val)
{
	poll_aux_status();
	pcimouse_outb(KBD_CCMD_WRITE_MODE, pcimouse_iobase + KBD_CNTL_REG);
	poll_aux_status();
	pcimouse_outb(val, pcimouse_iobase + KBD_DATA_REG);
}

/*
 * Interrupt from the auxiliary device: a character
 * is waiting in the keyboard/aux controller.
 */

void pcimouse_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	unsigned long flags;
	int head, maxhead;
	unsigned char val;

	spin_lock_irqsave(&pcikbd_lock, flags);

	head = queue->head;
	maxhead = (queue->tail - 1) & (AUX_BUF_SIZE - 1);

	if ((pcimouse_inb(pcimouse_iobase + KBD_STATUS_REG) & AUX_STAT_OBF) !=
	    AUX_STAT_OBF) {
		spin_unlock_irqrestore(&pcikbd_lock, flags);
		return;
	}

	val = pcimouse_inb(pcimouse_iobase + KBD_DATA_REG);
	queue->buf[head] = val;
	add_mouse_randomness(val);
	if (head != maxhead) {
		head++;
		head &= AUX_BUF_SIZE - 1;
	}
	queue->head = head;

	spin_unlock_irqrestore(&pcikbd_lock, flags);

	kill_fasync(&queue->fasync, SIGIO, POLL_IN);
	wake_up_interruptible(&queue->proc_list);
}

static int aux_release(struct inode * inode, struct file * file)
{
	unsigned long flags;

	aux_fasync(-1, file, 0);

	spin_lock_irqsave(&pcikbd_lock, flags);

	if (--aux_count)
		goto out;

	/* Disable controller ints */
	aux_write_cmd(AUX_INTS_OFF);
	poll_aux_status();

	/* Disable Aux device */
	pcimouse_outb(KBD_CCMD_MOUSE_DISABLE, pcimouse_iobase + KBD_CNTL_REG);
	poll_aux_status();

out:
	spin_unlock_irqrestore(&pcikbd_lock, flags);

	return 0;
}

/*
 * Install interrupt handler.
 * Enable auxiliary device.
 */

static int aux_open(struct inode * inode, struct file * file)
{
	unsigned long flags;

	if (!aux_present)
		return -ENODEV;

	spin_lock_irqsave(&pcikbd_lock, flags);

	if (aux_count++) {
		spin_unlock_irqrestore(&pcikbd_lock, flags);
		return 0;
	}

	if (!poll_aux_status()) {
		aux_count--;
		spin_unlock_irqrestore(&pcikbd_lock, flags);
		return -EBUSY;
	}
	queue->head = queue->tail = 0;		/* Flush input queue */

	poll_aux_status();
	pcimouse_outb(KBD_CCMD_MOUSE_ENABLE, pcimouse_iobase+KBD_CNTL_REG);    /* Enable Aux */
	aux_write_dev(AUX_ENABLE_DEV);			    /* Enable aux device */
	aux_write_cmd(AUX_INTS_ON);			    /* Enable controller ints */
	poll_aux_status();

	spin_unlock_irqrestore(&pcikbd_lock, flags);


	return 0;
}

/*
 * Write to the aux device.
 */

static ssize_t aux_write(struct file * file, const char * buffer,
			 size_t count, loff_t *ppos)
{
	ssize_t retval = 0;
	unsigned long flags;

	if (count) {
		ssize_t written = 0;

		spin_lock_irqsave(&pcikbd_lock, flags);

		do {
			char c;

			spin_unlock_irqrestore(&pcikbd_lock, flags);

			get_user(c, buffer++);

			spin_lock_irqsave(&pcikbd_lock, flags);

			if (!poll_aux_status())
				break;
			pcimouse_outb(KBD_CCMD_WRITE_MOUSE,
				      pcimouse_iobase + KBD_CNTL_REG);
			if (!poll_aux_status())
				break;

			pcimouse_outb(c, pcimouse_iobase + KBD_DATA_REG);
			written++;
		} while (--count);

		spin_unlock_irqrestore(&pcikbd_lock, flags);

		retval = -EIO;
		if (written) {
			retval = written;
			file->f_dentry->d_inode->i_mtime = CURRENT_TIME;
		}
	}

	return retval;
}

/*
 *	Generic part continues...
 */

/*
 * Put bytes from input queue to buffer.
 */

static ssize_t aux_read(struct file * file, char * buffer,
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

static unsigned int aux_poll(struct file *file, poll_table * wait)
{
	poll_wait(file, &queue->proc_list, wait);
	if (!queue_empty())
		return POLLIN | POLLRDNORM;
	return 0;
}

struct file_operations psaux_fops = {
	owner:		THIS_MODULE,
	read:		aux_read,
	write:		aux_write,
	poll:		aux_poll,
	open:		aux_open,
	release:	aux_release,
	fasync:		aux_fasync,
};

static int aux_no_open(struct inode *inode, struct file *file)
{
	return -ENODEV;
}

struct file_operations psaux_no_fops = {
	owner:		THIS_MODULE,
	open:		aux_no_open,
};

static struct miscdevice psaux_mouse = {
	PSMOUSE_MINOR, "ps2aux", &psaux_fops
};

static struct miscdevice psaux_no_mouse = {
	PSMOUSE_MINOR, "ps2aux", &psaux_no_fops
};

int __init pcimouse_init(void)
{
	struct linux_ebus *ebus;
	struct linux_ebus_device *edev;
	struct linux_ebus_child *child;

	if (pcikbd_mrcoffee) {
		if ((pcimouse_iobase = pcikbd_iobase) == 0) {
			printk("pcimouse_init: no 8042 given\n");
			goto do_enodev;
		}
		pcimouse_irq = pcikbd_irq;
	} else {
		for_each_ebus(ebus) {
			for_each_ebusdev(edev, ebus) {
				if(!strcmp(edev->prom_name, "8042")) {
					for_each_edevchild(edev, child) {
							if (strcmp(child->prom_name, PCI_MS_NAME1) == 0 ||
							    strcmp(child->prom_name, PCI_MS_NAME2) == 0)
							goto found;
					}
				}
			}
		}
		printk("pcimouse_init: no 8042 found\n");
		goto do_enodev;

found:
		pcimouse_iobase = child->resource[0].start;
		pcimouse_irq = child->irqs[0];
	}

	queue = (struct aux_queue *) kmalloc(sizeof(*queue), GFP_KERNEL);
	if (!queue) {
		printk("pcimouse_init: kmalloc(aux_queue) failed.\n");
		return -ENOMEM;
	}
	memset(queue, 0, sizeof(*queue));

	init_waitqueue_head(&queue->proc_list);

	if (request_irq(pcimouse_irq, &pcimouse_interrupt,
		        SA_SHIRQ, "mouse", (void *)pcimouse_iobase)) {
		printk("8042: Cannot register IRQ %s\n",
		       __irq_itoa(pcimouse_irq));
		goto do_enodev;
	}

	printk("8042(mouse) at %lx (irq %s)\n", pcimouse_iobase,
	       __irq_itoa(pcimouse_irq));

	printk("8042: PS/2 auxiliary pointing device detected.\n");
	aux_present = 1;
	pckbd_read_mask = AUX_STAT_OBF;

	misc_register(&psaux_mouse);

	spin_lock_irq(&pcikbd_lock);

	pcimouse_outb(KBD_CCMD_MOUSE_ENABLE, pcimouse_iobase + KBD_CNTL_REG);
	aux_write_ack(AUX_RESET);
	aux_write_ack(AUX_SET_SAMPLE);
	aux_write_ack(100);
	aux_write_ack(AUX_SET_RES);
	aux_write_ack(3);
	aux_write_ack(AUX_SET_SCALE21);
	poll_aux_status();
	pcimouse_outb(KBD_CCMD_MOUSE_DISABLE, pcimouse_iobase + KBD_CNTL_REG);
	poll_aux_status();
	pcimouse_outb(KBD_CCMD_WRITE_MODE, pcimouse_iobase + KBD_CNTL_REG);
	poll_aux_status();
	pcimouse_outb(AUX_INTS_OFF, pcimouse_iobase + KBD_DATA_REG);
	poll_aux_status();

	spin_unlock_irq(&pcikbd_lock);

	return 0;

do_enodev:
	misc_register(&psaux_no_mouse);
	return -ENODEV;
}

int __init pcimouse_no_init(void)
{
	misc_register(&psaux_no_mouse);
	return -ENODEV;
}

int __init ps2kbd_probe(void)
{
	int pnode, enode, node, dnode, xnode;
	int kbnode = 0, msnode = 0, bnode = 0;
	int devices = 0;
	char prop[128];
	int len;

#ifndef __sparc_v9__
	/*
	 * MrCoffee has hardware but has no PROM nodes whatsoever.
	 */
	len = prom_getproperty(prom_root_node, "name", prop, sizeof(prop));
	if (len < 0) {
		printk("ps2kbd_probe: no name of root node\n");
		goto do_enodev;
	}
	if (strncmp(prop, "SUNW,JavaStation-1", len) == 0) {
		pcikbd_mrcoffee = 1;	/* Brain damage detected */
		goto found;
	}
#endif
	/*
	 * Get the nodes for keyboard and mouse from aliases on normal systems.
	 */
        node = prom_getchild(prom_root_node);
	node = prom_searchsiblings(node, "aliases");
	if (!node)
		goto do_enodev;

	len = prom_getproperty(node, "keyboard", prop, sizeof(prop));
	if (len > 0) {
		prop[len] = 0;
		kbnode = prom_finddevice(prop);
	}
	if (!kbnode)
		goto do_enodev;

	len = prom_getproperty(node, "mouse", prop, sizeof(prop));
	if (len > 0) {
		prop[len] = 0;
		msnode = prom_finddevice(prop);
	}
	if (!msnode)
		goto do_enodev;

	/*
	 * Find matching EBus nodes...
	 */
        node = prom_getchild(prom_root_node);
	pnode = prom_searchsiblings(node, "pci");

	/*
	 * Check for SUNW,sabre on Ultra5/10/AXi.
	 */
	len = prom_getproperty(pnode, "model", prop, sizeof(prop));
	if ((len > 0) && !strncmp(prop, "SUNW,sabre", len)) {
		pnode = prom_getchild(pnode);
		pnode = prom_searchsiblings(pnode, "pci");
	}

	/*
	 * For each PCI bus...
	 */
	while (pnode) {
		enode = prom_getchild(pnode);
		enode = prom_searchsiblings(enode, "ebus");

		/*
		 * For each EBus on this PCI...
		 */
		while (enode) {
			node = prom_getchild(enode);
			bnode = prom_searchsiblings(node, "beeper");

			node = prom_getchild(enode);
			node = prom_searchsiblings(node, "8042");

			/*
			 * For each '8042' on this EBus...
			 */
			while (node) {
				dnode = prom_getchild(node);

				/*
				 * Does it match?
				 */
				if ((xnode = prom_searchsiblings(dnode, PCI_KB_NAME1)) == kbnode) {
					++devices;
				} else if ((xnode = prom_searchsiblings(dnode, PCI_KB_NAME2)) == kbnode) {
					++devices;
				}

				if ((xnode = prom_searchsiblings(dnode, PCI_MS_NAME1)) == msnode) {
					++devices;
				} else if ((xnode = prom_searchsiblings(dnode, PCI_MS_NAME2)) == msnode) {
					++devices;
				}

				/*
				 * Found everything we need?
				 */
				if (devices == 2)
					goto found;

				node = prom_getsibling(node);
				node = prom_searchsiblings(node, "8042");
			}
			enode = prom_getsibling(enode);
			enode = prom_searchsiblings(enode, "ebus");
		}
		pnode = prom_getsibling(pnode);
		pnode = prom_searchsiblings(pnode, "pci");
	}
do_enodev:
	sunkbd_setinitfunc(pcimouse_no_init);
	return -ENODEV;

found:
        sunkbd_setinitfunc(pcimouse_init);
        sunkbd_setinitfunc(pcikbd_init);
	kbd_ops.compute_shiftstate = pci_compute_shiftstate;
	kbd_ops.setledstate = pci_setledstate;
	kbd_ops.getledstate = pci_getledstate;
	kbd_ops.setkeycode = pci_setkeycode;
	kbd_ops.getkeycode = pci_getkeycode;
	return 0;
}
