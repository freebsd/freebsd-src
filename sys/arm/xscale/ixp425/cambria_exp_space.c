/*-
 * Copyright (c) 2009 Sam Leffler.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Bus space tag for devices on the Cambria expansion bus.
 * This interlocks accesses to allow the optional GPS+RS485 UART's
 * to share access with the CF-IDE adapter.  Note this does not
 * slow the timing UART r/w ops because the lock operation does
 * this implicitly for us.  Also note we do not DELAY after byte/word
 * chip select changes; this doesn't seem necessary (as required
 * for IXP425/Avila boards).
 *
 * XXX should make this generic so all expansion bus devices can
 * use it but probably not until we eliminate the ATA hacks
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/bus.h>
#include <sys/endian.h>

#include <machine/bus.h>
#include <machine/cpu.h>

#include <arm/xscale/ixp425/ixp425reg.h>
#include <arm/xscale/ixp425/ixp425var.h>

/* Prototypes for all the bus_space structure functions */
bs_protos(exp);
bs_protos(generic);

struct expbus_softc {
	struct ixp425_softc *sc;	/* bus space tag */
	struct mtx	lock;		/* i/o interlock */
	bus_size_t	csoff;		/* CS offset for 8/16 enable */
};
#define	EXP_LOCK_INIT(exp) \
	mtx_init(&(exp)->lock, "ExpBus", NULL, MTX_SPIN)
#define	EXP_LOCK_DESTROY(exp) \
	mtx_destroy(&(exp)->lock)
#define	EXP_LOCK(exp)	mtx_lock_spin(&(exp)->lock)
#define	EXP_UNLOCK(exp)	mtx_unlock_spin(&(exp)->lock)

/*
 * Enable/disable 16-bit ops on the expansion bus.
 */
static __inline void
enable_16(struct ixp425_softc *sc, bus_size_t cs)
{
	EXP_BUS_WRITE_4(sc, cs, EXP_BUS_READ_4(sc, cs) &~ EXP_BYTE_EN);
}

static __inline void
disable_16(struct ixp425_softc *sc, bus_size_t cs)
{
	EXP_BUS_WRITE_4(sc, cs, EXP_BUS_READ_4(sc, cs) | EXP_BYTE_EN);
}

static uint8_t
cambria_bs_r_1(void *t, bus_space_handle_t h, bus_size_t o)
{
	struct expbus_softc *exp = t;
	struct ixp425_softc *sc = exp->sc;
	uint8_t v;

	EXP_LOCK(exp);
	v = bus_space_read_1(sc->sc_iot, h, o);
	EXP_UNLOCK(exp);
	return v;
}

static void
cambria_bs_w_1(void *t, bus_space_handle_t h, bus_size_t o, u_int8_t v)
{
	struct expbus_softc *exp = t;
	struct ixp425_softc *sc = exp->sc;

	EXP_LOCK(exp);
	bus_space_write_1(sc->sc_iot, h, o, v);
	EXP_UNLOCK(exp);
}

static uint16_t
cambria_bs_r_2(void *t, bus_space_handle_t h, bus_size_t o)
{
	struct expbus_softc *exp = t;
	struct ixp425_softc *sc = exp->sc;
	uint16_t v;

	EXP_LOCK(exp);
	enable_16(sc, exp->csoff);
	v = bus_space_read_2(sc->sc_iot, h, o);
	disable_16(sc, exp->csoff);
	EXP_UNLOCK(exp);
	return v;
}

static void
cambria_bs_w_2(void *t, bus_space_handle_t h, bus_size_t o, uint16_t v)
{
	struct expbus_softc *exp = t;
	struct ixp425_softc *sc = exp->sc;

	EXP_LOCK(exp);
	enable_16(sc, exp->csoff);
	bus_space_write_2(sc->sc_iot, h, o, v);
	disable_16(sc, exp->csoff);
	EXP_UNLOCK(exp);
}

static void
cambria_bs_rm_2(void *t, bus_space_handle_t h, bus_size_t o,
	u_int16_t *d, bus_size_t c)
{
	struct expbus_softc *exp = t;
	struct ixp425_softc *sc = exp->sc;

	EXP_LOCK(exp);
	enable_16(sc, exp->csoff);
	bus_space_read_multi_2(sc->sc_iot, h, o, d, c);
	disable_16(sc, exp->csoff);
	EXP_UNLOCK(exp);
}

static void
cambria_bs_wm_2(void *t, bus_space_handle_t h, bus_size_t o,
	const u_int16_t *d, bus_size_t c)
{
	struct expbus_softc *exp = t;
	struct ixp425_softc *sc = exp->sc;

	EXP_LOCK(exp);
	enable_16(sc, exp->csoff);
	bus_space_write_multi_2(sc->sc_iot, h, o, d, c);
	disable_16(sc, exp->csoff);
	EXP_UNLOCK(exp);
}

/* XXX workaround ata driver by (incorrectly) byte swapping stream cases */

static void
cambria_bs_rm_2_s(void *t, bus_space_handle_t h, bus_size_t o,
	u_int16_t *d, bus_size_t c)
{
	struct expbus_softc *exp = t;
	struct ixp425_softc *sc = exp->sc;
	uint16_t v;
	bus_size_t i;

	EXP_LOCK(exp);
	enable_16(sc, exp->csoff);
#if 1
	for (i = 0; i < c; i++) {
		v = bus_space_read_2(sc->sc_iot, h, o);
		d[i] = bswap16(v);
	}
#else
	bus_space_read_multi_stream_2(sc->sc_iot, h, o, d, c);
#endif
	disable_16(sc, exp->csoff);
	EXP_UNLOCK(exp);
}

static void
cambria_bs_wm_2_s(void *t, bus_space_handle_t h, bus_size_t o,
	const u_int16_t *d, bus_size_t c)
{
	struct expbus_softc *exp = t;
	struct ixp425_softc *sc = exp->sc;
	bus_size_t i;

	EXP_LOCK(exp);
	enable_16(sc, exp->csoff);
#if 1
	for (i = 0; i < c; i++)
		bus_space_write_2(sc->sc_iot, h, o, bswap16(d[i]));
#else
	bus_space_write_multi_stream_2(sc->sc_iot, h, o, d, c);
#endif
	disable_16(sc, exp->csoff);
	EXP_UNLOCK(exp);
}

/* NB: we only define what's needed by ata+uart */
struct bus_space cambria_exp_bs_tag = {
	/* mapping/unmapping */
	.bs_map		= generic_bs_map,
	.bs_unmap	= generic_bs_unmap,

	/* barrier */
	.bs_barrier	= generic_bs_barrier,

	/* read (single) */
	.bs_r_1		= cambria_bs_r_1,
	.bs_r_2		= cambria_bs_r_2,

	/* write (single) */
	.bs_w_1		= cambria_bs_w_1,
	.bs_w_2		= cambria_bs_w_2,

	/* read multiple */
	.bs_rm_2	= cambria_bs_rm_2,
	.bs_rm_2_s	= cambria_bs_rm_2_s,

	/* write multiple */
	.bs_wm_2	= cambria_bs_wm_2,
	.bs_wm_2_s	= cambria_bs_wm_2_s,
};

void
cambria_exp_bus_init(struct ixp425_softc *sc)
{
	static struct expbus_softc c3;		/* NB: no need to malloc */
	uint32_t cs3;

	KASSERT(cpu_is_ixp43x(), ("wrong cpu type"));

	c3.sc = sc;
	c3.csoff = EXP_TIMING_CS3_OFFSET;
	EXP_LOCK_INIT(&c3);
	cambria_exp_bs_tag.bs_cookie = &c3;

	cs3 = EXP_BUS_READ_4(sc, EXP_TIMING_CS3_OFFSET);
	/* XXX force slowest possible timings and byte mode */
	EXP_BUS_WRITE_4(sc, EXP_TIMING_CS3_OFFSET,
	    cs3 | (EXP_T1|EXP_T2|EXP_T3|EXP_T4|EXP_T5) |
	        EXP_BYTE_EN | EXP_WR_EN | EXP_BYTE_RD16 | EXP_CS_EN);

	/* XXX force GPIO 3+4 for GPS+RS485 uarts */
	ixp425_set_gpio(sc, 3, GPIO_TYPE_EDG_RISING);
	ixp425_set_gpio(sc, 4, GPIO_TYPE_EDG_RISING);
}
