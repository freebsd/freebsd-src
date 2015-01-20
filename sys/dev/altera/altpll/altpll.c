/*-
 * Copyright (c) 2012 Robert N. M. Watson
 * Copyright (c) 2015 Ed Maste
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
 * ("CTSRD"), as part of the DARPA CRASH research programme.
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/endian.h>
#include <sys/conf.h>
#include <sys/rman.h>
#include <sys/systm.h>
#include <sys/uio.h>

#include <machine/bus.h>
#include <machine/vm.h>

#include <dev/altera/altpll/altpll.h>

/*
 * Device driver for the Altera reconfigurable PLL.
 */

devclass_t	altpll_devclass;

static d_mmap_t altpll_reg_mmap;
static d_read_t altpll_reg_read;
static d_write_t altpll_reg_write;

static struct cdevsw altpll_reg_cdevsw = {
	.d_version =	D_VERSION,
	.d_mmap =	altpll_reg_mmap,
	.d_read =	altpll_reg_read,
	.d_write =	altpll_reg_write,
	.d_name =	"altpll",
};

/*
 * Calculate best multiplier and divisor for a specified frequency.
 */
static int
altpll_calc_params(struct altpll_softc *sc, uint64_t output_frequency,
    uint32_t *mulp, uint32_t *divp)
{
	int64_t e, besterr;
	uint32_t div, mul;

	if (output_frequency == 0)
		return (EINVAL);

	besterr = INT64_MAX;
	for (mul = 1; mul < 64; mul++) {
		div = (sc->ap_base_frequency * mul + (output_frequency / 2)) /
		    output_frequency;
		if (div == 0)
			continue;
		e = output_frequency - sc->ap_base_frequency * mul / div;
		if (e < 0)
			e = -e;
		if (e < besterr) {
			besterr = e;
			*mulp = mul;
			*divp = div;
		}
	}
	return (besterr < INT64_MAX ? 0 : EINVAL);
}

/*
 * Configure the counter_type registers for a given PLL parameter.
 */
static int
altpll_write_param(struct altpll_softc *sc, uint32_t param_offset, uint32_t val)
{
	int high_count, low_count;

	high_count = (val + 1) / 2;
	low_count = val - high_count;

	bus_write_4(sc->ap_reg_res, param_offset + ALTPLL_OFF_PARAM_HIGH_COUNT,
	    htole32(high_count));
	bus_write_4(sc->ap_reg_res, param_offset + ALTPLL_OFF_PARAM_LOW_COUNT,
	    htole32(low_count));
	bus_write_4(sc->ap_reg_res, param_offset + ALTPLL_OFF_PARAM_BYPASS,
	    htole32(val == 1 ? 1 : 0));
	bus_write_4(sc->ap_reg_res, param_offset + ALTPLL_OFF_PARAM_ODD_COUNT,
	    htole32(val & 0x1 ? 1 : 0));

	return (0);
}

/*
 * Configure all PLL counter parameters.
 */
static int
altpll_write_params(struct altpll_softc *sc, uint32_t mul, uint32_t div,
     uint32_t c0)
{
	uint32_t status;
	int retry;

	altpll_write_param(sc, ALTPLL_OFF_TYPE_N, div);
	altpll_write_param(sc, ALTPLL_OFF_TYPE_M, mul);
	altpll_write_param(sc, ALTPLL_OFF_TYPE_C0, c0);
	/*
	 * Program C1 with the same parameters as C0. It seems the PLL does not
	 * run correctly otherwise.
	 */
	altpll_write_param(sc, ALTPLL_OFF_TYPE_C1, c0);
	/* Trigger the transfer. */
	bus_write_4(sc->ap_reg_res, ALTPLL_OFF_TRANSFER, htole32(0xff));
	/* Wait for the transfer to complete. */
	status = bus_read_4(sc->ap_reg_res, ALTPLL_OFF_TRANSFER);
	for (retry = 0;
	    status != htole32(ALTPLL_TRANSFER_COMPLETE) && retry < 10; retry++)
		status = bus_read_4(sc->ap_reg_res, ALTPLL_OFF_TRANSFER);
	if (status != htole32(ALTPLL_TRANSFER_COMPLETE)) {
		device_printf(sc->ap_dev,
		    "timed out waiting for transfer to PLL\n");
		/* XXXEM ignore error for now - not set by old FPGA bitfiles. */
	}

	return (0);
}

/*
 * fdt_clock interface to set the frequency.
 */
int
altpll_set_frequency(device_t dev, uint64_t frequency)
{
	uint32_t mul, div;
	int error;
	struct altpll_softc *sc;

	sc = device_get_softc(dev);

	mul = div = 0; /* XXX Quiet GCC uninitialized warning */
	error = altpll_calc_params(sc, frequency, &mul, &div);
	if (error)
		return (error);
	error = altpll_write_params(sc, mul, div, 1);
	return (error);
}

/*
 * All I/O to/from the altpll register device must be 32-bit, and aligned
 * to 32-bit.
 */
static int
altpll_reg_read(struct cdev *dev, struct uio *uio, int flag)
{
	struct altpll_softc *sc;
	u_long offset, size;
	uint32_t v;
	int error;

	if (uio->uio_offset < 0 || uio->uio_offset % 4 != 0 ||
	    uio->uio_resid % 4 != 0)
		return (ENODEV);
	sc = dev->si_drv1;
	size = rman_get_size(sc->ap_reg_res);
	error = 0;
	if ((uio->uio_offset + uio->uio_resid < 0) ||
	    (uio->uio_offset + uio->uio_resid > size))
		return (ENODEV);
	while (uio->uio_resid > 0) {
		offset = uio->uio_offset;
		if (offset + sizeof(v) > size)
			return (ENODEV);
		v = bus_read_4(sc->ap_reg_res, offset);
		error = uiomove(&v, sizeof(v), uio);
		if (error)
			return (error);
	}
	return (error);
}

static int
altpll_reg_write(struct cdev *dev, struct uio *uio, int flag)
{
	struct altpll_softc *sc;
	u_long offset, size;
	uint32_t v;
	int error;

	if (uio->uio_offset < 0 || uio->uio_offset % 4 != 0 ||
	    uio->uio_resid % 4 != 0)
		return (ENODEV);
	sc = dev->si_drv1;
	size = rman_get_size(sc->ap_reg_res);
	error = 0;
	while (uio->uio_resid > 0) {
		offset = uio->uio_offset;
		if (offset + sizeof(v) > size)
			return (ENODEV);
		error = uiomove(&v, sizeof(v), uio);
		if (error)
			return (error);
		bus_write_4(sc->ap_reg_res, offset, v);
	}
	return (error);
}

static int
altpll_reg_mmap(struct cdev *dev, vm_ooffset_t offset, vm_paddr_t *paddr,
    int nprot, vm_memattr_t *memattr)
{
	struct altpll_softc *sc;
	int error;

	sc = dev->si_drv1;
	error = 0;
	if (trunc_page(offset) == offset &&
	    rman_get_size(sc->ap_reg_res) >= offset + PAGE_SIZE) {
		*paddr = rman_get_start(sc->ap_reg_res) + offset;
		*memattr = VM_MEMATTR_UNCACHEABLE;
	} else {
		error = ENODEV;
	}
	return (error);
}

int
altpll_attach(struct altpll_softc *sc)
{

	sc->ap_reg_cdev = make_dev(&altpll_reg_cdevsw, sc->ap_unit,
	    UID_ROOT, GID_WHEEL, 0400, "altpll%d", sc->ap_unit);
	if (sc->ap_reg_cdev == NULL) {
		device_printf(sc->ap_dev, "%s: make_dev failed\n", __func__);
		return (ENXIO);
	}
	/* XXXRW: Slight race between make_dev(9) and here. */
	sc->ap_reg_cdev->si_drv1 = sc;
	return (0);
}

void
altpll_detach(struct altpll_softc *sc)
{

	if (sc->ap_reg_cdev != NULL)
		destroy_dev(sc->ap_reg_cdev);
}
