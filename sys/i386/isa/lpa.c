/*
 * Copyright (c) 1990 William F. Jolitz, TeleMuse
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *   This software is a component of "386BSD" developed by 
 *   William F. Jolitz, TeleMuse.
 * 4. Neither the name of the developer nor the name "386BSD"
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS A COMPONENT OF 386BSD DEVELOPED BY WILLIAM F. JOLITZ 
 * AND IS INTENDED FOR RESEARCH AND EDUCATIONAL PURPOSES ONLY. THIS 
 * SOFTWARE SHOULD NOT BE CONSIDERED TO BE A COMMERCIAL PRODUCT. 
 * THE DEVELOPER URGES THAT USERS WHO REQUIRE A COMMERCIAL PRODUCT 
 * NOT MAKE USE OF THIS WORK.
 *
 * FOR USERS WHO WISH TO UNDERSTAND THE 386BSD SYSTEM DEVELOPED
 * BY WILLIAM F. JOLITZ, WE RECOMMEND THE USER STUDY WRITTEN 
 * REFERENCES SUCH AS THE  "PORTING UNIX TO THE 386" SERIES 
 * (BEGINNING JANUARY 1991 "DR. DOBBS JOURNAL", USA AND BEGINNING 
 * JUNE 1991 "UNIX MAGAZIN", GERMANY) BY WILLIAM F. JOLITZ AND 
 * LYNNE GREER JOLITZ, AS WELL AS OTHER BOOKS ON UNIX AND THE 
 * ON-LINE 386BSD USER MANUAL BEFORE USE. A BOOK DISCUSSING THE INTERNALS 
 * OF 386BSD ENTITLED "386BSD FROM THE INSIDE OUT" WILL BE AVAILABLE LATE 1992.
 *
 * THIS SOFTWARE IS PROVIDED BY THE DEVELOPER ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE DEVELOPER BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$Id: lpa.c,v 1.3 1993/09/24 20:37:32 rgrimes Exp $
 */

/*
 * Device Driver for AT parallel printer port, without using interrupts
 */

#include "lpa.h"
#if NLPA > 0

#include "param.h"
#include "buf.h"
#include "systm.h"
#include "ioctl.h"
#include "tty.h"
#include "proc.h"
#include "user.h"
#include "uio.h"
#include "kernel.h"
#include "malloc.h"

#include "i386/isa/isa.h"
#include "i386/isa/isa_device.h"
#include "i386/isa/lptreg.h"

/* internal used flags */
#define   OPEN        (0x01)   /* device is open */
#define   INIT        (0x02)   /* device in open procedure */

/* flags from minor device */
#define   LPA_PRIME   (0x20)   /* prime printer on open   */
#define   LPA_ERROR   (0x10)   /* log error conditions    */

#define   LPA_FLAG(x) ((x) & 0xfc)
#define   LPA_UNIT(x) ((x) & 0x03)

/* Printer Ready condition */
#define   LPS_INVERT  (LPS_NBSY | LPS_NACK |           LPS_SEL | LPS_NERR)
#define   LPS_MASK    (LPS_NBSY | LPS_NACK | LPS_OUT | LPS_SEL | LPS_NERR)
#define   NOT_READY()   ((inb(sc->sc_stat)^LPS_INVERT)&LPS_MASK)

/* tsleep priority */
#define   LPPRI       ((PZERO+8) | PCATCH)

/* debug flags */
#ifndef DEBUG
#define lprintf
#else
#define lprintf		if (lpaflag) printf
int lpaflag = 1;
#endif

int lpaprobe(), lpaattach();
struct   isa_driver lpadriver = {lpaprobe, lpaattach, "lpa"};

/*
 *   copy usermode data into sysmode buffer
 */
#define   BUFSIZE      1024

/*
**   Waittimes
*/
#define   TIMEOUT   (hz*16)   /* Timeout while open device */
#define   LONG      (hz* 1)   /* Timesteps while open      */
#define   MAX_SLEEP (hz*5)    /* Timeout while waiting for device ready */
#define   MAX_SPIN  20        /* Max delay for device ready in usecs */

struct lpa_softc {
	char	*sc_cp;		/* current data to print	*/
	int	sc_count;	/* bytes queued in sc_inbuf	*/
	short	sc_data;	/* printer data port		*/
	short	sc_stat;	/* printer control port		*/
	short	sc_ctrl;	/* printer status port		*/
	u_char	sc_flags;	/* flags (open and internal)	*/
	u_char	sc_unit;	/* unit-number			*/
	char			/* buffer for data		*/
	 *sc_inbuf;
} lpa_sc[NLPA];

/*
 * Internal routine to lpaprobe to do port tests of one byte value
 */
int
lpa_port_test(short port, u_char data, u_char mask)
	{
	int	temp, timeout;

	data = data & mask;
	outb(port, data);
	timeout = 100;
	do
		temp = inb(port) & mask;
	while (temp != data && --timeout);
	lprintf("Port 0x%x\tout=%x\tin=%x\n", port, data, temp);
	return (temp == data);
	}

/*
 * New lpaprobe routine written by Rodney W. Grimes, 3/25/1993
 *
 * Logic:
 *	1) You should be able to write to and read back the same value
 *	   to the data port.  Do an alternating zeros, alternating ones,
 *	   walking zero, and walking one test to check for stuck bits.
 *
 *	2) You should be able to write to and read back the same value
 *	   to the control port lower 5 bits, the upper 3 bits are reserved
 *	   per the IBM PC technical reference manauls and different boards
 *	   do different things with them.  Do an alternating zeros, alternating
 *	   ones, walking zero, and walking one test to check for stuck bits.
 *
 *	   Some printers drag the strobe line down when the are powered off
 * 	   so this bit has been masked out of the control port test.
 *
 *	   XXX Some printers may not like a fast pulse on init or strobe, I
 *	   don't know at this point, if that becomes a problem these bits
 *	   should be turned off in the mask byte for the control port test.
 *
 *	3) Set the data and control ports to a value of 0
 */

int
lpaprobe(struct isa_device *dvp)
	{
	int	status;
	short	port;
	u_char	data;
	u_char	mask;
	int	i;

	status = IO_LPTSIZE;

	port = dvp->id_iobase + lpt_data;
	mask = 0xff;
	while (mask != 0)
	{
		data = 0x55;				/* Alternating zeros */
		if (!lpa_port_test(port, data, mask)) status = 0;

		data = 0xaa;				/* Alternating ones */
		if (!lpa_port_test(port, data, mask)) status = 0;

		for (i = 0; i < 8; i++)			/* Walking zero */
			{
			data = ~(1 << i);
			if (!lpa_port_test(port, data, mask)) status = 0;
			}

		for (i = 0; i < 8; i++)			/* Walking one */
			{
			data = (1 << i);
			if (!lpa_port_test(port, data, mask)) status = 0;
			}

		if (port == dvp->id_iobase + lpt_data)
			{
			port = dvp->id_iobase + lpt_control;
			mask = 0x1e;
			}
		else
			mask = 0;
		}
	outb(dvp->id_iobase+lpt_data, 0);
	outb(dvp->id_iobase+lpt_control, 0);
	return (status);
	}

/*
 * lpaattach()
 *	Install device
 */
lpaattach(isdp)
	struct isa_device *isdp;
{
	struct   lpa_softc   *sc;

	sc = lpa_sc + isdp->id_unit;
	sc->sc_unit = isdp->id_unit;
	sc->sc_data = isdp->id_iobase + lpt_data;
	sc->sc_stat = isdp->id_iobase + lpt_status;
	sc->sc_ctrl = isdp->id_iobase + lpt_control;
	outb(sc->sc_ctrl, LPC_NINIT);
	return (1);
}

/*
 * lpaopen()
 *	New open on device.
 *
 * We forbid all but first open
 */
lpaopen(dev, flag)
	dev_t dev;
	int flag;
{
	struct lpa_softc *sc;
	int delay;	/* slept time in 1/hz seconds of tsleep */
	int err;
	u_char sta, unit;

	unit= LPA_UNIT(minor(dev));
	sta = LPA_FLAG(minor(dev));

	/* minor number out of limits ? */
	if (unit >= NLPA)
		return (ENXIO);
	sc = lpa_sc + unit;

	/* Attached ? */
	if (!sc->sc_ctrl) { /* not attached */
		return(ENXIO);
	}

	/* Printer busy ? */
	if (sc->sc_flags) { /* too late .. */
		return(EBUSY);
	}

	/* Have memory for buffer? */
	sc->sc_inbuf = malloc(BUFSIZE, M_DEVBUF, M_WAITOK);
	if (sc->sc_inbuf == 0)
		return(ENOMEM);

	/* Init printer */
	sc->sc_flags = sta | INIT;
	if (sc->sc_flags & LPA_PRIME) {
		outb(sc->sc_ctrl, 0);
	}

	/* Select printer */
	outb(sc->sc_ctrl, LPC_SEL|LPC_NINIT);

	/* and wait for ready .. */
	for (delay=0; NOT_READY(); delay+= LONG) {
		if (delay >= TIMEOUT) { /* too long waited .. */
			sc->sc_flags = 0;
			return (EBUSY);
		}

		/* sleep a moment */
		if ((err = tsleep (sc, LPPRI, "lpaopen", LONG)) !=
				EWOULDBLOCK) {
			sc->sc_flags = 0;
			return (EBUSY);
		}
	}

	/* Printer ready .. set variables */
	sc->sc_flags |= OPEN;
	sc->sc_count = 0;

	return(0);
}

/*
 * pushbytes()
 *	Workhorse for actually spinning and writing bytes to printer
 */
static
pushbytes(sc)
	struct lpa_softc *sc;
{
	int spin, err, tic;
	char ch;

	/* loop for every character .. */
	while (sc->sc_count > 0) {
		/* printer data */
		ch = *(sc->sc_cp);
		sc->sc_cp += 1;
		sc->sc_count -= 1;

              /*
               * Wait for printer ready.
               * Loop 20 usecs testing BUSY bit, then sleep
               * for exponentially increasing timeout. (vak)
               */
              for (spin=0; NOT_READY() && spin<MAX_SPIN; ++spin)
                      DELAY(1);
              if (spin >= MAX_SPIN) {
                      tic = 0;
                      while (NOT_READY()) {
				/*
				 * Now sleep, every cycle a
				 * little longer ..
				 */
				tic = tic + tic + 1;
                                /*
                                 * But no more than 10 seconds. (vak)
                                 */
                                if (tic > MAX_SLEEP)
                                        tic = MAX_SLEEP;
                                err = tsleep(sc, LPPRI, "lpawrite", tic);
				if (err != EWOULDBLOCK) {
					return (err);
				}
			}
		}

                /* output data */
		outb(sc->sc_data, ch);
		/* strobe */
		outb(sc->sc_ctrl, LPC_NINIT|LPC_SEL|LPC_STB);
		outb(sc->sc_ctrl, LPC_NINIT|LPC_SEL);

	}
	return(0);
}

/*
 * lpaclose()
 *	Close on lp.  Try to flush data in buffer out.
 */
lpaclose(dev, flag)
	dev_t dev;
	int flag;
{
	struct lpa_softc *sc = lpa_sc + LPA_UNIT(minor(dev));

	/* If there's queued data, try to flush it */
	(void)pushbytes(sc);

	/* really close .. quite simple :-)  */
	outb(sc->sc_ctrl, LPC_NINIT);
	sc->sc_flags = 0;
	free(sc->sc_inbuf, M_DEVBUF);
	sc->sc_inbuf = 0;	/* Sanity */
	return(0);
}

/*
 * lpawrite()
 *	Copy from user's buffer, then print
 */
lpawrite(dev, uio)
	dev_t dev;
	struct uio *uio;
{
	struct lpa_softc *sc = lpa_sc + LPA_UNIT(minor(dev));
	int err;

	/* Write out old bytes from interrupted syscall */
	if (sc->sc_count > 0) {
		err = pushbytes(sc);
		if (err)
			return(err);
	}

	/* main loop */
	while ((sc->sc_count = MIN(BUFSIZE, uio->uio_resid)) > 0) {
		/*  get from user-space  */
		sc->sc_cp = sc->sc_inbuf;
		uiomove(sc->sc_inbuf, sc->sc_count, uio);
		err = pushbytes(sc);
		if (err)
			return(err);
	}
	return(0);
}

int
lpaioctl(dev_t dev, int cmd, caddr_t data, int flag)
{
	int	error;

	error = 0;
	switch (cmd) {
#ifdef THISISASAMPLE
	case XXX:
		dothis; andthis; andthat;
		error=x;
		break;
#endif /* THISISASAMPLE */
	default:
		error = ENODEV;
	}

	return(error);
}

#endif /* NLPA > 0 */
