/*	$NetBSD: jziic.c,v 1.2 2015/04/21 06:12:41 macallan Exp $ */

/*-
 * Copyright (c) 2015 Michael Lorenz
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: jziic.c,v 1.2 2015/04/21 06:12:41 macallan Exp $");

/*
 * a preliminary driver for JZ4780's on-chip SMBus controllers
 * - needs more error handling and interrupt support
 * - transfers can't be more than the chip's FIFO, supposedly 16 bytes per 
 *   direction
 * so, good enough for RTCs but not much else yet
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/mutex.h>
#include <sys/bus.h>
#include <sys/mutex.h>
#include <sys/condvar.h>

#include <mips/ingenic/ingenic_var.h>
#include <mips/ingenic/ingenic_regs.h>

#include <dev/i2c/i2cvar.h>

#include "opt_ingenic.h"

#ifdef JZIIC_DEBUG
#define DPRINTF aprint_error
#define STATIC /* */
#else
#define DPRINTF while (0) printf
#define STATIC static
#endif

STATIC int jziic_match(device_t, struct cfdata *, void *);
STATIC void jziic_attach(device_t, device_t, void *);

struct jziic_softc {
	device_t 		sc_dev;
	bus_space_tag_t 	sc_memt;
	bus_space_handle_t 	sc_memh;
	struct i2c_controller 	sc_i2c;
	kmutex_t		sc_buslock, sc_cvlock;
	uint32_t		sc_pclk;
	/* stuff used for interrupt-driven transfers */
	const uint8_t		*sc_cmd;
	uint8_t			*sc_buf;
	uint32_t		sc_cmdlen, sc_buflen;
	uint32_t		sc_cmdptr, sc_bufptr, sc_rds;
	uint32_t		sc_abort;
	kcondvar_t		sc_ping;
	uint8_t			sc_txbuf[256];
	boolean_t		sc_reading;
};

CFATTACH_DECL_NEW(jziic, sizeof(struct jziic_softc),
    jziic_match, jziic_attach, NULL, NULL);

STATIC int jziic_enable(struct jziic_softc *);
STATIC void jziic_disable(struct jziic_softc *);
STATIC int jziic_wait(struct jziic_softc *);
STATIC void jziic_set_speed(struct jziic_softc *);
STATIC int jziic_i2c_acquire_bus(void *, int);
STATIC void jziic_i2c_release_bus(void *, int);
STATIC int jziic_i2c_exec(void *, i2c_op_t, i2c_addr_t, const void *, size_t,
		    void *, size_t, int);
STATIC int jziic_i2c_exec_poll(struct jziic_softc *, i2c_op_t, i2c_addr_t,
    const void *, size_t, void *, size_t, int);
STATIC int jziic_i2c_exec_intr(struct jziic_softc *, i2c_op_t, i2c_addr_t,
    const void *, size_t, void *, size_t, int);

STATIC int jziic_intr(void *);


/* ARGSUSED */
STATIC int
jziic_match(device_t parent, struct cfdata *match, void *aux)
{
	struct apbus_attach_args *aa = aux;

	if (strcmp(aa->aa_name, "jziic") != 0)
		return 0;

	return 1;
}

/* ARGSUSED */
STATIC void
jziic_attach(device_t parent, device_t self, void *aux)
{
	struct jziic_softc *sc = device_private(self);
	struct apbus_attach_args *aa = aux;
	struct i2cbus_attach_args iba;
	int error;
	void *ih;
#ifdef JZIIC_DEBUG
	int i;
	uint8_t in[1] = {0}, out[16];
#endif

	sc->sc_dev = self;
	sc->sc_pclk = aa->aa_pclk;
	sc->sc_memt = aa->aa_bst;

	error = bus_space_map(aa->aa_bst, aa->aa_addr, 0x100, 0, &sc->sc_memh);
	if (error) {
		aprint_error_dev(self,
		    "can't map registers for %s: %d\n", aa->aa_name, error);
		return;
	}

	mutex_init(&sc->sc_buslock, MUTEX_DEFAULT, IPL_NONE);
	mutex_init(&sc->sc_cvlock, MUTEX_DEFAULT, IPL_NONE);
	cv_init(&sc->sc_ping, device_xname(self));

	aprint_naive(": SMBus controller\n");
	aprint_normal(": SMBus controller\n");

	ih = evbmips_intr_establish(aa->aa_irq, jziic_intr, sc);

	if (ih == NULL) {
		aprint_error_dev(self, "failed to establish interrupt %d\n",
		     aa->aa_irq);
		goto fail;
	}

#ifdef JZIIC_DEBUG
	if (jziic_i2c_exec(sc, I2C_OP_READ_WITH_STOP, 0x51, in, 1, out, 9, 0)
	    >= 0) {
		for (i = 0; i < 9; i++)
			printf(" %02x", out[i]);
		printf("\n");
		delay(1000000);
		jziic_i2c_exec(sc, I2C_OP_READ_WITH_STOP,
		    0x51, in, 1, out, 9, 0);
		for (i = 0; i < 9; i++)
			printf(" %02x", out[i]);
		printf("\n");
		delay(1000000);
		jziic_i2c_exec(sc, I2C_OP_READ_WITH_STOP,
		    0x51, in, 1, out, 9, 0);
		for (i = 0; i < 9; i++)
			printf(" %02x", out[i]);
		printf("\n");
	}
#endif

	/* fill in the i2c tag */
	sc->sc_i2c.ic_cookie = sc;
	sc->sc_i2c.ic_acquire_bus = jziic_i2c_acquire_bus;
	sc->sc_i2c.ic_release_bus = jziic_i2c_release_bus;
	sc->sc_i2c.ic_send_start = NULL;
	sc->sc_i2c.ic_send_stop = NULL;
	sc->sc_i2c.ic_initiate_xfer = NULL;
	sc->sc_i2c.ic_read_byte = NULL;
	sc->sc_i2c.ic_write_byte = NULL;
	sc->sc_i2c.ic_exec = jziic_i2c_exec;

	iba.iba_tag = &sc->sc_i2c;
	(void) config_found_ia(sc->sc_dev, "i2cbus", &iba, iicbus_print);


	return;

fail:
	if (ih) {
		evbmips_intr_disestablish(ih);
	}
	bus_space_unmap(sc->sc_memt, sc->sc_memh, 0x100);
}

STATIC int
jziic_enable(struct jziic_softc *sc)
{
	int bail = 100000;
	uint32_t reg;

	bus_space_write_4(sc->sc_memt, sc->sc_memh, JZ_SMBENB, JZ_ENABLE);
	reg = bus_space_read_4(sc->sc_memt, sc->sc_memh, JZ_SMBENBST);
	DPRINTF("status: %02x\n", reg);
	while ((bail > 0) && (reg == 0)) {
		bail--;
		reg = bus_space_read_4(sc->sc_memt, sc->sc_memh, JZ_SMBENBST);
	}
	DPRINTF("bail: %d\n", bail);
	return (reg != 0);
}

STATIC void
jziic_disable(struct jziic_softc *sc)
{
	int bail = 100000;
	uint32_t reg;

	bus_space_write_4(sc->sc_memt, sc->sc_memh, JZ_SMBENB, 0);
	reg = bus_space_read_4(sc->sc_memt, sc->sc_memh, JZ_SMBENBST);
	DPRINTF("status: %02x\n", reg);
	while ((bail > 0) && (reg != 0)) {
		bail--;
		reg = bus_space_read_4(sc->sc_memt, sc->sc_memh, JZ_SMBENBST);
	}
	DPRINTF("bail: %d\n", bail);
}

STATIC int
jziic_i2c_acquire_bus(void *cookie, int flags)
{
	struct jziic_softc *sc = cookie;

	mutex_enter(&sc->sc_buslock);
	return 0;
}

STATIC void
jziic_i2c_release_bus(void *cookie, int flags)
{
	struct jziic_softc *sc = cookie;

	mutex_exit(&sc->sc_buslock);
}

STATIC int
jziic_wait(struct jziic_softc *sc)
{
	uint32_t reg;
	int bail = 10000;
	reg = bus_space_read_4(sc->sc_memt, sc->sc_memh, JZ_SMBST);
	while ((reg & JZ_MSTACT) && (bail > 0)) {
		delay(100);
		reg = bus_space_read_4(sc->sc_memt, sc->sc_memh, JZ_SMBST);
		bail--;
	}  
	return ((reg & JZ_MSTACT) == 0);
}

STATIC void
jziic_set_speed(struct jziic_softc *sc)
{
	int ticks, hcnt, lcnt, hold, setup;

	/* PCLK ticks per SMBus cycle */
	ticks = sc->sc_pclk / 100; /* assuming 100kHz for now */
	hcnt = (ticks * 40 / (40 + 47)) - 8;
	lcnt = (ticks * 47 / (40 + 47)) - 1;
	hold = sc->sc_pclk * 4 / 10000 - 1; /* ... * 400 / 1000000 ... */
	hold = max(1, hold);
	hold |= JZ_HDENB;
	setup = sc->sc_pclk * 3 / 10000 + 1; /* ... * 300 / 1000000 ... */
	DPRINTF("hcnt %d lcnt %d hold %d\n", hcnt, lcnt, hold);
	bus_space_write_4(sc->sc_memt, sc->sc_memh, JZ_SMBSHCNT, hcnt);
	bus_space_write_4(sc->sc_memt, sc->sc_memh, JZ_SMBSLCNT, lcnt);
	bus_space_write_4(sc->sc_memt, sc->sc_memh, JZ_SMBSDAHD, hold);
	bus_space_write_4(sc->sc_memt, sc->sc_memh, JZ_SMBSDASU, setup);
	bus_space_write_4(sc->sc_memt, sc->sc_memh, JZ_SMBCON,
	    JZ_SLVDIS | JZ_STPHLD | JZ_REST | JZ_SPD_100KB | JZ_MD);
	(void)bus_space_read_4(sc->sc_memt, sc->sc_memh, JZ_SMBCINT);
}

STATIC int
jziic_i2c_exec(void *cookie, i2c_op_t op, i2c_addr_t addr, const void *vcmd,
    size_t cmdlen, void *vbuf, size_t buflen, int flags)
{
	struct jziic_softc *sc = cookie;

	if (cold || (flags & I2C_F_POLL)) {
		return jziic_i2c_exec_poll(sc, op, addr, vcmd, cmdlen, vbuf,
		    buflen, flags);
	} else {
#ifdef JZIIC_DEBUG
		uint8_t *b = vbuf;
		int i, ret;

		memset(vbuf, 0, buflen);
		jziic_i2c_exec_intr(sc, op, addr, vcmd, cmdlen, vbuf,
		    buflen, flags);
		for (i = 0; i < buflen; i++) {
			printf(" %02x", b[i]);
		}
		printf("\n");
		ret = jziic_i2c_exec_poll(sc, op, addr, vcmd, cmdlen, vbuf,
		    buflen, flags);
		for (i = 0; i < buflen; i++) {
			printf(" %02x", b[i]);
		}
		printf("\n");
		return ret;
#else
		return jziic_i2c_exec_intr(sc, op, addr, vcmd, cmdlen, vbuf,
		    buflen, flags);
#endif
	}
}

STATIC int
jziic_i2c_exec_poll(struct jziic_softc *sc, i2c_op_t op, i2c_addr_t addr,
    const void *vcmd, size_t cmdlen, void *vbuf, size_t buflen, int flags)
{
	int i, bail = 10000, ret = 0;
	uint32_t abort;
	uint8_t *rx, data;
	const uint8_t *tx;

	tx = vcmd;
	rx = vbuf;

	DPRINTF("%s: 0x%02x %d %d\n", __func__, addr, cmdlen, buflen);

	jziic_disable(sc);

	/* we're polling, so disable interrupts */
	bus_space_write_4(sc->sc_memt, sc->sc_memh, JZ_SMBINTM, 0);

	jziic_set_speed(sc);
	jziic_wait(sc);
	/* try to talk... */

	if (!jziic_enable(sc)) {
		ret = -1;
		goto bork;
	}
	bus_space_write_4(sc->sc_memt, sc->sc_memh, JZ_SMBINTM, 0);

	bus_space_write_4(sc->sc_memt, sc->sc_memh, JZ_SMBTAR, addr);
	jziic_wait(sc);
	DPRINTF("st: %02x\n",
	    bus_space_read_4(sc->sc_memt, sc->sc_memh, JZ_SMBST));
	DPRINTF("wr int: %02x\n",
	    bus_space_read_4(sc->sc_memt, sc->sc_memh, JZ_SMBINTST));
	abort = bus_space_read_4(sc->sc_memt, sc->sc_memh, JZ_SMBABTSRC);
	DPRINTF("abort: %02x\n", abort);
	if ((abort != 0)) {
		ret = -1;
		goto bork;
	}

	do {
		bail--;
		delay(100);
	} while (((bus_space_read_4(sc->sc_memt, sc->sc_memh, JZ_SMBST) &
	           JZ_TFE) == 0) && (bail > 0));

	if (cmdlen != 0) {
		for (i = 0; i < cmdlen; i++) {
			bus_space_write_4(sc->sc_memt, sc->sc_memh,
			    JZ_SMBDC, *tx);
			tx++;
		}
	}

	if (I2C_OP_READ_P(op)) {
		/* now read */
		for (i = 0; i < (buflen + 1); i++) {
			bus_space_write_4(sc->sc_memt, sc->sc_memh,
			    JZ_SMBDC, JZ_CMD);
		}
		wbflush();
		DPRINTF("rd st: %02x\n",
		    bus_space_read_4(sc->sc_memt, sc->sc_memh, JZ_SMBST));
		DPRINTF("rd int: %02x\n",
		    bus_space_read_4(sc->sc_memt, sc->sc_memh, JZ_SMBINTST));
		DPRINTF("abort: %02x\n",
		    bus_space_read_4(sc->sc_memt, sc->sc_memh, JZ_SMBABTSRC));
		for (i = 0; i < buflen; i++) {
			bail = 10000;
			while (((bus_space_read_4(sc->sc_memt, sc->sc_memh,
				  JZ_SMBST) & JZ_RFNE) == 0) && (bail > 0)) {
				bail--;
				delay(100);
			} 
			if (bail == 0) {
				ret = -1;
				goto bork;
			}
			data = bus_space_read_4(sc->sc_memt, sc->sc_memh,
			    JZ_SMBDC);
			DPRINTF("rd st: %02x %d\n",
			  bus_space_read_4(sc->sc_memt, sc->sc_memh, JZ_SMBST),
			  bail);
			DPRINTF("rd int: %02x\n",
			  bus_space_read_4(sc->sc_memt, sc->sc_memh,
			   JZ_SMBINTST));
			DPRINTF("abort: %02x\n", abort);
			DPRINTF("rd data: %02x\n", data);
			*rx = data;
			rx++;
		}
	} else {
		tx = vbuf;
		for (i = 0; i < buflen; i++) {
			DPRINTF("wr data: %02x\n", *tx);
			bus_space_write_4(sc->sc_memt, sc->sc_memh,
			    JZ_SMBDC, *tx);
			wbflush();
			tx++;
		}
		jziic_wait(sc);
		abort = bus_space_read_4(sc->sc_memt, sc->sc_memh,
		    JZ_SMBABTSRC);
		DPRINTF("abort: %02x\n", abort);
		if ((abort != 0)) {
			ret = -1;
			goto bork;
		}

		DPRINTF("st: %02x %d\n",
		    bus_space_read_4(sc->sc_memt, sc->sc_memh, JZ_SMBST), bail);
		DPRINTF("wr int: %02x\n",
		    bus_space_read_4(sc->sc_memt, sc->sc_memh, JZ_SMBINTST));
	}
	bus_space_write_4(sc->sc_memt, sc->sc_memh, JZ_SMBCON,
	    JZ_SLVDIS | JZ_REST | JZ_SPD_100KB | JZ_MD);
bork:
	jziic_disable(sc);
	return ret;
}

STATIC int
jziic_i2c_exec_intr(struct jziic_softc *sc, i2c_op_t op, i2c_addr_t addr,
    const void *vcmd, size_t cmdlen, void *vbuf, size_t buflen, int flags)
{
	int i, ret = 0, bail;

	DPRINTF("%s: 0x%02x %d %d\n", __func__, addr, cmdlen, buflen);

	jziic_disable(sc);
	bus_space_write_4(sc->sc_memt, sc->sc_memh, JZ_SMBINTM, 0);

	mutex_enter(&sc->sc_cvlock);

	sc->sc_reading = FALSE;

	if (I2C_OP_READ_P(op)) {
		sc->sc_cmd = vcmd;
		sc->sc_cmdlen = cmdlen;
		sc->sc_buf = vbuf;
		sc->sc_buflen = buflen;
		memset(vbuf, 0, buflen);
	} else {
		if ((cmdlen + buflen) > 256)
			return -1;
		memcpy(sc->sc_txbuf, vcmd, cmdlen);
		memcpy(sc->sc_txbuf + cmdlen, vbuf, buflen);
		sc->sc_cmd = sc->sc_txbuf;
		sc->sc_cmdlen = cmdlen + buflen;
		sc->sc_buf = NULL;
		sc->sc_buflen = 0;
	}
	sc->sc_cmdptr = 0;
	sc->sc_bufptr = 0;
	sc->sc_rds = 0;
	sc->sc_abort = 0;

	jziic_set_speed(sc);
	jziic_wait(sc);

	/* set FIFO levels */
	bus_space_write_4(sc->sc_memt, sc->sc_memh, JZ_SMBTXTL, 4);
	bus_space_write_4(sc->sc_memt, sc->sc_memh, JZ_SMBRXTL, 0
	    /*min(7, max(0, buflen - 2 ))*/);

	/* try to talk... */

	if (!jziic_enable(sc)) {
		ret = -1;
		goto bork;
	}
	bus_space_write_4(sc->sc_memt, sc->sc_memh, JZ_SMBINTM, 0);

	bus_space_write_4(sc->sc_memt, sc->sc_memh, JZ_SMBTAR, addr);
	jziic_wait(sc);
	bus_space_write_4(sc->sc_memt, sc->sc_memh, JZ_SMBCINT, JZ_CLEARALL);
	bus_space_write_4(sc->sc_memt, sc->sc_memh, JZ_SMBINTM,
	    JZ_TXABT | JZ_TXEMP);

	bail = 100 * sc->sc_cmdlen;
	while ((sc->sc_cmdptr < sc->sc_cmdlen) && (bail > 0)) {
		cv_timedwait(&sc->sc_ping, &sc->sc_cvlock, 1);
		if (sc->sc_abort) {
			/* we received an abort interrupt -> bailout */
		    	DPRINTF("abort: %x\n", sc->sc_abort);
		    	ret = -1;
		    	goto bork;
		}
	    	bail--;
	}

	if (sc->sc_cmdptr < sc->sc_cmdlen) {
		/* we didn't send everything? */
	    	DPRINTF("sent %d of %d\n", sc->sc_cmdptr, sc->sc_cmdlen);
	    	ret = -1;
	    	goto bork;
	}

	if (I2C_OP_READ_P(op)) {
		/* now read */
		sc->sc_reading = TRUE;
		bus_space_write_4(sc->sc_memt, sc->sc_memh, JZ_SMBINTM,
		    JZ_TXABT | JZ_RXFL | JZ_TXEMP);

		for (i = 0; i < min((buflen + 1), 4); i++) {
			bus_space_write_4(sc->sc_memt, sc->sc_memh,
			    JZ_SMBDC, JZ_CMD);
			wbflush();
		}
		sc->sc_rds = i;

		bail = 10 * sc->sc_buflen; /* 10 ticks per byte should be ok */
		while ((sc->sc_bufptr < sc->sc_buflen) && (bail > 0)) {
			cv_timedwait(&sc->sc_ping, &sc->sc_cvlock, 1); 
			if (sc->sc_abort) {
				/* we received an abort interrupt -> bailout */
		  	  	DPRINTF("rx abort: %x\n", sc->sc_abort);
			    	ret = -1;
			    	goto bork;
			}
			bail--;
		}

		if (sc->sc_bufptr < sc->sc_buflen) {
			/* we didn't get everything? */
		    	DPRINTF("rcvd %d of %d\n", sc->sc_bufptr, sc->sc_buflen);
		    	ret = -1;
		    	goto bork;
		}
	}
	bus_space_write_4(sc->sc_memt, sc->sc_memh, JZ_SMBCON,
	    JZ_SLVDIS | JZ_REST | JZ_SPD_100KB | JZ_MD);
bork:
	bus_space_write_4(sc->sc_memt, sc->sc_memh, JZ_SMBINTM, 0);
	jziic_disable(sc);
	mutex_exit(&sc->sc_cvlock);
	return ret;
}

STATIC int
jziic_intr(void *cookie)
{
	struct jziic_softc *sc = cookie;
	uint32_t stat, data, rstat;
	int i;

	stat = bus_space_read_4(sc->sc_memt, sc->sc_memh, JZ_SMBINTST);
	if (stat & JZ_TXEMP) {
		if (sc->sc_reading) {
			if (sc->sc_rds < (sc->sc_buflen + 1)) {
				for (i = 0;
				     i < min(4, (sc->sc_buflen + 1) -
				                 sc->sc_rds);
				     i++) {
					bus_space_write_4( sc->sc_memt,
					    sc->sc_memh,
					    JZ_SMBDC, JZ_CMD);
					wbflush();
				}
				sc->sc_rds += i;
			} else {
				/* we're done, so turn TX FIFO interrupt off */
				bus_space_write_4(sc->sc_memt, sc->sc_memh,
				    JZ_SMBINTM,
				    JZ_TXABT | JZ_RXFL);
			}
		} else {		
			rstat = bus_space_read_4(sc->sc_memt, sc->sc_memh,
			    JZ_SMBST);
			while ((rstat & JZ_TFNF) && 
			         (sc->sc_cmdptr < sc->sc_cmdlen)) {
				data = *sc->sc_cmd;
				sc->sc_cmd++;
				sc->sc_cmdptr++;
				bus_space_write_4(sc->sc_memt, sc->sc_memh,
				    JZ_SMBDC, data & 0xff);
				rstat = bus_space_read_4(sc->sc_memt, sc->sc_memh,
				    JZ_SMBST);
			};
			/* no need to clear this one */
			if (sc->sc_cmdptr >= sc->sc_cmdlen) {
				cv_signal(&sc->sc_ping);
				bus_space_write_4(sc->sc_memt, sc->sc_memh,
				    JZ_SMBINTM, JZ_TXABT);
			}
		}			
	}
	if (stat & JZ_RXFL) {
		rstat = bus_space_read_4(sc->sc_memt, sc->sc_memh, JZ_SMBST);
		while ((rstat & JZ_RFNE) && (sc->sc_bufptr < sc->sc_buflen)) {
			data = bus_space_read_4(sc->sc_memt, sc->sc_memh,
			   JZ_SMBDC);
			*sc->sc_buf = (uint8_t)(data & 0xff);
			sc->sc_buf++;
			sc->sc_bufptr++;
			rstat = bus_space_read_4(sc->sc_memt, sc->sc_memh,
			    JZ_SMBST);
		}
		if (sc->sc_bufptr >= sc->sc_buflen)
			cv_signal(&sc->sc_ping);
	}
	if (stat & JZ_TXABT) {
		sc->sc_abort = bus_space_read_4(sc->sc_memt, sc->sc_memh,
		    JZ_SMBABTSRC);
		cv_signal(&sc->sc_ping);
		bus_space_write_4(sc->sc_memt, sc->sc_memh, JZ_SMBCINT,
		    JZ_CLEARALL);
		bus_space_write_4(sc->sc_memt, sc->sc_memh, JZ_SMBINTM, 0);
	}
	return 0;
}
