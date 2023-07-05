/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2014 The FreeBSD Foundation
 * Copyright (c) 2021 Andrew Turner
 *
 * Portions of this software was developed by Aleksandr Rybalko under
 * sponsorship from the FreeBSD Foundation.
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
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/fbio.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <dev/vt/vt.h>
#include <dev/vt/hw/fb/vt_fb.h>
#include <dev/vt/colors/vt_termcolors.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_subr.h>
#include <dev/ofw/ofw_bus_subr.h>

static vd_init_t vt_simplefb_init;
static vd_fini_t vt_simplefb_fini;
static vd_probe_t vt_simplefb_probe;

static struct vt_driver vt_simplefb_driver = {
	.vd_name = "simplefb",
	.vd_probe = vt_simplefb_probe,
	.vd_init = vt_simplefb_init,
	.vd_fini = vt_simplefb_fini,
	.vd_blank = vt_fb_blank,
	.vd_bitblt_text = vt_fb_bitblt_text,
	.vd_invalidate_text = vt_fb_invalidate_text,
	.vd_bitblt_bmp = vt_fb_bitblt_bitmap,
	.vd_drawrect = vt_fb_drawrect,
	.vd_setpixel = vt_fb_setpixel,
	.vd_fb_ioctl = vt_fb_ioctl,
	.vd_fb_mmap = vt_fb_mmap,
	.vd_suspend = vt_suspend,
	.vd_resume = vt_resume,
	/* Better than efifb, but still generic driver. */
	.vd_priority = VD_PRIORITY_GENERIC + 2,
};

struct {
	const char *name;
	int rbits, rshift;
	int gbits, gshift;
	int bbits, bshift;
	int depth;
	enum vt_color_format format;
} simplefb_formats[] = {
	{
		.name = "r5g6b5",
		.rbits = 5, .rshift = 11,
		.gbits = 6, .gshift = 5,
		.bbits = 5, .bshift = 0,
		.depth = 16, .format = COLOR_FORMAT_RGB,
	},
	{
		.name = "r8g8b8",
		.rbits = 8, .rshift = 16,
		.gbits = 8, .gshift = 8,
		.bbits = 8, .bshift = 0,
		.depth = 24, .format = COLOR_FORMAT_RGB,
	},
	{
		.name = "a8r8g8b8",
		.rbits = 8, .rshift = 16,
		.gbits = 8, .gshift = 8,
		.bbits = 8, .bshift = 0,
		.depth = 32, .format = COLOR_FORMAT_RGB,
	},
	{
		.name = "x8r8g8b8",
		.rbits = 8, .rshift = 16,
		.gbits = 8, .gshift = 8,
		.bbits = 8, .bshift = 0,
		.depth = 32, .format = COLOR_FORMAT_RGB,
	},
	{
		.name = "x2r10g10b10",
		.rbits = 10, .rshift = 20,
		.gbits = 10, .gshift = 10,
		.bbits = 10, .bshift = 0,
		.depth = 32, .format = COLOR_FORMAT_RGB,
	},
};

static struct fb_info local_info;
VT_DRIVER_DECLARE(vt_simplefb, vt_simplefb_driver);

static bool
vt_simplefb_node(phandle_t *nodep)
{
	phandle_t chosen, node;

	chosen = OF_finddevice("/chosen");
	if (chosen == -1)
		return (false);

	for (node = OF_child(chosen); node != 0; node = OF_peer(node)) {
		if (ofw_bus_node_is_compatible(node, "simple-framebuffer"))
			break;
	}
	if (node == 0)
		return (false);

	if (nodep != NULL)
		*nodep = node;

	return (true);
}

static int
vt_simplefb_probe(struct vt_device *vd)
{
	int disabled;

	disabled = 0;
	TUNABLE_INT_FETCH("hw.syscons.disable", &disabled);
	if (disabled != 0)
		return (CN_DEAD);

	if (!vt_simplefb_node(NULL))
		return (CN_DEAD);

	return (CN_INTERNAL);
}

static int
vt_simplefb_init(struct vt_device *vd)
{
	char format[16];
	pcell_t height, width, stride;
	struct fb_info *sc;
	phandle_t node;
	bus_size_t size;
	int error;

	/* Initialize softc */
	vd->vd_softc = sc = &local_info;

	if (!vt_simplefb_node(&node))
		return (CN_DEAD);

	if (OF_getencprop(node, "height", &height, sizeof(height)) == -1 ||
	    OF_getencprop(node, "width", &width, sizeof(width)) == -1 ||
	    OF_getencprop(node, "stride", &stride, sizeof(stride)) == -1 ||
	    OF_getprop(node, "format", format, sizeof(format)) == -1) {
		return (CN_DEAD);
	}

	sc->fb_height = height;
	sc->fb_width = width;
	sc->fb_stride = stride;
	sc->fb_cmsize = NCOLORS;

	error = 1;
	for (int i = 0; i < nitems(simplefb_formats); i++) {
		if (strcmp(format, simplefb_formats[i].name) == 0) {
			vt_config_cons_colors(sc,
			    simplefb_formats[i].format,
			    (1 << simplefb_formats[i].rbits) - 1,
			    simplefb_formats[i].rshift,
			    (1 << simplefb_formats[i].gbits) - 1,
			    simplefb_formats[i].gshift,
			    (1 << simplefb_formats[i].bbits) - 1,
			    simplefb_formats[i].bshift);
			sc->fb_depth = sc->fb_bpp = simplefb_formats[i].depth;
			error = 0;
			break;
		}
	}
	if (error != 0)
		return (CN_DEAD);

	ofw_reg_to_paddr(node, 0, &sc->fb_pbase, &size, NULL);
	sc->fb_vbase = (intptr_t)pmap_mapdev_attr(sc->fb_pbase,
	    size, VM_MEMATTR_WRITE_COMBINING);
	sc->fb_size = size;

	vt_fb_init(vd);

	return (CN_INTERNAL);
}

static void
vt_simplefb_fini(struct vt_device *vd, void *softc)
{
	struct fb_info *sc;

	sc = softc;
	vt_fb_fini(vd, softc);
	pmap_unmapdev((void *)sc->fb_vbase, sc->fb_size);
}
