/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2019 Michal Meloun <mmel@FreeBSD.org>
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
 *
 * $FreeBSD$
 */

#ifndef _ACT8846_H_
#define _ACT8846_H_

#include <dev/iicbus/pmic/act8846_reg.h>

struct act8846_reg_sc;
struct act8846_gpio_pin;

struct act8846_softc {
	device_t		dev;
	struct sx		lock;
	int			bus_addr;

	/* Regulators. */
	struct act8846_reg_sc	**regs;
	int			nregs;
};

#define	RD1(sc, reg, val)	act8846_read(sc, reg, val)
#define	WR1(sc, reg, val)	act8846_write(sc, reg, val)
#define	RM1(sc, reg, clr, set)	act8846_modify(sc, reg, clr, set)

int act8846_read(struct act8846_softc *sc, uint8_t reg, uint8_t *val);
int act8846_write(struct act8846_softc *sc, uint8_t reg, uint8_t val);
int act8846_modify(struct act8846_softc *sc, uint8_t reg, uint8_t clear,
    uint8_t set);
int act8846_read_buf(struct act8846_softc *sc, uint8_t reg, uint8_t *buf,
    size_t size);
int act8846_write_buf(struct act8846_softc *sc, uint8_t reg, uint8_t *buf,
    size_t size);

/* Regulators */
int act8846_regulator_attach(struct act8846_softc *sc, phandle_t node);
int act8846_regulator_map(device_t dev, phandle_t xref, int ncells,
    pcell_t *cells, int *num);

#endif /* _ACT8846_H_ */
