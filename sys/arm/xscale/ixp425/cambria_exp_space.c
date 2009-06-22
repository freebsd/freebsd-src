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
 * Hack bus space tag for slow devices on the Cambria expansion bus;
 * we slow the timing and add a 2us delay between r/w ops.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>

#include <machine/bus.h>
#include <machine/cpu.h>

#include <arm/xscale/ixp425/ixp425reg.h>
#include <arm/xscale/ixp425/ixp425var.h>

/* Prototypes for all the bus_space structure functions */
bs_protos(exp);
bs_protos(generic);
bs_protos(generic_armv4);

static uint8_t
cambria_bs_r_1(void *t, bus_space_handle_t h, bus_size_t o)
{
	DELAY(2);
	return bus_space_read_1((struct bus_space *)t, h, o);
}

static void
cambria_bs_w_1(void *t, bus_space_handle_t h, bus_size_t o, u_int8_t v)
{
	DELAY(2);
	bus_space_write_1((struct bus_space *)t, h, o, v);
}

/* NB: we only define what's needed by uart */
struct bus_space cambria_exp_bs_tag = {
	/* mapping/unmapping */
	.bs_map		= generic_bs_map,
	.bs_unmap	= generic_bs_unmap,

	/* barrier */
	.bs_barrier	= generic_bs_barrier,

	/* read (single) */
	.bs_r_1		= cambria_bs_r_1,

	/* write (single) */
	.bs_w_1		= cambria_bs_w_1,
};

void
cambria_exp_bus_init(struct ixp425_softc *sc)
{
	uint32_t cs3;

	KASSERT(cpu_is_ixp43x(), ("wrong cpu type"));

	cambria_exp_bs_tag.bs_cookie = sc->sc_iot;

	cs3 = EXP_BUS_READ_4(sc, EXP_TIMING_CS3_OFFSET);
	/* XXX force slowest possible timings and byte mode */
	EXP_BUS_WRITE_4(sc, EXP_TIMING_CS3_OFFSET,
	    cs3 | (EXP_T1|EXP_T2|EXP_T3|EXP_T4|EXP_T5) | EXP_BYTE_EN);

	/* XXX force GPIO 3+4 for GPS+RS485 uarts */
	ixp425_set_gpio(sc, 3, GPIO_TYPE_EDG_RISING);
	ixp425_set_gpio(sc, 4, GPIO_TYPE_EDG_RISING);
}
