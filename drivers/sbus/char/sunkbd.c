/* keyboard.c: Sun keyboard driver.
 *
 * Copyright (C) 1995, 1996, 1997 David S. Miller (davem@caip.rutgers.edu)
 *
 * Added vuid event generation and /dev/kbd device for SunOS
 * compatibility - Miguel (miguel@nuclecu.unam.mx)
 *
 * Added PCI 8042 controller support -DaveM
 * Added Magic SysRq support -MJ
 */

#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/mm.h>
#include <linux/ptrace.h>
#include <linux/signal.h>
#include <linux/string.h>
#include <linux/fcntl.h>
#include <linux/poll.h>
#include <linux/random.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/sysrq.h>
#include <linux/spinlock.h>
#include <linux/smp_lock.h>
#include <linux/devfs_fs_kernel.h>

#include <asm/kbio.h>
#include <asm/vuid_event.h>
#include <asm/bitops.h>
#include <asm/oplib.h>
#include <asm/uaccess.h>

#include <linux/kbd_kern.h>
#include <linux/kbd_diacr.h>
#include <linux/vt_kern.h>

#ifdef CONFIG_PCI
#include <linux/pci.h>
#include <asm/pbm.h>
#include <asm/ebus.h>
#endif

#include "sunkbd.h"

#define SIZE(x) (sizeof(x)/sizeof((x)[0]))

/* Define this one if you are making a new frame buffer driver */
/* it will not block the keyboard */
/* #define CODING_NEW_DRIVER */

/* KBD device number, temporal */
#define KBD_MAJOR 11

#define KBD_REPORT_ERR
#define KBD_REPORT_UNKN

#ifndef KBD_DEFMODE
#define KBD_DEFMODE ((1 << VC_REPEAT) | (1 << VC_META))
#endif

#ifndef KBD_DEFLEDS
/*
 * Some laptops take the 789uiojklm,. keys as number pad when NumLock
 * is on. This seems a good reason to start with NumLock off.
 */
#define KBD_DEFLEDS 0
#endif

#ifndef KBD_DEFLOCK
#define KBD_DEFLOCK 0
#endif

extern void poke_blanked_console(void);
extern void ctrl_alt_del(void);
extern void reset_vc(unsigned int new_console);
extern void scrollback(int);
extern void scrollfront(int);

struct l1a_kbd_state l1a_state;

static spinlock_t sunkbd_lock = SPIN_LOCK_UNLOCKED;

/*
 * global state includes the following, and various static variables
 * in this module: prev_scancode, shift_state, diacr, npadch, dead_key_next.
 * (last_console is now a global variable)
 */

/* shift state counters.. */
static unsigned char k_down[NR_SHIFT];
/* keyboard key bitmap */
static unsigned long key_down[256/BITS_PER_LONG];

void push_kbd (int scan);
int kbd_redirected;

static int dead_key_next;
/* 
 * In order to retrieve the shift_state (for the mouse server), either
 * the variable must be global, or a new procedure must be created to 
 * return the value. I chose the former way.
 */
#ifndef CONFIG_PCI
int shift_state;
struct kbd_struct kbd_table[MAX_NR_CONSOLES];
#endif
static int npadch = -1;			/* -1 or number assembled on pad */
static unsigned char diacr;
static char rep;			/* flag telling character repeat */
static struct tty_struct **ttytab;
static struct kbd_struct * kbd = kbd_table;
static struct tty_struct * tty;
static int compose_led_on;
static int kbd_delay_ticks = HZ / 5;
static int kbd_rate_ticks = HZ / 20;

void sun_compute_shiftstate(void);

typedef void (*k_hand)(unsigned char value, char up_flag);
typedef void (k_handfn)(unsigned char value, char up_flag);

static k_handfn
	do_self, do_fn, do_spec, do_pad, do_dead, do_cons, do_cur, do_shift,
	do_meta, do_ascii, do_lock, do_lowercase, do_ignore;

static k_hand key_handler[16] = {
	do_self, do_fn, do_spec, do_pad, do_dead, do_cons, do_cur, do_shift,
	do_meta, do_ascii, do_lock, do_lowercase,
	do_ignore, do_ignore, do_ignore, do_ignore
};

typedef void (*void_fnp)(void);
typedef void (void_fn)(void);

static void_fn do_null, enter, show_ptregs, send_intr, lastcons, caps_toggle,
	num, hold, scroll_forw, scroll_back, boot_it, caps_on, compose,
	SAK, decr_console, incr_console, spawn_console, bare_num;

static void_fnp spec_fn_table[] = {
	do_null,	enter,		show_ptregs,	show_mem,
	show_state,	send_intr,	lastcons,	caps_toggle,
	num,		hold,		scroll_forw,	scroll_back,
	boot_it,	caps_on,	compose,	SAK,
	decr_console,	incr_console,	spawn_console,	bare_num
};

/* maximum values each key_handler can handle */
#ifndef CONFIG_PCI
const int max_vals[] = {
	255, SIZE(func_table) - 1, SIZE(spec_fn_table) - 1, NR_PAD - 1,
	NR_DEAD - 1, 255, 3, NR_SHIFT - 1,
	255, NR_ASCII - 1, NR_LOCK - 1, 255,
	NR_LOCK - 1
};

const int NR_TYPES = SIZE(max_vals);
#endif

static void put_queue(int);
static unsigned char handle_diacr(unsigned char);

/* pt_regs - set by keyboard_interrupt(), used by show_ptregs() */
static struct pt_regs * pt_regs;

#ifdef CONFIG_MAGIC_SYSRQ
unsigned char sun_sysrq_xlate[128] =
	"\0\0\0\0\0\201\202\212\203\213\204\214\205\0\206\0"	/* 0x00 - 0x0f */
	"\207\210\211\0\0\0\0\0\0\0\0\0\0\03312"		/* 0x10 - 0x1f */
	"34567890-=`\177\0=/*"					/* 0x20 - 0x2f */
	"\0\0.\0\0\011qwertyuiop"				/* 0x30 - 0x3f */
	"[]\177\000789-\0\0\0\0\0asd"				/* 0x40 - 0x4f */
	"fghjkl;'\\\015\0154560\0"				/* 0x50 - 0x5f */
	"\0\0\0\0zxcvbnm,./\0\012"				/* 0x60 - 0x6f */
	"123\0\0\0\0\0\0 \0\0\0\0\0\0";				/* 0x70 - 0x7f */
#endif

volatile unsigned char sunkbd_layout;
volatile unsigned char sunkbd_type;
#define SUNKBD_TYPE2        0x02
#define SUNKBD_TYPE3        0x03
#define SUNKBD_TYPE4        0x04

#define SUNKBD_LOUT_TYP4    	0x00
#define SUNKBD_LOUT_TYP5_MASK   0x20

volatile int kbd_reset_pending;
volatile int kbd_layout_pending;

/* commands */
#define SKBDCMD_RESET       0x1
#define SKBDCMD_GLAYOUT     0xf
#define SKBDCMD_BELLON      0x2
#define SKBDCMD_BELLOFF     0x3
#define SKBDCMD_SETLED      0xe
#define SKBDCMD_NOCLICK     0xb
#define SKBDCMD_CLICK       0xa

static unsigned char sunkbd_clickp;

/* The led set commands require sending the SETLED byte then
 * a byte encoding which led's to have set.  Here are the bit
 * values, a bit set = led-on.
 */
#define LED_NLOCK           0x1   /* Num-lock */
#define LED_CMPOSE          0x2   /* Compose */
#define LED_SCRLCK          0x4   /* Scroll-lock */
#define LED_CLOCK           0x8   /* Caps-lock */

/* Special state characters */
#define SKBD_RESET          0xff
#define SKBD_ALLUP          0x7f
#define SKBD_LYOUT          0xfe

/* On the Sparc the keyboard could be one of two things.
 * It could be a real keyboard speaking over one of the
 * channels of the second zs8530 chip (other channel is
 * used by the Sun mouse).  Else we have serial console
 * going, and thus the other zs8530 chip is who we speak
 * to.  Either way, we communicate through the zs8530
 * driver for all our I/O.
 */

#define SUNKBD_UBIT     0x80      /* If set, key went up */
#define SUNKBD_KMASK    0x7f      /* Other bits are the keycode */

#define KEY_LSHIFT      0x81
#define KEY_RSHIFT      0x82
#define KEY_CONTROL     0x83
#define KEY_NILL        0x84
#define KEY_CAPSLOCK    0x85
#define KEY_ALT         0x86
#define KEY_L1          0x87

/* Due to sun_kbd_init() being called before rs_init(), and sun_kbd_init() doing:
 *
 *	tasklet_enable(&keyboard_tasklet);
 *	tasklet_schedule(&keyboard_tasklet);
 *
 * this might well be called before some driver has claimed interest in
 * handling the keyboard input/output. So we need to assign an initial nop.
 */
static void nop_kbd_put_char(unsigned char c) { }
static void (*kbd_put_char)(unsigned char) = nop_kbd_put_char;

/* Must be invoked under sunkbd_lock. */
static inline void send_cmd(unsigned char c)
{
	kbd_put_char(c);
}

/* kbd_bh() calls this to send the SKBDCMD_SETLED to the sun keyboard
 * with the proper bit pattern for the leds to be set.  It basically
 * converts the kbd->ledflagstate values to corresponding sun kbd led
 * bit value.
 */
static inline unsigned char vcleds_to_sunkbd(unsigned char vcleds)
{
	unsigned char retval = 0;

	if(vcleds & (1<<VC_SCROLLOCK))
		retval |= LED_SCRLCK;
	if(vcleds & (1<<VC_NUMLOCK))
		retval |= LED_NLOCK;
	if(vcleds & (1<<VC_CAPSLOCK))
		retval |= LED_CLOCK;
	if(compose_led_on)
		retval |= LED_CMPOSE;
	return retval;
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

/* we use this map to determine if a particular key should not be
   autorepeated. We don't autorepeat CONTROL, LSHIFT, CAPS,
   ALT, LMETA, RSHIFT, RMETA, ALTG and COMPOSE */
static unsigned char norepeat_keys[128] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0,  /* 0x00-0x0f */
	0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x10-0x1f */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x20-0x2f */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x30-0x3f */
	0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0,  /* 0x40-0x4f */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x50-0x5f */
	0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0,  /* 0x60-0x6f */
	0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 1, 0, 0, 0, 0, 0,  /* 0x70-0x7f */
};


int sun_setkeycode(unsigned int scancode, unsigned int keycode)
{
	if (scancode < SC_LIM || scancode > 255 || keycode > 127)
	  return -EINVAL;
	if (scancode < 128)
	  high_keys[scancode - SC_LIM] = keycode;
	else
	  e0_keys[scancode - 128] = keycode;
	return 0;
}

int sun_getkeycode(unsigned int scancode)
{
	return
	  (scancode < SC_LIM || scancode > 255) ? -EINVAL :
	  (scancode < 128) ? high_keys[scancode - SC_LIM] :
	    e0_keys[scancode - 128];
}

static void __sunkbd_inchar(unsigned char ch, struct pt_regs *regs);
void sunkbd_inchar(unsigned char ch, struct pt_regs *regs);
static void keyboard_timer (unsigned long ignored);

static struct timer_list
auto_repeat_timer = { function: keyboard_timer };

/* Keeps track of the last pressed key */
static unsigned char last_keycode;

static void
keyboard_timer (unsigned long ignored)
{
	unsigned long flags;

	spin_lock_irqsave(&sunkbd_lock, flags);

	/* Auto repeat: send regs = 0 to indicate autorepeat */
	__sunkbd_inchar (last_keycode, 0);
	del_timer (&auto_repeat_timer);
	if (kbd_rate_ticks) {
		auto_repeat_timer.expires = jiffies + kbd_rate_ticks;
		add_timer (&auto_repeat_timer);
	}

	spin_unlock_irqrestore(&sunkbd_lock, flags);
}

#ifndef CONFIG_PCI
DECLARE_TASKLET_DISABLED(keyboard_tasklet, sun_kbd_bh, 0);
#endif

/* #define SKBD_DEBUG */
/* This is our keyboard 'interrupt' routine.
 * Must run under sunkbd_lock.
 */
static void __sunkbd_inchar(unsigned char ch, struct pt_regs *regs)
{
	unsigned char keycode;
	char up_flag;                          /* 0 or SUNKBD_UBIT */
	char raw_mode;

	if(ch == SKBD_RESET) {
		kbd_reset_pending = 1;
		goto out;
	}
	if(ch == SKBD_LYOUT) {
		kbd_layout_pending = 1;
		goto out;
	}
	if(kbd_reset_pending) {
		sunkbd_type = ch;
		kbd_reset_pending = 0;
		if(ch == SUNKBD_TYPE4)
			send_cmd(SKBDCMD_GLAYOUT);
		goto out;
	} else if(kbd_layout_pending) {
		sunkbd_layout = ch;
		kbd_layout_pending = 0;
		goto out;
	} else if(ch == SKBD_ALLUP) {
		del_timer (&auto_repeat_timer);
		memset(key_down, 0, sizeof(key_down));
		sun_compute_shiftstate();
		goto out;
	}
#ifdef SKBD_DEBUG
	if(ch == 0x7f)
		printk("KBD<ALL KEYS UP>");
	else
		printk("KBD<%x %s>", ch,
		       ((ch&0x80) ? "UP" : "DOWN"));
#endif

	/* Whee, a real character. */
	if(regs) {
		pt_regs = regs;
		last_keycode = keycode = ch;
	} else {
		keycode = ch;
	}
	
	do_poke_blanked_console = 1;
	schedule_console_callback();
	add_keyboard_randomness(keycode);

	tty = ttytab? ttytab[fg_console]: NULL;
	if (tty && (!tty->driver_data)) {
		/* This is to workaround ugly bug in tty_io.c, which
                   does not do locking when it should */
		tty = NULL;
	}
	kbd = kbd_table + fg_console;
	if((raw_mode = (kbd->kbdmode == VC_RAW))) {
		if (kbd_redirected == fg_console+1)
			push_kbd (keycode);
		else
			put_queue(keycode);
		/* we do not return yet, because we want to maintain
		 * the key_down array, so that we have the correct
		 * values  when finishing RAW mode or when changing VT's.
		 */
	}
	up_flag = (keycode & SUNKBD_UBIT);  /* The 'up' bit */
	keycode &= SUNKBD_KMASK;            /* all the rest */
	del_timer (&auto_repeat_timer);
	if(up_flag) {
		rep = 0;
		clear_bit(keycode, key_down);
	} else {
		if (!norepeat_keys[keycode]) {
			if (kbd_rate_ticks) {
				auto_repeat_timer.expires =
						jiffies + kbd_delay_ticks;
				add_timer (&auto_repeat_timer);
			}
		}
		rep = test_and_set_bit(keycode, key_down);
	}

#ifdef CONFIG_MAGIC_SYSRQ			/* Handle the SysRq hack */
	if (l1a_state.l1_down) {
		if (!up_flag)
			handle_sysrq(sun_sysrq_xlate[keycode], pt_regs, kbd, tty);
		goto out;
	}
#endif

	if(raw_mode)
		goto out;

	if(kbd->kbdmode == VC_MEDIUMRAW) {
		put_queue(keycode + up_flag);
		goto out;
	}

 	/*
	 * Small change in philosophy: earlier we defined repetition by
	 *	 rep = keycode == prev_keycode;
	 *	 prev_keycode = keycode;
	 * but now by the fact that the depressed key was down already.
	 * Does this ever make a difference? Yes.
	 */

	/*
 	 *  Repeat a key only if the input buffers are empty or the
 	 *  characters get echoed locally. This makes key repeat usable
 	 *  with slow applications and under heavy loads.
	 */
	if (!rep ||
	    (vc_kbd_mode(kbd,VC_REPEAT) && tty &&
	     (L_ECHO(tty) || (tty->driver.chars_in_buffer(tty) == 0)))) {
		u_short keysym;
		u_char type;

		/* the XOR below used to be an OR */
		int shift_final = shift_state ^ kbd->lockstate ^ kbd->slockstate;
		ushort *key_map = key_maps[shift_final];

		if (key_map != NULL) {
			keysym = key_map[keycode];
			type = KTYP(keysym);

			if (type >= 0xf0) {
			    type -= 0xf0;
			    if (type == KT_LETTER) {
				type = KT_LATIN;
				if (vc_kbd_led(kbd, VC_CAPSLOCK)) {
				    key_map = key_maps[shift_final ^ (1<<KG_SHIFT)];
				    if (key_map)
				      keysym = key_map[keycode];
				}
			    }
			    (*key_handler[type])(keysym & 0xff, up_flag);
			    if (type != KT_SLOCK)
			      kbd->slockstate = 0;
			}
		} else {
			/* maybe beep? */
			/* we have at least to update shift_state */
			sun_compute_shiftstate();
		}
	}
out:
	tasklet_schedule(&keyboard_tasklet);
}

void sunkbd_inchar(unsigned char ch, struct pt_regs *regs)
{
	unsigned long flags;

	spin_lock_irqsave(&sunkbd_lock, flags);
	__sunkbd_inchar(ch, regs);
	spin_unlock_irqrestore(&sunkbd_lock, flags);
}

static void put_queue(int ch)
{
	if (tty) {
		tty_insert_flip_char(tty, ch, 0);
		con_schedule_flip(tty);
	}
}

static void puts_queue(char *cp)
{
	if (!tty)
		return;

	while (*cp) {
		tty_insert_flip_char(tty, *cp, 0);
		cp++;
	}
	con_schedule_flip(tty);
}

static void applkey(int key, char mode)
{
	static char buf[] = { 0x1b, 'O', 0x00, 0x00 };

	buf[1] = (mode ? 'O' : '[');
	buf[2] = key;
	puts_queue(buf);
}

static void enter(void)
{
	put_queue(13);
	if (vc_kbd_mode(kbd,VC_CRLF))
		put_queue(10);
}

static void caps_toggle(void)
{
	if (rep)
		return;
	chg_vc_kbd_led(kbd, VC_CAPSLOCK);
}

static void caps_on(void)
{
	if (rep)
		return;
	set_vc_kbd_led(kbd, VC_CAPSLOCK);
}

static void show_ptregs(void)
{
	if (pt_regs)
		show_regs(pt_regs);
}

static void hold(void)
{
	if (rep || !tty)
		return;

	/*
	 * Note: SCROLLOCK will be set (cleared) by stop_tty (start_tty);
	 * these routines are also activated by ^S/^Q.
	 * (And SCROLLOCK can also be set by the ioctl KDSKBLED.)
	 */
	if (tty->stopped)
		start_tty(tty);
	else
		stop_tty(tty);
}

static void num(void)
{
	if (vc_kbd_mode(kbd,VC_APPLIC))
		applkey('P', 1);
	else
		bare_num();
}

/*
 * Bind this to Shift-NumLock if you work in application keypad mode
 * but want to be able to change the NumLock flag.
 * Bind this to NumLock if you prefer that the NumLock key always
 * changes the NumLock flag.
 */
static void bare_num(void)
{
	if (!rep)
		chg_vc_kbd_led(kbd,VC_NUMLOCK);
}

static void lastcons(void)
{
	/* switch to the last used console, ChN */
	set_console(last_console);
}

static void decr_console(void)
{
	int i;
 
	for (i = fg_console-1; i != fg_console; i--) {
		if (i == -1)
			i = MAX_NR_CONSOLES-1;
		if (vc_cons_allocated(i))
			break;
	}
	set_console(i);
}

static void incr_console(void)
{
	int i;

	for (i = fg_console+1; i != fg_console; i++) {
		if (i == MAX_NR_CONSOLES)
			i = 0;
		if (vc_cons_allocated(i))
			break;
	}
	set_console(i);
}

static void send_intr(void)
{
	if (!tty)
		return;
	tty_insert_flip_char(tty, 0, TTY_BREAK);
	con_schedule_flip(tty);
}

static void scroll_forw(void)
{
	scrollfront(0);
}

static void scroll_back(void)
{
	scrollback(0);
}

static void boot_it(void)
{
	extern int obp_system_intr(void);

	if (!obp_system_intr())
		ctrl_alt_del();
	/* sigh.. attempt to prevent multiple entry */
	last_keycode=1;
	rep = 0;
}

static void compose(void)
{
	dead_key_next = 1;
	compose_led_on = 1;
	set_leds();
}

#ifdef CONFIG_PCI
extern int spawnpid, spawnsig;
#else
int spawnpid, spawnsig;
#endif


static void spawn_console(void)
{
        if (spawnpid)
	   if(kill_proc(spawnpid, spawnsig, 1))
	     spawnpid = 0;
}

static void SAK(void)
{
	do_SAK(tty);
#if 0
	/*
	 * Need to fix SAK handling to fix up RAW/MEDIUM_RAW and
	 * vt_cons modes before we can enable RAW/MEDIUM_RAW SAK
	 * handling.
	 * 
	 * We should do this some day --- the whole point of a secure
	 * attention key is that it should be guaranteed to always
	 * work.
	 */
	reset_vc(fg_console);
	do_unblank_screen();	/* not in interrupt routine? */
#endif
}

static void do_ignore(unsigned char value, char up_flag)
{
}

static void do_null()
{
	sun_compute_shiftstate();
}

static void do_spec(unsigned char value, char up_flag)
{
	if (up_flag)
		return;
	if (value >= SIZE(spec_fn_table))
		return;
	spec_fn_table[value]();
}

static void do_lowercase(unsigned char value, char up_flag)
{
	printk("keyboard.c: do_lowercase was called - impossible\n");
}

static void do_self(unsigned char value, char up_flag)
{
	if (up_flag)
		return;		/* no action, if this is a key release */

	if (diacr) {
		value = handle_diacr(value);
		compose_led_on = 0;
		set_leds();
	}

	if (dead_key_next) {
		dead_key_next = 0;
		diacr = value;
		return;
	}

	put_queue(value);
}

#define A_GRAVE  '`'
#define A_ACUTE  '\''
#define A_CFLEX  '^'
#define A_TILDE  '~'
#define A_DIAER  '"'
#define A_CEDIL  ','
static unsigned char ret_diacr[NR_DEAD] =
	{A_GRAVE, A_ACUTE, A_CFLEX, A_TILDE, A_DIAER, A_CEDIL };

/* If a dead key pressed twice, output a character corresponding to it,	*/
/* otherwise just remember the dead key.				*/

static void do_dead(unsigned char value, char up_flag)
{
	if (up_flag)
		return;

	value = ret_diacr[value];
	if (diacr == value) {   /* pressed twice */
		diacr = 0;
		put_queue(value);
		return;
	}
	diacr = value;
}


/* If space is pressed, return the character corresponding the pending	*/
/* dead key, otherwise try to combine the two.				*/

unsigned char handle_diacr(unsigned char ch)
{
	int d = diacr;
	int i;

	diacr = 0;
	if (ch == ' ')
		return d;

	for (i = 0; i < accent_table_size; i++) {
		if (accent_table[i].diacr == d && accent_table[i].base == ch)
			return accent_table[i].result;
	}

	put_queue(d);
	return ch;
}

static void do_cons(unsigned char value, char up_flag)
{
	if (up_flag)
		return;
	set_console(value);
}

static void do_fn(unsigned char value, char up_flag)
{
	if (up_flag)
		return;
	if (value < SIZE(func_table)) {
		if (func_table[value])
			puts_queue(func_table[value]);
	} else
		printk("do_fn called with value=%d\n", value);
}

static void do_pad(unsigned char value, char up_flag)
{
	static const char *pad_chars = "0123456789+-*/\015,.?";
	static const char *app_map = "pqrstuvwxylSRQMnn?";

	if (up_flag)
		return;		/* no action, if this is a key release */

	/* kludge... shift forces cursor/number keys */
	if (vc_kbd_mode(kbd,VC_APPLIC) && !k_down[KG_SHIFT]) {
		applkey(app_map[value], 1);
		return;
	}

	if (!vc_kbd_led(kbd,VC_NUMLOCK))
		switch (value) {
			case KVAL(K_PCOMMA):
			case KVAL(K_PDOT):
				do_fn(KVAL(K_REMOVE), 0);
				return;
			case KVAL(K_P0):
				do_fn(KVAL(K_INSERT), 0);
				return;
			case KVAL(K_P1):
				do_fn(KVAL(K_SELECT), 0);
				return;
			case KVAL(K_P2):
				do_cur(KVAL(K_DOWN), 0);
				return;
			case KVAL(K_P3):
				do_fn(KVAL(K_PGDN), 0);
				return;
			case KVAL(K_P4):
				do_cur(KVAL(K_LEFT), 0);
				return;
			case KVAL(K_P6):
				do_cur(KVAL(K_RIGHT), 0);
				return;
			case KVAL(K_P7):
				do_fn(KVAL(K_FIND), 0);
				return;
			case KVAL(K_P8):
				do_cur(KVAL(K_UP), 0);
				return;
			case KVAL(K_P9):
				do_fn(KVAL(K_PGUP), 0);
				return;
			case KVAL(K_P5):
				applkey('G', vc_kbd_mode(kbd, VC_APPLIC));
				return;
		}

	put_queue(pad_chars[value]);
	if (value == KVAL(K_PENTER) && vc_kbd_mode(kbd, VC_CRLF))
		put_queue(10);
}

static void do_cur(unsigned char value, char up_flag)
{
	static const char *cur_chars = "BDCA";
	if (up_flag)
		return;

	applkey(cur_chars[value], vc_kbd_mode(kbd,VC_CKMODE));
}

static void do_shift(unsigned char value, char up_flag)
{
	int old_state = shift_state;

	if (rep)
		return;

	/* Mimic typewriter:
	   a CapsShift key acts like Shift but undoes CapsLock */
	if (value == KVAL(K_CAPSSHIFT)) {
		value = KVAL(K_SHIFT);
		if (!up_flag)
			clr_vc_kbd_led(kbd, VC_CAPSLOCK);
	}

	if (up_flag) {
		/* handle the case that two shift or control
		   keys are depressed simultaneously */
		if (k_down[value])
			k_down[value]--;
	} else
		k_down[value]++;

	if (k_down[value])
		shift_state |= (1 << value);
	else
		shift_state &= ~ (1 << value);

	/* kludge, no joke... */
	if (up_flag && shift_state != old_state && npadch != -1) {
		put_queue(npadch & 0xff);
		npadch = -1;
	}
}

/* called after returning from RAW mode or when changing consoles -
   recompute k_down[] and shift_state from key_down[] */
/* maybe called when keymap is undefined, so that shiftkey release is seen */
void sun_compute_shiftstate(void)
{
	int i, j, k, sym, val;

	shift_state = 0;
	for(i=0; i < SIZE(k_down); i++)
	  k_down[i] = 0;

	for(i=0; i < SIZE(key_down); i++)
	  if(key_down[i]) {	/* skip this word if not a single bit on */
	    k = i*BITS_PER_LONG;
	    for(j=0; j<BITS_PER_LONG; j++,k++)
	      if(test_bit(k, key_down)) {
		sym = U(plain_map[k]);
		if(KTYP(sym) == KT_SHIFT) {
		  val = KVAL(sym);
		  if (val == KVAL(K_CAPSSHIFT))
		    val = KVAL(K_SHIFT);
		  k_down[val]++;
		  shift_state |= (1<<val);
		}
	      }
	  }
}

static void do_meta(unsigned char value, char up_flag)
{
	if (up_flag)
		return;

	if (vc_kbd_mode(kbd, VC_META)) {
		put_queue('\033');
		put_queue(value);
	} else
		put_queue(value | 0x80);
}

static void do_ascii(unsigned char value, char up_flag)
{
	int base;

	if (up_flag)
		return;

	if (value < 10)    /* decimal input of code, while Alt depressed */
	    base = 10;
	else {       /* hexadecimal input of code, while AltGr depressed */
	    value -= 10;
	    base = 16;
	}

	if (npadch == -1)
	  npadch = value;
	else
	  npadch = npadch * base + value;
}

static void do_lock(unsigned char value, char up_flag)
{
	if (up_flag || rep)
		return;
	chg_vc_kbd_lock(kbd, value);
}

/*
 * The leds display either (i) the status of NumLock, CapsLock, ScrollLock,
 * or (ii) whatever pattern of lights people want to show using KDSETLED,
 * or (iii) specified bits of specified words in kernel memory.
 */

static unsigned char ledstate = 0xff; /* undefined */
static unsigned char ledioctl;

unsigned char sun_getledstate(void) {
    return ledstate;
}

void sun_setledstate(struct kbd_struct *kbd, unsigned int led) {
    if (!(led & ~7)) {
	ledioctl = led;
	kbd->ledmode = LED_SHOW_IOCTL;
    } else
	kbd->ledmode = LED_SHOW_FLAGS;
    set_leds();
}

static struct ledptr {
    unsigned int *addr;
    unsigned int mask;
    unsigned char valid:1;
} ledptrs[3];

void register_leds(int console, unsigned int led,
		   unsigned int *addr, unsigned int mask) {
    struct kbd_struct *kbd = kbd_table + console;
    if (led < 3) {
	ledptrs[led].addr = addr;
	ledptrs[led].mask = mask;
	ledptrs[led].valid = 1;
	kbd->ledmode = LED_SHOW_MEM;
    } else
	kbd->ledmode = LED_SHOW_FLAGS;
}

static inline unsigned char getleds(void){
    struct kbd_struct *kbd = kbd_table + fg_console;
    unsigned char leds;

    if (kbd->ledmode == LED_SHOW_IOCTL)
      return ledioctl;
    leds = kbd->ledflagstate;
    if (kbd->ledmode == LED_SHOW_MEM) {
	if (ledptrs[0].valid) {
	    if (*ledptrs[0].addr & ledptrs[0].mask)
	      leds |= 1;
	    else
	      leds &= ~1;
	}
	if (ledptrs[1].valid) {
	    if (*ledptrs[1].addr & ledptrs[1].mask)
	      leds |= 2;
	    else
	      leds &= ~2;
	}
	if (ledptrs[2].valid) {
	    if (*ledptrs[2].addr & ledptrs[2].mask)
	      leds |= 4;
	    else
	      leds &= ~4;
	}
    }
    return leds;
}

/*
 * This routine is the bottom half of the keyboard interrupt
 * routine, and runs with all interrupts enabled. It does
 * console changing, led setting and copy_to_cooked, which can
 * take a reasonably long time.
 *
 * Aside from timing (which isn't really that important for
 * keyboard interrupts as they happen often), using the software
 * interrupt routines for this thing allows us to easily mask
 * this when we don't want any of the above to happen. Not yet
 * used, but this allows for easy and efficient race-condition
 * prevention later on.
 */
static unsigned char sunkbd_ledstate = 0xff; /* undefined */
void sun_kbd_bh(unsigned long dummy)
{
	unsigned long flags;
	unsigned char leds, kbd_leds;

	spin_lock_irqsave(&sunkbd_lock, flags);

	leds = getleds();
	kbd_leds = vcleds_to_sunkbd(leds);
	if (kbd_leds != sunkbd_ledstate) {
		ledstate = leds;
		sunkbd_ledstate = kbd_leds;
		send_cmd(SKBDCMD_SETLED);
		send_cmd(kbd_leds);
	}

	spin_unlock_irqrestore(&sunkbd_lock, flags);
}

/* Support for keyboard "beeps". */ 

/* Timer routine to turn off the beep after the interval expires. */
static void sunkbd_kd_nosound(unsigned long __unused)
{
	unsigned long flags;

	spin_lock_irqsave(&sunkbd_lock, flags);
	send_cmd(SKBDCMD_BELLOFF);
	spin_unlock_irqrestore(&sunkbd_lock, flags);
}

/*
 * Initiate a keyboard beep. If the frequency is zero, then we stop
 * the beep. Any other frequency will start a monotone beep. The beep
 * will be stopped by a timer after "ticks" jiffies. If ticks is 0,
 * then we do not start a timer.
 */
static void sunkbd_kd_mksound(unsigned int hz, unsigned int ticks)
{
	unsigned long flags;
	static struct timer_list sound_timer = { function: sunkbd_kd_nosound };

	spin_lock_irqsave(&sunkbd_lock, flags);

	del_timer(&sound_timer);

	if (hz) {
		send_cmd(SKBDCMD_BELLON);
		if (ticks) {
			sound_timer.expires = jiffies + ticks;
			add_timer(&sound_timer);
		}
	} else
		send_cmd(SKBDCMD_BELLOFF);

	spin_unlock_irqrestore(&sunkbd_lock, flags);
}

extern void (*kd_mksound)(unsigned int hz, unsigned int ticks);

int __init sun_kbd_init(void)
{
	int i, opt_node;
	struct kbd_struct kbd0;
	extern struct tty_driver console_driver;

	kbd0.ledflagstate = kbd0.default_ledflagstate = KBD_DEFLEDS;
	kbd0.ledmode = LED_SHOW_FLAGS;
	kbd0.lockstate = KBD_DEFLOCK;
	kbd0.slockstate = 0;
	kbd0.modeflags = KBD_DEFMODE;
	kbd0.kbdmode = VC_XLATE;
 
	for (i = 0 ; i < MAX_NR_CONSOLES ; i++)
		kbd_table[i] = kbd0;

	ttytab = console_driver.table;

	kd_mksound = sunkbd_kd_mksound;

	/* XXX Check keyboard-click? property in 'options' PROM node XXX */
	if(sparc_cpu_model != sun4) {
		opt_node = prom_getchild(prom_root_node);
		opt_node = prom_searchsiblings(opt_node, "options");
		i = prom_getintdefault(opt_node, "keyboard-click?", -1);
		if(i != -1)
			sunkbd_clickp = 1;
		else
			sunkbd_clickp = 0;
	} else {
		sunkbd_clickp = 0;
	}

	keyboard_tasklet.func = sun_kbd_bh;

	tasklet_enable(&keyboard_tasklet);
	tasklet_schedule(&keyboard_tasklet);

	return 0;
}

/* /dev/kbd support */

#define KBD_QSIZE 32
static Firm_event kbd_queue [KBD_QSIZE];
static int kbd_head, kbd_tail;
static spinlock_t kbd_queue_lock = SPIN_LOCK_UNLOCKED;
char kbd_opened;
static int kbd_active = 0;
static DECLARE_WAIT_QUEUE_HEAD(kbd_wait);
static struct fasync_struct *kb_fasync;

void
push_kbd (int scan)
{
	unsigned long flags;
	int next;

	if (scan == KBD_IDLE)
		return;

	spin_lock_irqsave(&kbd_queue_lock, flags);
	next = (kbd_head + 1) % KBD_QSIZE;
	if (next != kbd_tail){
		kbd_queue [kbd_head].id = scan & KBD_KEYMASK;
		kbd_queue [kbd_head].value=scan & KBD_UP ? VKEY_UP : VKEY_DOWN;
		kbd_queue [kbd_head].time = xtime;
		kbd_head = next;
	}
	spin_unlock_irqrestore(&kbd_queue_lock, flags);

	kill_fasync (&kb_fasync, SIGIO, POLL_IN);
	wake_up_interruptible (&kbd_wait);
}

static ssize_t
kbd_read (struct file *f, char *buffer, size_t count, loff_t *ppos)
{
	DECLARE_WAITQUEUE(wait, current);
	unsigned long flags;
	char *end, *p;

	/* Return EWOULDBLOCK, because this is what the X server expects */
	if (kbd_head == kbd_tail){
		if (f->f_flags & O_NONBLOCK)
			return -EWOULDBLOCK;
		add_wait_queue (&kbd_wait, &wait);
repeat:
		set_current_state(TASK_INTERRUPTIBLE);
		if (kbd_head == kbd_tail && !signal_pending(current)) {
			schedule();
			goto repeat;
		}
		current->state = TASK_RUNNING;
		remove_wait_queue (&kbd_wait, &wait);
	}
	/* There is data in the keyboard, fill the user buffer */
	end = buffer+count;
	p = buffer;
	spin_lock_irqsave(&kbd_queue_lock, flags);
	for (; p < end && kbd_head != kbd_tail;){
		Firm_event this_event = kbd_queue[kbd_tail];

		kbd_tail = (kbd_tail + 1) % KBD_QSIZE;

		spin_unlock_irqrestore(&kbd_queue_lock, flags);

#ifdef CONFIG_SPARC32_COMPAT
		if (current->thread.flags & SPARC_FLAG_32BIT) {
			if (copy_to_user((Firm_event *)p, &this_event,
					 sizeof(Firm_event)-sizeof(struct timeval)))
				return -EFAULT;
			p += sizeof(Firm_event)-sizeof(struct timeval);
			if (__put_user(this_event.time.tv_sec, (u32 *)p))
				return -EFAULT;
			p += sizeof(u32);
			if (__put_user(this_event.time.tv_usec, (u32 *)p))
				return -EFAULT;
			p += sizeof(u32);
		} else
#endif
		{
			if (copy_to_user((Firm_event *)p, &this_event, 
					 sizeof(Firm_event)))
				return -EFAULT;
			p += sizeof (Firm_event);
		}
#ifdef KBD_DEBUG
		printk ("[%s]", this_event.value == VKEY_UP ? "UP" : "DOWN");
#endif

		spin_lock_irqsave(&kbd_queue_lock, flags);
	}

	spin_unlock_irqrestore(&kbd_queue_lock, flags);

	return p-buffer;
}

/* Needed by X */
static int kbd_fasync (int fd, struct file *filp, int on)
{
	int retval;

	retval = fasync_helper (fd, filp, on, &kb_fasync);
	if (retval < 0)
		return retval;
	return 0;
}

static unsigned int kbd_poll (struct file *f, poll_table *wait)
{
	poll_wait(f, &kbd_wait, wait);
	if (kbd_head != kbd_tail)
		return POLLIN | POLLRDNORM;
	return 0;
}

static int
kbd_ioctl (struct inode *i, struct file *f, unsigned int cmd, unsigned long arg)
{
	unsigned char c;
	unsigned char leds = 0;
	int value;

	switch (cmd){
	case KIOCTYPE:		  /* return keyboard type */
		if (put_user(sunkbd_type, (int *) arg))
			return -EFAULT;
		break;
	case KIOCGTRANS:
		if (put_user(TR_UNTRANS_EVENT, (int *) arg))
			return -EFAULT;
		break;
	case KIOCTRANS:
		if (get_user(value, (int *) arg))
			return -EFAULT;
		if (value != TR_UNTRANS_EVENT)
			return -EINVAL;
		break;
	case KIOCLAYOUT:
		if (put_user(sunkbd_layout, (int *) arg))
			return -EFAULT;
		break;
	case KIOCSDIRECT:
#ifndef CODING_NEW_DRIVER
		if (get_user(value, (int *) arg))
			return -EFAULT;
		if(value)
			kbd_redirected = fg_console + 1;
		else
			kbd_redirected = 0;
		kbd_table [fg_console].kbdmode = kbd_redirected ? VC_RAW : VC_XLATE;
#endif
		break;
	case KIOCCMD:
		if (get_user(value, (int *) arg))
			return -EFAULT;
		c = (unsigned char) value;
		switch (c) {
			case SKBDCMD_CLICK:
			case SKBDCMD_NOCLICK:
				spin_lock_irq(&sunkbd_lock);
				send_cmd(c);
				spin_unlock_irq(&sunkbd_lock);
				return 0;
			case SKBDCMD_BELLON:
				kd_mksound(1,0);
				return 0;
			case SKBDCMD_BELLOFF:
				kd_mksound(0,0);
				return 0;
			default:
				return -EINVAL;
		}
	case KIOCSLED:
		if (get_user(c, (unsigned char *) arg))
			return -EFAULT;

		if (c & LED_SCRLCK) leds |= (1 << VC_SCROLLOCK);
		if (c & LED_NLOCK) leds |= (1 << VC_NUMLOCK);
		if (c & LED_CLOCK) leds |= (1 << VC_CAPSLOCK);
		compose_led_on = !!(c & LED_CMPOSE);
		sun_setledstate(kbd_table + fg_console, leds);
		break;
	case KIOCGLED:
		if (put_user(vcleds_to_sunkbd(getleds()), (unsigned char *) arg))
			return -EFAULT;
		break;
	case KIOCGRATE:
	{
		struct kbd_rate rate;

		rate.delay = kbd_delay_ticks;
		if (kbd_rate_ticks)
			rate.rate = HZ / kbd_rate_ticks;
		else
			rate.rate = 0;

		if (copy_to_user((struct kbd_rate *)arg, &rate,
				 sizeof(struct kbd_rate)))
			return -EFAULT;

		return 0;
	}
	case KIOCSRATE:
	{
		struct kbd_rate rate;

		if (verify_area(VERIFY_READ, (void *)arg,
				sizeof(struct kbd_rate)))
			return -EFAULT;
		copy_from_user(&rate, (struct kbd_rate *)arg,
			       sizeof(struct kbd_rate));

		if (rate.rate > 50)
			return -EINVAL;
		if (rate.rate == 0)
			kbd_rate_ticks = 0;
		else
			kbd_rate_ticks = HZ / rate.rate;
		kbd_delay_ticks = rate.delay;

		return 0;
	}
	case FIONREAD:		/* return number of bytes in kbd queue */
	{
		int count;
		
		count = kbd_head - kbd_tail;
		if (put_user((count < 0) ? KBD_QSIZE - count : count, (int *) arg))
			return -EFAULT;
		return 0;
	}
	default:
		printk ("Unknown Keyboard ioctl: %8.8x\n", cmd);
		return -EINVAL;
	}
	return 0;
}

static int
kbd_open (struct inode *i, struct file *f)
{
	spin_lock_irq(&kbd_queue_lock);
	kbd_active++;

	if (kbd_opened)
		goto out;

	kbd_opened = fg_console + 1;

	kbd_head = kbd_tail = 0;

 out:
	spin_unlock_irq(&kbd_queue_lock);

	return 0;
}

static int
kbd_close (struct inode *i, struct file *f)
{
	spin_lock_irq(&kbd_queue_lock);
	if (!--kbd_active) {
		if (kbd_redirected)
			kbd_table [kbd_redirected-1].kbdmode = VC_XLATE;
		kbd_redirected = 0;
		kbd_opened = 0;
		kbd_fasync (-1, f, 0);
	}
	spin_unlock_irq(&kbd_queue_lock);

	return 0;
}

static struct file_operations kbd_fops =
{
	read:		kbd_read,
	poll:		kbd_poll,
	ioctl:		kbd_ioctl,
	open:		kbd_open,
	release:	kbd_close,
	fasync:		kbd_fasync,
};

void __init keyboard_zsinit(void (*put_char)(unsigned char))
{
	int timeout = 0;

	kbd_put_char = put_char;
	if (!kbd_put_char)
		panic("keyboard_zsinit: no put_char parameter");

	/* Test out the leds */
	sunkbd_type = 255;
	sunkbd_layout = 0;

	send_cmd(SKBDCMD_RESET);
	send_cmd(SKBDCMD_RESET);
	while((sunkbd_type==255) && timeout++ < 25000) {
		udelay(100);
		barrier();
	}

	if(timeout>=25000) {
		printk("keyboard: not present\n");
		return;
	}

	if(sunkbd_type != SUNKBD_TYPE4) {
		printk("Sun TYPE %d keyboard detected ", sunkbd_type);
	} else {
		timeout=0;
		while((sunkbd_layout==0) && timeout++ < 10000) {
			udelay(100);
			barrier();
		}
		printk("Sun TYPE %d keyboard detected ",
		       ((sunkbd_layout & SUNKBD_LOUT_TYP5_MASK) ? 5 : 4));
	}
	if(sunkbd_type == SUNKBD_TYPE2)
		sunkbd_clickp = 0;

	spin_lock_irq(&sunkbd_lock);

	if(sunkbd_clickp) {
		send_cmd(SKBDCMD_CLICK);
		printk("with keyclick\n");
	} else {
		send_cmd(SKBDCMD_NOCLICK);
		printk("without keyclick\n");
	}

	/* Dork with led lights, then turn them all off */
	send_cmd(SKBDCMD_SETLED); send_cmd(0xf); /* All on */
	send_cmd(SKBDCMD_SETLED); send_cmd(0x0); /* All off */

	spin_unlock_irq(&sunkbd_lock);

	/* Register the /dev/kbd interface */
	devfs_register (NULL, "kbd", DEVFS_FL_DEFAULT,
			KBD_MAJOR, 0,
			S_IFCHR | S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH,
			&kbd_fops, NULL);
	if (devfs_register_chrdev (KBD_MAJOR, "kbd", &kbd_fops)){
		printk ("Could not register /dev/kbd device\n");
		return;
	}
	return;
}
