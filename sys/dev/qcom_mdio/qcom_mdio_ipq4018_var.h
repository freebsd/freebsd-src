/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2022 Adrian Chadd <adrian@FreeBSD.org>.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
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

#ifndef	__QCOM_MDIO_IPQ4018_VAR_H__
#define	__QCOM_MDIO_IPQ4018_VAR_H__

#define	MDIO_LOCK(_sc)		mtx_lock(&(_sc)->sc_mtx)
#define	MDIO_UNLOCK(_sc)	mtx_unlock(&(_sc)->sc_mtx)
#define	MDIO_LOCK_ASSERT(_sc)	mtx_assert(&(_sc)->sc_mtx, MA_OWNED)

/*
 * register space access macros
 */
#define	MDIO_WRITE(sc, reg, val)	do {	\
		bus_write_4(sc->sc_mem_res, (reg), (val)); \
	} while (0)

#define	MDIO_READ(sc, reg)	 bus_read_4(sc->sc_mem_res, (reg))

#define	MDIO_BARRIER_WRITE(sc)		bus_barrier((sc)->sc_mem_res,	\
	    0, (sc)->sc_mem_res_size, BUS_SPACE_BARRIER_WRITE)
#define	MDIO_BARRIER_READ(sc)		bus_barrier((sc)->sc_mem_res,	\
	    0, (sc)->sc_mem_res_size, BUS_SPACE_BARRIER_READ)
#define	MDIO_BARRIER_RW(sc)		bus_barrier((sc)->sc_mem_res,	\
	    0, (sc)->sc_mem_res_size,					\
	    BUS_SPACE_BARRIER_READ | BUS_SPACE_BARRIER_WRITE)

#define	MDIO_SET_BITS(sc, reg, bits)	\
	GPIO_WRITE(sc, reg,	MDIO_READ(sc, (reg)) | (bits))

#define	MDIO_CLEAR_BITS(sc, reg, bits)	\
	GPIO_WRITE(sc, reg,	MDIO_READ(sc, (reg)) & ~(bits))

struct qcom_mdio_ipq4018_softc {
	device_t		sc_dev;
	struct mtx		sc_mtx;
	struct resource		*sc_mem_res;
	size_t			sc_mem_res_size;
	int			sc_mem_rid;
	uint32_t		sc_debug;
};

#endif	/* __QCOM_MDIO_IPQ4018_VAR_H__ */
