/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 1997-2000 Kazutaka YOKOTA <yokota@FreeBSD.org>
 * Copyright (c) 2004-2008 Philip Paeps <philip@FreeBSD.org>
 * Copyright (c) 2008 Jean-Sebastien Pedron <dumbbell@FreeBSD.org>
 * Copyright (c) 2021,2024 Vladimir Kondratyev <wulf@FreeBSD.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * MOUSED.C
 *
 * Mouse daemon : listens to a evdev device node for mouse data stream,
 * interprets data and passes ioctls off to the console driver.
 *
 */

#include <sys/param.h>
#include <sys/consio.h>
#include <sys/event.h>
#include <sys/mouse.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>

#include <dev/evdev/input.h>

#include <bitstring.h>
#include <ctype.h>
#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <libutil.h>
#include <math.h>
#include <setjmp.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include "util.h"
#include "quirks.h"

/*
 * bitstr_t implementation must be identical to one found in EVIOCG*
 * libevdev ioctls. Our bitstring(3) API is compatible since r299090.
 */
_Static_assert(sizeof(bitstr_t) == sizeof(unsigned long),
    "bitstr_t size mismatch");

#define MAX_CLICKTHRESHOLD	2000	/* 2 seconds */
#define MAX_BUTTON2TIMEOUT	2000	/* 2 seconds */
#define DFLT_CLICKTHRESHOLD	 500	/* 0.5 second */
#define DFLT_BUTTON2TIMEOUT	 100	/* 0.1 second */
#define DFLT_SCROLLTHRESHOLD	   3	/* 3 pixels */
#define DFLT_SCROLLSPEED	   2	/* 2 pixels */
#define	DFLT_MOUSE_RESOLUTION	   8	/* dpmm, == 200dpi */
#define	DFLT_TPAD_RESOLUTION	  40	/* dpmm, typical X res for Synaptics */
#define	DFLT_LINEHEIGHT		  10	/* pixels per line */

/* Abort 3-button emulation delay after this many movement events. */
#define BUTTON2_MAXMOVE	3

#define MOUSE_XAXIS	(-1)
#define MOUSE_YAXIS	(-2)

#define	ZMAP_MAXBUTTON	4	/* Number of zmap items */
#define	MAX_FINGERS	10

#define ID_NONE		0
#define ID_PORT		1
#define ID_IF		2
#define ID_TYPE		4
#define ID_MODEL	8
#define ID_ALL		(ID_PORT | ID_IF | ID_TYPE | ID_MODEL)

/* Operations on timespecs */
#define	tsclr(tvp)		timespecclear(tvp)
#define	tscmp(tvp, uvp, cmp)	timespeccmp(tvp, uvp, cmp)
#define	tssub(tvp, uvp, vvp)	timespecsub(tvp, uvp, vvp)
#define	msec2ts(msec)	(struct timespec) {			\
	.tv_sec = (msec) / 1000,				\
	.tv_nsec = (msec) % 1000 * 1000000,			\
}
static inline struct timespec
tsaddms(struct timespec* tsp, u_int ms)
{
	struct timespec ret;

	ret = msec2ts(ms);
	timespecadd(tsp, &ret, &ret);

	return (ret);
};

static inline struct timespec
tssubms(struct timespec* tsp, u_int ms)
{
	struct timespec ret;

	ret = msec2ts(ms);
	timespecsub(tsp, &ret, &ret);

	return (ret);
};

#define debug(...) do {						\
	if (debug && nodaemon)					\
		warnx(__VA_ARGS__);				\
} while (0)

#define logerr(e, ...) do {					\
	log_or_warn(LOG_DAEMON | LOG_ERR, errno, __VA_ARGS__);	\
	exit(e);						\
} while (0)

#define logerrx(e, ...) do {					\
	log_or_warn(LOG_DAEMON | LOG_ERR, 0, __VA_ARGS__);	\
	exit(e);						\
} while (0)

#define logwarn(...)						\
	log_or_warn(LOG_DAEMON | LOG_WARNING, errno, __VA_ARGS__)

#define logwarnx(...)						\
	log_or_warn(LOG_DAEMON | LOG_WARNING, 0, __VA_ARGS__)

/* structures */

enum gesture {
	GEST_IGNORE,
	GEST_ACCUMULATE,
	GEST_MOVE,
	GEST_VSCROLL,
	GEST_HSCROLL,
};

/* interfaces (the table must be ordered by DEVICE_IF_XXX in util.h) */
static const struct {
	const char *name;
	size_t p_size;
} rifs[] = {
	[DEVICE_IF_EVDEV]	= { "evdev",	sizeof(struct input_event) },
	[DEVICE_IF_SYSMOUSE]	= { "sysmouse",	MOUSE_SYS_PACKETSIZE },
};

/* types (the table must be ordered by DEVICE_TYPE_XXX in util.h) */
static const char *rnames[] = {
	[DEVICE_TYPE_MOUSE]		= "mouse",
	[DEVICE_TYPE_POINTINGSTICK]	= "pointing stick",
	[DEVICE_TYPE_TOUCHPAD]		= "touchpad",
	[DEVICE_TYPE_TOUCHSCREEN]	= "touchscreen",
	[DEVICE_TYPE_TABLET]		= "tablet",
	[DEVICE_TYPE_TABLET_PAD]	= "tablet pad",
	[DEVICE_TYPE_KEYBOARD]		= "keyboard",
	[DEVICE_TYPE_JOYSTICK]		= "joystick",
};

/* Default phisical to logical button mapping */
static const u_int default_p2l[MOUSE_MAXBUTTON] = {
    MOUSE_BUTTON1DOWN, MOUSE_BUTTON2DOWN, MOUSE_BUTTON3DOWN, MOUSE_BUTTON4DOWN,
    MOUSE_BUTTON5DOWN, MOUSE_BUTTON6DOWN, MOUSE_BUTTON7DOWN, MOUSE_BUTTON8DOWN,
    0x00000100,        0x00000200,        0x00000400,        0x00000800,
    0x00001000,        0x00002000,        0x00004000,        0x00008000,
    0x00010000,        0x00020000,        0x00040000,        0x00080000,
    0x00100000,        0x00200000,        0x00400000,        0x00800000,
    0x01000000,        0x02000000,        0x04000000,        0x08000000,
    0x10000000,        0x20000000,        0x40000000,
};

struct tpcaps {
	bool	is_clickpad;
	bool	is_topbuttonpad;
	bool	is_mt;
	bool	cap_touch;
	bool	cap_pressure;
	bool	cap_width;
	int	min_x;
	int	max_x;
	int	min_y;
	int	max_y;
	int	res_x;	/* dots per mm */
	int	res_y;	/* dots per mm */
	int	min_p;
	int	max_p;
};

struct tpinfo {
	bool	two_finger_scroll;	/* Enable two finger scrolling */
	bool	natural_scroll;		/* Enable natural scrolling */
	bool	three_finger_drag;	/* Enable dragging with three fingers */
	u_int	min_pressure_hi;	/* Min pressure to start an action */
	u_int	min_pressure_lo;	/* Min pressure to continue an action */
	u_int	max_pressure;		/* Maximum pressure to detect palm */
	u_int	max_width;		/* Max finger width to detect palm */
	int	margin_top;		/* Top margin */
	int	margin_right;		/* Right margin */
	int	margin_bottom;		/* Bottom margin */
	int	margin_left;		/* Left margin */
	u_int	tap_timeout;		/* */
	u_int	tap_threshold;		/* Minimum pressure to detect a tap */
	double	tap_max_delta;		/* Length of segments above which a tap is ignored */
	u_int	taphold_timeout;	/* Maximum elapsed time between two taps to consider a tap-hold action */
	double	vscroll_ver_area;	/* Area reserved for vertical virtual scrolling */
	double	vscroll_hor_area;	/* Area reserved for horizontal virtual scrolling */
	double	vscroll_min_delta;	/* Minimum movement to consider virtual scrolling */
	int	softbuttons_y;		/* Vertical size of softbuttons area */
	int	softbutton2_x;		/* Horizontal offset of 2-nd softbutton left edge */
	int	softbutton3_x;		/* Horizontal offset of 3-rd softbutton left edge */
};

struct tpstate {
	int 		start_x;
	int 		start_y;
	int 		prev_x;
	int 		prev_y;
	int		prev_nfingers;
	int		fingers_nb;
	int		tap_button;
	bool		fingerdown;
	bool		in_taphold;
	int		in_vscroll;
	u_int		zmax;           /* maximum pressure value */
	struct timespec	taptimeout;     /* tap timeout for touchpads */
	int		idletimeout;
	bool		timer_armed;
};

struct tpad {
	struct tpcaps	hw;	/* touchpad capabilities */
	struct tpinfo	info;	/* touchpad gesture parameters */
	struct tpstate	gest;	/* touchpad gesture state */
};

struct finger {
	int	x;
	int	y;
	int	p;
	int	w;
	int	id;	/* id=0 - no touch, id>1 - touch id */
};

struct evstate {
	int		buttons;
	/* Relative */
	int		dx;
	int		dy;
	int		dz;
	int		dw;
	int		acc_dx;
	int		acc_dy;
	/* Absolute single-touch */
	int		nfingers;
	struct finger	st;
	/* Absolute multi-touch */
	int		slot;
	struct finger	mt[MAX_FINGERS];
	bitstr_t bit_decl(key_ignore, KEY_CNT);
	bitstr_t bit_decl(rel_ignore, REL_CNT);
	bitstr_t bit_decl(abs_ignore, ABS_CNT);
	bitstr_t bit_decl(prop_ignore, INPUT_PROP_CNT);
};

/* button status */
struct button_state {
	int count;	/* 0: up, 1: single click, 2: double click,... */
	struct timespec ts;	/* timestamp on the last button event */
};

struct btstate {
	u_int	wmode;		/* wheel mode button number */
	u_int 	clickthreshold;	/* double click speed in msec */
	struct button_state	bstate[MOUSE_MAXBUTTON]; /* button state */
	struct button_state	*mstate[MOUSE_MAXBUTTON];/* mapped button st.*/
	u_int	p2l[MOUSE_MAXBUTTON];/* phisical to logical button mapping */
	int	zmap[ZMAP_MAXBUTTON];/* MOUSE_{X|Y}AXIS or a button number */
	struct button_state	zstate[ZMAP_MAXBUTTON];	 /* Z/W axis state */
};

/* state machine for 3 button emulation */

enum bt3_emul_state {
	S0,		/* start */
	S1,		/* button 1 delayed down */
	S2,		/* button 3 delayed down */
	S3,		/* both buttons down -> button 2 down */
	S4,		/* button 1 delayed up */
	S5,		/* button 1 down */
	S6,		/* button 3 down */
	S7,		/* both buttons down */
	S8,		/* button 3 delayed up */
	S9,		/* button 1 or 3 up after S3 */
};

#define A(b1, b3)	(((b1) ? 2 : 0) | ((b3) ? 1 : 0))
#define A_TIMEOUT	4
#define S_DELAYED(st)	(states[st].s[A_TIMEOUT] != (st))

static const struct {
	enum bt3_emul_state s[A_TIMEOUT + 1];
	int buttons;
	int mask;
	bool timeout;
} states[10] = {
    /* S0 */
    { { S0, S2, S1, S3, S0 }, 0, ~(MOUSE_BUTTON1DOWN | MOUSE_BUTTON3DOWN), false },
    /* S1 */
    { { S4, S2, S1, S3, S5 }, 0, ~MOUSE_BUTTON1DOWN, false },
    /* S2 */
    { { S8, S2, S1, S3, S6 }, 0, ~MOUSE_BUTTON3DOWN, false },
    /* S3 */
    { { S0, S9, S9, S3, S3 }, MOUSE_BUTTON2DOWN, ~0, false },
    /* S4 */
    { { S0, S2, S1, S3, S0 }, MOUSE_BUTTON1DOWN, ~0, true },
    /* S5 */
    { { S0, S2, S5, S7, S5 }, MOUSE_BUTTON1DOWN, ~0, false },
    /* S6 */
    { { S0, S6, S1, S7, S6 }, MOUSE_BUTTON3DOWN, ~0, false },
    /* S7 */
    { { S0, S6, S5, S7, S7 }, MOUSE_BUTTON1DOWN | MOUSE_BUTTON3DOWN, ~0, false },
    /* S8 */
    { { S0, S2, S1, S3, S0 }, MOUSE_BUTTON3DOWN, ~0, true },
    /* S9 */
    { { S0, S9, S9, S3, S9 }, 0, ~(MOUSE_BUTTON1DOWN | MOUSE_BUTTON3DOWN), false },
};

struct e3bstate {
	bool enabled;
	u_int button2timeout;	/* 3 button emulation timeout */
	enum bt3_emul_state	mouse_button_state;
	struct timespec		mouse_button_state_ts;
	int			mouse_move_delayed;
	bool timer_armed;
};

enum scroll_state {
	SCROLL_NOTSCROLLING,
	SCROLL_PREPARE,
	SCROLL_SCROLLING,
};

struct scroll {
	bool	enable_vert;
	bool	enable_hor;
	u_int	threshold;	/* Movement distance before virtual scrolling */
	u_int	speed;		/* Movement distance to rate of scrolling */
	enum scroll_state state;
	int	movement;
	int	hmovement;
};

struct drift_xy {
	int x;
	int y;
};
struct drift {
	u_int		distance;	/* max steps X+Y */
	u_int		time;		/* ms */
	struct timespec	time_ts;
	struct timespec	twotime_ts;	/* 2*drift_time */
	u_int		after;		/* ms */
	struct timespec	after_ts;
	bool		terminate;
	struct timespec	current_ts;
	struct timespec	last_activity;
	struct timespec	since;
	struct drift_xy	last;		/* steps in last drift_time */
	struct drift_xy	previous;	/* steps in prev. drift_time */
};

struct accel {
	bool is_exponential;	/* Exponential acceleration is enabled */
	double accelx;		/* Acceleration in the X axis */
	double accely;		/* Acceleration in the Y axis */
	double accelz;		/* Acceleration in the wheel axis */
	double expoaccel;	/* Exponential acceleration */
	double expoffset;	/* Movement offset for exponential accel. */
	double remainx;		/* Remainder on X, Y and wheel axis, ... */
	double remainy;		/*    ...  respectively to compensate */
	double remainz;		/*    ... for rounding errors. */
	double lastlength[3];
};

struct rodent {
	struct device dev;	/* Device */
	int mfd;		/* mouse file descriptor */
	struct btstate btstate;	/* button status */
	struct e3bstate e3b;	/* 3 button emulation state */
	struct drift drift;
	struct accel accel;	/* cursor acceleration state */
	struct scroll scroll;	/* virtual scroll state */
	struct tpad tp;		/* touchpad info and gesture state */
	struct evstate ev;	/* event device state */
	SLIST_ENTRY(rodent) next;
};

/* global variables */

static SLIST_HEAD(rodent_list, rodent) rodents = SLIST_HEAD_INITIALIZER();

static int	debug = 0;
static bool	nodaemon = false;
static bool	background = false;
static bool	paused = false;
static bool	opt_grab = false;
static int	identify = ID_NONE;
static int	cfd = -1;	/* /dev/consolectl file descriptor */
static int	kfd = -1;	/* kqueue file descriptor */
static int	dfd = -1;	/* devd socket descriptor */
static const char *portname = NULL;
static const char *pidfile = "/var/run/moused.pid";
static struct pidfh *pfh;
#ifndef CONFDIR
#define CONFDIR	"/etc"
#endif
static const char *config_file = CONFDIR "/moused.conf";
#ifndef QUIRKSDIR
#define	QUIRKSDIR	"/usr/share/moused"
#endif
static const char *quirks_path = QUIRKSDIR;
static struct quirks_context *quirks;
static enum device_if force_if = DEVICE_IF_UNKNOWN;

static int	opt_rate = 0;
static int	opt_resolution = MOUSE_RES_UNKNOWN;

static u_int	opt_wmode = 0;
static int	opt_clickthreshold = -1;
static bool	opt_e3b_enabled = false;
static int	opt_e3b_button2timeout = -1;
static struct btstate opt_btstate;

static bool	opt_drift_terminate = false;
static u_int	opt_drift_distance = 4;		/* max steps X+Y */
static u_int	opt_drift_time = 500;		/* ms */
static u_int	opt_drift_after = 4000;		/* ms */

static double	opt_accelx = 1.0;
static double	opt_accely = 1.0;
static bool	opt_exp_accel = false;
static double	opt_expoaccel = 1.0;
static double	opt_expoffset = 1.0;

static bool	opt_virtual_scroll = false;
static bool	opt_hvirtual_scroll = false;
static int	opt_scroll_speed = -1;
static int	opt_scroll_threshold = -1;

static jmp_buf env;

/* function prototypes */

static moused_log_handler	log_or_warn_va;

static void	linacc(struct accel *, int, int, int, int*, int*, int*);
static void	expoacc(struct accel *, int, int, int, int*, int*, int*);
static void	moused(void);
static void	reset(int sig);
static void	pause_mouse(int sig);
static int	connect_devd(void);
static void	fetch_and_parse_devd(void);
static void	usage(void);
static void	log_or_warn(int log_pri, int errnum, const char *fmt, ...)
		    __printflike(3, 4);

static int	r_daemon(void);
static enum device_if	r_identify_if(int fd);
static enum device_type	r_identify_evdev(int fd);
static enum device_type	r_identify_sysmouse(int fd);
static const char *r_if(enum device_if type);
static const char *r_name(enum device_type type);
static struct rodent *r_init(const char *path);
static void	r_init_all(void);
static void	r_deinit(struct rodent *r);
static void	r_deinit_all(void);
static int	r_protocol_evdev(enum device_type type, struct tpad *tp,
		    struct evstate *ev, struct input_event *ie,
		    mousestatus_t *act);
static int	r_protocol_sysmouse(uint8_t *pBuf, mousestatus_t *act);
static void	r_vscroll_detect(struct rodent *r, struct scroll *sc,
		    mousestatus_t *act);
static void	r_vscroll(struct scroll *sc, mousestatus_t *act);
static int	r_statetrans(struct rodent *r, mousestatus_t *a1,
		    mousestatus_t *a2, int trans);
static bool	r_installmap(char *arg, struct btstate *bt);
static char *	r_installzmap(char **argv, int argc, int* idx, struct btstate *bt);
static void	r_map(mousestatus_t *act1, mousestatus_t *act2,
		    struct btstate *bt);
static void	r_timestamp(mousestatus_t *act, struct btstate *bt,
		    struct e3bstate *e3b, struct drift *drift);
static bool	r_timeout(struct e3bstate *e3b);
static void	r_move(mousestatus_t *act, struct accel *acc);
static void	r_click(mousestatus_t *act, struct btstate *bt);
static bool	r_drift(struct drift *, mousestatus_t *);
static enum gesture r_gestures(struct tpad *tp, int x0, int y0, u_int z, int w,
		    int nfingers, struct timespec *time, mousestatus_t *ms);

int
main(int argc, char *argv[])
{
	struct rodent *r;
	pid_t mpid;
	int c;
	u_int i;
	int n;
	u_long ul;
	char *errstr;

	while ((c = getopt(argc, argv, "3A:C:E:F:HI:L:T:VU:a:dfghi:l:m:p:r:t:q:w:z:")) != -1) {
		switch(c) {

		case '3':
			opt_e3b_enabled = true;
			break;

		case 'E':
			errno = 0;
			ul = strtoul(optarg, NULL, 10);
			if ((ul == 0 && errno != 0) ||
			     ul > MAX_BUTTON2TIMEOUT) {
				warnx("invalid argument `%s'", optarg);
				usage();
			}
			opt_e3b_button2timeout = ul;
			break;

		case 'a':
			n = sscanf(optarg, "%lf,%lf", &opt_accelx, &opt_accely);
			if (n == 0) {
				warnx("invalid linear acceleration argument "
				    "'%s'", optarg);
				usage();
			}
			if (n == 1)
				opt_accely = opt_accelx;
			break;

		case 'A':
			opt_exp_accel = true;
			n = sscanf(optarg, "%lf,%lf", &opt_expoaccel,
			    &opt_expoffset);
			if (n == 0) {
				warnx("invalid exponential acceleration "
				    "argument '%s'", optarg);
				usage();
			}
			if (n == 1)
				opt_expoffset = 1.0;
			break;

		case 'd':
			++debug;
			break;

		case 'f':
			nodaemon = true;
			break;

		case 'g':
			opt_grab = true;
			break;

		case 'i':
			if (strcmp(optarg, "all") == 0)
				identify = ID_ALL;
			else if (strcmp(optarg, "port") == 0)
				identify = ID_PORT;
			else if (strcmp(optarg, "if") == 0)
				identify = ID_IF;
			else if (strcmp(optarg, "type") == 0)
				identify = ID_TYPE;
			else if (strcmp(optarg, "model") == 0)
				identify = ID_MODEL;
			else {
				warnx("invalid argument `%s'", optarg);
				usage();
			}
			nodaemon = true;
			break;

		case 'l':
			ul = strtoul(optarg, NULL, 10);
			if (ul != 1)
				warnx("ignore mouse level `%s'", optarg);
			break;

		case 'm':
			if (!r_installmap(optarg, &opt_btstate)) {
				warnx("invalid argument `%s'", optarg);
				usage();
			}
			break;

		case 'p':
			/* "auto" is an alias to no portname */
			if (strcmp(optarg, "auto") != 0)
				portname = optarg;
			break;

		case 'r':
			if (strcmp(optarg, "high") == 0)
				opt_resolution = MOUSE_RES_HIGH;
			else if (strcmp(optarg, "medium-high") == 0)
				opt_resolution = MOUSE_RES_HIGH;
			else if (strcmp(optarg, "medium-low") == 0)
				opt_resolution = MOUSE_RES_MEDIUMLOW;
			else if (strcmp(optarg, "low") == 0)
				opt_resolution = MOUSE_RES_LOW;
			else if (strcmp(optarg, "default") == 0)
				opt_resolution = MOUSE_RES_DEFAULT;
			else {
				ul= strtoul(optarg, NULL, 10);
				if (ul == 0) {
					warnx("invalid argument `%s'", optarg);
					usage();
				}
				opt_resolution = ul;
			}
			break;

		case 't':
			if (strcmp(optarg, "auto") == 0) {
				force_if = DEVICE_IF_UNKNOWN;
				break;
			}
			for (i = 0; i < nitems(rifs); i++)
				if (strcmp(optarg, rifs[i].name) == 0) {
					force_if = i;
					break;
				}
			if (i == nitems(rifs)) {
				warnx("no such interface type `%s'", optarg);
				usage();
			}
			break;

		case 'w':
			ul = strtoul(optarg, NULL, 10);
			if (ul == 0 || ul > MOUSE_MAXBUTTON) {
				warnx("invalid argument `%s'", optarg);
				usage();
			}
			opt_wmode = ul;
			break;

		case 'z':
			--optind;
			errstr = r_installzmap(argv, argc, &optind, &opt_btstate);
			if (errstr != NULL) {
				warnx("%s", errstr);
				free(errstr);
				usage();
			}
			break;

		case 'C':
			ul = strtoul(optarg, NULL, 10);
			if (ul > MAX_CLICKTHRESHOLD) {
				warnx("invalid argument `%s'", optarg);
				usage();
			}
			opt_clickthreshold = ul;
			break;

		case 'F':
			ul = strtoul(optarg, NULL, 10);
			if (ul == 0) {
				warnx("invalid argument `%s'", optarg);
				usage();
			}
			opt_rate = ul;
			break;

		case 'H':
			opt_hvirtual_scroll = true;
			break;
		
		case 'I':
			pidfile = optarg;
			break;

		case 'L':
			errno = 0;
			ul = strtoul(optarg, NULL, 10);
			if ((ul == 0 && errno != 0) || ul > INT_MAX) {
				warnx("invalid argument `%s'", optarg);
				usage();
			}
			opt_scroll_speed = ul;
			break;

		case 'q':
			config_file = optarg;
			break;

		case 'Q':
			quirks_path = optarg;
			break;

		case 'T':
			opt_drift_terminate = true;
			sscanf(optarg, "%u,%u,%u", &opt_drift_distance,
			    &opt_drift_time, &opt_drift_after);
			if (opt_drift_distance == 0 ||
			    opt_drift_time == 0 ||
			    opt_drift_after == 0) {
				warnx("invalid argument `%s'", optarg);
				usage();
			}
			break;

		case 'V':
			opt_virtual_scroll = true;
			break;

		case 'U':
			errno = 0;
			ul = strtoul(optarg, NULL, 10);
			if ((ul == 0 && errno != 0) || ul > INT_MAX) {
				warnx("invalid argument `%s'", optarg);
				usage();
			}
			opt_scroll_threshold = ul;
			break;

		case 'h':
		case '?':
		default:
			usage();
		}
	}

	if ((cfd = open("/dev/consolectl", O_RDWR, 0)) == -1)
		logerr(1, "cannot open /dev/consolectl");
	if ((kfd = kqueue()) == -1)
		logerr(1, "cannot create kqueue");
	if (portname == NULL && (dfd = connect_devd()) == -1)
		logwarnx("cannot open devd socket");

	switch (setjmp(env)) {
	case SIGHUP:
		quirks_context_unref(quirks);
		r_deinit_all();
		/* FALLTHROUGH */
	case 0:
		break;
	case SIGINT:
	case SIGQUIT:
	case SIGTERM:
		exit(0);
		/* NOT REACHED */
	default:
		goto out;
	}

	signal(SIGHUP , reset);
	signal(SIGINT , reset);
	signal(SIGQUIT, reset);
	signal(SIGTERM, reset);
	signal(SIGUSR1, pause_mouse);

	quirks = quirks_init_subsystem(quirks_path, config_file,
	    log_or_warn_va,
	    background ? QLOG_MOUSED_LOGGING : QLOG_CUSTOM_LOG_PRIORITIES);
	if (quirks == NULL)
		logwarnx("cannot open configuration file %s", config_file);

	if (portname == NULL) {
		r_init_all();
	} else {
		if ((r = r_init(portname)) == NULL)
			logerrx(1, "Can not initialize device");
	}

	/* print some information */
	if (identify != ID_NONE) {
		SLIST_FOREACH(r, &rodents, next) {
			if (identify == ID_ALL)
				printf("%s %s %s %s\n",
				    r->dev.path, r_if(r->dev.iftype),
				    r_name(r->dev.type), r->dev.name);
			else if (identify & ID_PORT)
				printf("%s\n", r->dev.path);
			else if (identify & ID_IF)
				printf("%s\n", r_if(r->dev.iftype));
			else if (identify & ID_TYPE)
				printf("%s\n", r_name(r->dev.type));
			else if (identify & ID_MODEL)
				printf("%s\n", r->dev.name);
		}
		exit(0);
	}

	if (!nodaemon && !background) {
		pfh = pidfile_open(pidfile, 0600, &mpid);
		if (pfh == NULL) {
			if (errno == EEXIST)
				logerrx(1, "moused already running, pid: %d", mpid);
			logwarn("cannot open pid file");
		}
		if (r_daemon()) {
			int saved_errno = errno;
			pidfile_remove(pfh);
			errno = saved_errno;
			logerr(1, "failed to become a daemon");
		} else {
			background = true;
			pidfile_write(pfh);
		}
	}

	moused();

out:
	quirks_context_unref(quirks);

	r_deinit_all();
	if (dfd != -1)
		close(dfd);
	if (kfd != -1)
		close(kfd);
	if (cfd != -1)
		close(cfd);

	exit(0);
}

/*
 * Function to calculate linear acceleration.
 *
 * If there are any rounding errors, the remainder
 * is stored in the remainx and remainy variables
 * and taken into account upon the next movement.
 */

static void
linacc(struct accel *acc, int dx, int dy, int dz,
    int *movex, int *movey, int *movez)
{
	double fdx, fdy, fdz;

	if (dx == 0 && dy == 0 && dz == 0) {
		*movex = *movey = *movez = 0;
		return;
	}
	fdx = dx * acc->accelx + acc->remainx;
	fdy = dy * acc->accely + acc->remainy;
	fdz = dz * acc->accelz + acc->remainz;
	*movex = lround(fdx);
	*movey = lround(fdy);
	*movez = lround(fdz);
	acc->remainx = fdx - *movex;
	acc->remainy = fdy - *movey;
	acc->remainz = fdz - *movez;
}

/*
 * Function to calculate exponential acceleration.
 * (Also includes linear acceleration if enabled.)
 *
 * In order to give a smoother behaviour, we record the four
 * most recent non-zero movements and use their average value
 * to calculate the acceleration.
 */

static void
expoacc(struct accel *acc, int dx, int dy, int dz,
    int *movex, int *movey, int *movez)
{
	double fdx, fdy, fdz, length, lbase, accel;

	if (dx == 0 && dy == 0 && dz == 0) {
		*movex = *movey = *movez = 0;
		return;
	}
	fdx = dx * acc->accelx;
	fdy = dy * acc->accely;
	fdz = dz * acc->accelz;
	length = sqrt((fdx * fdx) + (fdy * fdy));	/* Pythagoras */
	length = (length + acc->lastlength[0] + acc->lastlength[1] +
	    acc->lastlength[2]) / 4;
	lbase = length / acc->expoffset;
	accel = pow(lbase, acc->expoaccel) / lbase;
	fdx = fdx * accel + acc->remainx;
	fdy = fdy * accel + acc->remainy;
	*movex = lround(fdx);
	*movey = lround(fdy);
	*movez = lround(fdz);
	acc->remainx = fdx - *movex;
	acc->remainy = fdy - *movey;
	acc->remainz = fdz - *movez;
	acc->lastlength[2] = acc->lastlength[1];
	acc->lastlength[1] = acc->lastlength[0];
	/* Insert new average, not original length! */
	acc->lastlength[0] = length;
}

static void
moused(void)
{
	struct rodent *r = NULL;
	mousestatus_t action0;		/* original mouse action */
	mousestatus_t action;		/* interim buffer */
	mousestatus_t action2;		/* mapped action */
	struct kevent ke[3];
	int nchanges;
	union {
		struct input_event ie;
		uint8_t se[MOUSE_SYS_PACKETSIZE];
	} b;
	size_t b_size;
	ssize_t r_size;
	int flags;
	int c;

	/* clear mouse data */
	bzero(&action0, sizeof(action0));
	bzero(&action, sizeof(action));
	bzero(&action2, sizeof(action2));
	/* process mouse data */
	for (;;) {

		if (dfd == -1 && portname == NULL)
			dfd = connect_devd();
		nchanges = 0;
		if (r != NULL && r->e3b.enabled &&
		    S_DELAYED(r->e3b.mouse_button_state)) {
			EV_SET(ke + nchanges, r->mfd << 1, EVFILT_TIMER,
			    EV_ADD | EV_ENABLE | EV_DISPATCH, 0, 20, r);
			nchanges++;
			r->e3b.timer_armed = true;
		}
		if (r != NULL && r->tp.gest.idletimeout > 0) {
			EV_SET(ke + nchanges, r->mfd << 1 | 1, EVFILT_TIMER,
			    EV_ADD | EV_ENABLE | EV_DISPATCH,
			    0, r->tp.gest.idletimeout, r);
			nchanges++;
			r->tp.gest.timer_armed = true;
		}
		if (dfd == -1 && nchanges == 0 && portname == NULL) {
			EV_SET(ke + nchanges, UINTPTR_MAX, EVFILT_TIMER,
			    EV_ADD | EV_ENABLE | EV_ONESHOT, 0, 1000, NULL);
			nchanges++;
		}

		if (!(r != NULL && r->tp.gest.idletimeout == 0)) {
			c = kevent(kfd, ke, nchanges, ke, 1, NULL);
			if (c <= 0) {			/* error */
				logwarn("failed to read from mouse");
				continue;
			}
		} else
			c = 0;
		/* Devd event */
		if (c > 0 && ke[0].udata == NULL) {
			if (ke[0].filter == EVFILT_READ) {
				if ((ke[0].flags & EV_EOF) != 0) {
					logwarn("devd connection is closed");
					close(dfd);
					dfd = -1;
				} else
					fetch_and_parse_devd();
			} else if (ke[0].filter == EVFILT_TIMER) {
				/* DO NOTHING */
			}
			continue;
		}
		if (c > 0)
			r = ke[0].udata;
		/* E3B timeout */
		if (c > 0 && ke[0].filter == EVFILT_TIMER &&
		    (ke[0].ident & 1) == 0) {
			/* assert(rodent.flags & Emulate3Button) */
			action0.button = action0.obutton;
			action0.dx = action0.dy = action0.dz = 0;
			action0.flags = flags = 0;
			r->e3b.timer_armed = false;
			if (r_timeout(&r->e3b) &&
			    r_statetrans(r, &action0, &action, A_TIMEOUT)) {
				if (debug > 2)
					debug("flags:%08x buttons:%08x obuttons:%08x",
					    action.flags, action.button, action.obutton);
			} else {
				action0.obutton = action0.button;
				continue;
			}
		} else {
			/* mouse movement */
			if (c > 0 && ke[0].filter == EVFILT_READ) {
				b_size = rifs[r->dev.iftype].p_size;
				r_size = read(r->mfd, &b, b_size);
				if (r_size == -1) {
					if (errno == EWOULDBLOCK)
						continue;
					else if (portname == NULL) {
						r_deinit(r);
						r = NULL;
						continue;
					} else
						return;
				}
				if (r_size != (ssize_t)b_size) {
					logwarn("Short read from mouse: "
					    "%zd bytes", r_size);
					continue;
				}
				/* Disarm nonexpired timers */
				nchanges = 0;
				if (r->e3b.timer_armed) {
					EV_SET(ke + nchanges, r->mfd << 1,
					    EVFILT_TIMER, EV_DISABLE, 0, 0, r);
					nchanges++;
					r->e3b.timer_armed = false;
				}
				if (r->tp.gest.timer_armed) {
					EV_SET(ke + nchanges, r->mfd << 1 | 1,
					    EVFILT_TIMER, EV_DISABLE, 0, 0, r);
					nchanges++;
					r->tp.gest.timer_armed = false;
				}
				if (nchanges != 0)
					kevent(kfd, ke, nchanges, NULL, 0, NULL);
			} else {
				/*
				 * Gesture timeout expired.
				 * Notify r_gestures by empty packet.
				 */
#ifdef DONE_RIGHT
				struct timespec ts;
				clock_gettime(CLOCK_REALTIME, &ts);
				b.ie.time.tv_sec = ts.tv_sec;
				b.ie.time.tv_usec = ts.tv_nsec / 1000;
#else
				/* Hacky but cheap */
				b.ie.time.tv_sec =
				    r->tp.gest.idletimeout == 0 ? 0 : LONG_MAX;
				b.ie.time.tv_usec = 0;
#endif
				b.ie.type = EV_SYN;
				b.ie.code = SYN_REPORT;
				b.ie.value = 1;
				if (c > 0)
					r->tp.gest.timer_armed = false;
			}
			r->tp.gest.idletimeout = -1;
			flags = r->dev.iftype == DEVICE_IF_EVDEV ?
			    r_protocol_evdev(r->dev.type,
			        &r->tp, &r->ev, &b.ie, &action0) :
			    r_protocol_sysmouse(b.se, &action0);
			if (flags == 0)
				continue;

			if (r->scroll.enable_vert || r->scroll.enable_hor) {
				if (action0.button == MOUSE_BUTTON2DOWN) {
					debug("[BUTTON2] flags:%08x buttons:%08x obuttons:%08x",
					    action.flags, action.button, action.obutton);
				} else {
					debug("[NOTBUTTON2] flags:%08x buttons:%08x obuttons:%08x",
					    action.flags, action.button, action.obutton);
				}
				r_vscroll_detect(r, &r->scroll, &action0);
			}

			r_timestamp(&action0, &r->btstate, &r->e3b, &r->drift);
			r_statetrans(r, &action0, &action,
			    A(action0.button & MOUSE_BUTTON1DOWN,
			      action0.button & MOUSE_BUTTON3DOWN));
			debug("flags:%08x buttons:%08x obuttons:%08x", action.flags,
			    action.button, action.obutton);
		}
		action0.obutton = action0.button;
		flags &= MOUSE_POSCHANGED;
		flags |= action.obutton ^ action.button;
		action.flags = flags;

		if (flags == 0)
			continue;

		/* handler detected action */
		r_map(&action, &action2, &r->btstate);
		debug("activity : buttons 0x%08x  dx %d  dy %d  dz %d",
		    action2.button, action2.dx, action2.dy, action2.dz);

		if (r->scroll.enable_vert || r->scroll.enable_hor) {
			/*
			 * If *only* the middle button is pressed AND we are moving
			 * the stick/trackpoint/nipple, scroll!
			 */
			r_vscroll(&r->scroll, &action2);
		}

		if (r->drift.terminate) {
			if ((flags & MOUSE_POSCHANGED) == 0 ||
			    action.dz || action2.dz)
				r->drift.last_activity = r->drift.current_ts;
			else {
				if (r_drift (&r->drift, &action2))
					continue;
			}
		}

		/* Defer clicks until we aren't VirtualScroll'ing. */
		if (r->scroll.state == SCROLL_NOTSCROLLING)
			r_click(&action2, &r->btstate);

		if (action2.flags & MOUSE_POSCHANGED)
			r_move(&action2, &r->accel);

		/*
		 * If the Z axis movement is mapped to an imaginary physical
		 * button, we need to cook up a corresponding button `up' event
		 * after sending a button `down' event.
		 */
		if ((r->btstate.zmap[0] > 0) && (action.dz != 0)) {
			action.obutton = action.button;
			action.dx = action.dy = action.dz = 0;
			r_map(&action, &action2, &r->btstate);
			debug("activity : buttons 0x%08x  dx %d  dy %d  dz %d",
			    action2.button, action2.dx, action2.dy, action2.dz);

			r_click(&action2, &r->btstate);
		}
	}
	/* NOT REACHED */
}

static void
reset(int sig)
{
	longjmp(env, sig);
}

static void
pause_mouse(__unused int sig)
{
	paused = !paused;
}

static int
connect_devd(void)
{
	const static struct sockaddr_un sa = {
		.sun_family = AF_UNIX,
		.sun_path = "/var/run/devd.seqpacket.pipe",
	};
	struct kevent kev;
	int fd;

	fd = socket(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0);
	if (fd < 0)
		return (-1);
	if (connect(fd, (const struct sockaddr *) &sa, sizeof(sa)) < 0) {
		close(fd);
		return (-1);
	}
	EV_SET(&kev, fd, EVFILT_READ, EV_ADD, 0, 0, 0);
	if (kevent(kfd, &kev, 1, NULL, 0, NULL) < 0) {
		close(fd);
		return (-1);
	}

	return (fd);
}

static void
fetch_and_parse_devd(void)
{
	char ev[1024];
	char path[22] = "/dev/";
	char *cdev, *cr;
	ssize_t len;

	if ((len = recv(dfd, ev, sizeof(ev), MSG_WAITALL)) <= 0) {
		close(dfd);
		dfd = -1;
		return;
	}

	if (ev[0] != '!')
		return;
	if (strnstr(ev, "system=DEVFS", len) == NULL)
		return;
	if (strnstr(ev, "subsystem=CDEV", len) == NULL)
		return;
	if (strnstr(ev, "type=CREATE", len) == NULL)
		return;
	if ((cdev = strnstr(ev, "cdev=input/event", len)) == NULL)
		return;
	cr = strchr(cdev, '\n');
	if (cr != NULL)
		*cr = '\0';
	cr = strchr(cdev, ' ');
	if (cr != NULL)
		*cr = '\0';
	strncpy(path + 5, cdev + 5, 17);
	(void)r_init(path);
	return;
}

/*
 * usage
 *
 * Complain, and free the CPU for more worthy tasks
 */
static void
usage(void)
{
	fprintf(stderr, "%s\n%s\n%s\n%s\n%s\n",
	    "usage: moused [-dfg] [-I file] [-F rate] [-r resolution]",
	    "              [-VH [-U threshold]] [-a X[,Y]] [-C threshold] [-m N=M] [-w N]",
	    "              [-z N] [-t <interfacetype>] [-l level] [-3 [-E timeout]]",
	    "              [-T distance[,time[,after]]] -p <port> [-q config] [-Q quirks]",
	    "       moused [-d] -i <port|if|type|model|all> -p <port>");
	exit(1);
}

/*
 * Output an error message to syslog or stderr as appropriate. If
 * `errnum' is non-zero, append its string form to the message.
 */
static void
log_or_warn_va(int log_pri, int errnum, const char *fmt, va_list ap)
{
	char buf[256];
	size_t len;

	if (debug == 0 && log_pri > LOG_ERR)
		return;

	vsnprintf(buf, sizeof(buf), fmt, ap);

	/* Strip trailing line-feed appended by quirk subsystem */
	len = strlen(buf);
	if (len != 0 && buf[len - 1] == '\n')
		buf[len - 1] = '\0';

	if (errnum) {
		strlcat(buf, ": ", sizeof(buf));
		strlcat(buf, strerror(errnum), sizeof(buf));
	}

	if (background)
		syslog(log_pri, "%s", buf);
	else
		warnx("%s", buf);
}

static void
log_or_warn(int log_pri, int errnum, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	log_or_warn_va(log_pri, errnum, fmt, ap);
	va_end(ap);
}

static int
r_daemon(void)
{
	struct sigaction osa, sa;
	pid_t newgrp;
	int oerrno;
	int osa_ok;
	int nullfd;

	/* A SIGHUP may be thrown when the parent exits below. */
	sigemptyset(&sa.sa_mask);
	sa.sa_handler = SIG_IGN;
	sa.sa_flags = 0;
	osa_ok = sigaction(SIGHUP, &sa, &osa);

	/* Keep kqueue fd alive */
	switch (rfork(RFPROC)) {
	case -1:
		return (-1);
	case 0:
		break;
	default:
		/*
		 * A fine point:  _exit(0), not exit(0), to avoid triggering
		 * atexit(3) processing
		 */
		_exit(0);
	}

	newgrp = setsid();
	oerrno = errno;
	if (osa_ok != -1)
		sigaction(SIGHUP, &osa, NULL);

	if (newgrp == -1) {
		errno = oerrno;
		return (-1);
	}

	(void)chdir("/");

	nullfd = open("/dev/null", O_RDWR, 0);
	if (nullfd != -1) {
		(void)dup2(nullfd, STDIN_FILENO);
		(void)dup2(nullfd, STDOUT_FILENO);
		(void)dup2(nullfd, STDERR_FILENO);
	}
	if (nullfd > 2)
		close(nullfd);

	return (0);
}

static inline int
bit_find(bitstr_t *array, int start, int stop)
{
	int res;

	bit_ffs_at(array, start, stop + 1, &res);
	return (res != -1);
}

static enum device_if
r_identify_if(int fd)
{
	int dummy;

	if ((force_if == DEVICE_IF_UNKNOWN || force_if == DEVICE_IF_EVDEV) &&
	    ioctl(fd, EVIOCGVERSION, &dummy) >= 0)
		return (DEVICE_IF_EVDEV);
	if ((force_if == DEVICE_IF_UNKNOWN || force_if == DEVICE_IF_SYSMOUSE) &&
	    ioctl(fd, MOUSE_GETLEVEL, &dummy) >= 0)
		return (DEVICE_IF_SYSMOUSE);
	return (DEVICE_IF_UNKNOWN);
}

/* Derived from EvdevProbe() function of xf86-input-evdev driver */
static enum device_type
r_identify_evdev(int fd)
{
	enum device_type type;
	bitstr_t bit_decl(key_bits, KEY_CNT); /* */
	bitstr_t bit_decl(rel_bits, REL_CNT); /* Evdev capabilities */
	bitstr_t bit_decl(abs_bits, ABS_CNT); /* */
	bitstr_t bit_decl(prop_bits, INPUT_PROP_CNT);
	bool has_keys, has_buttons, has_lmr, has_rel_axes, has_abs_axes;
	bool has_mt;

	/* maybe this is a evdev mouse... */
	if (ioctl(fd, EVIOCGBIT(EV_REL, sizeof(rel_bits)), rel_bits) < 0 ||
	    ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(abs_bits)), abs_bits) < 0 ||
	    ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(key_bits)), key_bits) < 0 ||
	    ioctl(fd, EVIOCGPROP(sizeof(prop_bits)), prop_bits) < 0) {
		return (DEVICE_TYPE_UNKNOWN);
	}

	has_keys = bit_find(key_bits, 0, BTN_MISC - 1);
	has_buttons = bit_find(key_bits, BTN_MISC, BTN_JOYSTICK - 1);
	has_lmr = bit_find(key_bits, BTN_LEFT, BTN_MIDDLE);
	has_rel_axes = bit_find(rel_bits, 0, REL_MAX);
	has_abs_axes = bit_find(abs_bits, 0, ABS_MAX);
	has_mt = bit_find(abs_bits, ABS_MT_SLOT, ABS_MAX);
	type = DEVICE_TYPE_UNKNOWN;

	if (has_abs_axes) {
		if (has_mt && !has_buttons) {
			/* TBD:Improve joystick detection */
			if (bit_test(key_bits, BTN_JOYSTICK)) {
				return (DEVICE_TYPE_JOYSTICK);
			} else {
				has_buttons = true;
			}
		}

		if (bit_test(abs_bits, ABS_X) &&
		    bit_test(abs_bits, ABS_Y)) {
			if (bit_test(key_bits, BTN_TOOL_PEN) ||
			    bit_test(key_bits, BTN_STYLUS) ||
			    bit_test(key_bits, BTN_STYLUS2)) {
				type = DEVICE_TYPE_TABLET;
			} else if (bit_test(abs_bits, ABS_PRESSURE) ||
				   bit_test(key_bits, BTN_TOUCH)) {
				if (has_lmr ||
				    bit_test(key_bits, BTN_TOOL_FINGER)) {
					type = DEVICE_TYPE_TOUCHPAD;
				} else {
					type = DEVICE_TYPE_TOUCHSCREEN;
				}
			/* some touchscreens use BTN_LEFT rather than BTN_TOUCH */
			} else if (!(bit_test(rel_bits, REL_X) &&
				     bit_test(rel_bits, REL_Y)) &&
				     has_lmr) {
				type = DEVICE_TYPE_TOUCHSCREEN;
			}
		}
	}

	if (type == DEVICE_TYPE_UNKNOWN) {
		if (has_keys)
			type = DEVICE_TYPE_KEYBOARD;
		else if (has_rel_axes || has_buttons)
			type = DEVICE_TYPE_MOUSE;
	}

	return (type);
}

static enum device_type
r_identify_sysmouse(int fd __unused)
{
	/* All sysmouse devices act like mices */
	return (DEVICE_TYPE_MOUSE);
}

static const char *
r_if(enum device_if type)
{
	const char *unknown = "unknown";

	return (type == DEVICE_IF_UNKNOWN || type >= (int)nitems(rifs) ?
	    unknown : rifs[type].name);
}

static const char *
r_name(enum device_type type)
{
	const char *unknown = "unknown";

	return (type == DEVICE_TYPE_UNKNOWN || type >= (int)nitems(rnames) ?
	    unknown : rnames[type]);
}

static int
r_init_dev_evdev(int fd, struct device *dev)
{
	if (ioctl(fd, EVIOCGNAME(sizeof(dev->name) - 1), dev->name) < 0) {
		logwarnx("unable to get device %s name", dev->path);
		return (errno);
	}
	/* Do not loop events */
	if (strncmp(dev->name, "System mouse", sizeof(dev->name)) == 0) {
		return (ENOTSUP);
	}
	if (ioctl(fd, EVIOCGID, &dev->id) < 0) {
		logwarnx("unable to get device %s ID", dev->path);
		return (errno);
	}
	(void)ioctl(fd, EVIOCGUNIQ(sizeof(dev->uniq) - 1), dev->uniq);

	return (0);
}

static int
r_init_dev_sysmouse(int fd, struct device *dev)
{
	mousemode_t *mode = &dev->mode;
	int level;

	level = 1;
	if (ioctl(fd, MOUSE_SETLEVEL, &level) < 0) {
		logwarnx("unable to MOUSE_SETLEVEL for device %s", dev->path);
		return (errno);
	}
	if (ioctl(fd, MOUSE_GETLEVEL, &level) < 0) {
		logwarnx("unable to MOUSE_GETLEVEL for device %s", dev->path);
		return (errno);
	}
	if (level != 1) {
		logwarnx("unable to set level to 1 for device %s", dev->path);
		return (ENOTSUP);
	}
	memset(mode, 0, sizeof(*mode));
	if (ioctl(fd, MOUSE_GETMODE, mode) < 0) {
		logwarnx("unable to MOUSE_GETMODE for device %s", dev->path);
		return (errno);
	}
	if (mode->protocol != MOUSE_PROTO_SYSMOUSE) {
		logwarnx("unable to set sysmouse protocol for device %s",
		    dev->path);
		return (ENOTSUP);
	}
	if (mode->packetsize != MOUSE_SYS_PACKETSIZE) {
		logwarnx("unable to set sysmouse packet size for device %s",
		    dev->path);
		return (ENOTSUP);
	}

	/* TODO: Fill name, id and uniq from dev.* sysctls */
	strlcpy(dev->name, dev->path, sizeof(dev->name));

	return (0);
}

static void
r_init_evstate(struct quirks *q, struct evstate *ev)
{
	const struct quirk_tuples *t;
	bitstr_t *bitstr;
	int maxbit;

	if (quirks_get_tuples(q, QUIRK_ATTR_EVENT_CODE, &t)) {
		for (size_t i = 0; i < t->ntuples; i++) {
			int type = t->tuples[i].first;
			int code = t->tuples[i].second;
			bool enable = t->tuples[i].third;

			switch (type) {
			case EV_KEY:
				bitstr = (bitstr_t *)&ev->key_ignore;
				maxbit = KEY_MAX;
				break;
			case EV_REL:
				bitstr = (bitstr_t *)&ev->rel_ignore;
				maxbit = REL_MAX;
				break;
			case EV_ABS:
				bitstr = (bitstr_t *)&ev->abs_ignore;
				maxbit = ABS_MAX;
				break;
			default:
				continue;
			}

			if (code == EVENT_CODE_UNDEFINED) {
				if (enable)
					bit_nclear(bitstr, 0, maxbit);
				else
					bit_nset(bitstr, 0, maxbit);
			} else {
				if (code > maxbit)
					continue;
				if (enable)
					bit_clear(bitstr, code);
				else
					bit_set(bitstr, code);
	                }
	        }
	}

	if (quirks_get_tuples(q, QUIRK_ATTR_INPUT_PROP, &t)) {
		for (size_t idx = 0; idx < t->ntuples; idx++) {
			unsigned int p = t->tuples[idx].first;
			bool enable = t->tuples[idx].second;

			if (p > INPUT_PROP_MAX)
				continue;
			if (enable)
				bit_clear(ev->prop_ignore, p);
			else
				bit_set(ev->prop_ignore, p);
                }
        }
}

static void
r_init_buttons(struct quirks *q, struct btstate *bt, struct e3bstate *e3b)
{
	struct timespec ts;
	int i, j;

	*bt = (struct btstate) {
		.clickthreshold = DFLT_CLICKTHRESHOLD,
		.zmap = { 0, 0, 0, 0 },
	};

	memcpy(bt->p2l, default_p2l, sizeof(bt->p2l));
	for (i = 0; i < MOUSE_MAXBUTTON; ++i) {
		j = i;
		if (opt_btstate.p2l[i] != 0)
			bt->p2l[i] = opt_btstate.p2l[i];
		if (opt_btstate.mstate[i] != NULL)
			j = opt_btstate.mstate[i] - opt_btstate.bstate;
		bt->mstate[i] = bt->bstate + j;
	}

	if (opt_btstate.zmap[0] != 0)
		memcpy(bt->zmap, opt_btstate.zmap, sizeof(bt->zmap));
	if (opt_clickthreshold >= 0)
		bt->clickthreshold = opt_clickthreshold;
	else
		quirks_get_uint32(q, MOUSED_CLICK_THRESHOLD, &bt->clickthreshold);
	if (opt_wmode != 0)
		bt->wmode = opt_wmode;
	else
		quirks_get_uint32(q, MOUSED_WMODE, &bt->wmode);
	if (bt->wmode != 0)
		bt->wmode = 1 << (bt->wmode - 1);

	/* fix Z axis mapping */
	for (i = 0; i < ZMAP_MAXBUTTON; ++i) {
		if (bt->zmap[i] <= 0)
			continue;
		for (j = 0; j < MOUSE_MAXBUTTON; ++j) {
			if (bt->mstate[j] == &bt->bstate[bt->zmap[i] - 1])
				bt->mstate[j] = &bt->zstate[i];
		}
		bt->zmap[i] = 1 << (bt->zmap[i] - 1);
	}

	clock_gettime(CLOCK_MONOTONIC_FAST, &ts);

	*e3b = (struct e3bstate) {
		.enabled = false,
		.button2timeout = DFLT_BUTTON2TIMEOUT,
	};
	e3b->enabled = opt_e3b_enabled;
	if (!e3b->enabled)
		quirks_get_bool(q, MOUSED_EMULATE_THIRD_BUTTON, &e3b->enabled);
	if (opt_e3b_button2timeout >= 0)
		e3b->button2timeout = opt_e3b_button2timeout;
	else
		quirks_get_uint32(q, MOUSED_EMULATE_THIRD_BUTTON_TIMEOUT,
		    &e3b->button2timeout);
	e3b->mouse_button_state = S0;
	e3b->mouse_button_state_ts = ts;
	e3b->mouse_move_delayed = 0;

	for (i = 0; i < MOUSE_MAXBUTTON; ++i) {
		bt->bstate[i].count = 0;
		bt->bstate[i].ts = ts;
	}
	for (i = 0; i < ZMAP_MAXBUTTON; ++i) {
		bt->zstate[i].count = 0;
		bt->zstate[i].ts = ts;
	}
}

static void
r_init_touchpad_hw(int fd, struct quirks *q, struct tpcaps *tphw,
     struct evstate *ev)
{
	struct input_absinfo ai;
	bitstr_t bit_decl(key_bits, KEY_CNT);
	bitstr_t bit_decl(abs_bits, ABS_CNT);
	bitstr_t bit_decl(prop_bits, INPUT_PROP_CNT);
	struct quirk_range r;
	struct quirk_dimensions dim;
	u_int u;

	ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(abs_bits)), abs_bits);
	ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(key_bits)), key_bits);

	if (!bit_test(ev->abs_ignore, ABS_X) &&
	     ioctl(fd, EVIOCGABS(ABS_X), &ai) >= 0) {
		tphw->min_x = (ai.maximum > ai.minimum) ? ai.minimum : INT_MIN;
		tphw->max_x = (ai.maximum > ai.minimum) ? ai.maximum : INT_MAX;
		tphw->res_x = ai.resolution == 0 ?
		    DFLT_TPAD_RESOLUTION : ai.resolution;
	}
	if (!bit_test(ev->abs_ignore, ABS_Y) &&
	     ioctl(fd, EVIOCGABS(ABS_Y), &ai) >= 0) {
		tphw->min_y = (ai.maximum > ai.minimum) ? ai.minimum : INT_MIN;
		tphw->max_y = (ai.maximum > ai.minimum) ? ai.maximum : INT_MAX;
		tphw->res_y = ai.resolution == 0 ?
		    DFLT_TPAD_RESOLUTION : ai.resolution;
	}
	if (quirks_get_dimensions(q, QUIRK_ATTR_RESOLUTION_HINT, &dim)) {
		tphw->res_x = dim.x;
		tphw->res_y = dim.y;
	} else if (tphw->max_x != INT_MAX && tphw->max_y != INT_MAX &&
		   quirks_get_dimensions(q, QUIRK_ATTR_SIZE_HINT, &dim)) {
		tphw->res_x = (tphw->max_x - tphw->min_x) / dim.x;
		tphw->res_y = (tphw->max_y - tphw->min_y) / dim.y;
	}
	if (!bit_test(ev->key_ignore, BTN_TOUCH) &&
	     bit_test(key_bits, BTN_TOUCH))
		tphw->cap_touch = true;
	/* XXX: libinput uses ABS_MT_PRESSURE where available */
	if (!bit_test(ev->abs_ignore, ABS_PRESSURE) &&
	     bit_test(abs_bits, ABS_PRESSURE) &&
	     ioctl(fd, EVIOCGABS(ABS_PRESSURE), &ai) >= 0) {
		tphw->cap_pressure = true;
		tphw->min_p = ai.minimum;
		tphw->max_p = ai.maximum;
	}
	if (tphw->cap_pressure &&
	    quirks_get_range(q, QUIRK_ATTR_PRESSURE_RANGE, &r)) {
		if (r.upper == 0 && r.lower == 0) {
			debug("pressure-based touch detection disabled");
			tphw->cap_pressure = false;
		} else if (r.upper > tphw->max_p || r.upper < tphw->min_p ||
			   r.lower > tphw->max_p || r.lower < tphw->min_p) {
			debug("discarding out-of-bounds pressure range %d:%d",
			    r.lower, r.upper);
			tphw->cap_pressure = false;
		}
	}
	/* XXX: libinput uses ABS_MT_TOUCH_MAJOR where available */
	if (!bit_test(ev->abs_ignore, ABS_TOOL_WIDTH) &&
	     bit_test(abs_bits, ABS_TOOL_WIDTH) &&
	     quirks_get_uint32(q, QUIRK_ATTR_PALM_SIZE_THRESHOLD, &u) &&
	     u != 0)
		tphw->cap_width = true;
	if (!bit_test(ev->abs_ignore, ABS_MT_SLOT) &&
	     bit_test(abs_bits, ABS_MT_SLOT) &&
	    !bit_test(ev->abs_ignore, ABS_MT_TRACKING_ID) &&
	     bit_test(abs_bits, ABS_MT_TRACKING_ID) &&
	    !bit_test(ev->abs_ignore, ABS_MT_POSITION_X) &&
	     bit_test(abs_bits, ABS_MT_POSITION_X) &&
	    !bit_test(ev->abs_ignore, ABS_MT_POSITION_Y) &&
	     bit_test(abs_bits, ABS_MT_POSITION_Y))
		tphw->is_mt = true;
	if ( ioctl(fd, EVIOCGPROP(sizeof(prop_bits)), prop_bits) >= 0 &&
	    !bit_test(ev->prop_ignore, INPUT_PROP_BUTTONPAD) &&
	     bit_test(prop_bits, INPUT_PROP_BUTTONPAD))
		tphw->is_clickpad = true;
	if ( tphw->is_clickpad &&
	    !bit_test(ev->prop_ignore, INPUT_PROP_TOPBUTTONPAD) &&
	     bit_test(prop_bits, INPUT_PROP_TOPBUTTONPAD))
		tphw->is_topbuttonpad = true;
}

static void
r_init_touchpad_info(struct quirks *q, struct tpcaps *tphw,
    struct tpinfo *tpinfo)
{
	struct quirk_range r;
	int i;
	u_int u;
	int sz_x, sz_y;

	*tpinfo = (struct tpinfo) {
		.two_finger_scroll = true,
		.natural_scroll = false,
		.three_finger_drag = false,
		.min_pressure_hi = 1,
		.min_pressure_lo = 1,
		.max_pressure = 130,
		.max_width = 16,
		.tap_timeout = 180,		/* ms */
		.tap_threshold = 0,
		.tap_max_delta = 1.3,		/* mm */
		.taphold_timeout = 300,		/* ms */
		.vscroll_min_delta = 1.25,	/* mm */
		.vscroll_hor_area = 0.0,	/* mm */
		.vscroll_ver_area = -15.0,	/* mm */
	};

	quirks_get_bool(q, MOUSED_TWO_FINGER_SCROLL, &tpinfo->two_finger_scroll);
	quirks_get_bool(q, MOUSED_NATURAL_SCROLL, &tpinfo->natural_scroll);
	quirks_get_bool(q, MOUSED_THREE_FINGER_DRAG, &tpinfo->three_finger_drag);
	quirks_get_uint32(q, MOUSED_TAP_TIMEOUT, &tpinfo->tap_timeout);
	quirks_get_double(q, MOUSED_TAP_MAX_DELTA, &tpinfo->tap_max_delta);
	quirks_get_uint32(q, MOUSED_TAPHOLD_TIMEOUT, &tpinfo->taphold_timeout);
	quirks_get_double(q, MOUSED_VSCROLL_MIN_DELTA, &tpinfo->vscroll_min_delta);
	quirks_get_double(q, MOUSED_VSCROLL_HOR_AREA, &tpinfo->vscroll_hor_area);
	quirks_get_double(q, MOUSED_VSCROLL_VER_AREA, &tpinfo->vscroll_ver_area);

	if (tphw->cap_pressure &&
	    quirks_get_range(q, QUIRK_ATTR_PRESSURE_RANGE, &r)) {
		tpinfo->min_pressure_lo = r.lower;
		tpinfo->min_pressure_hi = r.upper;
		quirks_get_uint32(q, QUIRK_ATTR_PALM_PRESSURE_THRESHOLD,
		    &tpinfo->max_pressure);
		quirks_get_uint32(q, MOUSED_TAP_PRESSURE_THRESHOLD,
		    &tpinfo->tap_threshold);
	}
	if (tphw->cap_width)
		quirks_get_uint32(q, QUIRK_ATTR_PALM_SIZE_THRESHOLD,
		     &tpinfo->max_width);
	/* Set bottom quarter as 42% - 16% - 42% sized softbuttons */
	if (tphw->is_clickpad) {
		sz_x = tphw->max_x - tphw->min_x;
		sz_y = tphw->max_y - tphw->min_y;
		i = 25;
		if (tphw->is_topbuttonpad)
			i = -i;
		quirks_get_int32(q, MOUSED_SOFTBUTTONS_Y, &i);
		tpinfo->softbuttons_y = sz_y * i / 100;
		u = 42;
		quirks_get_uint32(q, MOUSED_SOFTBUTTON2_X, &u);
		tpinfo->softbutton2_x = sz_x * u / 100;
		u = 58;
		quirks_get_uint32(q, MOUSED_SOFTBUTTON3_X, &u);
		tpinfo->softbutton3_x = sz_x * u / 100;
	}
}

static void
r_init_touchpad_accel(struct tpcaps *tphw, struct accel *accel)
{
	/* Normalize pointer movement to match 200dpi mouse */
	accel->accelx *= DFLT_MOUSE_RESOLUTION;
	accel->accelx /= tphw->res_x;
	accel->accely *= DFLT_MOUSE_RESOLUTION;
	accel->accely /= tphw->res_y;
	accel->accelz *= DFLT_MOUSE_RESOLUTION;
	accel->accelz /= (tphw->res_x * DFLT_LINEHEIGHT);
}

static void
r_init_touchpad_gesture(struct tpstate *gest)
{
	gest->idletimeout = -1;
}

static void
r_init_drift(struct quirks *q, struct drift *d)
{
	if (opt_drift_terminate) {
		d->terminate = true;
		d->distance = opt_drift_distance;
		d->time = opt_drift_time;
		d->after = opt_drift_after;
	} else if (quirks_get_bool(q, MOUSED_DRIFT_TERMINATE, &d->terminate) &&
		   d->terminate) {
		quirks_get_uint32(q, MOUSED_DRIFT_DISTANCE, &d->distance);
		quirks_get_uint32(q, MOUSED_DRIFT_TIME, &d->time);
		quirks_get_uint32(q, MOUSED_DRIFT_AFTER, &d->after);
	} else
		return;

	if (d->distance == 0 || d->time == 0 || d->after == 0) {
		warnx("invalid drift parameter");
		exit(1);
	}

	debug("terminate drift: distance %d, time %d, after %d",
	    d->distance, d->time, d->after);

	d->time_ts = msec2ts(d->time);
	d->twotime_ts = msec2ts(d->time * 2);
	d->after_ts = msec2ts(d->after);
}

static void
r_init_accel(struct quirks *q, struct accel *acc)
{
	bool r1, r2;

	acc->accelx = opt_accelx;
	if (opt_accelx == 1.0)
		 quirks_get_double(q, MOUSED_LINEAR_ACCEL_X, &acc->accelx);
	acc->accely = opt_accely;
	if (opt_accely == 1.0)
		 quirks_get_double(q, MOUSED_LINEAR_ACCEL_Y, &acc->accely);
	if (!quirks_get_double(q, MOUSED_LINEAR_ACCEL_Z, &acc->accelz))
		acc->accelz = 1.0;
	acc->lastlength[0] = acc->lastlength[1] = acc->lastlength[2] = 0.0;
	if (opt_exp_accel) {
		acc->is_exponential = true;
		acc->expoaccel = opt_expoaccel;
		acc->expoffset = opt_expoffset;
		return;
	}
	acc->expoaccel = acc->expoffset = 1.0;
	r1 = quirks_get_double(q, MOUSED_EXPONENTIAL_ACCEL, &acc->expoaccel);
	r2 = quirks_get_double(q, MOUSED_EXPONENTIAL_OFFSET, &acc->expoffset);
	if (r1 || r2)
		acc->is_exponential = true;
}

static void
r_init_scroll(struct quirks *q, struct scroll *scroll)
{
	*scroll = (struct scroll) {
		.threshold = DFLT_SCROLLTHRESHOLD,
		.speed = DFLT_SCROLLSPEED,
		.state = SCROLL_NOTSCROLLING,
	};
	scroll->enable_vert = opt_virtual_scroll;
	if (!opt_virtual_scroll)
		quirks_get_bool(q, MOUSED_VIRTUAL_SCROLL_ENABLE, &scroll->enable_vert);
	scroll->enable_hor = opt_hvirtual_scroll;
	if (!opt_hvirtual_scroll)
		quirks_get_bool(q, MOUSED_HOR_VIRTUAL_SCROLL_ENABLE, &scroll->enable_hor);
	if (opt_scroll_speed >= 0)
		scroll->speed = opt_scroll_speed;
	else
		quirks_get_uint32(q, MOUSED_VIRTUAL_SCROLL_SPEED, &scroll->speed);
	if (opt_scroll_threshold >= 0)
		scroll->threshold = opt_scroll_threshold;
	else
		quirks_get_uint32(q, MOUSED_VIRTUAL_SCROLL_THRESHOLD, &scroll->threshold);
}

static struct rodent *
r_init(const char *path)
{
	struct rodent *r;
	struct device dev;
	struct quirks *q;
	struct kevent kev;
	enum device_if iftype;
	enum device_type type;
	int fd, err;
	bool grab;
	bool ignore;
	bool qvalid;

	fd = open(path, O_RDWR | O_NONBLOCK);
	if (fd == -1) {
		logwarnx("unable to open %s", path);
		return (NULL);
	}

	iftype =  r_identify_if(fd);
	switch (iftype) {
	case DEVICE_IF_UNKNOWN:
		debug("cannot determine interface type on %s", path);
		close(fd);
		errno = ENOTSUP;
		return (NULL);
	case DEVICE_IF_EVDEV:
		type = r_identify_evdev(fd);
		break;
	case DEVICE_IF_SYSMOUSE:
		type = r_identify_sysmouse(fd);
		break;
	default:
		debug("unsupported interface type: %s on %s",
		    r_if(iftype), path);
		close(fd);
		errno = ENXIO;
		return (NULL);
	}

	switch (type) {
	case DEVICE_TYPE_UNKNOWN:
		debug("cannot determine device type on %s", path);
		close(fd);
		errno = ENOTSUP;
		return (NULL);
	case DEVICE_TYPE_MOUSE:
	case DEVICE_TYPE_TOUCHPAD:
		break;
	default:
		debug("unsupported device type: %s on %s",
		    r_name(type), path);
		close(fd);
		errno = ENXIO;
		return (NULL);
	}

	memset(&dev, 0, sizeof(struct device));
	strlcpy(dev.path, path, sizeof(dev.path));
	dev.iftype = iftype;
	dev.type = type;
	switch (iftype) {
	case DEVICE_IF_EVDEV:
		err = r_init_dev_evdev(fd, &dev);
		break;
	case DEVICE_IF_SYSMOUSE:
		err = r_init_dev_sysmouse(fd, &dev);
		break;
	default:
		debug("unsupported interface type: %s on %s",
		    r_if(iftype), path);
		err = ENXIO;
	}
	if (err != 0) {
		debug("failed to initialize device: %s %s on %s",
		    r_if(iftype), r_name(type), path);
		close(fd);
		errno = err;
		return (NULL);
	}

	debug("port: %s  interface: %s  type: %s  model: %s",
	    path, r_if(iftype), r_name(type), dev.name);

	q = quirks_fetch_for_device(quirks, &dev);

	qvalid = quirks_get_bool(q, MOUSED_IGNORE_DEVICE, &ignore);
	if (qvalid && ignore) {
		debug("%s: device ignored", path);
		close(fd);
		quirks_unref(q);
		errno = EPERM;
		return (NULL);
	}

	switch (iftype) {
	case DEVICE_IF_EVDEV:
		grab = opt_grab;
		if (!grab)
			qvalid = quirks_get_bool(q, MOUSED_GRAB_DEVICE, &grab);
		if (qvalid && grab && ioctl(fd, EVIOCGRAB, 1) == -1) {
			logwarnx("failed to grab %s", path);
			err = errno;
		}
		break;
	case DEVICE_IF_SYSMOUSE:
		if (opt_resolution == MOUSE_RES_UNKNOWN && opt_rate == 0)
			break;
		if (opt_resolution != MOUSE_RES_UNKNOWN)
			dev.mode.resolution = opt_resolution;
		if (opt_resolution != 0)
			dev.mode.rate = opt_rate;
		if (ioctl(fd, MOUSE_SETMODE, &dev.mode) < 0)
			debug("failed to MOUSE_SETMODE for device %s", path);
		break;
	default:
		debug("unsupported interface type: %s on %s",
		    r_if(iftype), path);
		err = ENXIO;
	}
	if (err != 0) {
		debug("failed to initialize device: %s %s on %s",
		    r_if(iftype), r_name(type), path);
		close(fd);
		quirks_unref(q);
		errno = err;
		return (NULL);
	}

	r = calloc(1, sizeof(struct rodent));
	memcpy(&r->dev, &dev, sizeof(struct device));
	r->mfd = fd;

	EV_SET(&kev, fd, EVFILT_READ, EV_ADD, 0, 0, r);
	err = kevent(kfd, &kev, 1, NULL, 0, NULL);
	if (err == -1) {
		logwarnx("failed to register kevent on %s", path);
		close(fd);
		free(r);
		quirks_unref(q);
		return (NULL);
	}

	if (iftype == DEVICE_IF_EVDEV)
		r_init_evstate(q, &r->ev);
	r_init_buttons(q, &r->btstate, &r->e3b);
	r_init_scroll(q, &r->scroll);
	r_init_accel(q, &r->accel);
	switch (type) {
	case DEVICE_TYPE_TOUCHPAD:
		r_init_touchpad_hw(fd, q, &r->tp.hw, &r->ev);
		r_init_touchpad_info(q, &r->tp.hw, &r->tp.info);
		r_init_touchpad_accel(&r->tp.hw, &r->accel);
		r_init_touchpad_gesture(&r->tp.gest);
		break;

	case DEVICE_TYPE_MOUSE:
		r_init_drift(q, &r->drift);
		break;

	default:
		debug("unsupported device type: %s", r_name(type));
		break;
	}

	quirks_unref(q);

	SLIST_INSERT_HEAD(&rodents, r, next);

	return (r);
}

static void
r_init_all(void)
{
	char path[22] = "/dev/input/";
	DIR *dirp;
	struct dirent *dp;

	dirp = opendir("/dev/input");
	if (dirp == NULL)
		logerr(1, "Failed to open /dev/input");

	while ((dp = readdir(dirp)) != NULL) {
		if (fnmatch("event[0-9]*", dp->d_name, 0) == 0) {
			strncpy(path + 11, dp->d_name, 10);
			(void)r_init(path);
		}
	}
	(void)closedir(dirp);

	return;
}

static void
r_deinit(struct rodent *r)
{
	struct kevent ke[3];

	if (r == NULL)
		return;
	if (r->mfd != -1) {
		EV_SET(ke, r->mfd, EVFILT_READ, EV_DELETE, 0, 0, r);
		EV_SET(ke + 1, r->mfd << 1, EVFILT_TIMER, EV_DELETE, 0, 0, r);
		EV_SET(ke + 2, r->mfd << 1 | 1,
		    EVFILT_TIMER, EV_DELETE, 0, 0, r);
		kevent(kfd, ke, nitems(ke), NULL, 0, NULL);
		close(r->mfd);
	}
	SLIST_REMOVE(&rodents, r, rodent, next);
	debug("destroy device: port: %s  model: %s", r->dev.path, r->dev.name);
	free(r);
}

static void
r_deinit_all(void)
{
	while (!SLIST_EMPTY(&rodents))
		r_deinit(SLIST_FIRST(&rodents));
}

static int
r_protocol_evdev(enum device_type type, struct tpad *tp, struct evstate *ev,
    struct input_event *ie, mousestatus_t *act)
{
	const struct tpcaps *tphw = &tp->hw;
	const struct tpinfo *tpinfo = &tp->info;

	static int butmapev[8] = {	/* evdev */
	    0,
	    MOUSE_BUTTON1DOWN,
	    MOUSE_BUTTON3DOWN,
	    MOUSE_BUTTON1DOWN | MOUSE_BUTTON3DOWN,
	    MOUSE_BUTTON2DOWN,
	    MOUSE_BUTTON1DOWN | MOUSE_BUTTON2DOWN,
	    MOUSE_BUTTON2DOWN | MOUSE_BUTTON3DOWN,
	    MOUSE_BUTTON1DOWN | MOUSE_BUTTON2DOWN | MOUSE_BUTTON3DOWN
	};
	struct timespec ietime;

	/* Drop ignored codes */
	switch (ie->type) {
	case EV_REL:
		if (bit_test(ev->rel_ignore, ie->code))
			return (0);
	case EV_ABS:
		if (bit_test(ev->abs_ignore, ie->code))
			return (0);
	case EV_KEY:
		if (bit_test(ev->key_ignore, ie->code))
			return (0);
	}

	if (debug > 1)
		debug("received event 0x%02x, 0x%04x, %d",
		    ie->type, ie->code, ie->value);

	switch (ie->type) {
	case EV_REL:
		switch (ie->code) {
		case REL_X:
			ev->dx += ie->value;
			break;
		case REL_Y:
			ev->dy += ie->value;
			break;
		case REL_WHEEL:
			ev->dz += ie->value;
			break;
		case REL_HWHEEL:
			ev->dw += ie->value;
			break;
		}
		break;
	case EV_ABS:
		switch (ie->code) {
		case ABS_X:
			if (!tphw->is_mt)
				ev->dx += ie->value - ev->st.x;
			ev->st.x = ie->value;
			break;
		case ABS_Y:
			if (!tphw->is_mt)
				ev->dy += ie->value - ev->st.y;
			ev->st.y = ie->value;
			break;
		case ABS_PRESSURE:
			ev->st.p = ie->value;
			break;
		case ABS_TOOL_WIDTH:
			ev->st.w = ie->value;
			break;
		case ABS_MT_SLOT:
			if (tphw->is_mt)
				ev->slot = ie->value;
			break;
		case ABS_MT_TRACKING_ID:
			if (tphw->is_mt &&
			    ev->slot >= 0 && ev->slot < MAX_FINGERS) {
				if (ie->value != -1 && ev->mt[ev->slot].id > 0 &&
				    ie->value + 1 != ev->mt[ev->slot].id) {
					debug("tracking id changed %d->%d",
					    ie->value, ev->mt[ev->slot].id - 1);
					ev->mt[ev->slot].id = 0;
				} else
					ev->mt[ev->slot].id = ie->value + 1;
			}
			break;
		case ABS_MT_POSITION_X:
			if (tphw->is_mt &&
			    ev->slot >= 0 && ev->slot < MAX_FINGERS) {
			    	/* Find fastest finger */
			        int dx = ie->value - ev->mt[ev->slot].x;
				if (abs(dx) > abs(ev->dx))
					ev->dx = dx;
				ev->mt[ev->slot].x = ie->value;
			}
			break;
		case ABS_MT_POSITION_Y:
			if (tphw->is_mt &&
			    ev->slot >= 0 && ev->slot < MAX_FINGERS) {
			    	/* Find fastest finger */
				int dy = ie->value - ev->mt[ev->slot].y;
				if (abs(dy) > abs(ev->dy))
					ev->dy = dy;
				ev->mt[ev->slot].y = ie->value;
			}
			break;
		}
		break;
	case EV_KEY:
		switch (ie->code) {
		case BTN_TOUCH:
			ev->st.id = ie->value != 0 ? 1 : 0;
			break;
		case BTN_TOOL_FINGER:
			ev->nfingers = ie->value != 0 ? 1 : ev->nfingers;
			break;
		case BTN_TOOL_DOUBLETAP:
			ev->nfingers = ie->value != 0 ? 2 : ev->nfingers;
			break;
		case BTN_TOOL_TRIPLETAP:
			ev->nfingers = ie->value != 0 ? 3 : ev->nfingers;
			break;
		case BTN_TOOL_QUADTAP:
			ev->nfingers = ie->value != 0 ? 4 : ev->nfingers;
			break;
		case BTN_TOOL_QUINTTAP:
			ev->nfingers = ie->value != 0 ? 5 : ev->nfingers;
			break;
		case BTN_LEFT ... BTN_LEFT + 7:
			ev->buttons &= ~(1 << (ie->code - BTN_LEFT));
			ev->buttons |= ((!!ie->value) << (ie->code - BTN_LEFT));
			break;
		}
		break;
	}

	if ( ie->type != EV_SYN ||
	    (ie->code != SYN_REPORT && ie->code != SYN_DROPPED))
		return (0);

	/*
	 * assembly full package
	 */

	ietime.tv_sec = ie->time.tv_sec;
	ietime.tv_nsec = ie->time.tv_usec * 1000;

	if (!tphw->cap_pressure && ev->st.id != 0)
		ev->st.p = MAX(tpinfo->min_pressure_hi, tpinfo->tap_threshold);
	if (tphw->cap_touch && ev->st.id == 0)
		ev->st.p = 0;

	act->obutton = act->button;
	act->button = butmapev[ev->buttons & MOUSE_SYS_STDBUTTONS];
	act->button |= (ev->buttons & ~MOUSE_SYS_STDBUTTONS);

	if (type == DEVICE_TYPE_TOUCHPAD) {
		if (debug > 1)
			debug("absolute data %d,%d,%d,%d", ev->st.x, ev->st.y,
			    ev->st.p, ev->st.w);
		switch (r_gestures(tp, ev->st.x, ev->st.y, ev->st.p, ev->st.w,
		    ev->nfingers, &ietime, act)) {
		case GEST_IGNORE:
			ev->dx = 0;
			ev->dy = 0;
			ev->dz = 0;
			ev->acc_dx = ev->acc_dy = 0;
			debug("gesture IGNORE");
			break;
		case GEST_ACCUMULATE:	/* Revertable pointer movement. */
			ev->acc_dx += ev->dx;
			ev->acc_dy += ev->dy;
			debug("gesture ACCUMULATE %d,%d", ev->dx, ev->dy);
			ev->dx = 0;
			ev->dy = 0;
			break;
		case GEST_MOVE:		/* Pointer movement. */
			ev->dx += ev->acc_dx;
			ev->dy += ev->acc_dy;
			ev->acc_dx = ev->acc_dy = 0;
			debug("gesture MOVE %d,%d", ev->dx, ev->dy);
			break;
		case GEST_VSCROLL:	/* Vertical scrolling. */
			if (tpinfo->natural_scroll)
				ev->dz = -ev->dy;
			else
				ev->dz = ev->dy;
			ev->dx = -ev->acc_dx;
			ev->dy = -ev->acc_dy;
			ev->acc_dx = ev->acc_dy = 0;
			debug("gesture VSCROLL %d", ev->dz);
			break;
		case GEST_HSCROLL:	/* Horizontal scrolling. */
/*
			if (ev.dx != 0) {
				if (tpinfo->natural_scroll)
					act->button |= (ev.dx > 0)
					    ? MOUSE_BUTTON6DOWN
					    : MOUSE_BUTTON7DOWN;
				else
					act->button |= (ev.dx > 0)
					    ? MOUSE_BUTTON7DOWN
					    : MOUSE_BUTTON6DOWN;
			}
*/
			ev->dx = -ev->acc_dx;
			ev->dy = -ev->acc_dy;
			ev->acc_dx = ev->acc_dy = 0;
			debug("gesture HSCROLL %d", ev->dw);
			break;
		}
	}

	debug("assembled full packet %d,%d,%d", ev->dx, ev->dy, ev->dz);
	act->dx = ev->dx;
	act->dy = ev->dy;
	act->dz = ev->dz;
	ev->dx = ev->dy = ev->dz = ev->dw = 0;

	/* has something changed? */
	act->flags = ((act->dx || act->dy || act->dz) ? MOUSE_POSCHANGED : 0)
	    | (act->obutton ^ act->button);

	return (act->flags);
}

static int
r_protocol_sysmouse(uint8_t *pBuf, mousestatus_t *act)
{
	static int butmapmsc[8] = { /* sysmouse */
	    0,
	    MOUSE_BUTTON3DOWN,
	    MOUSE_BUTTON2DOWN,
	    MOUSE_BUTTON2DOWN | MOUSE_BUTTON3DOWN,
	    MOUSE_BUTTON1DOWN,
	    MOUSE_BUTTON1DOWN | MOUSE_BUTTON3DOWN,
	    MOUSE_BUTTON1DOWN | MOUSE_BUTTON2DOWN,
	    MOUSE_BUTTON1DOWN | MOUSE_BUTTON2DOWN | MOUSE_BUTTON3DOWN
	};

	debug("%02x %02x %02x %02x %02x %02x %02x %02x", pBuf[0], pBuf[1],
	    pBuf[2], pBuf[3], pBuf[4], pBuf[5], pBuf[6], pBuf[7]);

	if ((pBuf[0] & MOUSE_SYS_SYNCMASK) != MOUSE_SYS_SYNC)
		return (0);

	act->button = butmapmsc[(~pBuf[0]) & MOUSE_SYS_STDBUTTONS];
	act->dx =    (signed char)(pBuf[1]) + (signed char)(pBuf[3]);
	act->dy = - ((signed char)(pBuf[2]) + (signed char)(pBuf[4]));
	act->dz = ((signed char)(pBuf[5] << 1) + (signed char)(pBuf[6] << 1)) >> 1;
	act->button |= ((~pBuf[7] & MOUSE_SYS_EXTBUTTONS) << 3);

	/* has something changed? */
	act->flags = ((act->dx || act->dy || act->dz) ? MOUSE_POSCHANGED : 0)
	    | (act->obutton ^ act->button);

	return (act->flags);
}

static void
r_vscroll_detect(struct rodent *r, struct scroll *sc, mousestatus_t *act)
{
	mousestatus_t newaction;

	/* Allow middle button drags to scroll up and down */
	if (act->button == MOUSE_BUTTON2DOWN) {
		if (sc->state == SCROLL_NOTSCROLLING) {
			sc->state = SCROLL_PREPARE;
			sc->movement = sc->hmovement = 0;
			debug("PREPARING TO SCROLL");
		}
		return;
	}

	/* This isn't a middle button down... move along... */
	switch (sc->state) {
	case SCROLL_SCROLLING:
		/*
		 * We were scrolling, someone let go of button 2.
		 * Now turn autoscroll off.
		 */
		sc->state = SCROLL_NOTSCROLLING;
		debug("DONE WITH SCROLLING / %d", sc->state);
		break;
	case SCROLL_PREPARE:
		newaction = *act;

		/* We were preparing to scroll, but we never moved... */
		r_timestamp(act, &r->btstate, &r->e3b, &r->drift);
		r_statetrans(r, act, &newaction,
			     A(newaction.button & MOUSE_BUTTON1DOWN,
			       act->button & MOUSE_BUTTON3DOWN));

		/* Send middle down */
		newaction.button = MOUSE_BUTTON2DOWN;
		r_click(&newaction, &r->btstate);

		/* Send middle up */
		r_timestamp(&newaction, &r->btstate, &r->e3b, &r->drift);
		newaction.obutton = newaction.button;
		newaction.button = act->button;
		r_click(&newaction, &r->btstate);
		break;
	default:
		break;
	}
}

static void
r_vscroll(struct scroll *sc, mousestatus_t *act)
{
	switch (sc->state) {
	case SCROLL_PREPARE:
		/* Middle button down, waiting for movement threshold */
		if (act->dy == 0 && act->dx == 0)
			break;
		if (sc->enable_vert) {
			sc->movement += act->dy;
			if ((u_int)abs(sc->movement) > sc->threshold)
				sc->state = SCROLL_SCROLLING;
		}
		if (sc->enable_hor) {
			sc->hmovement += act->dx;
			if ((u_int)abs(sc->hmovement) > sc->threshold)
				sc->state = SCROLL_SCROLLING;
		}
		if (sc->state == SCROLL_SCROLLING)
			sc->movement = sc->hmovement = 0;
		break;
	case SCROLL_SCROLLING:
		if (sc->enable_vert) {
			sc->movement += act->dy;
			debug("SCROLL: %d", sc->movement);
			if (sc->movement < -(int)sc->speed) {
				/* Scroll down */
				act->dz = -1;
				sc->movement = 0;
			}
			else if (sc->movement > (int)sc->speed) {
				/* Scroll up */
				act->dz = 1;
				sc->movement = 0;
			}
		}
		if (sc->enable_hor) {
			sc->hmovement += act->dx;
			debug("HORIZONTAL SCROLL: %d", sc->hmovement);

			if (sc->hmovement < -(int)sc->speed) {
				act->dz = -2;
				sc->hmovement = 0;
			}
			else if (sc->hmovement > (int)sc->speed) {
				act->dz = 2;
				sc->hmovement = 0;
			}
		}

		/* Don't move while scrolling */
		act->dx = act->dy = 0;
		break;
	default:
		break;
	}
}

static bool
r_drift (struct drift *drift, mousestatus_t *act)
{
	struct timespec tmp;

	/* X or/and Y movement only - possibly drift */
	tssub(&drift->current_ts, &drift->last_activity, &tmp);
	if (tscmp(&tmp, &drift->after_ts, >)) {
		tssub(&drift->current_ts, &drift->since, &tmp);
		if (tscmp(&tmp, &drift->time_ts, <)) {
			drift->last.x += act->dx;
			drift->last.y += act->dy;
		} else {
			/* discard old accumulated steps (drift) */
			if (tscmp(&tmp, &drift->twotime_ts, >))
				drift->previous.x = drift->previous.y = 0;
			else
				drift->previous = drift->last;
			drift->last.x = act->dx;
			drift->last.y = act->dy;
			drift->since = drift->current_ts;
		}
		if ((u_int)abs(drift->last.x) + abs(drift->last.y) > drift->distance) {
			/* real movement, pass all accumulated steps */
			act->dx = drift->previous.x + drift->last.x;
			act->dy = drift->previous.y + drift->last.y;
			/* and reset accumulators */
			tsclr(&drift->since);
			drift->last.x = drift->last.y = 0;
			/* drift_previous will be cleared at next movement*/
			drift->last_activity = drift->current_ts;
		} else {
			return (true);	/* don't pass current movement to
					 * console driver */
		}
	}
	return (false);
}

static int
r_statetrans(struct rodent *r, mousestatus_t *a1, mousestatus_t *a2, int trans)
{
	struct e3bstate *e3b = &r->e3b;
	bool changed;
	int flags;

	a2->dx = a1->dx;
	a2->dy = a1->dy;
	a2->dz = a1->dz;
	a2->obutton = a2->button;
	a2->button = a1->button;
	a2->flags = a1->flags;
	changed = false;

	if (!e3b->enabled)
		return (false);

	if (debug > 2)
		debug("state:%d, trans:%d -> state:%d",
		    e3b->mouse_button_state, trans,
		    states[e3b->mouse_button_state].s[trans]);
	/*
	 * Avoid re-ordering button and movement events. While a button
	 * event is deferred, throw away up to BUTTON2_MAXMOVE movement
	 * events to allow for mouse jitter. If more movement events
	 * occur, then complete the deferred button events immediately.
	 */
	if ((a2->dx != 0 || a2->dy != 0) &&
	    S_DELAYED(states[e3b->mouse_button_state].s[trans])) {
		if (++e3b->mouse_move_delayed > BUTTON2_MAXMOVE) {
			e3b->mouse_move_delayed = 0;
			e3b->mouse_button_state =
			    states[e3b->mouse_button_state].s[A_TIMEOUT];
			changed = true;
		} else
			a2->dx = a2->dy = 0;
	} else
		e3b->mouse_move_delayed = 0;
	if (e3b->mouse_button_state != states[e3b->mouse_button_state].s[trans])
		changed = true;
	if (changed)
		clock_gettime(CLOCK_MONOTONIC_FAST,
		   &e3b->mouse_button_state_ts);
	e3b->mouse_button_state = states[e3b->mouse_button_state].s[trans];
	a2->button &= ~(MOUSE_BUTTON1DOWN | MOUSE_BUTTON2DOWN |
	    MOUSE_BUTTON3DOWN);
	a2->button &= states[e3b->mouse_button_state].mask;
	a2->button |= states[e3b->mouse_button_state].buttons;
	flags = a2->flags & MOUSE_POSCHANGED;
	flags |= a2->obutton ^ a2->button;
	if (flags & MOUSE_BUTTON2DOWN) {
		a2->flags = flags & MOUSE_BUTTON2DOWN;
		r_timestamp(a2, &r->btstate, e3b, &r->drift);
	}
	a2->flags = flags;

	return (changed);
}

static char *
skipspace(char *s)
{
	while(isspace(*s))
		++s;
	return (s);
}

static bool
r_installmap(char *arg, struct btstate *bt)
{
	u_long pbutton;
	u_long lbutton;
	char *s;

	while (*arg) {
		arg = skipspace(arg);
		s = arg;
		while (isdigit(*arg))
			++arg;
		arg = skipspace(arg);
		if ((arg <= s) || (*arg != '='))
			return (false);
		lbutton = strtoul(s, NULL, 10);

		arg = skipspace(++arg);
		s = arg;
		while (isdigit(*arg))
			++arg;
		if ((arg <= s) || (!isspace(*arg) && (*arg != '\0')))
			return (false);
		pbutton = strtoul(s, NULL, 10);

		if (lbutton == 0 || lbutton > MOUSE_MAXBUTTON)
			return (false);
		if (pbutton == 0 || pbutton > MOUSE_MAXBUTTON)
			return (false);
		bt->p2l[pbutton - 1] = 1 << (lbutton - 1);
		bt->mstate[lbutton - 1] = &bt->bstate[pbutton - 1];
	}

	return (true);
}

static char *
r_installzmap(char **argv, int argc, int* idx, struct btstate *bt)
{
	char *arg, *errstr;
	u_long i, j;

	arg = argv[*idx];
	++*idx;
	if (strcmp(arg, "x") == 0) {
		bt->zmap[0] = MOUSE_XAXIS;
		return (NULL);
	}
	if (strcmp(arg, "y") == 0) {
		bt->zmap[0] = MOUSE_YAXIS;
		return (NULL);
	}
	i = strtoul(arg, NULL, 10);
	/*
	 * Use button i for negative Z axis movement and
	 * button (i + 1) for positive Z axis movement.
	 */
	if (i == 0 || i >= MOUSE_MAXBUTTON) {
		asprintf(&errstr, "invalid argument `%s'", arg);
		return (errstr);
	}
	bt->zmap[0] = i;
	bt->zmap[1] = i + 1;
	debug("optind: %d, optarg: '%s'", *idx, arg);
	for (j = 1; j < ZMAP_MAXBUTTON; ++j) {
		if ((*idx >= argc) || !isdigit(*argv[*idx]))
			break;
		i = strtoul(argv[*idx], NULL, 10);
		if (i == 0 || i >= MOUSE_MAXBUTTON) {
			asprintf(&errstr, "invalid argument `%s'", argv[*idx]);
			return (errstr);
		}
		bt->zmap[j] = i;
		++*idx;
	}
	if ((bt->zmap[2] != 0) && (bt->zmap[3] == 0))
		bt->zmap[3] = bt->zmap[2] + 1;

	return (NULL);
}

static void
r_map(mousestatus_t *act1, mousestatus_t *act2, struct btstate *bt)
{
	int pb;
	int pbuttons;
	int lbuttons;

	pbuttons = act1->button;
	lbuttons = 0;

	act2->obutton = act2->button;
	if (pbuttons & bt->wmode) {
		pbuttons &= ~bt->wmode;
		act1->dz = act1->dy;
		act1->dx = 0;
		act1->dy = 0;
	}
	act2->dx = act1->dx;
	act2->dy = act1->dy;
	act2->dz = act1->dz;

	switch (bt->zmap[0]) {
	case 0:	/* do nothing */
		break;
	case MOUSE_XAXIS:
		if (act1->dz != 0) {
			act2->dx = act1->dz;
			act2->dz = 0;
		}
		break;
	case MOUSE_YAXIS:
		if (act1->dz != 0) {
			act2->dy = act1->dz;
			act2->dz = 0;
		}
		break;
	default:	/* buttons */
		pbuttons &= ~(bt->zmap[0] | bt->zmap[1]
			    | bt->zmap[2] | bt->zmap[3]);
		if ((act1->dz < -1) && bt->zmap[2]) {
			pbuttons |= bt->zmap[2];
			bt->zstate[2].count = 1;
		} else if (act1->dz < 0) {
			pbuttons |= bt->zmap[0];
			bt->zstate[0].count = 1;
		} else if ((act1->dz > 1) && bt->zmap[3]) {
			pbuttons |= bt->zmap[3];
			bt->zstate[3].count = 1;
		} else if (act1->dz > 0) {
			pbuttons |= bt->zmap[1];
			bt->zstate[1].count = 1;
		}
		act2->dz = 0;
		break;
	}

	for (pb = 0; (pb < MOUSE_MAXBUTTON) && (pbuttons != 0); ++pb) {
		lbuttons |= (pbuttons & 1) ? bt->p2l[pb] : 0;
		pbuttons >>= 1;
	}
	act2->button = lbuttons;

	act2->flags =
	    ((act2->dx || act2->dy || act2->dz) ? MOUSE_POSCHANGED : 0)
	    | (act2->obutton ^ act2->button);
}

static void
r_timestamp(mousestatus_t *act, struct btstate *bt, struct e3bstate *e3b,
    struct drift *drift)
{
	struct timespec ts;
	struct timespec ts1;
	struct timespec ts2;
	int button;
	int mask;
	int i;

	mask = act->flags & MOUSE_BUTTONS;
#if 0
	if (mask == 0)
		return;
#endif

	clock_gettime(CLOCK_MONOTONIC_FAST, &ts1);
	drift->current_ts = ts1;

	/* double click threshold */
	ts = tssubms(&ts1, bt->clickthreshold);
	debug("ts:  %jd %ld", (intmax_t)ts.tv_sec, ts.tv_nsec);

	/* 3 button emulation timeout */
	ts2 = tssubms(&ts1, e3b->button2timeout);

	button = MOUSE_BUTTON1DOWN;
	for (i = 0; (i < MOUSE_MAXBUTTON) && (mask != 0); ++i) {
		if (mask & 1) {
			if (act->button & button) {
				/* the button is down */
				debug("  :  %jd %ld",
				    (intmax_t)bt->bstate[i].ts.tv_sec,
				    bt->bstate[i].ts.tv_nsec);
				if (tscmp(&ts, &bt->bstate[i].ts, >)) {
					bt->bstate[i].count = 1;
				} else {
					++bt->bstate[i].count;
				}
				bt->bstate[i].ts = ts1;
			} else {
				/* the button is up */
				bt->bstate[i].ts = ts1;
			}
		} else {
			if (act->button & button) {
				/* the button has been down */
				if (tscmp(&ts2, &bt->bstate[i].ts, >)) {
					bt->bstate[i].count = 1;
					bt->bstate[i].ts = ts1;
					act->flags |= button;
					debug("button %d timeout", i + 1);
				}
			} else {
				/* the button has been up */
			}
		}
		button <<= 1;
		mask >>= 1;
	}
}

static bool
r_timeout(struct e3bstate *e3b)
{
	struct timespec ts;
	struct timespec ts1;

	if (states[e3b->mouse_button_state].timeout)
		return (true);
	clock_gettime(CLOCK_MONOTONIC_FAST, &ts1);
	ts = tssubms(&ts1, e3b->button2timeout);
	return (tscmp(&ts, &e3b->mouse_button_state_ts, >));
}

static void
r_move(mousestatus_t *act, struct accel *acc)
{
	struct mouse_info mouse;

	bzero(&mouse, sizeof(mouse));
	if (acc->is_exponential) {
		expoacc(acc, act->dx, act->dy, act->dz,
		    &mouse.u.data.x, &mouse.u.data.y, &mouse.u.data.z);
	} else {
		linacc(acc, act->dx, act->dy, act->dz,
		    &mouse.u.data.x, &mouse.u.data.y, &mouse.u.data.z);
	}
	mouse.operation = MOUSE_MOTION_EVENT;
	mouse.u.data.buttons = act->button;
	if (debug < 2 && !paused)
		ioctl(cfd, CONS_MOUSECTL, &mouse);
}

static void
r_click(mousestatus_t *act, struct btstate *bt)
{
	struct mouse_info mouse;
	int button;
	int mask;
	int i;

	mask = act->flags & MOUSE_BUTTONS;
	if (mask == 0)
		return;

	button = MOUSE_BUTTON1DOWN;
	for (i = 0; (i < MOUSE_MAXBUTTON) && (mask != 0); ++i) {
		if (mask & 1) {
			debug("mstate[%d]->count:%d", i, bt->mstate[i]->count);
			if (act->button & button) {
				/* the button is down */
				mouse.u.event.value = bt->mstate[i]->count;
			} else {
				/* the button is up */
				mouse.u.event.value = 0;
			}
			mouse.operation = MOUSE_BUTTON_EVENT;
			mouse.u.event.id = button;
			if (debug < 2 && !paused)
				ioctl(cfd, CONS_MOUSECTL, &mouse);
			debug("button %d  count %d", i + 1,
			    mouse.u.event.value);
		}
		button <<= 1;
		mask >>= 1;
	}
}

static enum gesture
r_gestures(struct tpad *tp, int x0, int y0, u_int z, int w, int nfingers,
    struct timespec *time, mousestatus_t *ms)
{
	struct tpstate *gest = &tp->gest;
	const struct tpcaps *tphw = &tp->hw;
	const struct tpinfo *tpinfo = &tp->info;
	int tap_timeout = tpinfo->tap_timeout;

	/*
	 * Check pressure to detect a real wanted action on the
	 * touchpad.
	 */
	if (z >= tpinfo->min_pressure_hi ||
	    (gest->fingerdown && z >= tpinfo->min_pressure_lo)) {
		/* XXX Verify values? */
		bool two_finger_scroll = tpinfo->two_finger_scroll;
		bool three_finger_drag = tpinfo->three_finger_drag;
		int max_width = tpinfo->max_width;
		u_int max_pressure = tpinfo->max_pressure;
		int margin_top = tpinfo->margin_top;
		int margin_right = tpinfo->margin_right;
		int margin_bottom = tpinfo->margin_bottom;
		int margin_left = tpinfo->margin_left;
		int vscroll_hor_area = tpinfo->vscroll_hor_area * tphw->res_x;
		int vscroll_ver_area = tpinfo->vscroll_ver_area * tphw->res_y;;

		int max_x = tphw->max_x;
		int max_y = tphw->max_y;
		int min_x = tphw->min_x;
		int min_y = tphw->min_y;

		int dx, dy;
		int start_x, start_y;
		int tap_max_delta_x, tap_max_delta_y;
		int prev_nfingers;

		/* Palm detection. */
		if (nfingers == 1 &&
		    ((tphw->cap_width && w > max_width) ||
		     (tphw->cap_pressure && z > max_pressure))) {
			/*
			 * We consider the packet irrelevant for the current
			 * action when:
			 *  - there is a single active touch
			 *  - the width isn't comprised in:
			 *    [0; max_width]
			 *  - the pressure isn't comprised in:
			 *    [min_pressure; max_pressure]
			 *
			 *  Note that this doesn't terminate the current action.
			 */
			debug("palm detected! (%d)", z);
			return(GEST_IGNORE);
		}

		/*
		 * Limit the coordinates to the specified margins because
		 * this area isn't very reliable.
		 */
		if (margin_left != 0 && x0 <= min_x + margin_left)
			x0 = min_x + margin_left;
		else if (margin_right != 0 && x0 >= max_x - margin_right)
			x0 = max_x - margin_right;
		if (margin_bottom != 0 && y0 <= min_y + margin_bottom)
			y0 = min_y + margin_bottom;
		else if (margin_top != 0 && y0 >= max_y - margin_top)
			y0 = max_y - margin_top;

		debug("packet: [%d, %d], %d, %d", x0, y0, z, w);

		/*
		 * If the action is just beginning, init the structure and
		 * compute tap timeout.
		 */
		if (!gest->fingerdown) {
			debug("----");

			/* Reset pressure peak. */
			gest->zmax = 0;

			/* Reset fingers count. */
			gest->fingers_nb = 0;

			/* Reset virtual scrolling state. */
			gest->in_vscroll = 0;

			/* Compute tap timeout. */
			if (tap_timeout != 0)
				gest->taptimeout = tsaddms(time, tap_timeout);
			else
				tsclr(&gest->taptimeout);

			gest->fingerdown = true;

			gest->start_x = x0;
			gest->start_y = y0;
		}

		prev_nfingers = gest->prev_nfingers;

		gest->prev_x = x0;
		gest->prev_y = y0;
		gest->prev_nfingers = nfingers;

		start_x = gest->start_x;
		start_y = gest->start_y;

		/* Process ClickPad softbuttons */
		if (tphw->is_clickpad && ms->button & MOUSE_BUTTON1DOWN) {
			int y_ok, center_bt, center_x, right_bt, right_x;
			y_ok = tpinfo->softbuttons_y < 0
			    ? start_y < min_y - tpinfo->softbuttons_y
			    : start_y > max_y - tpinfo->softbuttons_y;

			center_bt = MOUSE_BUTTON2DOWN;
			center_x = min_x + tpinfo->softbutton2_x;
			right_bt = MOUSE_BUTTON3DOWN;
			right_x = min_x + tpinfo->softbutton3_x;

			if (center_x > 0 && right_x > 0 && center_x > right_x) {
				center_bt = MOUSE_BUTTON3DOWN;
				center_x = min_x + tpinfo->softbutton3_x;
				right_bt = MOUSE_BUTTON2DOWN;
				right_x = min_x + tpinfo->softbutton2_x;
			}

			if (right_x > 0 && start_x > right_x && y_ok)
				ms->button = (ms->button &
				    ~MOUSE_BUTTON1DOWN) | right_bt;
			else if (center_x > 0 && start_x > center_x && y_ok)
				ms->button = (ms->button &
				    ~MOUSE_BUTTON1DOWN) | center_bt;
		}

		/* If in tap-hold or three fingers, add the recorded button. */
		if (gest->in_taphold || (nfingers == 3 && three_finger_drag))
			ms->button |= gest->tap_button;

		/*
		 * For tap, we keep the maximum number of fingers and the
		 * pressure peak.
		 */
		gest->fingers_nb = MAX(nfingers, gest->fingers_nb);
		gest->zmax = MAX(z, gest->zmax);

		dx = abs(x0 - start_x);
		dy = abs(y0 - start_y);

		/*
		 * A scrolling action must not conflict with a tap action.
		 * Here are the conditions to consider a scrolling action:
		 *  - the action in a configurable area
		 *  - one of the following:
		 *     . the distance between the last packet and the
		 *       first should be above a configurable minimum
		 *     . tap timed out
		 */
		if (!gest->in_taphold && !ms->button &&
		    (!gest->in_vscroll || two_finger_scroll) &&
		    (tscmp(time, &gest->taptimeout, >) ||
		    ((gest->fingers_nb == 2 || !two_finger_scroll) &&
		    (dx >= tpinfo->vscroll_min_delta * tphw->res_x ||
		     dy >= tpinfo->vscroll_min_delta * tphw->res_y)))) {
			/*
			 * Handle two finger scrolling.
			 * Note that we don't rely on fingers_nb
			 * as that keeps the maximum number of fingers.
			 */
			if (two_finger_scroll) {
				if (nfingers == 2) {
					gest->in_vscroll += dy ? 2 : 0;
					gest->in_vscroll += dx ? 1 : 0;
				}
			} else {
				/* Check for horizontal scrolling. */
				if ((vscroll_hor_area > 0 &&
				     start_y <= min_y + vscroll_hor_area) ||
				    (vscroll_hor_area < 0 &&
				     start_y >= max_y + vscroll_hor_area))
					gest->in_vscroll += 2;

				/* Check for vertical scrolling. */
				if ((vscroll_ver_area > 0 &&
				     start_x <= min_x + vscroll_ver_area) ||
				    (vscroll_ver_area < 0 &&
				     start_x >= max_x + vscroll_ver_area))
					gest->in_vscroll += 1;
			}
			/* Avoid conflicts if area overlaps. */
			if (gest->in_vscroll >= 3)
				gest->in_vscroll = (dx > dy) ? 2 : 1;
		}
		/*
		 * Reset two finger scrolling when the number of fingers
		 * is different from two or any button is pressed.
		 */
		if (two_finger_scroll && gest->in_vscroll != 0 &&
		    (nfingers != 2 || ms->button))
			gest->in_vscroll = 0;

		debug("virtual scrolling: %s "
			"(direction=%d, dx=%d, dy=%d, fingers=%d)",
			gest->in_vscroll != 0 ? "YES" : "NO",
			gest->in_vscroll, dx, dy, gest->fingers_nb);

		/* Workaround cursor jump on finger set changes */
		if (prev_nfingers != nfingers)
			return (GEST_IGNORE);

		switch (gest->in_vscroll) {
		case 1:
			return (GEST_VSCROLL);
		case 2:
			return (GEST_HSCROLL);
		default:
			/* NO-OP */;
		}

		/* Max delta is disabled for multi-fingers tap. */
		if (gest->fingers_nb == 1 &&
		    tscmp(time, &gest->taptimeout, <=)) {
			tap_max_delta_x = tpinfo->tap_max_delta * tphw->res_x;
			tap_max_delta_y = tpinfo->tap_max_delta * tphw->res_y;

			debug("dx=%d, dy=%d, deltax=%d, deltay=%d",
			    dx, dy, tap_max_delta_x, tap_max_delta_y);
			if (dx > tap_max_delta_x || dy > tap_max_delta_y) {
				debug("not a tap");
				tsclr(&gest->taptimeout);
			}
		}

		if (tscmp(time, &gest->taptimeout, <=))
			return (gest->fingers_nb > 1 ?
			    GEST_IGNORE : GEST_ACCUMULATE);
		else
			return (GEST_MOVE);
	}

	/*
	 * Handle a case when clickpad pressure drops before than
	 * button up event when surface is released after click.
	 * It interferes with softbuttons.
	 */
	if (tphw->is_clickpad && tpinfo->softbuttons_y != 0)
		ms->button &= ~MOUSE_BUTTON1DOWN;

	gest->prev_nfingers = 0;

	if (gest->fingerdown) {
		/*
		 * An action is currently taking place but the pressure
		 * dropped under the minimum, putting an end to it.
		 */

		gest->fingerdown = false;

		/* Check for tap. */
		debug("zmax=%d fingers=%d", gest->zmax, gest->fingers_nb);
		if (!gest->in_vscroll && gest->zmax >= tpinfo->tap_threshold &&
		    tscmp(time, &gest->taptimeout, <=)) {
			/*
			 * We have a tap if:
			 *   - the maximum pressure went over tap_threshold
			 *   - the action ended before tap_timeout
			 *
			 * To handle tap-hold, we must delay any button push to
			 * the next action.
			 */
			if (gest->in_taphold) {
				/*
				 * This is the second and last tap of a
				 * double tap action, not a tap-hold.
				 */
				gest->in_taphold = false;

				/*
				 * For double-tap to work:
				 *   - no button press is emitted (to
				 *     simulate a button release)
				 *   - PSM_FLAGS_FINGERDOWN is set to
				 *     force the next packet to emit a
				 *     button press)
				 */
				debug("button RELEASE: %d", gest->tap_button);
				gest->fingerdown = true;

				/* Schedule button press on next event */
				gest->idletimeout = 0;
			} else {
				/*
				 * This is the first tap: we set the
				 * tap-hold state and notify the button
				 * down event.
				 */
				gest->in_taphold = true;
				gest->idletimeout = tpinfo->taphold_timeout;
				gest->taptimeout = tsaddms(time, tap_timeout);

				switch (gest->fingers_nb) {
				case 3:
					gest->tap_button =
					    MOUSE_BUTTON2DOWN;
					break;
				case 2:
					gest->tap_button =
					    MOUSE_BUTTON3DOWN;
					break;
				default:
					gest->tap_button =
					    MOUSE_BUTTON1DOWN;
				}
				debug("button PRESS: %d", gest->tap_button);
				ms->button |= gest->tap_button;
			}
		} else {
			/*
			 * Not enough pressure or timeout: reset
			 * tap-hold state.
			 */
			if (gest->in_taphold) {
				debug("button RELEASE: %d", gest->tap_button);
				gest->in_taphold = false;
			} else {
				debug("not a tap-hold");
			}
		}
	} else if (!gest->fingerdown && gest->in_taphold) {
		/*
		 * For a tap-hold to work, the button must remain down at
		 * least until timeout (where the in_taphold flags will be
		 * cleared) or during the next action.
		 */
		if (tscmp(time, &gest->taptimeout, <=)) {
			ms->button |= gest->tap_button;
		} else {
			debug("button RELEASE: %d", gest->tap_button);
			gest->in_taphold = false;
		}
	}

	return (GEST_IGNORE);
}
