/*
 * linux/drivers/char/keyboard.c
 *
 * Written for linux by Johan Myreen as a translation from
 * the assembly version by Linus (with diacriticals added)
 *
 * Some additional features added by Christoph Niemann (ChN), March 1993
 *
 * Loadable keymaps by Risto Kankkunen, May 1993
 *
 * Diacriticals redone & other small changes, aeb@cwi.nl, June 1993
 * Added decr/incr_console, dynamic keymaps, Unicode support,
 * dynamic function/string keys, led setting,  Sept 1994
 * `Sticky' modifier keys, 951006.
 *
 * 11-11-96: SAK should now work in the raw mode (Martin Mares)
 * 
 * Modified to provide 'generic' keyboard support by Hamish Macdonald
 * Merge with the m68k keyboard driver and split-off of the PC low-level
 * parts by Geert Uytterhoeven, May 1997
 *
 * 27-05-97: Added support for the Magic SysRq Key (Martin Mares)
 * 30-07-98: Dead keys redone, aeb@cwi.nl.
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/mm.h>
#include <linux/string.h>
#include <linux/random.h>
#include <linux/init.h>

#include <asm/keyboard.h>
#include <asm/bitops.h>

#include <linux/kbd_kern.h>
#include <linux/kbd_diacr.h>
#include <linux/vt_kern.h>
#include <linux/kbd_ll.h>
#include <linux/sysrq.h>
#include <linux/pm.h>

#define SIZE(x) (sizeof(x)/sizeof((x)[0]))

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

void (*kbd_ledfunc)(unsigned int led);
EXPORT_SYMBOL(handle_scancode);
EXPORT_SYMBOL(kbd_ledfunc);
EXPORT_SYMBOL(kbd_refresh_leds);

extern void ctrl_alt_del(void);

struct console;

/*
 * global state includes the following, and various static variables
 * in this module: prev_scancode, shift_state, diacr, npadch, dead_key_next.
 * (last_console is now a global variable)
 */

/* shift state counters.. */
static unsigned char k_down[NR_SHIFT];
/* keyboard key bitmap */
static unsigned long key_down[256/BITS_PER_LONG];

static int dead_key_next;
/* 
 * In order to retrieve the shift_state (for the mouse server), either
 * the variable must be global, or a new procedure must be created to 
 * return the value. I chose the former way.
 */
int shift_state;
static int npadch = -1;			/* -1 or number assembled on pad */
static unsigned char diacr;
static char rep;			/* flag telling character repeat */
struct kbd_struct kbd_table[MAX_NR_CONSOLES];
static struct tty_struct **ttytab;
static struct kbd_struct * kbd = kbd_table;
static struct tty_struct * tty;
static unsigned char prev_scancode;

void compute_shiftstate(void);

typedef void (*k_hand)(unsigned char value, char up_flag);
typedef void (k_handfn)(unsigned char value, char up_flag);

static k_handfn
	do_self, do_fn, do_spec, do_pad, do_dead, do_cons, do_cur, do_shift,
	do_meta, do_ascii, do_lock, do_lowercase, do_slock, do_dead2,
	do_ignore;

static k_hand key_handler[16] = {
	do_self, do_fn, do_spec, do_pad, do_dead, do_cons, do_cur, do_shift,
	do_meta, do_ascii, do_lock, do_lowercase, do_slock, do_dead2,
	do_ignore, do_ignore
};

/* Key types processed even in raw modes */

#define TYPES_ALLOWED_IN_RAW_MODE ((1 << KT_SPEC) | (1 << KT_SHIFT))

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

#define SPECIALS_ALLOWED_IN_RAW_MODE (1 << KVAL(K_SAK))

/* maximum values each key_handler can handle */
const int max_vals[] = {
	255, SIZE(func_table) - 1, SIZE(spec_fn_table) - 1, NR_PAD - 1,
	NR_DEAD - 1, 255, 3, NR_SHIFT - 1,
	255, NR_ASCII - 1, NR_LOCK - 1, 255,
	NR_LOCK - 1, 255
};

const int NR_TYPES = SIZE(max_vals);

/* N.B. drivers/macintosh/mac_keyb.c needs to call put_queue */
void put_queue(int);
static unsigned char handle_diacr(unsigned char);

/* kbd_pt_regs - set by keyboard_interrupt(), used by show_ptregs() */
struct pt_regs * kbd_pt_regs;

#ifdef CONFIG_MAGIC_SYSRQ
static int sysrq_pressed;
#endif

static struct pm_dev *pm_kbd;

/*
 * Many other routines do put_queue, but I think either
 * they produce ASCII, or they produce some user-assigned
 * string, and in both cases we might assume that it is
 * in utf-8 already.
 */
void to_utf8(ushort c) {
    if (c < 0x80)
	put_queue(c);			/*  0*******  */
    else if (c < 0x800) {
	put_queue(0xc0 | (c >> 6)); 	/*  110***** 10******  */
	put_queue(0x80 | (c & 0x3f));
    } else {
	put_queue(0xe0 | (c >> 12)); 	/*  1110**** 10****** 10******  */
	put_queue(0x80 | ((c >> 6) & 0x3f));
	put_queue(0x80 | (c & 0x3f));
    }
    /* UTF-8 is defined for words of up to 31 bits,
       but we need only 16 bits here */
}

/*
 * Translation of escaped scancodes to keycodes.
 * This is now user-settable (for machines were it makes sense).
 */

int setkeycode(unsigned int scancode, unsigned int keycode)
{
    return kbd_setkeycode(scancode, keycode);
}

int getkeycode(unsigned int scancode)
{
    return kbd_getkeycode(scancode);
}

void handle_scancode(unsigned char scancode, int down)
{
	unsigned char keycode;
	char up_flag = down ? 0 : 0200;
	char raw_mode;
	char have_keycode;

	pm_access(pm_kbd);
	add_keyboard_randomness(scancode | up_flag);

	tty = ttytab? ttytab[fg_console]: NULL;
	if (tty && (!tty->driver_data)) {
		/*
		 * We touch the tty structure via the ttytab array
		 * without knowing whether or not tty is open, which
		 * is inherently dangerous.  We currently rely on that
		 * fact that console_open sets tty->driver_data when
		 * it opens it, and clears it when it closes it.
		 */
		tty = NULL;
	}
	kbd = kbd_table + fg_console;
	/*
	 *  Convert scancode to keycode
	 */
	raw_mode = (kbd->kbdmode == VC_RAW);
	have_keycode = kbd_translate(scancode, &keycode, raw_mode);
	if (raw_mode) {
		/*
		 *	The following is a workaround for hardware
		 *	which sometimes send the key release event twice 
		 */
		unsigned char next_scancode = scancode|up_flag;
		if (have_keycode && up_flag && next_scancode==prev_scancode) {
			/* unexpected 2nd release event */
		} else {
			/* 
			 * Only save previous scancode if it was a key-up
			 * and had a single-byte scancode.  
			 */
			if (!have_keycode)
				prev_scancode = 1;
			else if (!up_flag || prev_scancode == 1)
				prev_scancode = 0;
			else
				prev_scancode = next_scancode;
			put_queue(next_scancode);
		}
		/* we do not return yet, because we want to maintain
		   the key_down array, so that we have the correct
		   values when finishing RAW mode or when changing VT's */
	}

	if (!have_keycode)
		goto out;

	/*
	 * At this point the variable `keycode' contains the keycode.
	 * Note: the keycode must not be 0 (++Geert: on m68k 0 is valid).
	 * We keep track of the up/down status of the key, and
	 * return the keycode if in MEDIUMRAW mode.
	 */

	if (up_flag) {
		rep = 0;
		if(!test_and_clear_bit(keycode, key_down))
		    up_flag = kbd_unexpected_up(keycode);
	} else
		rep = test_and_set_bit(keycode, key_down);

#ifdef CONFIG_MAGIC_SYSRQ		/* Handle the SysRq Hack */
	if (keycode == SYSRQ_KEY) {
		sysrq_pressed = !up_flag;
		goto out;
	} else if (sysrq_pressed) {
		if (!up_flag) {
			handle_sysrq(kbd_sysrq_xlate[keycode], kbd_pt_regs, kbd, tty);
			goto out;
		}
	}
#endif

	if (kbd->kbdmode == VC_MEDIUMRAW) {
		/* soon keycodes will require more than one byte */
		put_queue(keycode + up_flag);
		raw_mode = 1;	/* Most key classes will be ignored */
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
		int shift_final = (shift_state | kbd->slockstate) ^
		    kbd->lockstate;
		ushort *key_map = key_maps[shift_final];

		if (key_map != NULL) {
			keysym = key_map[keycode];
			type = KTYP(keysym);

			if (type >= 0xf0) {
			    type -= 0xf0;
			    if (raw_mode && ! (TYPES_ALLOWED_IN_RAW_MODE & (1 << type)))
				goto out;
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
			} else {
			    /* maybe only if (kbd->kbdmode == VC_UNICODE) ? */
			    if (!up_flag && !raw_mode)
			      to_utf8(keysym);
			}
		} else {
			/* maybe beep? */
			/* we have at least to update shift_state */
#if 1			/* how? two almost equivalent choices follow */
			compute_shiftstate();
			kbd->slockstate = 0; /* play it safe */
#else
			keysym = U(key_maps[0][keycode]);
			type = KTYP(keysym);
			if (type == KT_SHIFT)
			  (*key_handler[type])(keysym & 0xff, up_flag);
#endif
		}
	}
out:
	do_poke_blanked_console = 1;
	schedule_console_callback();
}

void put_queue(int ch)
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
	if (diacr) {
		put_queue(diacr);
		diacr = 0;
	}
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
	if (kbd_pt_regs)
		show_regs(kbd_pt_regs);
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
	ctrl_alt_del();
}

static void compose(void)
{
	dead_key_next = 1;
}

int spawnpid, spawnsig;

static void spawn_console(void)
{
        if (spawnpid)
	   if(kill_proc(spawnpid, spawnsig, 1))
	     spawnpid = 0;
}

static void SAK(void)
{
	/*
	 * SAK should also work in all raw modes and reset
	 * them properly.
	 */

	do_SAK(tty);
	reset_vc(fg_console);
#if 0
	do_unblank_screen();	/* not in interrupt routine? */
#endif
}

static void do_ignore(unsigned char value, char up_flag)
{
}

static void do_null()
{
	compute_shiftstate();
}

static void do_spec(unsigned char value, char up_flag)
{
	if (up_flag)
		return;
	if (value >= SIZE(spec_fn_table))
		return;
	if ((kbd->kbdmode == VC_RAW || kbd->kbdmode == VC_MEDIUMRAW) &&
	    !(SPECIALS_ALLOWED_IN_RAW_MODE & (1 << value)))
		return;
	spec_fn_table[value]();
}

static void do_lowercase(unsigned char value, char up_flag)
{
	printk(KERN_ERR "keyboard.c: do_lowercase was called - impossible\n");
}

static void do_self(unsigned char value, char up_flag)
{
	if (up_flag)
		return;		/* no action, if this is a key release */

	if (diacr)
		value = handle_diacr(value);

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

/* Obsolete - for backwards compatibility only */
static void do_dead(unsigned char value, char up_flag)
{
	value = ret_diacr[value];
	do_dead2(value,up_flag);
}

/*
 * Handle dead key. Note that we now may have several
 * dead keys modifying the same character. Very useful
 * for Vietnamese.
 */
static void do_dead2(unsigned char value, char up_flag)
{
	if (up_flag)
		return;

	diacr = (diacr ? handle_diacr(value) : value);
}


/*
 * We have a combining character DIACR here, followed by the character CH.
 * If the combination occurs in the table, return the corresponding value.
 * Otherwise, if CH is a space or equals DIACR, return DIACR.
 * Otherwise, conclude that DIACR was not combining after all,
 * queue it and return CH.
 */
unsigned char handle_diacr(unsigned char ch)
{
	int d = diacr;
	int i;

	diacr = 0;

	for (i = 0; i < accent_table_size; i++) {
		if (accent_table[i].diacr == d && accent_table[i].base == ch)
			return accent_table[i].result;
	}

	if (ch == ' ' || ch == d)
		return d;

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
		printk(KERN_ERR "do_fn called with value=%d\n", value);
}

static void do_pad(unsigned char value, char up_flag)
{
	static const char *pad_chars = "0123456789+-*/\015,.?()";
	static const char *app_map = "pqrstuvwxylSRQMnnmPQ";

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

	/* kludge */
	if (up_flag && shift_state != old_state && npadch != -1) {
		if (kbd->kbdmode == VC_UNICODE)
		  to_utf8(npadch & 0xffff);
		else
		  put_queue(npadch & 0xff);
		npadch = -1;
	}
}

/* called after returning from RAW mode or when changing consoles -
   recompute k_down[] and shift_state from key_down[] */
/* maybe called when keymap is undefined, so that shiftkey release is seen */
void compute_shiftstate(void)
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
		sym = U(key_maps[0][k]);
		if(KTYP(sym) == KT_SHIFT || KTYP(sym) == KT_SLOCK) {
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

static void do_slock(unsigned char value, char up_flag)
{
	do_shift(value,up_flag);
	if (up_flag || rep)
		return;
	chg_vc_kbd_slock(kbd, value);
	/* try to make Alt, oops, AltGr and such work */
	if (!key_maps[kbd->lockstate ^ kbd->slockstate]) {
		kbd->slockstate = 0;
		chg_vc_kbd_slock(kbd, value);
	}
}

/*
 * The leds display either (i) the status of NumLock, CapsLock, ScrollLock,
 * or (ii) whatever pattern of lights people want to show using KDSETLED,
 * or (iii) specified bits of specified words in kernel memory.
 */

static unsigned char ledstate = 0xff; /* undefined */
static unsigned char ledioctl;

unsigned char getledstate(void) {
    return ledstate;
}

void setledstate(struct kbd_struct *kbd, unsigned int led) {
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
 * this when we don't want any of the above to happen.
 * This allows for easy and efficient race-condition prevention
 * for kbd_ledfunc => input_event(dev, EV_LED, ...) => ...
 */
static void kbd_bh(unsigned long dummy)
{
	unsigned char leds = getleds();

	if (leds != ledstate) {
		ledstate = leds;
		kbd_leds(leds);
		if (kbd_ledfunc) kbd_ledfunc(leds);
	}
}

EXPORT_SYMBOL(keyboard_tasklet);
DECLARE_TASKLET_DISABLED(keyboard_tasklet, kbd_bh, 0);

/*
 * This allows a newly plugged keyboard to pick the LED state.
 * We do it in this seemindly backwards fashion to ensure proper locking.
 * Built-in keyboard does refresh on its own.
 */
void kbd_refresh_leds(void)
{
	tasklet_disable(&keyboard_tasklet);
	if (ledstate != 0xff && kbd_ledfunc != NULL) kbd_ledfunc(ledstate);
	tasklet_enable(&keyboard_tasklet);
}

typedef void (pm_kbd_func) (void);

pm_callback pm_kbd_request_override = NULL;

int __init kbd_init(void)
{
	int i;
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

	kbd_init_hw();

	tasklet_enable(&keyboard_tasklet);
	tasklet_schedule(&keyboard_tasklet);
	
	pm_kbd = pm_register(PM_SYS_DEV, PM_SYS_KBC, pm_kbd_request_override);

	return 0;
}
