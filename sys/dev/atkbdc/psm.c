/*-
 * Copyright (c) 1992, 1993 Erik Forsberg.
 * Copyright (c) 1996, 1997 Kazutaka YOKOTA.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * THIS SOFTWARE IS PROVIDED BY ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 * NO EVENT SHALL I BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 *  Ported to 386bsd Oct 17, 1992
 *  Sandi Donno, Computer Science, University of Cape Town, South Africa
 *  Please send bug reports to sandi@cs.uct.ac.za
 *
 *  Thanks are also due to Rick Macklem, rick@snowhite.cis.uoguelph.ca -
 *  although I was only partially successful in getting the alpha release
 *  of his "driver for the Logitech and ATI Inport Bus mice for use with
 *  386bsd and the X386 port" to work with my Microsoft mouse, I nevertheless
 *  found his code to be an invaluable reference when porting this driver
 *  to 386bsd.
 *
 *  Further modifications for latest 386BSD+patchkit and port to NetBSD,
 *  Andrew Herbert <andrew@werple.apana.org.au> - 8 June 1993
 *
 *  Cloned from the Microsoft Bus Mouse driver, also by Erik Forsberg, by
 *  Andrew Herbert - 12 June 1993
 *
 *  Modified for PS/2 mouse by Charles Hannum <mycroft@ai.mit.edu>
 *  - 13 June 1993
 *
 *  Modified for PS/2 AUX mouse by Shoji Yuen <yuen@nuie.nagoya-u.ac.jp>
 *  - 24 October 1993
 *
 *  Hardware access routines and probe logic rewritten by
 *  Kazutaka Yokota <yokota@zodiac.mech.utsunomiya-u.ac.jp>
 *  - 3, 14, 22 October 1996.
 *  - 12 November 1996. IOCTLs and rearranging `psmread', `psmioctl'...
 *  - 14, 30 November 1996. Uses `kbdio.c'.
 *  - 13 December 1996. Uses queuing version of `kbdio.c'.
 *  - January/February 1997. Tweaked probe logic for
 *    HiNote UltraII/Latitude/Armada laptops.
 *  - 30 July 1997. Added APM support.
 *  - 5 March 1997. Defined driver configuration flags (PSM_CONFIG_XXX).
 *    Improved sync check logic.
 *    Vendor specific support routines.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/dev/atkbdc/psm.c,v 1.93.2.3.2.1 2008/11/25 02:59:29 kensmith Exp $");

#include "opt_isa.h"
#include "opt_psm.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/poll.h>
#include <sys/syslog.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <sys/selinfo.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/uio.h>

#include <sys/limits.h>
#include <sys/mouse.h>
#include <machine/resource.h>

#ifdef DEV_ISA
#include <isa/isavar.h>
#endif

#include <dev/atkbdc/atkbdcreg.h>
#include <dev/atkbdc/psm.h>

/*
 * Driver specific options: the following options may be set by
 * `options' statements in the kernel configuration file.
 */

/* debugging */
#ifndef PSM_DEBUG
#define	PSM_DEBUG	0	/*
				 * logging: 0: none, 1: brief, 2: verbose
				 *          3: sync errors, 4: all packets
				 */
#endif
#define	VLOG(level, args)	do {	\
	if (verbose >= level)		\
		log args;		\
} while (0)

#ifndef PSM_INPUT_TIMEOUT
#define	PSM_INPUT_TIMEOUT	2000000	/* 2 sec */
#endif

#ifndef PSM_TAP_TIMEOUT
#define	PSM_TAP_TIMEOUT		125000
#endif

#ifndef PSM_TAP_THRESHOLD
#define	PSM_TAP_THRESHOLD	25
#endif

/* end of driver specific options */

#define	PSMCPNP_DRIVER_NAME	"psmcpnp"

/* input queue */
#define	PSM_BUFSIZE		960
#define	PSM_SMALLBUFSIZE	240

/* operation levels */
#define	PSM_LEVEL_BASE		0
#define	PSM_LEVEL_STANDARD	1
#define	PSM_LEVEL_NATIVE	2
#define	PSM_LEVEL_MIN		PSM_LEVEL_BASE
#define	PSM_LEVEL_MAX		PSM_LEVEL_NATIVE

/* Logitech PS2++ protocol */
#define	MOUSE_PS2PLUS_CHECKBITS(b)	\
    ((((b[2] & 0x03) << 2) | 0x02) == (b[1] & 0x0f))
#define	MOUSE_PS2PLUS_PACKET_TYPE(b)	\
    (((b[0] & 0x30) >> 2) | ((b[1] & 0x30) >> 4))

/* some macros */
#define	PSM_UNIT(dev)		(minor(dev) >> 1)
#define	PSM_NBLOCKIO(dev)	(minor(dev) & 1)
#define	PSM_MKMINOR(unit,block)	(((unit) << 1) | ((block) ? 0:1))

/* ring buffer */
typedef struct ringbuf {
	int		count;	/* # of valid elements in the buffer */
	int		head;	/* head pointer */
	int		tail;	/* tail poiner */
	u_char buf[PSM_BUFSIZE];
} ringbuf_t;

/* data buffer */
typedef struct packetbuf {
	u_char	ipacket[16];	/* interim input buffer */
	int	inputbytes;	/* # of bytes in the input buffer */
} packetbuf_t;

#ifndef PSM_PACKETQUEUE
#define	PSM_PACKETQUEUE	128
#endif

typedef struct synapticsinfo {
	struct sysctl_ctx_list	sysctl_ctx;
	struct sysctl_oid	*sysctl_tree;
	int			directional_scrolls;
	int			low_speed_threshold;
	int			min_movement;
	int			squelch_level;
} synapticsinfo_t;

/* driver control block */
struct psm_softc {		/* Driver status information */
	int		unit;
	struct selinfo	rsel;		/* Process selecting for Input */
	u_char		state;		/* Mouse driver state */
	int		config;		/* driver configuration flags */
	int		flags;		/* other flags */
	KBDC		kbdc;		/* handle to access kbd controller */
	struct resource	*intr;		/* IRQ resource */
	void		*ih;		/* interrupt handle */
	mousehw_t	hw;		/* hardware information */
	synapticshw_t	synhw;		/* Synaptics hardware information */
	synapticsinfo_t	syninfo;	/* Synaptics configuration */
	mousemode_t	mode;		/* operation mode */
	mousemode_t	dflt_mode;	/* default operation mode */
	mousestatus_t	status;		/* accumulated mouse movement */
	ringbuf_t	queue;		/* mouse status queue */
	packetbuf_t	pqueue[PSM_PACKETQUEUE]; /* mouse data queue */
	int		pqueue_start;	/* start of data in queue */
	int		pqueue_end;	/* end of data in queue */
	int		button;		/* the latest button state */
	int		xold;		/* previous absolute X position */
	int		yold;		/* previous absolute Y position */
	int		xaverage;	/* average X position */
	int		yaverage;	/* average Y position */
	int		squelch; /* level to filter movement at low speed */
	int		zmax;	/* maximum pressure value for touchpads */
	int		syncerrors; /* # of bytes discarded to synchronize */
	int		pkterrors;  /* # of packets failed during quaranteen. */
	struct timeval	inputtimeout;
	struct timeval	lastsoftintr;	/* time of last soft interrupt */
	struct timeval	lastinputerr;	/* time last sync error happened */
	struct timeval	taptimeout;	/* tap timeout for touchpads */
	int		watchdog;	/* watchdog timer flag */
	struct callout_handle callout;	/* watchdog timer call out */
	struct callout_handle softcallout; /* buffer timer call out */
	struct cdev	*dev;
	struct cdev	*bdev;
	int		lasterr;
	int		cmdcount;
};
static devclass_t psm_devclass;
#define	PSM_SOFTC(unit)	\
    ((struct psm_softc*)devclass_get_softc(psm_devclass, unit))

/* driver state flags (state) */
#define	PSM_VALID		0x80
#define	PSM_OPEN		1	/* Device is open */
#define	PSM_ASLP		2	/* Waiting for mouse data */
#define	PSM_SOFTARMED		4	/* Software interrupt armed */
#define	PSM_NEED_SYNCBITS	8	/* Set syncbits using next data pkt */

/* driver configuration flags (config) */
#define	PSM_CONFIG_RESOLUTION	0x000f	/* resolution */
#define	PSM_CONFIG_ACCEL	0x00f0  /* acceleration factor */
#define	PSM_CONFIG_NOCHECKSYNC	0x0100  /* disable sync. test */
#define	PSM_CONFIG_NOIDPROBE	0x0200  /* disable mouse model probe */
#define	PSM_CONFIG_NORESET	0x0400  /* don't reset the mouse */
#define	PSM_CONFIG_FORCETAP	0x0800  /* assume `tap' action exists */
#define	PSM_CONFIG_IGNPORTERROR	0x1000  /* ignore error in aux port test */
#define	PSM_CONFIG_HOOKRESUME	0x2000	/* hook the system resume event */
#define	PSM_CONFIG_INITAFTERSUSPEND 0x4000 /* init the device at the resume event */
#define	PSM_CONFIG_SYNCHACK	0x8000	/* enable `out-of-sync' hack */

#define	PSM_CONFIG_FLAGS	\
    (PSM_CONFIG_RESOLUTION |	\
    PSM_CONFIG_ACCEL |		\
    PSM_CONFIG_NOCHECKSYNC |	\
    PSM_CONFIG_SYNCHACK |	\
    PSM_CONFIG_NOIDPROBE |	\
    PSM_CONFIG_NORESET |	\
    PSM_CONFIG_FORCETAP |	\
    PSM_CONFIG_IGNPORTERROR |	\
    PSM_CONFIG_HOOKRESUME |	\
    PSM_CONFIG_INITAFTERSUSPEND)

/* other flags (flags) */
#define	PSM_FLAGS_FINGERDOWN	0x0001	/* VersaPad finger down */

/* Tunables */
static int synaptics_support = 0;
TUNABLE_INT("hw.psm.synaptics_support", &synaptics_support);

static int verbose = PSM_DEBUG;
TUNABLE_INT("debug.psm.loglevel", &verbose);

/* for backward compatibility */
#define	OLD_MOUSE_GETHWINFO	_IOR('M', 1, old_mousehw_t)
#define	OLD_MOUSE_GETMODE	_IOR('M', 2, old_mousemode_t)
#define	OLD_MOUSE_SETMODE	_IOW('M', 3, old_mousemode_t)

typedef struct old_mousehw {
	int	buttons;
	int	iftype;
	int	type;
	int	hwid;
} old_mousehw_t;

typedef struct old_mousemode {
	int	protocol;
	int	rate;
	int	resolution;
	int	accelfactor;
} old_mousemode_t;

/* packet formatting function */
typedef int	packetfunc_t(struct psm_softc *, u_char *, int *, int,
    mousestatus_t *);

/* function prototypes */
static void	psmidentify(driver_t *, device_t);
static int	psmprobe(device_t);
static int	psmattach(device_t);
static int	psmdetach(device_t);
static int	psmresume(device_t);

static d_open_t		psmopen;
static d_close_t	psmclose;
static d_read_t		psmread;
static d_write_t	psmwrite;
static d_ioctl_t	psmioctl;
static d_poll_t		psmpoll;

static int	enable_aux_dev(KBDC);
static int	disable_aux_dev(KBDC);
static int	get_mouse_status(KBDC, int *, int, int);
static int	get_aux_id(KBDC);
static int	set_mouse_sampling_rate(KBDC, int);
static int	set_mouse_scaling(KBDC, int);
static int	set_mouse_resolution(KBDC, int);
static int	set_mouse_mode(KBDC);
static int	get_mouse_buttons(KBDC);
static int	is_a_mouse(int);
static void	recover_from_error(KBDC);
static int	restore_controller(KBDC, int);
static int	doinitialize(struct psm_softc *, mousemode_t *);
static int	doopen(struct psm_softc *, int);
static int	reinitialize(struct psm_softc *, int);
static char	*model_name(int);
static void	psmsoftintr(void *);
static void	psmintr(void *);
static void	psmtimeout(void *);
static int	timeelapsed(const struct timeval *, int, int,
		    const struct timeval *);
static void	dropqueue(struct psm_softc *);
static void	flushpackets(struct psm_softc *);
static void	proc_mmanplus(struct psm_softc *, packetbuf_t *,
		    mousestatus_t *, int *, int *, int *);
static int	proc_synaptics(struct psm_softc *, packetbuf_t *,
		    mousestatus_t *, int *, int *, int *);
static void	proc_versapad(struct psm_softc *, packetbuf_t *,
		    mousestatus_t *, int *, int *, int *);
static int	tame_mouse(struct psm_softc *, packetbuf_t *, mousestatus_t *,
		    u_char *);

/* vendor specific features */
typedef int	probefunc_t(struct psm_softc *);

static int	mouse_id_proc1(KBDC, int, int, int *);
static int	mouse_ext_command(KBDC, int);

static probefunc_t	enable_groller;
static probefunc_t	enable_gmouse;
static probefunc_t	enable_aglide;
static probefunc_t	enable_kmouse;
static probefunc_t	enable_msexplorer;
static probefunc_t	enable_msintelli;
static probefunc_t	enable_4dmouse;
static probefunc_t	enable_4dplus;
static probefunc_t	enable_mmanplus;
static probefunc_t	enable_synaptics;
static probefunc_t	enable_versapad;

static struct {
	int		model;
	u_char		syncmask;
	int		packetsize;
	probefunc_t	*probefunc;
} vendortype[] = {
	/*
	 * WARNING: the order of probe is very important.  Don't mess it
	 * unless you know what you are doing.
	 */
	{ MOUSE_MODEL_NET,		/* Genius NetMouse */
	  0x08, MOUSE_PS2INTELLI_PACKETSIZE, enable_gmouse },
	{ MOUSE_MODEL_NETSCROLL,	/* Genius NetScroll */
	  0xc8, 6, enable_groller },
	{ MOUSE_MODEL_MOUSEMANPLUS,	/* Logitech MouseMan+ */
	  0x08, MOUSE_PS2_PACKETSIZE, enable_mmanplus },
	{ MOUSE_MODEL_EXPLORER,		/* Microsoft IntelliMouse Explorer */
	  0x08, MOUSE_PS2INTELLI_PACKETSIZE, enable_msexplorer },
	{ MOUSE_MODEL_4D,		/* A4 Tech 4D Mouse */
	  0x08, MOUSE_4D_PACKETSIZE, enable_4dmouse },
	{ MOUSE_MODEL_4DPLUS,		/* A4 Tech 4D+ Mouse */
	  0xc8, MOUSE_4DPLUS_PACKETSIZE, enable_4dplus },
	{ MOUSE_MODEL_SYNAPTICS,	/* Synaptics Touchpad */
	  0xc0, MOUSE_SYNAPTICS_PACKETSIZE, enable_synaptics },
	{ MOUSE_MODEL_INTELLI,		/* Microsoft IntelliMouse */
	  0x08, MOUSE_PS2INTELLI_PACKETSIZE, enable_msintelli },
	{ MOUSE_MODEL_GLIDEPOINT,	/* ALPS GlidePoint */
	  0xc0, MOUSE_PS2_PACKETSIZE, enable_aglide },
	{ MOUSE_MODEL_THINK,		/* Kensington ThinkingMouse */
	  0x80, MOUSE_PS2_PACKETSIZE, enable_kmouse },
	{ MOUSE_MODEL_VERSAPAD,		/* Interlink electronics VersaPad */
	  0xe8, MOUSE_PS2VERSA_PACKETSIZE, enable_versapad },
	{ MOUSE_MODEL_GENERIC,
	  0xc0, MOUSE_PS2_PACKETSIZE, NULL },
};
#define	GENERIC_MOUSE_ENTRY	\
    ((sizeof(vendortype) / sizeof(*vendortype)) - 1)

/* device driver declarateion */
static device_method_t psm_methods[] = {
	/* Device interface */
	DEVMETHOD(device_identify,	psmidentify),
	DEVMETHOD(device_probe,		psmprobe),
	DEVMETHOD(device_attach,	psmattach),
	DEVMETHOD(device_detach,	psmdetach),
	DEVMETHOD(device_resume,	psmresume),

	{ 0, 0 }
};

static driver_t psm_driver = {
	PSM_DRIVER_NAME,
	psm_methods,
	sizeof(struct psm_softc),
};

static struct cdevsw psm_cdevsw = {
	.d_version =	D_VERSION,
	.d_flags =	D_NEEDGIANT,
	.d_open =	psmopen,
	.d_close =	psmclose,
	.d_read =	psmread,
	.d_write =	psmwrite,
	.d_ioctl =	psmioctl,
	.d_poll =	psmpoll,
	.d_name =	PSM_DRIVER_NAME,
};

/* device I/O routines */
static int
enable_aux_dev(KBDC kbdc)
{
	int res;

	res = send_aux_command(kbdc, PSMC_ENABLE_DEV);
	VLOG(2, (LOG_DEBUG, "psm: ENABLE_DEV return code:%04x\n", res));

	return (res == PSM_ACK);
}

static int
disable_aux_dev(KBDC kbdc)
{
	int res;

	res = send_aux_command(kbdc, PSMC_DISABLE_DEV);
	VLOG(2, (LOG_DEBUG, "psm: DISABLE_DEV return code:%04x\n", res));

	return (res == PSM_ACK);
}

static int
get_mouse_status(KBDC kbdc, int *status, int flag, int len)
{
	int cmd;
	int res;
	int i;

	switch (flag) {
	case 0:
	default:
		cmd = PSMC_SEND_DEV_STATUS;
		break;
	case 1:
		cmd = PSMC_SEND_DEV_DATA;
		break;
	}
	empty_aux_buffer(kbdc, 5);
	res = send_aux_command(kbdc, cmd);
	VLOG(2, (LOG_DEBUG, "psm: SEND_AUX_DEV_%s return code:%04x\n",
	    (flag == 1) ? "DATA" : "STATUS", res));
	if (res != PSM_ACK)
		return (0);

	for (i = 0; i < len; ++i) {
		status[i] = read_aux_data(kbdc);
		if (status[i] < 0)
			break;
	}

	VLOG(1, (LOG_DEBUG, "psm: %s %02x %02x %02x\n",
	    (flag == 1) ? "data" : "status", status[0], status[1], status[2]));

	return (i);
}

static int
get_aux_id(KBDC kbdc)
{
	int res;
	int id;

	empty_aux_buffer(kbdc, 5);
	res = send_aux_command(kbdc, PSMC_SEND_DEV_ID);
	VLOG(2, (LOG_DEBUG, "psm: SEND_DEV_ID return code:%04x\n", res));
	if (res != PSM_ACK)
		return (-1);

	/* 10ms delay */
	DELAY(10000);

	id = read_aux_data(kbdc);
	VLOG(2, (LOG_DEBUG, "psm: device ID: %04x\n", id));

	return (id);
}

static int
set_mouse_sampling_rate(KBDC kbdc, int rate)
{
	int res;

	res = send_aux_command_and_data(kbdc, PSMC_SET_SAMPLING_RATE, rate);
	VLOG(2, (LOG_DEBUG, "psm: SET_SAMPLING_RATE (%d) %04x\n", rate, res));

	return ((res == PSM_ACK) ? rate : -1);
}

static int
set_mouse_scaling(KBDC kbdc, int scale)
{
	int res;

	switch (scale) {
	case 1:
	default:
		scale = PSMC_SET_SCALING11;
		break;
	case 2:
		scale = PSMC_SET_SCALING21;
		break;
	}
	res = send_aux_command(kbdc, scale);
	VLOG(2, (LOG_DEBUG, "psm: SET_SCALING%s return code:%04x\n",
	    (scale == PSMC_SET_SCALING21) ? "21" : "11", res));

	return (res == PSM_ACK);
}

/* `val' must be 0 through PSMD_MAX_RESOLUTION */
static int
set_mouse_resolution(KBDC kbdc, int val)
{
	int res;

	res = send_aux_command_and_data(kbdc, PSMC_SET_RESOLUTION, val);
	VLOG(2, (LOG_DEBUG, "psm: SET_RESOLUTION (%d) %04x\n", val, res));

	return ((res == PSM_ACK) ? val : -1);
}

/*
 * NOTE: once `set_mouse_mode()' is called, the mouse device must be
 * re-enabled by calling `enable_aux_dev()'
 */
static int
set_mouse_mode(KBDC kbdc)
{
	int res;

	res = send_aux_command(kbdc, PSMC_SET_STREAM_MODE);
	VLOG(2, (LOG_DEBUG, "psm: SET_STREAM_MODE return code:%04x\n", res));

	return (res == PSM_ACK);
}

static int
get_mouse_buttons(KBDC kbdc)
{
	int c = 2;		/* assume two buttons by default */
	int status[3];

	/*
	 * NOTE: a special sequence to obtain Logitech Mouse specific
	 * information: set resolution to 25 ppi, set scaling to 1:1, set
	 * scaling to 1:1, set scaling to 1:1. Then the second byte of the
	 * mouse status bytes is the number of available buttons.
	 * Some manufactures also support this sequence.
	 */
	if (set_mouse_resolution(kbdc, PSMD_RES_LOW) != PSMD_RES_LOW)
		return (c);
	if (set_mouse_scaling(kbdc, 1) && set_mouse_scaling(kbdc, 1) &&
	    set_mouse_scaling(kbdc, 1) &&
	    get_mouse_status(kbdc, status, 0, 3) >= 3 && status[1] != 0)
		return (status[1]);
	return (c);
}

/* misc subroutines */
/*
 * Someday, I will get the complete list of valid pointing devices and
 * their IDs... XXX
 */
static int
is_a_mouse(int id)
{
#if 0
	static int valid_ids[] = {
		PSM_MOUSE_ID,		/* mouse */
		PSM_BALLPOINT_ID,	/* ballpoint device */
		PSM_INTELLI_ID,		/* Intellimouse */
		PSM_EXPLORER_ID,	/* Intellimouse Explorer */
		-1			/* end of table */
	};
	int i;

	for (i = 0; valid_ids[i] >= 0; ++i)
	if (valid_ids[i] == id)
		return (TRUE);
	return (FALSE);
#else
	return (TRUE);
#endif
}

static char *
model_name(int model)
{
	static struct {
		int	model_code;
		char	*model_name;
	} models[] = {
		{ MOUSE_MODEL_NETSCROLL,	"NetScroll" },
		{ MOUSE_MODEL_NET,		"NetMouse/NetScroll Optical" },
		{ MOUSE_MODEL_GLIDEPOINT,	"GlidePoint" },
		{ MOUSE_MODEL_THINK,		"ThinkingMouse" },
		{ MOUSE_MODEL_INTELLI,		"IntelliMouse" },
		{ MOUSE_MODEL_MOUSEMANPLUS,	"MouseMan+" },
		{ MOUSE_MODEL_VERSAPAD,		"VersaPad" },
		{ MOUSE_MODEL_EXPLORER,		"IntelliMouse Explorer" },
		{ MOUSE_MODEL_4D,		"4D Mouse" },
		{ MOUSE_MODEL_4DPLUS,		"4D+ Mouse" },
		{ MOUSE_MODEL_SYNAPTICS,	"Synaptics Touchpad" },
		{ MOUSE_MODEL_GENERIC,		"Generic PS/2 mouse" },
		{ MOUSE_MODEL_UNKNOWN,		"Unknown" },
	};
	int i;

	for (i = 0; models[i].model_code != MOUSE_MODEL_UNKNOWN; ++i)
		if (models[i].model_code == model)
			break;
	return (models[i].model_name);
}

static void
recover_from_error(KBDC kbdc)
{
	/* discard anything left in the output buffer */
	empty_both_buffers(kbdc, 10);

#if 0
	/*
	 * NOTE: KBDC_RESET_KBD may not restore the communication between the
	 * keyboard and the controller.
	 */
	reset_kbd(kbdc);
#else
	/*
	 * NOTE: somehow diagnostic and keyboard port test commands bring the
	 * keyboard back.
	 */
	if (!test_controller(kbdc))
		log(LOG_ERR, "psm: keyboard controller failed.\n");
	/* if there isn't a keyboard in the system, the following error is OK */
	if (test_kbd_port(kbdc) != 0)
		VLOG(1, (LOG_ERR, "psm: keyboard port failed.\n"));
#endif
}

static int
restore_controller(KBDC kbdc, int command_byte)
{
	empty_both_buffers(kbdc, 10);

	if (!set_controller_command_byte(kbdc, 0xff, command_byte)) {
		log(LOG_ERR, "psm: failed to restore the keyboard controller "
		    "command byte.\n");
		empty_both_buffers(kbdc, 10);
		return (FALSE);
	} else {
		empty_both_buffers(kbdc, 10);
		return (TRUE);
	}
}

/*
 * Re-initialize the aux port and device. The aux port must be enabled
 * and its interrupt must be disabled before calling this routine.
 * The aux device will be disabled before returning.
 * The keyboard controller must be locked via `kbdc_lock()' before
 * calling this routine.
 */
static int
doinitialize(struct psm_softc *sc, mousemode_t *mode)
{
	KBDC kbdc = sc->kbdc;
	int stat[3];
	int i;

	switch((i = test_aux_port(kbdc))) {
	case 1:	/* ignore these errors */
	case 2:
	case 3:
	case PSM_ACK:
		if (verbose)
			log(LOG_DEBUG,
			    "psm%d: strange result for test aux port (%d).\n",
			    sc->unit, i);
		/* FALLTHROUGH */
	case 0:		/* no error */
		break;
	case -1:	/* time out */
	default:	/* error */
		recover_from_error(kbdc);
		if (sc->config & PSM_CONFIG_IGNPORTERROR)
			break;
		log(LOG_ERR, "psm%d: the aux port is not functioning (%d).\n",
		    sc->unit, i);
		return (FALSE);
	}

	if (sc->config & PSM_CONFIG_NORESET) {
		/*
		 * Don't try to reset the pointing device.  It may possibly
		 * be left in the unknown state, though...
		 */
	} else {
		/*
		 * NOTE: some controllers appears to hang the `keyboard' when
		 * the aux port doesn't exist and `PSMC_RESET_DEV' is issued.
		 */
		if (!reset_aux_dev(kbdc)) {
			recover_from_error(kbdc);
			log(LOG_ERR, "psm%d: failed to reset the aux device.\n",
			    sc->unit);
			return (FALSE);
		}
	}

	/*
	 * both the aux port and the aux device is functioning, see
	 * if the device can be enabled.
	 */
	if (!enable_aux_dev(kbdc) || !disable_aux_dev(kbdc)) {
		log(LOG_ERR, "psm%d: failed to enable the aux device.\n",
		    sc->unit);
		return (FALSE);
	}
	empty_both_buffers(kbdc, 10);	/* remove stray data if any */

	if (sc->config & PSM_CONFIG_NOIDPROBE)
		i = GENERIC_MOUSE_ENTRY;
	else {
		/* FIXME: hardware ID, mouse buttons? */

		/* other parameters */
		for (i = 0; vendortype[i].probefunc != NULL; ++i)
			if ((*vendortype[i].probefunc)(sc)) {
				if (verbose >= 2)
					log(LOG_ERR, "psm%d: found %s\n",
					    sc->unit,
					    model_name(vendortype[i].model));
				break;
			}
	}

	sc->hw.model = vendortype[i].model;
	sc->mode.packetsize = vendortype[i].packetsize;

	/* set mouse parameters */
	if (mode != (mousemode_t *)NULL) {
		if (mode->rate > 0)
			mode->rate = set_mouse_sampling_rate(kbdc, mode->rate);
		if (mode->resolution >= 0)
			mode->resolution =
			    set_mouse_resolution(kbdc, mode->resolution);
		set_mouse_scaling(kbdc, 1);
		set_mouse_mode(kbdc);
	}

	/* Record sync on the next data packet we see. */
	sc->flags |= PSM_NEED_SYNCBITS;

	/* just check the status of the mouse */
	if (get_mouse_status(kbdc, stat, 0, 3) < 3)
		log(LOG_DEBUG, "psm%d: failed to get status (doinitialize).\n",
		    sc->unit);

	return (TRUE);
}

static int
doopen(struct psm_softc *sc, int command_byte)
{
	int stat[3];

	/* enable the mouse device */
	if (!enable_aux_dev(sc->kbdc)) {
		/* MOUSE ERROR: failed to enable the mouse because:
		 * 1) the mouse is faulty,
		 * 2) the mouse has been removed(!?)
		 * In the latter case, the keyboard may have hung, and need
		 * recovery procedure...
		 */
		recover_from_error(sc->kbdc);
#if 0
		/* FIXME: we could reset the mouse here and try to enable
		 * it again. But it will take long time and it's not a good
		 * idea to disable the keyboard that long...
		 */
		if (!doinitialize(sc, &sc->mode) || !enable_aux_dev(sc->kbdc)) {
			recover_from_error(sc->kbdc);
#else
		{
#endif
			restore_controller(sc->kbdc, command_byte);
			/* mark this device is no longer available */
			sc->state &= ~PSM_VALID;
			log(LOG_ERR,
			    "psm%d: failed to enable the device (doopen).\n",
			sc->unit);
			return (EIO);
		}
	}

	if (get_mouse_status(sc->kbdc, stat, 0, 3) < 3)
		log(LOG_DEBUG, "psm%d: failed to get status (doopen).\n",
		    sc->unit);

	/* enable the aux port and interrupt */
	if (!set_controller_command_byte(sc->kbdc,
	    kbdc_get_device_mask(sc->kbdc),
	    (command_byte & KBD_KBD_CONTROL_BITS) |
	    KBD_ENABLE_AUX_PORT | KBD_ENABLE_AUX_INT)) {
		/* CONTROLLER ERROR */
		disable_aux_dev(sc->kbdc);
		restore_controller(sc->kbdc, command_byte);
		log(LOG_ERR,
		    "psm%d: failed to enable the aux interrupt (doopen).\n",
		    sc->unit);
		return (EIO);
	}

	/* start the watchdog timer */
	sc->watchdog = FALSE;
	sc->callout = timeout(psmtimeout, (void *)(uintptr_t)sc, hz*2);

	return (0);
}

static int
reinitialize(struct psm_softc *sc, int doinit)
{
	int err;
	int c;
	int s;

	/* don't let anybody mess with the aux device */
	if (!kbdc_lock(sc->kbdc, TRUE))
		return (EIO);
	s = spltty();

	/* block our watchdog timer */
	sc->watchdog = FALSE;
	untimeout(psmtimeout, (void *)(uintptr_t)sc, sc->callout);
	callout_handle_init(&sc->callout);

	/* save the current controller command byte */
	empty_both_buffers(sc->kbdc, 10);
	c = get_controller_command_byte(sc->kbdc);
	VLOG(2, (LOG_DEBUG,
	    "psm%d: current command byte: %04x (reinitialize).\n",
	    sc->unit, c));

	/* enable the aux port but disable the aux interrupt and the keyboard */
	if ((c == -1) || !set_controller_command_byte(sc->kbdc,
	    kbdc_get_device_mask(sc->kbdc),
	    KBD_DISABLE_KBD_PORT | KBD_DISABLE_KBD_INT |
	    KBD_ENABLE_AUX_PORT | KBD_DISABLE_AUX_INT)) {
		/* CONTROLLER ERROR */
		splx(s);
		kbdc_lock(sc->kbdc, FALSE);
		log(LOG_ERR,
		    "psm%d: unable to set the command byte (reinitialize).\n",
		    sc->unit);
		return (EIO);
	}

	/* flush any data */
	if (sc->state & PSM_VALID) {
		/* this may fail; but never mind... */
		disable_aux_dev(sc->kbdc);
		empty_aux_buffer(sc->kbdc, 10);
	}
	flushpackets(sc);
	sc->syncerrors = 0;
	sc->pkterrors = 0;
	memset(&sc->lastinputerr, 0, sizeof(sc->lastinputerr));

	/* try to detect the aux device; are you still there? */
	err = 0;
	if (doinit) {
		if (doinitialize(sc, &sc->mode)) {
			/* yes */
			sc->state |= PSM_VALID;
		} else {
			/* the device has gone! */
			restore_controller(sc->kbdc, c);
			sc->state &= ~PSM_VALID;
			log(LOG_ERR,
			    "psm%d: the aux device has gone! (reinitialize).\n",
			    sc->unit);
			err = ENXIO;
		}
	}
	splx(s);

	/* restore the driver state */
	if ((sc->state & PSM_OPEN) && (err == 0)) {
		/* enable the aux device and the port again */
		err = doopen(sc, c);
		if (err != 0)
			log(LOG_ERR, "psm%d: failed to enable the device "
			    "(reinitialize).\n", sc->unit);
	} else {
		/* restore the keyboard port and disable the aux port */
		if (!set_controller_command_byte(sc->kbdc,
		    kbdc_get_device_mask(sc->kbdc),
		    (c & KBD_KBD_CONTROL_BITS) |
		    KBD_DISABLE_AUX_PORT | KBD_DISABLE_AUX_INT)) {
			/* CONTROLLER ERROR */
			log(LOG_ERR, "psm%d: failed to disable the aux port "
			    "(reinitialize).\n", sc->unit);
			err = EIO;
		}
	}

	kbdc_lock(sc->kbdc, FALSE);
	return (err);
}

/* psm driver entry points */

static void
psmidentify(driver_t *driver, device_t parent)
{
	device_t psmc;
	device_t psm;
	u_long irq;
	int unit;

	unit = device_get_unit(parent);

	/* always add at least one child */
	psm = BUS_ADD_CHILD(parent, KBDC_RID_AUX, driver->name, unit);
	if (psm == NULL)
		return;

	irq = bus_get_resource_start(psm, SYS_RES_IRQ, KBDC_RID_AUX);
	if (irq > 0)
		return;

	/*
	 * If the PS/2 mouse device has already been reported by ACPI or
	 * PnP BIOS, obtain the IRQ resource from it.
	 * (See psmcpnp_attach() below.)
	 */
	psmc = device_find_child(device_get_parent(parent),
	    PSMCPNP_DRIVER_NAME, unit);
	if (psmc == NULL)
		return;
	irq = bus_get_resource_start(psmc, SYS_RES_IRQ, 0);
	if (irq <= 0)
		return;
	bus_set_resource(psm, SYS_RES_IRQ, KBDC_RID_AUX, irq, 1);
}

#define	endprobe(v)	do {			\
	if (bootverbose)			\
		--verbose;			\
	kbdc_set_device_mask(sc->kbdc, mask);	\
	kbdc_lock(sc->kbdc, FALSE);		\
	return (v);				\
} while (0)

static int
psmprobe(device_t dev)
{
	int unit = device_get_unit(dev);
	struct psm_softc *sc = device_get_softc(dev);
	int stat[3];
	int command_byte;
	int mask;
	int rid;
	int i;

#if 0
	kbdc_debug(TRUE);
#endif

	/* see if IRQ is available */
	rid = KBDC_RID_AUX;
	sc->intr = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
	    RF_SHAREABLE | RF_ACTIVE);
	if (sc->intr == NULL) {
		if (bootverbose)
			device_printf(dev, "unable to allocate IRQ\n");
		return (ENXIO);
	}
	bus_release_resource(dev, SYS_RES_IRQ, rid, sc->intr);

	sc->unit = unit;
	sc->kbdc = atkbdc_open(device_get_unit(device_get_parent(dev)));
	sc->config = device_get_flags(dev) & PSM_CONFIG_FLAGS;
	/* XXX: for backward compatibility */
#if defined(PSM_HOOKRESUME) || defined(PSM_HOOKAPM)
	sc->config |=
#ifdef PSM_RESETAFTERSUSPEND
	PSM_CONFIG_HOOKRESUME | PSM_CONFIG_INITAFTERSUSPEND;
#else
	PSM_CONFIG_HOOKRESUME;
#endif
#endif /* PSM_HOOKRESUME | PSM_HOOKAPM */
	sc->flags = 0;
	if (bootverbose)
		++verbose;

	device_set_desc(dev, "PS/2 Mouse");

	if (!kbdc_lock(sc->kbdc, TRUE)) {
		printf("psm%d: unable to lock the controller.\n", unit);
		if (bootverbose)
			--verbose;
		return (ENXIO);
	}

	/*
	 * NOTE: two bits in the command byte controls the operation of the
	 * aux port (mouse port): the aux port disable bit (bit 5) and the aux
	 * port interrupt (IRQ 12) enable bit (bit 2).
	 */

	/* discard anything left after the keyboard initialization */
	empty_both_buffers(sc->kbdc, 10);

	/* save the current command byte; it will be used later */
	mask = kbdc_get_device_mask(sc->kbdc) & ~KBD_AUX_CONTROL_BITS;
	command_byte = get_controller_command_byte(sc->kbdc);
	if (verbose)
		printf("psm%d: current command byte:%04x\n", unit,
		    command_byte);
	if (command_byte == -1) {
		/* CONTROLLER ERROR */
		printf("psm%d: unable to get the current command byte value.\n",
			unit);
		endprobe(ENXIO);
	}

	/*
	 * disable the keyboard port while probing the aux port, which must be
	 * enabled during this routine
	 */
	if (!set_controller_command_byte(sc->kbdc,
	    KBD_KBD_CONTROL_BITS | KBD_AUX_CONTROL_BITS,
	    KBD_DISABLE_KBD_PORT | KBD_DISABLE_KBD_INT |
	    KBD_ENABLE_AUX_PORT | KBD_DISABLE_AUX_INT)) {
		/*
		 * this is CONTROLLER ERROR; I don't know how to recover
		 * from this error...
		 */
		restore_controller(sc->kbdc, command_byte);
		printf("psm%d: unable to set the command byte.\n", unit);
		endprobe(ENXIO);
	}
	write_controller_command(sc->kbdc, KBDC_ENABLE_AUX_PORT);

	/*
	 * NOTE: `test_aux_port()' is designed to return with zero if the aux
	 * port exists and is functioning. However, some controllers appears
	 * to respond with zero even when the aux port doesn't exist. (It may
	 * be that this is only the case when the controller DOES have the aux
	 * port but the port is not wired on the motherboard.) The keyboard
	 * controllers without the port, such as the original AT, are
	 * supporsed to return with an error code or simply time out. In any
	 * case, we have to continue probing the port even when the controller
	 * passes this test.
	 *
	 * XXX: some controllers erroneously return the error code 1, 2 or 3
	 * when it has the perfectly functional aux port. We have to ignore
	 * this error code. Even if the controller HAS error with the aux
	 * port, it will be detected later...
	 * XXX: another incompatible controller returns PSM_ACK (0xfa)...
	 */
	switch ((i = test_aux_port(sc->kbdc))) {
	case 1:		/* ignore these errors */
	case 2:
	case 3:
	case PSM_ACK:
		if (verbose)
			printf("psm%d: strange result for test aux port "
			    "(%d).\n", unit, i);
		/* FALLTHROUGH */
	case 0:		/* no error */
		break;
	case -1:	/* time out */
	default:	/* error */
		recover_from_error(sc->kbdc);
		if (sc->config & PSM_CONFIG_IGNPORTERROR)
			break;
		restore_controller(sc->kbdc, command_byte);
		if (verbose)
			printf("psm%d: the aux port is not functioning (%d).\n",
			    unit, i);
		endprobe(ENXIO);
	}

	if (sc->config & PSM_CONFIG_NORESET) {
		/*
		 * Don't try to reset the pointing device.  It may possibly be
		 * left in the unknown state, though...
		 */
	} else {
		/*
		 * NOTE: some controllers appears to hang the `keyboard' when
		 * the aux port doesn't exist and `PSMC_RESET_DEV' is issued.
		 *
		 * Attempt to reset the controller twice -- this helps
		 * pierce through some KVM switches. The second reset
		 * is non-fatal.
		 */
		if (!reset_aux_dev(sc->kbdc)) {
			recover_from_error(sc->kbdc);
			restore_controller(sc->kbdc, command_byte);
			if (verbose)
				printf("psm%d: failed to reset the aux "
				    "device.\n", unit);
			endprobe(ENXIO);
		} else if (!reset_aux_dev(sc->kbdc)) {
			recover_from_error(sc->kbdc);
			if (verbose >= 2)
				printf("psm%d: failed to reset the aux device "
				    "(2).\n", unit);
		}
	}

	/*
	 * both the aux port and the aux device is functioning, see if the
	 * device can be enabled. NOTE: when enabled, the device will start
	 * sending data; we shall immediately disable the device once we know
	 * the device can be enabled.
	 */
	if (!enable_aux_dev(sc->kbdc) || !disable_aux_dev(sc->kbdc)) {
		/* MOUSE ERROR */
		recover_from_error(sc->kbdc);
		restore_controller(sc->kbdc, command_byte);
		if (verbose)
			printf("psm%d: failed to enable the aux device.\n",
			    unit);
		endprobe(ENXIO);
	}

	/* save the default values after reset */
	if (get_mouse_status(sc->kbdc, stat, 0, 3) >= 3) {
		sc->dflt_mode.rate = sc->mode.rate = stat[2];
		sc->dflt_mode.resolution = sc->mode.resolution = stat[1];
	} else {
		sc->dflt_mode.rate = sc->mode.rate = -1;
		sc->dflt_mode.resolution = sc->mode.resolution = -1;
	}

	/* hardware information */
	sc->hw.iftype = MOUSE_IF_PS2;

	/* verify the device is a mouse */
	sc->hw.hwid = get_aux_id(sc->kbdc);
	if (!is_a_mouse(sc->hw.hwid)) {
		restore_controller(sc->kbdc, command_byte);
		if (verbose)
			printf("psm%d: unknown device type (%d).\n", unit,
			    sc->hw.hwid);
		endprobe(ENXIO);
	}
	switch (sc->hw.hwid) {
	case PSM_BALLPOINT_ID:
		sc->hw.type = MOUSE_TRACKBALL;
		break;
	case PSM_MOUSE_ID:
	case PSM_INTELLI_ID:
	case PSM_EXPLORER_ID:
	case PSM_4DMOUSE_ID:
	case PSM_4DPLUS_ID:
		sc->hw.type = MOUSE_MOUSE;
		break;
	default:
		sc->hw.type = MOUSE_UNKNOWN;
		break;
	}

	if (sc->config & PSM_CONFIG_NOIDPROBE) {
		sc->hw.buttons = 2;
		i = GENERIC_MOUSE_ENTRY;
	} else {
		/* # of buttons */
		sc->hw.buttons = get_mouse_buttons(sc->kbdc);

		/* other parameters */
		for (i = 0; vendortype[i].probefunc != NULL; ++i)
			if ((*vendortype[i].probefunc)(sc)) {
				if (verbose >= 2)
					printf("psm%d: found %s\n", unit,
					    model_name(vendortype[i].model));
				break;
			}
	}

	sc->hw.model = vendortype[i].model;

	sc->dflt_mode.level = PSM_LEVEL_BASE;
	sc->dflt_mode.packetsize = MOUSE_PS2_PACKETSIZE;
	sc->dflt_mode.accelfactor = (sc->config & PSM_CONFIG_ACCEL) >> 4;
	if (sc->config & PSM_CONFIG_NOCHECKSYNC)
		sc->dflt_mode.syncmask[0] = 0;
	else
		sc->dflt_mode.syncmask[0] = vendortype[i].syncmask;
	if (sc->config & PSM_CONFIG_FORCETAP)
		sc->dflt_mode.syncmask[0] &= ~MOUSE_PS2_TAP;
	sc->dflt_mode.syncmask[1] = 0;	/* syncbits */
	sc->mode = sc->dflt_mode;
	sc->mode.packetsize = vendortype[i].packetsize;

	/* set mouse parameters */
#if 0
	/*
	 * A version of Logitech FirstMouse+ won't report wheel movement,
	 * if SET_DEFAULTS is sent...  Don't use this command.
	 * This fix was found by Takashi Nishida.
	 */
	i = send_aux_command(sc->kbdc, PSMC_SET_DEFAULTS);
	if (verbose >= 2)
		printf("psm%d: SET_DEFAULTS return code:%04x\n", unit, i);
#endif
	if (sc->config & PSM_CONFIG_RESOLUTION)
		sc->mode.resolution =
		    set_mouse_resolution(sc->kbdc,
		    (sc->config & PSM_CONFIG_RESOLUTION) - 1);
	else if (sc->mode.resolution >= 0)
		sc->mode.resolution =
		    set_mouse_resolution(sc->kbdc, sc->dflt_mode.resolution);
	if (sc->mode.rate > 0)
		sc->mode.rate =
		    set_mouse_sampling_rate(sc->kbdc, sc->dflt_mode.rate);
	set_mouse_scaling(sc->kbdc, 1);

	/* Record sync on the next data packet we see. */
	sc->flags |= PSM_NEED_SYNCBITS;

	/* just check the status of the mouse */
	/*
	 * NOTE: XXX there are some arcane controller/mouse combinations out
	 * there, which hung the controller unless there is data transmission
	 * after ACK from the mouse.
	 */
	if (get_mouse_status(sc->kbdc, stat, 0, 3) < 3)
		printf("psm%d: failed to get status.\n", unit);
	else {
		/*
		 * When in its native mode, some mice operate with different
		 * default parameters than in the PS/2 compatible mode.
		 */
		sc->dflt_mode.rate = sc->mode.rate = stat[2];
		sc->dflt_mode.resolution = sc->mode.resolution = stat[1];
	}

	/* disable the aux port for now... */
	if (!set_controller_command_byte(sc->kbdc,
	    KBD_KBD_CONTROL_BITS | KBD_AUX_CONTROL_BITS,
	    (command_byte & KBD_KBD_CONTROL_BITS) |
	    KBD_DISABLE_AUX_PORT | KBD_DISABLE_AUX_INT)) {
		/*
		 * this is CONTROLLER ERROR; I don't know the proper way to
		 * recover from this error...
		 */
		restore_controller(sc->kbdc, command_byte);
		printf("psm%d: unable to set the command byte.\n", unit);
		endprobe(ENXIO);
	}

	/*
	 * Synaptics TouchPad seems to go back to Relative Mode after
	 * the previous set_controller_command_byte() call; by issueing
	 * a Read Mode Byte command, the touchpad is in Absolute Mode
	 * again.
	 */
	if (sc->hw.model == MOUSE_MODEL_SYNAPTICS)
		mouse_ext_command(sc->kbdc, 1);

	/* done */
	kbdc_set_device_mask(sc->kbdc, mask | KBD_AUX_CONTROL_BITS);
	kbdc_lock(sc->kbdc, FALSE);
	return (0);
}

static int
psmattach(device_t dev)
{
	int unit = device_get_unit(dev);
	struct psm_softc *sc = device_get_softc(dev);
	int error;
	int rid;

	/* Setup initial state */
	sc->state = PSM_VALID;
	callout_handle_init(&sc->callout);

	/* Setup our interrupt handler */
	rid = KBDC_RID_AUX;
	sc->intr = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
	    RF_SHAREABLE | RF_ACTIVE);
	if (sc->intr == NULL)
		return (ENXIO);
	error = bus_setup_intr(dev, sc->intr, INTR_TYPE_TTY, NULL, psmintr, sc,
	    &sc->ih);
	if (error) {
		bus_release_resource(dev, SYS_RES_IRQ, rid, sc->intr);
		return (error);
	}

	/* Done */
	sc->dev = make_dev(&psm_cdevsw, PSM_MKMINOR(unit, FALSE), 0, 0, 0666,
	    "psm%d", unit);
	sc->bdev = make_dev(&psm_cdevsw, PSM_MKMINOR(unit, TRUE), 0, 0, 0666,
	    "bpsm%d", unit);

	if (!verbose)
		printf("psm%d: model %s, device ID %d\n",
		    unit, model_name(sc->hw.model), sc->hw.hwid & 0x00ff);
	else {
		printf("psm%d: model %s, device ID %d-%02x, %d buttons\n",
		    unit, model_name(sc->hw.model), sc->hw.hwid & 0x00ff,
		    sc->hw.hwid >> 8, sc->hw.buttons);
		printf("psm%d: config:%08x, flags:%08x, packet size:%d\n",
		    unit, sc->config, sc->flags, sc->mode.packetsize);
		printf("psm%d: syncmask:%02x, syncbits:%02x\n",
		    unit, sc->mode.syncmask[0], sc->mode.syncmask[1]);
	}

	if (bootverbose)
		--verbose;

	return (0);
}

static int
psmdetach(device_t dev)
{
	struct psm_softc *sc;
	int rid;

	sc = device_get_softc(dev);
	if (sc->state & PSM_OPEN)
		return (EBUSY);

	rid = KBDC_RID_AUX;
	bus_teardown_intr(dev, sc->intr, sc->ih);
	bus_release_resource(dev, SYS_RES_IRQ, rid, sc->intr);

	destroy_dev(sc->dev);
	destroy_dev(sc->bdev);

	return (0);
}

static int
psmopen(struct cdev *dev, int flag, int fmt, struct thread *td)
{
	int unit = PSM_UNIT(dev);
	struct psm_softc *sc;
	int command_byte;
	int err;
	int s;

	/* Get device data */
	sc = PSM_SOFTC(unit);
	if ((sc == NULL) || (sc->state & PSM_VALID) == 0) {
		/* the device is no longer valid/functioning */
		return (ENXIO);
	}

	/* Disallow multiple opens */
	if (sc->state & PSM_OPEN)
		return (EBUSY);

	device_busy(devclass_get_device(psm_devclass, unit));

	/* Initialize state */
	sc->mode.level = sc->dflt_mode.level;
	sc->mode.protocol = sc->dflt_mode.protocol;
	sc->watchdog = FALSE;

	/* flush the event queue */
	sc->queue.count = 0;
	sc->queue.head = 0;
	sc->queue.tail = 0;
	sc->status.flags = 0;
	sc->status.button = 0;
	sc->status.obutton = 0;
	sc->status.dx = 0;
	sc->status.dy = 0;
	sc->status.dz = 0;
	sc->button = 0;
	sc->pqueue_start = 0;
	sc->pqueue_end = 0;

	/* empty input buffer */
	flushpackets(sc);
	sc->syncerrors = 0;
	sc->pkterrors = 0;

	/* don't let timeout routines in the keyboard driver to poll the kbdc */
	if (!kbdc_lock(sc->kbdc, TRUE))
		return (EIO);

	/* save the current controller command byte */
	s = spltty();
	command_byte = get_controller_command_byte(sc->kbdc);

	/* enable the aux port and temporalily disable the keyboard */
	if (command_byte == -1 || !set_controller_command_byte(sc->kbdc,
	    kbdc_get_device_mask(sc->kbdc),
	    KBD_DISABLE_KBD_PORT | KBD_DISABLE_KBD_INT |
	    KBD_ENABLE_AUX_PORT | KBD_DISABLE_AUX_INT)) {
		/* CONTROLLER ERROR; do you know how to get out of this? */
		kbdc_lock(sc->kbdc, FALSE);
		splx(s);
		log(LOG_ERR,
		    "psm%d: unable to set the command byte (psmopen).\n", unit);
		return (EIO);
	}
	/*
	 * Now that the keyboard controller is told not to generate
	 * the keyboard and mouse interrupts, call `splx()' to allow
	 * the other tty interrupts. The clock interrupt may also occur,
	 * but timeout routines will be blocked by the poll flag set
	 * via `kbdc_lock()'
	 */
	splx(s);

	/* enable the mouse device */
	err = doopen(sc, command_byte);

	/* done */
	if (err == 0)
		sc->state |= PSM_OPEN;
	kbdc_lock(sc->kbdc, FALSE);
	return (err);
}

static int
psmclose(struct cdev *dev, int flag, int fmt, struct thread *td)
{
	int unit = PSM_UNIT(dev);
	struct psm_softc *sc = PSM_SOFTC(unit);
	int stat[3];
	int command_byte;
	int s;

	/* don't let timeout routines in the keyboard driver to poll the kbdc */
	if (!kbdc_lock(sc->kbdc, TRUE))
		return (EIO);

	/* save the current controller command byte */
	s = spltty();
	command_byte = get_controller_command_byte(sc->kbdc);
	if (command_byte == -1) {
		kbdc_lock(sc->kbdc, FALSE);
		splx(s);
		return (EIO);
	}

	/* disable the aux interrupt and temporalily disable the keyboard */
	if (!set_controller_command_byte(sc->kbdc,
	    kbdc_get_device_mask(sc->kbdc),
	    KBD_DISABLE_KBD_PORT | KBD_DISABLE_KBD_INT |
	    KBD_ENABLE_AUX_PORT | KBD_DISABLE_AUX_INT)) {
		log(LOG_ERR,
		    "psm%d: failed to disable the aux int (psmclose).\n", unit);
		/* CONTROLLER ERROR;
		 * NOTE: we shall force our way through. Because the only
		 * ill effect we shall see is that we may not be able
		 * to read ACK from the mouse, and it doesn't matter much
		 * so long as the mouse will accept the DISABLE command.
		 */
	}
	splx(s);

	/* stop the watchdog timer */
	untimeout(psmtimeout, (void *)(uintptr_t)sc, sc->callout);
	callout_handle_init(&sc->callout);

	/* remove anything left in the output buffer */
	empty_aux_buffer(sc->kbdc, 10);

	/* disable the aux device, port and interrupt */
	if (sc->state & PSM_VALID) {
		if (!disable_aux_dev(sc->kbdc)) {
			/* MOUSE ERROR;
			 * NOTE: we don't return (error) and continue,
			 * pretending we have successfully disabled the device.
			 * It's OK because the interrupt routine will discard
			 * any data from the mouse hereafter.
			 */
			log(LOG_ERR,
			    "psm%d: failed to disable the device (psmclose).\n",
			    unit);
		}

		if (get_mouse_status(sc->kbdc, stat, 0, 3) < 3)
			log(LOG_DEBUG,
			    "psm%d: failed to get status (psmclose).\n", unit);
	}

	if (!set_controller_command_byte(sc->kbdc,
	    kbdc_get_device_mask(sc->kbdc),
	    (command_byte & KBD_KBD_CONTROL_BITS) |
	    KBD_DISABLE_AUX_PORT | KBD_DISABLE_AUX_INT)) {
		/*
		 * CONTROLLER ERROR;
		 * we shall ignore this error; see the above comment.
		 */
		log(LOG_ERR,
		    "psm%d: failed to disable the aux port (psmclose).\n",
		    unit);
	}

	/* remove anything left in the output buffer */
	empty_aux_buffer(sc->kbdc, 10);

	/* close is almost always successful */
	sc->state &= ~PSM_OPEN;
	kbdc_lock(sc->kbdc, FALSE);
	device_unbusy(devclass_get_device(psm_devclass, unit));
	return (0);
}

static int
tame_mouse(struct psm_softc *sc, packetbuf_t *pb, mousestatus_t *status,
    u_char *buf)
{
	static u_char butmapps2[8] = {
		0,
		MOUSE_PS2_BUTTON1DOWN,
		MOUSE_PS2_BUTTON2DOWN,
		MOUSE_PS2_BUTTON1DOWN | MOUSE_PS2_BUTTON2DOWN,
		MOUSE_PS2_BUTTON3DOWN,
		MOUSE_PS2_BUTTON1DOWN | MOUSE_PS2_BUTTON3DOWN,
		MOUSE_PS2_BUTTON2DOWN | MOUSE_PS2_BUTTON3DOWN,
		MOUSE_PS2_BUTTON1DOWN | MOUSE_PS2_BUTTON2DOWN |
		    MOUSE_PS2_BUTTON3DOWN,
	};
	static u_char butmapmsc[8] = {
		MOUSE_MSC_BUTTON1UP | MOUSE_MSC_BUTTON2UP |
		    MOUSE_MSC_BUTTON3UP,
		MOUSE_MSC_BUTTON2UP | MOUSE_MSC_BUTTON3UP,
		MOUSE_MSC_BUTTON1UP | MOUSE_MSC_BUTTON3UP,
		MOUSE_MSC_BUTTON3UP,
		MOUSE_MSC_BUTTON1UP | MOUSE_MSC_BUTTON2UP,
		MOUSE_MSC_BUTTON2UP,
		MOUSE_MSC_BUTTON1UP,
		0,
	};
	int mapped;
	int i;

	if (sc->mode.level == PSM_LEVEL_BASE) {
		mapped = status->button & ~MOUSE_BUTTON4DOWN;
		if (status->button & MOUSE_BUTTON4DOWN)
			mapped |= MOUSE_BUTTON1DOWN;
		status->button = mapped;
		buf[0] = MOUSE_PS2_SYNC | butmapps2[mapped & MOUSE_STDBUTTONS];
		i = imax(imin(status->dx, 255), -256);
		if (i < 0)
			buf[0] |= MOUSE_PS2_XNEG;
		buf[1] = i;
		i = imax(imin(status->dy, 255), -256);
		if (i < 0)
			buf[0] |= MOUSE_PS2_YNEG;
		buf[2] = i;
		return (MOUSE_PS2_PACKETSIZE);
	} else if (sc->mode.level == PSM_LEVEL_STANDARD) {
		buf[0] = MOUSE_MSC_SYNC |
		    butmapmsc[status->button & MOUSE_STDBUTTONS];
		i = imax(imin(status->dx, 255), -256);
		buf[1] = i >> 1;
		buf[3] = i - buf[1];
		i = imax(imin(status->dy, 255), -256);
		buf[2] = i >> 1;
		buf[4] = i - buf[2];
		i = imax(imin(status->dz, 127), -128);
		buf[5] = (i >> 1) & 0x7f;
		buf[6] = (i - (i >> 1)) & 0x7f;
		buf[7] = (~status->button >> 3) & 0x7f;
		return (MOUSE_SYS_PACKETSIZE);
	}
	return (pb->inputbytes);
}

static int
psmread(struct cdev *dev, struct uio *uio, int flag)
{
	register struct psm_softc *sc = PSM_SOFTC(PSM_UNIT(dev));
	u_char buf[PSM_SMALLBUFSIZE];
	int error = 0;
	int s;
	int l;

	if ((sc->state & PSM_VALID) == 0)
		return (EIO);

	/* block until mouse activity occured */
	s = spltty();
	while (sc->queue.count <= 0) {
		if (PSM_NBLOCKIO(dev)) {
			splx(s);
			return (EWOULDBLOCK);
		}
		sc->state |= PSM_ASLP;
		error = tsleep(sc, PZERO | PCATCH, "psmrea", 0);
		sc->state &= ~PSM_ASLP;
		if (error) {
			splx(s);
			return (error);
		} else if ((sc->state & PSM_VALID) == 0) {
			/* the device disappeared! */
			splx(s);
			return (EIO);
		}
	}
	splx(s);

	/* copy data to the user land */
	while ((sc->queue.count > 0) && (uio->uio_resid > 0)) {
		s = spltty();
		l = imin(sc->queue.count, uio->uio_resid);
		if (l > sizeof(buf))
			l = sizeof(buf);
		if (l > sizeof(sc->queue.buf) - sc->queue.head) {
			bcopy(&sc->queue.buf[sc->queue.head], &buf[0],
			    sizeof(sc->queue.buf) - sc->queue.head);
			bcopy(&sc->queue.buf[0],
			    &buf[sizeof(sc->queue.buf) - sc->queue.head],
			    l - (sizeof(sc->queue.buf) - sc->queue.head));
		} else
			bcopy(&sc->queue.buf[sc->queue.head], &buf[0], l);
		sc->queue.count -= l;
		sc->queue.head = (sc->queue.head + l) % sizeof(sc->queue.buf);
		splx(s);
		error = uiomove(buf, l, uio);
		if (error)
			break;
	}

	return (error);
}

static int
block_mouse_data(struct psm_softc *sc, int *c)
{
	int s;

	if (!kbdc_lock(sc->kbdc, TRUE))
		return (EIO);

	s = spltty();
	*c = get_controller_command_byte(sc->kbdc);
	if ((*c == -1) || !set_controller_command_byte(sc->kbdc,
	    kbdc_get_device_mask(sc->kbdc),
	    KBD_DISABLE_KBD_PORT | KBD_DISABLE_KBD_INT |
	    KBD_ENABLE_AUX_PORT | KBD_DISABLE_AUX_INT)) {
		/* this is CONTROLLER ERROR */
		splx(s);
		kbdc_lock(sc->kbdc, FALSE);
		return (EIO);
	}

	/*
	 * The device may be in the middle of status data transmission.
	 * The transmission will be interrupted, thus, incomplete status
	 * data must be discarded. Although the aux interrupt is disabled
	 * at the keyboard controller level, at most one aux interrupt
	 * may have already been pending and a data byte is in the
	 * output buffer; throw it away. Note that the second argument
	 * to `empty_aux_buffer()' is zero, so that the call will just
	 * flush the internal queue.
	 * `psmintr()' will be invoked after `splx()' if an interrupt is
	 * pending; it will see no data and returns immediately.
	 */
	empty_aux_buffer(sc->kbdc, 0);		/* flush the queue */
	read_aux_data_no_wait(sc->kbdc);	/* throw away data if any */
	flushpackets(sc);
	splx(s);

	return (0);
}

static void
dropqueue(struct psm_softc *sc)
{

	sc->queue.count = 0;
	sc->queue.head = 0;
	sc->queue.tail = 0;
	if ((sc->state & PSM_SOFTARMED) != 0) {
		sc->state &= ~PSM_SOFTARMED;
		untimeout(psmsoftintr, (void *)(uintptr_t)sc, sc->softcallout);
	}
	sc->pqueue_start = sc->pqueue_end;
}

static void
flushpackets(struct psm_softc *sc)
{

	dropqueue(sc);
	bzero(&sc->pqueue, sizeof(sc->pqueue));
}

static int
unblock_mouse_data(struct psm_softc *sc, int c)
{
	int error = 0;

	/*
	 * We may have seen a part of status data during `set_mouse_XXX()'.
	 * they have been queued; flush it.
	 */
	empty_aux_buffer(sc->kbdc, 0);

	/* restore ports and interrupt */
	if (!set_controller_command_byte(sc->kbdc,
	    kbdc_get_device_mask(sc->kbdc),
	    c & (KBD_KBD_CONTROL_BITS | KBD_AUX_CONTROL_BITS))) {
		/*
		 * CONTROLLER ERROR; this is serious, we may have
		 * been left with the inaccessible keyboard and
		 * the disabled mouse interrupt.
		 */
		error = EIO;
	}

	kbdc_lock(sc->kbdc, FALSE);
	return (error);
}

static int
psmwrite(struct cdev *dev, struct uio *uio, int flag)
{
	register struct psm_softc *sc = PSM_SOFTC(PSM_UNIT(dev));
	u_char buf[PSM_SMALLBUFSIZE];
	int error = 0, i, l;

	if ((sc->state & PSM_VALID) == 0)
		return (EIO);

	if (sc->mode.level < PSM_LEVEL_NATIVE)
		return (ENODEV);

	/* copy data from the user land */
	while (uio->uio_resid > 0) {
		l = imin(PSM_SMALLBUFSIZE, uio->uio_resid);
		error = uiomove(buf, l, uio);
		if (error)
			break;
		for (i = 0; i < l; i++) {
			VLOG(4, (LOG_DEBUG, "psm: cmd 0x%x\n", buf[i]));
			if (!write_aux_command(sc->kbdc, buf[i])) {
				VLOG(2, (LOG_DEBUG,
				    "psm: cmd 0x%x failed.\n", buf[i]));
				return (reinitialize(sc, FALSE));
			}
		}
	}

	return (error);
}

static int
psmioctl(struct cdev *dev, u_long cmd, caddr_t addr, int flag,
    struct thread *td)
{
	struct psm_softc *sc = PSM_SOFTC(PSM_UNIT(dev));
	mousemode_t mode;
	mousestatus_t status;
#if (defined(MOUSE_GETVARS))
	mousevar_t *var;
#endif
	mousedata_t *data;
	int stat[3];
	int command_byte;
	int error = 0;
	int s;

	/* Perform IOCTL command */
	switch (cmd) {

	case OLD_MOUSE_GETHWINFO:
		s = spltty();
		((old_mousehw_t *)addr)->buttons = sc->hw.buttons;
		((old_mousehw_t *)addr)->iftype = sc->hw.iftype;
		((old_mousehw_t *)addr)->type = sc->hw.type;
		((old_mousehw_t *)addr)->hwid = sc->hw.hwid & 0x00ff;
		splx(s);
		break;

	case MOUSE_GETHWINFO:
		s = spltty();
		*(mousehw_t *)addr = sc->hw;
		if (sc->mode.level == PSM_LEVEL_BASE)
			((mousehw_t *)addr)->model = MOUSE_MODEL_GENERIC;
		splx(s);
		break;

	case MOUSE_SYN_GETHWINFO:
		s = spltty();
		if (synaptics_support && sc->hw.model == MOUSE_MODEL_SYNAPTICS)
			*(synapticshw_t *)addr = sc->synhw;
		else
			error = EINVAL;
		splx(s);
		break;

	case OLD_MOUSE_GETMODE:
		s = spltty();
		switch (sc->mode.level) {
		case PSM_LEVEL_BASE:
			((old_mousemode_t *)addr)->protocol = MOUSE_PROTO_PS2;
			break;
		case PSM_LEVEL_STANDARD:
			((old_mousemode_t *)addr)->protocol =
			    MOUSE_PROTO_SYSMOUSE;
			break;
		case PSM_LEVEL_NATIVE:
			((old_mousemode_t *)addr)->protocol = MOUSE_PROTO_PS2;
			break;
		}
		((old_mousemode_t *)addr)->rate = sc->mode.rate;
		((old_mousemode_t *)addr)->resolution = sc->mode.resolution;
		((old_mousemode_t *)addr)->accelfactor = sc->mode.accelfactor;
		splx(s);
		break;

	case MOUSE_GETMODE:
		s = spltty();
		*(mousemode_t *)addr = sc->mode;
		if ((sc->flags & PSM_NEED_SYNCBITS) != 0) {
			((mousemode_t *)addr)->syncmask[0] = 0;
			((mousemode_t *)addr)->syncmask[1] = 0;
		}
		((mousemode_t *)addr)->resolution =
			MOUSE_RES_LOW - sc->mode.resolution;
		switch (sc->mode.level) {
		case PSM_LEVEL_BASE:
			((mousemode_t *)addr)->protocol = MOUSE_PROTO_PS2;
			((mousemode_t *)addr)->packetsize =
			    MOUSE_PS2_PACKETSIZE;
			break;
		case PSM_LEVEL_STANDARD:
			((mousemode_t *)addr)->protocol = MOUSE_PROTO_SYSMOUSE;
			((mousemode_t *)addr)->packetsize =
			    MOUSE_SYS_PACKETSIZE;
			((mousemode_t *)addr)->syncmask[0] = MOUSE_SYS_SYNCMASK;
			((mousemode_t *)addr)->syncmask[1] = MOUSE_SYS_SYNC;
			break;
		case PSM_LEVEL_NATIVE:
			/* FIXME: this isn't quite correct... XXX */
			((mousemode_t *)addr)->protocol = MOUSE_PROTO_PS2;
			break;
		}
		splx(s);
		break;

	case OLD_MOUSE_SETMODE:
	case MOUSE_SETMODE:
		if (cmd == OLD_MOUSE_SETMODE) {
			mode.rate = ((old_mousemode_t *)addr)->rate;
			/*
			 * resolution  old I/F   new I/F
			 * default        0         0
			 * low            1        -2
			 * medium low     2        -3
			 * medium high    3        -4
			 * high           4        -5
			 */
			if (((old_mousemode_t *)addr)->resolution > 0)
				mode.resolution =
				    -((old_mousemode_t *)addr)->resolution - 1;
			else
				mode.resolution = 0;
			mode.accelfactor =
			    ((old_mousemode_t *)addr)->accelfactor;
			mode.level = -1;
		} else
			mode = *(mousemode_t *)addr;

		/* adjust and validate parameters. */
		if (mode.rate > UCHAR_MAX)
			return (EINVAL);
		if (mode.rate == 0)
			mode.rate = sc->dflt_mode.rate;
		else if (mode.rate == -1)
			/* don't change the current setting */
			;
		else if (mode.rate < 0)
			return (EINVAL);
		if (mode.resolution >= UCHAR_MAX)
			return (EINVAL);
		if (mode.resolution >= 200)
			mode.resolution = MOUSE_RES_HIGH;
		else if (mode.resolution >= 100)
			mode.resolution = MOUSE_RES_MEDIUMHIGH;
		else if (mode.resolution >= 50)
			mode.resolution = MOUSE_RES_MEDIUMLOW;
		else if (mode.resolution > 0)
			mode.resolution = MOUSE_RES_LOW;
		if (mode.resolution == MOUSE_RES_DEFAULT)
			mode.resolution = sc->dflt_mode.resolution;
		else if (mode.resolution == -1)
			/* don't change the current setting */
			;
		else if (mode.resolution < 0) /* MOUSE_RES_LOW/MEDIUM/HIGH */
			mode.resolution = MOUSE_RES_LOW - mode.resolution;
		if (mode.level == -1)
			/* don't change the current setting */
			mode.level = sc->mode.level;
		else if ((mode.level < PSM_LEVEL_MIN) ||
		    (mode.level > PSM_LEVEL_MAX))
			return (EINVAL);
		if (mode.accelfactor == -1)
			/* don't change the current setting */
			mode.accelfactor = sc->mode.accelfactor;
		else if (mode.accelfactor < 0)
			return (EINVAL);

		/* don't allow anybody to poll the keyboard controller */
		error = block_mouse_data(sc, &command_byte);
		if (error)
			return (error);

		/* set mouse parameters */
		if (mode.rate > 0)
			mode.rate = set_mouse_sampling_rate(sc->kbdc,
			    mode.rate);
		if (mode.resolution >= 0)
			mode.resolution =
			    set_mouse_resolution(sc->kbdc, mode.resolution);
		set_mouse_scaling(sc->kbdc, 1);
		get_mouse_status(sc->kbdc, stat, 0, 3);

		s = spltty();
		sc->mode.rate = mode.rate;
		sc->mode.resolution = mode.resolution;
		sc->mode.accelfactor = mode.accelfactor;
		sc->mode.level = mode.level;
		splx(s);

		unblock_mouse_data(sc, command_byte);
		break;

	case MOUSE_GETLEVEL:
		*(int *)addr = sc->mode.level;
		break;

	case MOUSE_SETLEVEL:
		if ((*(int *)addr < PSM_LEVEL_MIN) ||
		    (*(int *)addr > PSM_LEVEL_MAX))
			return (EINVAL);
		sc->mode.level = *(int *)addr;
		break;

	case MOUSE_GETSTATUS:
		s = spltty();
		status = sc->status;
		sc->status.flags = 0;
		sc->status.obutton = sc->status.button;
		sc->status.button = 0;
		sc->status.dx = 0;
		sc->status.dy = 0;
		sc->status.dz = 0;
		splx(s);
		*(mousestatus_t *)addr = status;
		break;

#if (defined(MOUSE_GETVARS))
	case MOUSE_GETVARS:
		var = (mousevar_t *)addr;
		bzero(var, sizeof(*var));
		s = spltty();
		var->var[0] = MOUSE_VARS_PS2_SIG;
		var->var[1] = sc->config;
		var->var[2] = sc->flags;
		splx(s);
		break;

	case MOUSE_SETVARS:
		return (ENODEV);
#endif /* MOUSE_GETVARS */

	case MOUSE_READSTATE:
	case MOUSE_READDATA:
		data = (mousedata_t *)addr;
		if (data->len > sizeof(data->buf)/sizeof(data->buf[0]))
			return (EINVAL);

		error = block_mouse_data(sc, &command_byte);
		if (error)
			return (error);
		if ((data->len = get_mouse_status(sc->kbdc, data->buf,
		    (cmd == MOUSE_READDATA) ? 1 : 0, data->len)) <= 0)
			error = EIO;
		unblock_mouse_data(sc, command_byte);
		break;

#if (defined(MOUSE_SETRESOLUTION))
	case MOUSE_SETRESOLUTION:
		mode.resolution = *(int *)addr;
		if (mode.resolution >= UCHAR_MAX)
			return (EINVAL);
		else if (mode.resolution >= 200)
			mode.resolution = MOUSE_RES_HIGH;
		else if (mode.resolution >= 100)
			mode.resolution = MOUSE_RES_MEDIUMHIGH;
		else if (mode.resolution >= 50)
			mode.resolution = MOUSE_RES_MEDIUMLOW;
		else if (mode.resolution > 0)
			mode.resolution = MOUSE_RES_LOW;
		if (mode.resolution == MOUSE_RES_DEFAULT)
			mode.resolution = sc->dflt_mode.resolution;
		else if (mode.resolution == -1)
			mode.resolution = sc->mode.resolution;
		else if (mode.resolution < 0) /* MOUSE_RES_LOW/MEDIUM/HIGH */
			mode.resolution = MOUSE_RES_LOW - mode.resolution;

		error = block_mouse_data(sc, &command_byte);
		if (error)
			return (error);
		sc->mode.resolution =
		    set_mouse_resolution(sc->kbdc, mode.resolution);
		if (sc->mode.resolution != mode.resolution)
			error = EIO;
		unblock_mouse_data(sc, command_byte);
		break;
#endif /* MOUSE_SETRESOLUTION */

#if (defined(MOUSE_SETRATE))
	case MOUSE_SETRATE:
		mode.rate = *(int *)addr;
		if (mode.rate > UCHAR_MAX)
			return (EINVAL);
		if (mode.rate == 0)
			mode.rate = sc->dflt_mode.rate;
		else if (mode.rate < 0)
			mode.rate = sc->mode.rate;

		error = block_mouse_data(sc, &command_byte);
		if (error)
			return (error);
		sc->mode.rate = set_mouse_sampling_rate(sc->kbdc, mode.rate);
		if (sc->mode.rate != mode.rate)
			error = EIO;
		unblock_mouse_data(sc, command_byte);
		break;
#endif /* MOUSE_SETRATE */

#if (defined(MOUSE_SETSCALING))
	case MOUSE_SETSCALING:
		if ((*(int *)addr <= 0) || (*(int *)addr > 2))
			return (EINVAL);

		error = block_mouse_data(sc, &command_byte);
		if (error)
			return (error);
		if (!set_mouse_scaling(sc->kbdc, *(int *)addr))
			error = EIO;
		unblock_mouse_data(sc, command_byte);
		break;
#endif /* MOUSE_SETSCALING */

#if (defined(MOUSE_GETHWID))
	case MOUSE_GETHWID:
		error = block_mouse_data(sc, &command_byte);
		if (error)
			return (error);
		sc->hw.hwid &= ~0x00ff;
		sc->hw.hwid |= get_aux_id(sc->kbdc);
		*(int *)addr = sc->hw.hwid & 0x00ff;
		unblock_mouse_data(sc, command_byte);
		break;
#endif /* MOUSE_GETHWID */

	default:
		return (ENOTTY);
	}

	return (error);
}

static void
psmtimeout(void *arg)
{
	struct psm_softc *sc;
	int s;

	sc = (struct psm_softc *)arg;
	s = spltty();
	if (sc->watchdog && kbdc_lock(sc->kbdc, TRUE)) {
		VLOG(4, (LOG_DEBUG, "psm%d: lost interrupt?\n", sc->unit));
		psmintr(sc);
		kbdc_lock(sc->kbdc, FALSE);
	}
	sc->watchdog = TRUE;
	splx(s);
	sc->callout = timeout(psmtimeout, (void *)(uintptr_t)sc, hz);
}

/* Add all sysctls under the debug.psm and hw.psm nodes */
SYSCTL_NODE(_debug, OID_AUTO, psm, CTLFLAG_RD, 0, "ps/2 mouse");
SYSCTL_NODE(_hw, OID_AUTO, psm, CTLFLAG_RD, 0, "ps/2 mouse");

SYSCTL_INT(_debug_psm, OID_AUTO, loglevel, CTLFLAG_RW, &verbose, 0, "");

static int psmhz = 20;
SYSCTL_INT(_debug_psm, OID_AUTO, hz, CTLFLAG_RW, &psmhz, 0, "");
static int psmerrsecs = 2;
SYSCTL_INT(_debug_psm, OID_AUTO, errsecs, CTLFLAG_RW, &psmerrsecs, 0, "");
static int psmerrusecs = 0;
SYSCTL_INT(_debug_psm, OID_AUTO, errusecs, CTLFLAG_RW, &psmerrusecs, 0, "");
static int psmsecs = 0;
SYSCTL_INT(_debug_psm, OID_AUTO, secs, CTLFLAG_RW, &psmsecs, 0, "");
static int psmusecs = 500000;
SYSCTL_INT(_debug_psm, OID_AUTO, usecs, CTLFLAG_RW, &psmusecs, 0, "");
static int pkterrthresh = 2;
SYSCTL_INT(_debug_psm, OID_AUTO, pkterrthresh, CTLFLAG_RW, &pkterrthresh,
    0, "");

static int tap_threshold = PSM_TAP_THRESHOLD;
SYSCTL_INT(_hw_psm, OID_AUTO, tap_threshold, CTLFLAG_RW, &tap_threshold, 0, "");
static int tap_timeout = PSM_TAP_TIMEOUT;
SYSCTL_INT(_hw_psm, OID_AUTO, tap_timeout, CTLFLAG_RW, &tap_timeout, 0, "");

static void
psmintr(void *arg)
{
	struct psm_softc *sc = arg;
	struct timeval now;
	int c;
	packetbuf_t *pb;


	/* read until there is nothing to read */
	while((c = read_aux_data_no_wait(sc->kbdc)) != -1) {
		pb = &sc->pqueue[sc->pqueue_end];

		/* discard the byte if the device is not open */
		if ((sc->state & PSM_OPEN) == 0)
			continue;

		getmicrouptime(&now);
		if ((pb->inputbytes > 0) &&
		    timevalcmp(&now, &sc->inputtimeout, >)) {
			VLOG(3, (LOG_DEBUG, "psmintr: delay too long; "
			    "resetting byte count\n"));
			pb->inputbytes = 0;
			sc->syncerrors = 0;
			sc->pkterrors = 0;
		}
		sc->inputtimeout.tv_sec = PSM_INPUT_TIMEOUT / 1000000;
		sc->inputtimeout.tv_usec = PSM_INPUT_TIMEOUT % 1000000;
		timevaladd(&sc->inputtimeout, &now);

		pb->ipacket[pb->inputbytes++] = c;

		if (sc->mode.level == PSM_LEVEL_NATIVE) {
			VLOG(4, (LOG_DEBUG, "psmintr: %02x\n", pb->ipacket[0]));
			sc->syncerrors = 0;
			sc->pkterrors = 0;
			goto next;
		} else {
			if (pb->inputbytes < sc->mode.packetsize)
				continue;

			VLOG(4, (LOG_DEBUG,
			    "psmintr: %02x %02x %02x %02x %02x %02x\n",
			    pb->ipacket[0], pb->ipacket[1], pb->ipacket[2],
			    pb->ipacket[3], pb->ipacket[4], pb->ipacket[5]));
		}

		c = pb->ipacket[0];

		if ((sc->flags & PSM_NEED_SYNCBITS) != 0) {
			sc->mode.syncmask[1] = (c & sc->mode.syncmask[0]);
			sc->flags &= ~PSM_NEED_SYNCBITS;
			VLOG(2, (LOG_DEBUG,
			    "psmintr: Sync bytes now %04x,%04x\n",
			    sc->mode.syncmask[0], sc->mode.syncmask[0]));
		} else if ((c & sc->mode.syncmask[0]) != sc->mode.syncmask[1]) {
			VLOG(3, (LOG_DEBUG, "psmintr: out of sync "
			    "(%04x != %04x) %d cmds since last error.\n",
			    c & sc->mode.syncmask[0], sc->mode.syncmask[1],
			    sc->cmdcount - sc->lasterr));
			sc->lasterr = sc->cmdcount;
			/*
			 * The sync byte test is a weak measure of packet
			 * validity.  Conservatively discard any input yet
			 * to be seen by userland when we detect a sync
			 * error since there is a good chance some of
			 * the queued packets have undetected errors.
			 */
			dropqueue(sc);
			if (sc->syncerrors == 0)
				sc->pkterrors++;
			++sc->syncerrors;
			sc->lastinputerr = now;
			if (sc->syncerrors >= sc->mode.packetsize * 2 ||
			    sc->pkterrors >= pkterrthresh) {
				/*
				 * If we've failed to find a single sync byte
				 * in 2 packets worth of data, or we've seen
				 * persistent packet errors during the
				 * validation period, reinitialize the mouse
				 * in hopes of returning it to the expected
				 * mode.
				 */
				VLOG(3, (LOG_DEBUG,
				    "psmintr: reset the mouse.\n"));
				reinitialize(sc, TRUE);
			} else if (sc->syncerrors == sc->mode.packetsize) {
				/*
				 * Try a soft reset after searching for a sync
				 * byte through a packet length of bytes.
				 */
				VLOG(3, (LOG_DEBUG,
				    "psmintr: re-enable the mouse.\n"));
				pb->inputbytes = 0;
				disable_aux_dev(sc->kbdc);
				enable_aux_dev(sc->kbdc);
			} else {
				VLOG(3, (LOG_DEBUG,
				    "psmintr: discard a byte (%d)\n",
				    sc->syncerrors));
				pb->inputbytes--;
				bcopy(&pb->ipacket[1], &pb->ipacket[0],
				    pb->inputbytes);
			}
			continue;
		}

		/*
		 * We have what appears to be a valid packet.
		 * Reset the error counters.
		 */
		sc->syncerrors = 0;

		/*
		 * Drop even good packets if they occur within a timeout
		 * period of a sync error.  This allows the detection of
		 * a change in the mouse's packet mode without exposing
		 * erratic mouse behavior to the user.  Some KVMs forget
		 * enhanced mouse modes during switch events.
		 */
		if (!timeelapsed(&sc->lastinputerr, psmerrsecs, psmerrusecs,
		    &now)) {
			pb->inputbytes = 0;
			continue;
		}

		/*
		 * Now that we're out of the validation period, reset
		 * the packet error count.
		 */
		sc->pkterrors = 0;

		sc->cmdcount++;
next:
		if (++sc->pqueue_end >= PSM_PACKETQUEUE)
			sc->pqueue_end = 0;
		/*
		 * If we've filled the queue then call the softintr ourselves,
		 * otherwise schedule the interrupt for later.
		 */
		if (!timeelapsed(&sc->lastsoftintr, psmsecs, psmusecs, &now) ||
		    (sc->pqueue_end == sc->pqueue_start)) {
			if ((sc->state & PSM_SOFTARMED) != 0) {
				sc->state &= ~PSM_SOFTARMED;
				untimeout(psmsoftintr, arg, sc->softcallout);
			}
			psmsoftintr(arg);
		} else if ((sc->state & PSM_SOFTARMED) == 0) {
			sc->state |= PSM_SOFTARMED;
			sc->softcallout = timeout(psmsoftintr, arg,
			    psmhz < 1 ? 1 : (hz/psmhz));
		}
	}
}

static void
proc_mmanplus(struct psm_softc *sc, packetbuf_t *pb, mousestatus_t *ms,
    int *x, int *y, int *z)
{

	/*
	 * PS2++ protocl packet
	 *
	 *          b7 b6 b5 b4 b3 b2 b1 b0
	 * byte 1:  *  1  p3 p2 1  *  *  *
	 * byte 2:  c1 c2 p1 p0 d1 d0 1  0
	 *
	 * p3-p0: packet type
	 * c1, c2: c1 & c2 == 1, if p2 == 0
	 *         c1 & c2 == 0, if p2 == 1
	 *
	 * packet type: 0 (device type)
	 * See comments in enable_mmanplus() below.
	 *
	 * packet type: 1 (wheel data)
	 *
	 *          b7 b6 b5 b4 b3 b2 b1 b0
	 * byte 3:  h  *  B5 B4 s  d2 d1 d0
	 *
	 * h: 1, if horizontal roller data
	 *    0, if vertical roller data
	 * B4, B5: button 4 and 5
	 * s: sign bit
	 * d2-d0: roller data
	 *
	 * packet type: 2 (reserved)
	 */
	if (((pb->ipacket[0] & MOUSE_PS2PLUS_SYNCMASK) == MOUSE_PS2PLUS_SYNC) &&
	    (abs(*x) > 191) && MOUSE_PS2PLUS_CHECKBITS(pb->ipacket)) {
		/*
		 * the extended data packet encodes button
		 * and wheel events
		 */
		switch (MOUSE_PS2PLUS_PACKET_TYPE(pb->ipacket)) {
		case 1:
			/* wheel data packet */
			*x = *y = 0;
			if (pb->ipacket[2] & 0x80) {
				/* XXX horizontal roller count - ignore it */
				;
			} else {
				/* vertical roller count */
				*z = (pb->ipacket[2] & MOUSE_PS2PLUS_ZNEG) ?
				    (pb->ipacket[2] & 0x0f) - 16 :
				    (pb->ipacket[2] & 0x0f);
			}
			ms->button |= (pb->ipacket[2] &
			    MOUSE_PS2PLUS_BUTTON4DOWN) ?
			    MOUSE_BUTTON4DOWN : 0;
			ms->button |= (pb->ipacket[2] &
			    MOUSE_PS2PLUS_BUTTON5DOWN) ?
			    MOUSE_BUTTON5DOWN : 0;
			break;
		case 2:
			/*
			 * this packet type is reserved by
			 * Logitech...
			 */
			/*
			 * IBM ScrollPoint Mouse uses this
			 * packet type to encode both vertical
			 * and horizontal scroll movement.
			 */
			*x = *y = 0;
			/* horizontal count */
			if (pb->ipacket[2] & 0x0f)
				*z = (pb->ipacket[2] & MOUSE_SPOINT_WNEG) ?
				    -2 : 2;
			/* vertical count */
			if (pb->ipacket[2] & 0xf0)
				*z = (pb->ipacket[2] & MOUSE_SPOINT_ZNEG) ?
				    -1 : 1;
			break;
		case 0:
			/* device type packet - shouldn't happen */
			/* FALLTHROUGH */
		default:
			*x = *y = 0;
			ms->button = ms->obutton;
			VLOG(1, (LOG_DEBUG, "psmintr: unknown PS2++ packet "
			    "type %d: 0x%02x 0x%02x 0x%02x\n",
			    MOUSE_PS2PLUS_PACKET_TYPE(pb->ipacket),
			    pb->ipacket[0], pb->ipacket[1], pb->ipacket[2]));
			break;
		}
	} else {
		/* preserve button states */
		ms->button |= ms->obutton & MOUSE_EXTBUTTONS;
	}
}

static int
proc_synaptics(struct psm_softc *sc, packetbuf_t *pb, mousestatus_t *ms,
    int *x, int *y, int *z)
{
	static int touchpad_buttons;
	static int guest_buttons;
	int w, x0, y0, xavg, yavg, xsensitivity, ysensitivity, sensitivity = 0;

	/* TouchPad PS/2 absolute mode message format
	 *
	 *  Bits:        7   6   5   4   3   2   1   0 (LSB)
	 *  ------------------------------------------------
	 *  ipacket[0]:  1   0  W3  W2   0  W1   R   L
	 *  ipacket[1]: Yb  Ya  Y9  Y8  Xb  Xa  X9  X8
	 *  ipacket[2]: Z7  Z6  Z5  Z4  Z3  Z2  Z1  Z0
	 *  ipacket[3]:  1   1  Yc  Xc   0  W0   D   U
	 *  ipacket[4]: X7  X6  X5  X4  X3  X2  X1  X0
	 *  ipacket[5]: Y7  Y6  Y5  Y4  Y3  Y2  Y1  Y0
	 *
	 * Legend:
	 *  L: left physical mouse button
	 *  R: right physical mouse button
	 *  D: down button
	 *  U: up button
	 *  W: "wrist" value
	 *  X: x position
	 *  Y: x position
	 *  Z: pressure
	 *
	 * Absolute reportable limits:    0 - 6143.
	 * Typical bezel limits:       1472 - 5472.
	 * Typical edge marings:       1632 - 5312.
	 *
	 * w = 3 Passthrough Packet
	 *
	 * Byte 2,5,6 == Byte 1,2,3 of "Guest"
	 */

	if (!synaptics_support)
		return (0);

	/* Sanity check for out of sync packets. */
	if ((pb->ipacket[0] & 0xc8) != 0x80 ||
	    (pb->ipacket[3] & 0xc8) != 0xc0)
		return (-1);

	*x = *y = x0 = y0 = 0;

	/* Pressure value. */
	*z = pb->ipacket[2];

	/* Finger width value */
	if (sc->synhw.capExtended)
		w = ((pb->ipacket[0] & 0x30) >> 2) |
		    ((pb->ipacket[0] & 0x04) >> 1) |
		    ((pb->ipacket[3] & 0x04) >> 2);
	else {
		/* Assume a finger of regular width */
		w = 4;
	}

	/* Handle packets from the guest device */
	if (w == 3 && sc->synhw.capPassthrough) {
		*x = ((pb->ipacket[1] & 0x10) ?
		    pb->ipacket[4] - 256 : pb->ipacket[4]);
		*y = ((pb->ipacket[1] & 0x20) ?
		    pb->ipacket[5] - 256 : pb->ipacket[5]);
		*z = 0;

		guest_buttons = 0;
		if (pb->ipacket[1] & 0x01)
			guest_buttons |= MOUSE_BUTTON1DOWN;
		if (pb->ipacket[1] & 0x04)
			guest_buttons |= MOUSE_BUTTON2DOWN;
		if (pb->ipacket[1] & 0x02)
			guest_buttons |= MOUSE_BUTTON3DOWN;

		ms->button = touchpad_buttons | guest_buttons;
		return (0);
	}

	/* Button presses */
	touchpad_buttons = 0;
	if (pb->ipacket[0] & 0x01)
		touchpad_buttons |= MOUSE_BUTTON1DOWN;
	if (pb->ipacket[0] & 0x02)
		touchpad_buttons |= MOUSE_BUTTON3DOWN;

	if (sc->synhw.capExtended && sc->synhw.capFourButtons) {
		if ((pb->ipacket[3] & 0x01) && (pb->ipacket[0] & 0x01) == 0)
			touchpad_buttons |= MOUSE_BUTTON4DOWN;
		if ((pb->ipacket[3] & 0x02) && (pb->ipacket[0] & 0x02) == 0)
			touchpad_buttons |= MOUSE_BUTTON5DOWN;
	}

	/*
	 * In newer pads - bit 0x02 in the third byte of
	 * the packet indicates that we have an extended
	 * button press.
	 */
	if (pb->ipacket[3] & 0x02) {
		/*
		 * if directional_scrolls is not 1, we treat any of
		 * the scrolling directions as middle-click.
		 */
		if (sc->syninfo.directional_scrolls) {
			if (pb->ipacket[4] & 0x01)
				touchpad_buttons |= MOUSE_BUTTON4DOWN;
			if (pb->ipacket[5] & 0x01)
				touchpad_buttons |= MOUSE_BUTTON5DOWN;
			if (pb->ipacket[4] & 0x02)
				touchpad_buttons |= MOUSE_BUTTON6DOWN;
			if (pb->ipacket[5] & 0x02)
				touchpad_buttons |= MOUSE_BUTTON7DOWN;
		} else {
			if ((pb->ipacket[4] & 0x0F) || (pb->ipacket[5] & 0x0F))
				touchpad_buttons |= MOUSE_BUTTON2DOWN;
		}
	}

	ms->button = touchpad_buttons | guest_buttons;

	/* There is a finger on the pad. */
	if ((w >= 4 && w <= 7) && (*z >= 16 && *z < 200)) {
		x0 = ((pb->ipacket[3] & 0x10) << 8) |
		    ((pb->ipacket[1] & 0x0f) << 8) | pb->ipacket[4];
		y0 = ((pb->ipacket[3] & 0x20) << 7) |
		    ((pb->ipacket[1] & 0xf0) << 4) | pb->ipacket[5];

		if (sc->flags & PSM_FLAGS_FINGERDOWN) {
			*x = x0 - sc->xold;
			*y = y0 - sc->yold;

			/*
			 * we compute averages of x and y
			 * movement
			 */
			if (sc->xaverage == 0)
				sc->xaverage = *x;

			if (sc->yaverage == 0)
				sc->yaverage = *y;

			xavg = sc->xaverage;
			yavg = sc->yaverage;

			sc->xaverage = (xavg + *x) >> 1;
			sc->yaverage = (yavg + *y) >> 1;

			/*
			 * then use the averages to compute
			 * a sensitivity level in each dimension
			 */
			xsensitivity = (sc->xaverage - xavg);
			if (xsensitivity < 0)
				xsensitivity = -xsensitivity;

			ysensitivity = (sc->yaverage - yavg);
			if (ysensitivity < 0)
				ysensitivity = -ysensitivity;

			/*
			 * The sensitivity level is higher the faster
			 * the finger is moving.  It also tends to be
			 * higher in the middle of a touchpad motion
			 * than on either end
			 * Note - sensitivity gets to 0 when moving slowly -
			 * so we add 1 to it to give it a meaningful value
			 * in that case.
			 */
			sensitivity = (xsensitivity & ysensitivity) + 1;

			/*
			 * If either our x or y change is greater than
			 * our hi/low speed threshold - we do the high-speed
			 * absolute to relative calculation otherwise
			 * we do the low-speed calculation.
			 */
			if ((*x > sc->syninfo.low_speed_threshold ||
			    *x < -sc->syninfo.low_speed_threshold) ||
			    (*y > sc->syninfo.low_speed_threshold ||
			    *y < -sc->syninfo.low_speed_threshold)) {
				x0 = (x0 + sc->xold * 3) / 4;
				y0 = (y0 + sc->yold * 3) / 4;
				*x = (x0 - sc->xold) * 10 / 85;
				*y = (y0 - sc->yold) * 10 / 85;
			} else {
				/*
				 * This is the low speed calculation.
				 * We simply check to see if our movement is
				 * more than our minimum movement threshold
				 * and if it is - set the movement to 1
				 * in the correct direction.
				 * NOTE - Normally this would result
				 * in pointer movement that was WAY too fast.
				 * This works due to the movement squelch
				 * we do later.
				 */
				if (*x < -sc->syninfo.min_movement)
					*x = -1;
				else if (*x > sc->syninfo.min_movement)
					*x = 1;
				else
					*x = 0;
				if (*y < -sc->syninfo.min_movement)
					*y = -1;
				else if (*y > sc->syninfo.min_movement)
					*y = 1;
				else
					*y = 0;

			}
		} else
			sc->flags |= PSM_FLAGS_FINGERDOWN;

		/*
		 * The squelch process.  Take our sensitivity value and
		 * add it to the current squelch value - if squelch is
		 * less than our squelch threshold we kill the movement,
		 * otherwise we reset squelch and pass the movement through.
		 * Since squelch is cumulative - when mouse movement is slow
		 * (around sensitivity 1) the net result is that only 1
		 * out of every squelch_level packets is delivered,
		 * effectively slowing down the movement.
		 */
		sc->squelch += sensitivity;
		if (sc->squelch < sc->syninfo.squelch_level) {
			*x = 0;
			*y = 0;
		} else
			sc->squelch = 0;

		sc->xold = x0;
		sc->yold = y0;
		sc->zmax = imax(*z, sc->zmax);
	} else {
		sc->flags &= ~PSM_FLAGS_FINGERDOWN;

		if (sc->zmax > tap_threshold &&
		    timevalcmp(&sc->lastsoftintr, &sc->taptimeout, <=)) {
			if (w == 0)
				ms->button |= MOUSE_BUTTON3DOWN;
			else if (w == 1)
				ms->button |= MOUSE_BUTTON2DOWN;
			else
				ms->button |= MOUSE_BUTTON1DOWN;
		}

		sc->zmax = 0;
		sc->taptimeout.tv_sec = tap_timeout / 1000000;
		sc->taptimeout.tv_usec = tap_timeout % 1000000;
		timevaladd(&sc->taptimeout, &sc->lastsoftintr);
	}

	/* Use the extra buttons as a scrollwheel */
	if (ms->button & MOUSE_BUTTON4DOWN)
		*z = -1;
	else if (ms->button & MOUSE_BUTTON5DOWN)
		*z = 1;
	else
		*z = 0;

	return (0);
}

static void
proc_versapad(struct psm_softc *sc, packetbuf_t *pb, mousestatus_t *ms,
    int *x, int *y, int *z)
{
	static int butmap_versapad[8] = {
		0,
		MOUSE_BUTTON3DOWN,
		0,
		MOUSE_BUTTON3DOWN,
		MOUSE_BUTTON1DOWN,
		MOUSE_BUTTON1DOWN | MOUSE_BUTTON3DOWN,
		MOUSE_BUTTON1DOWN,
		MOUSE_BUTTON1DOWN | MOUSE_BUTTON3DOWN
	};
	int c, x0, y0;

	/* VersaPad PS/2 absolute mode message format
	 *
	 * [packet1]     7   6   5   4   3   2   1   0(LSB)
	 *  ipacket[0]:  1   1   0   A   1   L   T   R
	 *  ipacket[1]: H7  H6  H5  H4  H3  H2  H1  H0
	 *  ipacket[2]: V7  V6  V5  V4  V3  V2  V1  V0
	 *  ipacket[3]:  1   1   1   A   1   L   T   R
	 *  ipacket[4]:V11 V10  V9  V8 H11 H10  H9  H8
	 *  ipacket[5]:  0  P6  P5  P4  P3  P2  P1  P0
	 *
	 * [note]
	 *  R: right physical mouse button (1=on)
	 *  T: touch pad virtual button (1=tapping)
	 *  L: left physical mouse button (1=on)
	 *  A: position data is valid (1=valid)
	 *  H: horizontal data (12bit signed integer. H11 is sign bit.)
	 *  V: vertical data (12bit signed integer. V11 is sign bit.)
	 *  P: pressure data
	 *
	 * Tapping is mapped to MOUSE_BUTTON4.
	 */
	c = pb->ipacket[0];
	*x = *y = 0;
	ms->button = butmap_versapad[c & MOUSE_PS2VERSA_BUTTONS];
	ms->button |= (c & MOUSE_PS2VERSA_TAP) ? MOUSE_BUTTON4DOWN : 0;
	if (c & MOUSE_PS2VERSA_IN_USE) {
		x0 = pb->ipacket[1] | (((pb->ipacket[4]) & 0x0f) << 8);
		y0 = pb->ipacket[2] | (((pb->ipacket[4]) & 0xf0) << 4);
		if (x0 & 0x800)
			x0 -= 0x1000;
		if (y0 & 0x800)
			y0 -= 0x1000;
		if (sc->flags & PSM_FLAGS_FINGERDOWN) {
			*x = sc->xold - x0;
			*y = y0 - sc->yold;
			if (*x < 0)	/* XXX */
				++*x;
			else if (*x)
				--*x;
			if (*y < 0)
				++*y;
			else if (*y)
				--*y;
		} else
			sc->flags |= PSM_FLAGS_FINGERDOWN;
		sc->xold = x0;
		sc->yold = y0;
	} else
		sc->flags &= ~PSM_FLAGS_FINGERDOWN;
}

static void
psmsoftintr(void *arg)
{
	/*
	 * the table to turn PS/2 mouse button bits (MOUSE_PS2_BUTTON?DOWN)
	 * into `mousestatus' button bits (MOUSE_BUTTON?DOWN).
	 */
	static int butmap[8] = {
		0,
		MOUSE_BUTTON1DOWN,
		MOUSE_BUTTON3DOWN,
		MOUSE_BUTTON1DOWN | MOUSE_BUTTON3DOWN,
		MOUSE_BUTTON2DOWN,
		MOUSE_BUTTON1DOWN | MOUSE_BUTTON2DOWN,
		MOUSE_BUTTON2DOWN | MOUSE_BUTTON3DOWN,
		MOUSE_BUTTON1DOWN | MOUSE_BUTTON2DOWN | MOUSE_BUTTON3DOWN
	};
	register struct psm_softc *sc = arg;
	mousestatus_t ms;
	packetbuf_t *pb;
	int x, y, z, c, l, s;

	getmicrouptime(&sc->lastsoftintr);

	s = spltty();

	do {
		pb = &sc->pqueue[sc->pqueue_start];

		if (sc->mode.level == PSM_LEVEL_NATIVE)
			goto next_native;

		c = pb->ipacket[0];
		/*
		 * A kludge for Kensington device!
		 * The MSB of the horizontal count appears to be stored in
		 * a strange place.
		 */
		if (sc->hw.model == MOUSE_MODEL_THINK)
			pb->ipacket[1] |= (c & MOUSE_PS2_XOVERFLOW) ? 0x80 : 0;

		/* ignore the overflow bits... */
		x = (c & MOUSE_PS2_XNEG) ?
		    pb->ipacket[1] - 256 : pb->ipacket[1];
		y = (c & MOUSE_PS2_YNEG) ?
		    pb->ipacket[2] - 256 : pb->ipacket[2];
		z = 0;
		ms.obutton = sc->button;	  /* previous button state */
		ms.button = butmap[c & MOUSE_PS2_BUTTONS];
		/* `tapping' action */
		if (sc->config & PSM_CONFIG_FORCETAP)
			ms.button |= ((c & MOUSE_PS2_TAP)) ?
			    0 : MOUSE_BUTTON4DOWN;

		switch (sc->hw.model) {

		case MOUSE_MODEL_EXPLORER:
			/*
			 *          b7 b6 b5 b4 b3 b2 b1 b0
			 * byte 1:  oy ox sy sx 1  M  R  L
			 * byte 2:  x  x  x  x  x  x  x  x
			 * byte 3:  y  y  y  y  y  y  y  y
			 * byte 4:  *  *  S2 S1 s  d2 d1 d0
			 *
			 * L, M, R, S1, S2: left, middle, right and side buttons
			 * s: wheel data sign bit
			 * d2-d0: wheel data
			 */
			z = (pb->ipacket[3] & MOUSE_EXPLORER_ZNEG) ?
			    (pb->ipacket[3] & 0x0f) - 16 :
			    (pb->ipacket[3] & 0x0f);
			ms.button |=
			    (pb->ipacket[3] & MOUSE_EXPLORER_BUTTON4DOWN) ?
			    MOUSE_BUTTON4DOWN : 0;
			ms.button |=
			    (pb->ipacket[3] & MOUSE_EXPLORER_BUTTON5DOWN) ?
			    MOUSE_BUTTON5DOWN : 0;
			break;

		case MOUSE_MODEL_INTELLI:
		case MOUSE_MODEL_NET:
			/* wheel data is in the fourth byte */
			z = (char)pb->ipacket[3];
			/*
			 * XXX some mice may send 7 when there is no Z movement?			 */
			if ((z >= 7) || (z <= -7))
				z = 0;
			/* some compatible mice have additional buttons */
			ms.button |= (c & MOUSE_PS2INTELLI_BUTTON4DOWN) ?
			    MOUSE_BUTTON4DOWN : 0;
			ms.button |= (c & MOUSE_PS2INTELLI_BUTTON5DOWN) ?
			    MOUSE_BUTTON5DOWN : 0;
			break;

		case MOUSE_MODEL_MOUSEMANPLUS:
			proc_mmanplus(sc, pb, &ms, &x, &y, &z);
			break;

		case MOUSE_MODEL_GLIDEPOINT:
			/* `tapping' action */
			ms.button |= ((c & MOUSE_PS2_TAP)) ? 0 :
			    MOUSE_BUTTON4DOWN;
			break;

		case MOUSE_MODEL_NETSCROLL:
			/*
			 * three addtional bytes encode buttons and
			 * wheel events
			 */
			ms.button |= (pb->ipacket[3] & MOUSE_PS2_BUTTON3DOWN) ?
			    MOUSE_BUTTON4DOWN : 0;
			ms.button |= (pb->ipacket[3] & MOUSE_PS2_BUTTON1DOWN) ?
			    MOUSE_BUTTON5DOWN : 0;
			z = (pb->ipacket[3] & MOUSE_PS2_XNEG) ?
			    pb->ipacket[4] - 256 : pb->ipacket[4];
			break;

		case MOUSE_MODEL_THINK:
			/* the fourth button state in the first byte */
			ms.button |= (c & MOUSE_PS2_TAP) ?
			    MOUSE_BUTTON4DOWN : 0;
			break;

		case MOUSE_MODEL_VERSAPAD:
			proc_versapad(sc, pb, &ms, &x, &y, &z);
			c = ((x < 0) ? MOUSE_PS2_XNEG : 0) |
			    ((y < 0) ? MOUSE_PS2_YNEG : 0);
			break;
	
		case MOUSE_MODEL_4D:
			/*
			 *          b7 b6 b5 b4 b3 b2 b1 b0
			 * byte 1:  s2 d2 s1 d1 1  M  R  L
			 * byte 2:  sx x  x  x  x  x  x  x
			 * byte 3:  sy y  y  y  y  y  y  y
			 *
			 * s1: wheel 1 direction
			 * d1: wheel 1 data
			 * s2: wheel 2 direction
			 * d2: wheel 2 data
			 */
			x = (pb->ipacket[1] & 0x80) ?
			    pb->ipacket[1] - 256 : pb->ipacket[1];
			y = (pb->ipacket[2] & 0x80) ?
			    pb->ipacket[2] - 256 : pb->ipacket[2];
			switch (c & MOUSE_4D_WHEELBITS) {
			case 0x10:
				z = 1;
				break;
			case 0x30:
				z = -1;
				break;
			case 0x40:	/* XXX 2nd wheel turning right */
				z = 2;
				break;
			case 0xc0:	/* XXX 2nd wheel turning left */
				z = -2;
				break;
			}
			break;

		case MOUSE_MODEL_4DPLUS:
			if ((x < 16 - 256) && (y < 16 - 256)) {
				/*
				 *          b7 b6 b5 b4 b3 b2 b1 b0
				 * byte 1:  0  0  1  1  1  M  R  L
				 * byte 2:  0  0  0  0  1  0  0  0
				 * byte 3:  0  0  0  0  S  s  d1 d0
				 *
				 * L, M, R, S: left, middle, right,
				 *             and side buttons
				 * s: wheel data sign bit
				 * d1-d0: wheel data
				 */
				x = y = 0;
				if (pb->ipacket[2] & MOUSE_4DPLUS_BUTTON4DOWN)
					ms.button |= MOUSE_BUTTON4DOWN;
				z = (pb->ipacket[2] & MOUSE_4DPLUS_ZNEG) ?
				    ((pb->ipacket[2] & 0x07) - 8) :
				    (pb->ipacket[2] & 0x07) ;
			} else {
				/* preserve previous button states */
				ms.button |= ms.obutton & MOUSE_EXTBUTTONS;
			}
			break;

		case MOUSE_MODEL_SYNAPTICS:
			if (proc_synaptics(sc, pb, &ms, &x, &y, &z) != 0)
				goto next;
			break;

		case MOUSE_MODEL_GENERIC:
		default:
			break;
		}

	/* scale values */
	if (sc->mode.accelfactor >= 1) {
		if (x != 0) {
			x = x * x / sc->mode.accelfactor;
			if (x == 0)
				x = 1;
			if (c & MOUSE_PS2_XNEG)
				x = -x;
		}
		if (y != 0) {
			y = y * y / sc->mode.accelfactor;
			if (y == 0)
				y = 1;
			if (c & MOUSE_PS2_YNEG)
				y = -y;
		}
	}

	ms.dx = x;
	ms.dy = y;
	ms.dz = z;
	ms.flags = ((x || y || z) ? MOUSE_POSCHANGED : 0) |
	    (ms.obutton ^ ms.button);

	pb->inputbytes = tame_mouse(sc, pb, &ms, pb->ipacket);

	sc->status.flags |= ms.flags;
	sc->status.dx += ms.dx;
	sc->status.dy += ms.dy;
	sc->status.dz += ms.dz;
	sc->status.button = ms.button;
	sc->button = ms.button;

next_native:
	sc->watchdog = FALSE;

	/* queue data */
	if (sc->queue.count + pb->inputbytes < sizeof(sc->queue.buf)) {
		l = imin(pb->inputbytes,
		    sizeof(sc->queue.buf) - sc->queue.tail);
		bcopy(&pb->ipacket[0], &sc->queue.buf[sc->queue.tail], l);
		if (pb->inputbytes > l)
			bcopy(&pb->ipacket[l], &sc->queue.buf[0],
			    pb->inputbytes - l);
		sc->queue.tail = (sc->queue.tail + pb->inputbytes) %
		    sizeof(sc->queue.buf);
		sc->queue.count += pb->inputbytes;
	}
	pb->inputbytes = 0;

next:
	if (++sc->pqueue_start >= PSM_PACKETQUEUE)
		sc->pqueue_start = 0;
	} while (sc->pqueue_start != sc->pqueue_end);

	if (sc->state & PSM_ASLP) {
		sc->state &= ~PSM_ASLP;
		wakeup(sc);
	}
	selwakeuppri(&sc->rsel, PZERO);
	sc->state &= ~PSM_SOFTARMED;
	splx(s);
}

static int
psmpoll(struct cdev *dev, int events, struct thread *td)
{
	struct psm_softc *sc = PSM_SOFTC(PSM_UNIT(dev));
	int s;
	int revents = 0;

	/* Return true if a mouse event available */
	s = spltty();
	if (events & (POLLIN | POLLRDNORM)) {
		if (sc->queue.count > 0)
			revents |= events & (POLLIN | POLLRDNORM);
		else
			selrecord(td, &sc->rsel);
	}
	splx(s);

	return (revents);
}

/* vendor/model specific routines */

static int mouse_id_proc1(KBDC kbdc, int res, int scale, int *status)
{
	if (set_mouse_resolution(kbdc, res) != res)
		return (FALSE);
	if (set_mouse_scaling(kbdc, scale) &&
	    set_mouse_scaling(kbdc, scale) &&
	    set_mouse_scaling(kbdc, scale) &&
	    (get_mouse_status(kbdc, status, 0, 3) >= 3))
		return (TRUE);
	return (FALSE);
}

static int
mouse_ext_command(KBDC kbdc, int command)
{
	int c;

	c = (command >> 6) & 0x03;
	if (set_mouse_resolution(kbdc, c) != c)
		return (FALSE);
	c = (command >> 4) & 0x03;
	if (set_mouse_resolution(kbdc, c) != c)
		return (FALSE);
	c = (command >> 2) & 0x03;
	if (set_mouse_resolution(kbdc, c) != c)
		return (FALSE);
	c = (command >> 0) & 0x03;
	if (set_mouse_resolution(kbdc, c) != c)
		return (FALSE);
	return (TRUE);
}

#ifdef notyet
/* Logitech MouseMan Cordless II */
static int
enable_lcordless(struct psm_softc *sc)
{
	int status[3];
	int ch;

	if (!mouse_id_proc1(sc->kbdc, PSMD_RES_HIGH, 2, status))
		return (FALSE);
	if (status[1] == PSMD_RES_HIGH)
		return (FALSE);
	ch = (status[0] & 0x07) - 1;	/* channel # */
	if ((ch <= 0) || (ch > 4))
		return (FALSE);
	/*
	 * status[1]: always one?
	 * status[2]: battery status? (0-100)
	 */
	return (TRUE);
}
#endif /* notyet */

/* Genius NetScroll Mouse, MouseSystems SmartScroll Mouse */
static int
enable_groller(struct psm_softc *sc)
{
	int status[3];

	/*
	 * The special sequence to enable the fourth button and the
	 * roller. Immediately after this sequence check status bytes.
	 * if the mouse is NetScroll, the second and the third bytes are
	 * '3' and 'D'.
	 */

	/*
	 * If the mouse is an ordinary PS/2 mouse, the status bytes should
	 * look like the following.
	 *
	 * byte 1 bit 7 always 0
	 *        bit 6 stream mode (0)
	 *        bit 5 disabled (0)
	 *        bit 4 1:1 scaling (0)
	 *        bit 3 always 0
	 *        bit 0-2 button status
	 * byte 2 resolution (PSMD_RES_HIGH)
	 * byte 3 report rate (?)
	 */

	if (!mouse_id_proc1(sc->kbdc, PSMD_RES_HIGH, 1, status))
		return (FALSE);
	if ((status[1] != '3') || (status[2] != 'D'))
		return (FALSE);
	/* FIXME: SmartScroll Mouse has 5 buttons! XXX */
	sc->hw.buttons = 4;
	return (TRUE);
}

/* Genius NetMouse/NetMouse Pro, ASCII Mie Mouse, NetScroll Optical */
static int
enable_gmouse(struct psm_softc *sc)
{
	int status[3];

	/*
	 * The special sequence to enable the middle, "rubber" button.
	 * Immediately after this sequence check status bytes.
	 * if the mouse is NetMouse, NetMouse Pro, or ASCII MIE Mouse,
	 * the second and the third bytes are '3' and 'U'.
	 * NOTE: NetMouse reports that it has three buttons although it has
	 * two buttons and a rubber button. NetMouse Pro and MIE Mouse
	 * say they have three buttons too and they do have a button on the
	 * side...
	 */
	if (!mouse_id_proc1(sc->kbdc, PSMD_RES_HIGH, 1, status))
		return (FALSE);
	if ((status[1] != '3') || (status[2] != 'U'))
		return (FALSE);
	return (TRUE);
}

/* ALPS GlidePoint */
static int
enable_aglide(struct psm_softc *sc)
{
	int status[3];

	/*
	 * The special sequence to obtain ALPS GlidePoint specific
	 * information. Immediately after this sequence, status bytes will
	 * contain something interesting.
	 * NOTE: ALPS produces several models of GlidePoint. Some of those
	 * do not respond to this sequence, thus, cannot be detected this way.
	 */
	if (set_mouse_sampling_rate(sc->kbdc, 100) != 100)
		return (FALSE);
	if (!mouse_id_proc1(sc->kbdc, PSMD_RES_LOW, 2, status))
		return (FALSE);
	if ((status[1] == PSMD_RES_LOW) || (status[2] == 100))
		return (FALSE);
	return (TRUE);
}

/* Kensington ThinkingMouse/Trackball */
static int
enable_kmouse(struct psm_softc *sc)
{
	static u_char rate[] = { 20, 60, 40, 20, 20, 60, 40, 20, 20 };
	KBDC kbdc = sc->kbdc;
	int status[3];
	int id1;
	int id2;
	int i;

	id1 = get_aux_id(kbdc);
	if (set_mouse_sampling_rate(kbdc, 10) != 10)
		return (FALSE);
	/*
	 * The device is now in the native mode? It returns a different
	 * ID value...
	 */
	id2 = get_aux_id(kbdc);
	if ((id1 == id2) || (id2 != 2))
		return (FALSE);

	if (set_mouse_resolution(kbdc, PSMD_RES_LOW) != PSMD_RES_LOW)
		return (FALSE);
#if PSM_DEBUG >= 2
	/* at this point, resolution is LOW, sampling rate is 10/sec */
	if (get_mouse_status(kbdc, status, 0, 3) < 3)
		return (FALSE);
#endif

	/*
	 * The special sequence to enable the third and fourth buttons.
	 * Otherwise they behave like the first and second buttons.
	 */
	for (i = 0; i < sizeof(rate)/sizeof(rate[0]); ++i)
		if (set_mouse_sampling_rate(kbdc, rate[i]) != rate[i])
			return (FALSE);

	/*
	 * At this point, the device is using default resolution and
	 * sampling rate for the native mode.
	 */
	if (get_mouse_status(kbdc, status, 0, 3) < 3)
		return (FALSE);
	if ((status[1] == PSMD_RES_LOW) || (status[2] == rate[i - 1]))
		return (FALSE);

	/* the device appears be enabled by this sequence, diable it for now */
	disable_aux_dev(kbdc);
	empty_aux_buffer(kbdc, 5);

	return (TRUE);
}

/* Logitech MouseMan+/FirstMouse+, IBM ScrollPoint Mouse */
static int
enable_mmanplus(struct psm_softc *sc)
{
	KBDC kbdc = sc->kbdc;
	int data[3];

	/* the special sequence to enable the fourth button and the roller. */
	/*
	 * NOTE: for ScrollPoint to respond correctly, the SET_RESOLUTION
	 * must be called exactly three times since the last RESET command
	 * before this sequence. XXX
	 */
	if (!set_mouse_scaling(kbdc, 1))
		return (FALSE);
	if (!mouse_ext_command(kbdc, 0x39) || !mouse_ext_command(kbdc, 0xdb))
		return (FALSE);
	if (get_mouse_status(kbdc, data, 1, 3) < 3)
		return (FALSE);

	/*
	 * PS2++ protocl, packet type 0
	 *
	 *          b7 b6 b5 b4 b3 b2 b1 b0
	 * byte 1:  *  1  p3 p2 1  *  *  *
	 * byte 2:  1  1  p1 p0 m1 m0 1  0
	 * byte 3:  m7 m6 m5 m4 m3 m2 m1 m0
	 *
	 * p3-p0: packet type: 0
	 * m7-m0: model ID: MouseMan+:0x50,
	 *		    FirstMouse+:0x51,
	 *		    ScrollPoint:0x58...
	 */
	/* check constant bits */
	if ((data[0] & MOUSE_PS2PLUS_SYNCMASK) != MOUSE_PS2PLUS_SYNC)
		return (FALSE);
	if ((data[1] & 0xc3) != 0xc2)
		return (FALSE);
	/* check d3-d0 in byte 2 */
	if (!MOUSE_PS2PLUS_CHECKBITS(data))
		return (FALSE);
	/* check p3-p0 */
	if (MOUSE_PS2PLUS_PACKET_TYPE(data) != 0)
		return (FALSE);

	sc->hw.hwid &= 0x00ff;
	sc->hw.hwid |= data[2] << 8;	/* save model ID */

	/*
	 * MouseMan+ (or FirstMouse+) is now in its native mode, in which
	 * the wheel and the fourth button events are encoded in the
	 * special data packet. The mouse may be put in the IntelliMouse mode
	 * if it is initialized by the IntelliMouse's method.
	 */
	return (TRUE);
}

/* MS IntelliMouse Explorer */
static int
enable_msexplorer(struct psm_softc *sc)
{
	static u_char rate0[] = { 200, 100, 80, };
	static u_char rate1[] = { 200, 200, 80, };
	KBDC kbdc = sc->kbdc;
	int id;
	int i;

	/*
	 * This is needed for at least A4Tech X-7xx mice - they do not go
	 * straight to Explorer mode, but need to be set to Intelli mode
	 * first.
	 */
	enable_msintelli(sc);

	/* the special sequence to enable the extra buttons and the roller. */
	for (i = 0; i < sizeof(rate1)/sizeof(rate1[0]); ++i)
		if (set_mouse_sampling_rate(kbdc, rate1[i]) != rate1[i])
			return (FALSE);
	/* the device will give the genuine ID only after the above sequence */
	id = get_aux_id(kbdc);
	if (id != PSM_EXPLORER_ID)
		return (FALSE);

	sc->hw.hwid = id;
	sc->hw.buttons = 5;		/* IntelliMouse Explorer XXX */

	/*
	 * XXX: this is a kludge to fool some KVM switch products
	 * which think they are clever enough to know the 4-byte IntelliMouse
	 * protocol, and assume any other protocols use 3-byte packets.
	 * They don't convey 4-byte data packets from the IntelliMouse Explorer
	 * correctly to the host computer because of this!
	 * The following sequence is actually IntelliMouse's "wake up"
	 * sequence; it will make the KVM think the mouse is IntelliMouse
	 * when it is in fact IntelliMouse Explorer.
	 */
	for (i = 0; i < sizeof(rate0)/sizeof(rate0[0]); ++i)
		if (set_mouse_sampling_rate(kbdc, rate0[i]) != rate0[i])
			break;
	id = get_aux_id(kbdc);

	return (TRUE);
}

/* MS IntelliMouse */
static int
enable_msintelli(struct psm_softc *sc)
{
	/*
	 * Logitech MouseMan+ and FirstMouse+ will also respond to this
	 * probe routine and act like IntelliMouse.
	 */

	static u_char rate[] = { 200, 100, 80, };
	KBDC kbdc = sc->kbdc;
	int id;
	int i;

	/* the special sequence to enable the third button and the roller. */
	for (i = 0; i < sizeof(rate)/sizeof(rate[0]); ++i)
		if (set_mouse_sampling_rate(kbdc, rate[i]) != rate[i])
			return (FALSE);
	/* the device will give the genuine ID only after the above sequence */
	id = get_aux_id(kbdc);
	if (id != PSM_INTELLI_ID)
		return (FALSE);

	sc->hw.hwid = id;
	sc->hw.buttons = 3;

	return (TRUE);
}

/* A4 Tech 4D Mouse */
static int
enable_4dmouse(struct psm_softc *sc)
{
	/*
	 * Newer wheel mice from A4 Tech may use the 4D+ protocol.
	 */

	static u_char rate[] = { 200, 100, 80, 60, 40, 20 };
	KBDC kbdc = sc->kbdc;
	int id;
	int i;

	for (i = 0; i < sizeof(rate)/sizeof(rate[0]); ++i)
		if (set_mouse_sampling_rate(kbdc, rate[i]) != rate[i])
			return (FALSE);
	id = get_aux_id(kbdc);
	/*
	 * WinEasy 4D, 4 Way Scroll 4D: 6
	 * Cable-Free 4D: 8 (4DPLUS)
	 * WinBest 4D+, 4 Way Scroll 4D+: 8 (4DPLUS)
	 */
	if (id != PSM_4DMOUSE_ID)
		return (FALSE);

	sc->hw.hwid = id;
	sc->hw.buttons = 3;		/* XXX some 4D mice have 4? */

	return (TRUE);
}

/* A4 Tech 4D+ Mouse */
static int
enable_4dplus(struct psm_softc *sc)
{
	/*
	 * Newer wheel mice from A4 Tech seem to use this protocol.
	 * Older models are recognized as either 4D Mouse or IntelliMouse.
	 */
	KBDC kbdc = sc->kbdc;
	int id;

	/*
	 * enable_4dmouse() already issued the following ID sequence...
	static u_char rate[] = { 200, 100, 80, 60, 40, 20 };
	int i;

	for (i = 0; i < sizeof(rate)/sizeof(rate[0]); ++i)
		if (set_mouse_sampling_rate(kbdc, rate[i]) != rate[i])
			return (FALSE);
	*/

	id = get_aux_id(kbdc);
	switch (id) {
	case PSM_4DPLUS_ID:
		sc->hw.buttons = 4;
		break;
	case PSM_4DPLUS_RFSW35_ID:
		sc->hw.buttons = 3;
		break;
	default:
		return (FALSE);
	}

	sc->hw.hwid = id;

	return (TRUE);
}

/* Synaptics Touchpad */
static int
enable_synaptics(struct psm_softc *sc)
{
	int status[3];
	KBDC kbdc;

	if (!synaptics_support)
		return (FALSE);

	/* Attach extra synaptics sysctl nodes under hw.psm.synaptics */
	sysctl_ctx_init(&sc->syninfo.sysctl_ctx);
	sc->syninfo.sysctl_tree = SYSCTL_ADD_NODE(&sc->syninfo.sysctl_ctx,
	    SYSCTL_STATIC_CHILDREN(_hw_psm), OID_AUTO, "synaptics", CTLFLAG_RD,
	    0, "Synaptics TouchPad");

	/*
	 * synaptics_directional_scrolls - if non-zero, the directional
	 * pad scrolls, otherwise it registers as a middle-click.
	 */
	sc->syninfo.directional_scrolls = 1;
	SYSCTL_ADD_INT(&sc->syninfo.sysctl_ctx,
	    SYSCTL_CHILDREN(sc->syninfo.sysctl_tree), OID_AUTO,
	    "directional_scrolls", CTLFLAG_RW,
	    &sc->syninfo.directional_scrolls, 0,
	    "directional pad scrolls (1=yes  0=3rd button)");

	/*
	 * Synaptics_low_speed_threshold - the number of touchpad units
	 * below-which we go into low-speed tracking mode.
	 */
	sc->syninfo.low_speed_threshold = 20;
	SYSCTL_ADD_INT(&sc->syninfo.sysctl_ctx,
	    SYSCTL_CHILDREN(sc->syninfo.sysctl_tree), OID_AUTO,
	    "low_speed_threshold", CTLFLAG_RW,
	    &sc->syninfo.low_speed_threshold, 0,
	    "threshold between low and hi speed positioning");

	/*
	 * Synaptics_min_movement - the number of touchpad units below
	 * which we ignore altogether.
	 */
	sc->syninfo.min_movement = 2;
	SYSCTL_ADD_INT(&sc->syninfo.sysctl_ctx,
	    SYSCTL_CHILDREN(sc->syninfo.sysctl_tree), OID_AUTO,
	    "min_movement", CTLFLAG_RW,
	    &sc->syninfo.min_movement, 0,
	    "ignore touchpad movements less than this");

	/*
	 * Synaptics_squelch_level - level at which we squelch movement
	 * packets.
	 *
	 * This effectively sends 1 out of every synaptics_squelch_level
	 * packets when * running in low-speed mode.
	 */
	sc->syninfo.squelch_level=3;
	SYSCTL_ADD_INT(&sc->syninfo.sysctl_ctx,
	    SYSCTL_CHILDREN(sc->syninfo.sysctl_tree), OID_AUTO,
	    "squelch_level", CTLFLAG_RW,
	    &sc->syninfo.squelch_level, 0,
	    "squelch level for synaptics touchpads");

	kbdc = sc->kbdc;
	disable_aux_dev(kbdc);
	sc->hw.buttons = 3;
	sc->squelch = 0;

	/* Just to be on the safe side */
	set_mouse_scaling(kbdc, 1);

	/* Identify the Touchpad version */
	if (mouse_ext_command(kbdc, 0) == 0)
		return (FALSE);
	if (get_mouse_status(kbdc, status, 0, 3) != 3)
		return (FALSE);
	if (status[1] != 0x47)
		return (FALSE);

	sc->synhw.infoMinor = status[0];
	sc->synhw.infoMajor = status[2] & 0x0f;

	if (verbose >= 2)
		printf("Synaptics Touchpad v%d.%d\n", sc->synhw.infoMajor,
		    sc->synhw.infoMinor);

	if (sc->synhw.infoMajor < 4) {
		printf("  Unsupported (pre-v4) Touchpad detected\n");
		return (FALSE);
	}

	/* Get the Touchpad model information */
	if (mouse_ext_command(kbdc, 3) == 0)
		return (FALSE);
	if (get_mouse_status(kbdc, status, 0, 3) != 3)
		return (FALSE);
	if ((status[1] & 0x01) != 0) {
		printf("  Failed to read model information\n");
		return (FALSE);
	}

	sc->synhw.infoRot180   = (status[0] & 0x80) >> 7;
	sc->synhw.infoPortrait = (status[0] & 0x40) >> 6;
	sc->synhw.infoSensor   =  status[0] & 0x3f;
	sc->synhw.infoHardware = (status[1] & 0xfe) >> 1;
	sc->synhw.infoNewAbs   = (status[2] & 0x80) >> 7;
	sc->synhw.capPen       = (status[2] & 0x40) >> 6;
	sc->synhw.infoSimplC   = (status[2] & 0x20) >> 5;
	sc->synhw.infoGeometry =  status[2] & 0x0f;

	if (verbose >= 2) {
		printf("  Model information:\n");
		printf("   infoRot180: %d\n", sc->synhw.infoRot180);
		printf("   infoPortrait: %d\n", sc->synhw.infoPortrait);
		printf("   infoSensor: %d\n", sc->synhw.infoSensor);
		printf("   infoHardware: %d\n", sc->synhw.infoHardware);
		printf("   infoNewAbs: %d\n", sc->synhw.infoNewAbs);
		printf("   capPen: %d\n", sc->synhw.capPen);
		printf("   infoSimplC: %d\n", sc->synhw.infoSimplC);
		printf("   infoGeometry: %d\n", sc->synhw.infoGeometry);
	}

	/* Read the extended capability bits */
	if (mouse_ext_command(kbdc, 2) == 0)
		return (FALSE);
	if (get_mouse_status(kbdc, status, 0, 3) != 3)
		return (FALSE);
	if (status[1] != 0x47) {
		printf("  Failed to read extended capability bits\n");
		return (FALSE);
	}

	/* Set the different capabilities when they exist */
	if ((status[0] & 0x80) >> 7) {
		sc->synhw.capExtended    = (status[0] & 0x80) >> 7;
		sc->synhw.capPassthrough = (status[2] & 0x80) >> 7;
		sc->synhw.capSleep       = (status[2] & 0x10) >> 4;
		sc->synhw.capFourButtons = (status[2] & 0x08) >> 3;
		sc->synhw.capMultiFinger = (status[2] & 0x02) >> 1;
		sc->synhw.capPalmDetect  = (status[2] & 0x01);

		if (verbose >= 2) {
			printf("  Extended capabilities:\n");
			printf("   capExtended: %d\n", sc->synhw.capExtended);
			printf("   capPassthrough: %d\n",
			    sc->synhw.capPassthrough);
			printf("   capSleep: %d\n", sc->synhw.capSleep);
			printf("   capFourButtons: %d\n",
			    sc->synhw.capFourButtons);
			printf("   capMultiFinger: %d\n",
			    sc->synhw.capMultiFinger);
			printf("   capPalmDetect: %d\n",
			    sc->synhw.capPalmDetect);
		}

		/*
		 * if we have bits set in status[0] & 0x70 - then we can load
		 * more information about buttons using query 0x09
		 */
		if (status[0] & 0x70) {
			if (mouse_ext_command(kbdc, 0x09) == 0)
				return (FALSE);
			if (get_mouse_status(kbdc, status, 0, 3) != 3)
				return (FALSE);
			sc->hw.buttons = ((status[1] & 0xf0) >> 4) + 3;
			if (verbose >= 2)
				printf("  Additional Buttons: %d\n",
				    sc->hw.buttons -3);
		}
	} else {
		sc->synhw.capExtended = 0;

		if (verbose >= 2)
			printf("  No extended capabilities\n");
	}

	/*
	 * Read the mode byte
	 *
	 * XXX: Note the Synaptics documentation also defines the first
	 * byte of the response to this query to be a constant 0x3b, this
	 * does not appear to be true for Touchpads with guest devices.
	 */
	if (mouse_ext_command(kbdc, 1) == 0)
		return (FALSE);
	if (get_mouse_status(kbdc, status, 0, 3) != 3)
		return (FALSE);
	if (status[1] != 0x47) {
		printf("  Failed to read mode byte\n");
		return (FALSE);
	}

	/* Set the mode byte -- request wmode where available */
	if (sc->synhw.capExtended)
		mouse_ext_command(kbdc, 0xc1);
	else
		mouse_ext_command(kbdc, 0xc0);

	/* Reset the sampling rate */
	set_mouse_sampling_rate(kbdc, 20);

	/*
	 * Report the correct number of buttons
	 *
	 * XXX: I'm not sure this is used anywhere.
	 */
	if (sc->synhw.capExtended && sc->synhw.capFourButtons)
		sc->hw.buttons = 4;

	return (TRUE);
}

/* Interlink electronics VersaPad */
static int
enable_versapad(struct psm_softc *sc)
{
	KBDC kbdc = sc->kbdc;
	int data[3];

	set_mouse_resolution(kbdc, PSMD_RES_MEDIUM_HIGH); /* set res. 2 */
	set_mouse_sampling_rate(kbdc, 100);		/* set rate 100 */
	set_mouse_scaling(kbdc, 1);			/* set scale 1:1 */
	set_mouse_scaling(kbdc, 1);			/* set scale 1:1 */
	set_mouse_scaling(kbdc, 1);			/* set scale 1:1 */
	set_mouse_scaling(kbdc, 1);			/* set scale 1:1 */
	if (get_mouse_status(kbdc, data, 0, 3) < 3)	/* get status */
		return (FALSE);
	if (data[2] != 0xa || data[1] != 0 )	/* rate == 0xa && res. == 0 */
		return (FALSE);
	set_mouse_scaling(kbdc, 1);			/* set scale 1:1 */

	sc->config |= PSM_CONFIG_HOOKRESUME | PSM_CONFIG_INITAFTERSUSPEND;

	return (TRUE);				/* PS/2 absolute mode */
}

/*
 * Return true if 'now' is earlier than (start + (secs.usecs)).
 * Now may be NULL and the function will fetch the current time from
 * getmicrouptime(), or a cached 'now' can be passed in.
 * All values should be numbers derived from getmicrouptime().
 */
static int
timeelapsed(start, secs, usecs, now)
	const struct timeval *start, *now;
	int secs, usecs;
{
	struct timeval snow, tv;

	/* if there is no 'now' passed in, the get it as a convience. */
	if (now == NULL) {
		getmicrouptime(&snow);
		now = &snow;
	}

	tv.tv_sec = secs;
	tv.tv_usec = usecs;
	timevaladd(&tv, start);
	return (timevalcmp(&tv, now, <));
}

static int
psmresume(device_t dev)
{
	struct psm_softc *sc = device_get_softc(dev);
	int unit = device_get_unit(dev);
	int err;

	VLOG(2, (LOG_NOTICE, "psm%d: system resume hook called.\n", unit));

	if (!(sc->config & PSM_CONFIG_HOOKRESUME))
		return (0);

	err = reinitialize(sc, sc->config & PSM_CONFIG_INITAFTERSUSPEND);

	if ((sc->state & PSM_ASLP) && !(sc->state & PSM_VALID)) {
		/*
		 * Release the blocked process; it must be notified that
		 * the device cannot be accessed anymore.
		 */
		sc->state &= ~PSM_ASLP;
		wakeup(sc);
	}

	VLOG(2, (LOG_DEBUG, "psm%d: system resume hook exiting.\n", unit));

	return (err);
}

DRIVER_MODULE(psm, atkbdc, psm_driver, psm_devclass, 0, 0);

#ifdef DEV_ISA

/*
 * This sucks up assignments from PNPBIOS and ACPI.
 */

/*
 * When the PS/2 mouse device is reported by ACPI or PnP BIOS, it may
 * appear BEFORE the AT keyboard controller.  As the PS/2 mouse device
 * can be probed and attached only after the AT keyboard controller is
 * attached, we shall quietly reserve the IRQ resource for later use.
 * If the PS/2 mouse device is reported to us AFTER the keyboard controller,
 * copy the IRQ resource to the PS/2 mouse device instance hanging
 * under the keyboard controller, then probe and attach it.
 */

static	devclass_t			psmcpnp_devclass;

static	device_probe_t			psmcpnp_probe;
static	device_attach_t			psmcpnp_attach;

static device_method_t psmcpnp_methods[] = {
	DEVMETHOD(device_probe,		psmcpnp_probe),
	DEVMETHOD(device_attach,	psmcpnp_attach),

	{ 0, 0 }
};

static driver_t psmcpnp_driver = {
	PSMCPNP_DRIVER_NAME,
	psmcpnp_methods,
	1,			/* no softc */
};

static struct isa_pnp_id psmcpnp_ids[] = {
	{ 0x030fd041, "PS/2 mouse port" },		/* PNP0F03 */
	{ 0x0e0fd041, "PS/2 mouse port" },		/* PNP0F0E */
	{ 0x120fd041, "PS/2 mouse port" },		/* PNP0F12 */
	{ 0x130fd041, "PS/2 mouse port" },		/* PNP0F13 */
	{ 0x1303d041, "PS/2 port" },			/* PNP0313, XXX */
	{ 0x02002e4f, "Dell PS/2 mouse port" },		/* Lat. X200, Dell */
	{ 0x0002a906, "ALPS Glide Point" },		/* ALPS Glide Point */
	{ 0x80374d24, "IBM PS/2 mouse port" },		/* IBM3780, ThinkPad */
	{ 0x81374d24, "IBM PS/2 mouse port" },		/* IBM3781, ThinkPad */
	{ 0x0190d94d, "SONY VAIO PS/2 mouse port"},     /* SNY9001, Vaio */
	{ 0x0290d94d, "SONY VAIO PS/2 mouse port"},	/* SNY9002, Vaio */
	{ 0x0390d94d, "SONY VAIO PS/2 mouse port"},	/* SNY9003, Vaio */
	{ 0x0490d94d, "SONY VAIO PS/2 mouse port"},     /* SNY9004, Vaio */
	{ 0 }
};

static int
create_a_copy(device_t atkbdc, device_t me)
{
	device_t psm;
	u_long irq;

	/* find the PS/2 mouse device instance under the keyboard controller */
	psm = device_find_child(atkbdc, PSM_DRIVER_NAME,
	    device_get_unit(atkbdc));
	if (psm == NULL)
		return (ENXIO);
	if (device_get_state(psm) != DS_NOTPRESENT)
		return (0);

	/* move our resource to the found device */
	irq = bus_get_resource_start(me, SYS_RES_IRQ, 0);
	bus_set_resource(psm, SYS_RES_IRQ, KBDC_RID_AUX, irq, 1);

	/* ...then probe and attach it */
	return (device_probe_and_attach(psm));
}

static int
psmcpnp_probe(device_t dev)
{
	struct resource *res;
	u_long irq;
	int rid;

	if (ISA_PNP_PROBE(device_get_parent(dev), dev, psmcpnp_ids))
		return (ENXIO);

	/*
	 * The PnP BIOS and ACPI are supposed to assign an IRQ (12)
	 * to the PS/2 mouse device node. But, some buggy PnP BIOS
	 * declares the PS/2 mouse device node without an IRQ resource!
	 * If this happens, we shall refer to device hints.
	 * If we still don't find it there, use a hardcoded value... XXX
	 */
	rid = 0;
	irq = bus_get_resource_start(dev, SYS_RES_IRQ, rid);
	if (irq <= 0) {
		if (resource_long_value(PSM_DRIVER_NAME,
		    device_get_unit(dev),"irq", &irq) != 0)
			irq = 12;	/* XXX */
		device_printf(dev, "irq resource info is missing; "
		    "assuming irq %ld\n", irq);
		bus_set_resource(dev, SYS_RES_IRQ, rid, irq, 1);
	}
	res = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid, RF_SHAREABLE);
	bus_release_resource(dev, SYS_RES_IRQ, rid, res);

	/* keep quiet */
	if (!bootverbose)
		device_quiet(dev);

	return ((res == NULL) ? ENXIO : 0);
}

static int
psmcpnp_attach(device_t dev)
{
	device_t atkbdc;
	int rid;

	/* find the keyboard controller, which may be on acpi* or isa* bus */
	atkbdc = devclass_get_device(devclass_find(ATKBDC_DRIVER_NAME),
	    device_get_unit(dev));
	if ((atkbdc != NULL) && (device_get_state(atkbdc) == DS_ATTACHED))
		create_a_copy(atkbdc, dev);
	else {
		/*
		 * If we don't have the AT keyboard controller yet,
		 * just reserve the IRQ for later use...
		 * (See psmidentify() above.)
		 */
		rid = 0;
		bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid, RF_SHAREABLE);
	}

	return (0);
}

DRIVER_MODULE(psmcpnp, isa, psmcpnp_driver, psmcpnp_devclass, 0, 0);
DRIVER_MODULE(psmcpnp, acpi, psmcpnp_driver, psmcpnp_devclass, 0, 0);

#endif /* DEV_ISA */
