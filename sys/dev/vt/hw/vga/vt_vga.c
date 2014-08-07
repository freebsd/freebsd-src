/*-
 * Copyright (c) 2005 Marcel Moolenaar
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
#include <dev/vt/hw/vga/vt_vga_reg.h>

#include <machine/bus.h>

#if defined(__amd64__) || defined(__i386__)
#include <vm/vm.h>
#include <vm/pmap.h>
#include <machine/pmap.h>
#include <machine/vmparam.h>
#endif /* __amd64__ || __i386__ */

struct vga_softc {
	bus_space_tag_t		 vga_fb_tag;
	bus_space_handle_t	 vga_fb_handle;
	bus_space_tag_t		 vga_reg_tag;
	bus_space_handle_t	 vga_reg_handle;
	int			 vga_curcolor;
};

/* Convenience macros. */
#define	MEM_READ1(sc, ofs) \
	bus_space_read_1(sc->vga_fb_tag, sc->vga_fb_handle, ofs)
#define	MEM_WRITE1(sc, ofs, val) \
	bus_space_write_1(sc->vga_fb_tag, sc->vga_fb_handle, ofs, val)
#define	REG_READ1(sc, reg) \
	bus_space_read_1(sc->vga_reg_tag, sc->vga_reg_handle, reg)
#define	REG_WRITE1(sc, reg, val) \
	bus_space_write_1(sc->vga_reg_tag, sc->vga_reg_handle, reg, val)

#define	VT_VGA_WIDTH	640
#define	VT_VGA_HEIGHT	480
#define	VT_VGA_MEMSIZE	(VT_VGA_WIDTH * VT_VGA_HEIGHT / 8)

static vd_probe_t	vga_probe;
static vd_init_t	vga_init;
static vd_blank_t	vga_blank;
static vd_bitbltchr_t	vga_bitbltchr;
static vd_maskbitbltchr_t vga_maskbitbltchr;
static vd_drawrect_t	vga_drawrect;
static vd_setpixel_t	vga_setpixel;
static vd_putchar_t	vga_putchar;
static vd_postswitch_t	vga_postswitch;

static const struct vt_driver vt_vga_driver = {
	.vd_name	= "vga",
	.vd_probe	= vga_probe,
	.vd_init	= vga_init,
	.vd_blank	= vga_blank,
	.vd_bitbltchr	= vga_bitbltchr,
	.vd_maskbitbltchr = vga_bitbltchr,
	.vd_drawrect	= vga_drawrect,
	.vd_setpixel	= vga_setpixel,
	.vd_putchar	= vga_putchar,
	.vd_postswitch	= vga_postswitch,
	.vd_priority	= VD_PRIORITY_GENERIC,
};

/*
 * Driver supports both text mode and graphics mode.  Make sure the
 * buffer is always big enough to support both.
 */
static struct vga_softc vga_conssoftc;
VT_DRIVER_DECLARE(vt_vga, vt_vga_driver);

static inline void
vga_setcolor(struct vt_device *vd, term_color_t color)
{
	struct vga_softc *sc = vd->vd_softc;

	if (sc->vga_curcolor != color) {
		REG_WRITE1(sc, VGA_GC_ADDRESS, VGA_GC_SET_RESET);
		REG_WRITE1(sc, VGA_GC_DATA, color);
		sc->vga_curcolor = color;
	}
}

static void
vga_blank(struct vt_device *vd, term_color_t color)
{
	struct vga_softc *sc = vd->vd_softc;
	u_int ofs;

	vga_setcolor(vd, color);
	for (ofs = 0; ofs < VT_VGA_MEMSIZE; ofs++)
		MEM_WRITE1(sc, ofs, 0xff);
}

static inline void
vga_bitblt_put(struct vt_device *vd, u_long dst, term_color_t color,
    uint8_t v)
{
	struct vga_softc *sc = vd->vd_softc;

	/* Skip empty writes, in order to avoid palette changes. */
	if (v != 0x00) {
		vga_setcolor(vd, color);
		/*
		 * When this MEM_READ1() gets disabled, all sorts of
		 * artifacts occur.  This is because this read loads the
		 * set of 8 pixels that are about to be changed.  There
		 * is one scenario where we can avoid the read, namely
		 * if all pixels are about to be overwritten anyway.
		 */
		if (v != 0xff)
			MEM_READ1(sc, dst);
		MEM_WRITE1(sc, dst, v);
	}
}

static void
vga_setpixel(struct vt_device *vd, int x, int y, term_color_t color)
{

	vga_bitblt_put(vd, (y * VT_VGA_WIDTH / 8) + (x / 8), color,
	    0x80 >> (x % 8));
}

static void
vga_drawrect(struct vt_device *vd, int x1, int y1, int x2, int y2, int fill,
    term_color_t color)
{
	int x, y;

	for (y = y1; y <= y2; y++) {
		if (fill || (y == y1) || (y == y2)) {
			for (x = x1; x <= x2; x++)
				vga_setpixel(vd, x, y, color);
		} else {
			vga_setpixel(vd, x1, y, color);
			vga_setpixel(vd, x2, y, color);
		}
	}
}

/*
 * Shift bitmap of one row of the glyph.
 * a - array of bytes with src bitmap and result storage.
 * m - resulting background color bitmask.
 * size - number of bytes per glyph row (+ one byte to store shift overflow).
 * shift - offset for target bitmap.
 */

static void
vga_shift_u8array(uint8_t *a, uint8_t *m, int size, int shift)
{
	int i;

	for (i = (size - 1); i > 0; i--) {
		a[i] = (a[i] >> shift) | (a[i-1] << (7 - shift));
		m[i] = ~a[i];
	}
	a[0] = (a[0] >> shift);
	m[0] = ~a[0] & (0xff >> shift);
	m[size - 1] = ~a[size - 1] & (0xff << (7 - shift));
}

/* XXX: fix gaps on mouse track when character size is not rounded to 8. */
static void
vga_bitbltchr(struct vt_device *vd, const uint8_t *src, const uint8_t *mask,
    int bpl, vt_axis_t top, vt_axis_t left, unsigned int width,
    unsigned int height, term_color_t fg, term_color_t bg)
{
	uint8_t aa[64], ma[64], *r;
	int dst, shift, sz, x, y;
	struct vga_softc *sc;

	if ((left + width) > VT_VGA_WIDTH)
		return;
	if ((top + height) > VT_VGA_HEIGHT)
		return;

	sc = vd->vd_softc;

	sz = (width + 7) / 8;
	shift = left % 8;

	dst = (VT_VGA_WIDTH * top + left) / 8;

	for (y = 0; y < height; y++) {
		r = (uint8_t *)src + (y * sz);
		memcpy(aa, r, sz);
		aa[sz] = 0;
		vga_shift_u8array(aa, ma, sz + 1, shift);

		vga_setcolor(vd, bg);
		for (x = 0; x < (sz + 1); x ++) {
			if (ma[x] == 0)
				continue;
			/*
			 * XXX Only mouse cursor can go out of screen.
			 * So for mouse it have to just return, but for regular
			 * characters it have to panic, to indicate error in
			 * size/coordinates calculations.
			 */
			if ((dst + x) >= (VT_VGA_WIDTH * VT_VGA_HEIGHT))
				return;
			if (ma[x] != 0xff)
				MEM_READ1(sc, dst + x);
			MEM_WRITE1(sc, dst + x, ma[x]);
		}

		vga_setcolor(vd, fg);
		for (x = 0; x < (sz + 1); x ++) {
			if (aa[x] == 0)
				continue;
			if (aa[x] != 0xff)
				MEM_READ1(sc, dst + x);
			MEM_WRITE1(sc, dst + x, aa[x]);
		}

		dst += VT_VGA_WIDTH / 8;
	}
}

/*
 * Binary searchable table for Unicode to CP437 conversion.
 */

struct unicp437 {
	uint16_t	unicode_base;
	uint8_t		cp437_base;
	uint8_t		length;
};

static const struct unicp437 cp437table[] = {
	{ 0x0020, 0x20, 0x5e }, { 0x00a0, 0x20, 0x00 },
	{ 0x00a1, 0xad, 0x00 }, { 0x00a2, 0x9b, 0x00 },
	{ 0x00a3, 0x9c, 0x00 }, { 0x00a5, 0x9d, 0x00 },
	{ 0x00a7, 0x15, 0x00 }, { 0x00aa, 0xa6, 0x00 },
	{ 0x00ab, 0xae, 0x00 }, { 0x00ac, 0xaa, 0x00 },
	{ 0x00b0, 0xf8, 0x00 }, { 0x00b1, 0xf1, 0x00 },
	{ 0x00b2, 0xfd, 0x00 }, { 0x00b5, 0xe6, 0x00 },
	{ 0x00b6, 0x14, 0x00 }, { 0x00b7, 0xfa, 0x00 },
	{ 0x00ba, 0xa7, 0x00 }, { 0x00bb, 0xaf, 0x00 },
	{ 0x00bc, 0xac, 0x00 }, { 0x00bd, 0xab, 0x00 },
	{ 0x00bf, 0xa8, 0x00 }, { 0x00c4, 0x8e, 0x01 },
	{ 0x00c6, 0x92, 0x00 }, { 0x00c7, 0x80, 0x00 },
	{ 0x00c9, 0x90, 0x00 }, { 0x00d1, 0xa5, 0x00 },
	{ 0x00d6, 0x99, 0x00 }, { 0x00dc, 0x9a, 0x00 },
	{ 0x00df, 0xe1, 0x00 }, { 0x00e0, 0x85, 0x00 },
	{ 0x00e1, 0xa0, 0x00 }, { 0x00e2, 0x83, 0x00 },
	{ 0x00e4, 0x84, 0x00 }, { 0x00e5, 0x86, 0x00 },
	{ 0x00e6, 0x91, 0x00 }, { 0x00e7, 0x87, 0x00 },
	{ 0x00e8, 0x8a, 0x00 }, { 0x00e9, 0x82, 0x00 },
	{ 0x00ea, 0x88, 0x01 }, { 0x00ec, 0x8d, 0x00 },
	{ 0x00ed, 0xa1, 0x00 }, { 0x00ee, 0x8c, 0x00 },
	{ 0x00ef, 0x8b, 0x00 }, { 0x00f0, 0xeb, 0x00 },
	{ 0x00f1, 0xa4, 0x00 }, { 0x00f2, 0x95, 0x00 },
	{ 0x00f3, 0xa2, 0x00 }, { 0x00f4, 0x93, 0x00 },
	{ 0x00f6, 0x94, 0x00 }, { 0x00f7, 0xf6, 0x00 },
	{ 0x00f8, 0xed, 0x00 }, { 0x00f9, 0x97, 0x00 },
	{ 0x00fa, 0xa3, 0x00 }, { 0x00fb, 0x96, 0x00 },
	{ 0x00fc, 0x81, 0x00 }, { 0x00ff, 0x98, 0x00 },
	{ 0x0192, 0x9f, 0x00 }, { 0x0393, 0xe2, 0x00 },
	{ 0x0398, 0xe9, 0x00 }, { 0x03a3, 0xe4, 0x00 },
	{ 0x03a6, 0xe8, 0x00 }, { 0x03a9, 0xea, 0x00 },
	{ 0x03b1, 0xe0, 0x01 }, { 0x03b4, 0xeb, 0x00 },
	{ 0x03b5, 0xee, 0x00 }, { 0x03bc, 0xe6, 0x00 },
	{ 0x03c0, 0xe3, 0x00 }, { 0x03c3, 0xe5, 0x00 },
	{ 0x03c4, 0xe7, 0x00 }, { 0x03c6, 0xed, 0x00 },
	{ 0x03d5, 0xed, 0x00 }, { 0x2010, 0x2d, 0x00 },
	{ 0x2014, 0x2d, 0x00 }, { 0x2018, 0x60, 0x00 },
	{ 0x2019, 0x27, 0x00 }, { 0x201c, 0x22, 0x00 },
	{ 0x201d, 0x22, 0x00 }, { 0x2022, 0x07, 0x00 },
	{ 0x203c, 0x13, 0x00 }, { 0x207f, 0xfc, 0x00 },
	{ 0x20a7, 0x9e, 0x00 }, { 0x20ac, 0xee, 0x00 },
	{ 0x2126, 0xea, 0x00 }, { 0x2190, 0x1b, 0x00 },
	{ 0x2191, 0x18, 0x00 }, { 0x2192, 0x1a, 0x00 },
	{ 0x2193, 0x19, 0x00 }, { 0x2194, 0x1d, 0x00 },
	{ 0x2195, 0x12, 0x00 }, { 0x21a8, 0x17, 0x00 },
	{ 0x2202, 0xeb, 0x00 }, { 0x2208, 0xee, 0x00 },
	{ 0x2211, 0xe4, 0x00 }, { 0x2212, 0x2d, 0x00 },
	{ 0x2219, 0xf9, 0x00 }, { 0x221a, 0xfb, 0x00 },
	{ 0x221e, 0xec, 0x00 }, { 0x221f, 0x1c, 0x00 },
	{ 0x2229, 0xef, 0x00 }, { 0x2248, 0xf7, 0x00 },
	{ 0x2261, 0xf0, 0x00 }, { 0x2264, 0xf3, 0x00 },
	{ 0x2265, 0xf2, 0x00 }, { 0x2302, 0x7f, 0x00 },
	{ 0x2310, 0xa9, 0x00 }, { 0x2320, 0xf4, 0x00 },
	{ 0x2321, 0xf5, 0x00 }, { 0x2500, 0xc4, 0x00 },
	{ 0x2502, 0xb3, 0x00 }, { 0x250c, 0xda, 0x00 },
	{ 0x2510, 0xbf, 0x00 }, { 0x2514, 0xc0, 0x00 },
	{ 0x2518, 0xd9, 0x00 }, { 0x251c, 0xc3, 0x00 },
	{ 0x2524, 0xb4, 0x00 }, { 0x252c, 0xc2, 0x00 },
	{ 0x2534, 0xc1, 0x00 }, { 0x253c, 0xc5, 0x00 },
	{ 0x2550, 0xcd, 0x00 }, { 0x2551, 0xba, 0x00 },
	{ 0x2552, 0xd5, 0x00 }, { 0x2553, 0xd6, 0x00 },
	{ 0x2554, 0xc9, 0x00 }, { 0x2555, 0xb8, 0x00 },
	{ 0x2556, 0xb7, 0x00 }, { 0x2557, 0xbb, 0x00 },
	{ 0x2558, 0xd4, 0x00 }, { 0x2559, 0xd3, 0x00 },
	{ 0x255a, 0xc8, 0x00 }, { 0x255b, 0xbe, 0x00 },
	{ 0x255c, 0xbd, 0x00 }, { 0x255d, 0xbc, 0x00 },
	{ 0x255e, 0xc6, 0x01 }, { 0x2560, 0xcc, 0x00 },
	{ 0x2561, 0xb5, 0x00 }, { 0x2562, 0xb6, 0x00 },
	{ 0x2563, 0xb9, 0x00 }, { 0x2564, 0xd1, 0x01 },
	{ 0x2566, 0xcb, 0x00 }, { 0x2567, 0xcf, 0x00 },
	{ 0x2568, 0xd0, 0x00 }, { 0x2569, 0xca, 0x00 },
	{ 0x256a, 0xd8, 0x00 }, { 0x256b, 0xd7, 0x00 },
	{ 0x256c, 0xce, 0x00 }, { 0x2580, 0xdf, 0x00 },
	{ 0x2584, 0xdc, 0x00 }, { 0x2588, 0xdb, 0x00 },
	{ 0x258c, 0xdd, 0x00 }, { 0x2590, 0xde, 0x00 },
	{ 0x2591, 0xb0, 0x02 }, { 0x25a0, 0xfe, 0x00 },
	{ 0x25ac, 0x16, 0x00 }, { 0x25b2, 0x1e, 0x00 },
	{ 0x25ba, 0x10, 0x00 }, { 0x25bc, 0x1f, 0x00 },
	{ 0x25c4, 0x11, 0x00 }, { 0x25cb, 0x09, 0x00 },
	{ 0x25d8, 0x08, 0x00 }, { 0x25d9, 0x0a, 0x00 },
	{ 0x263a, 0x01, 0x01 }, { 0x263c, 0x0f, 0x00 },
	{ 0x2640, 0x0c, 0x00 }, { 0x2642, 0x0b, 0x00 },
	{ 0x2660, 0x06, 0x00 }, { 0x2663, 0x05, 0x00 },
	{ 0x2665, 0x03, 0x01 }, { 0x266a, 0x0d, 0x00 },
	{ 0x266c, 0x0e, 0x00 },
};

static uint8_t
vga_get_cp437(term_char_t c)
{
	int min, mid, max;

	min = 0;
	max = (sizeof(cp437table) / sizeof(struct unicp437)) - 1;

	if (c < cp437table[0].unicode_base ||
	    c > cp437table[max].unicode_base + cp437table[max].length)
		return '?';

	while (max >= min) {
		mid = (min + max) / 2;
		if (c < cp437table[mid].unicode_base)
			max = mid - 1;
		else if (c > cp437table[mid].unicode_base +
		    cp437table[mid].length)
			min = mid + 1;
		else
			return (c - cp437table[mid].unicode_base +
			    cp437table[mid].cp437_base);
	}

	return '?';
}

static void
vga_putchar(struct vt_device *vd, term_char_t c,
    vt_axis_t top, vt_axis_t left, term_color_t fg, term_color_t bg)
{
	struct vga_softc *sc = vd->vd_softc;
	uint8_t ch, attr;

	/*
	 * Convert character to CP437, which is the character set used
	 * by the VGA hardware by default.
	 */
	ch = vga_get_cp437(c);

	/*
	 * Convert colors to VGA attributes.
	 */
	attr = bg << 4 | fg;

	MEM_WRITE1(sc, 0x18000 + (top * 80 + left) * 2 + 0, ch);
	MEM_WRITE1(sc, 0x18000 + (top * 80 + left) * 2 + 1, attr);
}

static void
vga_initialize_graphics(struct vt_device *vd)
{
	struct vga_softc *sc = vd->vd_softc;

	/* Clock select. */
	REG_WRITE1(sc, VGA_GEN_MISC_OUTPUT_W, VGA_GEN_MO_VSP | VGA_GEN_MO_HSP |
	    VGA_GEN_MO_PB | VGA_GEN_MO_ER | VGA_GEN_MO_IOA);
	/* Set sequencer clocking and memory mode. */
	REG_WRITE1(sc, VGA_SEQ_ADDRESS, VGA_SEQ_CLOCKING_MODE);
	REG_WRITE1(sc, VGA_SEQ_DATA, VGA_SEQ_CM_89);
	REG_WRITE1(sc, VGA_SEQ_ADDRESS, VGA_SEQ_MEMORY_MODE);
	REG_WRITE1(sc, VGA_SEQ_DATA, VGA_SEQ_MM_OE | VGA_SEQ_MM_EM);

	/* Set the graphics controller in graphics mode. */
	REG_WRITE1(sc, VGA_GC_ADDRESS, VGA_GC_MISCELLANEOUS);
	REG_WRITE1(sc, VGA_GC_DATA, 0x04 + VGA_GC_MISC_GA);
	/* Program the CRT controller. */
	REG_WRITE1(sc, VGA_CRTC_ADDRESS, VGA_CRTC_HORIZ_TOTAL);
	REG_WRITE1(sc, VGA_CRTC_DATA, 0x5f);			/* 760 */
	REG_WRITE1(sc, VGA_CRTC_ADDRESS, VGA_CRTC_HORIZ_DISP_END);
	REG_WRITE1(sc, VGA_CRTC_DATA, 0x4f);			/* 640 - 8 */
	REG_WRITE1(sc, VGA_CRTC_ADDRESS, VGA_CRTC_START_HORIZ_BLANK);
	REG_WRITE1(sc, VGA_CRTC_DATA, 0x50);			/* 640 */
	REG_WRITE1(sc, VGA_CRTC_ADDRESS, VGA_CRTC_END_HORIZ_BLANK);
	REG_WRITE1(sc, VGA_CRTC_DATA, VGA_CRTC_EHB_CR + 2);
	REG_WRITE1(sc, VGA_CRTC_ADDRESS, VGA_CRTC_START_HORIZ_RETRACE);
	REG_WRITE1(sc, VGA_CRTC_DATA, 0x54);			/* 672 */
	REG_WRITE1(sc, VGA_CRTC_ADDRESS, VGA_CRTC_END_HORIZ_RETRACE);
	REG_WRITE1(sc, VGA_CRTC_DATA, VGA_CRTC_EHR_EHB + 0);
	REG_WRITE1(sc, VGA_CRTC_ADDRESS, VGA_CRTC_VERT_TOTAL);
	REG_WRITE1(sc, VGA_CRTC_DATA, 0x0b);			/* 523 */
	REG_WRITE1(sc, VGA_CRTC_ADDRESS, VGA_CRTC_OVERFLOW);
	REG_WRITE1(sc, VGA_CRTC_DATA, VGA_CRTC_OF_VT9 | VGA_CRTC_OF_LC8 |
	    VGA_CRTC_OF_VBS8 | VGA_CRTC_OF_VRS8 | VGA_CRTC_OF_VDE8);
	REG_WRITE1(sc, VGA_CRTC_ADDRESS, VGA_CRTC_MAX_SCAN_LINE);
	REG_WRITE1(sc, VGA_CRTC_DATA, VGA_CRTC_MSL_LC9);
	REG_WRITE1(sc, VGA_CRTC_ADDRESS, VGA_CRTC_VERT_RETRACE_START);
	REG_WRITE1(sc, VGA_CRTC_DATA, 0xea);			/* 480 + 10 */
	REG_WRITE1(sc, VGA_CRTC_ADDRESS, VGA_CRTC_VERT_RETRACE_END);
	REG_WRITE1(sc, VGA_CRTC_DATA, 0x0c);
	REG_WRITE1(sc, VGA_CRTC_ADDRESS, VGA_CRTC_VERT_DISPLAY_END);
	REG_WRITE1(sc, VGA_CRTC_DATA, 0xdf);			/* 480 - 1*/
	REG_WRITE1(sc, VGA_CRTC_ADDRESS, VGA_CRTC_OFFSET);
	REG_WRITE1(sc, VGA_CRTC_DATA, 0x28);
	REG_WRITE1(sc, VGA_CRTC_ADDRESS, VGA_CRTC_START_VERT_BLANK);
	REG_WRITE1(sc, VGA_CRTC_DATA, 0xe7);			/* 480 + 7 */
	REG_WRITE1(sc, VGA_CRTC_ADDRESS, VGA_CRTC_END_VERT_BLANK);
	REG_WRITE1(sc, VGA_CRTC_DATA, 0x04);
	REG_WRITE1(sc, VGA_CRTC_ADDRESS, VGA_CRTC_MODE_CONTROL);
	REG_WRITE1(sc, VGA_CRTC_DATA, VGA_CRTC_MC_WB | VGA_CRTC_MC_AW |
	    VGA_CRTC_MC_SRS | VGA_CRTC_MC_CMS);
	REG_WRITE1(sc, VGA_CRTC_ADDRESS, VGA_CRTC_LINE_COMPARE);
	REG_WRITE1(sc, VGA_CRTC_DATA, 0xff);			/* 480 + 31 */

	REG_WRITE1(sc, VGA_GEN_FEATURE_CTRL_W, 0);

	REG_WRITE1(sc, VGA_SEQ_ADDRESS, VGA_SEQ_MAP_MASK);
	REG_WRITE1(sc, VGA_SEQ_DATA, VGA_SEQ_MM_EM3 | VGA_SEQ_MM_EM2 |
	    VGA_SEQ_MM_EM1 | VGA_SEQ_MM_EM0);
	REG_WRITE1(sc, VGA_SEQ_ADDRESS, VGA_SEQ_CHAR_MAP_SELECT);
	REG_WRITE1(sc, VGA_SEQ_DATA, 0);

	REG_WRITE1(sc, VGA_GC_ADDRESS, VGA_GC_SET_RESET);
	REG_WRITE1(sc, VGA_GC_DATA, 0);
	REG_WRITE1(sc, VGA_GC_ADDRESS, VGA_GC_ENABLE_SET_RESET);
	REG_WRITE1(sc, VGA_GC_DATA, 0x0f);
	REG_WRITE1(sc, VGA_GC_ADDRESS, VGA_GC_COLOR_COMPARE);
	REG_WRITE1(sc, VGA_GC_DATA, 0);
	REG_WRITE1(sc, VGA_GC_ADDRESS, VGA_GC_DATA_ROTATE);
	REG_WRITE1(sc, VGA_GC_DATA, 0);
	REG_WRITE1(sc, VGA_GC_ADDRESS, VGA_GC_READ_MAP_SELECT);
	REG_WRITE1(sc, VGA_GC_DATA, 0);
	REG_WRITE1(sc, VGA_GC_ADDRESS, VGA_GC_MODE);
	REG_WRITE1(sc, VGA_GC_DATA, 0);
	REG_WRITE1(sc, VGA_GC_ADDRESS, VGA_GC_COLOR_DONT_CARE);
	REG_WRITE1(sc, VGA_GC_DATA, 0x0f);
	REG_WRITE1(sc, VGA_GC_ADDRESS, VGA_GC_BIT_MASK);
	REG_WRITE1(sc, VGA_GC_DATA, 0xff);
}

static void
vga_initialize(struct vt_device *vd, int textmode)
{
	struct vga_softc *sc = vd->vd_softc;
	uint8_t x;

	/* Make sure the VGA adapter is not in monochrome emulation mode. */
	x = REG_READ1(sc, VGA_GEN_MISC_OUTPUT_R);
	REG_WRITE1(sc, VGA_GEN_MISC_OUTPUT_W, x | VGA_GEN_MO_IOA);

	/* Unprotect CRTC registers 0-7. */
	REG_WRITE1(sc, VGA_CRTC_ADDRESS, VGA_CRTC_VERT_RETRACE_END);
	x = REG_READ1(sc, VGA_CRTC_DATA);
	REG_WRITE1(sc, VGA_CRTC_DATA, x & ~VGA_CRTC_VRE_PR);

	/*
	 * Wait for the vertical retrace.
	 * NOTE: this code reads the VGA_GEN_INPUT_STAT_1 register, which has
	 * the side-effect of clearing the internal flip-flip of the attribute
	 * controller's write register. This means that because this code is
	 * here, we know for sure that the first write to the attribute
	 * controller will be a write to the address register. Removing this
	 * code therefore also removes that guarantee and appropriate measures
	 * need to be taken.
	 */
	do {
		x = REG_READ1(sc, VGA_GEN_INPUT_STAT_1);
		x &= VGA_GEN_IS1_VR | VGA_GEN_IS1_DE;
	} while (x != (VGA_GEN_IS1_VR | VGA_GEN_IS1_DE));

	/* Now, disable the sync. signals. */
	REG_WRITE1(sc, VGA_CRTC_ADDRESS, VGA_CRTC_MODE_CONTROL);
	x = REG_READ1(sc, VGA_CRTC_DATA);
	REG_WRITE1(sc, VGA_CRTC_DATA, x & ~VGA_CRTC_MC_HR);

	/* Asynchronous sequencer reset. */
	REG_WRITE1(sc, VGA_SEQ_ADDRESS, VGA_SEQ_RESET);
	REG_WRITE1(sc, VGA_SEQ_DATA, VGA_SEQ_RST_SR);

	if (!textmode)
		vga_initialize_graphics(vd);

	REG_WRITE1(sc, VGA_CRTC_ADDRESS, VGA_CRTC_PRESET_ROW_SCAN);
	REG_WRITE1(sc, VGA_CRTC_DATA, 0);
	REG_WRITE1(sc, VGA_CRTC_ADDRESS, VGA_CRTC_CURSOR_START);
	REG_WRITE1(sc, VGA_CRTC_DATA, VGA_CRTC_CS_COO);
	REG_WRITE1(sc, VGA_CRTC_ADDRESS, VGA_CRTC_CURSOR_END);
	REG_WRITE1(sc, VGA_CRTC_DATA, 0);
	REG_WRITE1(sc, VGA_CRTC_ADDRESS, VGA_CRTC_START_ADDR_HIGH);
	REG_WRITE1(sc, VGA_CRTC_DATA, 0);
	REG_WRITE1(sc, VGA_CRTC_ADDRESS, VGA_CRTC_START_ADDR_LOW);
	REG_WRITE1(sc, VGA_CRTC_DATA, 0);
	REG_WRITE1(sc, VGA_CRTC_ADDRESS, VGA_CRTC_CURSOR_LOC_HIGH);
	REG_WRITE1(sc, VGA_CRTC_DATA, 0);
	REG_WRITE1(sc, VGA_CRTC_ADDRESS, VGA_CRTC_CURSOR_LOC_LOW);
	REG_WRITE1(sc, VGA_CRTC_DATA, 0x59);
	REG_WRITE1(sc, VGA_CRTC_ADDRESS, VGA_CRTC_UNDERLINE_LOC);
	REG_WRITE1(sc, VGA_CRTC_DATA, VGA_CRTC_UL_UL);

	if (textmode) {
		/* Set the attribute controller to blink disable. */
		REG_WRITE1(sc, VGA_AC_WRITE, VGA_AC_MODE_CONTROL);
		REG_WRITE1(sc, VGA_AC_WRITE, 0);
	} else {
		/* Set the attribute controller in graphics mode. */
		REG_WRITE1(sc, VGA_AC_WRITE, VGA_AC_MODE_CONTROL);
		REG_WRITE1(sc, VGA_AC_WRITE, VGA_AC_MC_GA);
		REG_WRITE1(sc, VGA_AC_WRITE, VGA_AC_HORIZ_PIXEL_PANNING);
		REG_WRITE1(sc, VGA_AC_WRITE, 0);
	}
	REG_WRITE1(sc, VGA_AC_WRITE, VGA_AC_PALETTE(0));
	REG_WRITE1(sc, VGA_AC_WRITE, 0);
	REG_WRITE1(sc, VGA_AC_WRITE, VGA_AC_PALETTE(1));
	REG_WRITE1(sc, VGA_AC_WRITE, VGA_AC_PAL_R);
	REG_WRITE1(sc, VGA_AC_WRITE, VGA_AC_PALETTE(2));
	REG_WRITE1(sc, VGA_AC_WRITE, VGA_AC_PAL_G);
	REG_WRITE1(sc, VGA_AC_WRITE, VGA_AC_PALETTE(3));
	REG_WRITE1(sc, VGA_AC_WRITE, VGA_AC_PAL_SG | VGA_AC_PAL_R);
	REG_WRITE1(sc, VGA_AC_WRITE, VGA_AC_PALETTE(4));
	REG_WRITE1(sc, VGA_AC_WRITE, VGA_AC_PAL_B);
	REG_WRITE1(sc, VGA_AC_WRITE, VGA_AC_PALETTE(5));
	REG_WRITE1(sc, VGA_AC_WRITE, VGA_AC_PAL_R | VGA_AC_PAL_B);
	REG_WRITE1(sc, VGA_AC_WRITE, VGA_AC_PALETTE(6));
	REG_WRITE1(sc, VGA_AC_WRITE, VGA_AC_PAL_G | VGA_AC_PAL_B);
	REG_WRITE1(sc, VGA_AC_WRITE, VGA_AC_PALETTE(7));
	REG_WRITE1(sc, VGA_AC_WRITE, VGA_AC_PAL_R | VGA_AC_PAL_G | VGA_AC_PAL_B);
	REG_WRITE1(sc, VGA_AC_WRITE, VGA_AC_PALETTE(8));
	REG_WRITE1(sc, VGA_AC_WRITE, VGA_AC_PAL_SR | VGA_AC_PAL_SG |
	    VGA_AC_PAL_SB);
	REG_WRITE1(sc, VGA_AC_WRITE, VGA_AC_PALETTE(9));
	REG_WRITE1(sc, VGA_AC_WRITE, VGA_AC_PAL_SR | VGA_AC_PAL_SG |
	    VGA_AC_PAL_SB | VGA_AC_PAL_R);
	REG_WRITE1(sc, VGA_AC_WRITE, VGA_AC_PALETTE(10));
	REG_WRITE1(sc, VGA_AC_WRITE, VGA_AC_PAL_SR | VGA_AC_PAL_SG |
	    VGA_AC_PAL_SB | VGA_AC_PAL_G);
	REG_WRITE1(sc, VGA_AC_WRITE, VGA_AC_PALETTE(11));
	REG_WRITE1(sc, VGA_AC_WRITE, VGA_AC_PAL_SR | VGA_AC_PAL_SG |
	    VGA_AC_PAL_SB | VGA_AC_PAL_R | VGA_AC_PAL_G);
	REG_WRITE1(sc, VGA_AC_WRITE, VGA_AC_PALETTE(12));
	REG_WRITE1(sc, VGA_AC_WRITE, VGA_AC_PAL_SR | VGA_AC_PAL_SG |
	    VGA_AC_PAL_SB | VGA_AC_PAL_B);
	REG_WRITE1(sc, VGA_AC_WRITE, VGA_AC_PALETTE(13));
	REG_WRITE1(sc, VGA_AC_WRITE, VGA_AC_PAL_SR | VGA_AC_PAL_SG |
	    VGA_AC_PAL_SB | VGA_AC_PAL_R | VGA_AC_PAL_B);
	REG_WRITE1(sc, VGA_AC_WRITE, VGA_AC_PALETTE(14));
	REG_WRITE1(sc, VGA_AC_WRITE, VGA_AC_PAL_SR | VGA_AC_PAL_SG |
	    VGA_AC_PAL_SB | VGA_AC_PAL_G | VGA_AC_PAL_B);
	REG_WRITE1(sc, VGA_AC_WRITE, VGA_AC_PALETTE(15));
	REG_WRITE1(sc, VGA_AC_WRITE, VGA_AC_PAL_SR | VGA_AC_PAL_SG |
	    VGA_AC_PAL_SB | VGA_AC_PAL_R | VGA_AC_PAL_G | VGA_AC_PAL_B);
	REG_WRITE1(sc, VGA_AC_WRITE, VGA_AC_OVERSCAN_COLOR);
	REG_WRITE1(sc, VGA_AC_WRITE, 0);
	REG_WRITE1(sc, VGA_AC_WRITE, VGA_AC_COLOR_PLANE_ENABLE);
	REG_WRITE1(sc, VGA_AC_WRITE, 0x0f);
	REG_WRITE1(sc, VGA_AC_WRITE, VGA_AC_COLOR_SELECT);
	REG_WRITE1(sc, VGA_AC_WRITE, 0);

	if (!textmode) {
		u_int ofs;

		/*
		 * Done.  Clear the frame buffer.  All bit planes are
		 * enabled, so a single-paged loop should clear all
		 * planes.
		 */
		for (ofs = 0; ofs < VT_VGA_MEMSIZE; ofs++) {
			MEM_READ1(sc, ofs);
			MEM_WRITE1(sc, ofs, 0);
		}
	}

	/* Re-enable the sequencer. */
	REG_WRITE1(sc, VGA_SEQ_ADDRESS, VGA_SEQ_RESET);
	REG_WRITE1(sc, VGA_SEQ_DATA, VGA_SEQ_RST_SR | VGA_SEQ_RST_NAR);
	/* Re-enable the sync signals. */
	REG_WRITE1(sc, VGA_CRTC_ADDRESS, VGA_CRTC_MODE_CONTROL);
	x = REG_READ1(sc, VGA_CRTC_DATA);
	REG_WRITE1(sc, VGA_CRTC_DATA, x | VGA_CRTC_MC_HR);

	if (!textmode) {
		/* Switch to write mode 3, because we'll mainly do bitblt. */
		REG_WRITE1(sc, VGA_GC_ADDRESS, VGA_GC_MODE);
		REG_WRITE1(sc, VGA_GC_DATA, 3);
		REG_WRITE1(sc, VGA_GC_ADDRESS, VGA_GC_ENABLE_SET_RESET);
		REG_WRITE1(sc, VGA_GC_DATA, 0x0f);
	}
}

static int
vga_probe(struct vt_device *vd)
{

	return (CN_INTERNAL);
}

static int
vga_init(struct vt_device *vd)
{
	struct vga_softc *sc;
	int textmode;

	if (vd->vd_softc == NULL)
		vd->vd_softc = (void *)&vga_conssoftc;
	sc = vd->vd_softc;
	textmode = 0;

#if defined(__amd64__) || defined(__i386__)
	sc->vga_fb_tag = X86_BUS_SPACE_MEM;
	sc->vga_fb_handle = KERNBASE + VGA_MEM_BASE;
	sc->vga_reg_tag = X86_BUS_SPACE_IO;
	sc->vga_reg_handle = VGA_REG_BASE;
#else
# error "Architecture not yet supported!"
#endif

	TUNABLE_INT_FETCH("hw.vga.textmode", &textmode);
	if (textmode) {
		vd->vd_flags |= VDF_TEXTMODE;
		vd->vd_width = 80;
		vd->vd_height = 25;
	} else {
		vd->vd_width = VT_VGA_WIDTH;
		vd->vd_height = VT_VGA_HEIGHT;
	}
	vga_initialize(vd, textmode);

	return (CN_INTERNAL);
}

static void
vga_postswitch(struct vt_device *vd)
{

	/* Reinit VGA mode, to restore view after app which change mode. */
	vga_initialize(vd, (vd->vd_flags & VDF_TEXTMODE));
	/* Ask vt(9) to update chars on visible area. */
	vd->vd_flags |= VDF_INVALID;
}
