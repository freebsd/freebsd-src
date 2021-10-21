/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2021, Adrian Chadd <adrian@FreeBSD.org>
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

/* Driver for Qualcomm MSM entropy device. */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/sglist.h>
#include <sys/random.h>
#include <sys/stdatomic.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/bus.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/random/randomdev.h>
#include <dev/random/random_harvestq.h>

#include <dev/qcom_rnd/qcom_rnd_reg.h>

struct qcom_rnd_softc {
	device_t	 dev;
	int		reg_rid;
	struct resource	*reg;
};

static int	qcom_rnd_modevent(module_t, int, void *);

static int	qcom_rnd_probe(device_t);
static int	qcom_rnd_attach(device_t);
static int	qcom_rnd_detach(device_t);

static int	qcom_rnd_harvest(struct qcom_rnd_softc *, void *, size_t *);
static unsigned	qcom_rnd_read(void *, unsigned);

static struct random_source random_qcom_rnd = {
	.rs_ident = "Qualcomm Entropy Adapter",
	.rs_source = RANDOM_PURE_QUALCOMM,
	.rs_read = qcom_rnd_read,
};

/* Kludge for API limitations of random(4). */
static _Atomic(struct qcom_rnd_softc *) g_qcom_rnd_softc;

static int
qcom_rnd_modevent(module_t mod, int type, void *unused)
{
	int error;

	switch (type) {
	case MOD_LOAD:
	case MOD_QUIESCE:
	case MOD_UNLOAD:
	case MOD_SHUTDOWN:
		error = 0;
		break;
	default:
		error = EOPNOTSUPP;
		break;
	}

	return (error);
}

static int
qcom_rnd_probe(device_t dev)
{
	if (! ofw_bus_status_okay(dev)) {
		return (ENXIO);
	}

	if (ofw_bus_is_compatible(dev, "qcom,prng") == 0) {
		return (ENXIO);
	}

	return (0);
}

static int
qcom_rnd_attach(device_t dev)
{
	struct qcom_rnd_softc *sc, *exp;
	uint32_t reg;

	sc = device_get_softc(dev);


	/* Found a compatible device! */

	sc->dev = dev;

	exp = NULL;
	if (!atomic_compare_exchange_strong_explicit(&g_qcom_rnd_softc, &exp,
	    sc, memory_order_release, memory_order_acquire)) {
		return (ENXIO);
	}

	sc->reg_rid = 0;
	sc->reg = bus_alloc_resource_anywhere(dev, SYS_RES_MEMORY,
	    &sc->reg_rid, 0x140, RF_ACTIVE);
	if (sc->reg == NULL) {
		device_printf(dev, "Couldn't allocate memory resource!\n");
		return (ENXIO);
	}

	device_set_desc(dev, "Qualcomm PRNG");

	/*
	 * Check to see whether the PRNG has already been setup or not.
	 */
	bus_barrier(sc->reg, 0, 0x120, BUS_SPACE_BARRIER_READ);
	reg = bus_read_4(sc->reg, QCOM_RND_PRNG_CONFIG);
	if (reg & QCOM_RND_PRNG_CONFIG_HW_ENABLE) {
		device_printf(dev, "PRNG HW already enabled\n");
	} else {
		/*
		 * Do PRNG setup and then enable it.
		 */
		reg = bus_read_4(sc->reg, QCOM_RND_PRNG_LFSR_CFG);
		reg &= QCOM_RND_PRNG_LFSR_CFG_MASK;
		reg |= QCOM_RND_PRNG_LFSR_CFG_CLOCKS;
		bus_write_4(sc->reg, QCOM_RND_PRNG_LFSR_CFG, reg);
		bus_barrier(sc->reg, 0, 0x120, BUS_SPACE_BARRIER_WRITE);

		reg = bus_read_4(sc->reg, QCOM_RND_PRNG_CONFIG);
		reg |= QCOM_RND_PRNG_CONFIG_HW_ENABLE;
		bus_write_4(sc->reg, QCOM_RND_PRNG_CONFIG, reg);
		bus_barrier(sc->reg, 0, 0x120, BUS_SPACE_BARRIER_WRITE);
	}

	random_source_register(&random_qcom_rnd);
	return (0);

}

static int
qcom_rnd_detach(device_t dev)
{
	struct qcom_rnd_softc *sc;

	sc = device_get_softc(dev);
	KASSERT(
	    atomic_load_explicit(&g_qcom_rnd_softc, memory_order_acquire) == sc,
	    ("only one global instance at a time"));

	random_source_deregister(&random_qcom_rnd);
	if (sc->reg != NULL) {
		bus_release_resource(sc->dev, SYS_RES_MEMORY, sc->reg_rid, sc->reg);
	}
	atomic_store_explicit(&g_qcom_rnd_softc, NULL, memory_order_release);
	return (0);
}

static int
qcom_rnd_harvest(struct qcom_rnd_softc *sc, void *buf, size_t *sz)
{
	/*
	 * Add data to buf until we either run out of entropy or we
	 * fill the buffer.
	 *
	 * Note - be mindful of the provided buffer size; we're reading
	 * 4 bytes at a time but we only want to supply up to the max
	 * buffer size, so don't write past it!
	 */
	size_t rz = 0;
	uint32_t reg;

	while (rz < *sz) {
		bus_barrier(sc->reg, 0, 0x120, BUS_SPACE_BARRIER_READ);
		reg = bus_read_4(sc->reg, QCOM_RND_PRNG_STATUS);
		if ((reg & QCOM_RND_PRNG_STATUS_DATA_AVAIL) == 0)
			break;
		reg = bus_read_4(sc->reg, QCOM_RND_PRNG_DATA_OUT);
		memcpy(((char *) buf) + rz, &reg, sizeof(uint32_t));
		rz += sizeof(uint32_t);
	}

	if (rz == 0)
		return (EAGAIN);
	*sz = rz;
	return (0);
}

static unsigned
qcom_rnd_read(void *buf, unsigned usz)
{
	struct qcom_rnd_softc *sc;
	size_t sz;
	int error;

	sc = g_qcom_rnd_softc;
	if (sc == NULL)
		return (0);

	sz = usz;
	error = qcom_rnd_harvest(sc, buf, &sz);
	if (error != 0)
		return (0);

	return (sz);
}

static device_method_t qcom_rnd_methods[] = {
	/* Device methods. */
	DEVMETHOD(device_probe,		qcom_rnd_probe),
	DEVMETHOD(device_attach,	qcom_rnd_attach),
	DEVMETHOD(device_detach,	qcom_rnd_detach),

	DEVMETHOD_END
};

static driver_t qcom_rnd_driver = {
	"qcom_rnd",
	qcom_rnd_methods,
	sizeof(struct qcom_rnd_softc)
};
static devclass_t qcom_rnd_devclass;

DRIVER_MODULE(qcom_rnd_random, simplebus, qcom_rnd_driver, qcom_rnd_devclass,
    qcom_rnd_modevent, 0);
DRIVER_MODULE(qcom_rnd_random, ofwbus, qcom_rnd_driver, qcom_rnd_devclass,
    qcom_rnd_modevent, 0);
MODULE_DEPEND(qcom_rnd_random, random_device, 1, 1, 1);
MODULE_VERSION(qcom_rnd_random, 1);
