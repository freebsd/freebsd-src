/*-
 * Copyright (c) 2012 Robert N. M. Watson
 * Copyright (c) 2014 Ed Maste
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
#include <sys/conf.h>
#include <sys/fbio.h>
#include <sys/rman.h>
#include <sys/uio.h>

#include <machine/bus.h>
#include <machine/vm.h>

#include <dev/terasic/pixelstream/pixelstream.h>

static d_mmap_t pixelstream_reg_mmap;
static d_read_t pixelstream_reg_read;
static d_write_t pixelstream_reg_write;

static struct cdevsw pixelstream_reg_cdevsw = {
	.d_version =	D_VERSION,
	.d_mmap =	pixelstream_reg_mmap,
	.d_read =	pixelstream_reg_read,
	.d_write =	pixelstream_reg_write,
	.d_name =	"pixelstream_reg",
};

/*
 * All I/O to/from the pixelstream register device must be 32-bit, and aligned
 * to 32-bit.
 */
static int
pixelstream_reg_read(struct cdev *dev, struct uio *uio, int flag)
{
	struct pixelstream_softc *sc;
	u_long offset, size;
	uint32_t v;
	int error;

	if (uio->uio_offset < 0 || uio->uio_offset % 4 != 0 ||
	    uio->uio_resid % 4 != 0)
		return (ENODEV);
	sc = dev->si_drv1;
	size = rman_get_size(sc->ps_reg_res);
	error = 0;
	if ((uio->uio_offset + uio->uio_resid < 0) ||
	    (uio->uio_offset + uio->uio_resid > size))
		return (ENODEV);
	while (uio->uio_resid > 0) {
		offset = uio->uio_offset;
		if (offset + sizeof(v) > size)
			return (ENODEV);
		v = bus_read_4(sc->ps_reg_res, offset);
		error = uiomove(&v, sizeof(v), uio);
		if (error)
			return (error);
	}
	return (error);
}

static int
pixelstream_reg_write(struct cdev *dev, struct uio *uio, int flag)
{
	struct pixelstream_softc *sc;
	u_long offset, size;
	uint32_t v;
	int error;

	if (uio->uio_offset < 0 || uio->uio_offset % 4 != 0 ||
	    uio->uio_resid % 4 != 0)
		return (ENODEV);
	sc = dev->si_drv1;
	size = rman_get_size(sc->ps_reg_res);
	error = 0;
	while (uio->uio_resid > 0) {
		offset = uio->uio_offset;
		if (offset + sizeof(v) > size)
			return (ENODEV);
		error = uiomove(&v, sizeof(v), uio);
		if (error)
			return (error);
		bus_write_4(sc->ps_reg_res, offset, v);
	}
	return (error);
}

static int
pixelstream_reg_mmap(struct cdev *dev, vm_ooffset_t offset, vm_paddr_t *paddr,
    int nprot, vm_memattr_t *memattr)
{
	struct pixelstream_softc *sc;
	int error;

	sc = dev->si_drv1;
	error = 0;
	if (trunc_page(offset) == offset &&
	    rman_get_size(sc->ps_reg_res) >= offset + PAGE_SIZE) {
		*paddr = rman_get_start(sc->ps_reg_res) + offset;
		*memattr = VM_MEMATTR_UNCACHEABLE;
	} else {
		error = ENODEV;
	}
	return (error);
}

int
pixelstream_reg_attach(struct pixelstream_softc *sc)
{
	sc->ps_reg_cdev = make_dev(&pixelstream_reg_cdevsw, sc->ps_unit,
	    UID_ROOT, GID_WHEEL, 0400, "ps_reg%d", sc->ps_unit);
	if (sc->ps_reg_cdev == NULL) {
		device_printf(sc->ps_dev, "%s: make_dev failed\n", __func__);
		return (ENXIO);
	}
	/* XXXRW: Slight race between make_dev(9) and here. */
	sc->ps_reg_cdev->si_drv1 = sc;
	return (0);
}

void
pixelstream_reg_detach(struct pixelstream_softc *sc)
{
	if (sc->ps_reg_cdev != NULL)
		destroy_dev(sc->ps_reg_cdev);
}
