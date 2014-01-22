/*-
 * Copyright (c) 2013 The FreeBSD Foundation
 * All rights reserved.
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
 *
 * $FreeBSD$
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/fbio.h>

#include "opt_platform.h"

#include <dev/vt/vt.h>
#include <dev/vt/hw/fb/vt_fb.h>
#include <dev/vt/colors/vt_termcolors.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/bus.h>
#include <machine/pmap.h>
#include <machine/vmparam.h>
#include <machine/xbox.h>

#define	VT_XBOX_WIDTH	640
#define	VT_XBOX_HEIGHT	480

static vd_init_t xboxfb_init;

static struct vt_driver xboxfb_driver = {
	.vd_init = xboxfb_init,
	.vd_blank = vt_fb_blank,
	.vd_bitbltchr = vt_fb_bitbltchr,
	.vd_priority = VD_PRIORITY_GENERIC,
};

static struct fb_info xboxfb_info;
VT_CONSDEV_DECLARE(xboxfb_driver, PIXEL_WIDTH(VT_XBOX_WIDTH),
    PIXEL_HEIGHT(VT_XBOX_HEIGHT), &xboxfb_info);

static int
xboxfb_init(struct vt_device *vd)
{
	struct fb_info *info;
	int i;

	if (!arch_i386_is_xbox)
		return (CN_DEAD);

	info = &xboxfb_info;
	/*
	 * We must make a mapping from video framebuffer memory
	 * to real. This is very crude:  we map the entire
	 * videomemory to PAGE_SIZE! Since our kernel lives at
	 * it's relocated address range (0xc0xxxxxx), it won't
	 * care.
	 *
	 * We use address PAGE_SIZE and up so we can still trap
	 * NULL pointers.  Once the real init is called, the
	 * mapping will be done via the OS and stored in a more
	 * sensible location ... but since we're not fully
	 * initialized, this is our only way to go :-(
	 */
	for (i = 0; i < (XBOX_FB_SIZE / PAGE_SIZE); i++) {
		pmap_kenter(((i + 1) * PAGE_SIZE),
		    XBOX_FB_START + (i * PAGE_SIZE));
	}
	pmap_kenter((i + 1) * PAGE_SIZE,
	    XBOX_FB_START_PTR - XBOX_FB_START_PTR % PAGE_SIZE);

	/* Ensure the framebuffer is where we want it to be. */
	*(uint32_t *)((i + 1) * PAGE_SIZE + XBOX_FB_START_PTR % PAGE_SIZE) =
	    XBOX_FB_START;

	/* Initialize fb_info. */
	info = vd->vd_softc;

	info->fb_width = VT_XBOX_WIDTH;
	info->fb_height = VT_XBOX_HEIGHT;

	info->fb_size = XBOX_FB_SIZE;
	info->fb_stride = VT_XBOX_WIDTH * 4; /* 32bits per pixel. */

	info->fb_vbase = PAGE_SIZE;
	info->fb_pbase = XBOX_FB_START_PTR;

	/* Get pixel storage size. */
	info->fb_bpp = 32;
	/* Get color depth. */
	info->fb_depth = 24;

	vt_generate_vga_palette(info->fb_cmap, COLOR_FORMAT_RGB, 255, 0, 255,
	    8, 255, 16);
	fb_probe(info);
	vt_fb_init(vd);

	return (CN_INTERNAL);
}

static void
xbox_remap(void *unused)
{
	struct fb_info *info;

	if (!arch_i386_is_xbox)
		return;

	info = &xboxfb_info;
	info->fb_vbase = (intptr_t)pmap_mapdev(info->fb_pbase, info->fb_size);
}

SYSINIT(xboxfb, SI_SUB_DRIVERS, SI_ORDER_ANY, xbox_remap, NULL);
