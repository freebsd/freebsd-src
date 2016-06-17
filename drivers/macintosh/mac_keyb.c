/*
 * drivers/char/mac_keyb.c
 *
 * Keyboard driver for Power Macintosh computers.
 *
 * Adapted from drivers/char/keyboard.c by Paul Mackerras
 * (see that file for its authors and contributors).
 *
 * Copyright (C) 1996 Paul Mackerras.
 *
 * Adapted to ADB changes and support for more devices by
 * Benjamin Herrenschmidt. Adapted from code in MkLinux
 * and reworked.
 *
 * Supported devices:
 *
 * - Standard 1 button mouse
 * - All standard Apple Extended protocol (handler ID 4)
 * - mouseman and trackman mice & trackballs
 * - PowerBook Trackpad (default setup: enable tapping)
 * - MicroSpeed mouse & trackball (needs testing)
 * - CH Products Trackball Pro (needs testing)
 * - Contour Design (Contour Mouse)
 * - Hunter digital (NoHandsMouse)
 * - Kensignton TurboMouse 5 (needs testing)
 * - Mouse Systems A3 mice and trackballs <aidan@kublai.com>
 * - MacAlly 2-buttons mouse (needs testing) <pochini@denise.shiny.it>
 *
 * To do:
 *
 * Improve Kensignton support, add MacX support as a dynamic
 * option (not a compile-time option).
 */

#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/tty.h>
#include <linux/mm.h>
#include <linux/signal.h>
#include <linux/ioport.h>
#include <linux/init.h>
#include <linux/tty_flip.h>
#include <linux/config.h>
#include <linux/notifier.h>

#include <asm/bitops.h>

#include <linux/adb.h>
#include <linux/cuda.h>
#include <linux/pmu.h>
#include <linux/kbd_kern.h>
#include <linux/kbd_ll.h>

#ifdef CONFIG_PMAC_BACKLIGHT
#include <asm/backlight.h>
#endif

#define KEYB_KEYREG	0	/* register # for key up/down data */
#define KEYB_LEDREG	2	/* register # for leds on ADB keyboard */
#define MOUSE_DATAREG	0	/* reg# for movement/button codes from mouse */

static int adb_message_handler(struct notifier_block *, unsigned long, void *);
static struct notifier_block mackeyb_adb_notifier = {
	adb_message_handler,
	NULL,
	0
};

/* this map indicates which keys shouldn't autorepeat. */
static unsigned char dont_repeat[128] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0,	/* esc...option */
	0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, /* fn, num lock */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, /* scroll lock */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0, /* R modifiers */
};

/* Simple translation table for the SysRq keys */

#ifdef CONFIG_MAGIC_SYSRQ
unsigned char mackbd_sysrq_xlate[128] =
	"asdfhgzxcv\000bqwer"				/* 0x00 - 0x0f */
	"yt123465=97-80o]"				/* 0x10 - 0x1f */
	"u[ip\rlj'k;\\,/nm."				/* 0x20 - 0x2f */
	"\t `\177\000\033\000\000\000\000\000\000\000\000\000\000"
							/* 0x30 - 0x3f */
	"\000\000\000*\000+\000\000\000\000\000/\r\000-\000"
							/* 0x40 - 0x4f */
	"\000\0000123456789\000\000\000"		/* 0x50 - 0x5f */
	"\205\206\207\203\210\211\000\213\000\215\000\000\000\000\000\212\000\214";
							/* 0x60 - 0x6f */
#endif

static u_short macplain_map[NR_KEYS] __initdata = {
	0xfb61,	0xfb73,	0xfb64,	0xfb66,	0xfb68,	0xfb67,	0xfb7a,	0xfb78,
	0xfb63,	0xfb76,	0xf200,	0xfb62,	0xfb71,	0xfb77,	0xfb65,	0xfb72,
	0xfb79,	0xfb74,	0xf031,	0xf032,	0xf033,	0xf034,	0xf036,	0xf035,
	0xf03d,	0xf039,	0xf037,	0xf02d,	0xf038,	0xf030,	0xf05d,	0xfb6f,
	0xfb75,	0xf05b,	0xfb69,	0xfb70,	0xf201,	0xfb6c,	0xfb6a,	0xf027,
	0xfb6b,	0xf03b,	0xf05c,	0xf02c,	0xf02f,	0xfb6e,	0xfb6d,	0xf02e,
	0xf009,	0xf020,	0xf060,	0xf07f,	0xf200,	0xf01b,	0xf702,	0xf703,
	0xf700,	0xf207,	0xf701,	0xf601,	0xf602,	0xf600,	0xf603,	0xf200,
	0xf200,	0xf310,	0xf200,	0xf30c,	0xf200,	0xf30a,	0xf200,	0xf208,
	0xf200,	0xf200,	0xf200,	0xf30d,	0xf30e,	0xf200,	0xf30b,	0xf200,
	0xf200,	0xf200,	0xf300,	0xf301,	0xf302,	0xf303,	0xf304,	0xf305,
	0xf306,	0xf307,	0xf200,	0xf308,	0xf309,	0xf200,	0xf200,	0xf200,
	0xf104,	0xf105,	0xf106,	0xf102,	0xf107,	0xf108,	0xf200,	0xf10a,
	0xf200,	0xf10c,	0xf200,	0xf209,	0xf200,	0xf109,	0xf200,	0xf10b,
	0xf200,	0xf11d,	0xf115,	0xf114,	0xf118,	0xf116,	0xf103,	0xf117,
	0xf101,	0xf119,	0xf100,	0xf700,	0xf701,	0xf702,	0xf200,	0xf200,
};

static u_short macshift_map[NR_KEYS] __initdata = {
	0xfb41,	0xfb53,	0xfb44,	0xfb46,	0xfb48,	0xfb47,	0xfb5a,	0xfb58,
	0xfb43,	0xfb56,	0xf200,	0xfb42,	0xfb51,	0xfb57,	0xfb45,	0xfb52,
	0xfb59,	0xfb54,	0xf021,	0xf040,	0xf023,	0xf024,	0xf05e,	0xf025,
	0xf02b,	0xf028,	0xf026,	0xf05f,	0xf02a,	0xf029,	0xf07d,	0xfb4f,
	0xfb55,	0xf07b,	0xfb49,	0xfb50,	0xf201,	0xfb4c,	0xfb4a,	0xf022,
	0xfb4b,	0xf03a,	0xf07c,	0xf03c,	0xf03f,	0xfb4e,	0xfb4d,	0xf03e,
	0xf009,	0xf020,	0xf07e,	0xf07f,	0xf200,	0xf01b,	0xf702,	0xf703,
	0xf700,	0xf207,	0xf701,	0xf601,	0xf602,	0xf600,	0xf603,	0xf200,
	0xf200,	0xf310,	0xf200,	0xf30c,	0xf200,	0xf30a,	0xf200,	0xf208,
	0xf200,	0xf200,	0xf200,	0xf30d,	0xf30e,	0xf200,	0xf30b,	0xf200,
	0xf200,	0xf200,	0xf300,	0xf301,	0xf302,	0xf303,	0xf304,	0xf305,
	0xf306,	0xf307,	0xf200,	0xf308,	0xf309,	0xf200,	0xf200,	0xf200,
	0xf10e,	0xf10f,	0xf110,	0xf10c,	0xf111,	0xf112,	0xf200,	0xf10a,
	0xf200,	0xf10c,	0xf200,	0xf203,	0xf200,	0xf113,	0xf200,	0xf10b,
	0xf200,	0xf11d,	0xf115,	0xf114,	0xf20b,	0xf116,	0xf10d,	0xf117,
	0xf10b,	0xf20a,	0xf10a,	0xf700,	0xf701,	0xf702,	0xf200,	0xf200,
};

static u_short macaltgr_map[NR_KEYS] __initdata = {
	0xf914,	0xfb73,	0xf917,	0xf919,	0xfb68,	0xfb67,	0xfb7a,	0xfb78,
	0xf916,	0xfb76,	0xf200,	0xf915,	0xfb71,	0xfb77,	0xf918,	0xfb72,
	0xfb79,	0xfb74,	0xf200,	0xf040,	0xf200,	0xf024,	0xf200,	0xf200,
	0xf200,	0xf05d,	0xf07b,	0xf05c,	0xf05b,	0xf07d,	0xf07e,	0xfb6f,
	0xfb75,	0xf200,	0xfb69,	0xfb70,	0xf201,	0xfb6c,	0xfb6a,	0xf200,
	0xfb6b,	0xf200,	0xf200,	0xf200,	0xf200,	0xfb6e,	0xfb6d,	0xf200,
	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf702,	0xf703,
	0xf700,	0xf207,	0xf701,	0xf601,	0xf602,	0xf600,	0xf603,	0xf200,
	0xf200,	0xf310,	0xf200,	0xf30c,	0xf200,	0xf30a,	0xf200,	0xf208,
	0xf200,	0xf200,	0xf200,	0xf30d,	0xf30e,	0xf200,	0xf30b,	0xf200,
	0xf200,	0xf200,	0xf90a,	0xf90b,	0xf90c,	0xf90d,	0xf90e,	0xf90f,
	0xf910,	0xf911,	0xf200,	0xf912,	0xf913,	0xf200,	0xf200,	0xf200,
	0xf510,	0xf511,	0xf512,	0xf50e,	0xf513,	0xf514,	0xf200,	0xf516,
	0xf200,	0xf10c,	0xf200,	0xf202,	0xf200,	0xf515,	0xf200,	0xf517,
	0xf200,	0xf11d,	0xf115,	0xf114,	0xf118,	0xf116,	0xf50f,	0xf117,
	0xf50d,	0xf119,	0xf50c,	0xf700,	0xf701,	0xf702,	0xf200,	0xf200,
};

static u_short macctrl_map[NR_KEYS] __initdata = {
	0xf001,	0xf013,	0xf004,	0xf006,	0xf008,	0xf007,	0xf01a,	0xf018,
	0xf003,	0xf016,	0xf200,	0xf002,	0xf011,	0xf017,	0xf005,	0xf012,
	0xf019,	0xf014,	0xf200,	0xf000,	0xf01b,	0xf01c,	0xf01e,	0xf01d,
	0xf200,	0xf200,	0xf01f,	0xf01f,	0xf07f,	0xf200,	0xf01d,	0xf00f,
	0xf015,	0xf01b,	0xf009,	0xf010,	0xf201,	0xf00c,	0xf00a,	0xf007,
	0xf00b,	0xf200,	0xf01c,	0xf200,	0xf07f,	0xf00e,	0xf00d,	0xf20e,
	0xf200,	0xf000,	0xf000,	0xf008,	0xf200,	0xf200,	0xf702,	0xf703,
	0xf700,	0xf207,	0xf701,	0xf601,	0xf602,	0xf600,	0xf603,	0xf200,
	0xf200,	0xf310,	0xf200,	0xf30c,	0xf200,	0xf30a,	0xf200,	0xf208,
	0xf200,	0xf200,	0xf200,	0xf30d,	0xf30e,	0xf200,	0xf30b,	0xf200,
	0xf200,	0xf200,	0xf300,	0xf301,	0xf302,	0xf303,	0xf304,	0xf305,
	0xf306,	0xf307,	0xf200,	0xf308,	0xf309,	0xf200,	0xf200,	0xf200,
	0xf104,	0xf105,	0xf106,	0xf102,	0xf107,	0xf108,	0xf200,	0xf10a,
	0xf200,	0xf10c,	0xf200,	0xf204,	0xf200,	0xf109,	0xf200,	0xf10b,
	0xf200,	0xf11d,	0xf115,	0xf114,	0xf118,	0xf116,	0xf103,	0xf117,
	0xf101,	0xf119,	0xf100,	0xf700,	0xf701,	0xf702,	0xf200,	0xf200,
};

static u_short macshift_ctrl_map[NR_KEYS] __initdata = {
	0xf001,	0xf013,	0xf004,	0xf006,	0xf008,	0xf007,	0xf01a,	0xf018,
	0xf003,	0xf016,	0xf200,	0xf002,	0xf011,	0xf017,	0xf005,	0xf012,
	0xf019,	0xf014,	0xf200,	0xf000,	0xf200,	0xf200,	0xf200,	0xf200,
	0xf200,	0xf200,	0xf200,	0xf01f,	0xf200,	0xf200,	0xf200,	0xf00f,
	0xf015,	0xf200,	0xf009,	0xf010,	0xf201,	0xf00c,	0xf00a,	0xf200,
	0xf00b,	0xf200,	0xf200,	0xf200,	0xf200,	0xf00e,	0xf00d,	0xf200,
	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf702,	0xf703,
	0xf700,	0xf207,	0xf701,	0xf601,	0xf602,	0xf600,	0xf603,	0xf200,
	0xf200,	0xf310,	0xf200,	0xf30c,	0xf200,	0xf30a,	0xf200,	0xf208,
	0xf200,	0xf200,	0xf200,	0xf30d,	0xf30e,	0xf200,	0xf30b,	0xf200,
	0xf200,	0xf200,	0xf300,	0xf301,	0xf302,	0xf303,	0xf304,	0xf305,
	0xf306,	0xf307,	0xf200,	0xf308,	0xf309,	0xf200,	0xf200,	0xf200,
	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,
	0xf200,	0xf10c,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,
	0xf200,	0xf11d,	0xf115,	0xf114,	0xf118,	0xf116,	0xf200,	0xf117,
	0xf200,	0xf119,	0xf200,	0xf700,	0xf701,	0xf702,	0xf200,	0xf20c,
};

static u_short macalt_map[NR_KEYS] __initdata = {
	0xf861,	0xf873,	0xf864,	0xf866,	0xf868,	0xf867,	0xf87a,	0xf878,
	0xf863,	0xf876,	0xf200,	0xf862,	0xf871,	0xf877,	0xf865,	0xf872,
	0xf879,	0xf874,	0xf831,	0xf832,	0xf833,	0xf834,	0xf836,	0xf835,
	0xf83d,	0xf839,	0xf837,	0xf82d,	0xf838,	0xf830,	0xf85d,	0xf86f,
	0xf875,	0xf85b,	0xf869,	0xf870,	0xf80d,	0xf86c,	0xf86a,	0xf827,
	0xf86b,	0xf83b,	0xf85c,	0xf82c,	0xf82f,	0xf86e,	0xf86d,	0xf82e,
	0xf809,	0xf820,	0xf860,	0xf87f,	0xf200,	0xf81b,	0xf702,	0xf703,
	0xf700,	0xf207,	0xf701,	0xf210,	0xf211,	0xf600,	0xf603,	0xf200,
	0xf200,	0xf310,	0xf200,	0xf30c,	0xf200,	0xf30a,	0xf200,	0xf208,
	0xf200,	0xf200,	0xf200,	0xf30d,	0xf30e,	0xf200,	0xf30b,	0xf200,
	0xf200,	0xf200,	0xf900,	0xf901,	0xf902,	0xf903,	0xf904,	0xf905,
	0xf906,	0xf907,	0xf200,	0xf908,	0xf909,	0xf200,	0xf200,	0xf200,
	0xf504,	0xf505,	0xf506,	0xf502,	0xf507,	0xf508,	0xf200,	0xf50a,
	0xf200,	0xf10c,	0xf200,	0xf209,	0xf200,	0xf509,	0xf200,	0xf50b,
	0xf200,	0xf11d,	0xf115,	0xf114,	0xf118,	0xf116,	0xf503,	0xf117,
	0xf501,	0xf119,	0xf500,	0xf700,	0xf701,	0xf702,	0xf200,	0xf200,
};

static u_short macctrl_alt_map[NR_KEYS] __initdata = {
	0xf801,	0xf813,	0xf804,	0xf806,	0xf808,	0xf807,	0xf81a,	0xf818,
	0xf803,	0xf816,	0xf200,	0xf802,	0xf811,	0xf817,	0xf805,	0xf812,
	0xf819,	0xf814,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,
	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf80f,
	0xf815,	0xf200,	0xf809,	0xf810,	0xf201,	0xf80c,	0xf80a,	0xf200,
	0xf80b,	0xf200,	0xf200,	0xf200,	0xf200,	0xf80e,	0xf80d,	0xf200,
	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf200,	0xf702,	0xf703,
	0xf700,	0xf207,	0xf701,	0xf601,	0xf602,	0xf600,	0xf603,	0xf200,
	0xf200,	0xf310,	0xf200,	0xf30c,	0xf200,	0xf30a,	0xf200,	0xf208,
	0xf200,	0xf200,	0xf200,	0xf30d,	0xf30e,	0xf200,	0xf30b,	0xf200,
	0xf200,	0xf200,	0xf300,	0xf301,	0xf302,	0xf303,	0xf304,	0xf305,
	0xf306,	0xf307,	0xf200,	0xf308,	0xf309,	0xf200,	0xf200,	0xf200,
	0xf504,	0xf505,	0xf506,	0xf502,	0xf507,	0xf508,	0xf200,	0xf50a,
	0xf200,	0xf10c,	0xf200,	0xf200,	0xf200,	0xf509,	0xf200,	0xf50b,
	0xf200,	0xf11d,	0xf115,	0xf114,	0xf118,	0xf116,	0xf503,	0xf117,
	0xf501,	0xf119,	0xf500,	0xf700,	0xf701,	0xf702,	0xf200,	0xf200,
};


static void kbd_repeat(unsigned long);
static struct timer_list repeat_timer = { function: kbd_repeat };
static int last_keycode;

static void mackeyb_probe(void);

static void keyboard_input(unsigned char *, int, struct pt_regs *, int);
static void input_keycode(int, int);
static void leds_done(struct adb_request *);
static void mac_put_queue(int);

static void buttons_input(unsigned char *, int, struct pt_regs *, int);

static void init_trackpad(int id);
static void init_trackball(int id);
static void init_turbomouse(int id);
static void init_microspeed(int id);
static void init_ms_a3(int id);

#ifdef CONFIG_ADBMOUSE
/* XXX: Hook for mouse driver */
void (*adb_mouse_interrupt_hook)(unsigned char *, int);
int adb_emulate_buttons = 0;
int adb_button2_keycode = 0x7d;	/* right control key */
int adb_button3_keycode = 0x7c; /* right option key */
#endif

extern struct kbd_struct kbd_table[];

extern void handle_scancode(unsigned char, int);

static struct adb_ids keyboard_ids;
static struct adb_ids mouse_ids;
static struct adb_ids buttons_ids;

/* Kind of mouse  */
#define ADBMOUSE_STANDARD_100	0	/* Standard 100cpi mouse (handler 1) */
#define ADBMOUSE_STANDARD_200	1	/* Standard 200cpi mouse (handler 2) */
#define ADBMOUSE_EXTENDED	2	/* Apple Extended mouse (handler 4) */
#define ADBMOUSE_TRACKBALL	3	/* TrackBall (handler 4) */
#define ADBMOUSE_TRACKPAD       4	/* Apple's PowerBook trackpad (handler 4) */
#define ADBMOUSE_TURBOMOUSE5    5	/* Turbomouse 5 (previously req. mousehack) */
#define ADBMOUSE_MICROSPEED	6	/* Microspeed mouse (&trackball ?), MacPoint */
#define ADBMOUSE_TRACKBALLPRO	7	/* Trackball Pro (special buttons) */
#define ADBMOUSE_MS_A3		8	/* Mouse systems A3 trackball (handler 3) */
#define ADBMOUSE_MACALLY2	9	/* MacAlly 2-button mouse */

static int adb_mouse_kinds[16];

int mackbd_setkeycode(unsigned int scancode, unsigned int keycode)
{
	return -EINVAL;
}

int mackbd_getkeycode(unsigned int scancode)
{
	return -EINVAL;
}

int mackbd_translate(unsigned char keycode, unsigned char *keycodep,
		     char raw_mode)
{
	if (!raw_mode) {
		/*
		 * Convert R-shift/control/option to L version.
		 */
		switch (keycode) {
		case 0x7b: keycode = 0x38; break; /* R-shift */
		case 0x7c: keycode = 0x3a; break; /* R-option */
		case 0x7d: keycode = 0x36; break; /* R-control */
		}
	}
	*keycodep = keycode;
	return 1;
}

char mackbd_unexpected_up(unsigned char keycode)
{
	return 0x80;
}

static void
keyboard_input(unsigned char *data, int nb, struct pt_regs *regs, int apoll)
{
	/* first check this is from register 0 */
	if (nb != 3 || (data[0] & 3) != KEYB_KEYREG)
		return;		/* ignore it */
	kbd_pt_regs = regs;
	input_keycode(data[1], 0);
	if (!(data[2] == 0xff || (data[2] == 0x7f && data[1] == 0x7f)))
		input_keycode(data[2], 0);
}

static void
input_keycode(int keycode, int repeat)
{
	struct kbd_struct *kbd;
	int up_flag;

 	kbd = kbd_table + fg_console;
	up_flag = (keycode & 0x80);
	keycode &= 0x7f;

	/* on the powerbook 3400, the power key gives code 0x7e */
	if (keycode == 0x7e)
		keycode = 0x7f;
	/* remap the "Fn" key of the PowerBook G3 Series to 0x48
	   to avoid conflict with button emulation */
	if (keycode == 0x3f)
		keycode = 0x48;

	if (!repeat)
		del_timer(&repeat_timer);

#ifdef CONFIG_ADBMOUSE
	/*
	 * XXX: Add mouse button 2+3 fake codes here if mouse open.
	 *	Keep track of 'button' states here as we only send
	 *	single up/down events!
	 *	Really messy; might need to check if keyboard is in
	 *	VC_RAW mode.
	 *	Might also want to know how many buttons need to be emulated.
	 *	-> hide this as function in arch/m68k/mac ?
	 */
	if (adb_emulate_buttons
	    && (keycode == adb_button2_keycode
		|| keycode == adb_button3_keycode)
	    && (adb_mouse_interrupt_hook || console_loglevel == 10)) {
		int button;
		/* faked ADB packet */
		static unsigned char data[4] = { 0, 0x80, 0x80, 0x80 };

		button = keycode == adb_button2_keycode? 2: 3;
		if (data[button] != up_flag) {
			/* send a fake mouse packet */
			data[button] = up_flag;
			if (console_loglevel >= 8)
				printk("fake mouse event: %x %x %x\n",
				       data[1], data[2], data[3]);
			if (adb_mouse_interrupt_hook)
				adb_mouse_interrupt_hook(data, 4);
		}
		return;
	}
#endif /* CONFIG_ADBMOUSE */

	if (kbd->kbdmode != VC_RAW) {
		if (!up_flag && !dont_repeat[keycode]) {
			last_keycode = keycode;
			repeat_timer.expires = jiffies + (repeat? HZ/15: HZ/2);
			add_timer(&repeat_timer);
		}

		/*
		 * adb kludge!! Imitate pc caps lock behaviour by
		 * generating an up/down event for each time caps
		 * is pressed/released. Also, makes sure that the
		 * LED are handled.  atong@uiuc.edu
		 */
		 switch (keycode) {
		 /*case 0xb9:*/
		 case 0x39:
			handle_scancode(0x39, 1);
			handle_scancode(0x39, 0);
		 	tasklet_schedule(&keyboard_tasklet);
		 	return;
		 case 0x47:
		 /*case 0xc7:*/
		 	tasklet_schedule(&keyboard_tasklet);
		 	break;
		 }
	}

	handle_scancode(keycode, !up_flag);
	tasklet_schedule(&keyboard_tasklet);
}

static void
kbd_repeat(unsigned long xxx)
{
	unsigned long flags;

	save_flags(flags);
	cli();
	input_keycode(last_keycode, 1);
	restore_flags(flags);
}

static void mac_put_queue(int ch)
{
	extern struct tty_driver console_driver;
	struct tty_struct *tty;

	tty = console_driver.table? console_driver.table[fg_console]: NULL;
	if (tty) {
		tty_insert_flip_char(tty, ch, 0);
		con_schedule_flip(tty);
	}
}

#ifdef CONFIG_ADBMOUSE
static void
mouse_input(unsigned char *data, int nb, struct pt_regs *regs, int autopoll)
{
  /* [ACA:23-Mar-97] Three button mouse support.  This is designed to
     function with MkLinux DR-2.1 style X servers.  It only works with
     three-button mice that conform to Apple's multi-button mouse
     protocol. */

  /*
    The X server for MkLinux DR2.1 uses the following unused keycodes to
    read the mouse:

    0x7e  This indicates that the next two keycodes should be interpreted
          as mouse information.  The first following byte's high bit
          represents the state of the left button.  The lower seven bits
          represent the x-axis acceleration.  The lower seven bits of the
          second byte represent y-axis acceleration.

    0x3f  The x server interprets this keycode as a middle button
          release.

    0xbf  The x server interprets this keycode as a middle button
          depress.

    0x40  The x server interprets this keycode as a right button
          release.

    0xc0  The x server interprets this keycode as a right button
          depress.

    NOTES: There should be a better way of handling mice in the X server.
    The MOUSE_ESCAPE code (0x7e) should be followed by three bytes instead
    of two.  The three mouse buttons should then, in the X server, be read
    as the high-bits of all three bytes.  The x and y motions can still be
    in the first two bytes.  Maybe I'll do this...
  */

  /*
    Handler 1 -- 100cpi original Apple mouse protocol.
    Handler 2 -- 200cpi original Apple mouse protocol.

    For Apple's standard one-button mouse protocol the data array will
    contain the following values:

                BITS    COMMENTS
    data[0] = dddd 1100 ADB command: Talk, register 0, for device dddd.
    data[1] = bxxx xxxx First button and x-axis motion.
    data[2] = byyy yyyy Second button and y-axis motion.

    Handler 4 -- Apple Extended mouse protocol.

    For Apple's 3-button mouse protocol the data array will contain the
    following values:

		BITS    COMMENTS
    data[0] = dddd 1100 ADB command: Talk, register 0, for device dddd.
    data[1] = bxxx xxxx Left button and x-axis motion.
    data[2] = byyy yyyy Second button and y-axis motion.
    data[3] = byyy bxxx Third button and fourth button.  Y is additional
	      high bits of y-axis motion.  XY is additional
	      high bits of x-axis motion.

    MacAlly 2-button mouse protocol.

    For MacAlly 2-button mouse protocol the data array will contain the
    following values:

		BITS    COMMENTS
    data[0] = dddd 1100 ADB command: Talk, register 0, for device dddd.
    data[1] = bxxx xxxx Left button and x-axis motion.
    data[2] = byyy yyyy Right button and y-axis motion.
    data[3] = ???? ???? unknown
    data[4] = ???? ???? unknown

  */
	struct kbd_struct *kbd;

	/* If it's a trackpad, we alias the second button to the first.
	   NOTE: Apple sends an ADB flush command to the trackpad when
	         the first (the real) button is released. We could do
		 this here using async flush requests.
	*/
	switch (adb_mouse_kinds[(data[0]>>4) & 0xf])
	{
	    case ADBMOUSE_TRACKPAD:
		data[1] = (data[1] & 0x7f) | ((data[1] & data[2]) & 0x80);
		data[2] = data[2] | 0x80;
		break;
	    case ADBMOUSE_MICROSPEED:
		data[1] = (data[1] & 0x7f) | ((data[3] & 0x01) << 7);
		data[2] = (data[2] & 0x7f) | ((data[3] & 0x02) << 6);
		data[3] = (data[3] & 0x77) | ((data[3] & 0x04) << 5)
			| (data[3] & 0x08);
		break;
	    case ADBMOUSE_TRACKBALLPRO:
		data[1] = (data[1] & 0x7f) | (((data[3] & 0x04) << 5)
			& ((data[3] & 0x08) << 4));
		data[2] = (data[2] & 0x7f) | ((data[3] & 0x01) << 7);
		data[3] = (data[3] & 0x77) | ((data[3] & 0x02) << 6);
		break;
	    case ADBMOUSE_MS_A3:
		data[1] = (data[1] & 0x7f) | ((data[3] & 0x01) << 7);
		data[2] = (data[2] & 0x7f) | ((data[3] & 0x02) << 6);
		data[3] = ((data[3] & 0x04) << 5);
		break;
            case ADBMOUSE_MACALLY2:
		data[3] = (data[2] & 0x80) ? 0x80 : 0x00;
		data[2] |= 0x80;  /* Right button is mapped as button 3 */
		nb=4;
                break;
	}

	if (adb_mouse_interrupt_hook)
		adb_mouse_interrupt_hook(data, nb);

	kbd = kbd_table + fg_console;

	/* Only send mouse codes when keyboard is in raw mode. */
	if (kbd->kbdmode == VC_RAW) {
		static unsigned char uch_ButtonStateSecond = 0x80;
		unsigned char uchButtonSecond;

		/* Send first button, second button and movement. */
		mac_put_queue(0x7e);
		mac_put_queue(data[1]);
		mac_put_queue(data[2]);

		/* [ACA: Are there any two-button ADB mice that use handler 1 or 2?] */

		/* Store the button state. */
		uchButtonSecond = (data[2] & 0x80);

		/* Send second button. */
		if (uchButtonSecond != uch_ButtonStateSecond) {
			mac_put_queue(0x3f | uchButtonSecond);
			uch_ButtonStateSecond = uchButtonSecond;
		}

		/* Macintosh 3-button mouse (handler 4). */
		if (nb >= 4) {
			static unsigned char uch_ButtonStateThird = 0x80;
			unsigned char uchButtonThird;

			/* Store the button state for speed. */
			uchButtonThird = (data[3] & 0x80);

			/* Send third button. */
			if (uchButtonThird != uch_ButtonStateThird) {
				mac_put_queue(0x40 | uchButtonThird);
				uch_ButtonStateThird = uchButtonThird;
			}
		}
	}
}
#endif /* CONFIG_ADBMOUSE */

static void
buttons_input(unsigned char *data, int nb, struct pt_regs *regs, int autopoll)
{
#ifdef CONFIG_PMAC_BACKLIGHT
	int backlight = get_backlight_level();

	/*
	 * XXX: Where is the contrast control for the passive?
	 *  -- Cort
	 */

	/* Ignore data from register other than 0 */
	if ((data[0] & 0x3) || (nb < 2))
		return;

	switch (data[1]) {
	case 0x8:		/* mute */
		break;

	case 0x7:		/* contrast decrease */
		break;

	case 0x6:		/* contrast increase */
		break;

	case 0xa:		/* brightness decrease */
		if (backlight < 0)
			break;
		if (backlight > BACKLIGHT_OFF)
			set_backlight_level(backlight-1);
		else
			set_backlight_level(BACKLIGHT_OFF);
		break;

	case 0x9:		/* brightness increase */
		if (backlight < 0)
			break;
		if (backlight < BACKLIGHT_MAX)
			set_backlight_level(backlight+1);
		else 
			set_backlight_level(BACKLIGHT_MAX);
		break;
	}
#endif /* CONFIG_PMAC_BACKLIGHT */
}

/* Map led flags as defined in kbd_kern.h to bits for Apple keyboard. */
static unsigned char mac_ledmap[8] = {
    0,		/* none */
    4,		/* scroll lock */
    1,		/* num lock */
    5,		/* scroll + num lock */
    2,		/* caps lock */
    6,		/* caps + scroll lock */
    3,		/* caps + num lock */
    7,		/* caps + num + scroll lock */
};

static struct adb_request led_request;
static int leds_pending[16];
static int pending_devs[16];
static int pending_led_start=0;
static int pending_led_end=0;

static void real_mackbd_leds(unsigned char leds, int device)
{
    if (led_request.complete) {
	adb_request(&led_request, leds_done, 0, 3,
		    ADB_WRITEREG(device, KEYB_LEDREG), 0xff,
		    ~mac_ledmap[leds]);
    } else {
	if (!(leds_pending[device] & 0x100)) {
	    pending_devs[pending_led_end] = device;
	    pending_led_end++;
	    pending_led_end = (pending_led_end < 16) ? pending_led_end : 0;
	}
	leds_pending[device] = leds | 0x100;
    }
}

void mackbd_leds(unsigned char leds)
{
    int i;

    for(i = 0; i < keyboard_ids.nids; i++)
	real_mackbd_leds(leds,keyboard_ids.id[i]);
}

static void leds_done(struct adb_request *req)
{
    int leds,device;

    if (pending_led_start != pending_led_end) {
	device = pending_devs[pending_led_start];
	leds = leds_pending[device] & 0xff;
	leds_pending[device] = 0;
	pending_led_start++;
	pending_led_start = (pending_led_start < 16) ? pending_led_start : 0;
	real_mackbd_leds(leds,device);
    }

}

void __init mackbd_init_hw(void)
{
#ifdef CONFIG_PPC
	if ( (_machine != _MACH_chrp) && (_machine != _MACH_Pmac) )
		return;
#endif
#ifdef CONFIG_MAC
	if (!MACH_IS_MAC)
		return;
#endif

	/* setup key map */
	memcpy(key_maps[0], macplain_map, sizeof(plain_map));
	memcpy(key_maps[1], macshift_map, sizeof(plain_map));
	memcpy(key_maps[2], macaltgr_map, sizeof(plain_map));
	memcpy(key_maps[4], macctrl_map, sizeof(plain_map));
	memcpy(key_maps[5], macshift_ctrl_map, sizeof(plain_map));
	memcpy(key_maps[8], macalt_map, sizeof(plain_map));
	memcpy(key_maps[12], macctrl_alt_map, sizeof(plain_map));

#ifdef CONFIG_ADBMOUSE
	/* initialize mouse interrupt hook */
	adb_mouse_interrupt_hook = NULL;
#endif

	led_request.complete = 1;

	mackeyb_probe();

	notifier_chain_register(&adb_client_list, &mackeyb_adb_notifier);
}

static int
adb_message_handler(struct notifier_block *this, unsigned long code, void *x)
{
	unsigned long flags;

	switch (code) {
	case ADB_MSG_PRE_RESET:
	case ADB_MSG_POWERDOWN:
		/* Stop the repeat timer. Autopoll is already off at this point */
		save_flags(flags);
		cli();
		del_timer(&repeat_timer);
		restore_flags(flags);

		/* Stop pending led requests */
		while(!led_request.complete)
			adb_poll();
		break;

	case ADB_MSG_POST_RESET:
		mackeyb_probe();
		break;
	}
	return NOTIFY_DONE;
}

static void
mackeyb_probe(void)
{
	struct adb_request req;
	int i;

#ifdef CONFIG_ADBMOUSE
	adb_register(ADB_MOUSE, 0, &mouse_ids, mouse_input);
#endif /* CONFIG_ADBMOUSE */

	adb_register(ADB_KEYBOARD, 0, &keyboard_ids, keyboard_input);
	adb_register(0x07, 0x1F, &buttons_ids, buttons_input);

	for (i = 0; i < keyboard_ids.nids; i++) {
		int id = keyboard_ids.id[i];

		/* turn off all leds */
		adb_request(&req, NULL, ADBREQ_SYNC, 3,
			    ADB_WRITEREG(id, KEYB_LEDREG), 0xff, 0xff);

		/* Enable full feature set of the keyboard
		   ->get it to send separate codes for left and right shift,
		   control, option keys */
#if 0		/* handler 5 doesn't send separate codes for R modifiers */
		if (adb_try_handler_change(id, 5))
			printk("ADB keyboard at %d, handler set to 5\n", id);
		else
#endif
		if (adb_try_handler_change(id, 3))
			printk("ADB keyboard at %d, handler set to 3\n", id);
		else
			printk("ADB keyboard at %d, handler 1\n", id);
	}

	/* Try to switch all mice to handler 4, or 2 for three-button
	   mode and full resolution. */
	for (i = 0; i < mouse_ids.nids; i++) {
		int id = mouse_ids.id[i];
		if (adb_try_handler_change(id, 4)) {
			printk("ADB mouse at %d, handler set to 4", id);
			adb_mouse_kinds[id] = ADBMOUSE_EXTENDED;
		}
		else if (adb_try_handler_change(id, 0x2F)) {
			printk("ADB mouse at %d, handler set to 0x2F", id);
			adb_mouse_kinds[id] = ADBMOUSE_MICROSPEED;
		}
		else if (adb_try_handler_change(id, 0x42)) {
			printk("ADB mouse at %d, handler set to 0x42", id);
			adb_mouse_kinds[id] = ADBMOUSE_TRACKBALLPRO;
		}
		else if (adb_try_handler_change(id, 0x66)) {
			printk("ADB mouse at %d, handler set to 0x66", id);
			adb_mouse_kinds[id] = ADBMOUSE_MICROSPEED;
		}
		else if (adb_try_handler_change(id, 0x5F)) {
			printk("ADB mouse at %d, handler set to 0x5F", id);
			adb_mouse_kinds[id] = ADBMOUSE_MICROSPEED;
		}
		else if (adb_try_handler_change(id, 3)) {
			printk("ADB mouse at %d, handler set to 3", id);
			adb_mouse_kinds[id] = ADBMOUSE_MS_A3;
		}
		else if (adb_try_handler_change(id, 2)) {
			printk("ADB mouse at %d, handler set to 2", id);
			adb_mouse_kinds[id] = ADBMOUSE_STANDARD_200;
		}
		else {
			printk("ADB mouse at %d, handler 1", id);
			adb_mouse_kinds[id] = ADBMOUSE_STANDARD_100;
		}

		if ((adb_mouse_kinds[id] == ADBMOUSE_TRACKBALLPRO)
		    || (adb_mouse_kinds[id] == ADBMOUSE_MICROSPEED)) {
			init_microspeed(id);
		} else if (adb_mouse_kinds[id] == ADBMOUSE_MS_A3) {
			init_ms_a3(id);
		}  else if (adb_mouse_kinds[id] ==  ADBMOUSE_EXTENDED) {
			/*
			 * Register 1 is usually used for device
			 * identification.  Here, we try to identify
			 * a known device and call the appropriate
			 * init function.
			 */
			adb_request(&req, NULL, ADBREQ_SYNC | ADBREQ_REPLY, 1,
				    ADB_READREG(id, 1));

			if ((req.reply_len) &&
			    (req.reply[1] == 0x9a) && ((req.reply[2] == 0x21)
				|| (req.reply[2] == 0x20)))
				init_trackball(id);
			else if ((req.reply_len >= 4) &&
			    (req.reply[1] == 0x74) && (req.reply[2] == 0x70) &&
			    (req.reply[3] == 0x61) && (req.reply[4] == 0x64))
				init_trackpad(id);
			else if ((req.reply_len >= 4) &&
			    (req.reply[1] == 0x4b) && (req.reply[2] == 0x4d) &&
			    (req.reply[3] == 0x4c) && (req.reply[4] == 0x31))
				init_turbomouse(id);
			else if ((req.reply_len == 9) &&
			    (req.reply[1] == 0x4b) && (req.reply[2] == 0x4f) &&
			    (req.reply[3] == 0x49) && (req.reply[4] == 0x54)){
				if (adb_try_handler_change(id, 0x42)) {
					printk("\nADB MacAlly 2-button mouse at %d, handler set to 0x42", id);
					adb_mouse_kinds[id] = ADBMOUSE_MACALLY2;
				}
			}
		}
		printk("\n");
        }
}

static void
init_trackpad(int id)
{
	struct adb_request req;
	unsigned char r1_buffer[8];

	printk(" (trackpad)");

	adb_mouse_kinds[id] = ADBMOUSE_TRACKPAD;

	adb_request(&req, NULL, ADBREQ_SYNC | ADBREQ_REPLY, 1,
	ADB_READREG(id,1));
	if (req.reply_len < 8)
	    printk("bad length for reg. 1\n");
	else
	{
	    memcpy(r1_buffer, &req.reply[1], 8);
	    adb_request(&req, NULL, ADBREQ_SYNC, 9,
	        ADB_WRITEREG(id,1),
	            r1_buffer[0],
	            r1_buffer[1],
	            r1_buffer[2],
	            r1_buffer[3],
	            r1_buffer[4],
	            r1_buffer[5],
	            0x0d, /*r1_buffer[6],*/
	            r1_buffer[7]);

            adb_request(&req, NULL, ADBREQ_SYNC, 9,
	        ADB_WRITEREG(id,2),
		    0x99,
		    0x94,
		    0x19,
		    0xff,
		    0xb2,
		    0x8a,
		    0x1b,
		    0x50);

	    adb_request(&req, NULL, ADBREQ_SYNC, 9,
	        ADB_WRITEREG(id,1),
	            r1_buffer[0],
	            r1_buffer[1],
	            r1_buffer[2],
	            r1_buffer[3],
	            r1_buffer[4],
	            r1_buffer[5],
	            0x03, /*r1_buffer[6],*/
	            r1_buffer[7]);

	    /* Without this flush, the trackpad may be locked up */	    
	    adb_request(&req, NULL, ADBREQ_SYNC, 1, ADB_FLUSH(id));
        }
}

static void
init_trackball(int id)
{
	struct adb_request req;

	printk(" (trackman/mouseman)");

	adb_mouse_kinds[id] = ADBMOUSE_TRACKBALL;

	adb_request(&req, NULL, ADBREQ_SYNC, 3,
	ADB_WRITEREG(id,1), 00,0x81);

	adb_request(&req, NULL, ADBREQ_SYNC, 3,
	ADB_WRITEREG(id,1), 01,0x81);

	adb_request(&req, NULL, ADBREQ_SYNC, 3,
	ADB_WRITEREG(id,1), 02,0x81);

	adb_request(&req, NULL, ADBREQ_SYNC, 3,
	ADB_WRITEREG(id,1), 03,0x38);

	adb_request(&req, NULL, ADBREQ_SYNC, 3,
	ADB_WRITEREG(id,1), 00,0x81);

	adb_request(&req, NULL, ADBREQ_SYNC, 3,
	ADB_WRITEREG(id,1), 01,0x81);

	adb_request(&req, NULL, ADBREQ_SYNC, 3,
	ADB_WRITEREG(id,1), 02,0x81);

	adb_request(&req, NULL, ADBREQ_SYNC, 3,
	ADB_WRITEREG(id,1), 03,0x38);
}

static void
init_turbomouse(int id)
{
	struct adb_request req;

        printk(" (TurboMouse 5)");

	adb_mouse_kinds[id] = ADBMOUSE_TURBOMOUSE5;

	adb_request(&req, NULL, ADBREQ_SYNC, 1, ADB_FLUSH(id));

	adb_request(&req, NULL, ADBREQ_SYNC, 1, ADB_FLUSH(3));

	adb_request(&req, NULL, ADBREQ_SYNC, 9,
	ADB_WRITEREG(3,2),
	    0xe7,
	    0x8c,
	    0,
	    0,
	    0,
	    0xff,
	    0xff,
	    0x94);

	adb_request(&req, NULL, ADBREQ_SYNC, 1, ADB_FLUSH(3));

	adb_request(&req, NULL, ADBREQ_SYNC, 9,
	ADB_WRITEREG(3,2),
	    0xa5,
	    0x14,
	    0,
	    0,
	    0x69,
	    0xff,
	    0xff,
	    0x27);
}

static void
init_microspeed(int id)
{
	struct adb_request req;

        printk(" (Microspeed/MacPoint or compatible)");

	adb_request(&req, NULL, ADBREQ_SYNC, 1, ADB_FLUSH(id));

	/* This will initialize mice using the Microspeed, MacPoint and
	   other compatible firmware. Bit 12 enables extended protocol.

	   Register 1 Listen (4 Bytes)
            0 -  3     Button is mouse (set also for double clicking!!!)
            4 -  7     Button is locking (affects change speed also)
            8 - 11     Button changes speed
           12          1 = Extended mouse mode, 0 = normal mouse mode
           13 - 15     unused 0
           16 - 23     normal speed
           24 - 31     changed speed

       Register 1 talk holds version and product identification information.
       Register 1 Talk (4 Bytes):
            0 -  7     Product code
            8 - 23     undefined, reserved
           24 - 31     Version number

       Speed 0 is max. 1 to 255 set speed in increments of 1/256 of max.
 */
	adb_request(&req, NULL, ADBREQ_SYNC, 5,
	ADB_WRITEREG(id,1),
	    0x20,	/* alt speed = 0x20 (rather slow) */
	    0x00,	/* norm speed = 0x00 (fastest) */
	    0x10,	/* extended protocol, no speed change */
	    0x07);	/* all buttons enabled as mouse buttons, no locking */


	adb_request(&req, NULL, ADBREQ_SYNC, 1, ADB_FLUSH(id));
}

static void
init_ms_a3(int id)
{
	struct adb_request req;

	printk(" (Mouse Systems A3 Mouse, or compatible)");
	adb_request(&req, NULL, ADBREQ_SYNC, 3,
	ADB_WRITEREG(id, 0x2),
	    0x00,
	    0x07);

 	adb_request(&req, NULL, ADBREQ_SYNC, 1, ADB_FLUSH(id));
 }

