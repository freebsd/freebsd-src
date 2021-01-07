/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright 2020 Toomas Soome
 * Copyright 2019 OmniOS Community Edition (OmniOSce) Association.
 * Copyright 2020 RackTop Systems, Inc.
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
#include <sys/param.h>
#include <stand.h>
#include <teken.h>
#include <gfx_fb.h>
#include <sys/font.h>
#include <sys/stdint.h>
#include <sys/endian.h>
#include <pnglite.h>
#include <bootstrap.h>
#include <lz4.h>
#if defined(EFI)
#include <efi.h>
#include <efilib.h>
#else
#include <vbe.h>
#endif

/* VGA text mode does use bold font. */
#if !defined(VGA_8X16_FONT)
#define	VGA_8X16_FONT		"/boot/fonts/8x16b.fnt"
#endif
#if !defined(DEFAULT_8X16_FONT)
#define	DEFAULT_8X16_FONT	"/boot/fonts/8x16.fnt"
#endif

/*
 * Must be sorted by font size in descending order
 */
font_list_t fonts = STAILQ_HEAD_INITIALIZER(fonts);

#define	DEFAULT_FONT_DATA	font_data_8x16
extern vt_font_bitmap_data_t	font_data_8x16;
teken_gfx_t gfx_state = { 0 };

static struct {
	unsigned char r;	/* Red percentage value. */
	unsigned char g;	/* Green percentage value. */
	unsigned char b;	/* Blue percentage value. */
} color_def[NCOLORS] = {
	{0,	0,	0},	/* black */
	{50,	0,	0},	/* dark red */
	{0,	50,	0},	/* dark green */
	{77,	63,	0},	/* dark yellow */
	{20,	40,	64},	/* dark blue */
	{50,	0,	50},	/* dark magenta */
	{0,	50,	50},	/* dark cyan */
	{75,	75,	75},	/* light gray */

	{18,	20,	21},	/* dark gray */
	{100,	0,	0},	/* light red */
	{0,	100,	0},	/* light green */
	{100,	100,	0},	/* light yellow */
	{45,	62,	81},	/* light blue */
	{100,	0,	100},	/* light magenta */
	{0,	100,	100},	/* light cyan */
	{100,	100,	100},	/* white */
};
uint32_t cmap[NCMAP];

/*
 * Between console's palette and VGA's one:
 *  - blue and red are swapped (1 <-> 4)
 *  - yellow and cyan are swapped (3 <-> 6)
 */
const int cons_to_vga_colors[NCOLORS] = {
	0,  4,  2,  6,  1,  5,  3,  7,
	8, 12, 10, 14,  9, 13, 11, 15
};

static const int vga_to_cons_colors[NCOLORS] = {
	0,  1,  2,  3,  4,  5,  6,  7,
	8,  9, 10, 11,  12, 13, 14, 15
};

struct text_pixel *screen_buffer;
#if defined(EFI)
static EFI_GRAPHICS_OUTPUT_BLT_PIXEL *GlyphBuffer;
#else
static struct paletteentry *GlyphBuffer;
#endif
static size_t GlyphBufferSize;

static bool insert_font(char *, FONT_FLAGS);
static int font_set(struct env_var *, int, const void *);
static void * allocate_glyphbuffer(uint32_t, uint32_t);
static void gfx_fb_cursor_draw(teken_gfx_t *, const teken_pos_t *, bool);

/*
 * Initialize gfx framework.
 */
void
gfx_framework_init(void)
{
	/*
	 * Setup font list to have builtin font.
	 */
	(void) insert_font(NULL, FONT_BUILTIN);
}

static uint8_t *
gfx_get_fb_address(void)
{
	return (ptov((uint32_t)gfx_state.tg_fb.fb_addr));
}

/*
 * Utility function to parse gfx mode line strings.
 */
bool
gfx_parse_mode_str(char *str, int *x, int *y, int *depth)
{
	char *p, *end;

	errno = 0;
	p = str;
	*x = strtoul(p, &end, 0);
	if (*x == 0 || errno != 0)
		return (false);
	if (*end != 'x')
		return (false);
	p = end + 1;
	*y = strtoul(p, &end, 0);
	if (*y == 0 || errno != 0)
		return (false);
	if (*end != 'x') {
		*depth = -1;    /* auto select */
	} else {
		p = end + 1;
		*depth = strtoul(p, &end, 0);
		if (*depth == 0 || errno != 0 || *end != '\0')
			return (false);
	}

	return (true);
}

static uint32_t
rgb_color_map(uint8_t index, uint32_t rmax, int roffset,
    uint32_t gmax, int goffset, uint32_t bmax, int boffset)
{
	uint32_t color, code, gray, level;

	if (index < NCOLORS) {
#define	CF(_f, _i) ((_f ## max * color_def[(_i)]._f / 100) << _f ## offset)
		return (CF(r, index) | CF(g, index) | CF(b, index));
#undef  CF
        }

#define	CF(_f, _c) ((_f ## max & _c) << _f ## offset)
        /* 6x6x6 color cube */
        if (index > 15 && index < 232) {
                uint32_t red, green, blue;

                for (red = 0; red < 6; red++) {
                        for (green = 0; green < 6; green++) {
                                for (blue = 0; blue < 6; blue++) {
                                        code = 16 + (red * 36) +
                                            (green * 6) + blue;
                                        if (code != index)
                                                continue;
                                        red = red ? (red * 40 + 55) : 0;
                                        green = green ? (green * 40 + 55) : 0;
                                        blue = blue ? (blue * 40 + 55) : 0;
                                        color = CF(r, red);
					color |= CF(g, green);
					color |= CF(b, blue);
					return (color);
                                }
                        }
                }
        }

        /* colors 232-255 are a grayscale ramp */
        for (gray = 0; gray < 24; gray++) {
                level = (gray * 10) + 8;
                code = 232 + gray;
                if (code == index)
                        break;
        }
        return (CF(r, level) | CF(g, level) | CF(b, level));
#undef  CF
}

/*
 * Support for color mapping.
 * For 8, 24 and 32 bit depth, use mask size 8.
 * 15/16 bit depth needs to use mask size from mode,
 * or we will lose color information from 32-bit to 15/16 bit translation.
 */
uint32_t
gfx_fb_color_map(uint8_t index)
{
	int rmask, gmask, bmask;
	int roff, goff, boff, bpp;

	roff = ffs(gfx_state.tg_fb.fb_mask_red) - 1;
        goff = ffs(gfx_state.tg_fb.fb_mask_green) - 1;
        boff = ffs(gfx_state.tg_fb.fb_mask_blue) - 1;
	bpp = roundup2(gfx_state.tg_fb.fb_bpp, 8) >> 3;

	if (bpp == 2)
		rmask = gfx_state.tg_fb.fb_mask_red >> roff;
	else
		rmask = 0xff;

	if (bpp == 2)
		gmask = gfx_state.tg_fb.fb_mask_green >> goff;
	else
		gmask = 0xff;

	if (bpp == 2)
		bmask = gfx_state.tg_fb.fb_mask_blue >> boff;
	else
		bmask = 0xff;

	return (rgb_color_map(index, rmask, 16, gmask, 8, bmask, 0));
}

/* Get indexed color */
static uint8_t
rgb_to_color_index(uint8_t r, uint8_t g, uint8_t b)
{
#if !defined(EFI)
	uint32_t color, best, dist, k;
	int diff;

	color = 0;
	best = NCMAP * NCMAP * NCMAP;
	for (k = 0; k < NCMAP; k++) {
		diff = r - pe8[k].Red;
		dist = diff * diff;
		diff = g - pe8[k].Green;
		dist += diff * diff;
		diff = b - pe8[k].Blue;
		dist += diff * diff;

		if (dist == 0)
			break;
		if (dist < best) {
			color = k;
			best = dist;
		}
	}
	if (k == NCMAP)
		k = color;
	return (k);
#else
	(void) r;
	(void) g;
	(void) b;
	return (0);
#endif
}

int
generate_cons_palette(uint32_t *palette, int format,
    uint32_t rmax, int roffset, uint32_t gmax, int goffset,
    uint32_t bmax, int boffset)
{
	int i;

	switch (format) {
	case COLOR_FORMAT_VGA:
		for (i = 0; i < NCOLORS; i++)
			palette[i] = cons_to_vga_colors[i];
		for (; i < NCMAP; i++)
			palette[i] = i;
		break;
	case COLOR_FORMAT_RGB:
		for (i = 0; i < NCMAP; i++)
			palette[i] = rgb_color_map(i, rmax, roffset,
			    gmax, goffset, bmax, boffset);
		break;
	default:
		return (ENODEV);
	}

	return (0);
}

static void
gfx_mem_wr1(uint8_t *base, size_t size, uint32_t o, uint8_t v)
{

	if (o >= size)
		return;
	*(uint8_t *)(base + o) = v;
}

static void
gfx_mem_wr2(uint8_t *base, size_t size, uint32_t o, uint16_t v)
{

	if (o >= size)
		return;
	*(uint16_t *)(base + o) = v;
}

static void
gfx_mem_wr4(uint8_t *base, size_t size, uint32_t o, uint32_t v)
{

	if (o >= size)
		return;
	*(uint32_t *)(base + o) = v;
}

/* Our GFX Block transfer toolkit. */
static int gfxfb_blt_fill(void *BltBuffer,
    uint32_t DestinationX, uint32_t DestinationY,
    uint32_t Width, uint32_t Height)
{
#if defined(EFI)
	EFI_GRAPHICS_OUTPUT_BLT_PIXEL *p;
#else
	struct paletteentry *p;
#endif
	uint32_t data, bpp, pitch, y, x;
	int roff, goff, boff;
	size_t size;
	off_t off;
	uint8_t *destination;

	if (BltBuffer == NULL)
		return (EINVAL);

	if (DestinationY + Height > gfx_state.tg_fb.fb_height)
		return (EINVAL);

	if (DestinationX + Width > gfx_state.tg_fb.fb_width)
		return (EINVAL);

	if (Width == 0 || Height == 0)
		return (EINVAL);

	p = BltBuffer;
	roff = ffs(gfx_state.tg_fb.fb_mask_red) - 1;
	goff = ffs(gfx_state.tg_fb.fb_mask_green) - 1;
	boff = ffs(gfx_state.tg_fb.fb_mask_blue) - 1;

	if (gfx_state.tg_fb.fb_bpp == 8) {
		data = rgb_to_color_index(p->Red, p->Green, p->Blue);
	} else {
		data = (p->Red &
		    (gfx_state.tg_fb.fb_mask_red >> roff)) << roff;
		data |= (p->Green &
		    (gfx_state.tg_fb.fb_mask_green >> goff)) << goff;
		data |= (p->Blue &
		    (gfx_state.tg_fb.fb_mask_blue >> boff)) << boff;
	}

	bpp = roundup2(gfx_state.tg_fb.fb_bpp, 8) >> 3;
	pitch = gfx_state.tg_fb.fb_stride * bpp;
	destination = gfx_get_fb_address();
	size = gfx_state.tg_fb.fb_size;

	for (y = DestinationY; y < Height + DestinationY; y++) {
		off = y * pitch + DestinationX * bpp;
		for (x = 0; x < Width; x++) {
			switch (bpp) {
			case 1:
				gfx_mem_wr1(destination, size, off,
				    (data < NCOLORS) ?
				    cons_to_vga_colors[data] : data);
				break;
			case 2:
				gfx_mem_wr2(destination, size, off, data);
				break;
			case 3:
				gfx_mem_wr1(destination, size, off,
				    (data >> 16) & 0xff);
				gfx_mem_wr1(destination, size, off + 1,
				    (data >> 8) & 0xff);
				gfx_mem_wr1(destination, size, off + 2,
				    data & 0xff);
				break;
			case 4:
				gfx_mem_wr4(destination, size, off, data);
				break;
			}
			off += bpp;
		}
	}

	return (0);
}

static int
gfxfb_blt_video_to_buffer(void *BltBuffer, uint32_t SourceX, uint32_t SourceY,
    uint32_t DestinationX, uint32_t DestinationY,
    uint32_t Width, uint32_t Height, uint32_t Delta)
{
#if defined(EFI)
	EFI_GRAPHICS_OUTPUT_BLT_PIXEL *p;
#else
	struct paletteentry *p;
#endif
	uint32_t x, sy, dy;
	uint32_t bpp, pitch, copybytes;
	off_t off;
	uint8_t *source, *destination, *buffer, *sb;
	uint8_t rm, rp, gm, gp, bm, bp;
	bool bgra;

	if (BltBuffer == NULL)
		return (EINVAL);

	if (SourceY + Height >
	    gfx_state.tg_fb.fb_height)
		return (EINVAL);

	if (SourceX + Width > gfx_state.tg_fb.fb_width)
		return (EINVAL);

	if (Width == 0 || Height == 0)
		return (EINVAL);

	if (Delta == 0)
		Delta = Width * sizeof (*p);

	bpp = roundup2(gfx_state.tg_fb.fb_bpp, 8) >> 3;
	pitch = gfx_state.tg_fb.fb_stride * bpp;

	copybytes = Width * bpp;

	rp = ffs(gfx_state.tg_fb.fb_mask_red) - 1;
	gp = ffs(gfx_state.tg_fb.fb_mask_green) - 1;
	bp = ffs(gfx_state.tg_fb.fb_mask_blue) - 1;
	rm = gfx_state.tg_fb.fb_mask_red >> rp;
	gm = gfx_state.tg_fb.fb_mask_green >> gp;
	bm = gfx_state.tg_fb.fb_mask_blue >> bp;

	/* If FB pixel format is BGRA, we can use direct copy. */
	bgra = bpp == 4 &&
	    ffs(rm) - 1 == 8 && rp == 16 &&
	    ffs(gm) - 1 == 8 && gp == 8 &&
	    ffs(bm) - 1 == 8 && bp == 0;

	if (bgra) {
		buffer = NULL;
	} else {
		buffer = malloc(copybytes);
		if (buffer == NULL)
			return (ENOMEM);
	}

	for (sy = SourceY, dy = DestinationY; dy < Height + DestinationY;
	    sy++, dy++) {
		off = sy * pitch + SourceX * bpp;
		source = gfx_get_fb_address() + off;

		if (bgra) {
			destination = (uint8_t *)BltBuffer + dy * Delta +
			    DestinationX * sizeof (*p);
		} else {
			destination = buffer;
		}

		bcopy(source, destination, copybytes);

		if (!bgra) {
			for (x = 0; x < Width; x++) {
				uint32_t c = 0;

				p = (void *)((uint8_t *)BltBuffer +
				    dy * Delta +
				    (DestinationX + x) * sizeof (*p));
				sb = buffer + x * bpp;
				switch (bpp) {
				case 1:
					c = *sb;
					break;
				case 2:
					c = *(uint16_t *)sb;
					break;
				case 3:
					c = sb[0] << 16 | sb[1] << 8 | sb[2];
					break;
				case 4:
					c = *(uint32_t *)sb;
					break;
				}

				if (bpp == 1) {
					*(uint32_t *)p = gfx_fb_color_map(
					    (c < 16) ?
					    vga_to_cons_colors[c] : c);
				} else {
					p->Red = (c >> rp) & rm;
					p->Green = (c >> gp) & gm;
					p->Blue = (c >> bp) & bm;
					p->Reserved = 0;
				}
			}
		}
	}

	free(buffer);
	return (0);
}

static int
gfxfb_blt_buffer_to_video(void *BltBuffer, uint32_t SourceX, uint32_t SourceY,
    uint32_t DestinationX, uint32_t DestinationY,
    uint32_t Width, uint32_t Height, uint32_t Delta)
{
#if defined(EFI)
	EFI_GRAPHICS_OUTPUT_BLT_PIXEL *p;
#else
	struct paletteentry *p;
#endif
	uint32_t x, sy, dy;
	uint32_t bpp, pitch, copybytes;
	off_t off;
	uint8_t *source, *destination, *buffer;
	uint8_t rm, rp, gm, gp, bm, bp;
	bool bgra;

	if (BltBuffer == NULL)
		return (EINVAL);

	if (DestinationY + Height >
	    gfx_state.tg_fb.fb_height)
		return (EINVAL);

	if (DestinationX + Width > gfx_state.tg_fb.fb_width)
		return (EINVAL);

	if (Width == 0 || Height == 0)
		return (EINVAL);

	if (Delta == 0)
		Delta = Width * sizeof (*p);

	bpp = roundup2(gfx_state.tg_fb.fb_bpp, 8) >> 3;
	pitch = gfx_state.tg_fb.fb_stride * bpp;

	copybytes = Width * bpp;

	rp = ffs(gfx_state.tg_fb.fb_mask_red) - 1;
	gp = ffs(gfx_state.tg_fb.fb_mask_green) - 1;
	bp = ffs(gfx_state.tg_fb.fb_mask_blue) - 1;
	rm = gfx_state.tg_fb.fb_mask_red >> rp;
	gm = gfx_state.tg_fb.fb_mask_green >> gp;
	bm = gfx_state.tg_fb.fb_mask_blue >> bp;

	/* If FB pixel format is BGRA, we can use direct copy. */
	bgra = bpp == 4 &&
	    ffs(rm) - 1 == 8 && rp == 16 &&
	    ffs(gm) - 1 == 8 && gp == 8 &&
	    ffs(bm) - 1 == 8 && bp == 0;

	if (bgra) {
		buffer = NULL;
	} else {
		buffer = malloc(copybytes);
		if (buffer == NULL)
			return (ENOMEM);
	}
	for (sy = SourceY, dy = DestinationY; sy < Height + SourceY;
	    sy++, dy++) {
		off = dy * pitch + DestinationX * bpp;
		destination = gfx_get_fb_address() + off;

		if (bgra) {
			source = (uint8_t *)BltBuffer + sy * Delta +
			    SourceX * sizeof (*p);
		} else {
			for (x = 0; x < Width; x++) {
				uint32_t c;

				p = (void *)((uint8_t *)BltBuffer +
				    sy * Delta +
				    (SourceX + x) * sizeof (*p));
				if (bpp == 1) {
					c = rgb_to_color_index(p->Red,
					    p->Green, p->Blue);
				} else {
					c = (p->Red & rm) << rp |
					    (p->Green & gm) << gp |
					    (p->Blue & bm) << bp;
				}
				off = x * bpp;
				switch (bpp) {
				case 1:
					gfx_mem_wr1(buffer, copybytes,
					    off, (c < 16) ?
					    cons_to_vga_colors[c] : c);
					break;
				case 2:
					gfx_mem_wr2(buffer, copybytes,
					    off, c);
					break;
				case 3:
					gfx_mem_wr1(buffer, copybytes,
					    off, (c >> 16) & 0xff);
					gfx_mem_wr1(buffer, copybytes,
					    off + 1, (c >> 8) & 0xff);
					gfx_mem_wr1(buffer, copybytes,
					    off + 2, c & 0xff);
					break;
				case 4:
					gfx_mem_wr4(buffer, copybytes,
					    x * bpp, c);
					break;
				}
			}
			source = buffer;
		}

		bcopy(source, destination, copybytes);
	}

	free(buffer);
	return (0);
}

static int
gfxfb_blt_video_to_video(uint32_t SourceX, uint32_t SourceY,
    uint32_t DestinationX, uint32_t DestinationY,
    uint32_t Width, uint32_t Height)
{
	uint32_t bpp, copybytes;
	int pitch;
	uint8_t *source, *destination;
	off_t off;

	if (SourceY + Height >
	    gfx_state.tg_fb.fb_height)
		return (EINVAL);

	if (SourceX + Width > gfx_state.tg_fb.fb_width)
		return (EINVAL);

	if (DestinationY + Height >
	    gfx_state.tg_fb.fb_height)
		return (EINVAL);

	if (DestinationX + Width > gfx_state.tg_fb.fb_width)
		return (EINVAL);

	if (Width == 0 || Height == 0)
		return (EINVAL);

	bpp = roundup2(gfx_state.tg_fb.fb_bpp, 8) >> 3;
	pitch = gfx_state.tg_fb.fb_stride * bpp;

	copybytes = Width * bpp;

	off = SourceY * pitch + SourceX * bpp;
	source = gfx_get_fb_address() + off;
	off = DestinationY * pitch + DestinationX * bpp;
	destination = gfx_get_fb_address() + off;

	if ((uintptr_t)destination > (uintptr_t)source) {
		source += Height * pitch;
		destination += Height * pitch;
		pitch = -pitch;
	}

	while (Height-- > 0) {
		bcopy(source, destination, copybytes);
		source += pitch;
		destination += pitch;
	}

	return (0);
}

int
gfxfb_blt(void *BltBuffer, GFXFB_BLT_OPERATION BltOperation,
    uint32_t SourceX, uint32_t SourceY,
    uint32_t DestinationX, uint32_t DestinationY,
    uint32_t Width, uint32_t Height, uint32_t Delta)
{
	int rv;
#if defined(EFI)
	EFI_STATUS status;
	EFI_GRAPHICS_OUTPUT *gop = gfx_state.tg_private;

	if (gop != NULL && (gop->Mode->Info->PixelFormat == PixelBltOnly ||
	    gfx_state.tg_fb.fb_addr == 0)) {
		switch (BltOperation) {
		case GfxFbBltVideoFill:
			status = gop->Blt(gop, BltBuffer, EfiBltVideoFill,
			    SourceX, SourceY, DestinationX, DestinationY,
			    Width, Height, Delta);
			break;

		case GfxFbBltVideoToBltBuffer:
			status = gop->Blt(gop, BltBuffer,
			    EfiBltVideoToBltBuffer,
			    SourceX, SourceY, DestinationX, DestinationY,
			    Width, Height, Delta);
			break;

		case GfxFbBltBufferToVideo:
			status = gop->Blt(gop, BltBuffer, EfiBltBufferToVideo,
			    SourceX, SourceY, DestinationX, DestinationY,
			    Width, Height, Delta);
			break;

		case GfxFbBltVideoToVideo:
			status = gop->Blt(gop, BltBuffer, EfiBltVideoToVideo,
			    SourceX, SourceY, DestinationX, DestinationY,
			    Width, Height, Delta);
			break;

		default:
			status = EFI_INVALID_PARAMETER;
			break;
		}

		switch (status) {
		case EFI_SUCCESS:
			rv = 0;
			break;

		case EFI_INVALID_PARAMETER:
			rv = EINVAL;
			break;

		case EFI_DEVICE_ERROR:
		default:
			rv = EIO;
			break;
		}

		return (rv);
	}
#endif

	switch (BltOperation) {
	case GfxFbBltVideoFill:
		rv = gfxfb_blt_fill(BltBuffer, DestinationX, DestinationY,
		    Width, Height);
		break;

	case GfxFbBltVideoToBltBuffer:
		rv = gfxfb_blt_video_to_buffer(BltBuffer, SourceX, SourceY,
		    DestinationX, DestinationY, Width, Height, Delta);
		break;

	case GfxFbBltBufferToVideo:
		rv = gfxfb_blt_buffer_to_video(BltBuffer, SourceX, SourceY,
		    DestinationX, DestinationY, Width, Height, Delta);
		break;

	case GfxFbBltVideoToVideo:
		rv = gfxfb_blt_video_to_video(SourceX, SourceY,
		    DestinationX, DestinationY, Width, Height);
		break;

	default:
		rv = EINVAL;
		break;
	}
	return (rv);
}

void
gfx_bitblt_bitmap(teken_gfx_t *state, const uint8_t *glyph,
    const teken_attr_t *a, uint32_t alpha, bool cursor)
{
	uint32_t width, height;
	uint32_t fgc, bgc, bpl, cc, o;
	int bpp, bit, byte;
	bool invert = false;

	bpp = 4;		/* We only generate BGRA */
	width = state->tg_font.vf_width;
	height = state->tg_font.vf_height;
	bpl = (width + 7) / 8;  /* Bytes per source line. */

	fgc = a->ta_fgcolor;
	bgc = a->ta_bgcolor;
	if (a->ta_format & TF_BOLD)
		fgc |= TC_LIGHT;
	if (a->ta_format & TF_BLINK)
		bgc |= TC_LIGHT;

	fgc = gfx_fb_color_map(fgc);
	bgc = gfx_fb_color_map(bgc);

	if (a->ta_format & TF_REVERSE)
		invert = !invert;
	if (cursor)
		invert = !invert;
	if (invert) {
		uint32_t tmp;

		tmp = fgc;
		fgc = bgc;
		bgc = tmp;
	}

	alpha = alpha << 24;
	fgc |= alpha;
	bgc |= alpha;

	for (uint32_t y = 0; y < height; y++) {
		for (uint32_t x = 0; x < width; x++) {
			byte = y * bpl + x / 8;
			bit = 0x80 >> (x % 8);
			o = y * width * bpp + x * bpp;
			cc = glyph[byte] & bit ? fgc : bgc;

			gfx_mem_wr4(state->tg_glyph,
			    state->tg_glyph_size, o, cc);
		}
	}
}

/*
 * Draw prepared glyph on terminal point p.
 */
static void
gfx_fb_printchar(teken_gfx_t *state, const teken_pos_t *p)
{
	unsigned x, y, width, height;

	width = state->tg_font.vf_width;
	height = state->tg_font.vf_height;
	x = state->tg_origin.tp_col + p->tp_col * width;
	y = state->tg_origin.tp_row + p->tp_row * height;

	gfx_fb_cons_display(x, y, width, height, state->tg_glyph);
}

/*
 * Store char with its attribute to buffer and put it on screen.
 */
void
gfx_fb_putchar(void *arg, const teken_pos_t *p, teken_char_t c,
    const teken_attr_t *a)
{
	teken_gfx_t *state = arg;
	const uint8_t *glyph;
	int idx;

	idx = p->tp_col + p->tp_row * state->tg_tp.tp_col;
	if (idx >= state->tg_tp.tp_col * state->tg_tp.tp_row)
		return;

	/* remove the cursor */
	if (state->tg_cursor_visible)
		gfx_fb_cursor_draw(state, &state->tg_cursor, false);

	screen_buffer[idx].c = c;
	screen_buffer[idx].a = *a;

	glyph = font_lookup(&state->tg_font, c, a);
	gfx_bitblt_bitmap(state, glyph, a, 0xff, false);
	gfx_fb_printchar(state, p);

	/* display the cursor */
	if (state->tg_cursor_visible) {
		const teken_pos_t *c;

		c = teken_get_cursor(&state->tg_teken);
		gfx_fb_cursor_draw(state, c, true);
	}
}

void
gfx_fb_fill(void *arg, const teken_rect_t *r, teken_char_t c,
    const teken_attr_t *a)
{
	teken_gfx_t *state = arg;
	const uint8_t *glyph;
	teken_pos_t p;
	struct text_pixel *row;

	/* remove the cursor */
	if (state->tg_cursor_visible)
		gfx_fb_cursor_draw(state, &state->tg_cursor, false);

	glyph = font_lookup(&state->tg_font, c, a);
	gfx_bitblt_bitmap(state, glyph, a, 0xff, false);

	for (p.tp_row = r->tr_begin.tp_row; p.tp_row < r->tr_end.tp_row;
	    p.tp_row++) {
		row = &screen_buffer[p.tp_row * state->tg_tp.tp_col];
		for (p.tp_col = r->tr_begin.tp_col;
		    p.tp_col < r->tr_end.tp_col; p.tp_col++) {
			row[p.tp_col].c = c;
			row[p.tp_col].a = *a;
			gfx_fb_printchar(state, &p);
		}
	}

	/* display the cursor */
	if (state->tg_cursor_visible) {
		const teken_pos_t *c;

		c = teken_get_cursor(&state->tg_teken);
		gfx_fb_cursor_draw(state, c, true);
	}
}

static void
gfx_fb_cursor_draw(teken_gfx_t *state, const teken_pos_t *p, bool on)
{
	const uint8_t *glyph;
	int idx;

	idx = p->tp_col + p->tp_row * state->tg_tp.tp_col;
	if (idx >= state->tg_tp.tp_col * state->tg_tp.tp_row)
		return;

	glyph = font_lookup(&state->tg_font, screen_buffer[idx].c,
	    &screen_buffer[idx].a);
	gfx_bitblt_bitmap(state, glyph, &screen_buffer[idx].a, 0xff, on);
	gfx_fb_printchar(state, p);
	state->tg_cursor = *p;
}

void
gfx_fb_cursor(void *arg, const teken_pos_t *p)
{
	teken_gfx_t *state = arg;
#if defined(EFI)
	EFI_TPL tpl;

	tpl = BS->RaiseTPL(TPL_NOTIFY);
#endif

	/* Switch cursor off in old location and back on in new. */
	if (state->tg_cursor_visible) {
		gfx_fb_cursor_draw(state, &state->tg_cursor, false);
		gfx_fb_cursor_draw(state, p, true);
	}
#if defined(EFI)
	BS->RestoreTPL(tpl);
#endif
}

void
gfx_fb_param(void *arg, int cmd, unsigned int value)
{
	teken_gfx_t *state = arg;
	const teken_pos_t *c;

	switch (cmd) {
	case TP_SETLOCALCURSOR:
		/*
		 * 0 means normal (usually block), 1 means hidden, and
		 * 2 means blinking (always block) for compatibility with
		 * syscons.  We don't support any changes except hiding,
		 * so must map 2 to 0.
		 */
		value = (value == 1) ? 0 : 1;
		/* FALLTHROUGH */
	case TP_SHOWCURSOR:
		c = teken_get_cursor(&state->tg_teken);
		gfx_fb_cursor_draw(state, c, true);
		if (value != 0)
			state->tg_cursor_visible = true;
		else
			state->tg_cursor_visible = false;
		break;
	default:
		/* Not yet implemented */
		break;
	}
}

bool
is_same_pixel(struct text_pixel *px1, struct text_pixel *px2)
{
	if (px1->c != px2->c)
		return (false);

	/* Is there image stored? */
	if ((px1->a.ta_format & TF_IMAGE) ||
	    (px2->a.ta_format & TF_IMAGE))
		return (false);

	if (px1->a.ta_format != px2->a.ta_format)
		return (false);
	if (px1->a.ta_fgcolor != px2->a.ta_fgcolor)
		return (false);
	if (px1->a.ta_bgcolor != px2->a.ta_bgcolor)
		return (false);

	return (true);
}

static void
gfx_fb_copy_area(teken_gfx_t *state, const teken_rect_t *s,
    const teken_pos_t *d)
{
	uint32_t sx, sy, dx, dy, width, height;

	width = state->tg_font.vf_width;
	height = state->tg_font.vf_height;

	sx = state->tg_origin.tp_col + s->tr_begin.tp_col * width;
	sy = state->tg_origin.tp_row + s->tr_begin.tp_row * height;
	dx = state->tg_origin.tp_col + d->tp_col * width;
	dy = state->tg_origin.tp_row + d->tp_row * height;

	width *= (s->tr_end.tp_col - s->tr_begin.tp_col + 1);

	(void) gfxfb_blt(NULL, GfxFbBltVideoToVideo, sx, sy, dx, dy,
		    width, height, 0);
}

static void
gfx_fb_copy_line(teken_gfx_t *state, int ncol, teken_pos_t *s, teken_pos_t *d)
{
	teken_rect_t sr;
	teken_pos_t dp;
	unsigned soffset, doffset;
	bool mark = false;
	int x;

	soffset = s->tp_col + s->tp_row * state->tg_tp.tp_col;
	doffset = d->tp_col + d->tp_row * state->tg_tp.tp_col;

	for (x = 0; x < ncol; x++) {
		if (is_same_pixel(&screen_buffer[soffset + x],
		    &screen_buffer[doffset + x])) {
			if (mark) {
				gfx_fb_copy_area(state, &sr, &dp);
				mark = false;
			}
		} else {
			screen_buffer[doffset + x] = screen_buffer[soffset + x];
			if (mark) {
				/* update end point */
				sr.tr_end.tp_col = s->tp_col + x;;
			} else {
				/* set up new rectangle */
				mark = true;
				sr.tr_begin.tp_col = s->tp_col + x;
				sr.tr_begin.tp_row = s->tp_row;
				sr.tr_end.tp_col = s->tp_col + x;
				sr.tr_end.tp_row = s->tp_row;
				dp.tp_col = d->tp_col + x;
				dp.tp_row = d->tp_row;
			}
		}
	}
	if (mark) {
		gfx_fb_copy_area(state, &sr, &dp);
	}
}

void
gfx_fb_copy(void *arg, const teken_rect_t *r, const teken_pos_t *p)
{
	teken_gfx_t *state = arg;
	unsigned doffset, soffset;
	teken_pos_t d, s;
	int nrow, ncol, y; /* Has to be signed - >= 0 comparison */

	/*
	 * Copying is a little tricky. We must make sure we do it in
	 * correct order, to make sure we don't overwrite our own data.
	 */

	nrow = r->tr_end.tp_row - r->tr_begin.tp_row;
	ncol = r->tr_end.tp_col - r->tr_begin.tp_col;

	if (p->tp_row + nrow > state->tg_tp.tp_row ||
	    p->tp_col + ncol > state->tg_tp.tp_col)
		return;

	soffset = r->tr_begin.tp_col + r->tr_begin.tp_row * state->tg_tp.tp_col;
	doffset = p->tp_col + p->tp_row * state->tg_tp.tp_col;

	/* remove the cursor */
	if (state->tg_cursor_visible)
		gfx_fb_cursor_draw(state, &state->tg_cursor, false);

	/*
	 * Copy line by line.
	 */
	if (doffset <= soffset) {
		s = r->tr_begin;
		d = *p;
		for (y = 0; y < nrow; y++) {
			s.tp_row = r->tr_begin.tp_row + y;
			d.tp_row = p->tp_row + y;

			gfx_fb_copy_line(state, ncol, &s, &d);
		}
	} else {
		for (y = nrow - 1; y >= 0; y--) {
			s.tp_row = r->tr_begin.tp_row + y;
			d.tp_row = p->tp_row + y;

			gfx_fb_copy_line(state, ncol, &s, &d);
		}
	}

	/* display the cursor */
	if (state->tg_cursor_visible) {
		const teken_pos_t *c;

		c = teken_get_cursor(&state->tg_teken);
		gfx_fb_cursor_draw(state, c, true);
	}
}

/*
 * Implements alpha blending for RGBA data, could use pixels for arguments,
 * but byte stream seems more generic.
 * The generic alpha blending is:
 * blend = alpha * fg + (1.0 - alpha) * bg.
 * Since our alpha is not from range [0..1], we scale appropriately.
 */
static uint8_t
alpha_blend(uint8_t fg, uint8_t bg, uint8_t alpha)
{
	uint16_t blend, h, l;

	/* trivial corner cases */
	if (alpha == 0)
		return (bg);
	if (alpha == 0xFF)
		return (fg);
	blend = (alpha * fg + (0xFF - alpha) * bg);
	/* Division by 0xFF */
	h = blend >> 8;
	l = blend & 0xFF;
	if (h + l >= 0xFF)
		h++;
	return (h);
}

/*
 * Implements alpha blending for RGBA data, could use pixels for arguments,
 * but byte stream seems more generic.
 * The generic alpha blending is:
 * blend = alpha * fg + (1.0 - alpha) * bg.
 * Since our alpha is not from range [0..1], we scale appropriately.
 */
static void
bitmap_cpy(void *dst, void *src, uint32_t size)
{
#if defined(EFI)
	EFI_GRAPHICS_OUTPUT_BLT_PIXEL *ps, *pd;
#else
	struct paletteentry *ps, *pd;
#endif
	uint32_t i;
	uint8_t a;

	ps = src;
	pd = dst;

	/*
	 * we only implement alpha blending for depth 32.
	 */
	for (i = 0; i < size; i ++) {
		a = ps[i].Reserved;
		pd[i].Red = alpha_blend(ps[i].Red, pd[i].Red, a);
		pd[i].Green = alpha_blend(ps[i].Green, pd[i].Green, a);
		pd[i].Blue = alpha_blend(ps[i].Blue, pd[i].Blue, a);
		pd[i].Reserved = a;
	}
}

static void *
allocate_glyphbuffer(uint32_t width, uint32_t height)
{
	size_t size;

	size = sizeof (*GlyphBuffer) * width * height;
	if (size != GlyphBufferSize) {
		free(GlyphBuffer);
		GlyphBuffer = malloc(size);
		if (GlyphBuffer == NULL)
			return (NULL);
		GlyphBufferSize = size;
	}
	return (GlyphBuffer);
}

void
gfx_fb_cons_display(uint32_t x, uint32_t y, uint32_t width, uint32_t height,
    void *data)
{
#if defined(EFI)
	EFI_GRAPHICS_OUTPUT_BLT_PIXEL *buf;
#else
	struct paletteentry *buf;
#endif
	size_t size;

	size = width * height * sizeof(*buf);

	/*
	 * Common data to display is glyph, use preallocated
	 * glyph buffer.
	 */
        if (gfx_state.tg_glyph_size != GlyphBufferSize)
                (void) allocate_glyphbuffer(width, height);

	if (size == GlyphBufferSize)
		buf = GlyphBuffer;
	else
		buf = malloc(size);
	if (buf == NULL)
		return;

	if (gfxfb_blt(buf, GfxFbBltVideoToBltBuffer, x, y, 0, 0,
	    width, height, 0) == 0) {
		bitmap_cpy(buf, data, width * height);
		(void) gfxfb_blt(buf, GfxFbBltBufferToVideo, 0, 0, x, y,
		    width, height, 0);
	}
	if (buf != GlyphBuffer)
		free(buf);
}

/*
 * Public graphics primitives.
 */

static int
isqrt(int num)
{
	int res = 0;
	int bit = 1 << 30;

	/* "bit" starts at the highest power of four <= the argument. */
	while (bit > num)
		bit >>= 2;

	while (bit != 0) {
		if (num >= res + bit) {
			num -= res + bit;
			res = (res >> 1) + bit;
		} else {
			res >>= 1;
		}
		bit >>= 2;
	}
	return (res);
}

/* set pixel in framebuffer using gfx coordinates */
void
gfx_fb_setpixel(uint32_t x, uint32_t y)
{
	uint32_t c;
	const teken_attr_t *ap;

	if (gfx_state.tg_fb_type == FB_TEXT)
		return;

	ap = teken_get_curattr(&gfx_state.tg_teken);
        if (ap->ta_format & TF_REVERSE) {
		c = ap->ta_bgcolor;
		if (ap->ta_format & TF_BLINK)
			c |= TC_LIGHT;
	} else {
		c = ap->ta_fgcolor;
		if (ap->ta_format & TF_BOLD)
			c |= TC_LIGHT;
	}

	c = gfx_fb_color_map(c);

	if (x >= gfx_state.tg_fb.fb_width ||
	    y >= gfx_state.tg_fb.fb_height)
		return;

	gfxfb_blt(&c, GfxFbBltVideoFill, 0, 0, x, y, 1, 1, 0);
}

/*
 * draw rectangle in framebuffer using gfx coordinates.
 * The function is borrowed from vt_fb.c
 */
void
gfx_fb_drawrect(uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2,
    uint32_t fill)
{
	uint32_t x, y;

	if (gfx_state.tg_fb_type == FB_TEXT)
		return;

	for (y = y1; y <= y2; y++) {
		if (fill || (y == y1) || (y == y2)) {
			for (x = x1; x <= x2; x++)
				gfx_fb_setpixel(x, y);
		} else {
			gfx_fb_setpixel(x1, y);
			gfx_fb_setpixel(x2, y);
		}
	}
}

void
gfx_fb_line(uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1, uint32_t wd)
{
	int dx, sx, dy, sy;
	int err, e2, x2, y2, ed, width;

	if (gfx_state.tg_fb_type == FB_TEXT)
		return;

	width = wd;
	sx = x0 < x1? 1 : -1;
	sy = y0 < y1? 1 : -1;
	dx = x1 > x0? x1 - x0 : x0 - x1;
	dy = y1 > y0? y1 - y0 : y0 - y1;
	err = dx + dy;
	ed = dx + dy == 0 ? 1: isqrt(dx * dx + dy * dy);

	for (;;) {
		gfx_fb_setpixel(x0, y0);
		e2 = err;
		x2 = x0;
		if ((e2 << 1) >= -dx) {		/* x step */
			e2 += dy;
			y2 = y0;
			while (e2 < ed * width &&
			    (y1 != (uint32_t)y2 || dx > dy)) {
				y2 += sy;
				gfx_fb_setpixel(x0, y2);
				e2 += dx;
			}
			if (x0 == x1)
				break;
			e2 = err;
			err -= dy;
			x0 += sx;
		}
		if ((e2 << 1) <= dy) {		/* y step */
			e2 = dx-e2;
			while (e2 < ed * width &&
			    (x1 != (uint32_t)x2 || dx < dy)) {
				x2 += sx;
				gfx_fb_setpixel(x2, y0);
				e2 += dy;
			}
			if (y0 == y1)
				break;
			err += dx;
			y0 += sy;
		}
	}
}

/*
 * quadratic Bézier curve limited to gradients without sign change.
 */
void
gfx_fb_bezier(uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1, uint32_t x2,
    uint32_t y2, uint32_t wd)
{
	int sx, sy, xx, yy, xy, width;
	int dx, dy, err, curvature;
	int i;

	if (gfx_state.tg_fb_type == FB_TEXT)
		return;

	width = wd;
	sx = x2 - x1;
	sy = y2 - y1;
	xx = x0 - x1;
	yy = y0 - y1;
	curvature = xx*sy - yy*sx;

	if (sx*sx + sy*sy > xx*xx+yy*yy) {
		x2 = x0;
		x0 = sx + x1;
		y2 = y0;
		y0 = sy + y1;
		curvature = -curvature;
	}
	if (curvature != 0) {
		xx += sx;
		sx = x0 < x2? 1 : -1;
		xx *= sx;
		yy += sy;
		sy = y0 < y2? 1 : -1;
		yy *= sy;
		xy = (xx*yy) << 1;
		xx *= xx;
		yy *= yy;
		if (curvature * sx * sy < 0) {
			xx = -xx;
			yy = -yy;
			xy = -xy;
			curvature = -curvature;
		}
		dx = 4 * sy * curvature * (x1 - x0) + xx - xy;
		dy = 4 * sx * curvature * (y0 - y1) + yy - xy;
		xx += xx;
		yy += yy;
		err = dx + dy + xy;
		do {
			for (i = 0; i <= width; i++)
				gfx_fb_setpixel(x0 + i, y0);
			if (x0 == x2 && y0 == y2)
				return;  /* last pixel -> curve finished */
			y1 = 2 * err < dx;
			if (2 * err > dy) {
				x0 += sx;
				dx -= xy;
				dy += yy;
				err += dy;
			}
			if (y1 != 0) {
				y0 += sy;
				dy -= xy;
				dx += xx;
				err += dx;
			}
		} while (dy < dx); /* gradient negates -> algorithm fails */
	}
	gfx_fb_line(x0, y0, x2, y2, width);
}

/*
 * draw rectangle using terminal coordinates and current foreground color.
 */
void
gfx_term_drawrect(uint32_t ux1, uint32_t uy1, uint32_t ux2, uint32_t uy2)
{
	int x1, y1, x2, y2;
	int xshift, yshift;
	int width, i;
	uint32_t vf_width, vf_height;
	teken_rect_t r;

	if (gfx_state.tg_fb_type == FB_TEXT)
		return;

	vf_width = gfx_state.tg_font.vf_width;
	vf_height = gfx_state.tg_font.vf_height;
	width = vf_width / 4;			/* line width */
	xshift = (vf_width - width) / 2;
	yshift = (vf_height - width) / 2;

	/* Shift coordinates */
	if (ux1 != 0)
		ux1--;
	if (uy1 != 0)
		uy1--;
	ux2--;
	uy2--;

	/* mark area used in terminal */
	r.tr_begin.tp_col = ux1;
	r.tr_begin.tp_row = uy1;
	r.tr_end.tp_col = ux2 + 1;
	r.tr_end.tp_row = uy2 + 1;

	term_image_display(&gfx_state, &r);

	/*
	 * Draw horizontal lines width points thick, shifted from outer edge.
	 */
	x1 = (ux1 + 1) * vf_width + gfx_state.tg_origin.tp_col;
	y1 = uy1 * vf_height + gfx_state.tg_origin.tp_row + yshift;
	x2 = ux2 * vf_width + gfx_state.tg_origin.tp_col;
	gfx_fb_drawrect(x1, y1, x2, y1 + width, 1);
	y2 = uy2 * vf_height + gfx_state.tg_origin.tp_row;
	y2 += vf_height - yshift - width;
	gfx_fb_drawrect(x1, y2, x2, y2 + width, 1);

	/*
	 * Draw vertical lines width points thick, shifted from outer edge.
	 */
	x1 = ux1 * vf_width + gfx_state.tg_origin.tp_col + xshift;
	y1 = uy1 * vf_height + gfx_state.tg_origin.tp_row;
	y1 += vf_height;
	y2 = uy2 * vf_height + gfx_state.tg_origin.tp_row;
	gfx_fb_drawrect(x1, y1, x1 + width, y2, 1);
	x1 = ux2 * vf_width + gfx_state.tg_origin.tp_col;
	x1 += vf_width - xshift - width;
	gfx_fb_drawrect(x1, y1, x1 + width, y2, 1);

	/* Draw upper left corner. */
	x1 = ux1 * vf_width + gfx_state.tg_origin.tp_col + xshift;
	y1 = uy1 * vf_height + gfx_state.tg_origin.tp_row;
	y1 += vf_height;

	x2 = ux1 * vf_width + gfx_state.tg_origin.tp_col;
	x2 += vf_width;
	y2 = uy1 * vf_height + gfx_state.tg_origin.tp_row + yshift;
	for (i = 0; i <= width; i++)
		gfx_fb_bezier(x1 + i, y1, x1 + i, y2 + i, x2, y2 + i, width-i);

	/* Draw lower left corner. */
	x1 = ux1 * vf_width + gfx_state.tg_origin.tp_col;
	x1 += vf_width;
	y1 = uy2 * vf_height + gfx_state.tg_origin.tp_row;
	y1 += vf_height - yshift;
	x2 = ux1 * vf_width + gfx_state.tg_origin.tp_col + xshift;
	y2 = uy2 * vf_height + gfx_state.tg_origin.tp_row;
	for (i = 0; i <= width; i++)
		gfx_fb_bezier(x1, y1 - i, x2 + i, y1 - i, x2 + i, y2, width-i);

	/* Draw upper right corner. */
	x1 = ux2 * vf_width + gfx_state.tg_origin.tp_col;
	y1 = uy1 * vf_height + gfx_state.tg_origin.tp_row + yshift;
	x2 = ux2 * vf_width + gfx_state.tg_origin.tp_col;
	x2 += vf_width - xshift - width;
	y2 = uy1 * vf_height + gfx_state.tg_origin.tp_row;
	y2 += vf_height;
	for (i = 0; i <= width; i++)
		gfx_fb_bezier(x1, y1 + i, x2 + i, y1 + i, x2 + i, y2, width-i);

	/* Draw lower right corner. */
	x1 = ux2 * vf_width + gfx_state.tg_origin.tp_col;
	y1 = uy2 * vf_height + gfx_state.tg_origin.tp_row;
	y1 += vf_height - yshift;
	x2 = ux2 * vf_width + gfx_state.tg_origin.tp_col;
	x2 += vf_width - xshift - width;
	y2 = uy2 * vf_height + gfx_state.tg_origin.tp_row;
	for (i = 0; i <= width; i++)
		gfx_fb_bezier(x1, y1 - i, x2 + i, y1 - i, x2 + i, y2, width-i);
}

int
gfx_fb_putimage(png_t *png, uint32_t ux1, uint32_t uy1, uint32_t ux2,
    uint32_t uy2, uint32_t flags)
{
#if defined(EFI)
	EFI_GRAPHICS_OUTPUT_BLT_PIXEL *p;
#else
	struct paletteentry *p;
#endif
	uint8_t *data;
	uint32_t i, j, x, y, fheight, fwidth;
	int rs, gs, bs;
	uint8_t r, g, b, a;
	bool scale = false;
	bool trace = false;
	teken_rect_t rect;

	trace = (flags & FL_PUTIMAGE_DEBUG) != 0;

	if (gfx_state.tg_fb_type == FB_TEXT) {
		if (trace)
			printf("Framebuffer not active.\n");
		return (1);
	}

	if (png->color_type != PNG_TRUECOLOR_ALPHA) {
		if (trace)
			printf("Not truecolor image.\n");
		return (1);
	}

	if (ux1 > gfx_state.tg_fb.fb_width ||
	    uy1 > gfx_state.tg_fb.fb_height) {
		if (trace)
			printf("Top left coordinate off screen.\n");
		return (1);
	}

	if (png->width > UINT16_MAX || png->height > UINT16_MAX) {
		if (trace)
			printf("Image too large.\n");
		return (1);
	}

	if (png->width < 1 || png->height < 1) {
		if (trace)
			printf("Image too small.\n");
		return (1);
	}

	/*
	 * If 0 was passed for either ux2 or uy2, then calculate the missing
	 * part of the bottom right coordinate.
	 */
	scale = true;
	if (ux2 == 0 && uy2 == 0) {
		/* Both 0, use the native resolution of the image */
		ux2 = ux1 + png->width;
		uy2 = uy1 + png->height;
		scale = false;
	} else if (ux2 == 0) {
		/* Set ux2 from uy2/uy1 to maintain aspect ratio */
		ux2 = ux1 + (png->width * (uy2 - uy1)) / png->height;
	} else if (uy2 == 0) {
		/* Set uy2 from ux2/ux1 to maintain aspect ratio */
		uy2 = uy1 + (png->height * (ux2 - ux1)) / png->width;
	}

	if (ux2 > gfx_state.tg_fb.fb_width ||
	    uy2 > gfx_state.tg_fb.fb_height) {
		if (trace)
			printf("Bottom right coordinate off screen.\n");
		return (1);
	}

	fwidth = ux2 - ux1;
	fheight = uy2 - uy1;

	/*
	 * If the original image dimensions have been passed explicitly,
	 * disable scaling.
	 */
	if (fwidth == png->width && fheight == png->height)
		scale = false;

	if (ux1 == 0) {
		/*
		 * No top left X co-ordinate (real coordinates start at 1),
		 * place as far right as it will fit.
		 */
		ux2 = gfx_state.tg_fb.fb_width - gfx_state.tg_origin.tp_col;
		ux1 = ux2 - fwidth;
	}

	if (uy1 == 0) {
		/*
		 * No top left Y co-ordinate (real coordinates start at 1),
		 * place as far down as it will fit.
		 */
		uy2 = gfx_state.tg_fb.fb_height - gfx_state.tg_origin.tp_row;
		uy1 = uy2 - fheight;
	}

	if (ux1 >= ux2 || uy1 >= uy2) {
		if (trace)
			printf("Image dimensions reversed.\n");
		return (1);
	}

	if (fwidth < 2 || fheight < 2) {
		if (trace)
			printf("Target area too small\n");
		return (1);
	}

	if (trace)
		printf("Image %ux%u -> %ux%u @%ux%u\n",
		    png->width, png->height, fwidth, fheight, ux1, uy1);

	rect.tr_begin.tp_col = ux1 / gfx_state.tg_font.vf_width;
	rect.tr_begin.tp_row = uy1 / gfx_state.tg_font.vf_height;
	rect.tr_end.tp_col = (ux1 + fwidth) / gfx_state.tg_font.vf_width;
	rect.tr_end.tp_row = (uy1 + fheight) / gfx_state.tg_font.vf_height;

	/*
	 * mark area used in terminal
	 */
	if (!(flags & FL_PUTIMAGE_NOSCROLL))
		term_image_display(&gfx_state, &rect);

	if ((flags & FL_PUTIMAGE_BORDER))
		gfx_fb_drawrect(ux1, uy1, ux2, uy2, 0);

	data = malloc(fwidth * fheight * sizeof(*p));
	p = (void *)data;
	if (data == NULL) {
		if (trace)
			printf("Out of memory.\n");
		return (1);
	}

	/*
	 * Build image for our framebuffer.
	 */

	/* Helper to calculate the pixel index from the source png */
#define	GETPIXEL(xx, yy)	(((yy) * png->width + (xx)) * png->bpp)

	/*
	 * For each of the x and y directions, calculate the number of pixels
	 * in the source image that correspond to a single pixel in the target.
	 * Use fixed-point arithmetic with 16-bits for each of the integer and
	 * fractional parts.
	 */
	const uint32_t wcstep = ((png->width - 1) << 16) / (fwidth - 1);
	const uint32_t hcstep = ((png->height - 1) << 16) / (fheight - 1);

	rs = 8 - (fls(gfx_state.tg_fb.fb_mask_red) -
	    ffs(gfx_state.tg_fb.fb_mask_red) + 1);
	gs = 8 - (fls(gfx_state.tg_fb.fb_mask_green) -
	    ffs(gfx_state.tg_fb.fb_mask_green) + 1);
	bs = 8 - (fls(gfx_state.tg_fb.fb_mask_blue) -
	    ffs(gfx_state.tg_fb.fb_mask_blue) + 1);

	uint32_t hc = 0;
	for (y = 0; y < fheight; y++) {
		uint32_t hc2 = (hc >> 9) & 0x7f;
		uint32_t hc1 = 0x80 - hc2;

		uint32_t offset_y = hc >> 16;
		uint32_t offset_y1 = offset_y + 1;

		uint32_t wc = 0;
		for (x = 0; x < fwidth; x++) {
			uint32_t wc2 = (wc >> 9) & 0x7f;
			uint32_t wc1 = 0x80 - wc2;

			uint32_t offset_x = wc >> 16;
			uint32_t offset_x1 = offset_x + 1;

			/* Target pixel index */
			j = y * fwidth + x;

			if (!scale) {
				i = GETPIXEL(x, y);
				r = png->image[i];
				g = png->image[i + 1];
				b = png->image[i + 2];
				a = png->image[i + 3];
			} else {
				uint8_t pixel[4];

				uint32_t p00 = GETPIXEL(offset_x, offset_y);
				uint32_t p01 = GETPIXEL(offset_x, offset_y1);
				uint32_t p10 = GETPIXEL(offset_x1, offset_y);
				uint32_t p11 = GETPIXEL(offset_x1, offset_y1);

				/*
				 * Given a 2x2 array of pixels in the source
				 * image, combine them to produce a single
				 * value for the pixel in the target image.
				 * Each column of pixels is combined using
				 * a weighted average where the top and bottom
				 * pixels contribute hc1 and hc2 respectively.
				 * The calculation for bottom pixel pB and
				 * top pixel pT is:
				 *   (pT * hc1 + pB * hc2) / (hc1 + hc2)
				 * Once the values are determined for the two
				 * columns of pixels, then the columns are
				 * averaged together in the same way but using
				 * wc1 and wc2 for the weightings.
				 *
				 * Since hc1 and hc2 are chosen so that
				 * hc1 + hc2 == 128 (and same for wc1 + wc2),
				 * the >> 14 below is a quick way to divide by
				 * (hc1 + hc2) * (wc1 + wc2)
				 */
				for (i = 0; i < 4; i++)
					pixel[i] = (
					    (png->image[p00 + i] * hc1 +
					    png->image[p01 + i] * hc2) * wc1 +
					    (png->image[p10 + i] * hc1 +
					    png->image[p11 + i] * hc2) * wc2)
					    >> 14;

				r = pixel[0];
				g = pixel[1];
				b = pixel[2];
				a = pixel[3];
			}

			if (trace)
				printf("r/g/b: %x/%x/%x\n", r, g, b);
			/*
			 * Rough colorspace reduction for 15/16 bit colors.
			 */
			p[j].Red = r >> rs;
                        p[j].Green = g >> gs;
                        p[j].Blue = b >> bs;
                        p[j].Reserved = a;

			wc += wcstep;
		}
		hc += hcstep;
	}

	gfx_fb_cons_display(ux1, uy1, fwidth, fheight, data);
	free(data);
	return (0);
}

/*
 * Reset font flags to FONT_AUTO.
 */
void
reset_font_flags(void)
{
	struct fontlist *fl;

	STAILQ_FOREACH(fl, &fonts, font_next) {
		fl->font_flags = FONT_AUTO;
	}
}

static vt_font_bitmap_data_t *
set_font(teken_unit_t *rows, teken_unit_t *cols, teken_unit_t h, teken_unit_t w)
{
	vt_font_bitmap_data_t *font = NULL;
	struct fontlist *fl;
	unsigned height = h;
	unsigned width = w;

	/*
	 * First check for manually loaded font.
	 */
	STAILQ_FOREACH(fl, &fonts, font_next) {
		if (fl->font_flags == FONT_MANUAL) {
			font = fl->font_data;
			if (font->vfbd_font == NULL && fl->font_load != NULL &&
			    fl->font_name != NULL) {
				font = fl->font_load(fl->font_name);
			}
			if (font == NULL || font->vfbd_font == NULL)
				font = NULL;
			break;
		}
	}

	if (font != NULL) {
		*rows = (height - BORDER_PIXELS) / font->vfbd_height;
		*cols = (width - BORDER_PIXELS) / font->vfbd_width;
		return (font);
	}

	/*
	 * Find best font for these dimensions, or use default
	 *
	 * A 1 pixel border is the absolute minimum we could have
	 * as a border around the text window (BORDER_PIXELS = 2),
	 * however a slightly larger border not only looks better
	 * but for the fonts currently statically built into the
	 * emulator causes much better font selection for the
	 * normal range of screen resolutions.
	 */
	STAILQ_FOREACH(fl, &fonts, font_next) {
		font = fl->font_data;
		if ((((*rows * font->vfbd_height) + BORDER_PIXELS) <= height) &&
		    (((*cols * font->vfbd_width) + BORDER_PIXELS) <= width)) {
			if (font->vfbd_font == NULL ||
			    fl->font_flags == FONT_RELOAD) {
				if (fl->font_load != NULL &&
				    fl->font_name != NULL) {
					font = fl->font_load(fl->font_name);
				}
				if (font == NULL)
					continue;
			}
			*rows = (height - BORDER_PIXELS) / font->vfbd_height;
			*cols = (width - BORDER_PIXELS) / font->vfbd_width;
			break;
		}
		font = NULL;
	}

	if (font == NULL) {
		/*
		 * We have fonts sorted smallest last, try it before
		 * falling back to builtin.
		 */
		fl = STAILQ_LAST(&fonts, fontlist, font_next);
		if (fl != NULL && fl->font_load != NULL &&
		    fl->font_name != NULL) {
			font = fl->font_load(fl->font_name);
		}
		if (font == NULL)
			font = &DEFAULT_FONT_DATA;

		*rows = (height - BORDER_PIXELS) / font->vfbd_height;
		*cols = (width - BORDER_PIXELS) / font->vfbd_width;
	}

	return (font);
}

static void
cons_clear(void)
{
	char clear[] = { '\033', 'c' };

	/* Reset terminal */
	teken_input(&gfx_state.tg_teken, clear, sizeof(clear));
	gfx_state.tg_functions->tf_param(&gfx_state, TP_SHOWCURSOR, 0);
}

void
setup_font(teken_gfx_t *state, teken_unit_t height, teken_unit_t width)
{
	vt_font_bitmap_data_t *font_data;
	teken_pos_t *tp = &state->tg_tp;
	char env[8];
	int i;

	/*
	 * set_font() will select a appropriate sized font for
	 * the number of rows and columns selected.  If we don't
	 * have a font that will fit, then it will use the
	 * default builtin font and adjust the rows and columns
	 * to fit on the screen.
	 */
	font_data = set_font(&tp->tp_row, &tp->tp_col, height, width);

        if (font_data == NULL)
		panic("out of memory");

	for (i = 0; i < VFNT_MAPS; i++) {
		state->tg_font.vf_map[i] =
		    font_data->vfbd_font->vf_map[i];
		state->tg_font.vf_map_count[i] =
		    font_data->vfbd_font->vf_map_count[i];
	}

	state->tg_font.vf_bytes = font_data->vfbd_font->vf_bytes;
	state->tg_font.vf_height = font_data->vfbd_font->vf_height;
	state->tg_font.vf_width = font_data->vfbd_font->vf_width;

	snprintf(env, sizeof (env), "%ux%u",
	    state->tg_font.vf_width, state->tg_font.vf_height);
	env_setenv("screen.font", EV_VOLATILE | EV_NOHOOK,
	    env, font_set, env_nounset);
}

/* Binary search for the glyph. Return 0 if not found. */
static uint16_t
font_bisearch(const vfnt_map_t *map, uint32_t len, teken_char_t src)
{
	unsigned min, mid, max;

	min = 0;
	max = len - 1;

	/* Empty font map. */
	if (len == 0)
		return (0);
	/* Character below minimal entry. */
	if (src < map[0].vfm_src)
		return (0);
	/* Optimization: ASCII characters occur very often. */
	if (src <= map[0].vfm_src + map[0].vfm_len)
		return (src - map[0].vfm_src + map[0].vfm_dst);
	/* Character above maximum entry. */
	if (src > map[max].vfm_src + map[max].vfm_len)
		return (0);

	/* Binary search. */
	while (max >= min) {
		mid = (min + max) / 2;
		if (src < map[mid].vfm_src)
			max = mid - 1;
		else if (src > map[mid].vfm_src + map[mid].vfm_len)
			min = mid + 1;
		else
			return (src - map[mid].vfm_src + map[mid].vfm_dst);
	}

	return (0);
}

/*
 * Return glyph bitmap. If glyph is not found, we will return bitmap
 * for the first (offset 0) glyph.
 */
uint8_t *
font_lookup(const struct vt_font *vf, teken_char_t c, const teken_attr_t *a)
{
	uint16_t dst;
	size_t stride;

	/* Substitute bold with normal if not found. */
	if (a->ta_format & TF_BOLD) {
		dst = font_bisearch(vf->vf_map[VFNT_MAP_BOLD],
		    vf->vf_map_count[VFNT_MAP_BOLD], c);
		if (dst != 0)
			goto found;
	}
	dst = font_bisearch(vf->vf_map[VFNT_MAP_NORMAL],
	    vf->vf_map_count[VFNT_MAP_NORMAL], c);

found:
	stride = howmany(vf->vf_width, 8) * vf->vf_height;
	return (&vf->vf_bytes[dst * stride]);
}

static int
load_mapping(int fd, struct vt_font *fp, int n)
{
	size_t i, size;
	ssize_t rv;
	vfnt_map_t *mp;

	if (fp->vf_map_count[n] == 0)
		return (0);

	size = fp->vf_map_count[n] * sizeof(*mp);
	mp = malloc(size);
	if (mp == NULL)
		return (ENOMEM);
	fp->vf_map[n] = mp;

	rv = read(fd, mp, size);
	if (rv < 0 || (size_t)rv != size) {
		free(fp->vf_map[n]);
		fp->vf_map[n] = NULL;
		return (EIO);
	}

	for (i = 0; i < fp->vf_map_count[n]; i++) {
		mp[i].vfm_src = be32toh(mp[i].vfm_src);
		mp[i].vfm_dst = be16toh(mp[i].vfm_dst);
		mp[i].vfm_len = be16toh(mp[i].vfm_len);
	}
	return (0);
}

static int
builtin_mapping(struct vt_font *fp, int n)
{
	size_t size;
	struct vfnt_map *mp;

	if (n >= VFNT_MAPS)
		return (EINVAL);

	if (fp->vf_map_count[n] == 0)
		return (0);

	size = fp->vf_map_count[n] * sizeof(*mp);
	mp = malloc(size);
	if (mp == NULL)
		return (ENOMEM);
	fp->vf_map[n] = mp;

	memcpy(mp, DEFAULT_FONT_DATA.vfbd_font->vf_map[n], size);
	return (0);
}

/*
 * Load font from builtin or from file.
 * We do need special case for builtin because the builtin font glyphs
 * are compressed and we do need to uncompress them.
 * Having single load_font() for both cases will help us to simplify
 * font switch handling.
 */
static vt_font_bitmap_data_t *
load_font(char *path)
{
	int fd, i;
	uint32_t glyphs;
	struct font_header fh;
	struct fontlist *fl;
	vt_font_bitmap_data_t *bp;
	struct vt_font *fp;
	size_t size;
	ssize_t rv;

	/* Get our entry from the font list. */
	STAILQ_FOREACH(fl, &fonts, font_next) {
		if (strcmp(fl->font_name, path) == 0)
			break;
	}
	if (fl == NULL)
		return (NULL);	/* Should not happen. */

	bp = fl->font_data;
	if (bp->vfbd_font != NULL && fl->font_flags != FONT_RELOAD)
		return (bp);

	fd = -1;
	/*
	 * Special case for builtin font.
	 * Builtin font is the very first font we load, we do not have
	 * previous loads to be released.
	 */
	if (fl->font_flags == FONT_BUILTIN) {
		if ((fp = calloc(1, sizeof(struct vt_font))) == NULL)
			return (NULL);

		fp->vf_width = DEFAULT_FONT_DATA.vfbd_width;
		fp->vf_height = DEFAULT_FONT_DATA.vfbd_height;

		fp->vf_bytes = malloc(DEFAULT_FONT_DATA.vfbd_uncompressed_size);
		if (fp->vf_bytes == NULL) {
			free(fp);
			return (NULL);
		}

		bp->vfbd_uncompressed_size =
		    DEFAULT_FONT_DATA.vfbd_uncompressed_size;
		bp->vfbd_compressed_size =
		    DEFAULT_FONT_DATA.vfbd_compressed_size;

		if (lz4_decompress(DEFAULT_FONT_DATA.vfbd_compressed_data,
		    fp->vf_bytes,
		    DEFAULT_FONT_DATA.vfbd_compressed_size,
		    DEFAULT_FONT_DATA.vfbd_uncompressed_size, 0) != 0) {
			free(fp->vf_bytes);
			free(fp);
			return (NULL);
		}

		for (i = 0; i < VFNT_MAPS; i++) {
			fp->vf_map_count[i] =
			    DEFAULT_FONT_DATA.vfbd_font->vf_map_count[i];
			if (builtin_mapping(fp, i) != 0)
				goto free_done;
		}

		bp->vfbd_font = fp;
		return (bp);
	}

	fd = open(path, O_RDONLY);
	if (fd < 0)
		return (NULL);

	size = sizeof(fh);
	rv = read(fd, &fh, size);
	if (rv < 0 || (size_t)rv != size) {
		bp = NULL;
		goto done;
	}
	if (memcmp(fh.fh_magic, FONT_HEADER_MAGIC, sizeof(fh.fh_magic)) != 0) {
		bp = NULL;
		goto done;
	}
	if ((fp = calloc(1, sizeof(struct vt_font))) == NULL) {
		bp = NULL;
		goto done;
	}
	for (i = 0; i < VFNT_MAPS; i++)
		fp->vf_map_count[i] = be32toh(fh.fh_map_count[i]);

	glyphs = be32toh(fh.fh_glyph_count);
	fp->vf_width = fh.fh_width;
	fp->vf_height = fh.fh_height;

	size = howmany(fp->vf_width, 8) * fp->vf_height * glyphs;
	bp->vfbd_uncompressed_size = size;
	if ((fp->vf_bytes = malloc(size)) == NULL)
		goto free_done;

	rv = read(fd, fp->vf_bytes, size);
	if (rv < 0 || (size_t)rv != size)
		goto free_done;
	for (i = 0; i < VFNT_MAPS; i++) {
		if (load_mapping(fd, fp, i) != 0)
			goto free_done;
	}

	/*
	 * Reset builtin flag now as we have full font loaded.
	 */
	if (fl->font_flags == FONT_BUILTIN)
		fl->font_flags = FONT_AUTO;

	/*
	 * Release previously loaded entries. We can do this now, as
	 * the new font is loaded. Note, there can be no console
	 * output till the new font is in place and teken is notified.
	 * We do need to keep fl->font_data for glyph dimensions.
	 */
	STAILQ_FOREACH(fl, &fonts, font_next) {
		if (fl->font_data->vfbd_font == NULL)
			continue;

		for (i = 0; i < VFNT_MAPS; i++)
			free(fl->font_data->vfbd_font->vf_map[i]);
		free(fl->font_data->vfbd_font->vf_bytes);
		free(fl->font_data->vfbd_font);
		fl->font_data->vfbd_font = NULL;
	}

	bp->vfbd_font = fp;
	bp->vfbd_compressed_size = 0;

done:
	if (fd != -1)
		close(fd);
	return (bp);

free_done:
	for (i = 0; i < VFNT_MAPS; i++)
		free(fp->vf_map[i]);
	free(fp->vf_bytes);
	free(fp);
	bp = NULL;
	goto done;
}

struct name_entry {
	char			*n_name;
	SLIST_ENTRY(name_entry)	n_entry;
};

SLIST_HEAD(name_list, name_entry);

/* Read font names from index file. */
static struct name_list *
read_list(char *fonts)
{
	struct name_list *nl;
	struct name_entry *np;
	char *dir, *ptr;
	char buf[PATH_MAX];
	int fd, len;

	dir = strdup(fonts);
	if (dir == NULL)
		return (NULL);

	ptr = strrchr(dir, '/');
	*ptr = '\0';

	fd = open(fonts, O_RDONLY);
	if (fd < 0)
		return (NULL);

	nl = malloc(sizeof(*nl));
	if (nl == NULL) {
		close(fd);
		return (nl);
	}

	SLIST_INIT(nl);
	while ((len = fgetstr(buf, sizeof (buf), fd)) >= 0) {
		if (*buf == '#' || *buf == '\0')
			continue;

		if (bcmp(buf, "MENU", 4) == 0)
			continue;

		if (bcmp(buf, "FONT", 4) == 0)
			continue;

		ptr = strchr(buf, ':');
		if (ptr == NULL)
			continue;
		else
			*ptr = '\0';

		np = malloc(sizeof(*np));
		if (np == NULL) {
			close(fd);
			return (nl);	/* return what we have */
		}
		if (asprintf(&np->n_name, "%s/%s", dir, buf) < 0) {
			free(np);
			close(fd);
			return (nl);    /* return what we have */
		}
		SLIST_INSERT_HEAD(nl, np, n_entry);
	}
	close(fd);
	return (nl);
}

/*
 * Read the font properties and insert new entry into the list.
 * The font list is built in descending order.
 */
static bool
insert_font(char *name, FONT_FLAGS flags)
{
	struct font_header fh;
	struct fontlist *fp, *previous, *entry, *next;
	size_t size;
	ssize_t rv;
	int fd;
	char *font_name;

	font_name = NULL;
	if (flags == FONT_BUILTIN) {
		/*
		 * We only install builtin font once, while setting up
		 * initial console. Since this will happen very early,
		 * we assume asprintf will not fail. Once we have access to
		 * files, the builtin font will be replaced by font loaded
		 * from file.
		 */
		if (!STAILQ_EMPTY(&fonts))
			return (false);

		fh.fh_width = DEFAULT_FONT_DATA.vfbd_width;
		fh.fh_height = DEFAULT_FONT_DATA.vfbd_height;

		(void) asprintf(&font_name, "%dx%d",
		    DEFAULT_FONT_DATA.vfbd_width,
		    DEFAULT_FONT_DATA.vfbd_height);
	} else {
		fd = open(name, O_RDONLY);
		if (fd < 0)
			return (false);
		rv = read(fd, &fh, sizeof(fh));
		close(fd);
		if (rv < 0 || (size_t)rv != sizeof(fh))
			return (false);

		if (memcmp(fh.fh_magic, FONT_HEADER_MAGIC,
		    sizeof(fh.fh_magic)) != 0)
			return (false);
		font_name = strdup(name);
	}

	if (font_name == NULL)
		return (false);

	/*
	 * If we have an entry with the same glyph dimensions, replace
	 * the file name and mark us. We only support unique dimensions.
	 */
	STAILQ_FOREACH(entry, &fonts, font_next) {
		if (fh.fh_width == entry->font_data->vfbd_width &&
		    fh.fh_height == entry->font_data->vfbd_height) {
			free(entry->font_name);
			entry->font_name = font_name;
			entry->font_flags = FONT_RELOAD;
			return (true);
		}
	}

	fp = calloc(sizeof(*fp), 1);
	if (fp == NULL) {
		free(font_name);
		return (false);
	}
	fp->font_data = calloc(sizeof(*fp->font_data), 1);
	if (fp->font_data == NULL) {
		free(font_name);
		free(fp);
		return (false);
	}
	fp->font_name = font_name;
	fp->font_flags = flags;
	fp->font_load = load_font;
	fp->font_data->vfbd_width = fh.fh_width;
	fp->font_data->vfbd_height = fh.fh_height;

	if (STAILQ_EMPTY(&fonts)) {
		STAILQ_INSERT_HEAD(&fonts, fp, font_next);
		return (true);
	}

	previous = NULL;
	size = fp->font_data->vfbd_width * fp->font_data->vfbd_height;

	STAILQ_FOREACH(entry, &fonts, font_next) {
		vt_font_bitmap_data_t *bd;

		bd = entry->font_data;
		/* Should fp be inserted before the entry? */
		if (size > bd->vfbd_width * bd->vfbd_height) {
			if (previous == NULL) {
				STAILQ_INSERT_HEAD(&fonts, fp, font_next);
			} else {
				STAILQ_INSERT_AFTER(&fonts, previous, fp,
				    font_next);
			}
			return (true);
		}
		next = STAILQ_NEXT(entry, font_next);
		if (next == NULL ||
		    size > next->font_data->vfbd_width *
		    next->font_data->vfbd_height) {
			STAILQ_INSERT_AFTER(&fonts, entry, fp, font_next);
			return (true);
		}
		previous = entry;
	}
	return (true);
}

static int
font_set(struct env_var *ev __unused, int flags __unused, const void *value)
{
	struct fontlist *fl;
	char *eptr;
	unsigned long x = 0, y = 0;

	/*
	 * Attempt to extract values from "XxY" string. In case of error,
	 * we have unmaching glyph dimensions and will just output the
	 * available values.
	 */
	if (value != NULL) {
		x = strtoul(value, &eptr, 10);
		if (*eptr == 'x')
			y = strtoul(eptr + 1, &eptr, 10);
	}
	STAILQ_FOREACH(fl, &fonts, font_next) {
		if (fl->font_data->vfbd_width == x &&
		    fl->font_data->vfbd_height == y)
			break;
	}
	if (fl != NULL) {
		/* Reset any FONT_MANUAL flag. */
		reset_font_flags();

		/* Mark this font manually loaded */
		fl->font_flags = FONT_MANUAL;
		cons_update_mode(gfx_state.tg_fb_type != FB_TEXT);
		return (CMD_OK);
	}

	printf("Available fonts:\n");
	STAILQ_FOREACH(fl, &fonts, font_next) {
		printf("    %dx%d\n", fl->font_data->vfbd_width,
		    fl->font_data->vfbd_height);
	}
	return (CMD_OK);
}

void
bios_text_font(bool use_vga_font)
{
	if (use_vga_font)
		(void) insert_font(VGA_8X16_FONT, FONT_MANUAL);
	else
		(void) insert_font(DEFAULT_8X16_FONT, FONT_MANUAL);
}

void
autoload_font(bool bios)
{
	struct name_list *nl;
	struct name_entry *np;

	nl = read_list("/boot/fonts/INDEX.fonts");
	if (nl == NULL)
		return;

	while (!SLIST_EMPTY(nl)) {
		np = SLIST_FIRST(nl);
		SLIST_REMOVE_HEAD(nl, n_entry);
		if (insert_font(np->n_name, FONT_AUTO) == false)
			printf("failed to add font: %s\n", np->n_name);
		free(np->n_name);
		free(np);
	}

	/*
	 * If vga text mode was requested, load vga.font (8x16 bold) font.
	 */
	if (bios) {
		bios_text_font(true);
	}

	(void) cons_update_mode(gfx_state.tg_fb_type != FB_TEXT);
}

COMMAND_SET(load_font, "loadfont", "load console font from file", command_font);

static int
command_font(int argc, char *argv[])
{
	int i, c, rc;
	struct fontlist *fl;
	vt_font_bitmap_data_t *bd;
	bool list;

	list = false;
	optind = 1;
	optreset = 1;
	rc = CMD_OK;

	while ((c = getopt(argc, argv, "l")) != -1) {
		switch (c) {
		case 'l':
			list = true;
			break;
		case '?':
		default:
			return (CMD_ERROR);
		}
	}

	argc -= optind;
	argv += optind;

	if (argc > 1 || (list && argc != 0)) {
		printf("Usage: loadfont [-l] | [file.fnt]\n");
		return (CMD_ERROR);
	}

	if (list) {
		STAILQ_FOREACH(fl, &fonts, font_next) {
			printf("font %s: %dx%d%s\n", fl->font_name,
			    fl->font_data->vfbd_width,
			    fl->font_data->vfbd_height,
			    fl->font_data->vfbd_font == NULL? "" : " loaded");
		}
		return (CMD_OK);
	}

	/* Clear scren */
	cons_clear();

	if (argc == 1) {
		char *name = argv[0];

		if (insert_font(name, FONT_MANUAL) == false) {
			printf("loadfont error: failed to load: %s\n", name);
			return (CMD_ERROR);
		}

		(void) cons_update_mode(gfx_state.tg_fb_type != FB_TEXT);
		return (CMD_OK);
	}

	if (argc == 0) {
		/*
		 * Walk entire font list, release any loaded font, and set
		 * autoload flag. The font list does have at least the builtin
		 * default font.
		 */
		STAILQ_FOREACH(fl, &fonts, font_next) {
			if (fl->font_data->vfbd_font != NULL) {

				bd = fl->font_data;
				/*
				 * Note the setup_font() is releasing
				 * font bytes.
				 */
				for (i = 0; i < VFNT_MAPS; i++)
					free(bd->vfbd_font->vf_map[i]);
				free(fl->font_data->vfbd_font);
				fl->font_data->vfbd_font = NULL;
				fl->font_data->vfbd_uncompressed_size = 0;
				fl->font_flags = FONT_AUTO;
			}
		}
		(void) cons_update_mode(gfx_state.tg_fb_type != FB_TEXT);
	}
	return (rc);
}

bool
gfx_get_edid_resolution(struct vesa_edid_info *edid, edid_res_list_t *res)
{
	struct resolution *rp, *p;

	/*
	 * Walk detailed timings tables (4).
	 */
	if ((edid->display.supported_features
	    & EDID_FEATURE_PREFERRED_TIMING_MODE) != 0) {
		/* Walk detailed timing descriptors (4) */
		for (int i = 0; i < DET_TIMINGS; i++) {
			/*
			 * Reserved value 0 is not used for display decriptor.
			 */
			if (edid->detailed_timings[i].pixel_clock == 0)
				continue;
			if ((rp = malloc(sizeof(*rp))) == NULL)
				continue;
			rp->width = GET_EDID_INFO_WIDTH(edid, i);
			rp->height = GET_EDID_INFO_HEIGHT(edid, i);
			if (rp->width > 0 && rp->width <= EDID_MAX_PIXELS &&
			    rp->height > 0 && rp->height <= EDID_MAX_LINES)
				TAILQ_INSERT_TAIL(res, rp, next);
			else
				free(rp);
		}
	}

	/*
	 * Walk standard timings list (8).
	 */
	for (int i = 0; i < STD_TIMINGS; i++) {
		/* Is this field unused? */
		if (edid->standard_timings[i] == 0x0101)
			continue;

		if ((rp = malloc(sizeof(*rp))) == NULL)
			continue;

		rp->width = HSIZE(edid->standard_timings[i]);
		switch (RATIO(edid->standard_timings[i])) {
		case RATIO1_1:
			rp->height = HSIZE(edid->standard_timings[i]);
			if (edid->header.version > 1 ||
			    edid->header.revision > 2) {
				rp->height = rp->height * 10 / 16;
			}
			break;
		case RATIO4_3:
			rp->height = HSIZE(edid->standard_timings[i]) * 3 / 4;
			break;
		case RATIO5_4:
			rp->height = HSIZE(edid->standard_timings[i]) * 4 / 5;
			break;
		case RATIO16_9:
			rp->height = HSIZE(edid->standard_timings[i]) * 9 / 16;
			break;
		}

		/*
		 * Create resolution list in decreasing order, except keep
		 * first entry (preferred timing mode).
		 */
		TAILQ_FOREACH(p, res, next) {
			if (p->width * p->height < rp->width * rp->height) {
				/* Keep preferred mode first */
				if (TAILQ_FIRST(res) == p)
					TAILQ_INSERT_AFTER(res, p, rp, next);
				else
					TAILQ_INSERT_BEFORE(p, rp, next);
				break;
			}
			if (TAILQ_NEXT(p, next) == NULL) {
				TAILQ_INSERT_TAIL(res, rp, next);
				break;
			}
		}
	}
	return (!TAILQ_EMPTY(res));
}
