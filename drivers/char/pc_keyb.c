/*
 * linux/drivers/char/pc_keyb.c
 *
 * Separation of the PC low-level part by Geert Uytterhoeven, May 1997
 * See keyboard.c for the whole history.
 *
 * Major cleanup by Martin Mares, May 1997
 *
 * Combined the keyboard and PS/2 mouse handling into one file,
 * because they share the same hardware.
 * Johan Myreen <jem@iki.fi> 1998-10-08.
 *
 * Code fixes to handle mouse ACKs properly.
 * C. Scott Ananian <cananian@alumni.princeton.edu> 1999-01-29.
 *
 */

#include <linux/config.h>

#include <linux/spinlock.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/tty.h>
#include <linux/mm.h>
#include <linux/signal.h>
#include <linux/init.h>
#include <linux/kbd_ll.h>
#include <linux/delay.h>
#include <linux/random.h>
#include <linux/poll.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/kbd_kern.h>
#include <linux/vt_kern.h>
#include <linux/smp_lock.h>
#include <linux/kd.h>
#include <linux/pm.h>

#include <asm/keyboard.h>
#include <asm/bitops.h>
#include <asm/uaccess.h>
#include <asm/irq.h>
#include <asm/system.h>

#include <asm/io.h>

/* Some configuration switches are present in the include file... */

#include <linux/pc_keyb.h>

/* Simple translation table for the SysRq keys */

#ifdef CONFIG_MAGIC_SYSRQ
unsigned char pckbd_sysrq_xlate[128] =
	"\000\0331234567890-=\177\t"			/* 0x00 - 0x0f */
	"qwertyuiop[]\r\000as"				/* 0x10 - 0x1f */
	"dfghjkl;'`\000\\zxcv"				/* 0x20 - 0x2f */
	"bnm,./\000*\000 \000\201\202\203\204\205"	/* 0x30 - 0x3f */
	"\206\207\210\211\212\000\000789-456+1"		/* 0x40 - 0x4f */
	"230\177\000\000\213\214\000\000\000\000\000\000\000\000\000\000" /* 0x50 - 0x5f */
	"\r\000/";					/* 0x60 - 0x6f */
#endif

static void kbd_write_command_w(int data);
static void kbd_write_output_w(int data);
#ifdef CONFIG_PSMOUSE
static void aux_write_ack(int val);
static void __aux_write_ack(int val);
static int aux_reconnect = 0;
#endif

#ifndef kbd_controller_present
#define kbd_controller_present()	1
#endif
static spinlock_t kbd_controller_lock = SPIN_LOCK_UNLOCKED;
static unsigned char handle_kbd_event(void);

/* used only by send_data - set by keyboard_interrupt */
static volatile unsigned char reply_expected;
static volatile unsigned char acknowledge;
static volatile unsigned char resend;


#if defined CONFIG_PSMOUSE
/*
 *	PS/2 Auxiliary Device
 */

static int __init psaux_init(void);

#define AUX_RECONNECT1 0xaa	/* scancode1 when ps2 device is plugged (back) in */
#define AUX_RECONNECT2 0x00	/* scancode2 when ps2 device is plugged (back) in */
 
static struct aux_queue *queue;	/* Mouse data buffer. */
static int aux_count;
/* used when we send commands to the mouse that expect an ACK. */
static unsigned char mouse_reply_expected;

#define AUX_INTS_OFF (KBD_MODE_KCC | KBD_MODE_DISABLE_MOUSE | KBD_MODE_SYS | KBD_MODE_KBD_INT)
#define AUX_INTS_ON  (KBD_MODE_KCC | KBD_MODE_SYS | KBD_MODE_MOUSE_INT | KBD_MODE_KBD_INT)

#define MAX_RETRIES	60		/* some aux operations take long time*/
#endif /* CONFIG_PSMOUSE */

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

static void kb_wait(void)
{
	unsigned long timeout = KBC_TIMEOUT;

	do {
		/*
		 * "handle_kbd_event()" will handle any incoming events
		 * while we wait - keypresses or mouse movement.
		 */
		unsigned char status = handle_kbd_event();

		if (! (status & KBD_STAT_IBF))
			return;
		mdelay(1);
		timeout--;
	} while (timeout);
#ifdef KBD_REPORT_TIMEOUTS
	printk(KERN_WARNING "Keyboard timed out[1]\n");
#endif
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

int pckbd_setkeycode(unsigned int scancode, unsigned int keycode)
{
	if (scancode < SC_LIM || scancode > 255 || keycode > 127)
	  return -EINVAL;
	if (scancode < 128)
	  high_keys[scancode - SC_LIM] = keycode;
	else
	  e0_keys[scancode - 128] = keycode;
	return 0;
}

int pckbd_getkeycode(unsigned int scancode)
{
	return
	  (scancode < SC_LIM || scancode > 255) ? -EINVAL :
	  (scancode < 128) ? high_keys[scancode - SC_LIM] :
	    e0_keys[scancode - 128];
}

static int do_acknowledge(unsigned char scancode)
{
	if (reply_expected) {
	  /* Unfortunately, we must recognise these codes only if we know they
	   * are known to be valid (i.e., after sending a command), because there
	   * are some brain-damaged keyboards (yes, FOCUS 9000 again) which have
	   * keys with such codes :(
	   */
		if (scancode == KBD_REPLY_ACK) {
			acknowledge = 1;
			reply_expected = 0;
			return 0;
		} else if (scancode == KBD_REPLY_RESEND) {
			resend = 1;
			reply_expected = 0;
			return 0;
		}
		/* Should not happen... */
#if 0
		printk(KERN_DEBUG "keyboard reply expected - got %02x\n",
		       scancode);
#endif
	}
	return 1;
}

int pckbd_translate(unsigned char scancode, unsigned char *keycode,
		    char raw_mode)
{
	static int prev_scancode;

	/* special prefix scancodes.. */
	if (scancode == 0xe0 || scancode == 0xe1) {
		prev_scancode = scancode;
		return 0;
	}

	/* 0xFF is sent by a few keyboards, ignore it. 0x00 is error */
	if (scancode == 0x00 || scancode == 0xff) {
		prev_scancode = 0;
		return 0;
	}

	scancode &= 0x7f;

	if (prev_scancode) {
	  /*
	   * usually it will be 0xe0, but a Pause key generates
	   * e1 1d 45 e1 9d c5 when pressed, and nothing when released
	   */
	  if (prev_scancode != 0xe0) {
	      if (prev_scancode == 0xe1 && scancode == 0x1d) {
		  prev_scancode = 0x100;
		  return 0;
	      } else if (prev_scancode == 0x100 && scancode == 0x45) {
		  *keycode = E1_PAUSE;
		  prev_scancode = 0;
	      } else {
#ifdef KBD_REPORT_UNKN
		  if (!raw_mode)
		    printk(KERN_INFO "keyboard: unknown e1 escape sequence\n");
#endif
		  prev_scancode = 0;
		  return 0;
	      }
	  } else {
	      prev_scancode = 0;
	      /*
	       *  The keyboard maintains its own internal caps lock and
	       *  num lock statuses. In caps lock mode E0 AA precedes make
	       *  code and E0 2A follows break code. In num lock mode,
	       *  E0 2A precedes make code and E0 AA follows break code.
	       *  We do our own book-keeping, so we will just ignore these.
	       */
	      /*
	       *  For my keyboard there is no caps lock mode, but there are
	       *  both Shift-L and Shift-R modes. The former mode generates
	       *  E0 2A / E0 AA pairs, the latter E0 B6 / E0 36 pairs.
	       *  So, we should also ignore the latter. - aeb@cwi.nl
	       */
	      if (scancode == 0x2a || scancode == 0x36)
		return 0;

	      if (e0_keys[scancode])
		*keycode = e0_keys[scancode];
	      else {
#ifdef KBD_REPORT_UNKN
		  if (!raw_mode)
		    printk(KERN_INFO "keyboard: unknown scancode e0 %02x\n",
			   scancode);
#endif
		  return 0;
	      }
	  }
	} else if (scancode >= SC_LIM) {
	    /* This happens with the FOCUS 9000 keyboard
	       Its keys PF1..PF12 are reported to generate
	       55 73 77 78 79 7a 7b 7c 74 7e 6d 6f
	       Moreover, unless repeated, they do not generate
	       key-down events, so we have to zero up_flag below */
	    /* Also, Japanese 86/106 keyboards are reported to
	       generate 0x73 and 0x7d for \ - and \ | respectively. */
	    /* Also, some Brazilian keyboard is reported to produce
	       0x73 and 0x7e for \ ? and KP-dot, respectively. */

	  *keycode = high_keys[scancode - SC_LIM];

	  if (!*keycode) {
	      if (!raw_mode) {
#ifdef KBD_REPORT_UNKN
		  printk(KERN_INFO "keyboard: unrecognized scancode (%02x)"
			 " - ignored\n", scancode);
#endif
	      }
	      return 0;
	  }
 	} else
	  *keycode = scancode;
 	return 1;
}

char pckbd_unexpected_up(unsigned char keycode)
{
	/* unexpected, but this can happen: maybe this was a key release for a
	   FOCUS 9000 PF key; if we want to see it, we have to clear up_flag */
	if (keycode >= SC_LIM || keycode == 85)
	    return 0;
	else
	    return 0200;
}

int pckbd_pm_resume(struct pm_dev *dev, pm_request_t rqst, void *data) 
{
#if defined CONFIG_PSMOUSE
       unsigned long flags;

       if (rqst == PM_RESUME) {
               if (queue) {                    /* Aux port detected */
                       if (aux_count == 0) {   /* Mouse not in use */ 
                               spin_lock_irqsave(&kbd_controller_lock, flags);
			       /*
				* Dell Lat. C600 A06 enables mouse after resume.
				* When user touches the pad, it posts IRQ 12
				* (which we do not process), thus holding keyboard.
				*/
			       kbd_write_command(KBD_CCMD_MOUSE_DISABLE);
			       /* kbd_write_cmd(AUX_INTS_OFF); */ /* Config & lock */
			       kb_wait();
			       kbd_write_command(KBD_CCMD_WRITE_MODE);
			       kb_wait();
			       kbd_write_output(AUX_INTS_OFF);
			       spin_unlock_irqrestore(&kbd_controller_lock, flags);
		       }
	       }
       }
#endif
       return 0;
}


static inline void handle_mouse_event(unsigned char scancode)
{
#ifdef CONFIG_PSMOUSE
	static unsigned char prev_code;
	if (mouse_reply_expected) {
		if (scancode == AUX_ACK) {
			mouse_reply_expected--;
			return;
		}
		mouse_reply_expected = 0;
	}
	else if(scancode == AUX_RECONNECT2 && prev_code == AUX_RECONNECT1
		&& aux_reconnect) {
		printk (KERN_INFO "PS/2 mouse reconnect detected\n");
		queue->head = queue->tail = 0;	/* Flush input queue */
		__aux_write_ack(AUX_ENABLE_DEV);  /* ping the mouse :) */
		return;
	}

	prev_code = scancode;
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
#endif
}

static unsigned char kbd_exists = 1;

static inline void handle_keyboard_event(unsigned char scancode)
{
#ifdef CONFIG_VT
	kbd_exists = 1;
	if (do_acknowledge(scancode))
		handle_scancode(scancode, !(scancode & 0x80));
#endif				
	tasklet_schedule(&keyboard_tasklet);
}	

/*
 * This reads the keyboard status port, and does the
 * appropriate action.
 *
 * It requires that we hold the keyboard controller
 * spinlock.
 */
static unsigned char handle_kbd_event(void)
{
	unsigned char status = kbd_read_status();
	unsigned int work = 10000;

	while ((--work > 0) && (status & KBD_STAT_OBF)) {
		unsigned char scancode;

		scancode = kbd_read_input();

		/* Error bytes must be ignored to make the 
		   Synaptics touchpads compaq use work */
#if 1
		/* Ignore error bytes */
		if (!(status & (KBD_STAT_GTO | KBD_STAT_PERR)))
#endif
		{
			if (status & KBD_STAT_MOUSE_OBF)
				handle_mouse_event(scancode);
			else
				handle_keyboard_event(scancode);
		}

		status = kbd_read_status();
	}
		
	if (!work)
		printk(KERN_ERR "pc_keyb: controller jammed (0x%02X).\n", status);

	return status;
}


static void keyboard_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
#ifdef CONFIG_VT
	kbd_pt_regs = regs;
#endif

	spin_lock_irq(&kbd_controller_lock);
	handle_kbd_event();
	spin_unlock_irq(&kbd_controller_lock);
}

/*
 * send_data sends a character to the keyboard and waits
 * for an acknowledge, possibly retrying if asked to. Returns
 * the success status.
 *
 * Don't use 'jiffies', so that we don't depend on interrupts
 */
static int send_data(unsigned char data)
{
	int retries = 3;

	do {
		unsigned long timeout = KBD_TIMEOUT;

		acknowledge = 0; /* Set by interrupt routine on receipt of ACK. */
		resend = 0;
		reply_expected = 1;
		kbd_write_output_w(data);
		for (;;) {
			if (acknowledge)
				return 1;
			if (resend)
				break;
			mdelay(1);
			if (!--timeout) {
#ifdef KBD_REPORT_TIMEOUTS
				printk(KERN_WARNING "keyboard: Timeout - AT keyboard not present?(%02x)\n", data);
#endif
				return 0;
			}
		}
	} while (retries-- > 0);
#ifdef KBD_REPORT_TIMEOUTS
	printk(KERN_WARNING "keyboard: Too many NACKs -- noisy kbd cable?\n");
#endif
	return 0;
}

void pckbd_leds(unsigned char leds)
{
	if (kbd_exists && (!send_data(KBD_CMD_SET_LEDS) || !send_data(leds))) {
		send_data(KBD_CMD_ENABLE);	/* re-enable kbd if any errors */
		kbd_exists = 0;
	}
}

#define DEFAULT_KEYB_REP_DELAY	250
#define DEFAULT_KEYB_REP_RATE	30	/* cps */

static struct kbd_repeat kbdrate={
	DEFAULT_KEYB_REP_DELAY,
	DEFAULT_KEYB_REP_RATE
};

static unsigned char parse_kbd_rate(struct kbd_repeat *r)
{
	static struct r2v{
		int rate;
		unsigned char val;
	} kbd_rates[]={	{5,0x14},
			{7,0x10},
			{10,0x0c},
			{15,0x08},
			{20,0x04},
			{25,0x02},
			{30,0x00}
	};
	static struct d2v{
		int delay;
		unsigned char val;
	} kbd_delays[]={{250,0},
			{500,1},
			{750,2},
			{1000,3}
	};
	int rate=0,delay=0;
	if (r != NULL){
		int i,new_rate=30,new_delay=250;
		if (r->rate <= 0)
			r->rate=kbdrate.rate;
		if (r->delay <= 0)
			r->delay=kbdrate.delay;
		for (i=0; i < sizeof(kbd_rates)/sizeof(struct r2v); i++)
			if (kbd_rates[i].rate == r->rate){
				new_rate=kbd_rates[i].rate;
				rate=kbd_rates[i].val;
				break;
			}
		for (i=0; i < sizeof(kbd_delays)/sizeof(struct d2v); i++)
			if (kbd_delays[i].delay == r->delay){
				new_delay=kbd_delays[i].delay;
				delay=kbd_delays[i].val;
				break;
			}
		r->rate=new_rate;
		r->delay=new_delay;
	}
	return (delay << 5) | rate;
}

static int write_kbd_rate(unsigned char r)
{
	if (!send_data(KBD_CMD_SET_RATE) || !send_data(r)){
		send_data(KBD_CMD_ENABLE); 	/* re-enable kbd if any errors */
		return 0;
	}else
		return 1;
}

static int pckbd_rate(struct kbd_repeat *rep)
{
	if (rep == NULL)
		return -EINVAL;
	else{
		unsigned char r=parse_kbd_rate(rep);
		struct kbd_repeat old_rep;
		memcpy(&old_rep,&kbdrate,sizeof(struct kbd_repeat));
		if (write_kbd_rate(r)){
			memcpy(&kbdrate,rep,sizeof(struct kbd_repeat));
			memcpy(rep,&old_rep,sizeof(struct kbd_repeat));
			return 0;
		}
	}
	return -EIO;
}

/*
 * In case we run on a non-x86 hardware we need to initialize both the
 * keyboard controller and the keyboard.  On a x86, the BIOS will
 * already have initialized them.
 *
 * Some x86 BIOSes do not correctly initialize the keyboard, so the
 * "kbd-reset" command line options can be given to force a reset.
 * [Ranger]
 */
#ifdef __i386__
 int kbd_startup_reset __initdata = 0;
#else
 int kbd_startup_reset __initdata = 1;
#endif

/* for "kbd-reset" cmdline param */
static int __init kbd_reset_setup(char *str)
{
	kbd_startup_reset = 1;
	return 1;
}

__setup("kbd-reset", kbd_reset_setup);

#define KBD_NO_DATA	(-1)	/* No data */
#define KBD_BAD_DATA	(-2)	/* Parity or other error */

static int __init kbd_read_data(void)
{
	int retval = KBD_NO_DATA;
	unsigned char status;

	status = kbd_read_status();
	if (status & KBD_STAT_OBF) {
		unsigned char data = kbd_read_input();

		retval = data;
		if (status & (KBD_STAT_GTO | KBD_STAT_PERR))
			retval = KBD_BAD_DATA;
	}
	return retval;
}

static void __init kbd_clear_input(void)
{
	int maxread = 100;	/* Random number */

	do {
		if (kbd_read_data() == KBD_NO_DATA)
			break;
	} while (--maxread);
}

static int __init kbd_wait_for_input(void)
{
	long timeout = KBD_INIT_TIMEOUT;

	do {
		int retval = kbd_read_data();
		if (retval >= 0)
			return retval;
		mdelay(1);
	} while (--timeout);
	return -1;
}

static void kbd_write_command_w(int data)
{
	unsigned long flags;

	spin_lock_irqsave(&kbd_controller_lock, flags);
	kb_wait();
	kbd_write_command(data);
	spin_unlock_irqrestore(&kbd_controller_lock, flags);
}

static void kbd_write_output_w(int data)
{
	unsigned long flags;

	spin_lock_irqsave(&kbd_controller_lock, flags);
	kb_wait();
	kbd_write_output(data);
	spin_unlock_irqrestore(&kbd_controller_lock, flags);
}

#if defined(__alpha__)
/*
 * Some Alphas cannot mask some/all interrupts, so we have to
 * make sure not to allow interrupts AT ALL when polling for
 * specific return values from the keyboard.
 *
 * I think this should work on any architecture, but for now, only Alpha.
 */
static int kbd_write_command_w_and_wait(int data)
{
	unsigned long flags;
	int input;

	spin_lock_irqsave(&kbd_controller_lock, flags);
	kb_wait();
	kbd_write_command(data);
	input = kbd_wait_for_input();
	spin_unlock_irqrestore(&kbd_controller_lock, flags);
	return input;
}

static int kbd_write_output_w_and_wait(int data)
{
	unsigned long flags;
	int input;

	spin_lock_irqsave(&kbd_controller_lock, flags);
	kb_wait();
	kbd_write_output(data);
	input = kbd_wait_for_input();
	spin_unlock_irqrestore(&kbd_controller_lock, flags);
	return input;
}
#else
static int kbd_write_command_w_and_wait(int data)
{
	kbd_write_command_w(data);
	return kbd_wait_for_input();
}

static int kbd_write_output_w_and_wait(int data)
{
	kbd_write_output_w(data);
	return kbd_wait_for_input();
}
#endif /* __alpha__ */

#if defined CONFIG_PSMOUSE
static void kbd_write_cmd(int cmd)
{
	unsigned long flags;

	spin_lock_irqsave(&kbd_controller_lock, flags);
	kb_wait();
	kbd_write_command(KBD_CCMD_WRITE_MODE);
	kb_wait();
	kbd_write_output(cmd);
	spin_unlock_irqrestore(&kbd_controller_lock, flags);
}
#endif /* CONFIG_PSMOUSE */

static char * __init initialize_kbd(void)
{
	int status;

	/*
	 * Test the keyboard interface.
	 * This seems to be the only way to get it going.
	 * If the test is successful a x55 is placed in the input buffer.
	 */
	kbd_write_command_w(KBD_CCMD_SELF_TEST);
	if (kbd_wait_for_input() != 0x55)
		return "Keyboard failed self test";

	/*
	 * Perform a keyboard interface test.  This causes the controller
	 * to test the keyboard clock and data lines.  The results of the
	 * test are placed in the input buffer.
	 */
	kbd_write_command_w(KBD_CCMD_KBD_TEST);
	if (kbd_wait_for_input() != 0x00)
		return "Keyboard interface failed self test";

	/*
	 * Enable the keyboard by allowing the keyboard clock to run.
	 */
	kbd_write_command_w(KBD_CCMD_KBD_ENABLE);

	/*
	 * Reset keyboard. If the read times out
	 * then the assumption is that no keyboard is
	 * plugged into the machine.
	 * This defaults the keyboard to scan-code set 2.
	 *
	 * Set up to try again if the keyboard asks for RESEND.
	 */
	do {
		kbd_write_output_w(KBD_CMD_RESET);
		status = kbd_wait_for_input();
		if (status == KBD_REPLY_ACK)
			break;
		if (status != KBD_REPLY_RESEND)
			return "Keyboard reset failed, no ACK";
	} while (1);

	if (kbd_wait_for_input() != KBD_REPLY_POR)
		return "Keyboard reset failed, no POR";

	/*
	 * Set keyboard controller mode. During this, the keyboard should be
	 * in the disabled state.
	 *
	 * Set up to try again if the keyboard asks for RESEND.
	 */
	do {
		kbd_write_output_w(KBD_CMD_DISABLE);
		status = kbd_wait_for_input();
		if (status == KBD_REPLY_ACK)
			break;
		if (status != KBD_REPLY_RESEND)
			return "Disable keyboard: no ACK";
	} while (1);

	kbd_write_command_w(KBD_CCMD_WRITE_MODE);
	kbd_write_output_w(KBD_MODE_KBD_INT
			      | KBD_MODE_SYS
			      | KBD_MODE_DISABLE_MOUSE
			      | KBD_MODE_KCC);

	/* ibm powerpc portables need this to use scan-code set 1 -- Cort */
	if (!(kbd_write_command_w_and_wait(KBD_CCMD_READ_MODE) & KBD_MODE_KCC))
	{
		/*
		 * If the controller does not support conversion,
		 * Set the keyboard to scan-code set 1.
		 */
		kbd_write_output_w(0xF0);
		kbd_wait_for_input();
		kbd_write_output_w(0x01);
		kbd_wait_for_input();
	}

	if (kbd_write_output_w_and_wait(KBD_CMD_ENABLE) != KBD_REPLY_ACK)
		return "Enable keyboard: no ACK";

	/*
	 * Finally, set the typematic rate to maximum.
	 */
	if (kbd_write_output_w_and_wait(KBD_CMD_SET_RATE) != KBD_REPLY_ACK)
		return "Set rate: no ACK";
	if (kbd_write_output_w_and_wait(0x00) != KBD_REPLY_ACK)
		return "Set rate: no 2nd ACK";

	return NULL;
}

void __init pckbd_init_hw(void)
{
	if (!kbd_controller_present()) {
		kbd_exists = 0;
		return;
	}

	kbd_request_region();

	/* Flush any pending input. */
	kbd_clear_input();

	if (kbd_startup_reset) {
		char *msg = initialize_kbd();
		if (msg)
			printk(KERN_WARNING "initialize_kbd: %s\n", msg);
	}

#if defined CONFIG_PSMOUSE
	psaux_init();
#endif

	kbd_rate = pckbd_rate;

	/* Ok, finally allocate the IRQ, and off we go.. */
	kbd_request_irq(keyboard_interrupt);
}

#if defined CONFIG_PSMOUSE

static int __init aux_reconnect_setup (char *str)
{
	aux_reconnect = 1;
	return 1;
}

__setup("psaux-reconnect", aux_reconnect_setup);

/*
 * Check if this is a dual port controller.
 */
static int __init detect_auxiliary_port(void)
{
	unsigned long flags;
	int loops = 10;
	int retval = 0;

	/* Check if the BIOS detected a device on the auxiliary port. */
	if (aux_device_present == 0xaa)
		return 1;

	spin_lock_irqsave(&kbd_controller_lock, flags);

	/* Put the value 0x5A in the output buffer using the "Write
	 * Auxiliary Device Output Buffer" command (0xD3). Poll the
	 * Status Register for a while to see if the value really
	 * turns up in the Data Register. If the KBD_STAT_MOUSE_OBF
	 * bit is also set to 1 in the Status Register, we assume this
	 * controller has an Auxiliary Port (a.k.a. Mouse Port).
	 */
	kb_wait();
	kbd_write_command(KBD_CCMD_WRITE_AUX_OBUF);

	kb_wait();
	kbd_write_output(0x5a); /* 0x5a is a random dummy value. */

	do {
		unsigned char status = kbd_read_status();

		if (status & KBD_STAT_OBF) {
			(void) kbd_read_input();
			if (status & KBD_STAT_MOUSE_OBF) {
				printk(KERN_INFO "Detected PS/2 Mouse Port.\n");
				retval = 1;
			}
			break;
		}
		mdelay(1);
	} while (--loops);
	spin_unlock_irqrestore(&kbd_controller_lock, flags);

	return retval;
}

/*
 * Send a byte to the mouse.
 */
static void aux_write_dev(int val)
{
	unsigned long flags;

	spin_lock_irqsave(&kbd_controller_lock, flags);
	kb_wait();
	kbd_write_command(KBD_CCMD_WRITE_MOUSE);
	kb_wait();
	kbd_write_output(val);
	spin_unlock_irqrestore(&kbd_controller_lock, flags);
}

/*
 * Send a byte to the mouse & handle returned ack
 */
static void __aux_write_ack(int val)
{
	kb_wait();
	kbd_write_command(KBD_CCMD_WRITE_MOUSE);
	kb_wait();
	kbd_write_output(val);
	/* we expect an ACK in response. */
	mouse_reply_expected++;
	kb_wait();
}

static void aux_write_ack(int val)
{
	unsigned long flags;

	spin_lock_irqsave(&kbd_controller_lock, flags);
	__aux_write_ack(val);
	spin_unlock_irqrestore(&kbd_controller_lock, flags);
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
	lock_kernel();
	fasync_aux(-1, file, 0);
	if (--aux_count) {
		unlock_kernel();
		return 0;
	}
	kbd_write_cmd(AUX_INTS_OFF);			    /* Disable controller ints */
	kbd_write_command_w(KBD_CCMD_MOUSE_DISABLE);
	aux_free_irq(AUX_DEV);
	unlock_kernel();
	return 0;
}

/*
 * Install interrupt handler.
 * Enable auxiliary device.
 */

static int open_aux(struct inode * inode, struct file * file)
{
	if (aux_count++) {
		return 0;
	}
	queue->head = queue->tail = 0;		/* Flush input queue */
	if (aux_request_irq(keyboard_interrupt, AUX_DEV)) {
		aux_count--;
		return -EBUSY;
	}
	kbd_write_command_w(KBD_CCMD_MOUSE_ENABLE);	/* Enable the
							   auxiliary port on
							   controller. */
	aux_write_ack(AUX_ENABLE_DEV); /* Enable aux device */
	kbd_write_cmd(AUX_INTS_ON); /* Enable controller ints */
	
	mdelay(2);			/* Ensure we follow the kbc access delay rules.. */

	send_data(KBD_CMD_ENABLE);	/* try to workaround toshiba4030cdt problem */

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

/* No kernel lock held - fine */
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

static int __init psaux_init(void)
{
	int retval;

	if (!detect_auxiliary_port())
		return -EIO;

	if ((retval = misc_register(&psaux_mouse)))
		return retval;

	queue = (struct aux_queue *) kmalloc(sizeof(*queue), GFP_KERNEL);
	if (queue == NULL) {
		printk(KERN_ERR "psaux_init(): out of memory\n");
		misc_deregister(&psaux_mouse);
		return -ENOMEM;
	}
	memset(queue, 0, sizeof(*queue));
	queue->head = queue->tail = 0;
	init_waitqueue_head(&queue->proc_list);

#ifdef INITIALIZE_MOUSE
	kbd_write_command_w(KBD_CCMD_MOUSE_ENABLE); /* Enable Aux. */
	aux_write_ack(AUX_SET_SAMPLE);
	aux_write_ack(100);			/* 100 samples/sec */
	aux_write_ack(AUX_SET_RES);
	aux_write_ack(3);			/* 8 counts per mm */
	aux_write_ack(AUX_SET_SCALE21);		/* 2:1 scaling */
#endif /* INITIALIZE_MOUSE */
	kbd_write_command(KBD_CCMD_MOUSE_DISABLE); /* Disable aux device. */
	kbd_write_cmd(AUX_INTS_OFF); /* Disable controller ints. */

	return 0;
}

#endif /* CONFIG_PSMOUSE */


static int blink_frequency = HZ/2;

/* Tell the user who may be running in X and not see the console that we have 
   panic'ed. This is to distingush panics from "real" lockups. 
   Could in theory send the panic message as morse, but that is left as an
   exercise for the reader.  */ 
void panic_blink(void)
{ 
	static unsigned long last_jiffie;
	static char led;
	/* Roughly 1/2s frequency. KDB uses about 1s. Make sure it is 
	   different. */
	if (!blink_frequency) 
		return;
	if (jiffies - last_jiffie > blink_frequency) {
		led ^= 0x01 | 0x04;
		while (kbd_read_status() & KBD_STAT_IBF) mdelay(1); 
		kbd_write_output(KBD_CMD_SET_LEDS);
		mdelay(1); 
		while (kbd_read_status() & KBD_STAT_IBF) mdelay(1); 
		mdelay(1); 
		kbd_write_output(led);
		last_jiffie = jiffies;
	}
}  

static int __init panicblink_setup(char *str)
{
    int par;
    if (get_option(&str,&par)) 
	    blink_frequency = par*(1000/HZ);
    return 1;
}

/* panicblink=0 disables the blinking as it caused problems with some console
   switches. otherwise argument is ms of a blink period. */
__setup("panicblink=", panicblink_setup);

