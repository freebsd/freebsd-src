/*-
 * Copyright (c) 1992, 1993 Erik Forsberg.
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
 * $Id: psm.c,v 1.25.2.4 1997/01/05 06:34:11 nate Exp $
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
 *  - 3 October 1996.
 *  - 14 October 1996.
 *  - 22 October 1996.
 *  - 12 November 1996. IOCTLs and rearranging `psmread', `psmioctl'...
 *  - 14 November 1996. Uses `kbdio.c'.
 *  - 30 November 1996. More fixes.
 */

#include "psm.h"

#if NPSM > 0

#include <limits.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/file.h>
#include <sys/proc.h>
#include <sys/conf.h>
#include <sys/syslog.h>
#include <sys/malloc.h>
#ifdef DEVFS
#include <sys/devfsext.h>
#endif

#include <i386/include/mouse.h>
#include <i386/include/clock.h>

#include <i386/isa/isa.h>
#include <i386/isa/isa_device.h>
#include <i386/isa/kbdio.h>

/*
 * driver specific options: the following options may be set by
 * `options' statements in the kernel configuration file.
 */

/* debugging */
#ifndef PSM_DEBUG
#define PSM_DEBUG	0	/* logging: 0: none, 1: brief, 2: verbose */
#endif

/* #define PSM_CHECKSYNC	   if defined, check the header data byte */

/* features */
#ifndef PSM_ACCEL
#define PSM_ACCEL	0	/* must be one or greater; acceleration will be
				 * disabled if zero */
#endif

/* #define PSM_EMULATION	   enables protocol emulation */

/* end of driver specific options */

/* default values */
#define PSMD_DEFAULT_RESOLUTION	200	/* resolution: 200 ppi */
#define PSMD_DEFAULT_RATE	100	/* report rate: 100 Hz */

/* some macros */
#define PSM_UNIT(dev)		(minor(dev) >> 1)
#define PSM_NBLOCKIO(dev)	(minor(dev) & 1)
#define PSM_MKMINOR(unit,block)	(((unit) << 1) | ((block) ? 0:1))

#ifndef max
#define max(x,y)	((x) > (y) ? (x) : (y))
#endif
#ifndef min
#define min(x,y)	((x) < (y) ? (x) : (y))
#endif

/* ring buffer */
#define PSM_BUFSIZE	256

typedef struct ringbuf {
    int count;
    int head;
    int tail;
    mousestatus_t buf[PSM_BUFSIZE];
} ringbuf_t;

/* driver control block */
typedef int (*packetfunc_t) __P((unsigned char *, int *, int, mousestatus_t *));

static struct psm_softc {    /* Driver status information */
    struct selinfo rsel;	/* Process selecting for Input */
    unsigned char state;	/* Mouse driver state */
    int     addr;		/* I/O port address */
    int     command_byte;	/* controller command byte */
    mousehw_t hw;		/* hardware information */
    mousemode_t mode;		/* operation mode */
    ringbuf_t queue;		/* mouse status queue */
    packetfunc_t mkpacket;	/* func. to turn queued data into output format */
    char    ipacket[MOUSE_PS2_PACKETSIZE];	/* interim input buffer */
    unsigned char opacket[PSM_BUFSIZE];		/* output buffer */
    int     inputbytes;		/* # of bytes in the input buffer */
    int     outputbytes;	/* # of bytes in the output buffer */
    int     outputhead;		/* points the head of the output buffer */
    int     button;		/* the latest button state */
#ifdef DEVFS
    void   *devfs_token;
    void   *n_devfs_token;
#endif
} *psm_softc[NPSM];

/* driver state flags (state) */
#define PSM_VALID	0x80
#define PSM_OPEN	1	/* Device is open */
#define PSM_ASLP	2	/* Waiting for mouse data */

/* function prototypes */
static int psmprobe __P((struct isa_device *));
static int psmattach __P((struct isa_device *));
static int mkms __P((unsigned char *, int *, int, mousestatus_t *));
static int mkman __P((unsigned char *, int *, int, mousestatus_t *));
static int mkmsc __P((unsigned char *, int *, int, mousestatus_t *));
static int mkmm __P((unsigned char *, int *, int, mousestatus_t *));
static int mkps2 __P((unsigned char *, int *, int, mousestatus_t *));

static d_open_t psmopen;
static d_close_t psmclose;
static d_read_t psmread;
static d_ioctl_t psmioctl;
static d_select_t psmselect;

/* device driver declarateion */
struct isa_driver psmdriver = { psmprobe, psmattach, "psm", FALSE };
#define CDEV_MAJOR        21

static struct  cdevsw psm_cdevsw = {
	psmopen,	psmclose,	psmread,	nowrite,	/* 21 */
	psmioctl,	nostop,		nullreset,	nodevtotty,
	psmselect,	nommap,		NULL,		"psm",	NULL,	-1
};

/* debug message level */
static int verbose = PSM_DEBUG;

/* device I/O routines */
static int
enable_aux_dev(int port)
{
    int res;

    res = send_aux_command(port, PSMC_ENABLE_DEV);
    if (verbose >= 2)
        log(LOG_DEBUG, "psm: ENABLE_DEV return code:%04x\n", res);

    return (res == PSM_ACK);
}

static int
disable_aux_dev(int port)
{
    int res;

    res = send_aux_command(port, PSMC_DISABLE_DEV);
    if (verbose >= 2)
        log(LOG_DEBUG, "psm: DISABLE_DEV return code:%04x\n", res);

    return (res == PSM_ACK);
}

static int
get_mouse_status(int port, int *status)
{
    int res;

    empty_both_buffers(port, 2);
    res = send_aux_command(port, PSMC_SEND_DEV_STATUS);
    if (verbose >= 2)
        log(LOG_DEBUG, "psm: SEND_AUX_STATUS return code:%04x\n", res);
    if (res != PSM_ACK)
        return FALSE;

    status[0] = read_aux_data(port);
    status[1] = read_aux_data(port);
    status[2] = read_aux_data(port);

    return TRUE;
}

static int
get_aux_id(int port)
{
    int res;
    int id;

    empty_both_buffers(port, 2);
    res = send_aux_command(port, PSMC_SEND_DEV_ID);
    if (verbose >= 2)
        log(LOG_DEBUG, "psm: SEND_DEV_ID return code:%04x\n", res);
    if (res != PSM_ACK)
	return (-1);

        /* 10ms delay */
        DELAY(10000);

    id = read_aux_data(port);
    if (verbose >= 2)
        log(LOG_DEBUG, "psm: device ID: %04x\n", id);

    return id;
}

static int
set_mouse_sampling_rate(int port, int rate)
{
    int res;

    res = send_aux_command_and_data(port, PSMC_SET_SAMPLING_RATE, rate);
    if (verbose >= 2)
        log(LOG_DEBUG, "psm: SET_SAMPLING_RATE (%d) %04x\n", rate, res);

    return ((res == PSM_ACK) ? rate : -1);
}

static int
set_mouse_scaling(int port)
{
    int res;

    res = send_aux_command(port, PSMC_SET_SCALING11);
    if (verbose >= 2)
        log(LOG_DEBUG, "psm: SET_SCALING11 return code:%04x\n", res);

    return (res == PSM_ACK);
}

static struct {
        int resolution;
        int code;
} rescode[] = {
        { 25, PSMD_RESOLUTION_25 },
        { 50, PSMD_RESOLUTION_50 },
        { 100, PSMD_RESOLUTION_100 },
        { 200, PSMD_RESOLUTION_200 },
        { 400, PSMD_RESOLUTION_400 },        /* ?? */
        { 800, PSMD_RESOLUTION_800 },        /* ?? */
        { INT_MAX, PSMD_MAX_RESOLUTION },
    { -1 },
};

static int
set_mouse_resolution(int port, int res)
{
    int ret;
    int i;

    if (res <= 0)
        return (-1);
    for (i = 0; rescode[i].resolution > 0; ++i)
        if (rescode[i].resolution >= res)
            break;

    for (; i >= 0; --i) {
        ret = send_aux_command_and_data(port,
            PSMC_SET_RESOLUTION, rescode[i].code);
        if (verbose >= 2)
            log(LOG_DEBUG, "psm: SET_RESOLUTION (%d) %04x\n",
                rescode[i].code, ret);
        if (ret == PSM_ACK)
            return rescode[i].resolution;
    }

    return (-1);
}

/*
 * NOTE: once `set_mouse_mode()' is called, the mouse device must be
 * re-enabled by calling `enable_aux_dev()'
 */
static int
set_mouse_mode(int port)
{
    int res;

    res = send_aux_command(port, PSMC_SET_STREAM_MODE);
    if (verbose >= 2)
        log(LOG_DEBUG, "psm: SET_STREAM_MODE return code:%04x\n", res);

    return (res == PSM_ACK);
}

static int
get_mouse_buttons(int port)
{
    int c = 2;		/* assume two buttons by default */
    int res;
    int status[3];

    /*
     * NOTE: a special sequence to obtain Logitech-Mouse-specific
     * information: set resolution to 25 ppi, set scaling to 1:1, set
     * scaling to 1:1, set scaling to 1:1. Then the second byte of the
     * mouse status bytes is the number of available buttons.
     */
    if (set_mouse_resolution(port, 25) != 25)
        return c;
    if (set_mouse_scaling(port) && set_mouse_scaling(port)
        && set_mouse_scaling(port) && get_mouse_status(port, status)) {
        if (verbose) {
            log(LOG_DEBUG, "psm: status %02x %02x %02x (get_mouse_buttons)\n",
                status[0], status[1], status[2]);
        }
        if (status[1] == 3)
            return 3;
    }
    return c;
}

/*
 * FIXME:XXX
 * someday, I will get the list of valid pointing devices and
 * their IDs...
 */
static int
is_a_mouse(int id)
{
    static int valid_ids[] = {
        PSM_MOUSE_ID,		/* mouse */
        PSM_BALLPOINT_ID,	/* ballpoint device */
        -1			/* end of table */
    };
#if 0
     int i;

     for(i = 0; valid_ids[i] >= 0; ++i)
         if (valid_ids[i] == id)
             return TRUE;
     return FALSE;
#else
    return TRUE;
#endif
}

static void
recover_from_error(int port)
{
    /* discard anything left in the output buffer */
    empty_both_buffers(port, 10);

#if 0
    /*
     * NOTE: KBDC_RESET_KBD may not restore the communication between the
     * keyboard and the controller.
     */
    reset_kbd(port);
#else
    /*
     * NOTE: somehow diagnostic and keyboard port test commands bring the
     * keyboard back.
     */
    if (!test_controller(port)) 
        log(LOG_ERR, "psm: keyboard controller failed.\n");
    /* if there isn't a keyboard in the system, the following error is OK */
    if (test_kbd_port(port) != 0) {
	if (verbose)
	    log(LOG_ERR, "psm: keyboard port failed.\n");
    }
#endif
}

static int
restore_controller(int port, int command_byte)
{
    if (!set_controller_command_byte(port, command_byte, 0)) {
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
 */
static int
reinitialize(dev_t dev, mousemode_t *mode)
{
    int port = psm_softc[PSM_UNIT(dev)]->addr;
    int stat[3];
    int i;

    switch((i = test_aux_port(port))) {
    case 1:	/* ignore this error */
	if (verbose)
	    log(LOG_DEBUG, "psm%d: strange result for test aux port (%d).\n",
	        PSM_UNIT(dev), i);
	/* fall though */
    case 0:	/* no error */
    	break;
    case -1: 	/* time out */
    default: 	/* error */
    	recover_from_error(port);
    	log(LOG_ERR, "psm%d: the aux port is not functioning (%d).\n",
    	    PSM_UNIT(dev),i);
    	return FALSE;
    }

    /* 
     * NOTE: some controllers appears to hang the `keyboard' when
     * the aux port doesn't exist and `PSMC_RESET_DEV' is issued. 
     */
    if (!reset_aux_dev(port)) {
        recover_from_error(port);
        log(LOG_ERR, "psm%d: failed to reset the aux device.\n",
    	    PSM_UNIT(dev));
        return FALSE;
    }

    /* 
     * both the aux port and the aux device is functioning, see
     * if the device can be enabled. 
     */
    if (!enable_aux_dev(port) || !disable_aux_dev(port)) {
        log(LOG_ERR, "psm%d: failed to enable the aux device.\n",
    	    PSM_UNIT(dev));
        return FALSE;
    }
    empty_both_buffers(port, 10);

    /* set mouse parameters */
    if (mode != (mousemode_t *)NULL) {
	if (mode->rate > 0)
            mode->rate = set_mouse_sampling_rate(port, mode->rate);
	if (mode->resolution > 0)
            mode->resolution = set_mouse_resolution(port, mode->resolution);
        set_mouse_scaling(port);
        set_mouse_mode(port);	
    }

    /* just check the status of the mouse */
    i = get_mouse_status(port, stat);
    if (verbose) {
	if (i)
            log(LOG_DEBUG, "psm%d: status %02x %02x %02x\n",
                PSM_UNIT(dev), stat[0], stat[1], stat[2]);
	else
	    log(LOG_DEBUG, "psm%d: failed to get mouse status\n", 
		PSM_UNIT(dev));
    }

    return TRUE;
}

/* psm driver entry points */

#define endprobe(v)	{   if (bootverbose) \
				--verbose;   \
			    return (v);	     \
			}

static int
psmprobe(struct isa_device *dvp)
{
    int unit = dvp->id_unit;
    int ioport = dvp->id_iobase;
    struct psm_softc *sc;
    int stat[3];
    int setparams;
    int i;

    /* validate unit number */
    if (unit >= NPSM)
        return (0);

    psm_softc[unit] = NULL; 

    sc =  malloc(sizeof *sc, M_DEVBUF, M_NOWAIT);

    bzero(sc, sizeof *sc);

    sc->addr = ioport;
    if (bootverbose)
        ++verbose;

    /*
     * FIXME:XXX
     * the keyboard interrupt should be disabled while probing a
     * mouse?
     */

    /*
     * NOTE: two bits in the command byte controls the operation of the
     * aux port (mouse port): the aux port disable bit (bit 5) and the aux
     * port interrupt (IRQ 12) enable bit (bit 2). When this probe routine
     * is called, there are following possibilities about the presence of
     * the aux port and the PS/2 mouse.
     * 
     * Case 1: aux port disabled (bit 5:1), aux int. disabled (bit 2:0) The
     * aux port most certainly exists. A device may or may not be
     * connected to the port. No driver is probably installed yet.
     * 
     * Case 2: aux port enabled (bit 5:0), aux int. disabled (bit 2:0) Three
     * possibile situations here:
     * 
     * Case 2a: The aux port does not exist, therefore, is not explicitly
     * disabled. Case 2b: The aux port exists. A device and a driver may
     * exist, using the device in the polling(remote) mode. Case 2c: The
     * aux port exists. A device may exist, but someone who knows nothing
     * about the aux port has set the command byte this way (this is the
     * case with `syscons').
     * 
     * Case 3: aux port disabled (bit 5:1), aux int. enabled (bit 2:1) The
     * aux port exists, but someone is controlloing the device and
     * temporalily disabled the port.
     * 
     * Case 4: aux port enabled (bit 5:0), aux int. enabled (bit 2:1) The aux
     * port exists, a device is attached to the port, and someone is
     * controlling the device. Some BIOS set the bits this way after boot.
     * 
     * All in all, it is no use examing the bits for detecting the presence
     * of the port and the mouse device.
     */

    /* save the current command byte; it will be used later */
    empty_both_buffers(ioport, 20);	
    if (!write_controller_command(ioport,KBDC_GET_COMMAND_BYTE)) {
        /* CONTROLLER ERROR */
        printf("psm%d: failed to get the current command byte value.\n",
            unit);
 	free(sc, M_DEVBUF);
        endprobe(0);
    }
    sc->command_byte = read_controller_data(ioport);
    if (verbose) {
        printf("psm%d: current command byte:%04x\n",
            unit, sc->command_byte);
    }
    if (sc->command_byte == -1) {
        /* CONTROLLER ERROR */
        printf("psm%d: unable to get the current command byte value.\n",
            unit);
	free(sc, M_DEVBUF);
        endprobe(0);
    }

    /*
     * disable the keyboard port while probing the aux port, which must be
     * enabled during this routine
     */
    if (!set_controller_command_byte(ioport,
  	    sc->command_byte & ~(KBD_KBD_CONTROL_BITS | KBD_AUX_CONTROL_BITS),
        KBD_DISABLE_KBD_PORT | KBD_DISABLE_KBD_INT
            | KBD_ENABLE_AUX_PORT | KBD_DISABLE_AUX_INT)) {
        /* 
	 * this is CONTROLLER ERROR; I don't know how to recover 
         * from this error... 
	 */
        restore_controller(ioport, sc->command_byte);
        printf("psm%d: unable to set the command byte.\n", unit);
 	free(sc, M_DEVBUF);
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
    switch ((i = test_aux_port(ioport))) {
    case 1:	   /* ignore this error */
        if (verbose)
	    printf("psm%d: strange result for test aux port (%d).\n",
	        unit, i);
	/* fall though */
    case 0:        /* no error */
        break;
    case -1:        /* time out */
    default:        /* error */
        recover_from_error(ioport);
        restore_controller(ioport, sc->command_byte);
        if (verbose)
            printf("psm%d: the aux port is not functioning (%d).\n",
                unit, i);
	free(sc, M_DEVBUF);
        endprobe(0);
    }

    /*
     * NOTE: some controllers appears to hang the `keyboard' when the aux
     * port doesn't exist and `PSMC_RESET_DEV' is issued.
     */
    if (!reset_aux_dev(ioport)) {
        recover_from_error(ioport);
        restore_controller(ioport, sc->command_byte);
        if (verbose)
            printf("psm%d: failed to reset the aux device.\n", unit);
	free(sc, M_DEVBUF);
        endprobe(0);
    }
    /*
     * both the aux port and the aux device is functioning, see if the
     * device can be enabled. NOTE: when enabled, the device will start
     * sending data; we shall immediately disable the device once we know
     * the device can be enabled.
     */
    if (!enable_aux_dev(ioport) || !disable_aux_dev(ioport)) {
	/* MOUSE ERROR */
        restore_controller(ioport, sc->command_byte);
        if (verbose)
            printf("psm%d: failed to enable the aux device.\n", unit);
	free(sc, M_DEVBUF);
        endprobe(0);
    }
    empty_both_buffers(ioport, 10);	/* remove stray data if any */

    /* save the default values after reset */
    if (get_mouse_status(ioport, stat)) {
	if (verbose)
            printf("psm%d: status after reset %02x %02x %02x\n",
                unit, stat[0], stat[1], stat[2]);
	sc->mode.rate = stat[2];
	sc->mode.resolution = PSMD_DEFAULT_RESOLUTION;
	for (i = 0; rescode[i].resolution > 0; ++i)
	    if (rescode[i].code == stat[1])
		sc->mode.resolution = rescode[i].resolution;
	setparams = TRUE;
    } else {
	sc->mode.rate = -1;
	sc->mode.resolution = -1;
	setparams = FALSE;
    }

    /* hardware information */
    sc->hw.iftype = MOUSE_IF_PS2;

    /* verify the device is a mouse */
    sc->hw.hwid = get_aux_id(ioport);
    if (!is_a_mouse(sc->hw.hwid)) {
        restore_controller(ioport, sc->command_byte);
        if (verbose)
            printf("psm%d: unknown device type (%d).\n", unit, sc->hw.hwid);
	free(sc, M_DEVBUF);
        endprobe(0);
    }
    switch (sc->hw.hwid) {
    case PSM_BALLPOINT_ID:
        sc->hw.type = MOUSE_TRACKBALL;
        break;
    case PSM_MOUSE_ID:
        sc->hw.type = MOUSE_MOUSE;
        break;
    default:
        sc->hw.type = MOUSE_UNKNOWN;
        break;
    }

    /* # of buttons */
    sc->hw.buttons = (setparams) ? get_mouse_buttons(ioport) : 2;

    /* set mouse parameters */
    /* FIXME:XXX should we set them in `psmattach()' rather than here? */
#if 0
    if (setparams) {
        if (sc->mode.rate > 0)
            sc->mode.rate = set_mouse_sampling_rate(ioport, sc->mode.rate);
        if (sc->mode.resolution > 0)
    sc->mode.resolution =
                set_mouse_resolution(ioport, sc->mode.resolution);
    }
    /* FIXME:XXX I don't know if these parameters are reasonable */
    set_mouse_scaling(ioport);    /* 1:1 scaling */
    set_mouse_mode(ioport);    /* stream mode */
#else
    i = send_aux_command(ioport, PSMC_SET_DEFAULTS);
    if (verbose >= 2)
	log(LOG_DEBUG, "psm%d: SET_DEFAULTS return code:%04x\n", unit, i);
#endif

    /* just check the status of the mouse */
    /* 
     * XXX there are some arcane controller/mouse combinations out there,
     * which hung the controller unless there is data transmission 
     * after ACK from the mouse.
     */
    i = get_mouse_status(ioport, stat);
    if (verbose) {
	if (i)
            log(LOG_DEBUG, "psm%d: status %02x %02x %02x\n",
                unit, stat[0], stat[1], stat[2]);
	else
	    log(LOG_DEBUG, "psm%d: failed to get mouse status\n", unit);
    }

    /* disable the aux port for now... */

    /*
     * WARNING: we save the controller command byte and use it later
     * during `psmopen()' and `psmclose()'. This will be OK, so long as
     * the keyboard/console device driver won't change the command byte in
     * the course of its operation (this is the case with `syscons'). If
     * not,...
     */
    if (!set_controller_command_byte(ioport, 
	    sc->command_byte & ~KBD_AUX_CONTROL_BITS,
            KBD_DISABLE_AUX_PORT | KBD_DISABLE_AUX_INT)) {
        /* 
	 * this is CONTROLLER ERROR; I don't know the proper way to 
         * recover from this error... 
	 */
        restore_controller(ioport, sc->command_byte);
        printf("psm%d: unable to set the command byte.\n", unit);
 	free(sc, M_DEVBUF);
        endprobe(0);
    }
    sc->command_byte &= ~KBD_AUX_CONTROL_BITS;

    /* done */
    psm_softc[unit] = sc;
    return (IO_PSMSIZE);
}

static int
psmattach(struct isa_device *dvp)
{
    int unit = dvp->id_unit;
    struct psm_softc *sc = psm_softc[unit];

    if (sc == NULL)    /* shouldn't happen */ 
	return (0); 

    /* initial operation mode */
    sc->mode.accelfactor = PSM_ACCEL;
    sc->mode.protocol = MOUSE_PROTO_PS2;
    sc->mkpacket = mkps2;

    /* Setup initial state */
    sc->state = PSM_VALID;

    /* Done */
#ifdef    DEVFS
    sc->devfs_token =
        devfs_add_devswf(&psm_cdevsw, PSM_MKMINOR(unit, TRUE),
        DV_CHR, 0, 0, 0666, "psm%d", unit);
    sc->n_devfs_token =
        devfs_add_devswf(&psm_cdevsw, PSM_MKMINOR(unit, FALSE),
        DV_CHR, 0, 0, 0666, "npsm%d", unit);
#endif

    if (verbose)
        printf("psm%d: device ID %d, %d buttons?\n",
            unit, sc->hw.hwid, sc->hw.buttons);
    else
        printf("psm%d: device ID %d\n", unit, sc->hw.hwid);

    if (bootverbose)
        --verbose;

    return (1);
}

static int
psmopen(dev_t dev, int flag, int fmt, struct proc *p)
{
    int unit = PSM_UNIT(dev);
    int ioport;
    struct psm_softc *sc;
    int stat[3];
    int ret;

    /* Validate unit number */
    if (unit >= NPSM)
        return (ENXIO);

    /* Get device data */
    sc = psm_softc[unit];
    if ((sc == NULL) || (sc->state & PSM_VALID) == 0)
	/* the device is no longer valid/functioning */
        return (ENXIO);
    ioport = sc->addr;

    /* Disallow multiple opens */
    if (sc->state & PSM_OPEN)
        return (EBUSY);

    /* Initialize state */
    sc->rsel.si_flags = 0;
    sc->rsel.si_pid = 0;

    /* flush the event queue */
    sc->queue.count = 0;
    sc->queue.head = 0;
    sc->queue.tail = 0;
    sc->button = 0;

    /* empty input/output buffers */
    sc->inputbytes = 0;
    sc->outputbytes = 0;
    sc->outputhead = 0;

    /* enable the aux port and temporalily disable the keyboard */
    if (!set_controller_command_byte(ioport,
        sc->command_byte & ~KBD_KBD_CONTROL_BITS,
        KBD_DISABLE_KBD_PORT | KBD_DISABLE_KBD_INT
	    | KBD_ENABLE_AUX_PORT | KBD_DISABLE_AUX_INT)) {
        /* CONTROLLER ERROR; do you know how to get out of this? */
        set_controller_command_byte(ioport, 
	    sc->command_byte, KBD_DISABLE_AUX_PORT | KBD_DISABLE_AUX_INT);
	log(LOG_ERR, "psm%d: unable to set the command byte (psmopen).\n",
	    unit);
	return (EIO);
    }

    /* enable the mouse device */
    if (!enable_aux_dev(ioport)) {
	/* MOUSE ERROR: failed to enable the mouse because:
	 * 1) the mouse is faulty,
	 * 2) the mouse has been removed(!?)
	 * In the latter case, the keyboard may have hung, and need 
	 * recovery procedure...
	 */
	recover_from_error(ioport);
#if 0
	/* FIXME: we could reset the mouse here and try to enable
	 * it again. But it will take long time and it's not a good
	 * idea to disable the keyboard that long...
	 */
	if (!reinitialize(dev, &sc->mode) || !enable_aux_dev(ioport)) 
	    recover_from_error(ioport);
#endif
        set_controller_command_byte(ioport, 
	    sc->command_byte, KBD_DISABLE_AUX_PORT | KBD_DISABLE_AUX_INT);
	/* mark this device is no longer available */
	sc->state &= ~PSM_VALID;	
	log(LOG_ERR, "psm%d: failed to enable the device (psmopen).\n",
	    unit);
        return (EIO);
    }

    ret = get_mouse_status(ioport, stat);
    if (verbose >= 2) {
	if (ret)
            log(LOG_DEBUG, "psm%d: status %02x %02x %02x (psmopen)\n",
                unit, stat[0], stat[1], stat[2]);
	else
	    log(LOG_DEBUG, "psm%d: failed to get mouse status (psmopen).\n",
		unit);
    }

    /* enable the aux port and interrupt */
    if (!set_controller_command_byte(ioport, sc->command_byte,
	KBD_ENABLE_AUX_PORT | KBD_ENABLE_AUX_INT)) {
	/* CONTROLLER ERROR */
	disable_aux_dev(ioport);
    set_controller_command_byte(ioport, sc->command_byte,
	    KBD_DISABLE_AUX_PORT | KBD_DISABLE_AUX_INT);
	log(LOG_ERR, "psm%d: failed to enable the aux interrupt (psmopen).\n",
	    unit);
	return (EIO);
    }

    /* done */
    sc->state |= PSM_OPEN;
    return (0);
}

static int
psmclose(dev_t dev, int flag, int fmt, struct proc *p)
{
    struct psm_softc *sc = psm_softc[PSM_UNIT(dev)];
    int ioport = sc->addr;
    int stat[3];
    int ret;

    /* disable the aux interrupt */
    if (!set_controller_command_byte(ioport, sc->command_byte,
	KBD_ENABLE_AUX_PORT | KBD_DISABLE_AUX_INT)) {
	log(LOG_ERR, "psm%d: failed to disable the aux int (psmclose).\n",
	    PSM_UNIT(dev));
	/* CONTROLLER ERROR;
	 * NOTE: we shall force our way through. Because the only
	 * ill effect we shall see is that we may not be able
	 * to read ACK from the mouse, and it doesn't matter much 
	 * so long as the mouse will accept the DISABLE command.
	 */
    }

    /* remove anything left in the output buffer */
    empty_aux_buffer(ioport, 20);

    /* disable the aux device, port and interrupt */
    if (!disable_aux_dev(ioport)) {
	/* MOUSE ERROR; 
	 * NOTE: we don't return error and continue, pretending 
	 * we have successfully disabled the device. It's OK because 
	 * the interrupt routine will discard any data from the mouse
	 * hereafter. 
	 */
	log(LOG_ERR, "psm%d: failed to disable the device (psmclose).\n",
	    PSM_UNIT(dev));
    }

    ret = get_mouse_status(ioport, stat);
    if (verbose >= 2) {
	if (ret)
            log(LOG_DEBUG, "psm%d: status %02x %02x %02x (psmclose)\n",
                PSM_UNIT(dev), stat[0], stat[1], stat[2]);
	else
	    log(LOG_DEBUG, "psm%d: failed to get mouse status (psmclose).\n",
		PSM_UNIT(dev));
    }

    if (!set_controller_command_byte(ioport, sc->command_byte,
	KBD_DISABLE_AUX_PORT | KBD_DISABLE_AUX_INT)) {
	/* CONTROLLER ERROR; 
	 * we shall ignore this error; see the above comment.
	 */
	log(LOG_ERR, "psm%d: failed to disable the aux port (psmclose).\n",
	    PSM_UNIT(dev));
    }

    /* remove anything left in the output buffer */
    empty_aux_buffer(ioport, 10);

    /* close is almost always successful */
    sc->state &= ~PSM_OPEN;
    return (0);
}

#ifdef PSM_EMULATION
static int
mkms(unsigned char *buf, int *len, int maxlen, register mousestatus_t *status)
{
    static int butmap[] = {
        0, MOUSE_MSS_BUTTON1DOWN, 
        0, MOUSE_MSS_BUTTON1DOWN ,
        MOUSE_MSS_BUTTON3DOWN, MOUSE_MSS_BUTTON1DOWN | MOUSE_MSS_BUTTON3DOWN,
        MOUSE_MSS_BUTTON3DOWN, MOUSE_MSS_BUTTON1DOWN | MOUSE_MSS_BUTTON3DOWN,
    };
    unsigned char delta;

    if (maxlen - *len < MOUSE_MSS_PACKETSIZE)
        return FALSE;

    buf[0] = MOUSE_MSS_SYNC | butmap[status->button & MOUSE_STDBUTTONS];

    if (status->dx < -128)
        delta = 0x80;    /* -128 */
    else
        if (status->dx > 127)
            delta = 0x7f;    /* 127 */
        else
            delta = (unsigned char) status->dx;
    buf[0] |= (delta & 0xc0) >> 6;    /* bit 6-7 */
    buf[1] = delta & 0x3f;    /* bit 0-5 */

    if (status->dy < -128)
        delta = 0x80;    /* -128 */
    else
        if (status->dy > 127)
            delta = 0x7f;    /* 127 */
        else
            delta = (unsigned char) status->dy;
    buf[0] |= (delta & 0xc0) >> 4;    /* bit 6-7 */
    buf[2] = delta & 0x3f;    /* bit 0-5 */

    *len += MOUSE_MSS_PACKETSIZE;

    return TRUE;
}

static int
mkman(unsigned char *buf, int *len, int maxlen, register mousestatus_t *status)
{
    static int butmap[] = {
        0, MOUSE_MSS_BUTTON1DOWN, 
	0, MOUSE_MSS_BUTTON1DOWN,
        MOUSE_MSS_BUTTON3DOWN, MOUSE_MSS_BUTTON1DOWN | MOUSE_MSS_BUTTON3DOWN,
        MOUSE_MSS_BUTTON3DOWN, MOUSE_MSS_BUTTON1DOWN | MOUSE_MSS_BUTTON3DOWN,
    };
    unsigned char delta;
    int l;

    l = ((status->button ^ status->obutton) & MOUSE_BUTTON2DOWN) ?
	MOUSE_MSS_PACKETSIZE + 1: MOUSE_MSS_PACKETSIZE;
    if (maxlen - *len < l) 
        return FALSE;

    buf[0] = MOUSE_MSS_SYNC | butmap[status->button & MOUSE_STDBUTTONS];

    if (status->dx < -128)
        delta = 0x80;    /* -128 */
    else
        if (status->dx > 127)
            delta = 0x7f;    /* 127 */
        else
            delta = (unsigned char) status->dx;
    buf[0] |= (delta & 0xc0) >> 6;    /* bit 6-7 */
    buf[1] = delta & 0x3f;    /* bit 0-5 */

    if (status->dy < -128)
        delta = 0x80;    /* -128 */
    else
        if (status->dy > 127)
            delta = 0x7f;    /* 127 */
        else
            delta = (unsigned char) status->dy;
    buf[0] |= (delta & 0xc0) >> 4;    /* bit 6-7 */
    buf[2] = delta & 0x3f;    /* bit 0-5 */

    if (l > MOUSE_MSS_PACKETSIZE)
       buf[3] = (status->button & MOUSE_BUTTON2DOWN) 
	   ? MOUSE_LMAN_BUTTON2DOWN : 0;

    *len += l;

    return TRUE;
}

static int
mkmsc(unsigned char *buf, int *len, int maxlen, register mousestatus_t *status)
{
    static int butmap[] = {
        0,
        MOUSE_MSC_BUTTON1UP, MOUSE_MSC_BUTTON2UP,
        MOUSE_MSC_BUTTON1UP | MOUSE_MSC_BUTTON2UP,
        MOUSE_MSC_BUTTON3UP,
        MOUSE_MSC_BUTTON1UP | MOUSE_MSC_BUTTON3UP,
        MOUSE_MSC_BUTTON2UP | MOUSE_MSC_BUTTON3UP,
        MOUSE_MSC_BUTTON1UP | MOUSE_MSC_BUTTON2UP | MOUSE_MSC_BUTTON3UP,
    };
    register int delta;

    if (maxlen - *len < MOUSE_MSC_PACKETSIZE)
        return FALSE;

    buf[0] = MOUSE_MSC_SYNC
	     | (~butmap[status->button & MOUSE_STDBUTTONS] & MOUSE_MSC_BUTTONS);

    /* data bytes cannot be between 80h (-128) and 87h (-120).
     * values we can return is 255 (0ffh) through -251 (10fh = 87h*2 + 1).
     */
    if (status->dx < -251)
        delta = -251;
    else
        if (status->dx > 255)
            delta = 255;
        else
            delta = status->dx;
    buf[1] = delta/2;
    buf[3] = delta - buf[1];

    if (status->dy < -251)
        delta = -251;
    else
        if (status->dy > 255)
            delta = 255;
        else
            delta = status->dy;
    buf[2] = delta/2;
    buf[4] = delta - buf[2];

    *len += MOUSE_MSC_PACKETSIZE;

    return TRUE;
}

static int
mkmm(unsigned char *buf, int *len, int maxlen, register mousestatus_t *status)
{
    static int butmap[] = {
        0,
        MOUSE_MM_BUTTON1DOWN, MOUSE_MM_BUTTON2DOWN,
        MOUSE_MM_BUTTON1DOWN | MOUSE_MM_BUTTON2DOWN,
        MOUSE_MM_BUTTON3DOWN,
        MOUSE_MM_BUTTON1DOWN | MOUSE_MM_BUTTON3DOWN,
        MOUSE_MM_BUTTON2DOWN | MOUSE_MM_BUTTON3DOWN,
        MOUSE_MM_BUTTON1DOWN | MOUSE_MM_BUTTON2DOWN | MOUSE_MM_BUTTON3DOWN,
    };
    int delta;

    if (maxlen - *len < MOUSE_MM_PACKETSIZE)
        return FALSE;

    buf[0] = MOUSE_MM_SYNC
	     | butmap[status->button & MOUSE_STDBUTTONS]
	     | ((status->dx > 0) ? MOUSE_MM_XPOSITIVE : 0)
	     | ((status->dy > 0) ? MOUSE_MM_YPOSITIVE : 0);

    delta = (status->dx < 0) ? -status->dx : status->dx;
    buf[1] = min(delta, 127);
    delta = (status->dy < 0) ? -status->dy : status->dy;
    buf[2] = min(delta, 127);

    *len += MOUSE_MM_PACKETSIZE;

    return TRUE;
}
#endif /* PSM_EMULATION */

static int
mkps2(unsigned char *buf, int *len, int maxlen, register mousestatus_t *status)
{
    static int butmap[] = {
        0,
        MOUSE_PS2_BUTTON1DOWN, MOUSE_PS2_BUTTON2DOWN,
        MOUSE_PS2_BUTTON1DOWN | MOUSE_PS2_BUTTON2DOWN,
        MOUSE_PS2_BUTTON3DOWN,
        MOUSE_PS2_BUTTON1DOWN | MOUSE_PS2_BUTTON3DOWN,
        MOUSE_PS2_BUTTON2DOWN | MOUSE_PS2_BUTTON3DOWN,
        MOUSE_PS2_BUTTON1DOWN | MOUSE_PS2_BUTTON2DOWN | MOUSE_PS2_BUTTON3DOWN,
    };
    register int delta;

    if (maxlen - *len < MOUSE_PS2_PACKETSIZE)
        return FALSE;

    buf[0] = ((status->button & MOUSE_BUTTON4DOWN) ? 0 : MOUSE_PS2_BUTTON4UP)
        | butmap[status->button & MOUSE_STDBUTTONS];
    if ((verbose >= 2) && ((buf[0] & MOUSE_PS2_BUTTON4UP) == 0))
	log(LOG_DEBUG, "psm: button 4 down (%04x) (mkps2)\n", buf[0]);

    if (status->dx < -128)
        delta = -128;
    else
        if (status->dx > 127)
            delta = 127;
        else
            delta = status->dx;
    if (delta < 0)
        buf[0] |= MOUSE_PS2_XNEG;
    buf[1] = delta;

    if (status->dy < -128)
        delta = -128;
    else
        if (status->dy > 127)
            delta = 127;
        else
            delta = status->dy;
    if (delta < 0)
        buf[0] |= MOUSE_PS2_YNEG;
    buf[2] = delta;

    *len += MOUSE_PS2_PACKETSIZE;

    return TRUE;
}

static int
psmread(dev_t dev, struct uio *uio, int flag)
{
    register struct psm_softc *sc = psm_softc[PSM_UNIT(dev)];
    unsigned int length;
    int error;
    int s;
    int i;

    /* block until mouse activity occured */
    s = spltty();
    if ((sc->outputbytes <= 0) && (sc->queue.count <= 0)) {
        while (sc->queue.count <= 0) {
            if (PSM_NBLOCKIO(dev)) {
                splx(s);
                return (EWOULDBLOCK);
            }
            sc->state |= PSM_ASLP;
            error = tsleep((caddr_t) sc, PZERO | PCATCH, "psmread", 0);
            if (error) {
                splx(s);
                return (error);
            }
        }
    }

    if (sc->outputbytes >= uio->uio_resid) {
        /* nothing to be done */
    } else {
        if (sc->outputbytes > 0) {
            bcopy(&sc->opacket[sc->outputhead], sc->opacket,
                sc->outputbytes);
        }
        sc->outputhead = 0;
        for (i = sc->queue.head; sc->queue.count > 0;
            i = (i + 1) % PSM_BUFSIZE, --sc->queue.count) {
            if (!(*sc->mkpacket) (&sc->opacket[sc->outputbytes],
                &sc->outputbytes, PSM_BUFSIZE, &sc->queue.buf[i]))
                break;
        }
        sc->queue.head = i;
    }

    /* allow interrupts again */
    splx(s);

    /* copy data to user process */
    length = min(sc->outputbytes, uio->uio_resid);
    error = uiomove(&sc->opacket[sc->outputhead], length, uio);
    if (error)
        return (error);
    sc->outputhead += length;
    sc->outputbytes -= length;

    return (error);
}

static int
psmioctl(dev_t dev, int cmd, caddr_t addr, int flag, struct proc *p)
{
    struct psm_softc *sc = psm_softc[PSM_UNIT(dev)];
    mousemode_t mode;
    mousestatus_t status;
    packetfunc_t func;
    int error = 0;
    int s;

    /* Perform IOCTL command */
    switch (cmd) {

    case MOUSE_GETINFO:
        *(mousehw_t *) addr = sc->hw;
        break;

    case MOUSE_GETMODE:
        *(mousemode_t *) addr = sc->mode;
        break;

    case MOUSE_SETMODE:
	mode = *(mousemode_t *) addr;
        if (mode.rate == 0) {
            mode.rate = PSMD_DEFAULT_RATE;
        } else if (mode.rate > 0) {
            mode.rate = min(mode.rate, PSMD_MAX_RATE);
	} else { /* mode.rate < 0 */
	    mode.rate = sc->mode.rate;
	}
        if (mode.resolution == 0) {
            mode.resolution = PSMD_DEFAULT_RESOLUTION;
        } else if (mode.resolution > 0) {
            mode.resolution = mode.resolution;
	} else { /* mode.resolution < 0 */
	    mode.resolution = sc->mode.resolution;
        }
#ifdef PSM_EMULATION
        switch (mode.protocol) {
        case MOUSE_PROTO_MS:
            func = mkms;
            break;
        case MOUSE_PROTO_LOGIMOUSEMAN:
            func = mkman;
            break;
        case MOUSE_PROTO_MSC:
            func = mkmsc;
            break;
        case MOUSE_PROTO_MM:
            func = mkmm;
            break;
        case MOUSE_PROTO_PS2:
            func = mkps2;
            break;
        default:
            error = EINVAL;
            func = (packetfunc_t) NULL;
            break;
        }
        if (error)
            break;
#else
	mode.protocol = sc->mode.protocol;
#endif    /* PSM_EMULATION */
        if (mode.accelfactor < 0) {
            error = EINVAL;
            break;
        }

        s = spltty();    /* disable interrupt while updating */

        /* temporalily disable the keyboard */
	if (!set_controller_command_byte(sc->addr,
            sc->command_byte & ~KBD_KBD_CONTROL_BITS,
            KBD_DISABLE_KBD_PORT | KBD_DISABLE_KBD_INT
		| KBD_ENABLE_AUX_PORT | KBD_DISABLE_AUX_INT)) {
	    /* this is CONTROLLER ERROR; I don't know how to recover 
	     * from this error. The least we can do is to send
	     * ENABLE_KBD_PORT... 
	     */
	    write_controller_command(sc->addr, KBDC_ENABLE_KBD_PORT);
	    splx(s);
	    log(LOG_ERR, "psm%d: failed to set the command byte (psmioctl).\n",
	        PSM_UNIT(dev));
	    error = EIO;
	    break;
	}

        /* program the mouse */
	if (mode.rate > 0)
	    mode.rate = set_mouse_sampling_rate(sc->addr, mode.rate);
	if (mode.resolution > 0)
	    mode.resolution = set_mouse_resolution(sc->addr, mode.resolution);

        /* enable the aux port and interrupt */
	if (!set_controller_command_byte(sc->addr, sc->command_byte,
	    KBD_ENABLE_AUX_PORT | KBD_ENABLE_AUX_INT)) {
	    /* CONTROLLER ERROR; this is serious, we may have
	     * been left with the unaccessible keyboard and
	     * the disabled mouse interrupt. 
	     */
	    splx(s);
	    log(LOG_ERR, "psm%d: failed to re-enable the aux port and int (psmioctl).\n",
		PSM_UNIT(dev));
	    error = EIO;
	    break;
    	}

    	sc->mode = mode;
#ifdef PSM_EMULATION
        sc->mkpacket = func;
        sc->outputbytes = 0;
        sc->outputhead = 0;
#endif    /* PSM_EMULATION */

        splx(s);
        break;

    case MOUSEIOCREAD:    /* FIXME:XXX this should go... */
        error = EINVAL;
        break;

    case MOUSE_GETSTATE:
        s = spltty();
        if (sc->queue.count > 0) {
	    status = sc->queue.buf[sc->queue.head];
            sc->queue.head = (sc->queue.head + 1) % PSM_BUFSIZE;
            --sc->queue.count;
        } else {
	    status.button = status.obutton = sc->button;
	    status.dx = status.dy = 0;
        }
        splx(s);

        *(mousestatus_t *) addr = status;
        break;

    default:
        error = EINVAL;
        break;
    }

    /* Return error code */
    return (error);
}

void
psmintr(int unit)
{
    /*
     * the table to turn PS/2 mouse button bits (MOUSE_PS2_BUTTON?DOWN)
     * into `mousestatus' button bits (BUTTON?DOWN).
     */
    static  butmap[8] = {
        0, 
	MOUSE_BUTTON1DOWN, MOUSE_BUTTON3DOWN, 
	MOUSE_BUTTON1DOWN | MOUSE_BUTTON3DOWN, 
	MOUSE_BUTTON2DOWN, 
	MOUSE_BUTTON1DOWN | MOUSE_BUTTON2DOWN, 
	MOUSE_BUTTON2DOWN | MOUSE_BUTTON3DOWN,
        MOUSE_BUTTON1DOWN | MOUSE_BUTTON2DOWN | MOUSE_BUTTON3DOWN
    };
    register struct psm_softc *sc = psm_softc[unit];
    int ioport = sc->addr;
    mousestatus_t *ms;
    unsigned char c;
    int     x, y;

    /* is this really for us? */
    if ((inb(ioport + KBD_STATUS_PORT) & KBDS_BUFFER_FULL)
        != KBDS_AUX_BUFFER_FULL)
        return;
    DELAY(7);

    /* read a byte */
    c = inb(ioport + KBD_DATA_PORT);

    /* discard the byte if the device is not open */
    if ((sc->state & PSM_OPEN) == 0)
        return;

    /* interpret data bytes */
    /*
     * FIXME: there seems no way to reliably
     * re-synchronize with the PS/2 mouse once we are out of sync. Sure,
     * there is sync bits in the first data byte, but the second and the
     * third bytes may have these bits on (they are not functioning as
     * sync bits then!). There need to be two consequtive bytes with these
     * bits off to re-sync. (This can be done if the user clicks buttons
     * without moving the mouse?)
     */
    if (sc->inputbytes == 0) {
#ifdef PSM_CHECKSYNC
        if ((c & MOUSE_PS2_SYNCMASK) == MOUSE_PS2_SYNC)
            sc->ipacket[sc->inputbytes++] = c;
	else
            log(LOG_DEBUG, "psmintr: sync. bit is off.\n");
#else
        sc->ipacket[sc->inputbytes++] = c;
#endif    /* PSM_CHECKSYNC */
    } else {
        sc->ipacket[sc->inputbytes++] = c;
        if (sc->inputbytes >= MOUSE_PS2_PACKETSIZE) {
            if (sc->queue.count >= PSM_BUFSIZE) {
                /* no room in the queue */
                sc->inputbytes = 0;
                return;
            }
            if (sc->mode.accelfactor >= 1) {
                x = (sc->ipacket[0] & MOUSE_PS2_XOVERFLOW) ?
                    128 : sc->ipacket[1];
                if (x != 0) {
                    x = x * x / sc->mode.accelfactor;
                    if (x == 0)
                        x = 1;
                    if (sc->ipacket[0] & MOUSE_PS2_XNEG)
                        x = -x;
                }
                y = (sc->ipacket[0] & MOUSE_PS2_YOVERFLOW) ?
                    128 : sc->ipacket[2];
                if (y != 0) {
                    y = y * y / sc->mode.accelfactor;
                    if (y == 0)
                        y = 1;
                    if (sc->ipacket[0] & MOUSE_PS2_YNEG)
                        y = -y;
                }
            } else {/* sc->mode.accelfactor <= 0 */
                x = (sc->ipacket[0] & MOUSE_PS2_XOVERFLOW) ?
                    ((sc->ipacket[0] & MOUSE_PS2_XNEG) ?
                    -128 : 127) : sc->ipacket[1];
                y = (sc->ipacket[0] & MOUSE_PS2_YOVERFLOW) ?
                    ((sc->ipacket[0] & MOUSE_PS2_YNEG) ?
                    -128 : 127) : sc->ipacket[2];
            }

            /*
             * FIXME:XXX
             * we shouldn't store data if no movement and
             * no button status change is detected?
             */
            ms = &sc->queue.buf[sc->queue.tail];
            ms->dx = x;
            ms->dy = y;
            ms->obutton = sc->button;    /* previous button state */
            sc->button = ms->button =    /* latest button state */
                butmap[sc->ipacket[0] & MOUSE_PS2_BUTTONS]
		| ((sc->ipacket[0] & MOUSE_PS2_BUTTON4UP) 
		    ? 0 : MOUSE_BUTTON4DOWN);
#if 0
	    if ((verbose >= 2)
		&& ((sc->ipacket[0] & MOUSE_PS2_BUTTON4UP) == 0))
		log(LOG_DEBUG, "psm%d: button 4 down (%04x) (psmintr)\n",
		    unit, sc->ipacket[0]);
#endif
            sc->queue.tail =
                (sc->queue.tail + 1) % PSM_BUFSIZE;
            ++sc->queue.count;
            sc->inputbytes = 0;
        }
    }

    if (sc->state & PSM_ASLP) {
        sc->state &= ~PSM_ASLP;
        wakeup((caddr_t) sc);
    }
    selwakeup(&sc->rsel);
}

static int
psmselect(dev_t dev, int rw, struct proc *p)
{
    struct psm_softc *sc = psm_softc[PSM_UNIT(dev)];
    int s, ret;

    /* Silly to select for output */
    if (rw == FWRITE)
        return (0);

    /* Return true if a mouse event available */
    s = spltty();
    if ((sc->outputbytes > 0) || (sc->queue.count > 0)) {
        ret = 1;
    } else {
        selrecord(p, &sc->rsel);
        ret = 0;
    }
    splx(s);

    return (ret);
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

SYSINIT(psmdev, SI_SUB_DRIVERS, SI_ORDER_MIDDLE + CDEV_MAJOR, psm_drvinit, NULL)

#endif /* NPSM > 0 */
