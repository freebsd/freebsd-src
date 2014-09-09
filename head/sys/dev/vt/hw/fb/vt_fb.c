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
#include <sys/malloc.h>
#include <sys/queue.h>
#include <sys/fbio.h>
#include <dev/vt/vt.h>
#include <dev/vt/hw/fb/vt_fb.h>
#include <dev/vt/colors/vt_termcolors.h>

static vd_drawrect_t	vt_fb_drawrect;
static vd_setpixel_t	vt_fb_setpixel;

static struct vt_driver vt_fb_driver = {
	.vd_name = "fb",
	.vd_init = vt_fb_init,
	.vd_blank = vt_fb_blank,
	.vd_bitblt_text = vt_fb_bitblt_text,
	.vd_bitblt_bmp = vt_fb_bitblt_bitmap,
	.vd_drawrect = vt_fb_drawrect,
	.vd_setpixel = vt_fb_setpixel,
	.vd_postswitch = vt_fb_postswitch,
	.vd_priority = VD_PRIORITY_GENERIC+10,
	.vd_fb_ioctl = vt_fb_ioctl,
	.vd_fb_mmap = vt_fb_mmap,
};

VT_DRIVER_DECLARE(vt_fb, vt_fb_driver);

static void
vt_fb_mem_wr1(struct fb_info *sc, uint32_t o, uint8_t v)
{

	KASSERT((o < sc->fb_size), ("Offset %#08x out of fb size", o));
	*(uint8_t *)(sc->fb_vbase + o) = v;
}

static void
vt_fb_mem_wr2(struct fb_info *sc, uint32_t o, uint16_t v)
{

	KASSERT((o < sc->fb_size), ("Offset %#08x out of fb size", o));
	*(uint16_t *)(sc->fb_vbase + o) = v;
}

static void
vt_fb_mem_wr4(struct fb_info *sc, uint32_t o, uint32_t v)
{

	KASSERT((o < sc->fb_size), ("Offset %#08x out of fb size", o));
	*(uint32_t *)(sc->fb_vbase + o) = v;
}

int
vt_fb_ioctl(struct vt_device *vd, u_long cmd, caddr_t data, struct thread *td)
{
	struct fb_info *info;
	int error = 0;

	info = vd->vd_softc;

	switch (cmd) {
	case FBIOGTYPE:
		bcopy(info, (struct fbtype *)data, sizeof(struct fbtype));
		break;

	case FBIO_GETWINORG:	/* get frame buffer window origin */
		*(u_int *)data = 0;
		break;

	case FBIO_GETDISPSTART:	/* get display start address */
		((video_display_start_t *)data)->x = 0;
		((video_display_start_t *)data)->y = 0;
		break;

	case FBIO_GETLINEWIDTH:	/* get scan line width in bytes */
		*(u_int *)data = info->fb_stride;
		break;

	case FBIO_BLANK:	/* blank display */
		if (vd->vd_driver->vd_blank == NULL)
			return (ENODEV);
		vd->vd_driver->vd_blank(vd, TC_BLACK);
		break;

	default:
		error = ENOIOCTL;
		break;
	}

	return (error);
}

int
vt_fb_mmap(struct vt_device *vd, vm_ooffset_t offset, vm_paddr_t *paddr,
    int prot, vm_memattr_t *memattr)
{
	struct fb_info *info;

	info = vd->vd_softc;

	if (info->fb_flags & FB_FLAG_NOMMAP)
		return (ENODEV);

	if (offset >= 0 && offset < info->fb_size) {
		*paddr = info->fb_pbase + offset;
	#ifdef VM_MEMATTR_WRITE_COMBINING
		*memattr = VM_MEMATTR_WRITE_COMBINING;
	#endif
		return (0);
	}

	return (EINVAL);
}

static void
vt_fb_setpixel(struct vt_device *vd, int x, int y, term_color_t color)
{
	struct fb_info *info;
	uint32_t c;
	u_int o;

	info = vd->vd_softc;
	c = info->fb_cmap[color];
	o = info->fb_stride * y + x * FBTYPE_GET_BYTESPP(info);

	KASSERT((info->fb_vbase != 0), ("Unmapped framebuffer"));

	switch (FBTYPE_GET_BYTESPP(info)) {
	case 1:
		vt_fb_mem_wr1(info, o, c);
		break;
	case 2:
		vt_fb_mem_wr2(info, o, c);
		break;
	case 3:
		vt_fb_mem_wr1(info, o, (c >> 16) & 0xff);
		vt_fb_mem_wr1(info, o + 1, (c >> 8) & 0xff);
		vt_fb_mem_wr1(info, o + 2, c & 0xff);
		break;
	case 4:
		vt_fb_mem_wr4(info, o, c);
		break;
	default:
		/* panic? */
		return;
	}

}

static void
vt_fb_drawrect(struct vt_device *vd, int x1, int y1, int x2, int y2, int fill,
    term_color_t color)
{
	int x, y;

	for (y = y1; y <= y2; y++) {
		if (fill || (y == y1) || (y == y2)) {
			for (x = x1; x <= x2; x++)
				vt_fb_setpixel(vd, x, y, color);
		} else {
			vt_fb_setpixel(vd, x1, y, color);
			vt_fb_setpixel(vd, x2, y, color);
		}
	}
}

void
vt_fb_blank(struct vt_device *vd, term_color_t color)
{
	struct fb_info *info;
	uint32_t c;
	u_int o, h;

	info = vd->vd_softc;
	c = info->fb_cmap[color];

	KASSERT((info->fb_vbase != 0), ("Unmapped framebuffer"));

	switch (FBTYPE_GET_BYTESPP(info)) {
	case 1:
		for (h = 0; h < info->fb_height; h++)
			for (o = 0; o < info->fb_stride; o++)
				vt_fb_mem_wr1(info, h*info->fb_stride + o, c);
		break;
	case 2:
		for (h = 0; h < info->fb_height; h++)
			for (o = 0; o < info->fb_stride; o += 2)
				vt_fb_mem_wr2(info, h*info->fb_stride + o, c);
		break;
	case 3:
		for (h = 0; h < info->fb_height; h++)
			for (o = 0; o < info->fb_stride; o += 3) {
				vt_fb_mem_wr1(info, h*info->fb_stride + o,
				    (c >> 16) & 0xff);
				vt_fb_mem_wr1(info, h*info->fb_stride + o + 1,
				    (c >> 8) & 0xff);
				vt_fb_mem_wr1(info, h*info->fb_stride + o + 2,
				    c & 0xff);
			}
		break;
	case 4:
		for (h = 0; h < info->fb_height; h++)
			for (o = 0; o < info->fb_stride; o += 4)
				vt_fb_mem_wr4(info, h*info->fb_stride + o, c);
		break;
	default:
		/* panic? */
		return;
	}
}

void
vt_fb_bitblt_bitmap(struct vt_device *vd, const struct vt_window *vw,
    const uint8_t *pattern, const uint8_t *mask,
    unsigned int width, unsigned int height,
    unsigned int x, unsigned int y, term_color_t fg, term_color_t bg)
{
	struct fb_info *info;
	uint32_t fgc, bgc, cc, o;
	int c, l, bpp, bpl;
	u_long line;
	uint8_t b, m;
	const uint8_t *ch;

	info = vd->vd_softc;
	bpp = FBTYPE_GET_BYTESPP(info);
	fgc = info->fb_cmap[fg];
	bgc = info->fb_cmap[bg];
	b = m = 0;
	bpl = (width + 7) >> 3; /* Bytes per source line. */

	KASSERT((info->fb_vbase != 0), ("Unmapped framebuffer"));

	line = (info->fb_stride * y) + (x * bpp);
	for (l = 0;
	    l < height && y + l < vw->vw_draw_area.tr_end.tp_row;
	    l++) {
		ch = pattern;
		for (c = 0;
		    c < width && x + c < vw->vw_draw_area.tr_end.tp_col;
		    c++) {
			if (c % 8 == 0)
				b = *ch++;
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
			o = line + (c * bpp);
			cc = b & 0x80 ? fgc : bgc;

			switch(bpp) {
			case 1:
				vt_fb_mem_wr1(info, o, cc);
				break;
			case 2:
				vt_fb_mem_wr2(info, o, cc);
				break;
			case 3:
				/* Packed mode, so unaligned. Byte access. */
				vt_fb_mem_wr1(info, o, (cc >> 16) & 0xff);
				vt_fb_mem_wr1(info, o + 1, (cc >> 8) & 0xff);
				vt_fb_mem_wr1(info, o + 2, cc & 0xff);
				break;
			case 4:
				vt_fb_mem_wr4(info, o, cc);
				break;
			default:
				/* panic? */
				break;
			}
		}
		line += info->fb_stride;
		pattern += bpl;
	}
}

void
vt_fb_bitblt_text(struct vt_device *vd, const struct vt_window *vw,
    const term_rect_t *area)
{
	unsigned int col, row, x, y;
	struct vt_font *vf;
	term_char_t c;
	term_color_t fg, bg;
	const uint8_t *pattern;

	vf = vw->vw_font;

	for (row = area->tr_begin.tp_row; row < area->tr_end.tp_row; ++row) {
		for (col = area->tr_begin.tp_col; col < area->tr_end.tp_col;
		    ++col) {
			x = col * vf->vf_width +
			    vw->vw_draw_area.tr_begin.tp_col;
			y = row * vf->vf_height +
			    vw->vw_draw_area.tr_begin.tp_row;

			c = VTBUF_GET_FIELD(&vw->vw_buf, row, col);
			pattern = vtfont_lookup(vf, c);
			vt_determine_colors(c,
			    VTBUF_ISCURSOR(&vw->vw_buf, row, col), &fg, &bg);

			vt_fb_bitblt_bitmap(vd, vw,
			    pattern, NULL, vf->vf_width, vf->vf_height,
			    x, y, fg, bg);
		}
	}

#ifndef SC_NO_CUTPASTE
	if (!vd->vd_mshown)
		return;

	term_rect_t drawn_area;

	drawn_area.tr_begin.tp_col = area->tr_begin.tp_col * vf->vf_width;
	drawn_area.tr_begin.tp_row = area->tr_begin.tp_row * vf->vf_height;
	drawn_area.tr_end.tp_col = area->tr_end.tp_col * vf->vf_width;
	drawn_area.tr_end.tp_row = area->tr_end.tp_row * vf->vf_height;

	if (vt_is_cursor_in_area(vd, &drawn_area)) {
		vt_fb_bitblt_bitmap(vd, vw,
		    vd->vd_mcursor->map, vd->vd_mcursor->mask,
		    vd->vd_mcursor->width, vd->vd_mcursor->height,
		    vd->vd_mx_drawn + vw->vw_draw_area.tr_begin.tp_col,
		    vd->vd_my_drawn + vw->vw_draw_area.tr_begin.tp_row,
		    vd->vd_mcursor_fg, vd->vd_mcursor_bg);
	}
#endif
}

void
vt_fb_postswitch(struct vt_device *vd)
{
	struct fb_info *info;

	info = vd->vd_softc;

	if (info->enter != NULL)
		info->enter(info->fb_priv);
}

static int
vt_fb_init_cmap(uint32_t *cmap, int depth)
{

	switch (depth) {
	case 8:
		return (vt_generate_cons_palette(cmap, COLOR_FORMAT_RGB,
		    0x7, 5, 0x7, 2, 0x3, 0));
	case 15:
		return (vt_generate_cons_palette(cmap, COLOR_FORMAT_RGB,
		    0x1f, 10, 0x1f, 5, 0x1f, 0));
	case 16:
		return (vt_generate_cons_palette(cmap, COLOR_FORMAT_RGB,
		    0x1f, 11, 0x3f, 5, 0x1f, 0));
	case 24:
	case 32: /* Ignore alpha. */
		return (vt_generate_cons_palette(cmap, COLOR_FORMAT_RGB,
		    0xff, 16, 0xff, 8, 0xff, 0));
	default:
		return (1);
	}
}

int
vt_fb_init(struct vt_device *vd)
{
	struct fb_info *info;
	int err;

	info = vd->vd_softc;
	vd->vd_height = info->fb_height;
	vd->vd_width = info->fb_width;

	if (info->fb_size == 0)
		return (CN_DEAD);

	if (info->fb_pbase == 0)
		info->fb_flags |= FB_FLAG_NOMMAP;

	if (info->fb_cmsize <= 0) {
		err = vt_fb_init_cmap(info->fb_cmap, FBTYPE_GET_BPP(info));
		if (err)
			return (CN_DEAD);
		info->fb_cmsize = 16;
	}

	/* Clear the screen. */
	vd->vd_driver->vd_blank(vd, TC_BLACK);

	/* Wakeup screen. KMS need this. */
	vt_fb_postswitch(vd);

	return (CN_INTERNAL);
}

int
vt_fb_attach(struct fb_info *info)
{

	vt_allocate(&vt_fb_driver, info);

	return (0);
}

void
vt_fb_resume(void)
{

	vt_resume();
}

void
vt_fb_suspend(void)
{

	vt_suspend();
}
