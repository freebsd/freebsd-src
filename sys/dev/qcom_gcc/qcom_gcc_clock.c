/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025, Adrian Chadd <adrian@FreeBSD.org>
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

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/sglist.h>
#include <sys/random.h>
#include <sys/stdatomic.h>
#include <sys/mutex.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/bus.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include "qcom_gcc_var.h"

int
qcom_gcc_clock_read(device_t dev, bus_addr_t addr, uint32_t *val)
{
	struct qcom_gcc_softc *sc;

	sc = device_get_softc(dev);
	*val = bus_read_4(sc->reg, addr);
	return (0);
}

int
qcom_gcc_clock_write(device_t dev, bus_addr_t addr, uint32_t val)
{
	struct qcom_gcc_softc *sc;

	sc = device_get_softc(dev);
	bus_write_4(sc->reg, addr, val);
	return (0);
}

int
qcom_gcc_clock_modify(device_t dev, bus_addr_t addr,
     uint32_t clear_mask, uint32_t set_mask)
{
	struct qcom_gcc_softc *sc;
	uint32_t reg;

	sc = device_get_softc(dev);
	reg = bus_read_4(sc->reg, addr);
	reg &= clear_mask;
	reg |= set_mask;
	bus_write_4(sc->reg, addr, reg);
	return (0);
}

void
qcom_gcc_clock_lock(device_t dev)
{
	struct qcom_gcc_softc *sc;

	sc = device_get_softc(dev);
	mtx_lock(&sc->mtx);
}

void
qcom_gcc_clock_unlock(device_t dev)
{
	struct qcom_gcc_softc *sc;

	sc = device_get_softc(dev);
	mtx_unlock(&sc->mtx);
}
