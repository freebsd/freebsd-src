/* $NetBSD$ */
/* $FreeBSD$ */

/*-
 * Copyright (c) 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Bill Sommerfeld
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
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

#define AM79C930_BUS_PCMCIA 1
#define AM79C930_BUS_ISAPNP 2	/* not implemented */

struct am79c930_softc 
{
	bus_space_tag_t sc_iot;
	bus_space_handle_t sc_ioh;

	bus_space_tag_t sc_memt;
	bus_space_handle_t sc_memh;

	struct am79c930_ops *sc_ops;

	int sc_bustype;
};

struct am79c930_ops 
{
	void (*write_1)(struct am79c930_softc *, u_int32_t, u_int8_t);
	void (*write_2)(struct am79c930_softc *, u_int32_t, u_int16_t);
	void (*write_4)(struct am79c930_softc *, u_int32_t, u_int32_t);
	void (*write_bytes)(struct am79c930_softc *, u_int32_t, u_int8_t *, size_t);

	u_int8_t (*read_1)(struct am79c930_softc *, u_int32_t);
	u_int16_t (*read_2)(struct am79c930_softc *, u_int32_t);
	u_int32_t (*read_4)(struct am79c930_softc *, u_int32_t);
	void (*read_bytes)(struct am79c930_softc *, u_int32_t, u_int8_t *, size_t);
};

void am79c930_chip_init(struct am79c930_softc *sc, int);

void am79c930_gcr_setbits(struct am79c930_softc *sc, u_int8_t bits);
void am79c930_gcr_clearbits(struct am79c930_softc *sc, u_int8_t bits);

u_int8_t am79c930_gcr_read(struct am79c930_softc *sc);

#define am79c930_hard_reset(sc) am79c930_gcr_setbits(sc, AM79C930_GCR_CORESET)
#define am79c930_hard_reset_off(sc) am79c930_gcr_clearbits(sc, AM79C930_GCR_CORESET)


