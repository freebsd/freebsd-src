/*-
 * Copyright (c) 2011 Nathan Whitehorn
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: user/ed/newcons/sys/dev/vt/hw/ofwfb/ofwfb.c 219888 2011-03-22 21:31:31Z ed $");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>

#include <dev/vt/vt.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/bus.h>
#ifdef __sparc64__
#include <machine/bus_private.h>
#endif

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_pci.h>

struct ofwfb_softc {
	phandle_t	sc_node;

	intptr_t	sc_addr;
	int		sc_depth;
	int		sc_stride;

#ifdef __sparc64__
	bus_space_tag_t	sc_memt;
#endif
};

static vd_init_t	ofwfb_init;
static vd_blank_t	ofwfb_blank;
static vd_bitblt_t	ofwfb_bitblt;

static const struct vt_driver vt_ofwfb_driver = {
	.vd_init	= ofwfb_init,
	.vd_blank	= ofwfb_blank,
	.vd_bitblt	= ofwfb_bitblt,
	.vd_priority	= VD_PRIORITY_GENERIC,
};

static struct ofwfb_softc ofwfb_conssoftc;
VT_CONSDEV_DECLARE(vt_ofwfb_driver, PIXEL_WIDTH(1920), PIXEL_HEIGHT(1200),
    &ofwfb_conssoftc);
/* XXX: hardcoded max size */

static const uint32_t colormap[] = {
	0x00000000,	/* Black */
	0x00ff0000,	/* Red */
	0x0000ff00,	/* Green */
	0x00c0c000,	/* Brown */
	0x000000ff,	/* Blue */
	0x00c000c0,	/* Magenta */
	0x0000c0c0,	/* Cyan */
	0x00c0c0c0,	/* Light grey */
	0x00808080,	/* Dark grey */
	0x00ff8080,	/* Light red */
	0x0080ff80,	/* Light green */
	0x00ffff80,	/* Yellow */
	0x008080ff,	/* Light blue */
	0x00ff80ff,	/* Light magenta */
	0x0080ffff,	/* Light cyan */
	0x00ffffff, 	/* White */
};

static void
ofwfb_blank(struct vt_device *vd, term_color_t color)
{
	struct ofwfb_softc *sc = vd->vd_softc;
	u_int ofs;
	uint32_t c;

	switch (sc->sc_depth) {
	case 8:
		for (ofs = 0; ofs < sc->sc_stride*vd->vd_height; ofs++)
			*(uint8_t *)(sc->sc_addr + ofs) = color;
		break;
	case 32:
		c = colormap[color];
		for (ofs = 0; ofs < sc->sc_stride*vd->vd_height; ofs++)
			*(uint32_t *)(sc->sc_addr + 4*ofs) = c;
		break;
	default:
		/* panic? */
		break;
	}
}

static void
ofwfb_bitblt(struct vt_device *vd, const uint8_t *src,
    vt_axis_t top, vt_axis_t left, unsigned int width, unsigned int height,
    term_color_t fg, term_color_t bg)
{
	struct ofwfb_softc *sc = vd->vd_softc;
	u_long line;
	uint32_t fgc, bgc;
	int c;
	uint8_t b = 0;

	fgc = colormap[fg];
	bgc = colormap[bg];

	line = (sc->sc_stride * top) + left * sc->sc_depth/8;
	for (; height > 0; height--) {
		line += sc->sc_stride;
		for (c = 0; c < width; c++) {
			if (c % 8 == 0)
				b = *src++;
			else
				b <<= 1;
			switch(sc->sc_depth) {
			case 8:
				*(uint8_t *)(sc->sc_addr + line + c) =
				    b & 0x80 ? fg : bg;
				break;
			case 32:
				*(uint32_t *)(sc->sc_addr + line + 4*c) = 
				    (b & 0x80) ? fgc : bgc;
				break;
			default:
				/* panic? */
				break;
			}
		}
	}
}

static void
ofwfb_initialize(struct vt_device *vd)
{
	struct ofwfb_softc *sc = vd->vd_softc;
	char name[64];
	ihandle_t ih;
	int i;
	cell_t retval;

	/* Open display device, thereby initializing it */
	memset(name, 0, sizeof(name));
	OF_package_to_path(sc->sc_node, name, sizeof(name));
	ih = OF_open(name);

	if (sc->sc_depth == 8) {
		/*
		 * Install the color map
		 */
		for (i = 0; i < 16; i++) {
			OF_call_method("color!", ih, 4, 1,
			    (cell_t)((colormap[i] >> 16) & 0xff),
			    (cell_t)((colormap[i] >> 8) & 0xff),
			    (cell_t)((colormap[i] >> 0) & 0xff),
			    (cell_t)i, &retval);
		}
        }

	/* Clear the screen. */
	ofwfb_blank(vd, TC_BLACK);
}

static int
ofwfb_init(struct vt_device *vd)
{
	struct ofwfb_softc *sc = vd->vd_softc;
	char type[64];
	phandle_t chosen;
	ihandle_t stdout;
	phandle_t node;
	uint32_t depth, height, width;
	uint32_t fb_phys;
#ifdef __sparc64__
	static struct bus_space_tag ofwfb_memt[1];
	bus_addr_t phys;
	int space;
#endif

	chosen = OF_finddevice("/chosen");
	OF_getprop(chosen, "stdout", &stdout, sizeof(stdout));
	node = OF_instance_to_package(stdout);
	if (node == -1) {
		/*
		 * The "/chosen/stdout" does not exist try
		 * using "screen" directly.
		 */
		node = OF_finddevice("screen");
	}
	OF_getprop(node, "device_type", type, sizeof(type));
	if (strcmp(type, "display") != 0)
		return (CN_DEAD);

	/* Keep track of the OF node */
	sc->sc_node = node;

	/* Only support 8 and 32-bit framebuffers */
	OF_getprop(node, "depth", &depth, sizeof(depth));
	sc->sc_depth = depth;

	OF_getprop(node, "height", &height, sizeof(height));
	OF_getprop(node, "width", &width, sizeof(width));
	OF_getprop(node, "linebytes", &sc->sc_stride, sizeof(sc->sc_stride));

	vd->vd_height = height;
	vd->vd_width = width;

	/*
	 * Grab the physical address of the framebuffer, and then map it
	 * into our memory space. If the MMU is not yet up, it will be
	 * remapped for us when relocation turns on.
	 */

	 /* XXX We assume #address-cells is 1 at this point. */
	OF_getprop(node, "address", &fb_phys, sizeof(fb_phys));

#if defined(__powerpc__)
	bus_space_map(&bs_be_tag, fb_phys, height * sc->sc_stride,
	    BUS_SPACE_MAP_PREFETCHABLE, &sc->sc_addr);
#elif defined(__sparc64__)
	OF_decode_addr(node, 0, &space, &phys);
	sc->sc_memt = &ofwfb_memt[0];
	sc->sc_addr = sparc64_fake_bustag(space, fb_phys, sc->sc_memt);
#else
	#error Unsupported platform!
#endif

	ofwfb_initialize(vd);

	return (CN_INTERNAL);
}

