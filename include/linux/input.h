#ifndef _INPUT_H
#define _INPUT_H

/*
 * $Id: input.h,v 1.34 2001/05/28 09:06:44 vojtech Exp $
 *
 *  Copyright (c) 1999-2000 Vojtech Pavlik
 *
 *  Sponsored by SuSE
 */

/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * Should you need to contact me, the author, you can do so either by
 * e-mail - mail your message to <vojtech@suse.cz>, or by paper mail:
 * Vojtech Pavlik, Ucitelska 1576, Prague 8, 182 00 Czech Republic
 */

#ifdef __KERNEL__
#include <linux/time.h>
#else
#include <sys/time.h>
#include <sys/ioctl.h>
#include <asm/types.h>
#endif

/*
 * The event structure itself
 */

struct input_event {
	struct timeval time;
	unsigned short type;
	unsigned short code;
	unsigned int value;
};

/*
 * Protocol version.
 */

#define EV_VERSION		0x010000

/*
 * IOCTLs (0x00 - 0x7f)
 */

#define EVIOCGVERSION		_IOR('E', 0x01, int)			/* get driver version */
#define EVIOCGID		_IOR('E', 0x02, short[4])		/* get device ID */
#define EVIOCGREP		_IOR('E', 0x03, int[2])			/* get repeat settings */
#define EVIOCSREP		_IOW('E', 0x03, int[2])			/* get repeat settings */
#define EVIOCGKEYCODE		_IOR('E', 0x04, int[2])			/* get keycode */
#define EVIOCSKEYCODE		_IOW('E', 0x04, int[2])			/* set keycode */
#define EVIOCGKEY		_IOR('E', 0x05, int[2])			/* get key value */
#define EVIOCGNAME(len)		_IOC(_IOC_READ, 'E', 0x06, len)		/* get device name */
#define EVIOCGBUS		_IOR('E', 0x07, short[4])		/* get bus address */

#define EVIOCGBIT(ev,len)	_IOC(_IOC_READ, 'E', 0x20 + ev, len)	/* get event bits */
#define EVIOCGABS(abs)		_IOR('E', 0x40 + abs, int[5])		/* get abs value/limits */

#define EVIOCSFF		_IOC(_IOC_WRITE, 'E', 0x80, sizeof(struct ff_effect))	/* send a force effect to a force feedback device */
#define EVIOCRMFF		_IOW('E', 0x81, int)			/* Erase a force effect */
#define EVIOCSGAIN		_IOW('E', 0x82, unsigned short)		/* Set overall gain */
#define EVIOCSAUTOCENTER	_IOW('E', 0x83, unsigned short)		/* Enable or disable auto-centering */
#define EVIOCGEFFECTS		_IOR('E', 0x84, int)			/* Report number of effects playable at the same time */

/*
 * Event types
 */

#define EV_RST			0x00
#define EV_KEY			0x01
#define EV_REL			0x02
#define EV_ABS			0x03
#define EV_MSC			0x04
#define EV_LED			0x11
#define EV_SND			0x12
#define EV_REP			0x14
#define EV_FF			0x15
#define EV_MAX			0x1f

/*
 * Keys and buttons
 */

#define KEY_RESERVED		0
#define KEY_ESC			1
#define KEY_1			2
#define KEY_2			3
#define KEY_3			4
#define KEY_4			5
#define KEY_5			6
#define KEY_6			7
#define KEY_7			8
#define KEY_8			9
#define KEY_9			10
#define KEY_0			11
#define KEY_MINUS		12
#define KEY_EQUAL		13
#define KEY_BACKSPACE		14
#define KEY_TAB			15
#define KEY_Q			16
#define KEY_W			17
#define KEY_E			18
#define KEY_R			19
#define KEY_T			20
#define KEY_Y			21
#define KEY_U			22
#define KEY_I			23
#define KEY_O			24
#define KEY_P			25
#define KEY_LEFTBRACE		26
#define KEY_RIGHTBRACE		27
#define KEY_ENTER		28
#define KEY_LEFTCTRL		29
#define KEY_A			30
#define KEY_S			31
#define KEY_D			32
#define KEY_F			33
#define KEY_G			34
#define KEY_H			35
#define KEY_J			36
#define KEY_K			37
#define KEY_L			38
#define KEY_SEMICOLON		39
#define KEY_APOSTROPHE		40
#define KEY_GRAVE		41
#define KEY_LEFTSHIFT		42
#define KEY_BACKSLASH		43
#define KEY_Z			44
#define KEY_X			45
#define KEY_C			46
#define KEY_V			47
#define KEY_B			48
#define KEY_N			49
#define KEY_M			50
#define KEY_COMMA		51
#define KEY_DOT			52
#define KEY_SLASH		53
#define KEY_RIGHTSHIFT		54
#define KEY_KPASTERISK		55
#define KEY_LEFTALT		56
#define KEY_SPACE		57
#define KEY_CAPSLOCK		58
#define KEY_F1			59
#define KEY_F2			60
#define KEY_F3			61
#define KEY_F4			62
#define KEY_F5			63
#define KEY_F6			64
#define KEY_F7			65
#define KEY_F8			66
#define KEY_F9			67
#define KEY_F10			68
#define KEY_NUMLOCK		69
#define KEY_SCROLLLOCK		70
#define KEY_KP7			71
#define KEY_KP8			72
#define KEY_KP9			73
#define KEY_KPMINUS		74
#define KEY_KP4			75
#define KEY_KP5			76
#define KEY_KP6			77
#define KEY_KPPLUS		78
#define KEY_KP1			79
#define KEY_KP2			80
#define KEY_KP3			81
#define KEY_KP0			82
#define KEY_KPDOT		83
#define KEY_103RD		84
#define KEY_F13			85
#define KEY_102ND		86
#define KEY_F11			87
#define KEY_F12			88
#define KEY_F14			89
#define KEY_F15			90
#define KEY_F16			91
#define KEY_F17			92
#define KEY_F18			93
#define KEY_F19			94
#define KEY_F20			95
#define KEY_KPENTER		96
#define KEY_RIGHTCTRL		97
#define KEY_KPSLASH		98
#define KEY_SYSRQ		99
#define KEY_RIGHTALT		100
#define KEY_LINEFEED		101
#define KEY_HOME		102
#define KEY_UP			103
#define KEY_PAGEUP		104
#define KEY_LEFT		105
#define KEY_RIGHT		106
#define KEY_END			107
#define KEY_DOWN		108
#define KEY_PAGEDOWN		109
#define KEY_INSERT		110
#define KEY_DELETE		111
#define KEY_MACRO		112
#define KEY_MUTE		113
#define KEY_VOLUMEDOWN		114
#define KEY_VOLUMEUP		115
#define KEY_POWER		116
#define KEY_KPEQUAL		117
#define KEY_KPPLUSMINUS		118
#define KEY_PAUSE		119
#define KEY_F21			120
#define KEY_F22			121
#define KEY_F23			122
#define KEY_F24			123
#define KEY_KPCOMMA		124
#define KEY_LEFTMETA		125
#define KEY_RIGHTMETA		126
#define KEY_COMPOSE		127

#define KEY_STOP		128
#define KEY_AGAIN		129
#define KEY_PROPS		130
#define KEY_UNDO		131
#define KEY_FRONT		132
#define KEY_COPY		133
#define KEY_OPEN		134
#define KEY_PASTE		135
#define KEY_FIND		136
#define KEY_CUT			137
#define KEY_HELP		138
#define KEY_MENU		139
#define KEY_CALC		140
#define KEY_SETUP		141
#define KEY_SLEEP		142
#define KEY_WAKEUP		143
#define KEY_FILE		144
#define KEY_SENDFILE		145
#define KEY_DELETEFILE		146
#define KEY_XFER		147
#define KEY_PROG1		148
#define KEY_PROG2		149
#define KEY_WWW			150
#define KEY_MSDOS		151
#define KEY_COFFEE		152
#define KEY_DIRECTION		153
#define KEY_CYCLEWINDOWS	154
#define KEY_MAIL		155
#define KEY_BOOKMARKS		156
#define KEY_COMPUTER		157
#define KEY_BACK		158
#define KEY_FORWARD		159
#define KEY_CLOSECD		160
#define KEY_EJECTCD		161
#define KEY_EJECTCLOSECD	162
#define KEY_NEXTSONG		163
#define KEY_PLAYPAUSE		164
#define KEY_PREVIOUSSONG	165
#define KEY_STOPCD		166
#define KEY_RECORD		167
#define KEY_REWIND		168
#define KEY_PHONE		169
#define KEY_ISO			170
#define KEY_CONFIG		171
#define KEY_HOMEPAGE		172
#define KEY_REFRESH		173
#define KEY_EXIT		174
#define KEY_MOVE		175
#define KEY_EDIT		176
#define KEY_SCROLLUP		177
#define KEY_SCROLLDOWN		178
#define KEY_KPLEFTPAREN		179
#define KEY_KPRIGHTPAREN	180

#define KEY_INTL1		181
#define KEY_INTL2		182
#define KEY_INTL3		183
#define KEY_INTL4		184
#define KEY_INTL5		185
#define KEY_INTL6		186
#define KEY_INTL7		187
#define KEY_INTL8		188
#define KEY_INTL9		189
#define KEY_LANG1		190
#define KEY_LANG2		191
#define KEY_LANG3		192
#define KEY_LANG4		193
#define KEY_LANG5		194
#define KEY_LANG6		195
#define KEY_LANG7		196
#define KEY_LANG8		197
#define KEY_LANG9		198

#define KEY_PLAYCD		200
#define KEY_PAUSECD		201
#define KEY_PROG3		202
#define KEY_PROG4		203
#define KEY_SUSPEND		205
#define KEY_CLOSE		206

#define KEY_UNKNOWN		220

#define KEY_BRIGHTNESSDOWN	224
#define KEY_BRIGHTNESSUP	225

#define BTN_MISC		0x100
#define BTN_0			0x100
#define BTN_1			0x101
#define BTN_2			0x102
#define BTN_3			0x103
#define BTN_4			0x104
#define BTN_5			0x105
#define BTN_6			0x106
#define BTN_7			0x107
#define BTN_8			0x108
#define BTN_9			0x109

#define BTN_MOUSE		0x110
#define BTN_LEFT		0x110
#define BTN_RIGHT		0x111
#define BTN_MIDDLE		0x112
#define BTN_SIDE		0x113
#define BTN_EXTRA		0x114
#define BTN_FORWARD		0x115
#define BTN_BACK		0x116

#define BTN_JOYSTICK		0x120
#define BTN_TRIGGER		0x120
#define BTN_THUMB		0x121
#define BTN_THUMB2		0x122
#define BTN_TOP			0x123
#define BTN_TOP2		0x124
#define BTN_PINKIE		0x125
#define BTN_BASE		0x126
#define BTN_BASE2		0x127
#define BTN_BASE3		0x128
#define BTN_BASE4		0x129
#define BTN_BASE5		0x12a
#define BTN_BASE6		0x12b
#define BTN_DEAD		0x12f

#define BTN_GAMEPAD		0x130
#define BTN_A			0x130
#define BTN_B			0x131
#define BTN_C			0x132
#define BTN_X			0x133
#define BTN_Y			0x134
#define BTN_Z			0x135
#define BTN_TL			0x136
#define BTN_TR			0x137
#define BTN_TL2			0x138
#define BTN_TR2			0x139
#define BTN_SELECT		0x13a
#define BTN_START		0x13b
#define BTN_MODE		0x13c
#define BTN_THUMBL		0x13d
#define BTN_THUMBR		0x13e

#define BTN_DIGI		0x140
#define BTN_TOOL_PEN		0x140
#define BTN_TOOL_RUBBER		0x141
#define BTN_TOOL_BRUSH		0x142
#define BTN_TOOL_PENCIL		0x143
#define BTN_TOOL_AIRBRUSH	0x144
#define BTN_TOOL_FINGER		0x145
#define BTN_TOOL_MOUSE		0x146
#define BTN_TOOL_LENS		0x147
#define BTN_TOUCH		0x14a
#define BTN_STYLUS		0x14b
#define BTN_STYLUS2		0x14c

#define KEY_MAX			0x1ff

/*
 * Relative axes
 */

#define REL_X			0x00
#define REL_Y			0x01
#define REL_Z			0x02
#define REL_HWHEEL		0x06
#define REL_DIAL		0x07
#define REL_WHEEL		0x08
#define REL_MISC		0x09
#define REL_MAX			0x0f

/*
 * Absolute axes
 */

#define ABS_X			0x00
#define ABS_Y			0x01
#define ABS_Z			0x02
#define ABS_RX			0x03
#define ABS_RY			0x04
#define ABS_RZ			0x05
#define ABS_THROTTLE		0x06
#define ABS_RUDDER		0x07
#define ABS_WHEEL		0x08
#define ABS_GAS			0x09
#define ABS_BRAKE		0x0a
#define ABS_HAT0X		0x10
#define ABS_HAT0Y		0x11
#define ABS_HAT1X		0x12
#define ABS_HAT1Y		0x13
#define ABS_HAT2X		0x14
#define ABS_HAT2Y		0x15
#define ABS_HAT3X		0x16
#define ABS_HAT3Y		0x17
#define ABS_PRESSURE		0x18
#define ABS_DISTANCE		0x19
#define ABS_TILT_X		0x1a
#define ABS_TILT_Y		0x1b
#define ABS_MISC		0x1c
#define ABS_MAX			0x1f

/*
 * Misc events
 */

#define MSC_SERIAL		0x00
#define MSC_PULSELED		0x01
#define MSC_MAX			0x07

/*
 * LEDs
 */

#define LED_NUML		0x00
#define LED_CAPSL		0x01
#define LED_SCROLLL		0x02
#define LED_COMPOSE		0x03
#define LED_KANA		0x04
#define LED_SLEEP		0x05
#define LED_SUSPEND		0x06
#define LED_MUTE		0x07
#define LED_MISC		0x08
#define LED_MAX			0x0f

/*
 * Autorepeat values
 */

#define REP_DELAY		0x00
#define REP_PERIOD		0x01
#define REP_MAX			0x01

/*
 * Sounds
 */

#define SND_CLICK		0x00
#define SND_BELL		0x01
#define SND_MAX			0x07

/*
 * IDs.
 */

#define ID_BUS			0
#define ID_VENDOR		1
#define ID_PRODUCT		2
#define ID_VERSION		3

#define BUS_PCI			0x01
#define BUS_ISAPNP		0x02
#define BUS_USB			0x03
#define BUS_HIL			0x04
#define BUS_BLUETOOTH		0x05

#define BUS_ISA			0x10
#define BUS_I8042		0x11
#define BUS_XTKBD		0x12
#define BUS_RS232		0x13
#define BUS_GAMEPORT		0x14
#define BUS_PARPORT		0x15
#define BUS_AMIGA		0x16
#define BUS_ADB			0x17
#define BUS_I2C			0x18

/*
 * Structures used in ioctls to upload effects to a device
 * The first structures are not passed directly by using ioctls.
 * They are sub-structures of the actually sent structure (called ff_effect)
 */

struct ff_replay {
	__u16 length;		/* Duration of an effect */
	__u16 delay;		/* Time to wait before to start playing an effect */
};

struct ff_trigger {
	__u16 button;		/* Number of button triggering an effect */
	__u16 interval;		/* Time to wait before an effect can be re-triggered */
};

struct ff_shape {
	__u16 attack_length;	/* Duration of attack */
	__s16 attack_level;	/* Level at beginning of attack */
	__u16 fade_length;	/* Duration of fade */
	__s16 fade_level;	/* Level at end of fade */
};

/* FF_CONSTANT */
struct ff_constant_effect {
	__s16 level;		/* Strength of effect */
	__u16 direction;	/* Direction of effect (see periodic effects) */
	struct ff_shape shape;
};

/* FF_SPRING of FF_FRICTION */
struct ff_interactive_effect {
/* Axis along which effect must be created. If null, the field named direction
 * is used
 * It is a bit array (ie to enable axes X and Y, use BIT(ABS_X) | BIT(ABS_Y)
 */
	__u16 axis;
	__u16 direction;

	__s16 right_saturation; /* Max level when joystick is on the right */
	__s16 left_saturation;  /* Max level when joystick in on the left */

	__s16 right_coeff;	/* Indicates how fast the force grows when the
				   joystick moves to the right */
	__s16 left_coeff;	/* Same for left side */

	__u16 deadband;		/* Size of area where no force is produced */
	__s16 center;		/* Position of dead dead zone */

};

/* FF_PERIODIC */
struct ff_periodic_effect {
	__u16 waveform;		/* Kind of wave (sine, square...) */
	__u16 period;
	__s16 magnitude;	/* Peak value */
	__s16 offset;		/* Mean value of wave (roughly) */
	__u16 phase;		/* 'Horizontal' shift */
	__u16 direction;	/* Direction. 0 deg -> 0x0000
					     90 deg -> 0x4000 */

	struct ff_shape shape;
};

/*
 * Structure sent through ioctl from the application to the driver
 */
struct ff_effect {
	__u16 type;
/* Following field denotes the unique id assigned to an effect.
 * It is set by the driver.
 */
	__s16 id;

	struct ff_trigger trigger;
	struct ff_replay replay;

	union {
		struct ff_constant_effect constant;
		struct ff_periodic_effect periodic;
		struct ff_interactive_effect interactive;
	} u;
};

/*
 * Buttons that can trigger effects.  Use for example FF_BTN(BTN_TRIGGER) to
 * access the bitmap.
 */

#define FF_BTN(x)	((x) - BTN_MISC + FF_BTN_OFFSET)
#define FF_BTN_OFFSET	0x00

/*
 * Force feedback axis mappings. Use FF_ABS() to access the bitmap.
 */

#define FF_ABS(x)	((x) + FF_ABS_OFFSET)
#define FF_ABS_OFFSET	0x40

/*
 * Force feedback effect types
 */

#define FF_RUMBLE	0x50
#define FF_PERIODIC	0x51
#define FF_CONSTANT	0x52
#define FF_SPRING	0x53
#define FF_FRICTION	0x54

/*
 * Force feedback periodic effect types
 */

#define FF_SQUARE	0x58
#define FF_TRIANGLE	0x59
#define FF_SINE		0x5a
#define FF_SAW_UP	0x5b
#define FF_SAW_DOWN	0x5c
#define FF_CUSTOM	0x5d

/*
 * Set ff device properties
 */

#define FF_GAIN		0x60
#define FF_AUTOCENTER	0x61

#define FF_MAX		0x7f

#ifdef __KERNEL__

/*
 * In-kernel definitions.
 */

#include <linux/sched.h>
#include <linux/devfs_fs_kernel.h>

#define NBITS(x) ((((x)-1)/BITS_PER_LONG)+1)
#define BIT(x)	(1UL<<((x)%BITS_PER_LONG))
#define LONG(x) ((x)/BITS_PER_LONG)

struct input_dev {

	void *private;

	int number;
	char *name;
	unsigned short idbus;
	unsigned short idvendor;
	unsigned short idproduct;
	unsigned short idversion;

	unsigned long evbit[NBITS(EV_MAX)];
	unsigned long keybit[NBITS(KEY_MAX)];
	unsigned long relbit[NBITS(REL_MAX)];
	unsigned long absbit[NBITS(ABS_MAX)];
	unsigned long mscbit[NBITS(MSC_MAX)];
	unsigned long ledbit[NBITS(LED_MAX)];
	unsigned long sndbit[NBITS(SND_MAX)];
	unsigned long ffbit[NBITS(FF_MAX)];
	int ff_effects_max;

	unsigned int keycodemax;
	unsigned int keycodesize;
	void *keycode;

	unsigned int repeat_key;
	struct timer_list timer;

	int abs[ABS_MAX + 1];
	int rep[REP_MAX + 1];

	unsigned long key[NBITS(KEY_MAX)];
	unsigned long led[NBITS(LED_MAX)];
	unsigned long snd[NBITS(SND_MAX)];

	int absmax[ABS_MAX + 1];
	int absmin[ABS_MAX + 1];
	int absfuzz[ABS_MAX + 1];
	int absflat[ABS_MAX + 1];

	int (*open)(struct input_dev *dev);
	void (*close)(struct input_dev *dev);
	int (*event)(struct input_dev *dev, unsigned int type, unsigned int code, int value);
	int (*upload_effect)(struct input_dev *dev, struct ff_effect *effect);
	int (*erase_effect)(struct input_dev *dev, int effect_id);

	struct input_handle *handle;
	struct input_dev *next;
};

struct input_handler {

	void *private;

	void (*event)(struct input_handle *handle, unsigned int type, unsigned int code, int value);
	struct input_handle* (*connect)(struct input_handler *handler, struct input_dev *dev);
	void (*disconnect)(struct input_handle *handle);

	struct file_operations *fops;
	int minor;

	struct input_handle *handle;
	struct input_handler *next;
};

struct input_handle {

	void *private;

	int open;

	struct input_dev *dev;
	struct input_handler *handler;

	struct input_handle *dnext;
	struct input_handle *hnext;
};

void input_register_device(struct input_dev *);
void input_unregister_device(struct input_dev *);

void input_register_handler(struct input_handler *);
void input_unregister_handler(struct input_handler *);

int input_open_device(struct input_handle *);
void input_close_device(struct input_handle *);

devfs_handle_t input_register_minor(char *name, int minor, int minor_base);
void input_unregister_minor(devfs_handle_t handle);

void input_event(struct input_dev *dev, unsigned int type, unsigned int code, int value);

#define input_report_key(a,b,c) input_event(a, EV_KEY, b, !!(c))
#define input_report_rel(a,b,c) input_event(a, EV_REL, b, c)
#define input_report_abs(a,b,c) input_event(a, EV_ABS, b, c)

#endif
#endif
