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

static int vt_fb_ioctl(struct vt_device *vd, u_long cmd, caddr_t data,
    struct thread *td);
static int vt_fb_mmap(struct vt_device *vd, vm_ooffset_t offset,
    vm_paddr_t *paddr, int prot, vm_memattr_t *memattr);

static struct vt_driver vt_fb_driver = {
	.vd_init = vt_fb_init,
	.vd_blank = vt_fb_blank,
	.vd_bitbltchr = vt_fb_bitbltchr,
	.vd_postswitch = vt_fb_postswitch,
	.vd_priority = VD_PRIORITY_GENERIC+10,
	.vd_fb_ioctl = vt_fb_ioctl,
	.vd_fb_mmap = vt_fb_mmap,
};

static int
vt_fb_ioctl(struct vt_device *vd, u_long cmd, caddr_t data, struct thread *td)
{
	struct fb_info *info;

	info = vd->vd_softc;

	if (info->fb_ioctl == NULL)
		return (-1);

	return (info->fb_ioctl(info->fb_cdev, cmd, data, 0, td));
}

static int vt_fb_mmap(struct vt_device *vd, vm_ooffset_t offset,
    vm_paddr_t *paddr, int prot, vm_memattr_t *memattr)
{
	struct fb_info *info;

	info = vd->vd_softc;

	if (info->fb_ioctl == NULL)
		return (ENXIO);

	return (info->fb_mmap(info->fb_cdev, offset, paddr, prot, memattr));
}

void
vt_fb_blank(struct vt_device *vd, term_color_t color)
{
	struct fb_info *info;
	uint32_t c;
	u_int o;

	info = vd->vd_softc;
	c = info->fb_cmap[color];

	switch (FBTYPE_GET_BYTESPP(info)) {
	case 1:
		for (o = 0; o < info->fb_stride; o++)
			info->wr1(info, o, c);
		break;
	case 2:
		for (o = 0; o < info->fb_stride; o += 2)
			info->wr2(info, o, c);
		break;
	case 3:
		/* line 0 */
		for (o = 0; o < info->fb_stride; o += 3) {
			info->wr1(info, o, (c >> 16) & 0xff);
			info->wr1(info, o + 1, (c >> 8) & 0xff);
			info->wr1(info, o + 2, c & 0xff);
		}
		break;
	case 4:
		for (o = 0; o < info->fb_stride; o += 4)
			info->wr4(info, o, c);
		break;
	default:
		/* panic? */
		return;
	}
	/* Copy line0 to all other lines. */
	/* XXX will copy with borders. */
	for (o = info->fb_stride; o < info->fb_size; o += info->fb_stride) {
		info->copy(info, o, 0, info->fb_stride);
	}
}

void
vt_fb_bitbltchr(struct vt_device *vd, const uint8_t *src, const uint8_t *mask,
    int bpl, vt_axis_t top, vt_axis_t left, unsigned int width,
    unsigned int height, term_color_t fg, term_color_t bg)
{
	struct fb_info *info;
	uint32_t fgc, bgc, cc, o;
	int c, l, bpp;
	u_long line;
	uint8_t b, m;
	const uint8_t *ch;

	info = vd->vd_softc;
	bpp = FBTYPE_GET_BYTESPP(info);
	fgc = info->fb_cmap[fg];
	bgc = info->fb_cmap[bg];
	b = m = 0;
	if (bpl == 0)
		bpl = (width + 7) >> 3; /* Bytes per sorce line. */

	/* Don't try to put off screen pixels */
	if (((left + width) > info->fb_width) || ((top + height) >
	    info->fb_height))
		return;

	line = (info->fb_stride * top) + (left * bpp);
	for (l = 0; l < height; l++) {
		ch = src;
		for (c = 0; c < width; c++) {
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
				info->wr1(info, o, cc);
				break;
			case 2:
				info->wr2(info, o, cc);
				break;
			case 3:
				/* Packed mode, so unaligned. Byte access. */
				info->wr1(info, o, (cc >> 16) & 0xff);
				info->wr1(info, o + 1, (cc >> 8) & 0xff);
				info->wr1(info, o + 2, cc & 0xff);
				break;
			case 4:
				info->wr4(info, o, cc);
				break;
			default:
				/* panic? */
				break;
			}
		}
		line += info->fb_stride;
		src += bpl;
	}
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
		return (vt_generate_vga_palette(cmap, COLOR_FORMAT_RGB,
		    0x7, 5, 0x7, 2, 0x3, 0));
	case 15:
		return (vt_generate_vga_palette(cmap, COLOR_FORMAT_RGB,
		    0x1f, 10, 0x1f, 5, 0x1f, 0));
	case 16:
		return (vt_generate_vga_palette(cmap, COLOR_FORMAT_RGB,
		    0x1f, 11, 0x3f, 5, 0x1f, 0));
	case 24:
	case 32: /* Ignore alpha. */
		return (vt_generate_vga_palette(cmap, COLOR_FORMAT_RGB,
		    0xff, 0, 0xff, 8, 0xff, 16));
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

	if (info->fb_cmsize <= 0) {
		err = vt_fb_init_cmap(info->fb_cmap, FBTYPE_GET_BPP(info));
		if (err)
			return (CN_DEAD);
		info->fb_cmsize = 16;
	}

	/* Clear the screen. */
	vt_fb_blank(vd, TC_BLACK);

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
