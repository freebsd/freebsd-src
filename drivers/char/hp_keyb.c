/*
 * linux/drivers/char/hp_keyb.c
 * helper-functions for the keyboard/psaux driver for HP-PARISC workstations
 *
 * based on pc_keyb.c by Geert Uytterhoeven & Martin Mares
 *
 * 2000/10/26	Debacker Xavier <debackex@esiee.fr>
 *		Marteau Thomas <marteaut@esiee.fr>
 *		Djoudi Malek <djoudim@esiee.fr>
 * - fixed some keysym defines 
 *
 * 2001/04/28	Debacker Xavier <debackex@esiee.fr>
 * - scancode translation rewritten in handle_at_scancode()
 */  

#include <linux/config.h>
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
#include <linux/init.h>

#include <asm/bitops.h>
#include <asm/irq.h>
#include <asm/hardware.h>
#include <asm/io.h>
#include <asm/system.h>

#define KBD_REPORT_ERR
#define KBD_REPORT_UNKN

#define KBD_ESCAPEE0	0xe0		/* in */
#define KBD_ESCAPEE1	0xe1		/* in */

#define ESCE0(x)	(0xe000|(x))
#define ESCE1(x)	(0xe100|(x))

#define KBD_BAT		0xaa		/* in */
#define KBD_SETLEDS	0xed		/* out */
#define KBD_ECHO	0xee		/* in/out */
#define KBD_BREAK	0xf0		/* in */
#define KBD_TYPRATEDLY	0xf3		/* out */
#define KBD_SCANENABLE	0xf4		/* out */
#define KBD_DEFDISABLE	0xf5		/* out */
#define KBD_DEFAULT	0xf6		/* out */
#define KBD_ACK		0xfa		/* in */
#define KBD_DIAGFAIL	0xfd		/* in */
#define KBD_RESEND	0xfe		/* in/out */
#define KBD_RESET	0xff		/* out */

#define CODE_BREAK	1
#define CODE_ESCAPEE0	2
#define CODE_ESCAPEE1	4
#define CODE_ESCAPE12	8

#define K_NONE		0x7f
#define K_ESC		0x01
#define K_F1		0x3b
#define K_F2		0x3c
#define K_F3		0x3d
#define K_F4		0x3e
#define K_F5		0x3f
#define K_F6		0x40
#define K_F7		0x41
#define K_F8		0x42
#define K_F9		0x43
#define K_F10		0x44
#define K_F11		0x57
#define K_F12		0x58
#define K_PRNT		0x54
#define K_SCRL		0x46
#define K_BRK		0x77
#define K_AGR		0x29
#define K_1		0x02
#define K_2		0x03
#define K_3		0x04
#define K_4		0x05
#define K_5		0x06
#define K_6		0x07
#define K_7		0x08
#define K_8		0x09
#define K_9		0x0a
#define K_0		0x0b
#define K_MINS		0x0c
#define K_EQLS		0x0d
#define K_BKSP		0x0e
#define K_INS		0x6e
#define K_HOME		0x66
#define K_PGUP		0x68
#define K_NUML		0x45
#define KP_SLH		0x62
#define KP_STR		0x37
#define KP_MNS		0x4a
#define K_TAB		0x0f
#define K_Q		0x10
#define K_W		0x11
#define K_E		0x12
#define K_R		0x13
#define K_T		0x14
#define K_Y		0x15
#define K_U		0x16
#define K_I		0x17
#define K_O		0x18
#define K_P		0x19
#define K_LSBK		0x1a
#define K_RSBK		0x1b
#define K_ENTR		0x1c
#define K_DEL		111
#define K_END		0x6b
#define K_PGDN		0x6d
#define KP_7		0x47
#define KP_8		0x48
#define KP_9		0x49
#define KP_PLS		0x4e
#define K_CAPS		0x3a
#define K_A		0x1e
#define K_S		0x1f
#define K_D		0x20
#define K_F		0x21
#define K_G		0x22
#define K_H		0x23
#define K_J		0x24
#define K_K		0x25
#define K_L		0x26
#define K_SEMI		0x27
#define K_SQOT		0x28
#define K_HASH		K_NONE
#define KP_4		0x4b
#define KP_5		0x4c
#define KP_6		0x4d
#define K_LSFT		0x2a
#define K_BSLH		0x2b
#define K_Z		0x2c
#define K_X		0x2d
#define K_C		0x2e
#define K_V		0x2f
#define K_B		0x30
#define K_N		0x31
#define K_M		0x32
#define K_COMA		0x33
#define K_DOT		0x34
#define K_FSLH		0x35
#define K_RSFT		0x36
#define K_UP		0x67
#define KP_1		0x4f
#define KP_2		0x50
#define KP_3		0x51
#define KP_ENT		0x60
#define K_LCTL		0x1d
#define K_LALT		0x38
#define K_SPCE		0x39
#define K_RALT		0x64
#define K_RCTL		0x61
#define K_LEFT		0x69
#define K_DOWN		0x6c
#define K_RGHT		0x6a
#define KP_0		0x52
#define KP_DOT		0x53

static unsigned char keycode_translate[256] =
{
/* 00 */  K_NONE, K_F9  , K_NONE, K_F5  , K_F3  , K_F1  , K_F2  , K_F12 ,
/* 08 */  K_NONE, K_F10 , K_F8  , K_F6  , K_F4  , K_TAB , K_AGR , K_NONE,
/* 10 */  K_NONE, K_LALT, K_LSFT, K_NONE, K_LCTL, K_Q   , K_1   , K_NONE,
/* 18 */  K_NONE, K_NONE, K_Z   , K_S   , K_A   , K_W   , K_2   , K_NONE,
/* 20 */  K_NONE, K_C   , K_X   , K_D   , K_E   , K_4   , K_3   , K_NONE,
/* 28 */  K_NONE, K_SPCE, K_V   , K_F   , K_T   , K_R   , K_5   , K_NONE,
/* 30 */  K_NONE, K_N   , K_B   , K_H   , K_G   , K_Y   , K_6   , K_NONE,
/* 38 */  K_NONE, K_NONE, K_M   , K_J   , K_U   , K_7   , K_8   , K_NONE,
/* 40 */  K_NONE, K_COMA, K_K   , K_I   , K_O   , K_0   , K_9   , K_NONE,
/* 48 */  K_PGUP, K_DOT , K_FSLH, K_L   , K_SEMI, K_P   , K_MINS, K_NONE,
/* 50 */  K_NONE, K_NONE, K_SQOT, K_NONE, K_LSBK, K_EQLS, K_NONE, K_NONE,
/* 58 */  K_CAPS, K_RSFT, K_ENTR, K_RSBK, K_NONE, K_BSLH, K_NONE, K_NONE,
/* 60 */  K_NONE, K_HASH, K_NONE, K_NONE, K_NONE, K_NONE, K_BKSP, K_NONE,
/* 68 */  K_NONE, KP_1  , K_NONE, KP_4  , KP_7  , K_NONE, K_NONE, K_NONE,
/* 70 */  KP_0  , KP_DOT, KP_2  , KP_5  , KP_6  , KP_8  , K_ESC , K_NUML,
/* 78 */  K_F11 , KP_PLS, KP_3  , KP_MNS, KP_STR, KP_9  , K_SCRL, K_PRNT,
/* 80 */  K_NONE, K_NONE, K_NONE, K_F7  , K_NONE, K_NONE, K_NONE, K_NONE,
/* 88 */  K_NONE, K_NONE, K_NONE, K_NONE, K_NONE, K_NONE, K_NONE, K_NONE,
/* 90 */  K_NONE, K_NONE, K_NONE, K_NONE, K_NONE, K_NONE, K_NONE, K_NONE,
/* 98 */  K_NONE, K_NONE, K_NONE, K_NONE, K_NONE, K_NONE, K_NONE, K_NONE,
/* a0 */  K_NONE, K_NONE, K_NONE, K_NONE, K_NONE, K_NONE, K_NONE, K_NONE,
/* a8 */  K_NONE, K_NONE, K_NONE, K_NONE, K_NONE, K_NONE, K_NONE, K_NONE,
/* b0 */  K_NONE, K_NONE, K_NONE, K_NONE, K_NONE, K_NONE, K_NONE, K_NONE,
/* b8 */  K_NONE, K_NONE, K_NONE, K_NONE, K_NONE, K_NONE, K_NONE, K_NONE,
/* c0 */  K_NONE, K_NONE, K_NONE, K_NONE, K_NONE, K_NONE, K_NONE, K_NONE,
/* c8 */  K_NONE, K_NONE, K_NONE, K_NONE, K_NONE, K_NONE, K_NONE, K_NONE,
/* d0 */  K_NONE, K_NONE, K_NONE, K_NONE, K_NONE, K_NONE, K_NONE, K_NONE,
/* d8 */  K_NONE, K_NONE, K_NONE, K_NONE, K_NONE, K_NONE, K_NONE, K_NONE,
/* e0 */  K_NONE, K_NONE, K_NONE, K_NONE, K_NONE, K_NONE, K_NONE, K_NONE,
/* e8 */  K_NONE, K_NONE, K_NONE, K_NONE, K_NONE, K_NONE, K_NONE, K_NONE,
/* f0 */  K_NONE, K_NONE, K_NONE, K_NONE, K_NONE, K_NONE, K_NONE, K_NONE,
/* f8 */  K_NONE, K_NONE, K_NONE, K_NONE, K_NONE, K_NONE, KBD_RESEND, K_NONE
};

/* ----- the following code stolen from pc_keyb.c */


#ifdef CONFIG_MAGIC_SYSRQ
unsigned char hp_ps2kbd_sysrq_xlate[128] =
	"\000\0331234567890-=\177\t"			/* 0x00 - 0x0f */
	"qwertyuiop[]\r\000as"				/* 0x10 - 0x1f */
	"dfghjkl;'`\000\\zxcv"				/* 0x20 - 0x2f */
	"bnm,./\000*\000 \000\201\202\203\204\205"	/* 0x30 - 0x3f */
	"\206\207\210\211\212\000\000789-456+1"		/* 0x40 - 0x4f */
	"230\177\000\000\213\214\000\000\000\000\000\000\000\000\000\000" /* 0x50 - 0x5f */
	"\r\000/";					/* 0x60 - 0x6f */
#endif

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
#define E0_BREAK   101	/* (control-pause) */
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
#define SC_LIM 89		/* 0x59 == 89 */

#define FOCUS_PF1 85		/* actual code! */
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

/* On one Compaq UK keyboard, at least, bar/backslash generates scancode
 * 0x7f.  0x7f generated on some .de and .no keyboards also.
 */
#define UK_86	   86

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
  FOCUS_PF8, JAP_86, FOCUS_PF10, UK_86               /* 0x7c-0x7f */
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
  0, 0, 0, 0, 0, 0, 0, E0_MSLW			      /* 0x78-0x7f */
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

/* ----- end of stolen part ------ */


void kbd_reset_setup(void) 
{ 
}

void handle_at_scancode(int keyval)
{
	static int brk;
	static int esc0;
	static int esc1;
	int scancode = 0;
	
	switch (keyval) {
		case KBD_BREAK :  
			/* sets the "release_key" bit when a key is 
			   released. HP keyboard send f0 followed by 
			   the keycode while AT keyboard send the keycode
			   with this bit set. */
			brk = 0x80;
			return;
		case KBD_ESCAPEE0 :
			/* 2chars sequence, commonly used to differenciate 
			   the two ALT keys and the two ENTER keys and so 
			   on... */
			esc0 = 2;	/* e0-xx are 2 chars */
			scancode = keyval;
			break;
		case KBD_ESCAPEE1 :  
			/* 3chars sequence, only used by the Pause key. */
			esc1 = 3;	/* e1-xx-xx are 3 chars */
			scancode = keyval;
			break;
#if 0
		case KBD_RESEND :
			/* dunno what to do when it happens. RFC */
			printk(KERN_INFO "keyboard: KBD_RESEND received.\n");
			return;
#endif
		case 0x14 : 
			/* translate e1-14-77-e1-f0-14-f0-77 to 
			   e1-1d-45-e1-9d-c5 (the Pause key) */
			if (esc1==2) scancode = brk | 0x1d;
			break;
		case 0x77 :
			if (esc1==1) scancode = brk | 0x45;
			break;
		case 0x12 :
			/* an extended key is e0-12-e0-xx e0-f0-xx-e0-f0-12
			   on HP, while it is e0-2a-e0-xx e0-(xx|80)-f0-aa 
			   on AT. */
			if (esc0==1) scancode = brk | 0x2a;
			break;
	}
	

	/* translates HP scancodes to AT scancodes */
	if (!scancode) scancode = brk | keycode_translate[keyval];


	if (!scancode) printk(KERN_INFO "keyboard: unexpected key code %02x\n",keyval);

	/* now behave like an AT keyboard */
	handle_scancode(scancode,!(scancode&0x80));

	if (esc0) esc0--;
	if (esc1) esc1--;

	/* release key bit must be unset for the next key */
	brk = 0;
}

