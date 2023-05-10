/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2014 The FreeBSD Foundation
 *
 * This software was developed by Aleksandr Rybalko under sponsorship from the
 * FreeBSD Foundation.
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
#include <sys/linker.h>

#include "opt_platform.h"

#include <machine/metadata.h>
#include <machine/vmparam.h>
#include <vm/vm.h>
#include <vm/pmap.h>

#include <dev/vt/vt.h>
#include <dev/vt/hw/fb/vt_fb.h>
#include <dev/vt/colors/vt_termcolors.h>

static vd_init_t vt_vbefb_init;
static vd_fini_t vt_vbefb_fini;
static vd_probe_t vt_vbefb_probe;

static struct vt_driver vt_vbefb_driver = {
	.vd_name = "vbefb",
	.vd_probe = vt_vbefb_probe,
	.vd_init = vt_vbefb_init,
	.vd_fini = vt_vbefb_fini,
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
	/* Better than VGA, but still generic driver. */
	.vd_priority = VD_PRIORITY_GENERIC + 1,
};

static struct fb_info local_vbe_info;
VT_DRIVER_DECLARE(vt_vbefb, vt_vbefb_driver);

static int
vt_vbefb_probe(struct vt_device *vd)
{
	int		disabled;
	struct vbe_fb	*vbefb;
	caddr_t		kmdp;

	disabled = 0;
	TUNABLE_INT_FETCH("hw.syscons.disable", &disabled);
	if (disabled != 0)
		return (CN_DEAD);

	kmdp = preload_search_by_type("elf kernel");
	if (kmdp == NULL)
		kmdp = preload_search_by_type("elf64 kernel");
	vbefb = (struct vbe_fb *)preload_search_info(kmdp,
	    MODINFO_METADATA | MODINFOMD_VBE_FB);
	if (vbefb == NULL)
		return (CN_DEAD);

	return (CN_INTERNAL);
}

static int
vt_vbefb_init(struct vt_device *vd)
{
	struct fb_info	*info;
	struct vbe_fb	*vbefb;
	caddr_t		kmdp;
	int		format, roff, goff, boff;

	info = vd->vd_softc;
	if (info == NULL)
		info = vd->vd_softc = (void *)&local_vbe_info;

	kmdp = preload_search_by_type("elf kernel");
	if (kmdp == NULL)
		kmdp = preload_search_by_type("elf64 kernel");
	vbefb = (struct vbe_fb *)preload_search_info(kmdp,
	    MODINFO_METADATA | MODINFOMD_VBE_FB);
	if (vbefb == NULL)
		return (CN_DEAD);

	info->fb_height = vbefb->fb_height;
	info->fb_width = vbefb->fb_width;

	info->fb_depth = vbefb->fb_bpp;
	/* Round to a multiple of the bits in a byte. */
	info->fb_bpp = roundup2(vbefb->fb_bpp, NBBY);

	/* Stride in bytes, not pixels */
	info->fb_stride = vbefb->fb_stride * (info->fb_bpp / NBBY);

	if (info->fb_depth == 8)
		format = COLOR_FORMAT_VGA;
	else
		format = COLOR_FORMAT_RGB;

	roff = ffs(vbefb->fb_mask_red) - 1;
	goff = ffs(vbefb->fb_mask_green) - 1;
	boff = ffs(vbefb->fb_mask_blue) - 1;
	vt_config_cons_colors(info, format,
	    vbefb->fb_mask_red >> roff, roff,
	    vbefb->fb_mask_green >> goff, goff,
	    vbefb->fb_mask_blue >> boff, boff);

	/* Mark cmap initialized. */
	info->fb_cmsize = NCOLORS;

	info->fb_size = info->fb_height * info->fb_stride;
	info->fb_pbase = vbefb->fb_addr;
	info->fb_vbase = (intptr_t)pmap_mapdev_attr(info->fb_pbase,
	    info->fb_size, VM_MEMATTR_WRITE_COMBINING);

	vt_fb_init(vd);

	return (CN_INTERNAL);
}

static void
vt_vbefb_fini(struct vt_device *vd, void *softc)
{
	struct fb_info	*info = softc;

	vt_fb_fini(vd, softc);
	pmap_unmapdev((void *)info->fb_vbase, info->fb_size);
}
