/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2009 Jared D. McNeill <jmcneill@invisible.ca>
 * All rights reserved.
 * Copyright 2020 Toomas Soome
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
#include <stand.h>
#include <sys/param.h>
#include <machine/psl.h>
#include <machine/cpufunc.h>
#include <stdbool.h>
#include <bootstrap.h>
#include <btxv86.h>
#include <gfx_fb.h>
#include <dev/vt/hw/vga/vt_vga_reg.h>
#include "libi386.h"
#include "vbe.h"

/*
 * VESA BIOS Extensions routines
 */

static struct vbeinfoblock *vbe;
static struct modeinfoblock *vbe_mode;

static uint16_t *vbe_mode_list;
static size_t vbe_mode_list_size;
struct vesa_edid_info *edid_info = NULL;

/* The default VGA color palette format is 6 bits per primary color. */
int palette_format = 6;

#define	VESA_MODE_BASE	0x100

/*
 * palette array for 8-bit indexed colors. In this case, cmap does store
 * index and pe8 does store actual RGB. This is needed because we may
 * not be able to read palette data from hardware.
 */
struct paletteentry *pe8 = NULL;

static struct named_resolution {
	const char *name;
	const char *alias;
	unsigned int width;
	unsigned int height;
} resolutions[] = {
	{
		.name = "480p",
		.width = 640,
		.height = 480,
	},
	{
		.name = "720p",
		.width = 1280,
		.height = 720,
	},
	{
		.name = "1080p",
		.width = 1920,
		.height = 1080,
	},
	{
		.name = "2160p",
		.alias = "4k",
		.width = 3840,
		.height = 2160,
	},
	{
		.name = "5k",
		.width = 5120,
		.height = 2880,
	}
};

static bool
vbe_resolution_compare(struct named_resolution *res, const char *cmp)
{

	if (strcasecmp(res->name, cmp) == 0)
		return (true);
	if (res->alias != NULL && strcasecmp(res->alias, cmp) == 0)
		return (true);
	return (false);
}

static void
vbe_get_max_resolution(int *width, int *height)
{
	struct named_resolution *res;
	char *maxres;
	char *height_start, *width_start;
	int idx;

	*width = *height = 0;
	maxres = getenv("vbe_max_resolution");
	/* No max_resolution set? Bail out; choose highest resolution */
	if (maxres == NULL)
		return;
	/* See if it matches one of our known resolutions */
	for (idx = 0; idx < nitems(resolutions); ++idx) {
		res = &resolutions[idx];
		if (vbe_resolution_compare(res, maxres)) {
			*width = res->width;
			*height = res->height;
			return;
		}
	}
	/* Not a known resolution, try to parse it; make a copy we can modify */
	maxres = strdup(maxres);
	if (maxres == NULL)
		return;
	height_start = strchr(maxres, 'x');
	if (height_start == NULL) {
		free(maxres);
		return;
	}
	width_start = maxres;
	*height_start++ = 0;
	/* Errors from this will effectively mean "no max" */
	*width = (int)strtol(width_start, NULL, 0);
	*height = (int)strtol(height_start, NULL, 0);
	free(maxres);
}

int
vga_get_reg(int reg, int index)
{
	return (inb(reg + index));
}

int
vga_get_atr(int reg, int i)
{
	int ret;

	(void) inb(reg + VGA_GEN_INPUT_STAT_1);
	outb(reg + VGA_AC_WRITE, i);
	ret = inb(reg + VGA_AC_READ);

	(void) inb(reg + VGA_GEN_INPUT_STAT_1);

	return (ret);
}

void
vga_set_atr(int reg, int i, int v)
{
	(void) inb(reg + VGA_GEN_INPUT_STAT_1);
	outb(reg + VGA_AC_WRITE, i);
	outb(reg + VGA_AC_WRITE, v);

	(void) inb(reg + VGA_GEN_INPUT_STAT_1);
}

void
vga_set_indexed(int reg, int indexreg, int datareg, uint8_t index, uint8_t val)
{
	outb(reg + indexreg, index);
	outb(reg + datareg, val);
}

int
vga_get_indexed(int reg, int indexreg, int datareg, uint8_t index)
{
	outb(reg + indexreg, index);
	return (inb(reg + datareg));
}

int
vga_get_crtc(int reg, int i)
{
	return (vga_get_indexed(reg, VGA_CRTC_ADDRESS, VGA_CRTC_DATA, i));
}

void
vga_set_crtc(int reg, int i, int v)
{
	vga_set_indexed(reg, VGA_CRTC_ADDRESS, VGA_CRTC_DATA, i, v);
}

int
vga_get_seq(int reg, int i)
{
	return (vga_get_indexed(reg, VGA_SEQ_ADDRESS, VGA_SEQ_DATA, i));
}

void
vga_set_seq(int reg, int i, int v)
{
	vga_set_indexed(reg, VGA_SEQ_ADDRESS, VGA_SEQ_DATA, i, v);
}

int
vga_get_grc(int reg, int i)
{
	return (vga_get_indexed(reg, VGA_GC_ADDRESS, VGA_GC_DATA, i));
}

void
vga_set_grc(int reg, int i, int v)
{
	vga_set_indexed(reg, VGA_GC_ADDRESS, VGA_GC_DATA, i, v);
}

/*
 * Return true when this controller is VGA compatible.
 */
bool
vbe_is_vga(void)
{
	if (vbe == NULL)
		return (false);

	return ((vbe->Capabilities & VBE_CAP_NONVGA) == 0);
}

/* Actually assuming mode 3. */
void
bios_set_text_mode(int mode)
{
	int atr;

	if (vbe->Capabilities & VBE_CAP_DAC8) {
		int m;

		/*
		 * The mode change should reset the palette format to
		 * 6 bits, but apparently some systems do fail with 8-bit
		 * palette, so we switch to 6-bit here.
		 */
		m = 0x0600;
		(void) biosvbe_palette_format(&m);
		palette_format = m;
	}
	v86.ctl = V86_FLAGS;
	v86.addr = 0x10;
	v86.eax = mode;				/* set VGA text mode */
	v86int();
	atr = vga_get_atr(VGA_REG_BASE, VGA_AC_MODE_CONTROL);
	atr &= ~VGA_AC_MC_BI;
	atr &= ~VGA_AC_MC_ELG;
	vga_set_atr(VGA_REG_BASE, VGA_AC_MODE_CONTROL, atr);

	gfx_state.tg_mode = mode;
	gfx_state.tg_fb_type = FB_TEXT;
	gfx_state.tg_fb.fb_height = TEXT_ROWS;
	gfx_state.tg_fb.fb_width = TEXT_COLS;

	gfx_state.tg_fb.fb_mask_red = (1 << palette_format) - 1 << 16;
	gfx_state.tg_fb.fb_mask_green = (1 << palette_format) - 1 << 8;
	gfx_state.tg_fb.fb_mask_blue = (1 << palette_format) - 1 << 0;
	gfx_state.tg_ctype = CT_INDEXED;
	env_setenv("screen.textmode", EV_VOLATILE | EV_NOHOOK, "1", NULL, NULL);
}

/* Function 00h - Return VBE Controller Information */
static int
biosvbe_info(struct vbeinfoblock *vbep)
{
	struct vbeinfoblock *rvbe;
	int ret;

	if (vbep == NULL)
		return (VBE_FAILED);

	rvbe = bio_alloc(sizeof(*rvbe));
	if (rvbe == NULL)
		return (VBE_FAILED);

	/* Now check if we have vesa. */
	memset(rvbe, 0, sizeof (*vbe));
	memcpy(rvbe->VbeSignature, "VBE2", 4);

	v86.ctl = V86_FLAGS;
	v86.addr = 0x10;
	v86.eax = 0x4f00;
	v86.es = VTOPSEG(rvbe);
	v86.edi = VTOPOFF(rvbe);
	v86int();
	ret = v86.eax & 0xffff;

	if (ret != VBE_SUCCESS)
		goto done;

	if (memcmp(rvbe->VbeSignature, "VESA", 4) != 0) {
		ret = VBE_NOTSUP;
		goto done;
	}
	bcopy(rvbe, vbep, sizeof(*vbep));
done:
	bio_free(rvbe, sizeof(*rvbe));
	return (ret);
}

/* Function 01h - Return VBE Mode Information */
static int
biosvbe_get_mode_info(int mode, struct modeinfoblock *mi)
{
	struct modeinfoblock *rmi;
	int ret;

	rmi = bio_alloc(sizeof(*rmi));
	if (rmi == NULL)
		return (VBE_FAILED);

	v86.ctl = V86_FLAGS;
	v86.addr = 0x10;
	v86.eax = 0x4f01;
	v86.ecx = mode;
	v86.es = VTOPSEG(rmi);
	v86.edi = VTOPOFF(rmi);
	v86int();

	ret = v86.eax & 0xffff;
	if (ret != VBE_SUCCESS)
		goto done;
	bcopy(rmi, mi, sizeof(*rmi));
done:
	bio_free(rmi, sizeof(*rmi));
	return (ret);
}

/* Function 02h - Set VBE Mode */
static int
biosvbe_set_mode(int mode, struct crtciinfoblock *ci)
{
	int rv;

	if (vbe->Capabilities & VBE_CAP_DAC8) {
		int m;

		/*
		 * The mode change should reset the palette format to
		 * 6 bits, but apparently some systems do fail with 8-bit
		 * palette, so we switch to 6-bit here.
		 */
		m = 0x0600;
		if (biosvbe_palette_format(&m) == VBE_SUCCESS)
			palette_format = m;
	}
	v86.ctl = V86_FLAGS;
	v86.addr = 0x10;
	v86.eax = 0x4f02;
	v86.ebx = mode | 0x4000;	/* set linear FB bit */
	v86.es = VTOPSEG(ci);
	v86.edi = VTOPOFF(ci);
	v86int();
	rv = v86.eax & 0xffff;
	if (vbe->Capabilities & VBE_CAP_DAC8) {
		int m;

		/* Switch to 8-bits per primary color. */
		m = 0x0800;
		if (biosvbe_palette_format(&m) == VBE_SUCCESS)
			palette_format = m;
	}
	env_setenv("screen.textmode", EV_VOLATILE | EV_NOHOOK, "0", NULL, NULL);
	return (rv);
}

/* Function 03h - Get VBE Mode */
static int
biosvbe_get_mode(int *mode)
{
	v86.ctl = V86_FLAGS;
	v86.addr = 0x10;
	v86.eax = 0x4f03;
	v86int();
	*mode = v86.ebx & 0x3fff;	/* Bits 0-13 */
	return (v86.eax & 0xffff);
}

/* Function 08h - Set/Get DAC Palette Format */
int
biosvbe_palette_format(int *format)
{
	v86.ctl = V86_FLAGS;
	v86.addr = 0x10;
	v86.eax = 0x4f08;
	v86.ebx = *format;
	v86int();
	*format = (v86.ebx >> 8) & 0xff;
	return (v86.eax & 0xffff);
}

/* Function 09h - Set/Get Palette Data */
static int
biosvbe_palette_data(int mode, int reg, struct paletteentry *pe)
{
	v86.ctl = V86_FLAGS;
	v86.addr = 0x10;
	v86.eax = 0x4f09;
	v86.ebx = mode;
	v86.edx = reg;
	v86.ecx = 1;
	v86.es = VTOPSEG(pe);
	v86.edi = VTOPOFF(pe);
	v86int();
	return (v86.eax & 0xffff);
}

/*
 * Function 15h BL=00h - Report VBE/DDC Capabilities
 *
 * int biosvbe_ddc_caps(void)
 * return: VBE/DDC capabilities
 */
static int
biosvbe_ddc_caps(void)
{
	v86.ctl = V86_FLAGS;
	v86.addr = 0x10;
	v86.eax = 0x4f15;	/* display identification extensions */
	v86.ebx = 0;		/* report DDC capabilities */
	v86.ecx = 0;		/* controller unit number (00h = primary) */
	v86.es = 0;
	v86.edi = 0;
	v86int();
	if (VBE_ERROR(v86.eax & 0xffff))
		return (0);
	return (v86.ebx & 0xffff);
}

/* Function 11h BL=01h - Flat Panel status */
static int
biosvbe_ddc_read_flat_panel_info(void *buf)
{
	v86.ctl = V86_FLAGS;
	v86.addr = 0x10;
	v86.eax = 0x4f11;	/* Flat Panel Interface extensions */
	v86.ebx = 1;		/* Return Flat Panel Information */
	v86.es = VTOPSEG(buf);
	v86.edi = VTOPOFF(buf);
	v86int();
	return (v86.eax & 0xffff);
}

/* Function 15h BL=01h - Read EDID */
static int
biosvbe_ddc_read_edid(int blockno, void *buf)
{
	v86.ctl = V86_FLAGS;
	v86.addr = 0x10;
	v86.eax = 0x4f15;	/* display identification extensions */
	v86.ebx = 1;		/* read EDID */
	v86.ecx = 0;		/* controller unit number (00h = primary) */
	v86.edx = blockno;
	v86.es = VTOPSEG(buf);
	v86.edi = VTOPOFF(buf);
	v86int();
	return (v86.eax & 0xffff);
}

static int
vbe_mode_is_supported(struct modeinfoblock *mi)
{
	if ((mi->ModeAttributes & 0x01) == 0)
		return (0);	/* mode not supported by hardware */
	if ((mi->ModeAttributes & 0x08) == 0)
		return (0);	/* linear fb not available */
	if ((mi->ModeAttributes & 0x10) == 0)
		return (0);	/* text mode */
	if (mi->NumberOfPlanes != 1)
		return (0);	/* planar mode not supported */
	if (mi->MemoryModel != 0x04 /* Packed pixel */ &&
	    mi->MemoryModel != 0x06 /* Direct Color */)
		return (0);	/* unsupported pixel format */
	return (1);
}

static bool
vbe_check(void)
{

	if (vbe == NULL) {
		printf("VBE not available\n");
		return (false);
	}
	return (true);
}

static int
mode_set(struct env_var *ev, int flags __unused, const void *value)
{
	int mode;

	if (strcmp(ev->ev_name, "screen.textmode") == 0) {
		unsigned long v;
		char *end;

		if (value == NULL)
			return (0);
		errno = 0;
		v = strtoul(value, &end, 0);
		if (errno != 0 || *(char *)value == '\0' || *end != '\0' ||
		    (v != 0 && v != 1))
			return (EINVAL);
		env_setenv("screen.textmode", EV_VOLATILE | EV_NOHOOK,
		    value, NULL, NULL);
		if (v == 1) {
			reset_font_flags();
			bios_text_font(true);
			bios_set_text_mode(VGA_TEXT_MODE);
			(void) cons_update_mode(false);
			return (0);
		}
	} else if (strcmp(ev->ev_name, "vbe_max_resolution") == 0) {
		env_setenv("vbe_max_resolution", EV_VOLATILE | EV_NOHOOK,
		    value, NULL, NULL);
	} else {
		return (EINVAL);
	}

	mode = vbe_default_mode();
	if (gfx_state.tg_mode != mode) {
		reset_font_flags();
		bios_text_font(false);
		vbe_set_mode(mode);
		cons_update_mode(true);
	}
	return (0);
}

static void *
vbe_farptr(uint32_t farptr)
{
	return (PTOV((((farptr & 0xffff0000) >> 12) + (farptr & 0xffff))));
}

void
vbe_init(void)
{
	uint16_t *p, *ml;

	/* First set FB for text mode. */
	gfx_state.tg_fb_type = FB_TEXT;
	gfx_state.tg_fb.fb_height = TEXT_ROWS;
	gfx_state.tg_fb.fb_width = TEXT_COLS;
	gfx_state.tg_ctype = CT_INDEXED;
	gfx_state.tg_mode = 3;

	env_setenv("screen.textmode", EV_VOLATILE, "1", mode_set,
	    env_nounset);
	env_setenv("vbe_max_resolution", EV_VOLATILE, NULL, mode_set,
	    env_nounset);

	if (vbe == NULL) {
		vbe = malloc(sizeof(*vbe));
		if (vbe == NULL)
			return;
	}

	if (vbe_mode == NULL) {
		vbe_mode = malloc(sizeof(*vbe_mode));
		if (vbe_mode == NULL) {
			free(vbe);
			vbe = NULL;
		}
	}

	if (biosvbe_info(vbe) != VBE_SUCCESS) {
		free(vbe);
		vbe = NULL;
		free(vbe_mode);
		vbe_mode = NULL;
		return;
	}

	/*
	 * Copy mode list. We must do this because some systems do
	 * corrupt the provided list (vbox 6.1 is one example).
	 */
	p = ml = vbe_farptr(vbe->VideoModePtr);
	while(*p++ != 0xFFFF)
		;

	vbe_mode_list_size = (uintptr_t)p - (uintptr_t)ml;

	/*
	 * Since vbe_init() is used only once at very start of the loader,
	 * we assume malloc will not fail there, but in case it does,
	 * we point vbe_mode_list to memory pointed by VideoModePtr.
	 */
	vbe_mode_list = malloc(vbe_mode_list_size);
	if (vbe_mode_list == NULL)
		vbe_mode_list = ml;
	else
		bcopy(ml, vbe_mode_list, vbe_mode_list_size);

	/* reset VideoModePtr, to make sure, we only do use vbe_mode_list. */
	vbe->VideoModePtr = 0;

	/* vbe_set_mode() will set up the rest. */
}

bool
vbe_available(void)
{
	return (gfx_state.tg_fb_type == FB_VBE);
}

int
vbe_set_palette(const struct paletteentry *entry, size_t slot)
{
	struct paletteentry pe;
	int mode, ret;

	if (!vbe_check() || (vbe->Capabilities & VBE_CAP_DAC8) == 0)
		return (1);

	if (gfx_state.tg_ctype != CT_INDEXED) {
		return (1);
	}

	pe.Blue = entry->Blue;
	pe.Green = entry->Green;
	pe.Red = entry->Red;
	pe.Reserved = entry->Reserved;

	if (vbe->Capabilities & VBE_CAP_SNOW)
		mode = 0x80;
	else
		mode = 0;

	ret = biosvbe_palette_data(mode, slot, &pe);

	return (ret == VBE_SUCCESS ? 0 : 1);
}

int
vbe_get_mode(void)
{
	return (gfx_state.tg_mode);
}

int
vbe_set_mode(int modenum)
{
	struct modeinfoblock mi;
	int bpp, ret;

	if (!vbe_check())
		return (1);

	ret = biosvbe_get_mode_info(modenum, &mi);
	if (VBE_ERROR(ret)) {
		printf("mode 0x%x invalid\n", modenum);
		return (1);
	}

	if (!vbe_mode_is_supported(&mi)) {
		printf("mode 0x%x not supported\n", modenum);
		return (1);
	}

	/* calculate bytes per pixel */
	switch (mi.BitsPerPixel) {
	case 32:
	case 24:
	case 16:
	case 15:
	case 8:
		break;
	default:
		printf("BitsPerPixel %d is not supported\n", mi.BitsPerPixel);
		return (1);
	}

	ret = biosvbe_set_mode(modenum, NULL);
	if (VBE_ERROR(ret)) {
		printf("mode 0x%x could not be set\n", modenum);
		return (1);
	}

	gfx_state.tg_mode = modenum;
	gfx_state.tg_fb_type = FB_VBE;
	/* make sure we have current MI in vbestate */
	memcpy(vbe_mode, &mi, sizeof (*vbe_mode));

	gfx_state.tg_fb.fb_addr = (uint64_t)mi.PhysBasePtr & 0xffffffff;
	gfx_state.tg_fb.fb_height = mi.YResolution;
	gfx_state.tg_fb.fb_width = mi.XResolution;
	gfx_state.tg_fb.fb_bpp = mi.BitsPerPixel;

	free(gfx_state.tg_shadow_fb);
	gfx_state.tg_shadow_fb = malloc(mi.YResolution * mi.XResolution *
	    sizeof(struct paletteentry));

	/* Bytes per pixel */
	bpp = roundup2(mi.BitsPerPixel, NBBY) / NBBY;

	/* vbe_mode_is_supported() excludes the rest */
	switch (mi.MemoryModel) {
	case 0x4:
		gfx_state.tg_ctype = CT_INDEXED;
		break;
	case 0x6:
		gfx_state.tg_ctype = CT_RGB;
		break;
	}

#define	COLOR_MASK(size, pos) (((1 << size) - 1) << pos)
	if (gfx_state.tg_ctype == CT_INDEXED) {
		gfx_state.tg_fb.fb_mask_red = COLOR_MASK(palette_format, 16);
		gfx_state.tg_fb.fb_mask_green = COLOR_MASK(palette_format, 8);
		gfx_state.tg_fb.fb_mask_blue = COLOR_MASK(palette_format, 0);
	} else if (vbe->VbeVersion >= 0x300) {
		gfx_state.tg_fb.fb_mask_red =
		    COLOR_MASK(mi.LinRedMaskSize, mi.LinRedFieldPosition);
		gfx_state.tg_fb.fb_mask_green =
		    COLOR_MASK(mi.LinGreenMaskSize, mi.LinGreenFieldPosition);
		gfx_state.tg_fb.fb_mask_blue =
		    COLOR_MASK(mi.LinBlueMaskSize, mi.LinBlueFieldPosition);
	} else {
		gfx_state.tg_fb.fb_mask_red =
		    COLOR_MASK(mi.RedMaskSize, mi.RedFieldPosition);
		gfx_state.tg_fb.fb_mask_green =
		    COLOR_MASK(mi.GreenMaskSize, mi.GreenFieldPosition);
		gfx_state.tg_fb.fb_mask_blue =
		    COLOR_MASK(mi.BlueMaskSize, mi.BlueFieldPosition);
	}
	gfx_state.tg_fb.fb_mask_reserved = ~(gfx_state.tg_fb.fb_mask_red |
	    gfx_state.tg_fb.fb_mask_green |
	    gfx_state.tg_fb.fb_mask_blue);

	if (vbe->VbeVersion >= 0x300)
		gfx_state.tg_fb.fb_stride = mi.LinBytesPerScanLine / bpp;
	else
		gfx_state.tg_fb.fb_stride = mi.BytesPerScanLine / bpp;

	gfx_state.tg_fb.fb_size = mi.YResolution * gfx_state.tg_fb.fb_stride *
	    bpp;

	return (0);
}

/*
 * Verify existence of mode number or find mode by
 * dimensions. If depth is not given, walk values 32, 24, 16, 8.
 */
static int
vbe_find_mode_xydm(int x, int y, int depth, int m)
{
	struct modeinfoblock mi;
	uint16_t *farptr;
	uint16_t mode;
	int idx, nentries, i;

	memset(vbe, 0, sizeof (*vbe));
	if (biosvbe_info(vbe) != VBE_SUCCESS)
		return (0);

	if (m != -1)
		i = 8;
	else if (depth == -1)
		i = 32;
	else
		i = depth;

	nentries = vbe_mode_list_size / sizeof(*vbe_mode_list);
	while (i > 0) {
		for (idx = 0; idx < nentries; idx++) {
			mode = vbe_mode_list[idx];
			if (mode == 0xffff)
				break;

			if (biosvbe_get_mode_info(mode, &mi) != VBE_SUCCESS) {
				continue;
			}

			/* we only care about linear modes here */
			if (vbe_mode_is_supported(&mi) == 0)
				continue;

			if (m != -1) {
				if (m == mode)
					return (mode);
				else
					continue;
			}

			if (mi.XResolution == x &&
			    mi.YResolution == y &&
			    mi.BitsPerPixel == i)
				return (mode);
		}
		if (depth != -1)
			break;

		i -= 8;
	}

	return (0);
}

static int
vbe_find_mode(char *str)
{
	int x, y, depth;

	if (!gfx_parse_mode_str(str, &x, &y, &depth))
		return (0);

	return (vbe_find_mode_xydm(x, y, depth, -1));
}

static void
vbe_dump_mode(int modenum, struct modeinfoblock *mi)
{
	printf("0x%x=%dx%dx%d", modenum,
	    mi->XResolution, mi->YResolution, mi->BitsPerPixel);
}

static bool
vbe_get_edid(edid_res_list_t *res)
{
	struct vesa_edid_info *edidp;
	const uint8_t magic[] = EDID_MAGIC;
	int ddc_caps;
	bool ret = false;

	if (edid_info != NULL)
		return (gfx_get_edid_resolution(edid_info, res));

	ddc_caps = biosvbe_ddc_caps();
	if (ddc_caps == 0) {
		return (ret);
	}

	edidp = bio_alloc(sizeof(*edidp));
	if (edidp == NULL)
		return (ret);
	memset(edidp, 0, sizeof(*edidp));

	if (VBE_ERROR(biosvbe_ddc_read_edid(0, edidp)))
		goto done;

	if (memcmp(edidp, magic, sizeof(magic)) != 0)
		goto done;

	/* Unknown EDID version. */
	if (edidp->header.version != 1)
		goto done;

	ret = gfx_get_edid_resolution(edidp, res);
	edid_info = malloc(sizeof(*edid_info));
	if (edid_info != NULL)
		memcpy(edid_info, edidp, sizeof (*edid_info));
done:
	bio_free(edidp, sizeof(*edidp));
	return (ret);
}

static bool
vbe_get_flatpanel(uint32_t *pwidth, uint32_t *pheight)
{
	struct vesa_flat_panel_info *fp_info;
	bool ret = false;

	fp_info = bio_alloc(sizeof (*fp_info));
	if (fp_info == NULL)
		return (ret);
	memset(fp_info, 0, sizeof (*fp_info));

	if (VBE_ERROR(biosvbe_ddc_read_flat_panel_info(fp_info)))
		goto done;

	*pwidth = fp_info->HSize;
	*pheight = fp_info->VSize;
	ret = true;

done:
	bio_free(fp_info, sizeof (*fp_info));
	return (ret);
}

static void
vbe_print_memory(unsigned vmem)
{
	char unit = 'K';

	vmem /= 1024;
	if (vmem >= 10240000) {
		vmem /= 1048576;
		unit = 'G';
	} else if (vmem >= 10000) {
		vmem /= 1024;
		unit = 'M';
	}
	printf("Total memory: %u%cB\n", vmem, unit);
}

static void
vbe_print_vbe_info(struct vbeinfoblock *vbep)
{
	char *oemstring = "";
	char *oemvendor = "", *oemproductname = "", *oemproductrev = "";

	if (vbep->OemStringPtr != 0)
		oemstring = vbe_farptr(vbep->OemStringPtr);

	if (vbep->OemVendorNamePtr != 0)
		oemvendor = vbe_farptr(vbep->OemVendorNamePtr);

	if (vbep->OemProductNamePtr != 0)
		oemproductname = vbe_farptr(vbep->OemProductNamePtr);

	if (vbep->OemProductRevPtr != 0)
		oemproductrev = vbe_farptr(vbep->OemProductRevPtr);

	printf("VESA VBE Version %d.%d\n%s\n", vbep->VbeVersion >> 8,
	    vbep->VbeVersion & 0xF, oemstring);

	if (vbep->OemSoftwareRev != 0) {
		printf("OEM Version %d.%d, %s (%s, %s)\n",
		    vbep->OemSoftwareRev >> 8, vbep->OemSoftwareRev & 0xF,
		    oemvendor, oemproductname, oemproductrev);
	}
	vbe_print_memory(vbep->TotalMemory << 16);
	printf("Number of Image Pages: %d\n", vbe_mode->LinNumberOfImagePages);
}

/* List available modes, filter by depth. If depth is -1, list all. */
void
vbe_modelist(int depth)
{
	struct modeinfoblock mi;
	uint16_t mode;
	int nmodes, idx, nentries;
	int ddc_caps;
	uint32_t width, height;
	bool edid = false;
	edid_res_list_t res;
	struct resolution *rp;

	if (!vbe_check())
		return;

	ddc_caps = biosvbe_ddc_caps();
	if (ddc_caps & 3) {
		printf("DDC");
		if (ddc_caps & 1)
			printf(" [DDC1]");
		if (ddc_caps & 2)
			printf(" [DDC2]");

		TAILQ_INIT(&res);
		edid = vbe_get_edid(&res);
		if (edid) {
			printf(": EDID");
			while ((rp = TAILQ_FIRST(&res)) != NULL) {
				printf(" %dx%d", rp->width, rp->height);
				TAILQ_REMOVE(&res, rp, next);
				free(rp);
			}
			printf("\n");
		} else {
			printf(": no EDID information\n");
		}
	}
	if (!edid)
		if (vbe_get_flatpanel(&width, &height))
			printf(": Panel %dx%d\n", width, height);

	nmodes = 0;
	memset(vbe, 0, sizeof (*vbe));
	memcpy(vbe->VbeSignature, "VBE2", 4);
	if (biosvbe_info(vbe) != VBE_SUCCESS)
		goto done;
	if (memcmp(vbe->VbeSignature, "VESA", 4) != 0)
		goto done;

	vbe_print_vbe_info(vbe);
	printf("Modes: ");

	nentries = vbe_mode_list_size / sizeof(*vbe_mode_list);
	for (idx = 0; idx < nentries; idx++) {
		mode = vbe_mode_list[idx];
		if (mode == 0xffff)
			break;

		if (biosvbe_get_mode_info(mode, &mi) != VBE_SUCCESS)
			continue;

		/* we only care about linear modes here */
		if (vbe_mode_is_supported(&mi) == 0)
			continue;

		/* apply requested filter */
		if (depth != -1 && mi.BitsPerPixel != depth)
			continue;

		if (nmodes % 4 == 0)
			printf("\n");
		else
			printf("  ");

		vbe_dump_mode(mode, &mi);
		nmodes++;
	}

done:
	if (nmodes == 0)
		printf("none found");
	printf("\n");
}

static void
vbe_print_mode(bool verbose __unused)
{
	int nc, mode, i, rc;

	nc = NCOLORS;

	memset(vbe, 0, sizeof (*vbe));
	if (biosvbe_info(vbe) != VBE_SUCCESS)
		return;

	vbe_print_vbe_info(vbe);

	if (biosvbe_get_mode(&mode) != VBE_SUCCESS) {
		printf("Error getting current VBE mode\n");
		return;
	}

	if (biosvbe_get_mode_info(mode, vbe_mode) != VBE_SUCCESS ||
	    vbe_mode_is_supported(vbe_mode) == 0) {
		printf("VBE mode (0x%x) is not framebuffer mode\n", mode);
		return;
	}

	printf("\nCurrent VBE mode: ");
	vbe_dump_mode(mode, vbe_mode);
	printf("\n");

	printf("%ux%ux%u, stride=%u\n",
	    gfx_state.tg_fb.fb_width,
	    gfx_state.tg_fb.fb_height,
	    gfx_state.tg_fb.fb_bpp,
	    gfx_state.tg_fb.fb_stride *
	    (roundup2(gfx_state.tg_fb.fb_bpp, NBBY) / NBBY));
	printf("    frame buffer: address=%jx, size=%jx\n",
	    (uintmax_t)gfx_state.tg_fb.fb_addr,
	    (uintmax_t)gfx_state.tg_fb.fb_size);

	if (vbe_mode->MemoryModel == 0x6) {
		printf("    color mask: R=%08x, G=%08x, B=%08x\n",
		    gfx_state.tg_fb.fb_mask_red,
		    gfx_state.tg_fb.fb_mask_green,
		    gfx_state.tg_fb.fb_mask_blue);
		pager_open();
		for (i = 0; i < nc; i++) {
			printf("%d: R=%02x, G=%02x, B=%02x %08x", i,
			    (cmap[i] & gfx_state.tg_fb.fb_mask_red) >>
			    ffs(gfx_state.tg_fb.fb_mask_red) - 1,
			    (cmap[i] & gfx_state.tg_fb.fb_mask_green) >>
			    ffs(gfx_state.tg_fb.fb_mask_green) - 1,
			    (cmap[i] & gfx_state.tg_fb.fb_mask_blue) >>
			    ffs(gfx_state.tg_fb.fb_mask_blue) - 1, cmap[i]);
			if (pager_output("\n") != 0)
				break;
		}
		pager_close();
		return;
	}

	mode = 1;	/* get DAC palette width */
	rc = biosvbe_palette_format(&mode);
	if (rc != VBE_SUCCESS)
		return;

	printf("    palette format: %x bits per primary\n", mode);
	if (pe8 == NULL)
		return;

	pager_open();
	for (i = 0; i < nc; i++) {
		printf("%d: R=%02x, G=%02x, B=%02x", i,
		    pe8[i].Red, pe8[i].Green, pe8[i].Blue);
		if (pager_output("\n") != 0)
			break;
	}
	pager_close();
}

/*
 * Try EDID preferred mode, if EDID or the suggested mode is not available,
 * then try flat panel information.
 * Fall back to VBE_DEFAULT_MODE.
 */
int
vbe_default_mode(void)
{
	edid_res_list_t res;
	struct resolution *rp;
	int modenum;
	uint32_t width, height;

	modenum = 0;
	vbe_get_max_resolution(&width, &height);
	if (width != 0 && height != 0)
		modenum = vbe_find_mode_xydm(width, height, -1, -1);

	TAILQ_INIT(&res);
	if (vbe_get_edid(&res)) {
		while ((rp = TAILQ_FIRST(&res)) != NULL) {
			if (modenum == 0) {
				modenum = vbe_find_mode_xydm(
				    rp->width, rp->height, -1, -1);
			}
			TAILQ_REMOVE(&res, rp, next);
			free(rp);
		}
	}

	if (modenum == 0 &&
	    vbe_get_flatpanel(&width, &height)) {
		modenum = vbe_find_mode_xydm(width, height, -1, -1);
	}

	/* Still no mode? Fall back to default. */
	if (modenum == 0)
		modenum = vbe_find_mode(VBE_DEFAULT_MODE);
	return (modenum);
}

COMMAND_SET(vbe, "vbe", "vesa framebuffer mode management", command_vesa);

int
command_vesa(int argc, char *argv[])
{
	char *arg, *cp;
	int modenum = -1, n;

	if (!vbe_check())
		return (CMD_OK);

	if (argc < 2)
		goto usage;

	if (strcmp(argv[1], "list") == 0) {
		n = -1;
		if (argc != 2 && argc != 3)
			goto usage;

		if (argc == 3) {
			arg = argv[2];
			errno = 0;
			n = strtoul(arg, &cp, 0);
			if (errno != 0 || *arg == '\0' || cp[0] != '\0') {
				snprintf(command_errbuf,
				    sizeof (command_errbuf),
				    "depth should be an integer");
				return (CMD_ERROR);
			}
		}
		vbe_modelist(n);
		return (CMD_OK);
	}

	if (strcmp(argv[1], "get") == 0) {
		bool verbose = false;

		if (argc != 2) {
			if (argc > 3 || strcmp(argv[2], "-v") != 0)
				goto usage;
			verbose = true;
		}
		vbe_print_mode(verbose);
		return (CMD_OK);
	}

	if (strcmp(argv[1], "off") == 0) {
		if (argc != 2)
			goto usage;

		if (gfx_state.tg_mode == VGA_TEXT_MODE)
			return (CMD_OK);

		reset_font_flags();
		bios_text_font(true);
		bios_set_text_mode(VGA_TEXT_MODE);
		cons_update_mode(false);
		return (CMD_OK);
	}

	if (strcmp(argv[1], "on") == 0) {
		if (argc != 2)
			goto usage;

		modenum = vbe_default_mode();
		if (modenum == 0) {
			snprintf(command_errbuf, sizeof (command_errbuf),
			    "%s: no suitable VBE mode number found", argv[0]);
			return (CMD_ERROR);
		}
	} else if (strcmp(argv[1], "set") == 0) {
		if (argc != 3)
			goto usage;

		if (strncmp(argv[2], "0x", 2) == 0) {
			arg = argv[2];
			errno = 0;
			n = strtoul(arg, &cp, 0);
			if (errno != 0 || *arg == '\0' || cp[0] != '\0') {
				snprintf(command_errbuf,
				    sizeof (command_errbuf),
				    "mode should be an integer");
				return (CMD_ERROR);
			}
			modenum = vbe_find_mode_xydm(0, 0, 0, n);
		} else if (strchr(argv[2], 'x') != NULL) {
			modenum = vbe_find_mode(argv[2]);
		}
	} else {
		goto usage;
	}

	if (modenum == 0) {
		snprintf(command_errbuf, sizeof (command_errbuf),
		    "%s: mode %s not supported by firmware\n",
		    argv[0], argv[2]);
		return (CMD_ERROR);
	}

	if (modenum >= VESA_MODE_BASE) {
		if (gfx_state.tg_mode != modenum) {
			reset_font_flags();
			bios_text_font(false);
			vbe_set_mode(modenum);
			cons_update_mode(true);
		}
		return (CMD_OK);
	} else {
		snprintf(command_errbuf, sizeof (command_errbuf),
		    "%s: mode %s is not framebuffer mode\n", argv[0], argv[2]);
		return (CMD_ERROR);
	}

usage:
	snprintf(command_errbuf, sizeof (command_errbuf),
	    "usage: %s on | off | get | list [depth] | "
	    "set <display or VBE mode number>", argv[0]);
	return (CMD_ERROR);
}
