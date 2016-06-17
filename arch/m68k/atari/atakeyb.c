/*
 * linux/atari/atakeyb.c
 *
 * Atari Keyboard driver for 680x0 Linux
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive
 * for more details.
 */

/*
 * Atari support by Robert de Vries
 * enhanced by Bjoern Brauel and Roman Hodek
 */

#include <linux/config.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/errno.h>
#include <linux/keyboard.h>
#include <linux/delay.h>
#include <linux/timer.h>
#include <linux/kd.h>
#include <linux/random.h>
#include <linux/init.h>
#include <linux/kbd_ll.h>
#include <linux/kbd_kern.h>

#include <asm/atariints.h>
#include <asm/atarihw.h>
#include <asm/atarikb.h>
#include <asm/atari_joystick.h>
#include <asm/irq.h>

static void atakeyb_rep( unsigned long ignore );
extern unsigned int keymap_count;

/* Hook for MIDI serial driver */
void (*atari_MIDI_interrupt_hook) (void);
/* Hook for mouse driver */
void (*atari_mouse_interrupt_hook) (char *);

/* variables for IKBD self test: */

/* state: 0: off; >0: in progress; >1: 0xf1 received */
static volatile int ikbd_self_test;
/* timestamp when last received a char */
static volatile unsigned long self_test_last_rcv;
/* bitmap of keys reported as broken */
static unsigned long broken_keys[128/(sizeof(unsigned long)*8)] = { 0, };

#define BREAK_MASK	(0x80)

/*
 * ++roman: The following changes were applied manually:
 *
 *  - The Alt (= Meta) key works in combination with Shift and
 *    Control, e.g. Alt+Shift+a sends Meta-A (0xc1), Alt+Control+A sends
 *    Meta-Ctrl-A (0x81) ...
 *
 *  - The parentheses on the keypad send '(' and ')' with all
 *    modifiers (as would do e.g. keypad '+'), but they cannot be used as
 *    application keys (i.e. sending Esc O c).
 *
 *  - HELP and UNDO are mapped to be F21 and F24, resp, that send the
 *    codes "\E[M" and "\E[P". (This is better than the old mapping to
 *    F11 and F12, because these codes are on Shift+F1/2 anyway.) This
 *    way, applications that allow their own keyboard mappings
 *    (e.g. tcsh, X Windows) can be configured to use them in the way
 *    the label suggests (providing help or undoing).
 *
 *  - Console switching is done with Alt+Fx (consoles 1..10) and
 *    Shift+Alt+Fx (consoles 11..20).
 *
 *  - The misc. special function implemented in the kernel are mapped
 *    to the following key combinations:
 *
 *      ClrHome          -> Home/Find
 *      Shift + ClrHome  -> End/Select
 *      Shift + Up       -> Page Up
 *      Shift + Down     -> Page Down
 *      Alt + Help       -> show system status
 *      Shift + Help     -> show memory info
 *      Ctrl + Help      -> show registers
 *      Ctrl + Alt + Del -> Reboot
 *      Alt + Undo       -> switch to last console
 *      Shift + Undo     -> send interrupt
 *      Alt + Insert     -> stop/start output (same as ^S/^Q)
 *      Alt + Up         -> Scroll back console (if implemented)
 *      Alt + Down       -> Scroll forward console (if implemented)
 *      Alt + CapsLock   -> NumLock
 *
 * ++Andreas:
 *
 *  - Help mapped to K_HELP
 *  - Undo mapped to K_UNDO (= K_F246)
 *  - Keypad Left/Right Parenthesis mapped to new K_PPAREN[LR]
 */

static u_short ataplain_map[NR_KEYS] __initdata = {
	0xf200, 0xf01b, 0xf031, 0xf032, 0xf033, 0xf034, 0xf035, 0xf036,
	0xf037, 0xf038, 0xf039, 0xf030, 0xf02d, 0xf03d, 0xf008, 0xf009,
	0xfb71, 0xfb77, 0xfb65, 0xfb72, 0xfb74, 0xfb79, 0xfb75, 0xfb69,
	0xfb6f, 0xfb70, 0xf05b, 0xf05d, 0xf201, 0xf702, 0xfb61, 0xfb73,
	0xfb64, 0xfb66, 0xfb67, 0xfb68, 0xfb6a, 0xfb6b, 0xfb6c, 0xf03b,
	0xf027, 0xf060, 0xf700, 0xf05c, 0xfb7a, 0xfb78, 0xfb63, 0xfb76,
	0xfb62, 0xfb6e, 0xfb6d, 0xf02c, 0xf02e, 0xf02f, 0xf700, 0xf200,
	0xf703, 0xf020, 0xf207, 0xf100, 0xf101, 0xf102, 0xf103, 0xf104,
	0xf105, 0xf106, 0xf107, 0xf108, 0xf109, 0xf200, 0xf200, 0xf114,
	0xf603, 0xf200, 0xf30b, 0xf601, 0xf200, 0xf602, 0xf30a, 0xf200,
	0xf600, 0xf200, 0xf115, 0xf07f, 0xf200, 0xf200, 0xf200, 0xf200,
	0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200,
	0xf200, 0xf1ff, 0xf11b, 0xf312, 0xf313, 0xf30d, 0xf30c, 0xf307,
	0xf308, 0xf309, 0xf304, 0xf305, 0xf306, 0xf301, 0xf302, 0xf303,
	0xf300, 0xf310, 0xf30e, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200,
	0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200
};

static u_short atashift_map[NR_KEYS] __initdata = {
	0xf200, 0xf01b, 0xf021, 0xf040, 0xf023, 0xf024, 0xf025, 0xf05e,
	0xf026, 0xf02a, 0xf028, 0xf029, 0xf05f, 0xf02b, 0xf008, 0xf009,
	0xfb51, 0xfb57, 0xfb45, 0xfb52, 0xfb54, 0xfb59, 0xfb55, 0xfb49,
	0xfb4f, 0xfb50, 0xf07b, 0xf07d, 0xf201, 0xf702, 0xfb41, 0xfb53,
	0xfb44, 0xfb46, 0xfb47, 0xfb48, 0xfb4a, 0xfb4b, 0xfb4c, 0xf03a,
	0xf022, 0xf07e, 0xf700, 0xf07c, 0xfb5a, 0xfb58, 0xfb43, 0xfb56,
	0xfb42, 0xfb4e, 0xfb4d, 0xf03c, 0xf03e, 0xf03f, 0xf700, 0xf200,
	0xf703, 0xf020, 0xf207, 0xf10a, 0xf10b, 0xf10c, 0xf10d, 0xf10e,
	0xf10f, 0xf110, 0xf111, 0xf112, 0xf113, 0xf200, 0xf200, 0xf117,
	0xf118, 0xf200, 0xf30b, 0xf601, 0xf200, 0xf602, 0xf30a, 0xf200,
	0xf119, 0xf200, 0xf115, 0xf07f, 0xf200, 0xf200, 0xf200, 0xf200,
	0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200,
	0xf200, 0xf205, 0xf203, 0xf312, 0xf313, 0xf30d, 0xf30c, 0xf307,
	0xf308, 0xf309, 0xf304, 0xf305, 0xf306, 0xf301, 0xf302, 0xf303,
	0xf300, 0xf310, 0xf30e, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200,
	0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200
};

static u_short atactrl_map[NR_KEYS] __initdata = {
	0xf200, 0xf200, 0xf200, 0xf000, 0xf01b, 0xf01c, 0xf01d, 0xf01e,
	0xf01f, 0xf07f, 0xf200, 0xf200, 0xf01f, 0xf200, 0xf008, 0xf200,
	0xf011, 0xf017, 0xf005, 0xf012, 0xf014, 0xf019, 0xf015, 0xf009,
	0xf00f, 0xf010, 0xf01b, 0xf01d, 0xf201, 0xf702, 0xf001, 0xf013,
	0xf004, 0xf006, 0xf007, 0xf008, 0xf00a, 0xf00b, 0xf00c, 0xf200,
	0xf007, 0xf000, 0xf700, 0xf01c, 0xf01a, 0xf018, 0xf003, 0xf016,
	0xf002, 0xf00e, 0xf00d, 0xf200, 0xf200, 0xf07f, 0xf700, 0xf200,
	0xf703, 0xf000, 0xf207, 0xf100, 0xf101, 0xf102, 0xf103, 0xf104,
	0xf105, 0xf106, 0xf107, 0xf108, 0xf109, 0xf200, 0xf200, 0xf114,
	0xf603, 0xf200, 0xf30b, 0xf601, 0xf200, 0xf602, 0xf30a, 0xf200,
	0xf600, 0xf200, 0xf115, 0xf07f, 0xf200, 0xf200, 0xf200, 0xf200,
	0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200,
	0xf200, 0xf1ff, 0xf202, 0xf312, 0xf313, 0xf30d, 0xf30c, 0xf307,
	0xf308, 0xf309, 0xf304, 0xf305, 0xf306, 0xf301, 0xf302, 0xf303,
	0xf300, 0xf310, 0xf30e, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200,
	0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200
};

static u_short atashift_ctrl_map[NR_KEYS] __initdata = {
	0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200,
	0xf200, 0xf200, 0xf200, 0xf200, 0xf01f, 0xf200, 0xf008, 0xf200,
	0xf011, 0xf017, 0xf005, 0xf012, 0xf014, 0xf019, 0xf015, 0xf009,
	0xf00f, 0xf010, 0xf200, 0xf200, 0xf201, 0xf702, 0xf001, 0xf013,
	0xf004, 0xf006, 0xf007, 0xf008, 0xf00a, 0xf00b, 0xf00c, 0xf200,
	0xf200, 0xf200, 0xf700, 0xf200, 0xf01a, 0xf018, 0xf003, 0xf016,
	0xf002, 0xf00e, 0xf00d, 0xf200, 0xf200, 0xf07f, 0xf700, 0xf200,
	0xf703, 0xf200, 0xf207, 0xf100, 0xf101, 0xf102, 0xf103, 0xf104,
	0xf105, 0xf106, 0xf107, 0xf108, 0xf109, 0xf200, 0xf200, 0xf117,
	0xf603, 0xf200, 0xf30b, 0xf601, 0xf200, 0xf602, 0xf30a, 0xf200,
	0xf600, 0xf200, 0xf115, 0xf07f, 0xf200, 0xf200, 0xf200, 0xf200,
	0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200,
	0xf200, 0xf1ff, 0xf11b, 0xf312, 0xf313, 0xf30d, 0xf30c, 0xf307,
	0xf308, 0xf309, 0xf304, 0xf305, 0xf306, 0xf301, 0xf302, 0xf303,
	0xf300, 0xf310, 0xf30e, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200,
	0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200
};

static u_short ataalt_map[NR_KEYS] __initdata = {
	0xf200, 0xf81b, 0xf831, 0xf832, 0xf833, 0xf834, 0xf835, 0xf836,
	0xf837, 0xf838, 0xf839, 0xf830, 0xf82d, 0xf83d, 0xf808, 0xf809,
	0xf871, 0xf877, 0xf865, 0xf872, 0xf874, 0xf879, 0xf875, 0xf869,
	0xf86f, 0xf870, 0xf85b, 0xf85d, 0xf80d, 0xf702, 0xf861, 0xf873,
	0xf864, 0xf866, 0xf867, 0xf868, 0xf86a, 0xf86b, 0xf86c, 0xf83b,
	0xf827, 0xf860, 0xf700, 0xf85c, 0xf87a, 0xf878, 0xf863, 0xf876,
	0xf862, 0xf86e, 0xf86d, 0xf82c, 0xf82e, 0xf82f, 0xf700, 0xf200,
	0xf703, 0xf820, 0xf208, 0xf500, 0xf501, 0xf502, 0xf503, 0xf504,
	0xf505, 0xf506, 0xf507, 0xf508, 0xf509, 0xf200, 0xf200, 0xf114,
	0xf20b, 0xf200, 0xf30b, 0xf601, 0xf200, 0xf602, 0xf30a, 0xf200,
	0xf20a, 0xf200, 0xf209, 0xf87f, 0xf200, 0xf200, 0xf200, 0xf200,
	0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200,
	0xf200, 0xf206, 0xf204, 0xf312, 0xf313, 0xf30d, 0xf30c, 0xf907,
	0xf908, 0xf909, 0xf904, 0xf905, 0xf906, 0xf901, 0xf902, 0xf903,
	0xf900, 0xf310, 0xf30e, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200,
	0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200
};

static u_short atashift_alt_map[NR_KEYS] __initdata = {
	0xf200, 0xf81b, 0xf821, 0xf840, 0xf823, 0xf824, 0xf825, 0xf85e,
	0xf826, 0xf82a, 0xf828, 0xf829, 0xf85f, 0xf82b, 0xf808, 0xf809,
	0xf851, 0xf857, 0xf845, 0xf852, 0xf854, 0xf859, 0xf855, 0xf849,
	0xf84f, 0xf850, 0xf87b, 0xf87d, 0xf201, 0xf702, 0xf841, 0xf853,
	0xf844, 0xf846, 0xf847, 0xf848, 0xf84a, 0xf84b, 0xf84c, 0xf83a,
	0xf822, 0xf87e, 0xf700, 0xf87c, 0xf85a, 0xf858, 0xf843, 0xf856,
	0xf842, 0xf84e, 0xf84d, 0xf83c, 0xf83e, 0xf83f, 0xf700, 0xf200,
	0xf703, 0xf820, 0xf207, 0xf50a, 0xf50b, 0xf50c, 0xf50d, 0xf50e,
	0xf50f, 0xf510, 0xf511, 0xf512, 0xf513, 0xf200, 0xf200, 0xf117,
	0xf118, 0xf200, 0xf30b, 0xf601, 0xf200, 0xf602, 0xf30a, 0xf200,
	0xf119, 0xf200, 0xf115, 0xf87f, 0xf200, 0xf200, 0xf200, 0xf200,
	0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200,
	0xf200, 0xf1ff, 0xf11b, 0xf312, 0xf313, 0xf30d, 0xf30c, 0xf307,
	0xf308, 0xf309, 0xf304, 0xf305, 0xf306, 0xf301, 0xf302, 0xf303,
	0xf300, 0xf310, 0xf30e, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200,
	0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200
};

static u_short atactrl_alt_map[NR_KEYS] __initdata = {
	0xf200, 0xf200, 0xf200, 0xf800, 0xf81b, 0xf81c, 0xf81d, 0xf81e,
	0xf81f, 0xf87f, 0xf200, 0xf200, 0xf81f, 0xf200, 0xf808, 0xf200,
	0xf811, 0xf817, 0xf805, 0xf812, 0xf814, 0xf819, 0xf815, 0xf809,
	0xf80f, 0xf810, 0xf81b, 0xf81d, 0xf201, 0xf702, 0xf801, 0xf813,
	0xf804, 0xf806, 0xf807, 0xf808, 0xf80a, 0xf80b, 0xf80c, 0xf200,
	0xf807, 0xf800, 0xf700, 0xf81c, 0xf81a, 0xf818, 0xf803, 0xf816,
	0xf802, 0xf80e, 0xf80d, 0xf200, 0xf200, 0xf87f, 0xf700, 0xf200,
	0xf703, 0xf800, 0xf207, 0xf100, 0xf101, 0xf102, 0xf103, 0xf104,
	0xf105, 0xf106, 0xf107, 0xf108, 0xf109, 0xf200, 0xf200, 0xf114,
	0xf603, 0xf200, 0xf30b, 0xf601, 0xf200, 0xf602, 0xf30a, 0xf200,
	0xf600, 0xf200, 0xf115, 0xf87f, 0xf200, 0xf200, 0xf200, 0xf200,
	0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200,
	0xf200, 0xf1ff, 0xf202, 0xf312, 0xf313, 0xf30d, 0xf30c, 0xf307,
	0xf308, 0xf309, 0xf304, 0xf305, 0xf306, 0xf301, 0xf302, 0xf303,
	0xf300, 0xf310, 0xf30e, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200,
	0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200
};

static u_short atashift_ctrl_alt_map[NR_KEYS] = {
	0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200,
	0xf200, 0xf200, 0xf200, 0xf200, 0xf81f, 0xf200, 0xf808, 0xf200,
	0xf811, 0xf817, 0xf805, 0xf812, 0xf814, 0xf819, 0xf815, 0xf809,
	0xf80f, 0xf810, 0xf200, 0xf200, 0xf201, 0xf702, 0xf801, 0xf813,
	0xf804, 0xf806, 0xf807, 0xf808, 0xf80a, 0xf80b, 0xf80c, 0xf200,
	0xf200, 0xf200, 0xf700, 0xf200, 0xf81a, 0xf818, 0xf803, 0xf816,
	0xf802, 0xf80e, 0xf80d, 0xf200, 0xf200, 0xf87f, 0xf700, 0xf200,
	0xf703, 0xf200, 0xf207, 0xf100, 0xf101, 0xf102, 0xf103, 0xf104,
	0xf105, 0xf106, 0xf107, 0xf108, 0xf109, 0xf200, 0xf200, 0xf117,
	0xf603, 0xf200, 0xf30b, 0xf601, 0xf200, 0xf602, 0xf30a, 0xf200,
	0xf600, 0xf200, 0xf115, 0xf87f, 0xf200, 0xf200, 0xf200, 0xf200,
	0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200,
	0xf200, 0xf1ff, 0xf11b, 0xf312, 0xf313, 0xf30d, 0xf30c, 0xf307,
	0xf308, 0xf309, 0xf304, 0xf305, 0xf306, 0xf301, 0xf302, 0xf303,
	0xf300, 0xf310, 0xf30e, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200,
	0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200, 0xf200
};

typedef enum kb_state_t
{
    KEYBOARD, AMOUSE, RMOUSE, JOYSTICK, CLOCK, RESYNC
} KB_STATE_T;

#define	IS_SYNC_CODE(sc)	((sc) >= 0x04 && (sc) <= 0xfb)

typedef struct keyboard_state
{
    unsigned char  buf[6];
    int 	   len;
    KB_STATE_T	   state;
} KEYBOARD_STATE;

KEYBOARD_STATE kb_state;

#define	DEFAULT_KEYB_REP_DELAY	(HZ/4)
#define	DEFAULT_KEYB_REP_RATE	(HZ/25)

/* These could be settable by some ioctl() in future... */
static unsigned int key_repeat_delay = DEFAULT_KEYB_REP_DELAY;
static unsigned int key_repeat_rate  = DEFAULT_KEYB_REP_RATE;

static unsigned char rep_scancode;
static struct timer_list atakeyb_rep_timer = { function: atakeyb_rep };

static void atakeyb_rep( unsigned long ignore )

{
	kbd_pt_regs = NULL;

	/* Disable keyboard for the time we call handle_scancode(), else a race
	 * in the keyboard tty queue may happen */
	atari_disable_irq( IRQ_MFP_ACIA );
	del_timer( &atakeyb_rep_timer );

	/* A keyboard int may have come in before we disabled the irq, so
	 * double-check whether rep_scancode is still != 0 */
	if (rep_scancode) {
		init_timer(&atakeyb_rep_timer);
		atakeyb_rep_timer.expires = jiffies + key_repeat_rate;
		add_timer( &atakeyb_rep_timer );

		handle_scancode(rep_scancode, 1);
	}

	atari_enable_irq( IRQ_MFP_ACIA );
}


/* ++roman: If a keyboard overrun happened, we can't tell in general how much
 * bytes have been lost and in which state of the packet structure we are now.
 * This usually causes keyboards bytes to be interpreted as mouse movements
 * and vice versa, which is very annoying. It seems better to throw away some
 * bytes (that are usually mouse bytes) than to misinterpret them. Therefor I
 * introduced the RESYNC state for IKBD data. In this state, the bytes up to
 * one that really looks like a key event (0x04..0xf2) or the start of a mouse
 * packet (0xf8..0xfb) are thrown away, but at most 2 bytes. This at least
 * speeds up the resynchronization of the event structure, even if maybe a
 * mouse movement is lost. However, nothing is perfect. For bytes 0x01..0x03,
 * it's really hard to decide whether they're mouse or keyboard bytes. Since
 * overruns usually occur when moving the Atari mouse rapidly, they're seen as
 * mouse bytes here. If this is wrong, only a make code of the keyboard gets
 * lost, which isn't too bad. Loosing a break code would be disastrous,
 * because then the keyboard repeat strikes...
 */

static void keyboard_interrupt(int irq, void *dummy, struct pt_regs *fp)
{
  u_char acia_stat;
  int scancode;
  int break_flag;

  /* save frame for register dump */
  kbd_pt_regs = fp;

 repeat:
  if (acia.mid_ctrl & ACIA_IRQ)
	if (atari_MIDI_interrupt_hook)
		atari_MIDI_interrupt_hook();
  acia_stat = acia.key_ctrl;
  /* check out if the interrupt came from this ACIA */
  if (!((acia_stat | acia.mid_ctrl) & ACIA_IRQ))
	return;

    if (acia_stat & ACIA_OVRN)
    {
	/* a very fast typist or a slow system, give a warning */
	/* ...happens often if interrupts were disabled for too long */
	printk( KERN_DEBUG "Keyboard overrun\n" );
	scancode = acia.key_data;
	/* Turn off autorepeating in case a break code has been lost */
	del_timer( &atakeyb_rep_timer );
	rep_scancode = 0;
	if (ikbd_self_test)
	    /* During self test, don't do resyncing, just process the code */
	    goto interpret_scancode;
	else if (IS_SYNC_CODE(scancode)) {
	    /* This code seem already to be the start of a new packet or a
	     * single scancode */
	    kb_state.state = KEYBOARD;
	    goto interpret_scancode;
	}
	else {
	    /* Go to RESYNC state and skip this byte */
	    kb_state.state = RESYNC;
	    kb_state.len = 1; /* skip max. 1 another byte */
	    goto repeat;
	}
    }

    if (acia_stat & ACIA_RDRF)	/* received a character */
    {
	scancode = acia.key_data;	/* get it or reset the ACIA, I'll get it! */
	tasklet_schedule(&keyboard_tasklet);
      interpret_scancode:
	switch (kb_state.state)
	{
	  case KEYBOARD:
	    switch (scancode)
	    {
	      case 0xF7:
		kb_state.state = AMOUSE;
		kb_state.len = 0;
		break;

	      case 0xF8:
	      case 0xF9:
     	      case 0xFA:
	      case 0xFB:
		kb_state.state = RMOUSE;
	    	kb_state.len = 1;
		kb_state.buf[0] = scancode;
		break;

	      case 0xFC:
		kb_state.state = CLOCK;
		kb_state.len = 0;
		break;

	      case 0xFE:
	      case 0xFF:
		kb_state.state = JOYSTICK;
		kb_state.len = 1;
		kb_state.buf[0] = scancode;
		break;

	      case 0xF1:
		/* during self-test, note that 0xf1 received */
		if (ikbd_self_test) {
		    ++ikbd_self_test;
		    self_test_last_rcv = jiffies;
		    break;
		}
		/* FALL THROUGH */
		
	      default:
		break_flag = scancode & BREAK_MASK;
		scancode &= ~BREAK_MASK;

		if (ikbd_self_test) {
		    /* Scancodes sent during the self-test stand for broken
		     * keys (keys being down). The code *should* be a break
		     * code, but nevertheless some AT keyboard interfaces send
		     * make codes instead. Therefore, simply ignore
		     * break_flag...
		     * */
		    int keyval = plain_map[scancode], keytyp;
		    
		    set_bit( scancode, broken_keys );
		    self_test_last_rcv = jiffies;
		    keyval = plain_map[scancode];
		    keytyp = KTYP(keyval) - 0xf0;
		    keyval = KVAL(keyval);

		    printk( KERN_WARNING "Key with scancode %d ", scancode );
		    if (keytyp == KT_LATIN || keytyp == KT_LETTER) {
			if (keyval < ' ')
			    printk( "('^%c') ", keyval + '@' );
			else
			    printk( "('%c') ", keyval );
		    }
		    printk( "is broken -- will be ignored.\n" );
		    break;
		}
		else if (test_bit( scancode, broken_keys ))
		    break;

		if (break_flag) {
		    del_timer( &atakeyb_rep_timer );
		    rep_scancode = 0;
		}
		else {
		    del_timer( &atakeyb_rep_timer );
		    rep_scancode = scancode;
		    atakeyb_rep_timer.expires = jiffies + key_repeat_delay;
		    add_timer( &atakeyb_rep_timer );
		}

		handle_scancode(scancode, !break_flag);
		break;
	    }
	    break;

	  case AMOUSE:
	    kb_state.buf[kb_state.len++] = scancode;
	    if (kb_state.len == 5)
	    {
		kb_state.state = KEYBOARD;
		/* not yet used */
		/* wake up someone waiting for this */
	    }
	    break;	

	  case RMOUSE:
	    kb_state.buf[kb_state.len++] = scancode;
	    if (kb_state.len == 3)
	    {
		kb_state.state = KEYBOARD;
		if (atari_mouse_interrupt_hook)
			atari_mouse_interrupt_hook(kb_state.buf);
	    }
	    break;

	  case JOYSTICK:
	    kb_state.buf[1] = scancode;
	    kb_state.state = KEYBOARD;
	    atari_joystick_interrupt(kb_state.buf);
	    break;

	  case CLOCK:
	    kb_state.buf[kb_state.len++] = scancode;
	    if (kb_state.len == 6)
	    {
		kb_state.state = KEYBOARD;
		/* wake up someone waiting for this.
		   But will this ever be used, as Linux keeps its own time.
		   Perhaps for synchronization purposes? */
		/* wake_up_interruptible(&clock_wait); */
	    }
	    break;

	  case RESYNC:
	    if (kb_state.len <= 0 || IS_SYNC_CODE(scancode)) {
		kb_state.state = KEYBOARD;
		goto interpret_scancode;
	    }
	    kb_state.len--;
	    break;
	}
    }

#if 0
    if (acia_stat & ACIA_CTS)
	/* cannot happen */;
#endif

    if (acia_stat & (ACIA_FE | ACIA_PE))
    {
	printk("Error in keyboard communication\n");
    }

    /* handle_scancode() can take a lot of time, so check again if
	 * some character arrived
	 */
    goto repeat;
}

/*
 * I write to the keyboard without using interrupts, I poll instead.
 * This takes for the maximum length string allowed (7) at 7812.5 baud
 * 8 data 1 start 1 stop bit: 9.0 ms
 * If this takes too long for normal operation, interrupt driven writing
 * is the solution. (I made a feeble attempt in that direction but I
 * kept it simple for now.)
 */
void ikbd_write(const char *str, int len)
{
    u_char acia_stat;

    if ((len < 1) || (len > 7))
	panic("ikbd: maximum string length exceeded");
    while (len)
    {
	acia_stat = acia.key_ctrl;
	if (acia_stat & ACIA_TDRE)
	{
	    acia.key_data = *str++;
	    len--;
	}
    }
}

/* Reset (without touching the clock) */
void ikbd_reset(void)
{
    static const char cmd[2] = { 0x80, 0x01 };
    
    ikbd_write(cmd, 2);

    /* if all's well code 0xF1 is returned, else the break codes of
       all keys making contact */
}

/* Set mouse button action */
void ikbd_mouse_button_action(int mode)
{
    char cmd[2] = { 0x07, mode };

    ikbd_write(cmd, 2);
}

/* Set relative mouse position reporting */
void ikbd_mouse_rel_pos(void)
{
    static const char cmd[1] = { 0x08 };

    ikbd_write(cmd, 1);
}

/* Set absolute mouse position reporting */
void ikbd_mouse_abs_pos(int xmax, int ymax)
{
    char cmd[5] = { 0x09, xmax>>8, xmax&0xFF, ymax>>8, ymax&0xFF };

    ikbd_write(cmd, 5);
}

/* Set mouse keycode mode */
void ikbd_mouse_kbd_mode(int dx, int dy)
{
    char cmd[3] = { 0x0A, dx, dy };

    ikbd_write(cmd, 3);
}

/* Set mouse threshold */
void ikbd_mouse_thresh(int x, int y)
{
    char cmd[3] = { 0x0B, x, y };

    ikbd_write(cmd, 3);
}

/* Set mouse scale */
void ikbd_mouse_scale(int x, int y)
{
    char cmd[3] = { 0x0C, x, y };

    ikbd_write(cmd, 3);
}

/* Interrogate mouse position */
void ikbd_mouse_pos_get(int *x, int *y)
{
    static const char cmd[1] = { 0x0D };

    ikbd_write(cmd, 1);

    /* wait for returning bytes */
}

/* Load mouse position */
void ikbd_mouse_pos_set(int x, int y)
{
    char cmd[6] = { 0x0E, 0x00, x>>8, x&0xFF, y>>8, y&0xFF };

    ikbd_write(cmd, 6);
}

/* Set Y=0 at bottom */
void ikbd_mouse_y0_bot(void)
{
    static const char cmd[1] = { 0x0F };

    ikbd_write(cmd, 1);
}

/* Set Y=0 at top */
void ikbd_mouse_y0_top(void)
{
    static const char cmd[1] = { 0x10 };

    ikbd_write(cmd, 1);
}

/* Resume */
void ikbd_resume(void)
{
    static const char cmd[1] = { 0x11 };

    ikbd_write(cmd, 1);
}

/* Disable mouse */
void ikbd_mouse_disable(void)
{
    static const char cmd[1] = { 0x12 };

    ikbd_write(cmd, 1);
}

/* Pause output */
void ikbd_pause(void)
{
    static const char cmd[1] = { 0x13 };

    ikbd_write(cmd, 1);
}

/* Set joystick event reporting */
void ikbd_joystick_event_on(void)
{
    static const char cmd[1] = { 0x14 };

    ikbd_write(cmd, 1);
}

/* Set joystick interrogation mode */
void ikbd_joystick_event_off(void)
{
    static const char cmd[1] = { 0x15 };

    ikbd_write(cmd, 1);
}

/* Joystick interrogation */
void ikbd_joystick_get_state(void)
{
    static const char cmd[1] = { 0x16 };

    ikbd_write(cmd, 1);
}

#if 0
/* This disables all other ikbd activities !!!! */
/* Set joystick monitoring */
void ikbd_joystick_monitor(int rate)
{
    static const char cmd[2] = { 0x17, rate };

    ikbd_write(cmd, 2);

    kb_state.state = JOYSTICK_MONITOR;
}
#endif

/* some joystick routines not in yet (0x18-0x19) */

/* Disable joysticks */
void ikbd_joystick_disable(void)
{
    static const char cmd[1] = { 0x1A };

    ikbd_write(cmd, 1);
}

/* Time-of-day clock set */
void ikbd_clock_set(int year, int month, int day, int hour, int minute, int second)
{
    char cmd[7] = { 0x1B, year, month, day, hour, minute, second };

    ikbd_write(cmd, 7);
}

/* Interrogate time-of-day clock */
void ikbd_clock_get(int *year, int *month, int *day, int *hour, int *minute, int second)
{
    static const char cmd[1] = { 0x1C };

    ikbd_write(cmd, 1);
}

/* Memory load */
void ikbd_mem_write(int address, int size, char *data)
{
    panic("Attempt to write data into keyboard memory");
}

/* Memory read */
void ikbd_mem_read(int address, char data[6])
{
    char cmd[3] = { 0x21, address>>8, address&0xFF };

    ikbd_write(cmd, 3);

    /* receive data and put it in data */
}

/* Controller execute */
void ikbd_exec(int address)
{
    char cmd[3] = { 0x22, address>>8, address&0xFF };

    ikbd_write(cmd, 3);
}

/* Status inquiries (0x87-0x9A) not yet implemented */

/* Set the state of the caps lock led. */
void atari_kbd_leds (unsigned int leds)
{
    char cmd[6] = {32, 0, 4, 1, 254 + ((leds & 4) != 0), 0};
    ikbd_write(cmd, 6);
}

/*
 * The original code sometimes left the interrupt line of 
 * the ACIAs low forever. I hope, it is fixed now.
 *
 * Martin Rogge, 20 Aug 1995
 */
 
int __init atari_keyb_init(void)
{
    /* setup key map */
    memcpy(key_maps[0], ataplain_map, sizeof(plain_map));
    memcpy(key_maps[1], atashift_map, sizeof(plain_map));
    memcpy(key_maps[4], atactrl_map, sizeof(plain_map));
    memcpy(key_maps[5], atashift_ctrl_map, sizeof(plain_map));
    memcpy(key_maps[8], ataalt_map, sizeof(plain_map));
    /* Atari doesn't have an altgr_map, so we can reuse its memory for
       atashift_alt_map */
    memcpy(key_maps[2], atashift_alt_map, sizeof(plain_map));
    key_maps[9]  = key_maps[2];
    key_maps[2]  = 0; /* ataaltgr_map */
    memcpy(key_maps[12], atactrl_alt_map, sizeof(plain_map));
    key_maps[13] = atashift_ctrl_alt_map;
    keymap_count = 8;

    /* say that we don't have an AltGr key */
    keyboard_type = KB_84;

    kb_state.state = KEYBOARD;
    kb_state.len = 0;

    request_irq(IRQ_MFP_ACIA, keyboard_interrupt, IRQ_TYPE_SLOW,
                "keyboard/mouse/MIDI", keyboard_interrupt);

    atari_turnoff_irq(IRQ_MFP_ACIA);
    do {
	/* reset IKBD ACIA */
	acia.key_ctrl = ACIA_RESET |
			(atari_switches & ATARI_SWITCH_IKBD) ? ACIA_RHTID : 0;
	(void)acia.key_ctrl;
	(void)acia.key_data;

	/* reset MIDI ACIA */
	acia.mid_ctrl = ACIA_RESET |
			(atari_switches & ATARI_SWITCH_MIDI) ? ACIA_RHTID : 0;
	(void)acia.mid_ctrl;
	(void)acia.mid_data;

	/* divide 500kHz by 64 gives 7812.5 baud */
	/* 8 data no parity 1 start 1 stop bit */
	/* receive interrupt enabled */
	/* RTS low (except if switch selected), transmit interrupt disabled */
	acia.key_ctrl = (ACIA_DIV64|ACIA_D8N1S|ACIA_RIE) |
			((atari_switches & ATARI_SWITCH_IKBD) ?
			 ACIA_RHTID : ACIA_RLTID);
	   
	acia.mid_ctrl = ACIA_DIV16 | ACIA_D8N1S |
			(atari_switches & ATARI_SWITCH_MIDI) ? ACIA_RHTID : 0;
    }
    /* make sure the interrupt line is up */
    while ((mfp.par_dt_reg & 0x10) == 0);

    /* enable ACIA Interrupts */ 
    mfp.active_edge &= ~0x10;
    atari_turnon_irq(IRQ_MFP_ACIA);

    ikbd_self_test = 1;
    ikbd_reset();
    /* wait for a period of inactivity (here: 0.25s), then assume the IKBD's
     * self-test is finished */
    self_test_last_rcv = jiffies;
    while (time_before(jiffies, self_test_last_rcv + HZ/4))
	barrier();
    /* if not incremented: no 0xf1 received */
    if (ikbd_self_test == 1)
	printk( KERN_ERR "WARNING: keyboard self test failed!\n" );
    ikbd_self_test = 0;
    
    ikbd_mouse_disable();
    ikbd_joystick_disable();

    atari_joystick_init();
  
    return 0;
}


int atari_kbdrate( struct kbd_repeat *k )

{
	if (k->delay > 0) {
		/* convert from msec to jiffies */
		key_repeat_delay = (k->delay * HZ + 500) / 1000;
		if (key_repeat_delay < 1)
			key_repeat_delay = 1;
	}
	if (k->rate > 0) {
		key_repeat_rate = (k->rate * HZ + 500) / 1000;
		if (key_repeat_rate < 1)
			key_repeat_rate = 1;
	}

	k->delay = key_repeat_delay * 1000 / HZ;
	k->rate  = key_repeat_rate  * 1000 / HZ;
	
	return( 0 );
}

int atari_kbd_translate(unsigned char keycode, unsigned char *keycodep, char raw_mode)
{
#ifdef CONFIG_MAGIC_SYSRQ
        /* ALT+HELP pressed? */
        if ((keycode == 98) && ((shift_state & 0xff) == 8))
                *keycodep = 0xff;
        else
#endif
                *keycodep = keycode;
        return 1;
}

