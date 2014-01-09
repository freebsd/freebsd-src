/*-
 * Copyright (c) 2005 Rink Springer
 * All rights reserved.
 *
 * Copyright (c) 2009 The FreeBSD Foundation
 * All rights reserved.
 *
 * Portions of this software were developed by Ed Schouten
 * under sponsorship from the FreeBSD Foundation.
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
#include <sys/kernel.h>
#include <sys/systm.h>

#include <dev/vt/vt.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/bus.h>
#include <machine/pmap.h>
#include <machine/vmparam.h>
#include <machine/xbox.h>

struct xbox_softc {
	bus_space_tag_t		xbox_fb_tag;
	bus_space_handle_t	xbox_fb_handle;
};

/* Convenience macros. */
#define	MEM_WRITE4(sc, ofs, val) \
	bus_space_write_4(sc->xbox_fb_tag, sc->xbox_fb_handle, ofs, val)

#define	VT_XBOX_WIDTH	640
#define	VT_XBOX_HEIGHT	480

static vd_init_t	xbox_init;
static vd_blank_t	xbox_blank;
static vd_bitbltchr_t	xbox_bitbltchr;

static const struct vt_driver vt_xbox_driver = {
	.vd_init	= xbox_init,
	.vd_blank	= xbox_blank,
	.vd_bitbltchr	= xbox_bitbltchr,
	.vd_priority	= VD_PRIORITY_GENERIC+1,
};

static struct xbox_softc xbox_conssoftc;
VT_CONSDEV_DECLARE(vt_xbox_driver, PIXEL_WIDTH(VT_XBOX_WIDTH),
    PIXEL_HEIGHT(VT_XBOX_HEIGHT), &xbox_conssoftc);

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
xbox_blank(struct vt_device *vd, term_color_t color)
{
	struct xbox_softc *sc = vd->vd_softc;
	u_int ofs;
	uint32_t c;

	c = colormap[color];
	for (ofs = 0; ofs < (VT_XBOX_WIDTH * VT_XBOX_HEIGHT) * 4; ofs += 4)
		MEM_WRITE4(sc, ofs, c);
}

static void
xbox_bitbltchr(struct vt_device *vd, const uint8_t *src, const uint8_t *mask,
    int bpl, vt_axis_t top, vt_axis_t left, unsigned int width,
    unsigned int height, term_color_t fg, term_color_t bg)
{
	struct xbox_softc *sc = vd->vd_softc;
	u_long line;
	uint32_t fgc, bgc;
	int c;
	uint8_t b, m;

	fgc = colormap[fg];
	bgc = colormap[bg];

	/* Don't try to put off screen pixels */
	if (((left + width) > info->fb_width) || ((top + height) >
	    info->fb_height))
		return;

	line = (VT_XBOX_WIDTH * top + left) * 4;
	for (; height > 0; height--) {
		for (c = 0; c < width; c++) {
			if (c % 8 == 0)
				b = *src++;
			else
				b <<= 1;
			if (mask != NULL) {
				if (c % 8 == 0)
					m = *mask++;
				else
					m <<= 1;
				/* Skip pixel write, if mask has no bit set. */
				if ((m & 0x80) == 0)
					continue;
			}
			MEM_WRITE4(sc, line + c * 4, b & 0x80 ? fgc : bgc);
		}
		line += VT_XBOX_WIDTH * 4;
	}
}

static void
xbox_initialize(struct vt_device *vd)
{
	int i;

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

	/* Clear the screen. */
	xbox_blank(vd, TC_BLACK);
}

static int
xbox_init(struct vt_device *vd)
{
	struct xbox_softc *sc = vd->vd_softc;

	if (!arch_i386_is_xbox)
		return (CN_DEAD);

	sc->xbox_fb_tag = X86_BUS_SPACE_MEM;
	sc->xbox_fb_handle = PAGE_SIZE;

	vd->vd_width = VT_XBOX_WIDTH;
	vd->vd_height = VT_XBOX_HEIGHT;

	xbox_initialize(vd);

	return (CN_INTERNAL);
}

static void
xbox_remap(void *unused)
{

	if (!arch_i386_is_xbox)
		return;

	xbox_conssoftc.xbox_fb_handle =
	    (bus_space_handle_t)pmap_mapdev(XBOX_FB_START, XBOX_FB_SIZE);
}

SYSINIT(xboxfb, SI_SUB_DRIVERS, SI_ORDER_ANY, xbox_remap, NULL);
