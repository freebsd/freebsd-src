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
 *
 * $Id: psm.c,v 1.52 1998/04/15 17:06:52 phk Exp $
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
 *    Vender specific support routines.
 */

#include "psm.h"
#include "apm.h"
#include "opt_devfs.h"
#include "opt_psm.h"

#if NPSM > 0

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/poll.h>
#include <sys/syslog.h>
#include <sys/malloc.h>
#ifdef DEVFS
#include <sys/devfsext.h>
#endif
#include <sys/select.h>
#include <sys/uio.h>

#include <machine/apm_bios.h>
#include <machine/clock.h>
#include <machine/limits.h>
#include <machine/mouse.h>

#include <i386/isa/isa_device.h>
#include <i386/isa/kbdio.h>

/*
 * Driver specific options: the following options may be set by
 * `options' statements in the kernel configuration file.
 */

/* debugging */
#ifndef PSM_DEBUG
#define PSM_DEBUG	0	/* logging: 0: none, 1: brief, 2: verbose */
#endif

/* features */

/* #define PSM_HOOKAPM	   	   hook the APM resume event */
/* #define PSM_RESETAFTERSUSPEND   reset the device at the resume event */

#if NAPM <= 0
#undef PSM_HOOKAPM
#endif /* NAPM */

#ifndef PSM_HOOKAPM
#undef PSM_RESETAFTERSUSPEND
#endif /* PSM_HOOKAPM */

/* end of driver specific options */

/* input queue */
#define PSM_BUFSIZE		960
#define PSM_SMALLBUFSIZE	240

/* operation levels */
#define PSM_LEVEL_BASE		0
#define PSM_LEVEL_STANDARD	1
#define PSM_LEVEL_NATIVE	2
#define PSM_LEVEL_MIN		PSM_LEVEL_BASE
#define PSM_LEVEL_MAX		PSM_LEVEL_NATIVE

/* some macros */
#define PSM_UNIT(dev)		(minor(dev) >> 1)
#define PSM_NBLOCKIO(dev)	(minor(dev) & 1)
#define PSM_MKMINOR(unit,block)	(((unit) << 1) | ((block) ? 0:1))

#ifndef max
#define max(x,y)		((x) > (y) ? (x) : (y))
#endif
#ifndef min
#define min(x,y)		((x) < (y) ? (x) : (y))
#endif

/* ring buffer */
typedef struct ringbuf {
    int           count;	/* # of valid elements in the buffer */
    int           head;		/* head pointer */
    int           tail;		/* tail poiner */
    unsigned char buf[PSM_BUFSIZE];
} ringbuf_t;

/* driver control block */
static struct psm_softc {    /* Driver status information */
    struct selinfo rsel;	/* Process selecting for Input */
    unsigned char state;	/* Mouse driver state */
    int           config;	/* driver configuration flags */
    int           flags;	/* other flags */
    KBDC          kbdc;		/* handle to access the keyboard controller */
    int           addr;		/* I/O port address */
    mousehw_t     hw;		/* hardware information */
    mousemode_t   mode;		/* operation mode */
    mousemode_t   dflt_mode;	/* default operation mode */
    mousestatus_t status;	/* accumulated mouse movement */
    ringbuf_t     queue;	/* mouse status queue */
    unsigned char ipacket[16];	/* interim input buffer */
    int           inputbytes;	/* # of bytes in the input buffer */
    int           button;	/* the latest button state */
#ifdef DEVFS
    void          *devfs_token;
    void          *b_devfs_token;
#endif
#ifdef PSM_HOOKAPM
    struct apmhook resumehook;
#endif
} *psm_softc[NPSM];

/* driver state flags (state) */
#define PSM_VALID		0x80
#define PSM_OPEN		1	/* Device is open */
#define PSM_ASLP		2	/* Waiting for mouse data */

/* driver configuration flags (config) */
#define PSM_CONFIG_RESOLUTION	0x000f	/* resolution */
#define PSM_CONFIG_ACCEL	0x00f0  /* acceleration factor */
#define PSM_CONFIG_NOCHECKSYNC	0x0100  /* disable sync. test */

#define PSM_CONFIG_FLAGS	(PSM_CONFIG_RESOLUTION 		\
				    | PSM_CONFIG_ACCEL		\
				    | PSM_CONFIG_NOCHECKSYNC)

/* other flags (flags) */
/*
 * Pass mouse data packet to the user land program `as is', even if 
 * the mouse has vender-specific enhanced features and uses non-standard 
 * packet format.  Otherwise manipulate the mouse data packet so that 
 * it can be recognized by the programs which can only understand 
 * the standard packet format.
*/
#define PSM_FLAGS_NATIVEMODE 	0x0200

/* for backward compatibility */
#define OLD_MOUSE_GETHWINFO	_IOR('M', 1, old_mousehw_t)
#define OLD_MOUSE_GETMODE	_IOR('M', 2, old_mousemode_t)
#define OLD_MOUSE_SETMODE	_IOW('M', 3, old_mousemode_t)

typedef struct old_mousehw {
    int buttons;
    int iftype;
    int type;
    int hwid;
} old_mousehw_t;

typedef struct old_mousemode {
    int protocol;
    int rate;
    int resolution;
    int accelfactor;
} old_mousemode_t;

/* packet formatting function */
typedef int packetfunc_t __P((struct psm_softc *, unsigned char *,
			      int *, int, mousestatus_t *));

/* function prototypes */
static int psmprobe __P((struct isa_device *));
static int psmattach __P((struct isa_device *));
static void psm_drvinit __P((void *));
#ifdef PSM_HOOKAPM
static int psmresume __P((void *));
#endif

static d_open_t psmopen;
static d_close_t psmclose;
static d_read_t psmread;
static d_ioctl_t psmioctl;
static d_poll_t psmpoll;

static int enable_aux_dev __P((KBDC));
static int disable_aux_dev __P((KBDC));
static int get_mouse_status __P((KBDC, int *, int, int));
static int get_aux_id __P((KBDC));
static int set_mouse_sampling_rate __P((KBDC, int));
static int set_mouse_scaling __P((KBDC, int));
static int set_mouse_resolution __P((KBDC, int));
static int set_mouse_mode __P((KBDC));
static int get_mouse_buttons __P((KBDC));
static int is_a_mouse __P((int));
static void recover_from_error __P((KBDC));
static int restore_controller __P((KBDC, int));
static int reinitialize __P((int, mousemode_t *));
static int doopen __P((int, int));
static char *model_name(int);

/* vender specific features */
typedef int probefunc_t __P((struct psm_softc *));

static int mouse_id_proc1 __P((KBDC, int, int, int *));
static probefunc_t enable_groller;
static probefunc_t enable_gmouse;
static probefunc_t enable_aglide; 
static probefunc_t enable_kmouse;
static probefunc_t enable_msintelli;
static probefunc_t enable_mmanplus;
static int tame_mouse __P((struct psm_softc *, mousestatus_t *, unsigned char *));

static struct {
    int                 model;
    unsigned char	syncmask;
    int 		packetsize;
    probefunc_t 	*probefunc;
} vendertype[] = {
    { MOUSE_MODEL_NET,			/* Genius NetMouse */
      0xc8, MOUSE_INTELLI_PACKETSIZE, enable_gmouse, },
    { MOUSE_MODEL_NETSCROLL,		/* Genius NetScroll */
      0xc8, 6, enable_groller, },
    { MOUSE_MODEL_GLIDEPOINT,		/* ALPS GlidePoint */
      0xc0, MOUSE_PS2_PACKETSIZE, enable_aglide, },
    { MOUSE_MODEL_MOUSEMANPLUS,		/* Logitech MouseMan+ */
      0x08, MOUSE_PS2_PACKETSIZE, enable_mmanplus, },
    { MOUSE_MODEL_THINK,		/* Kensignton ThinkingMouse */
      0x80, MOUSE_PS2_PACKETSIZE, enable_kmouse, },
    { MOUSE_MODEL_INTELLI,		/* Microsoft IntelliMouse */
      0xc8, MOUSE_INTELLI_PACKETSIZE, enable_msintelli, },
    { MOUSE_MODEL_GENERIC,
      0xc0, MOUSE_PS2_PACKETSIZE, NULL, },
};

/* device driver declarateion */
struct isa_driver psmdriver = { psmprobe, psmattach, "psm", FALSE };
#define CDEV_MAJOR        21

static struct  cdevsw psm_cdevsw = {
	psmopen,	psmclose,	psmread,	nowrite,	/* 21 */
	psmioctl,	nostop,		nullreset,	nodevtotty,
	psmpoll,	nommap,		NULL,		"psm",	NULL,	-1
};

/* debug message level */
static int verbose = PSM_DEBUG;

/* device I/O routines */
static int
enable_aux_dev(KBDC kbdc)
{
    int res;

    res = send_aux_command(kbdc, PSMC_ENABLE_DEV);
    if (verbose >= 2)
        log(LOG_DEBUG, "psm: ENABLE_DEV return code:%04x\n", res);

    return (res == PSM_ACK);
}

static int
disable_aux_dev(KBDC kbdc)
{
    int res;

    res = send_aux_command(kbdc, PSMC_DISABLE_DEV);
    if (verbose >= 2)
        log(LOG_DEBUG, "psm: DISABLE_DEV return code:%04x\n", res);

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
    if (verbose >= 2)
        log(LOG_DEBUG, "psm: SEND_AUX_DEV_%s return code:%04x\n", 
	    (flag == 1) ? "DATA" : "STATUS", res);
    if (res != PSM_ACK)
        return 0;

    for (i = 0; i < len; ++i) {
        status[i] = read_aux_data(kbdc);
	if (status[i] < 0)
	    break;
    }

    if (verbose) {
        log(LOG_DEBUG, "psm: %s %02x %02x %02x\n",
            (flag == 1) ? "data" : "status", status[0], status[1], status[2]);
    }

    return i;
}

static int
get_aux_id(KBDC kbdc)
{
    int res;
    int id;

    empty_aux_buffer(kbdc, 5);
    res = send_aux_command(kbdc, PSMC_SEND_DEV_ID);
    if (verbose >= 2)
        log(LOG_DEBUG, "psm: SEND_DEV_ID return code:%04x\n", res);
    if (res != PSM_ACK)
	return (-1);

    /* 10ms delay */
    DELAY(10000);

    id = read_aux_data(kbdc);
    if (verbose >= 2)
        log(LOG_DEBUG, "psm: device ID: %04x\n", id);

    return id;
}

static int
set_mouse_sampling_rate(KBDC kbdc, int rate)
{
    int res;

    res = send_aux_command_and_data(kbdc, PSMC_SET_SAMPLING_RATE, rate);
    if (verbose >= 2)
        log(LOG_DEBUG, "psm: SET_SAMPLING_RATE (%d) %04x\n", rate, res);

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
    if (verbose >= 2)
        log(LOG_DEBUG, "psm: SET_SCALING%s return code:%04x\n", 
	    (scale == PSMC_SET_SCALING21) ? "21" : "11", res);

    return (res == PSM_ACK);
}

/* `val' must be 0 through PSMD_MAX_RESOLUTION */
static int
set_mouse_resolution(KBDC kbdc, int val)
{
    int res;

    res = send_aux_command_and_data(kbdc, PSMC_SET_RESOLUTION, val);
    if (verbose >= 2)
        log(LOG_DEBUG, "psm: SET_RESOLUTION (%d) %04x\n", val, res);

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
    if (verbose >= 2)
        log(LOG_DEBUG, "psm: SET_STREAM_MODE return code:%04x\n", res);

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
        return c;
    if (set_mouse_scaling(kbdc, 1) && set_mouse_scaling(kbdc, 1)
        && set_mouse_scaling(kbdc, 1) 
	&& (get_mouse_status(kbdc, status, 0, 3) >= 3)) {
        if (status[1] != 0)
            return status[1];
    }
    return c;
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
        -1			/* end of table */
    };
    int i;

    for (i = 0; valid_ids[i] >= 0; ++i)
        if (valid_ids[i] == id)
            return TRUE;
    return FALSE;
#else
    return TRUE;
#endif
}

static char *
model_name(int model)
{
    static struct {
	int model_code;
	char *model_name;
    } models[] = {
        { MOUSE_MODEL_NETSCROLL,	"NetScroll Mouse" },
        { MOUSE_MODEL_NET,		"NetMouse" },
        { MOUSE_MODEL_GLIDEPOINT,	"GlidePoint" },
        { MOUSE_MODEL_THINK,		"ThinkingMouse" },
        { MOUSE_MODEL_INTELLI,		"IntelliMouse" },
        { MOUSE_MODEL_MOUSEMANPLUS,	"MouseMan+" },
        { MOUSE_MODEL_GENERIC,		"Generic PS/2 mouse" },
        { MOUSE_MODEL_UNKNOWN,		NULL },
    };
    int i;

    for (i = 0; models[i].model_code != MOUSE_MODEL_UNKNOWN; ++i) {
	if (models[i].model_code == model)
	    return models[i].model_name;
    }
    return "Unknown";
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
    if (test_kbd_port(kbdc) != 0) {
	if (verbose)
	    log(LOG_ERR, "psm: keyboard port failed.\n");
    }
#endif
}

static int
restore_controller(KBDC kbdc, int command_byte)
{
    empty_both_buffers(kbdc, 10);

    if (!set_controller_command_byte(kbdc, 0xff, command_byte)) {
	log(LOG_ERR, "psm: failed to restore the keyboard controller "
		     "command byte.\n");
	return FALSE;
    } else {
	return TRUE;
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
reinitialize(int unit, mousemode_t *mode)
{
    struct psm_softc *sc = psm_softc[unit];
    KBDC kbdc = psm_softc[unit]->kbdc;
    int stat[3];
    int i;

    switch((i = test_aux_port(kbdc))) {
    case 1:	/* ignore this error */
	if (verbose)
	    log(LOG_DEBUG, "psm%d: strange result for test aux port (%d).\n",
	        unit, i);
	/* fall though */
    case 0:	/* no error */
    	break;
    case -1: 	/* time out */
    default: 	/* error */
    	recover_from_error(kbdc);
    	log(LOG_ERR, "psm%d: the aux port is not functioning (%d).\n",
    	    unit, i);
    	return FALSE;
    }

    /* 
     * NOTE: some controllers appears to hang the `keyboard' when
     * the aux port doesn't exist and `PSMC_RESET_DEV' is issued. 
     */
    if (!reset_aux_dev(kbdc)) {
        recover_from_error(kbdc);
        log(LOG_ERR, "psm%d: failed to reset the aux device.\n", unit);
        return FALSE;
    }

    /* 
     * both the aux port and the aux device is functioning, see
     * if the device can be enabled. 
     */
    if (!enable_aux_dev(kbdc) || !disable_aux_dev(kbdc)) {
        log(LOG_ERR, "psm%d: failed to enable the aux device.\n", unit);
        return FALSE;
    }
    empty_both_buffers(kbdc, 10);	/* remove stray data if any */

    /* FIXME: hardware ID, mouse buttons? */

    /* other parameters */
    for (i = 0; vendertype[i].probefunc != NULL; ++i) {
	if ((*vendertype[i].probefunc)(sc)) {
	    if (verbose >= 2)
		log(LOG_ERR, "psm%d: found %s\n", 
		    unit, model_name(vendertype[i].model));
	    break;
	}
    }

    sc->hw.model = vendertype[i].model;
    sc->mode.packetsize = vendertype[i].packetsize;

    /* set mouse parameters */
    if (mode != (mousemode_t *)NULL) {
	if (mode->rate > 0)
            mode->rate = set_mouse_sampling_rate(kbdc, mode->rate);
	if (mode->resolution >= 0)
            mode->resolution = set_mouse_resolution(kbdc, mode->resolution);
        set_mouse_scaling(kbdc, 1);
        set_mouse_mode(kbdc);	
    }

    /* request a data packet and extract sync. bits */
    if (get_mouse_status(kbdc, stat, 1, 3) < 3) {
        log(LOG_DEBUG, "psm%d: failed to get data (reinitialize).\n", unit);
        sc->mode.syncmask[0] = 0;
    } else {
        sc->mode.syncmask[1] = stat[0] & sc->mode.syncmask[0];	/* syncbits */
	/* the NetScroll Mouse will send three more bytes... Ignore them */
	empty_aux_buffer(kbdc, 5);
    }

    /* just check the status of the mouse */
    if (get_mouse_status(kbdc, stat, 0, 3) < 3)
        log(LOG_DEBUG, "psm%d: failed to get status (reinitialize).\n", unit);

    return TRUE;
}

static int
doopen(int unit, int command_byte)
{
    struct psm_softc *sc = psm_softc[unit];
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
	if (!reinitialize(unit, &sc->mode) || !enable_aux_dev(sc->kbdc)) {
	    recover_from_error(sc->kbdc);
#else
        {
#endif
            restore_controller(sc->kbdc, command_byte);
	    /* mark this device is no longer available */
	    sc->state &= ~PSM_VALID;	
	    log(LOG_ERR, "psm%d: failed to enable the device (doopen).\n",
		unit);
	    return (EIO);
	}
    }

    if (get_mouse_status(sc->kbdc, stat, 0, 3) < 3) 
        log(LOG_DEBUG, "psm%d: failed to get status (doopen).\n", unit);

    /* enable the aux port and interrupt */
    if (!set_controller_command_byte(sc->kbdc, 
	    kbdc_get_device_mask(sc->kbdc),
	    (command_byte & KBD_KBD_CONTROL_BITS)
		| KBD_ENABLE_AUX_PORT | KBD_ENABLE_AUX_INT)) {
	/* CONTROLLER ERROR */
	disable_aux_dev(sc->kbdc);
        restore_controller(sc->kbdc, command_byte);
	log(LOG_ERR, "psm%d: failed to enable the aux interrupt (doopen).\n",
	    unit);
	return (EIO);
    }

    return (0);
}

/* psm driver entry points */

#define endprobe(v)	{   if (bootverbose) 				\
				--verbose;   				\
                            kbdc_set_device_mask(sc->kbdc, mask);	\
			    kbdc_lock(sc->kbdc, FALSE);			\
 	                    free(sc, M_DEVBUF);                         \
			    return (v);	     				\
			}

static int
psmprobe(struct isa_device *dvp)
{
    int unit = dvp->id_unit;
    struct psm_softc *sc;
    int stat[3];
    int command_byte;
    int mask;
    int i;

    /* validate unit number */
    if (unit >= NPSM)
        return (0);

    psm_softc[unit] = NULL;

    sc =  malloc(sizeof *sc, M_DEVBUF, M_NOWAIT);
    if (sc == NULL)
        return (0);
    bzero(sc, sizeof *sc);

#if 0
    kbdc_debug(TRUE);
#endif
    sc->addr = dvp->id_iobase;
    sc->kbdc = kbdc_open(sc->addr);
    sc->config = dvp->id_flags & PSM_CONFIG_FLAGS;
    sc->flags = 0;
    if (bootverbose)
        ++verbose;

    if (!kbdc_lock(sc->kbdc, TRUE)) {
        printf("psm%d: unable to lock the controller.\n", unit);
        if (bootverbose)
            --verbose;
        free(sc, M_DEVBUF);
	return (0);
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
        printf("psm%d: current command byte:%04x\n", unit, command_byte);
    if (command_byte == -1) {
        /* CONTROLLER ERROR */
        printf("psm%d: unable to get the current command byte value.\n",
            unit);
        endprobe(0);
    }

    /*
     * disable the keyboard port while probing the aux port, which must be
     * enabled during this routine
     */
    if (!set_controller_command_byte(sc->kbdc,
	    KBD_KBD_CONTROL_BITS | KBD_AUX_CONTROL_BITS,
  	    KBD_DISABLE_KBD_PORT | KBD_DISABLE_KBD_INT
                | KBD_ENABLE_AUX_PORT | KBD_DISABLE_AUX_INT)) {
        /* 
	 * this is CONTROLLER ERROR; I don't know how to recover 
         * from this error... 
	 */
        restore_controller(sc->kbdc, command_byte);
        printf("psm%d: unable to set the command byte.\n", unit);
        endprobe(0);
    }

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
     * XXX: some controllers erroneously return the error code 1 when
     * it has the perfectly functional aux port. We have to ignore this
     * error code. Even if the controller HAS error with the aux port,
     * it will be detected later...
     */
    switch ((i = test_aux_port(sc->kbdc))) {
    case 1:	   /* ignore this error */
        if (verbose)
	    printf("psm%d: strange result for test aux port (%d).\n",
	        unit, i);
	/* fall though */
    case 0:        /* no error */
        break;
    case -1:        /* time out */
    default:        /* error */
        recover_from_error(sc->kbdc);
        restore_controller(sc->kbdc, command_byte);
        if (verbose)
            printf("psm%d: the aux port is not functioning (%d).\n",
                unit, i);
        endprobe(0);
    }

    /*
     * NOTE: some controllers appears to hang the `keyboard' when the aux
     * port doesn't exist and `PSMC_RESET_DEV' is issued.
     */
    if (!reset_aux_dev(sc->kbdc)) {
        recover_from_error(sc->kbdc);
        restore_controller(sc->kbdc, command_byte);
        if (verbose)
            printf("psm%d: failed to reset the aux device.\n", unit);
        endprobe(0);
    }
    /*
     * both the aux port and the aux device is functioning, see if the
     * device can be enabled. NOTE: when enabled, the device will start
     * sending data; we shall immediately disable the device once we know
     * the device can be enabled.
     */
    if (!enable_aux_dev(sc->kbdc) || !disable_aux_dev(sc->kbdc)) {
	/* MOUSE ERROR */
        restore_controller(sc->kbdc, command_byte);
        if (verbose)
            printf("psm%d: failed to enable the aux device.\n", unit);
        endprobe(0);
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
            printf("psm%d: unknown device type (%d).\n", unit, sc->hw.hwid);
        endprobe(0);
    }
    switch (sc->hw.hwid) {
    case PSM_BALLPOINT_ID:
        sc->hw.type = MOUSE_TRACKBALL;
        break;
    case PSM_MOUSE_ID:
    case PSM_INTELLI_ID:
        sc->hw.type = MOUSE_MOUSE;
        break;
    default:
        sc->hw.type = MOUSE_UNKNOWN;
        break;
    }

    /* # of buttons */
    sc->hw.buttons = get_mouse_buttons(sc->kbdc);

    /* other parameters */
    for (i = 0; vendertype[i].probefunc != NULL; ++i) {
	if ((*vendertype[i].probefunc)(sc)) {
	    if (verbose >= 2)
		printf("psm%d: found %s\n",
		    unit, model_name(vendertype[i].model));
	    break;
	}
    }

    sc->hw.model = vendertype[i].model;

    sc->dflt_mode.level = PSM_LEVEL_BASE;
    sc->dflt_mode.packetsize = MOUSE_PS2_PACKETSIZE;
    sc->dflt_mode.accelfactor = (sc->config & PSM_CONFIG_ACCEL) >> 4;
    if (sc->config & PSM_CONFIG_NOCHECKSYNC)
        sc->dflt_mode.syncmask[0] = 0;
    else
        sc->dflt_mode.syncmask[0] = vendertype[i].syncmask;
    sc->dflt_mode.syncmask[1] = 0;	/* syncbits */
    sc->mode = sc->dflt_mode;
    sc->mode.packetsize = vendertype[i].packetsize;

    /* set mouse parameters */
    i = send_aux_command(sc->kbdc, PSMC_SET_DEFAULTS);
    if (verbose >= 2)
	printf("psm%d: SET_DEFAULTS return code:%04x\n", unit, i);
    if (sc->config & PSM_CONFIG_RESOLUTION) {
        sc->mode.resolution
	    = set_mouse_resolution(sc->kbdc, 
	        (sc->config & PSM_CONFIG_RESOLUTION) - 1);
    }

    /* request a data packet and extract sync. bits */
    if (get_mouse_status(sc->kbdc, stat, 1, 3) < 3) {
        printf("psm%d: failed to get data.\n", unit);
        sc->mode.syncmask[0] = 0;
    } else {
        sc->mode.syncmask[1] = stat[0] & sc->mode.syncmask[0];	/* syncbits */
	/* the NetScroll Mouse will send three more bytes... Ignore them */
	empty_aux_buffer(sc->kbdc, 5);
    }

    /* just check the status of the mouse */
    /* 
     * NOTE: XXX there are some arcane controller/mouse combinations out 
     * there, which hung the controller unless there is data transmission 
     * after ACK from the mouse.
     */
    if (get_mouse_status(sc->kbdc, stat, 0, 3) < 3) {
        printf("psm%d: failed to get status.\n", unit);
    } else {
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
            (command_byte & KBD_KBD_CONTROL_BITS)
                | KBD_DISABLE_AUX_PORT | KBD_DISABLE_AUX_INT)) {
        /* 
	 * this is CONTROLLER ERROR; I don't know the proper way to 
         * recover from this error... 
	 */
        restore_controller(sc->kbdc, command_byte);
        printf("psm%d: unable to set the command byte.\n", unit);
        endprobe(0);
    }

    /* done */
    psm_softc[unit] = sc;
    kbdc_set_device_mask(sc->kbdc, mask | KBD_AUX_CONTROL_BITS);
    kbdc_lock(sc->kbdc, FALSE);
    return (IO_PSMSIZE);
}

static int
psmattach(struct isa_device *dvp)
{
    int unit = dvp->id_unit;
    struct psm_softc *sc = psm_softc[unit];

    if (sc == NULL)    /* shouldn't happen */
	return (0);

    /* Setup initial state */
    sc->state = PSM_VALID;

    /* Done */
#ifdef    DEVFS
    sc->devfs_token =
        devfs_add_devswf(&psm_cdevsw, PSM_MKMINOR(unit, FALSE),
        DV_CHR, 0, 0, 0666, "psm%d", unit);
    sc->b_devfs_token =
        devfs_add_devswf(&psm_cdevsw, PSM_MKMINOR(unit, TRUE),
        DV_CHR, 0, 0, 0666, "bpsm%d", unit);
#endif /* DEVFS */

#ifdef PSM_HOOKAPM
    sc->resumehook.ah_name = "PS/2 mouse";
    sc->resumehook.ah_fun = psmresume;
    sc->resumehook.ah_arg = (void *)unit;
    sc->resumehook.ah_order = APM_MID_ORDER;
    apm_hook_establish(APM_HOOK_RESUME , &sc->resumehook);
    if (verbose)
        printf("psm%d: APM hooks installed.\n", unit);
#endif /* PSM_HOOKAPM */

    if (!verbose) {
        printf("psm%d: model %s, device ID %d\n", 
	    unit, model_name(sc->hw.model), sc->hw.hwid);
    } else {
        printf("psm%d: model %s, device ID %d, %d buttons\n",
	    unit, model_name(sc->hw.model), sc->hw.hwid, sc->hw.buttons);
	printf("psm%d: config:%08x, flags:%08x, packet size:%d\n",
	    unit, sc->config, sc->flags, sc->mode.packetsize);
	printf("psm%d: syncmask:%02x, syncbits:%02x\n",
	    unit, sc->mode.syncmask[0], sc->mode.syncmask[1]);
    }

    if (bootverbose)
        --verbose;

    return (1);
}

static int
psmopen(dev_t dev, int flag, int fmt, struct proc *p)
{
    int unit = PSM_UNIT(dev);
    struct psm_softc *sc;
    int command_byte;
    int err;
    int s;

    /* Validate unit number */
    if (unit >= NPSM)
        return (ENXIO);

    /* Get device data */
    sc = psm_softc[unit];
    if ((sc == NULL) || (sc->state & PSM_VALID) == 0)
	/* the device is no longer valid/functioning */
        return (ENXIO);

    /* Disallow multiple opens */
    if (sc->state & PSM_OPEN)
        return (EBUSY);

    /* Initialize state */
    sc->rsel.si_flags = 0;
    sc->rsel.si_pid = 0;
    sc->mode.level = sc->dflt_mode.level;
    sc->mode.protocol = sc->dflt_mode.protocol;

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

    /* empty input buffer */
    bzero(sc->ipacket, sizeof(sc->ipacket));
    sc->inputbytes = 0;

    /* don't let timeout routines in the keyboard driver to poll the kbdc */
    if (!kbdc_lock(sc->kbdc, TRUE))
	return (EIO);

    /* save the current controller command byte */
    s = spltty();
    command_byte = get_controller_command_byte(sc->kbdc);

    /* enable the aux port and temporalily disable the keyboard */
    if ((command_byte == -1) 
        || !set_controller_command_byte(sc->kbdc,
	    kbdc_get_device_mask(sc->kbdc),
  	    KBD_DISABLE_KBD_PORT | KBD_DISABLE_KBD_INT
	        | KBD_ENABLE_AUX_PORT | KBD_DISABLE_AUX_INT)) {
        /* CONTROLLER ERROR; do you know how to get out of this? */
        kbdc_lock(sc->kbdc, FALSE);
	splx(s);
	log(LOG_ERR, "psm%d: unable to set the command byte (psmopen).\n",
	    unit);
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
    err = doopen(unit, command_byte);

    /* done */
    if (err == 0) 
        sc->state |= PSM_OPEN;
    kbdc_lock(sc->kbdc, FALSE);
    return (err);
}

static int
psmclose(dev_t dev, int flag, int fmt, struct proc *p)
{
    struct psm_softc *sc = psm_softc[PSM_UNIT(dev)];
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
  	    KBD_DISABLE_KBD_PORT | KBD_DISABLE_KBD_INT
	        | KBD_ENABLE_AUX_PORT | KBD_DISABLE_AUX_INT)) {
	log(LOG_ERR, "psm%d: failed to disable the aux int (psmclose).\n",
	    PSM_UNIT(dev));
	/* CONTROLLER ERROR;
	 * NOTE: we shall force our way through. Because the only
	 * ill effect we shall see is that we may not be able
	 * to read ACK from the mouse, and it doesn't matter much 
	 * so long as the mouse will accept the DISABLE command.
	 */
    }
    splx(s);

    /* remove anything left in the output buffer */
    empty_aux_buffer(sc->kbdc, 10);

    /* disable the aux device, port and interrupt */
    if (sc->state & PSM_VALID) {
        if (!disable_aux_dev(sc->kbdc)) {
	    /* MOUSE ERROR; 
	     * NOTE: we don't return error and continue, pretending 
	     * we have successfully disabled the device. It's OK because 
	     * the interrupt routine will discard any data from the mouse
	     * hereafter. 
	     */
	    log(LOG_ERR, "psm%d: failed to disable the device (psmclose).\n",
	        PSM_UNIT(dev));
        }

        if (get_mouse_status(sc->kbdc, stat, 0, 3) < 3)
            log(LOG_DEBUG, "psm%d: failed to get status (psmclose).\n", 
	        PSM_UNIT(dev));
    }

    if (!set_controller_command_byte(sc->kbdc, 
	    kbdc_get_device_mask(sc->kbdc),
	    (command_byte & KBD_KBD_CONTROL_BITS)
	        | KBD_DISABLE_AUX_PORT | KBD_DISABLE_AUX_INT)) {
	/* CONTROLLER ERROR; 
	 * we shall ignore this error; see the above comment.
	 */
	log(LOG_ERR, "psm%d: failed to disable the aux port (psmclose).\n",
	    PSM_UNIT(dev));
    }

    /* remove anything left in the output buffer */
    empty_aux_buffer(sc->kbdc, 10);

    /* close is almost always successful */
    sc->state &= ~PSM_OPEN;
    kbdc_lock(sc->kbdc, FALSE);
    return (0);
}

static int
tame_mouse(struct psm_softc *sc, mousestatus_t *status, unsigned char *buf)
{
    static unsigned char butmapps2[8] = {
        0,
        MOUSE_PS2_BUTTON1DOWN, 
        MOUSE_PS2_BUTTON2DOWN,
        MOUSE_PS2_BUTTON1DOWN | MOUSE_PS2_BUTTON2DOWN,
        MOUSE_PS2_BUTTON3DOWN,
        MOUSE_PS2_BUTTON1DOWN | MOUSE_PS2_BUTTON3DOWN,
        MOUSE_PS2_BUTTON2DOWN | MOUSE_PS2_BUTTON3DOWN,
        MOUSE_PS2_BUTTON1DOWN | MOUSE_PS2_BUTTON2DOWN | MOUSE_PS2_BUTTON3DOWN,
    };
    static unsigned char butmapmsc[8] = {
        MOUSE_MSC_BUTTON1UP | MOUSE_MSC_BUTTON2UP | MOUSE_MSC_BUTTON3UP,
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
        i = max(min(status->dx, 255), -256);
	if (i < 0)
	    buf[0] |= MOUSE_PS2_XNEG;
        buf[1] = i;
        i = max(min(status->dy, 255), -256);
	if (i < 0)
	    buf[0] |= MOUSE_PS2_YNEG;
        buf[2] = i;
	return MOUSE_PS2_PACKETSIZE;
    } else if (sc->mode.level == PSM_LEVEL_STANDARD) {
        buf[0] = MOUSE_MSC_SYNC | butmapmsc[status->button & MOUSE_STDBUTTONS];
        i = max(min(status->dx, 255), -256);
        buf[1] = i >> 1;
        buf[3] = i - buf[1];
        i = max(min(status->dy, 255), -256);
        buf[2] = i >> 1;
        buf[4] = i - buf[2];
        i = max(min(status->dz, 127), -128);
        buf[5] = (i >> 1) & 0x7f;
        buf[6] = (i - (i >> 1)) & 0x7f;
        buf[7] = (~status->button >> 3) & 0x7f;
	return MOUSE_SYS_PACKETSIZE;
    }
    return sc->inputbytes;;
}

static int
psmread(dev_t dev, struct uio *uio, int flag)
{
    register struct psm_softc *sc = psm_softc[PSM_UNIT(dev)];
    unsigned char buf[PSM_SMALLBUFSIZE];
    int error = 0;
    int s;
    int l;

    if ((sc->state & PSM_VALID) == 0)
	return EIO;

    /* block until mouse activity occured */
    s = spltty();
    while (sc->queue.count <= 0) {
        if (PSM_NBLOCKIO(dev)) {
            splx(s);
            return EWOULDBLOCK;
        }
        sc->state |= PSM_ASLP;
        error = tsleep((caddr_t) sc, PZERO | PCATCH, "psmrea", 0);
        sc->state &= ~PSM_ASLP;
        if (error) {
            splx(s);
            return error;
        } else if ((sc->state & PSM_VALID) == 0) {
            /* the device disappeared! */
            splx(s);
            return EIO;
	}
    }
    splx(s);

    /* copy data to the user land */
    while ((sc->queue.count > 0) && (uio->uio_resid > 0)) {
        s = spltty();
	l = min(sc->queue.count, uio->uio_resid);
	if (l > sizeof(buf))
	    l = sizeof(buf);
	if (l > sizeof(sc->queue.buf) - sc->queue.head) {
	    bcopy(&sc->queue.buf[sc->queue.head], &buf[0], 
		sizeof(sc->queue.buf) - sc->queue.head);
	    bcopy(&sc->queue.buf[0], 
		&buf[sizeof(sc->queue.buf) - sc->queue.head],
		l - (sizeof(sc->queue.buf) - sc->queue.head));
	} else {
	    bcopy(&sc->queue.buf[sc->queue.head], &buf[0], l);
	}
	sc->queue.count -= l;
	sc->queue.head = (sc->queue.head + l) % sizeof(sc->queue.buf);
        splx(s);
        error = uiomove(buf, l, uio);
        if (error)
	    break;
    }

    return error;
}

static int
block_mouse_data(struct psm_softc *sc, int *c)
{
    int s;

    if (!kbdc_lock(sc->kbdc, TRUE)) 
	return EIO;

    s = spltty();
    *c = get_controller_command_byte(sc->kbdc);
    if ((*c == -1) 
	|| !set_controller_command_byte(sc->kbdc, 
	    kbdc_get_device_mask(sc->kbdc),
            KBD_DISABLE_KBD_PORT | KBD_DISABLE_KBD_INT
                | KBD_ENABLE_AUX_PORT | KBD_DISABLE_AUX_INT)) {
        /* this is CONTROLLER ERROR */
	splx(s);
        kbdc_lock(sc->kbdc, FALSE);
	return EIO;
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
    empty_aux_buffer(sc->kbdc, 0);	/* flush the queue */
    read_aux_data_no_wait(sc->kbdc);	/* throw away data if any */
    sc->inputbytes = 0;
    splx(s);

    return 0;
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
        /* CONTROLLER ERROR; this is serious, we may have
         * been left with the inaccessible keyboard and
         * the disabled mouse interrupt. 
         */
        error = EIO;
    }

    kbdc_lock(sc->kbdc, FALSE);
    return error;
}

static int
psmioctl(dev_t dev, u_long cmd, caddr_t addr, int flag, struct proc *p)
{
    struct psm_softc *sc = psm_softc[PSM_UNIT(dev)];
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
        ((old_mousehw_t *)addr)->hwid = sc->hw.hwid;
	splx(s);
        break;

    case MOUSE_GETHWINFO:
	s = spltty();
        *(mousehw_t *)addr = sc->hw;
	if (sc->mode.level == PSM_LEVEL_BASE)
	    ((mousehw_t *)addr)->model = MOUSE_MODEL_GENERIC;
	splx(s);
        break;

    case OLD_MOUSE_GETMODE:
	s = spltty();
	switch (sc->mode.level) {
	case PSM_LEVEL_BASE:
	    ((old_mousemode_t *)addr)->protocol = MOUSE_PROTO_PS2;
	    break;
	case PSM_LEVEL_STANDARD:
	    ((old_mousemode_t *)addr)->protocol = MOUSE_PROTO_SYSMOUSE;
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
        ((mousemode_t *)addr)->resolution = 
	    MOUSE_RES_LOW - sc->mode.resolution;
	switch (sc->mode.level) {
	case PSM_LEVEL_BASE:
	    ((mousemode_t *)addr)->protocol = MOUSE_PROTO_PS2;
	    ((mousemode_t *)addr)->packetsize = MOUSE_PS2_PACKETSIZE;
	    break;
	case PSM_LEVEL_STANDARD:
	    ((mousemode_t *)addr)->protocol = MOUSE_PROTO_SYSMOUSE;
	    ((mousemode_t *)addr)->packetsize = MOUSE_SYS_PACKETSIZE;
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
	        mode.resolution = -((old_mousemode_t *)addr)->resolution - 1;
	    mode.accelfactor = ((old_mousemode_t *)addr)->accelfactor;
	    mode.level = -1;
	} else {
	    mode = *(mousemode_t *)addr;
	}

	/* adjust and validate parameters. */
	if (mode.rate > UCHAR_MAX)
	    return EINVAL;
        if (mode.rate == 0)
            mode.rate = sc->dflt_mode.rate;
	else if (mode.rate == -1)
	    /* don't change the current setting */
	    ;
	else if (mode.rate < 0)
	    return EINVAL;
	if (mode.resolution >= UCHAR_MAX)
	    return EINVAL;
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
	else if ((mode.level < PSM_LEVEL_MIN) || (mode.level > PSM_LEVEL_MAX))
	    return EINVAL;
        if (mode.accelfactor == -1)
	    /* don't change the current setting */
	    mode.accelfactor = sc->mode.accelfactor;
        else if (mode.accelfactor < 0)
	    return EINVAL;

	/* don't allow anybody to poll the keyboard controller */
	error = block_mouse_data(sc, &command_byte);
	if (error)
            return error;

        /* set mouse parameters */
	if (mode.rate > 0)
	    mode.rate = set_mouse_sampling_rate(sc->kbdc, mode.rate);
	if (mode.resolution >= 0)
	    mode.resolution = set_mouse_resolution(sc->kbdc, mode.resolution);
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
	if ((*(int *)addr < PSM_LEVEL_MIN) || (*(int *)addr > PSM_LEVEL_MAX))
	    return EINVAL;
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
	return ENODEV;
#endif /* MOUSE_GETVARS */

    case MOUSE_READSTATE:
    case MOUSE_READDATA:
	data = (mousedata_t *)addr;
	if (data->len > sizeof(data->buf)/sizeof(data->buf[0]))
	    return EINVAL;

	error = block_mouse_data(sc, &command_byte);
	if (error)
            return error;
        if ((data->len = get_mouse_status(sc->kbdc, data->buf, 
		(cmd == MOUSE_READDATA) ? 1 : 0, data->len)) <= 0)
            error = EIO;
	unblock_mouse_data(sc, command_byte);
	break;

#if (defined(MOUSE_SETRESOLUTION))
    case MOUSE_SETRESOLUTION:
	mode.resolution = *(int *)addr;
	if (mode.resolution >= UCHAR_MAX)
	    return EINVAL;
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
            return error;
        sc->mode.resolution = set_mouse_resolution(sc->kbdc, mode.resolution);
	if (sc->mode.resolution != mode.resolution)
	    error = EIO;
	unblock_mouse_data(sc, command_byte);
        break;
#endif /* MOUSE_SETRESOLUTION */

#if (defined(MOUSE_SETRATE))
    case MOUSE_SETRATE:
	mode.rate = *(int *)addr;
	if (mode.rate > UCHAR_MAX)
	    return EINVAL;
        if (mode.rate == 0)
            mode.rate = sc->dflt_mode.rate;
	else if (mode.rate < 0)
	    mode.rate = sc->mode.rate;

	error = block_mouse_data(sc, &command_byte);
	if (error)
            return error;
        sc->mode.rate = set_mouse_sampling_rate(sc->kbdc, mode.rate);
	if (sc->mode.rate != mode.rate)
	    error = EIO;
	unblock_mouse_data(sc, command_byte);
        break;
#endif /* MOUSE_SETRATE */

#if (defined(MOUSE_SETSCALING))
    case MOUSE_SETSCALING:
	if ((*(int *)addr <= 0) || (*(int *)addr > 2))
	    return EINVAL;

	error = block_mouse_data(sc, &command_byte);
	if (error)
            return error;
        if (!set_mouse_scaling(sc->kbdc, *(int *)addr))
	    error = EIO;
	unblock_mouse_data(sc, command_byte);
        break;
#endif /* MOUSE_SETSCALING */

#if (defined(MOUSE_GETHWID))
    case MOUSE_GETHWID:
	error = block_mouse_data(sc, &command_byte);
	if (error)
            return error;
        sc->hw.hwid = get_aux_id(sc->kbdc);
	*(int *)addr = sc->hw.hwid;
	unblock_mouse_data(sc, command_byte);
        break;
#endif /* MOUSE_GETHWID */

    default:
	return ENOTTY;
    }

    return error;
}

void
psmintr(int unit)
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
    register struct psm_softc *sc = psm_softc[unit];
    mousestatus_t ms;
    int x, y, z;
    int c;
    int l;

    /* read until there is nothing to read */
    while((c = read_aux_data_no_wait(sc->kbdc)) != -1) {
    
        /* discard the byte if the device is not open */
        if ((sc->state & PSM_OPEN) == 0)
            continue;
    
        /* 
	 * Check sync bits. We check for overflow bits and the bit 3
	 * for most mice. True, the code doesn't work if overflow 
	 * condition occurs. But we expect it rarely happens...
	 */
	if ((sc->inputbytes == 0) 
		&& ((c & sc->mode.syncmask[0]) != sc->mode.syncmask[1])) {
            log(LOG_DEBUG, "psmintr: out of sync (%04x != %04x).\n", 
		c & sc->mode.syncmask[0], sc->mode.syncmask[1]);
            continue;
	}

        sc->ipacket[sc->inputbytes++] = c;
        if (sc->inputbytes < sc->mode.packetsize) 
	    continue;

#if 0
        log(LOG_DEBUG, "psmintr: %02x %02x %02x %02x %02x %02x\n",
	    sc->ipacket[0], sc->ipacket[1], sc->ipacket[2],
	    sc->ipacket[3], sc->ipacket[4], sc->ipacket[5]);
#endif

	c = sc->ipacket[0];

        /* 
	 * A kludge for Kensington device! 
	 * The MSB of the horizontal count appears to be stored in 
	 * a strange place. This kludge doesn't affect other mice 
	 * because the bit is the overflow bit which is, in most cases, 
	 * expected to be zero when we reach here. XXX 
	 */
        sc->ipacket[1] |= (c & MOUSE_PS2_XOVERFLOW) ? 0x80 : 0;

        /* ignore the overflow bits... */
        x = (c & MOUSE_PS2_XNEG) ?  sc->ipacket[1] - 256 : sc->ipacket[1];
        y = (c & MOUSE_PS2_YNEG) ?  sc->ipacket[2] - 256 : sc->ipacket[2];
	z = 0;
        ms.obutton = sc->button;		  /* previous button state */
        ms.button = butmap[c & MOUSE_PS2_BUTTONS];

	switch (sc->hw.model) {

	case MOUSE_MODEL_INTELLI:
	case MOUSE_MODEL_NET:
	    /* wheel data is in the fourth byte */
	    z = (char)sc->ipacket[3];
	    break;

	case MOUSE_MODEL_MOUSEMANPLUS:
	    if ((c & ~MOUSE_PS2_BUTTONS) == 0xc8) {
		/* the extended data packet encodes button and wheel events */
		x = y = 0;
		z = (sc->ipacket[1] & MOUSE_PS2PLUS_ZNEG)
		    ? (sc->ipacket[2] & 0x0f) - 16 : (sc->ipacket[2] & 0x0f);
		ms.button |= (sc->ipacket[2] & MOUSE_PS2PLUS_BUTTON4DOWN)
		    ? MOUSE_BUTTON4DOWN : 0;
	    } else {
		/* preserve button states */
		ms.button |= ms.obutton & MOUSE_EXTBUTTONS;
	    }
	    break;

	case MOUSE_MODEL_GLIDEPOINT:
	    /* `tapping' action */
	    ms.button |= ((c & MOUSE_PS2_TAP)) ? 0 : MOUSE_BUTTON4DOWN;
	    break;

	case MOUSE_MODEL_NETSCROLL:
	    /* three addtional bytes encode button and wheel events */
	    ms.button |= (sc->ipacket[3] & MOUSE_PS2_BUTTON3DOWN) 
		? MOUSE_BUTTON4DOWN : 0;
	    z = (sc->ipacket[3] & MOUSE_PS2_XNEG) 
		? sc->ipacket[4] - 256 : sc->ipacket[4];
	    break;

	case MOUSE_MODEL_THINK:
	    /* the fourth button state in the first byte */
	    ms.button |= (c & MOUSE_PS2_TAP) ? MOUSE_BUTTON4DOWN : 0;
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
        ms.flags = ((x || y || z) ? MOUSE_POSCHANGED : 0) 
	    | (ms.obutton ^ ms.button);

	if (sc->mode.level < PSM_LEVEL_NATIVE)
	    sc->inputbytes = tame_mouse(sc, &ms, sc->ipacket);

        sc->status.flags |= ms.flags;
        sc->status.dx += ms.dx;
        sc->status.dy += ms.dy;
        sc->status.dz += ms.dz;
        sc->status.button = ms.button;
        sc->button = ms.button;

        /* queue data */
        if (sc->queue.count + sc->inputbytes < sizeof(sc->queue.buf)) {
	    l = min(sc->inputbytes, sizeof(sc->queue.buf) - sc->queue.tail);
	    bcopy(&sc->ipacket[0], &sc->queue.buf[sc->queue.tail], l);
	    if (sc->inputbytes > l)
	        bcopy(&sc->ipacket[l], &sc->queue.buf[0], sc->inputbytes - l);
            sc->queue.tail = 
		(sc->queue.tail + sc->inputbytes) % sizeof(sc->queue.buf);
            sc->queue.count += sc->inputbytes;
	}
        sc->inputbytes = 0;

        if (sc->state & PSM_ASLP) {
            sc->state &= ~PSM_ASLP;
            wakeup((caddr_t) sc);
    	}
        selwakeup(&sc->rsel);
    }
}

static int
psmpoll(dev_t dev, int events, struct proc *p)
{
    struct psm_softc *sc = psm_softc[PSM_UNIT(dev)];
    int s;
    int revents = 0;

    /* Return true if a mouse event available */
    s = spltty();
    if (events & (POLLIN | POLLRDNORM))
	if (sc->queue.count > 0)
	    revents |= events & (POLLIN | POLLRDNORM);
	else
	    selrecord(p, &sc->rsel);

    splx(s);

    return (revents);
}

/* vender/model specific routines */

static int mouse_id_proc1(KBDC kbdc, int res, int scale, int *status)
{
    if (set_mouse_resolution(kbdc, res) != res)
        return FALSE;
    if (set_mouse_scaling(kbdc, scale)
	&& set_mouse_scaling(kbdc, scale)
	&& set_mouse_scaling(kbdc, scale) 
	&& (get_mouse_status(kbdc, status, 0, 3) >= 3)) 
	return TRUE;
    return FALSE;
}

#if notyet
/* Logitech MouseMan Cordless II */
static int
enable_lcordless(struct psm_softc *sc)
{
    int status[3];
    int ch;

    if (!mouse_id_proc1(sc->kbdc, PSMD_RES_HIGH, 2, status))
        return FALSE;
    if (status[1] == PSMD_RES_HIGH)
	return FALSE;
    ch = (status[0] & 0x07) - 1;	/* channel # */
    if ((ch <= 0) || (ch > 4))
	return FALSE;
    /* 
     * status[1]: always one?
     * status[2]: battery status? (0-100)
     */
    return TRUE;
}
#endif /* notyet */

/* Genius NetScroll Mouse */
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
        return FALSE;
    if ((status[1] != '3') || (status[2] != 'D'))
        return FALSE;
    /* FIXME!! */
    sc->hw.buttons = get_mouse_buttons(sc->kbdc);
    sc->hw.buttons = 4;
    return TRUE;
}

/* Genius NetMouse/NetMouse Pro */
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
        return FALSE;
    if ((status[1] != '3') || (status[2] != 'U'))
        return FALSE;
    return TRUE;
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
    if (!mouse_id_proc1(sc->kbdc, PSMD_RES_LOW, 2, status))
        return FALSE;
    if ((status[0] & 0x10) || (status[1] == PSMD_RES_LOW)) 
        return FALSE;
    return TRUE;
}

/* Kensington ThinkingMouse/Trackball */
static int
enable_kmouse(struct psm_softc *sc)
{
    static unsigned char rate[] = { 20, 60, 40, 20, 20, 60, 40, 20, 20 };
    KBDC kbdc = sc->kbdc;
    int status[3];
    int id1;
    int id2;
    int i;

    id1 = get_aux_id(kbdc);
    if (set_mouse_sampling_rate(kbdc, 10) != 10)
	return FALSE;
    /* 
     * The device is now in the native mode? It returns a different
     * ID value...
     */
    id2 = get_aux_id(kbdc);
    if ((id1 == id2) || (id2 != 2))
	return FALSE;

    if (set_mouse_resolution(kbdc, PSMD_RES_LOW) != PSMD_RES_LOW)
        return FALSE;
#if PSM_DEBUG >= 2
    /* at this point, resolution is LOW, sampling rate is 10/sec */
    if (get_mouse_status(kbdc, status, 0, 3) < 3)
        return FALSE;
#endif

    /*
     * The special sequence to enable the third and fourth buttons.
     * Otherwise they behave like the first and second buttons.
     */
    for (i = 0; i < sizeof(rate)/sizeof(rate[0]); ++i) {
        if (set_mouse_sampling_rate(kbdc, rate[i]) != rate[i])
	    return FALSE;
    }

    /* 
     * At this point, the device is using default resolution and
     * sampling rate for the native mode. 
     */
    if (get_mouse_status(kbdc, status, 0, 3) < 3)
        return FALSE;
    if ((status[1] == PSMD_RES_LOW) || (status[2] == rate[i - 1]))
        return FALSE;

    /* the device appears be enabled by this sequence, diable it for now */
    disable_aux_dev(kbdc);
    empty_aux_buffer(kbdc, 5);

    return TRUE;
}

/* Logitech MouseMan+/FirstMouse+ */
static int
enable_mmanplus(struct psm_softc *sc)
{
    static char res[] = {
	-1, PSMD_RES_LOW, PSMD_RES_HIGH, PSMD_RES_MEDIUM_HIGH,
	PSMD_RES_MEDIUM_LOW, -1, PSMD_RES_HIGH, PSMD_RES_MEDIUM_LOW,
	PSMD_RES_MEDIUM_HIGH, PSMD_RES_HIGH, 
    };
    KBDC kbdc = sc->kbdc;
    int data[3];
    int i;

    /* the special sequence to enable the fourth button and the roller. */
    for (i = 0; i < sizeof(res)/sizeof(res[0]); ++i) {
	if (res[i] < 0) {
	    if (!set_mouse_scaling(kbdc, 1))
		return FALSE;
	} else {
	    if (set_mouse_resolution(kbdc, res[i]) != res[i])
		return FALSE;
	}
    }

    if (get_mouse_status(kbdc, data, 1, 3) < 3)
        return FALSE;

    /* 
     * MouseMan+ and FirstMouse+ return following data.
     *
     * byte 1 0xc8
     * byte 2 ?? (MouseMan+:0xc2, FirstMouse+:0xc6)
     * byte 3 model ID? MouseMan+:0x50, FirstMouse+:0x51
     */
    if ((data[0] & ~MOUSE_PS2_BUTTONS) != 0xc8)
        return FALSE;

    /*
     * MouseMan+ (or FirstMouse+) is now in its native mode, in which
     * the wheel and the fourth button events are encoded in the
     * special data packet. The mouse may be put in the IntelliMouse mode
     * if it is initialized by the IntelliMouse's method.
     */
    return TRUE;
}

/* MS IntelliMouse */
static int
enable_msintelli(struct psm_softc *sc)
{
    /*
     * Logitech MouseMan+ and FirstMouse+ will also respond to this
     * probe routine and act like IntelliMouse.
     */

    static unsigned char rate[] = { 200, 100, 80, };
    KBDC kbdc = sc->kbdc;
    int id;
    int i;

    /* the special sequence to enable the third button and the roller. */
    for (i = 0; i < sizeof(rate)/sizeof(rate[0]); ++i) {
        if (set_mouse_sampling_rate(kbdc, rate[i]) != rate[i])
	    return FALSE;
    }
    /* the device will give the genuine ID only after the above sequence */
    id = get_aux_id(kbdc);
    if (id != PSM_INTELLI_ID)
	return FALSE;

    sc->hw.hwid = id;
    sc->hw.buttons = 3;

    return TRUE;
}

static int psm_devsw_installed = FALSE;

static void
psm_drvinit(void *unused)
{
    dev_t dev;

    if (!psm_devsw_installed) {
        dev = makedev(CDEV_MAJOR, 0);
        cdevsw_add(&dev, &psm_cdevsw, NULL);
        psm_devsw_installed = TRUE;
    }
}

#ifdef PSM_HOOKAPM
static int
psmresume(void *dummy)
{
    struct psm_softc *sc = psm_softc[(int)dummy];
    int unit = (int)dummy;
    int err = 0;
    int s;
    int c;

    if (verbose >= 2)
        log(LOG_NOTICE, "psm%d: APM resume hook called.\n", unit);

    /* don't let anybody mess with the aux device */
    if (!kbdc_lock(sc->kbdc, TRUE))
	return (EIO);
    s = spltty();

    /* save the current controller command byte */
    empty_both_buffers(sc->kbdc, 10);
    c = get_controller_command_byte(sc->kbdc);
    if (verbose >= 2)
        log(LOG_DEBUG, "psm%d: current command byte: %04x (psmresume).\n", 
	    unit, c);

    /* enable the aux port but disable the aux interrupt and the keyboard */
    if ((c == -1) || !set_controller_command_byte(sc->kbdc,
	    kbdc_get_device_mask(sc->kbdc),
  	    KBD_DISABLE_KBD_PORT | KBD_DISABLE_KBD_INT
	        | KBD_ENABLE_AUX_PORT | KBD_DISABLE_AUX_INT)) {
        /* CONTROLLER ERROR */
	splx(s);
        kbdc_lock(sc->kbdc, FALSE);
	log(LOG_ERR, "psm%d: unable to set the command byte (psmresume).\n",
	    unit);
	return (EIO);
    }

    /* flush any data */
    if (sc->state & PSM_VALID) {
	disable_aux_dev(sc->kbdc);	/* this may fail; but never mind... */
	empty_aux_buffer(sc->kbdc, 10);
    }
    sc->inputbytes = 0;

#ifdef PSM_RESETAFTERSUSPEND
    /* try to detect the aux device; are you still there? */
    if (reinitialize(unit, &sc->mode)) {
	/* yes */
	sc->state |= PSM_VALID;
    } else {
	/* the device has gone! */
        restore_controller(sc->kbdc, c);
	sc->state &= ~PSM_VALID;
	log(LOG_ERR, "psm%d: the aux device has gone! (psmresume).\n",
	    unit);
	err = ENXIO;
    }
#endif /* PSM_RESETAFTERSUSPEND */
    splx(s);

    /* restore the driver state */
    if ((sc->state & PSM_OPEN) && (err == 0)) {
        /* enable the aux device and the port again */
	err = doopen(unit, c);
	if (err != 0) 
	    log(LOG_ERR, "psm%d: failed to enable the device (psmresume).\n",
		unit);
    } else {
        /* restore the keyboard port and disable the aux port */
        if (!set_controller_command_byte(sc->kbdc, 
                kbdc_get_device_mask(sc->kbdc),
                (c & KBD_KBD_CONTROL_BITS)
                    | KBD_DISABLE_AUX_PORT | KBD_DISABLE_AUX_INT)) {
            /* CONTROLLER ERROR */
            log(LOG_ERR, "psm%d: failed to disable the aux port (psmresume).\n",
                unit);
            err = EIO;
	}
    }

    /* done */
    kbdc_lock(sc->kbdc, FALSE);
    if ((sc->state & PSM_ASLP) && !(sc->state & PSM_VALID)) {
	/* 
	 * Release the blocked process; it must be notified that the device
	 * cannot be accessed anymore.
	 */
        sc->state &= ~PSM_ASLP;
        wakeup((caddr_t)sc);
    }

    if (verbose >= 2)
        log(LOG_DEBUG, "psm%d: APM resume hook exiting.\n", unit);

    return (err);
}
#endif /* PSM_HOOKAPM */

SYSINIT(psmdev, SI_SUB_DRIVERS, SI_ORDER_MIDDLE + CDEV_MAJOR, psm_drvinit, NULL)

#endif /* NPSM > 0 */
