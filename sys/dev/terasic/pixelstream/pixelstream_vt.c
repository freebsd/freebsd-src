/*-
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
#include <sys/endian.h>
#include <sys/fbio.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/rman.h>

#include <machine/bus.h>
#include <sys/bus_dma.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/terasic/pixelstream/pixelstream.h>

#include <dev/vt/colors/vt_termcolors.h>

/*
 * Pixelstream vt(4) framebuffer driver.
 */

MALLOC_DEFINE(M_PIXELSTREAM, "pixelstream", "Pixelstream frame buffer");

static void
pixelstream_fbd_dmamap_callback(void *arg, bus_dma_segment_t *seg, int nseg,
    int error)
{
	*(bus_addr_t *)arg = seg[0].ds_addr;
}

static int
pixelstream_fbd_fballoc(struct pixelstream_softc *sc)
{
	struct fb_info *info;
	void *addr;
	bus_addr_t physaddr;
	unsigned long size;

	info = &sc->ps_fb_info;
	size = info->fb_height * info->fb_stride;

	if (bus_dma_tag_create(bus_get_dma_tag(sc->ps_dev), PAGE_SIZE, 0,
	    BUS_SPACE_MAXADDR_32BIT, BUS_SPACE_MAXADDR, NULL, NULL,
	    size, 1, BUS_SPACE_MAXSIZE_32BIT, 0, NULL, NULL, &sc->ps_fb_dmat)) {
		device_printf(sc->ps_dev,
		    "Failed to allocate pixelstream DMA tag\n");
		return (ENOMEM);
	}
	if (bus_dmamem_alloc(sc->ps_fb_dmat, &addr,
	    BUS_DMA_WAITOK | BUS_DMA_ZERO, &sc->ps_fb_dmamap)) {
		device_printf(sc->ps_dev,
		    "Failed to allocate pixelstream framebuffer\n");
		return (ENOMEM);
	}
	bus_dmamap_load(sc->ps_fb_dmat, sc->ps_fb_dmamap, addr,
             size, pixelstream_fbd_dmamap_callback, &physaddr, 0);

	info->fb_pbase = physaddr;
	info->fb_size = size;
	info->fb_vbase = (intptr_t)addr;

	bus_write_4(sc->ps_reg_res, PIXELSTREAM_OFF_BASE_ADDR_LOWER,
	    htole32(physaddr & 0xffffffff));

	device_printf(sc->ps_dev,
	    "allocated pixelstream framebuffer at phys=0x%llx virt=%p "
            "size=0x%lx\n", (unsigned long long)physaddr, addr, size);

	return (0);
}

static int
pixelstream_fbd_get_params(struct pixelstream_softc *sc)
{
	uint32_t height, width;

	width = le32toh(bus_read_4(sc->ps_reg_res,
	    PIXELSTREAM_OFF_X_RESOLUTION));
	height = le32toh(bus_read_4(sc->ps_reg_res,
	    PIXELSTREAM_OFF_Y_RESOLUTION));
	if (width == 0 || width > 0xffff || height == 0 || height > 0xffff) {
		device_printf(sc->ps_dev,
		    "ignoring invalid resolution %u x %u\n", width, height);
		return (EINVAL);
	}
	sc->ps_fb_info.fb_width = width;
	sc->ps_fb_info.fb_height = height;
	return (0);
}

int
pixelstream_fbd_attach(struct pixelstream_softc *sc)
{
	struct fb_info *info;
	device_t fbd;

	info = &sc->ps_fb_info;

	/*
	 * Determine framebuffer size and allocate it.
	 */
	info->fb_name = device_get_nameunit(sc->ps_dev);
	info->fb_bpp = info->fb_depth = 32;
	if (pixelstream_fbd_get_params(sc) != 0) {
		info->fb_width = 800;
		info->fb_height = 600;
	}
	device_printf(sc->ps_dev, "resolution is %u x %u\n", info->fb_width,
	    info->fb_height);
	info->fb_stride = info->fb_width * (info->fb_depth / 8);

	if (pixelstream_fbd_fballoc(sc) != 0)
		return (ENXIO);

	/*
	 * Connect vt(4) framebuffer device.
	 */
	fbd = device_add_child(sc->ps_dev, "fbd", device_get_unit(sc->ps_dev));
	if (fbd == NULL) {
		device_printf(sc->ps_dev, "Failed to attach fbd child\n");
		return (ENXIO);
	}
	if (device_probe_and_attach(fbd) != 0) {
		device_printf(sc->ps_dev, "Failed to attach fbd device\n");
		return (ENXIO);
	}
	return (0);
}

void
pixelstream_fbd_detach(struct pixelstream_softc *sc)
{
	panic("%s: detach not implemented", __func__);
}

extern device_t fbd_driver;
extern devclass_t fbd_devclass;
DRIVER_MODULE(fbd, pixelstream, fbd_driver, fbd_devclass, 0, 0);
