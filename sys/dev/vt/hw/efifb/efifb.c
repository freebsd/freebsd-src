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

static vd_init_t vt_efifb_init;
static vd_fini_t vt_efifb_fini;
static vd_probe_t vt_efifb_probe;

static struct vt_driver vt_efifb_driver = {
	.vd_name = "efifb",
	.vd_probe = vt_efifb_probe,
	.vd_init = vt_efifb_init,
	.vd_fini = vt_efifb_fini,
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

static struct fb_info local_info;
VT_DRIVER_DECLARE(vt_efifb, vt_efifb_driver);

static int
vt_efifb_probe(struct vt_device *vd)
{
	int		disabled;
	struct efi_fb	*efifb;
	caddr_t		kmdp;

	disabled = 0;
	TUNABLE_INT_FETCH("hw.syscons.disable", &disabled);
	if (disabled != 0)
		return (CN_DEAD);

	kmdp = preload_search_by_type("elf kernel");
	if (kmdp == NULL)
		kmdp = preload_search_by_type("elf64 kernel");
	efifb = (struct efi_fb *)preload_search_info(kmdp,
	    MODINFO_METADATA | MODINFOMD_EFI_FB);
	if (efifb == NULL)
		return (CN_DEAD);

	return (CN_INTERNAL);
}

static int
vt_efifb_init(struct vt_device *vd)
{
	struct fb_info	*info;
	struct efi_fb	*efifb;
	caddr_t		kmdp;
	int		memattr;
	int		roff, goff, boff;
	char		attr[16];

	/*
	 * XXX TODO: I think there's more nuance here than we're acknowledging,
	 * and we should look into it.  It may be that the framebuffer lives in
	 * a segment of memory that doesn't support one or both of these.  We
	 * should likely be consulting the memory map for any applicable
	 * cacheability attributes before making a final decision.
	 */
	memattr = VM_MEMATTR_WRITE_COMBINING;
	if (TUNABLE_STR_FETCH("hw.efifb.cache_attr", attr, sizeof(attr))) {
		/*
		 * We'll allow WC but it's currently the default, UC is the only
		 * other tested one at this time.
		 */
		if (strcasecmp(attr, "wc") != 0 &&
		    strcasecmp(attr, "uc") != 0) {
			printf("efifb: unsupported cache attr specified: %s\n",
			    attr);
			printf("efifb: expected \"wc\" or \"uc\"\n");
		} else if (strcasecmp(attr, "uc") == 0) {
			memattr = VM_MEMATTR_UNCACHEABLE;
		}
	}

	info = vd->vd_softc;
	if (info == NULL)
		info = vd->vd_softc = (void *)&local_info;

	kmdp = preload_search_by_type("elf kernel");
	if (kmdp == NULL)
		kmdp = preload_search_by_type("elf64 kernel");
	efifb = (struct efi_fb *)preload_search_info(kmdp,
	    MODINFO_METADATA | MODINFOMD_EFI_FB);
	if (efifb == NULL)
		return (CN_DEAD);

	info->fb_type = FBTYPE_EFIFB;
	info->fb_height = efifb->fb_height;
	info->fb_width = efifb->fb_width;

	info->fb_depth = fls(efifb->fb_mask_red | efifb->fb_mask_green |
	    efifb->fb_mask_blue | efifb->fb_mask_reserved);
	/* Round to a multiple of the bits in a byte. */
	info->fb_bpp = roundup2(info->fb_depth, NBBY);

	/* Stride in bytes, not pixels */
	info->fb_stride = efifb->fb_stride * (info->fb_bpp / NBBY);

	roff = ffs(efifb->fb_mask_red) - 1;
	goff = ffs(efifb->fb_mask_green) - 1;
	boff = ffs(efifb->fb_mask_blue) - 1;
	vt_config_cons_colors(info, COLOR_FORMAT_RGB,
	    efifb->fb_mask_red >> roff, roff,
	    efifb->fb_mask_green >> goff, goff,
	    efifb->fb_mask_blue >> boff, boff);
	info->fb_cmsize = NCOLORS;

	info->fb_size = info->fb_height * info->fb_stride;
	info->fb_pbase = efifb->fb_addr;
	info->fb_vbase = (intptr_t)pmap_mapdev_attr(info->fb_pbase,
	    info->fb_size, memattr);

	vt_fb_init(vd);

	return (CN_INTERNAL);
}

static void
vt_efifb_fini(struct vt_device *vd, void *softc)
{
	struct fb_info	*info = softc;

	vt_fb_fini(vd, softc);
	pmap_unmapdev((void *)info->fb_vbase, info->fb_size);
}
