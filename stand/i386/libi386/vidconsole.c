/*-
 * Copyright (c) 1998 Michael Smith (msmith@freebsd.org)
 * Copyright (c) 1997 Kazutaka YOKOTA (yokota@zodiac.mech.utsunomiya-u.ac.jp)
 * All rights reserved.
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
 * 	Id: probe_keyboard.c,v 1.13 1997/06/09 05:10:55 bde Exp
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <stand.h>
#include <sys/param.h>
#include <bootstrap.h>
#include <btxv86.h>
#include <gfx_fb.h>
#include <teken.h>
#include <stdbool.h>
#include "vbe.h"

#include <dev/vt/hw/vga/vt_vga_reg.h>

#include "libi386.h"

#if KEYBOARD_PROBE
static int	probe_keyboard(void);
#endif
static void	vidc_probe(struct console *cp);
static int	vidc_init(int arg);
static void	vidc_putchar(int c);
static int	vidc_getchar(void);
static int	vidc_ischar(void);
static void	cons_draw_frame(teken_attr_t *);

static bool	vidc_started;
static uint16_t	*vgatext;

static tf_bell_t	vidc_cons_bell;
static tf_cursor_t	vidc_text_cursor;
static tf_putchar_t	vidc_text_putchar;
static tf_fill_t	vidc_text_fill;
static tf_copy_t	vidc_text_copy;
static tf_param_t	vidc_text_param;
static tf_respond_t	vidc_cons_respond;
 
static teken_funcs_t tf = {
	.tf_bell	= vidc_cons_bell,
	.tf_cursor	= vidc_text_cursor,
	.tf_putchar	= vidc_text_putchar,
	.tf_fill	= vidc_text_fill,
	.tf_copy	= vidc_text_copy,
	.tf_param	= vidc_text_param,
	.tf_respond	= vidc_cons_respond,
};

static teken_funcs_t tfx = {
	.tf_bell	= vidc_cons_bell,
	.tf_cursor	= gfx_fb_cursor,
	.tf_putchar	= gfx_fb_putchar,
	.tf_fill	= gfx_fb_fill,
	.tf_copy	= gfx_fb_copy,
	.tf_param	= gfx_fb_param,
	.tf_respond	= vidc_cons_respond,
};

#define	KEYBUFSZ	10

static uint8_t	keybuf[KEYBUFSZ];	/* keybuf for extended codes */

struct console vidconsole = {
	.c_name = "vidconsole",
	.c_desc = "internal video/keyboard",
	.c_flags = 0,
	.c_probe = vidc_probe,
	.c_init = vidc_init,
	.c_out = vidc_putchar,
	.c_in = vidc_getchar,
	.c_ready = vidc_ischar
};

/*
 * This function is used to mark a rectangular image area so the scrolling
 * will know we need to copy the data from there.
 */
void
term_image_display(teken_gfx_t *state, const teken_rect_t *r)
{
	teken_pos_t p;
	int idx;

	if (screen_buffer == NULL)
		return;

	for (p.tp_row = r->tr_begin.tp_row;
	    p.tp_row < r->tr_end.tp_row; p.tp_row++) {
                for (p.tp_col = r->tr_begin.tp_col;
                    p.tp_col < r->tr_end.tp_col; p.tp_col++) {
			idx = p.tp_col + p.tp_row * state->tg_tp.tp_col;
			if (idx >= state->tg_tp.tp_col * state->tg_tp.tp_row)
				return;
			screen_buffer[idx].a.ta_format |= TF_IMAGE;
		}
	}
}

static void
vidc_text_set_cursor(teken_unit_t row, teken_unit_t col, bool visible)
{
        uint16_t addr;
        uint8_t msl, s, e;

        msl = vga_get_crtc(VGA_REG_BASE, VGA_CRTC_MAX_SCAN_LINE) & 0x1f;
        s = vga_get_crtc(VGA_REG_BASE, VGA_CRTC_CURSOR_START) & 0xC0;
        e = vga_get_crtc(VGA_REG_BASE, VGA_CRTC_CURSOR_END);

        if (visible == true) {
                addr = row * TEXT_COLS + col;
                vga_set_crtc(VGA_REG_BASE, VGA_CRTC_CURSOR_LOC_HIGH, addr >> 8);
                vga_set_crtc(VGA_REG_BASE, VGA_CRTC_CURSOR_LOC_LOW,
		    addr & 0xff);
                e = msl;
        } else {
                s |= (1<<5);
        }
        vga_set_crtc(VGA_REG_BASE, VGA_CRTC_CURSOR_START, s);
        vga_set_crtc(VGA_REG_BASE, VGA_CRTC_CURSOR_END, e);
}

static void
vidc_text_get_cursor(teken_unit_t *row, teken_unit_t *col)
{
	uint16_t addr;

	addr = (vga_get_crtc(VGA_REG_BASE, VGA_CRTC_CURSOR_LOC_HIGH) << 8) +
	    vga_get_crtc(VGA_REG_BASE, VGA_CRTC_CURSOR_LOC_LOW);

	*row = addr / TEXT_COLS;
	*col = addr % TEXT_COLS;
}

/*
 * Not implemented.
 */
static void 
vidc_cons_bell(void *s __unused)
{
}

static void
vidc_text_cursor(void *s __unused, const teken_pos_t *p)
{
	teken_unit_t row, col;

	if (p->tp_col == TEXT_COLS)
		col = p->tp_col - 1;
	else
		col = p->tp_col;

	if (p->tp_row == TEXT_ROWS)
		row = p->tp_row - 1;
	else
		row = p->tp_row;

	vidc_text_set_cursor(row, col, true);
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
	{ 0x266c, 0x0e, 0x00 }
};

static uint8_t
vga_get_cp437(teken_char_t c)
{
	int min, mid, max;

	min = 0;
	max = (sizeof(cp437table) / sizeof(struct unicp437)) - 1;

	if (c < cp437table[0].unicode_base ||
	    c > cp437table[max].unicode_base + cp437table[max].length)
		return ('?');

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

	return ('?');
}

static void
vidc_text_printchar(teken_gfx_t *state, const teken_pos_t *p)
{
	int idx;
	uint8_t attr;
	struct text_pixel *px;
	teken_color_t fg, bg, tmp;
	struct cgatext {
		uint8_t ch;
		uint8_t attr;
	} *addr;

	idx = p->tp_col + p->tp_row * state->tg_tp.tp_col;
	px = &screen_buffer[idx];
	fg = teken_256to16(px->a.ta_fgcolor);
	bg = teken_256to16(px->a.ta_bgcolor);
	if (px->a.ta_format & TF_BOLD)
		fg |= TC_LIGHT;
	if (px->a.ta_format & TF_BLINK)
		bg |= TC_LIGHT;

	if (px->a.ta_format & TF_REVERSE) {
		tmp = fg;
		fg = bg;
		bg = tmp;
	}

	attr = (cmap[bg & 0xf] << 4) | cmap[fg & 0xf];
	addr = (struct cgatext *)vgatext;
	addr[idx].ch = vga_get_cp437(px->c);
	addr[idx].attr = attr;
}

static void
vidc_text_putchar(void *s, const teken_pos_t *p, teken_char_t c,
    const teken_attr_t *a)
{
	teken_gfx_t *state = s;
	int attr, idx;

	idx = p->tp_col + p->tp_row * state->tg_tp.tp_col;
	if (idx >= state->tg_tp.tp_col * state->tg_tp.tp_row)
		return;

	screen_buffer[idx].c = c;
	screen_buffer[idx].a = *a;

	vidc_text_printchar(state, p);
}

static void
vidc_text_fill(void *arg, const teken_rect_t *r, teken_char_t c,
    const teken_attr_t *a)
{
	teken_gfx_t *state = arg;
	teken_pos_t p;
	teken_unit_t row, col;

	vidc_text_get_cursor(&row, &col);
	vidc_text_set_cursor(row, col, false);
	for (p.tp_row = r->tr_begin.tp_row; p.tp_row < r->tr_end.tp_row;
	    p.tp_row++)
		for (p.tp_col = r->tr_begin.tp_col;
		    p.tp_col < r->tr_end.tp_col; p.tp_col++)
			vidc_text_putchar(state, &p, c, a);
	vidc_text_set_cursor(row, col, true);
}

static void
vidc_text_copy(void *ptr, const teken_rect_t *r, const teken_pos_t *p)
{
	teken_gfx_t *state = ptr;
	int srow, drow;
	int nrow, ncol, x, y; /* Has to be signed - >= 0 comparison */
	teken_pos_t d, s;
	teken_unit_t row, col;

	/*
	 * Copying is a little tricky. We must make sure we do it in
	 * correct order, to make sure we don't overwrite our own data.
	 */

	nrow = r->tr_end.tp_row - r->tr_begin.tp_row;
	ncol = r->tr_end.tp_col - r->tr_begin.tp_col;

	vidc_text_get_cursor(&row, &col);
	vidc_text_set_cursor(row, col, false);
	if (p->tp_row < r->tr_begin.tp_row) {
		/* Copy from bottom to top. */
		for (y = 0; y < nrow; y++) {
			d.tp_row = p->tp_row + y;
			s.tp_row = r->tr_begin.tp_row + y;
			drow = d.tp_row * state->tg_tp.tp_col;
			srow = s.tp_row * state->tg_tp.tp_col;
			for (x = 0; x < ncol; x++) {
				d.tp_col = p->tp_col + x;
				s.tp_col = r->tr_begin.tp_col + x;

				if (!is_same_pixel(
				    &screen_buffer[d.tp_col + drow],
				    &screen_buffer[s.tp_col + srow])) {
					screen_buffer[d.tp_col + drow] =
					    screen_buffer[s.tp_col + srow];
					vidc_text_printchar(state, &d);
				}
			}
		}
	} else {
		/* Copy from top to bottom. */
		if (p->tp_col < r->tr_begin.tp_col) {
			/* Copy from right to left. */
			for (y = nrow - 1; y >= 0; y--) {
				d.tp_row = p->tp_row + y;
				s.tp_row = r->tr_begin.tp_row + y;
				drow = d.tp_row * state->tg_tp.tp_col;
				srow = s.tp_row * state->tg_tp.tp_col;
				for (x = 0; x < ncol; x++) {
					d.tp_col = p->tp_col + x;
					s.tp_col = r->tr_begin.tp_col + x;

					if (!is_same_pixel(
					    &screen_buffer[d.tp_col + drow],
					    &screen_buffer[s.tp_col + srow])) {
						screen_buffer[d.tp_col + drow] =
						    screen_buffer[s.tp_col + srow];
						vidc_text_printchar(state, &d);
					}
				}
			}
		} else {
			/* Copy from left to right. */
			for (y = nrow - 1; y >= 0; y--) {
				d.tp_row = p->tp_row + y;
				s.tp_row = r->tr_begin.tp_row + y;
				drow = d.tp_row * state->tg_tp.tp_col;
				srow = s.tp_row * state->tg_tp.tp_col;
				for (x = ncol - 1; x >= 0; x--) {
					d.tp_col = p->tp_col + x;
					s.tp_col = r->tr_begin.tp_col + x;

					if (!is_same_pixel(
					    &screen_buffer[d.tp_col + drow],
					    &screen_buffer[s.tp_col + srow])) {
						screen_buffer[d.tp_col + drow] =
						    screen_buffer[s.tp_col + srow];
						vidc_text_printchar(state, &d);
					}
				}
			}
		}
	}
	vidc_text_set_cursor(row, col, true);
}

static void
vidc_text_param(void *arg, int cmd, unsigned int value)
{
	teken_gfx_t *state = arg;
	teken_unit_t row, col;

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
		vidc_text_get_cursor(&row, &col);
		if (value != 0) {
			vidc_text_set_cursor(row, col, true);
			state->tg_cursor_visible = true;
		} else {
			vidc_text_set_cursor(row, col, false);
			state->tg_cursor_visible = false;
		}
		break;
	default:
		/* Not yet implemented */
		break;
	}
}

/*
 * Not implemented.
 */
static void
vidc_cons_respond(void *s __unused, const void *buf __unused,
    size_t len __unused)
{
}

static void
vidc_probe(struct console *cp)
{
    
    /* look for a keyboard */
#if KEYBOARD_PROBE
    if (probe_keyboard())
#endif
    {
	
	cp->c_flags |= C_PRESENTIN;
    }

    /* XXX for now, always assume we can do BIOS screen output */
    cp->c_flags |= C_PRESENTOUT;
}

static bool
color_name_to_teken(const char *name, int *val)
{
	int light = 0;
	if (strncasecmp(name, "light", 5) == 0) {
		name += 5;
		light = TC_LIGHT;
	} else if (strncasecmp(name, "bright", 6) == 0) {
		name += 6;
		light = TC_LIGHT;
	}
	if (strcasecmp(name, "black") == 0) {
		*val = TC_BLACK | light;
		return (true);
	}
	if (strcasecmp(name, "red") == 0) {
		*val = TC_RED | light;
		return (true);
	}
	if (strcasecmp(name, "green") == 0) {
		*val = TC_GREEN | light;
		return (true);
	}
	if (strcasecmp(name, "yellow") == 0 || strcasecmp(name, "brown") == 0) {
		*val = TC_YELLOW | light;
		return (true);
	}
	if (strcasecmp(name, "blue") == 0) {
		*val = TC_BLUE | light;
		return (true);
	}
	if (strcasecmp(name, "magenta") == 0) {
		*val = TC_MAGENTA | light;
		return (true);
	}
	if (strcasecmp(name, "cyan") == 0) {
		*val = TC_CYAN | light;
		return (true);
	}
	if (strcasecmp(name, "white") == 0) {
		*val = TC_WHITE | light;
		return (true);
	}
	return (false);
}

static int
vidc_set_colors(struct env_var *ev, int flags, const void *value)
{
	int val = 0;
	char buf[3];
	const void *evalue;
	const teken_attr_t *ap;
	teken_attr_t a;

	if (value == NULL)
		return (CMD_OK);

	if (color_name_to_teken(value, &val)) {
		snprintf(buf, sizeof (buf), "%d", val);
		evalue = buf;
	} else {
		char *end;
		long lval;

		errno = 0;
		lval = strtol(value, &end, 0);
		if (errno != 0 || *end != '\0' || lval < 0 || lval > 15) {
			printf("Allowed values are either ansi color name or "
			    "number from range [0-15].\n");
			return (CMD_OK);
		}
		val = (int)lval;
		evalue = value;
	}

	ap = teken_get_defattr(&gfx_state.tg_teken);
	a = *ap;
	if (strcmp(ev->ev_name, "teken.fg_color") == 0) {
		/* is it already set? */
		if (ap->ta_fgcolor == val)
			return (CMD_OK);
		a.ta_fgcolor = val;
	}
	if (strcmp(ev->ev_name, "teken.bg_color") == 0) {
		/* is it already set? */
		if (ap->ta_bgcolor == val)
			return (CMD_OK);
		a.ta_bgcolor = val;
	}

	/* Improve visibility */
	if (a.ta_bgcolor == TC_WHITE)
		a.ta_bgcolor |= TC_LIGHT;

	teken_set_defattr(&gfx_state.tg_teken, &a);
	cons_draw_frame(&a);
	env_setenv(ev->ev_name, flags | EV_NOHOOK, evalue, NULL, NULL);
	teken_input(&gfx_state.tg_teken, "\e[2J", 4);

	return (CMD_OK);
}

static int
env_screen_nounset(struct env_var *ev __unused)
{
	if (gfx_state.tg_fb_type == FB_TEXT)
		return (0);
	return (EPERM);
}

static int
vidc_load_palette(void)
{
	int i, roff, goff, boff, rc;

	if (pe8 == NULL)
		pe8 = calloc(sizeof(*pe8), NCMAP);
	if (pe8 == NULL)
		return (ENOMEM);

	/* Generate VGA colors */
	roff = ffs(gfx_state.tg_fb.fb_mask_red) - 1;
	goff = ffs(gfx_state.tg_fb.fb_mask_green) - 1;
	boff = ffs(gfx_state.tg_fb.fb_mask_blue) - 1;
	rc = generate_cons_palette((uint32_t *)pe8, COLOR_FORMAT_RGB,
	    gfx_state.tg_fb.fb_mask_red >> roff, roff,
	    gfx_state.tg_fb.fb_mask_green >> goff, goff,
	    gfx_state.tg_fb.fb_mask_blue >> boff, boff);

	if (rc == 0) {
		for (i = 0; i < NCMAP; i++) {
			int idx;

			if (i < NCOLORS)
				idx = cons_to_vga_colors[i];
			else
				idx = i;

			rc = vbe_set_palette(&pe8[i], idx);
			if (rc != 0)
				break;
		}
	}
	return (rc);
}

static void
cons_draw_frame(teken_attr_t *a)
{
	teken_attr_t attr = *a;
	teken_color_t fg = a->ta_fgcolor;

	attr.ta_fgcolor = attr.ta_bgcolor;
	teken_set_defattr(&gfx_state.tg_teken, &attr);

	gfx_fb_drawrect(0, 0, gfx_state.tg_fb.fb_width,
	    gfx_state.tg_origin.tp_row, 1);
	gfx_fb_drawrect(0,
	    gfx_state.tg_fb.fb_height - gfx_state.tg_origin.tp_row - 1,
	    gfx_state.tg_fb.fb_width, gfx_state.tg_fb.fb_height, 1);
	gfx_fb_drawrect(0, gfx_state.tg_origin.tp_row,
	    gfx_state.tg_origin.tp_col,
	    gfx_state.tg_fb.fb_height - gfx_state.tg_origin.tp_row - 1, 1);
	gfx_fb_drawrect(
	    gfx_state.tg_fb.fb_width - gfx_state.tg_origin.tp_col - 1,
	    gfx_state.tg_origin.tp_row, gfx_state.tg_fb.fb_width,
	    gfx_state.tg_fb.fb_height, 1);

	attr.ta_fgcolor = fg;
	teken_set_defattr(&gfx_state.tg_teken, &attr);
}

/*
 * Binary searchable table for CP437 to Unicode conversion.
 */
struct cp437uni {
	uint8_t		cp437_base;
	uint16_t	unicode_base;
	uint8_t		length;
};

static const struct cp437uni cp437unitable[] = {
	{   0, 0x0000, 0 }, {   1, 0x263A, 1 }, {   3, 0x2665, 1 },
	{   5, 0x2663, 0 }, {   6, 0x2660, 0 }, {   7, 0x2022, 0 },
	{   8, 0x25D8, 0 }, {   9, 0x25CB, 0 }, {  10, 0x25D9, 0 },
	{  11, 0x2642, 0 }, {  12, 0x2640, 0 }, {  13, 0x266A, 1 },
	{  15, 0x263C, 0 }, {  16, 0x25BA, 0 }, {  17, 0x25C4, 0 },
	{  18, 0x2195, 0 }, {  19, 0x203C, 0 }, {  20, 0x00B6, 0 },
	{  21, 0x00A7, 0 }, {  22, 0x25AC, 0 }, {  23, 0x21A8, 0 },
	{  24, 0x2191, 0 }, {  25, 0x2193, 0 }, {  26, 0x2192, 0 },
	{  27, 0x2190, 0 }, {  28, 0x221F, 0 }, {  29, 0x2194, 0 },
	{  30, 0x25B2, 0 }, {  31, 0x25BC, 0 }, {  32, 0x0020, 0x5e },
	{ 127, 0x2302, 0 }, { 128, 0x00C7, 0 }, { 129, 0x00FC, 0 },
	{ 130, 0x00E9, 0 }, { 131, 0x00E2, 0 }, { 132, 0x00E4, 0 },
	{ 133, 0x00E0, 0 }, { 134, 0x00E5, 0 }, { 135, 0x00E7, 0 },
	{ 136, 0x00EA, 1 }, { 138, 0x00E8, 0 }, { 139, 0x00EF, 0 },
	{ 140, 0x00EE, 0 }, { 141, 0x00EC, 0 }, { 142, 0x00C4, 1 },
	{ 144, 0x00C9, 0 }, { 145, 0x00E6, 0 }, { 146, 0x00C6, 0 },
	{ 147, 0x00F4, 0 }, { 148, 0x00F6, 0 }, { 149, 0x00F2, 0 },
	{ 150, 0x00FB, 0 }, { 151, 0x00F9, 0 }, { 152, 0x00FF, 0 },
	{ 153, 0x00D6, 0 }, { 154, 0x00DC, 0 }, { 155, 0x00A2, 1 },
	{ 157, 0x00A5, 0 }, { 158, 0x20A7, 0 }, { 159, 0x0192, 0 },
	{ 160, 0x00E1, 0 }, { 161, 0x00ED, 0 }, { 162, 0x00F3, 0 },
	{ 163, 0x00FA, 0 }, { 164, 0x00F1, 0 }, { 165, 0x00D1, 0 },
	{ 166, 0x00AA, 0 }, { 167, 0x00BA, 0 }, { 168, 0x00BF, 0 },
	{ 169, 0x2310, 0 }, { 170, 0x00AC, 0 }, { 171, 0x00BD, 0 },
	{ 172, 0x00BC, 0 }, { 173, 0x00A1, 0 }, { 174, 0x00AB, 0 },
	{ 175, 0x00BB, 0 }, { 176, 0x2591, 2 }, { 179, 0x2502, 0 },
	{ 180, 0x2524, 0 }, { 181, 0x2561, 1 }, { 183, 0x2556, 0 },
	{ 184, 0x2555, 0 }, { 185, 0x2563, 0 }, { 186, 0x2551, 0 },
	{ 187, 0x2557, 0 }, { 188, 0x255D, 0 }, { 189, 0x255C, 0 },
	{ 190, 0x255B, 0 }, { 191, 0x2510, 0 }, { 192, 0x2514, 0 },
	{ 193, 0x2534, 0 }, { 194, 0x252C, 0 }, { 195, 0x251C, 0 },
	{ 196, 0x2500, 0 }, { 197, 0x253C, 0 }, { 198, 0x255E, 1 },
	{ 200, 0x255A, 0 }, { 201, 0x2554, 0 }, { 202, 0x2569, 0 },
	{ 203, 0x2566, 0 }, { 204, 0x2560, 0 }, { 205, 0x2550, 0 },
	{ 206, 0x256C, 0 }, { 207, 0x2567, 1 }, { 209, 0x2564, 1 },
	{ 211, 0x2559, 0 }, { 212, 0x2558, 0 }, { 213, 0x2552, 1 },
	{ 215, 0x256B, 0 }, { 216, 0x256A, 0 }, { 217, 0x2518, 0 },
	{ 218, 0x250C, 0 }, { 219, 0x2588, 0 }, { 220, 0x2584, 0 },
	{ 221, 0x258C, 0 }, { 222, 0x2590, 0 }, { 223, 0x2580, 0 },
	{ 224, 0x03B1, 0 }, { 225, 0x00DF, 0 }, { 226, 0x0393, 0 },
	{ 227, 0x03C0, 0 }, { 228, 0x03A3, 0 }, { 229, 0x03C3, 0 },
	{ 230, 0x00B5, 0 }, { 231, 0x03C4, 0 }, { 232, 0x03A6, 0 },
	{ 233, 0x0398, 0 }, { 234, 0x03A9, 0 }, { 235, 0x03B4, 0 },
	{ 236, 0x221E, 0 }, { 237, 0x03C6, 0 }, { 238, 0x03B5, 0 },
	{ 239, 0x2229, 0 }, { 240, 0x2261, 0 }, { 241, 0x00B1, 0 },
	{ 242, 0x2265, 0 }, { 243, 0x2264, 0 }, { 244, 0x2320, 1 },
	{ 246, 0x00F7, 0 }, { 247, 0x2248, 0 }, { 248, 0x00B0, 0 },
	{ 249, 0x2219, 0 }, { 250, 0x00B7, 0 }, { 251, 0x221A, 0 },
	{ 252, 0x207F, 0 }, { 253, 0x00B2, 0 }, { 254, 0x25A0, 0 },
	{ 255, 0x00A0, 0 }
};

static uint16_t
vga_cp437_to_uni(uint8_t c)
{
	int min, mid, max;

	min = 0;
	max = (sizeof(cp437unitable) / sizeof(struct cp437uni)) - 1;

	while (max >= min) {
		mid = (min + max) / 2;
		if (c < cp437unitable[mid].cp437_base)
			max = mid - 1;
		else if (c > cp437unitable[mid].cp437_base +
		    cp437unitable[mid].length)
			min = mid + 1;
		else
			return (c - cp437unitable[mid].cp437_base +
			    cp437unitable[mid].unicode_base);
	}

	return ('?');
}

/*
 * install font for text mode
 */
static void
vidc_install_font(void)
{
	uint8_t reg[7];
	const uint8_t *from;
	uint8_t volatile *to;
	uint16_t c;
	int i, j, s;
	int bpc, f_offset;
	teken_attr_t a = { 0 };

	/* We can only program VGA registers. */
	if (!vbe_is_vga())
		return;

	if (gfx_state.tg_fb_type != FB_TEXT)
		return;

	/* Sync-reset the sequencer registers */
	vga_set_seq(VGA_REG_BASE, VGA_SEQ_RESET, VGA_SEQ_RST_NAR);

	reg[0] = vga_get_seq(VGA_REG_BASE, VGA_SEQ_MAP_MASK);
	reg[1] = vga_get_seq(VGA_REG_BASE, VGA_SEQ_CLOCKING_MODE);
	reg[2] = vga_get_seq(VGA_REG_BASE, VGA_SEQ_MEMORY_MODE);
	reg[3] = vga_get_grc(VGA_REG_BASE, VGA_GC_READ_MAP_SELECT);
	reg[4] = vga_get_grc(VGA_REG_BASE, VGA_GC_MODE);
	reg[5] = vga_get_grc(VGA_REG_BASE, VGA_GC_MISCELLANEOUS);
	reg[6] = vga_get_atr(VGA_REG_BASE, VGA_AC_MODE_CONTROL);

	/* Screen off */
	vga_set_seq(VGA_REG_BASE, VGA_SEQ_CLOCKING_MODE,
	    reg[1] | VGA_SEQ_CM_SO);

	/*
	 * enable write to plane2, since fonts
	 * could only be loaded into plane2
	 */
	vga_set_seq(VGA_REG_BASE, VGA_SEQ_MAP_MASK, VGA_SEQ_MM_EM2);
	/*
	 * sequentially access data in the bit map being
	 * selected by MapMask register (index 0x02)
	 */
	vga_set_seq(VGA_REG_BASE, VGA_SEQ_MEMORY_MODE, 0x07);
	/* Sync-reset ended, and allow the sequencer to operate */
	vga_set_seq(VGA_REG_BASE, VGA_SEQ_RESET,
	    VGA_SEQ_RST_SR | VGA_SEQ_RST_NAR);

	/*
	 * select plane 2 on Read Mode 0
	 */
	vga_set_grc(VGA_REG_BASE, VGA_GC_READ_MAP_SELECT, 0x02);
	/*
	 * system addresses sequentially access data, follow
	 * Memory Mode register bit 2 in the sequencer
	 */
	vga_set_grc(VGA_REG_BASE, VGA_GC_MODE, 0x00);
	/*
	 * set range of host memory addresses decoded by VGA
	 * hardware -- A0000h-BFFFFh (128K region)
	 */
	vga_set_grc(VGA_REG_BASE, VGA_GC_MISCELLANEOUS, 0x00);

	/*
	 * This assumes 8x16 characters, which yield the traditional 80x25
	 * screen.
	 */
	bpc = 16;
	s = 0;	/* font slot, maybe should use tunable there. */
	f_offset = s * 8 * 1024;
	for (i = 0; i < 256; i++) {
		c = vga_cp437_to_uni(i);
		from = font_lookup(&gfx_state.tg_font, c, &a);
		to = (unsigned char *)ptov(VGA_MEM_BASE) + f_offset +
		    i * 0x20;
		for (j = 0; j < bpc; j++)
			*to++ = *from++;
	}

	vga_set_atr(VGA_REG_BASE, VGA_AC_MODE_CONTROL, reg[6]);

	/* Sync-reset the sequencer registers */
	vga_set_seq(VGA_REG_BASE, VGA_SEQ_RESET, VGA_SEQ_RST_NAR);
	vga_set_seq(VGA_REG_BASE, VGA_SEQ_MAP_MASK, reg[0]);
	vga_set_seq(VGA_REG_BASE, VGA_SEQ_MEMORY_MODE, reg[2]);
	/* Sync-reset ended, and allow the sequencer to operate */
	vga_set_seq(VGA_REG_BASE, VGA_SEQ_RESET,
	    VGA_SEQ_RST_SR | VGA_SEQ_RST_NAR);

	/* restore graphic registers */
	vga_set_grc(VGA_REG_BASE, VGA_GC_READ_MAP_SELECT, reg[3]);
	vga_set_grc(VGA_REG_BASE, VGA_GC_MODE, reg[4]);
	vga_set_grc(VGA_REG_BASE, VGA_GC_MISCELLANEOUS, (reg[5] & 0x03) | 0x0c);

	/* Screen on */
	vga_set_seq(VGA_REG_BASE, VGA_SEQ_CLOCKING_MODE, reg[1] & 0xdf);
}

bool
cons_update_mode(bool use_gfx_mode)
{
	const teken_attr_t *a;
	teken_attr_t attr;
	char env[10], *ptr;
	int format, roff, goff, boff;

	/* vidc_init() is not called yet. */
	if (!vidc_started)
		return (false);

	gfx_state.tg_tp.tp_row = TEXT_ROWS;
	gfx_state.tg_tp.tp_col = TEXT_COLS;

	if (use_gfx_mode) {
		setup_font(&gfx_state, gfx_state.tg_fb.fb_height,
		    gfx_state.tg_fb.fb_width);
		/* Point of origin in pixels. */
		gfx_state.tg_origin.tp_row = (gfx_state.tg_fb.fb_height -
		    (gfx_state.tg_tp.tp_row * gfx_state.tg_font.vf_height)) / 2;
		gfx_state.tg_origin.tp_col = (gfx_state.tg_fb.fb_width -
		    (gfx_state.tg_tp.tp_col * gfx_state.tg_font.vf_width)) / 2;

		gfx_state.tg_glyph_size = gfx_state.tg_font.vf_height *
		    gfx_state.tg_font.vf_width * 4;
		free(gfx_state.tg_glyph);
		gfx_state.tg_glyph = malloc(gfx_state.tg_glyph_size);
		if (gfx_state.tg_glyph == NULL)
			return (false);
		gfx_state.tg_functions = &tfx;
		snprintf(env, sizeof (env), "%d", gfx_state.tg_fb.fb_height);
		env_setenv("screen.height", EV_VOLATILE | EV_NOHOOK, env,
		    env_noset, env_screen_nounset);
		snprintf(env, sizeof (env), "%d", gfx_state.tg_fb.fb_width);
		env_setenv("screen.width", EV_VOLATILE | EV_NOHOOK, env,
		    env_noset, env_screen_nounset);
		snprintf(env, sizeof (env), "%d", gfx_state.tg_fb.fb_bpp);
		env_setenv("screen.depth", EV_VOLATILE | EV_NOHOOK, env,
		    env_noset, env_screen_nounset);
	} else {
		/* Trigger loading of 8x16 font. */
		setup_font(&gfx_state,
		    16 * gfx_state.tg_fb.fb_height,
		    8 * gfx_state.tg_fb.fb_width);
		gfx_state.tg_functions = &tf;
		/* ensure the following are not set for text mode */
		unsetenv("screen.height");
		unsetenv("screen.width");
		unsetenv("screen.depth");
		vidc_install_font();
	}

	free(screen_buffer);
	screen_buffer = malloc(gfx_state.tg_tp.tp_row * gfx_state.tg_tp.tp_col *
	    sizeof(*screen_buffer));
	if (screen_buffer == NULL)
		return (false);

	teken_init(&gfx_state.tg_teken, gfx_state.tg_functions, &gfx_state);

	if (gfx_state.tg_ctype == CT_INDEXED)
		format = COLOR_FORMAT_VGA;
	else
		format = COLOR_FORMAT_RGB;

	roff = ffs(gfx_state.tg_fb.fb_mask_red) - 1;
	goff = ffs(gfx_state.tg_fb.fb_mask_green) - 1;
	boff = ffs(gfx_state.tg_fb.fb_mask_blue) - 1;
	(void) generate_cons_palette(cmap, format,
	    gfx_state.tg_fb.fb_mask_red >> roff, roff,
	    gfx_state.tg_fb.fb_mask_green >> goff, goff,
	    gfx_state.tg_fb.fb_mask_blue >> boff, boff);

	if (gfx_state.tg_ctype == CT_INDEXED && use_gfx_mode)
		vidc_load_palette();

	teken_set_winsize(&gfx_state.tg_teken, &gfx_state.tg_tp);
	a = teken_get_defattr(&gfx_state.tg_teken);
	attr = *a;

	/*
	 * On first run, we set up the vidc_set_colors()
	 * callback. If the env is already set, we
	 * pick up fg and bg color values from the environment.
	 */
	ptr = getenv("teken.fg_color");
	if (ptr != NULL) {
		attr.ta_fgcolor = strtol(ptr, NULL, 10);
		ptr = getenv("teken.bg_color");
		attr.ta_bgcolor = strtol(ptr, NULL, 10);

		teken_set_defattr(&gfx_state.tg_teken, &attr);
	} else {
		snprintf(env, sizeof(env), "%d", attr.ta_fgcolor);
		env_setenv("teken.fg_color", EV_VOLATILE, env,
		    vidc_set_colors, env_nounset);
		snprintf(env, sizeof(env), "%d", attr.ta_bgcolor);
		env_setenv("teken.bg_color", EV_VOLATILE, env,
		    vidc_set_colors, env_nounset);
	}

	/* Improve visibility */
	if (attr.ta_bgcolor == TC_WHITE)
		attr.ta_bgcolor |= TC_LIGHT;
	teken_set_defattr(&gfx_state.tg_teken, &attr);

	snprintf(env, sizeof (env), "%u", (unsigned)gfx_state.tg_tp.tp_row);
	setenv("LINES", env, 1);
	snprintf(env, sizeof (env), "%u", (unsigned)gfx_state.tg_tp.tp_col);
	setenv("COLUMNS", env, 1);

	/* Draw frame around terminal area. */
	cons_draw_frame(&attr);
	/* Erase display, this will also fill our screen buffer. */
	teken_input(&gfx_state.tg_teken, "\e[2J", 4);
	gfx_state.tg_functions->tf_param(&gfx_state, TP_SHOWCURSOR, 1);

        return (true);
}

static int
vidc_init(int arg)
{
	const teken_attr_t *a;
	int val;
	char env[8];

	if (vidc_started && arg == 0)
		return (0);

	vidc_started = true;
	vbe_init();

	/*
	 * Check Miscellaneous Output Register (Read at 3CCh, Write at 3C2h)
	 * for bit 1 (Input/Output Address Select), which means
	 * color/graphics adapter.
	 */
	if (vga_get_reg(VGA_REG_BASE, VGA_GEN_MISC_OUTPUT_R) & VGA_GEN_MO_IOA)
		vgatext = (uint16_t *)PTOV(VGA_TXT_BASE);
	else
		vgatext = (uint16_t *)PTOV(VGA_MEM_BASE + VGA_MEM_SIZE);

        /* set 16bit colors */
        val = vga_get_atr(VGA_REG_BASE, VGA_AC_MODE_CONTROL);
        val &= ~VGA_AC_MC_BI;
        val &= ~VGA_AC_MC_ELG;
        vga_set_atr(VGA_REG_BASE, VGA_AC_MODE_CONTROL, val);

#if defined(FRAMEBUFFER_MODE)
	val = vbe_default_mode();
	/* if val is not legal VBE mode, use text mode */
	if (VBE_VALID_MODE(val)) {
		if (vbe_set_mode(val) != 0)
			bios_set_text_mode(VGA_TEXT_MODE);
	}
#endif

	gfx_framework_init();

	if (!cons_update_mode(VBE_VALID_MODE(vbe_get_mode())))
		return (1);

	for (int i = 0; i < 10 && vidc_ischar(); i++)
		(void) vidc_getchar();

	return (0);	/* XXX reinit? */
}

void
vidc_biosputchar(int c)
{

    v86.ctl = 0;
    v86.addr = 0x10;
    v86.eax = 0xe00 | (c & 0xff);
    v86.ebx = 0x7;
    v86int();
}

static void
vidc_putchar(int c)
{
	unsigned char ch = c;

	if (screen_buffer != NULL)
		teken_input(&gfx_state.tg_teken, &ch, sizeof (ch));
	else
		vidc_biosputchar(c);
}

static int
vidc_getchar(void)
{
	int i, c;

	for (i = 0; i < KEYBUFSZ; i++) {
		if (keybuf[i] != 0) {
			c = keybuf[i];
			keybuf[i] = 0;
			return (c);
		}
	}

	if (vidc_ischar()) {
		v86.ctl = 0;
		v86.addr = 0x16;
		v86.eax = 0x0;
		v86int();
		if ((v86.eax & 0xff) != 0) {
			return (v86.eax & 0xff);
		}

		/* extended keys */
		switch (v86.eax & 0xff00) {
		case 0x4800:	/* up */
			keybuf[0] = '[';
			keybuf[1] = 'A';
			return (0x1b);	/* esc */
		case 0x4b00:	/* left */
			keybuf[0] = '[';
			keybuf[1] = 'D';
			return (0x1b);	/* esc */
		case 0x4d00:	/* right */
			keybuf[0] = '[';
			keybuf[1] = 'C';
			return (0x1b);	/* esc */
		case 0x5000:	/* down */
			keybuf[0] = '[';
			keybuf[1] = 'B';
			return (0x1b);	/* esc */
		default:
			return (-1);
		}
	} else {
		return (-1);
	}
}

static int
vidc_ischar(void)
{
	int i;

	for (i = 0; i < KEYBUFSZ; i++) {
		if (keybuf[i] != 0) {
			return (1);
		}
	}

	v86.ctl = V86_FLAGS;
	v86.addr = 0x16;
	v86.eax = 0x100;
	v86int();
	return (!V86_ZR(v86.efl));
}

#if KEYBOARD_PROBE

#define	PROBE_MAXRETRY	5
#define	PROBE_MAXWAIT	400
#define	IO_DUMMY	0x84
#define	IO_KBD		0x060		/* 8042 Keyboard */

/* selected defines from kbdio.h */
#define	KBD_STATUS_PORT 	4	/* status port, read */
#define	KBD_DATA_PORT		0	/* data port, read/write
					 * also used as keyboard command
					 * and mouse command port 
					 */
#define	KBDC_ECHO		0x00ee
#define	KBDS_ANY_BUFFER_FULL	0x0001
#define	KBDS_INPUT_BUFFER_FULL	0x0002
#define	KBD_ECHO		0x00ee

/* 7 microsec delay necessary for some keyboard controllers */
static void
delay7(void)
{
	/*
	 * I know this is broken, but no timer is available yet at this stage...
	 * See also comments in `delay1ms()'.
	 */
	inb(IO_DUMMY); inb(IO_DUMMY);
	inb(IO_DUMMY); inb(IO_DUMMY);
	inb(IO_DUMMY); inb(IO_DUMMY);
}

/*
 * This routine uses an inb to an unused port, the time to execute that
 * inb is approximately 1.25uS.  This value is pretty constant across
 * all CPU's and all buses, with the exception of some PCI implentations
 * that do not forward this I/O address to the ISA bus as they know it
 * is not a valid ISA bus address, those machines execute this inb in
 * 60 nS :-(.
 *
 */
static void
delay1ms(void)
{
	int i = 800;
	while (--i >= 0)
		(void) inb(0x84);
}

/*
 * We use the presence/absence of a keyboard to determine whether the internal
 * console can be used for input.
 *
 * Perform a simple test on the keyboard; issue the ECHO command and see
 * if the right answer is returned. We don't do anything as drastic as
 * full keyboard reset; it will be too troublesome and take too much time.
 */
static int
probe_keyboard(void)
{
	int retry = PROBE_MAXRETRY;
	int wait;
	int i;

	while (--retry >= 0) {
		/* flush any noise */
		while (inb(IO_KBD + KBD_STATUS_PORT) & KBDS_ANY_BUFFER_FULL) {
			delay7();
			inb(IO_KBD + KBD_DATA_PORT);
			delay1ms();
		}

		/* wait until the controller can accept a command */
		for (wait = PROBE_MAXWAIT; wait > 0; --wait) {
			if (((i = inb(IO_KBD + KBD_STATUS_PORT)) 
			    & (KBDS_INPUT_BUFFER_FULL | KBDS_ANY_BUFFER_FULL))
			    == 0)
				break;
			if (i & KBDS_ANY_BUFFER_FULL) {
				delay7();
				inb(IO_KBD + KBD_DATA_PORT);
			}
			delay1ms();
		}
		if (wait <= 0)
			continue;

		/* send the ECHO command */
		outb(IO_KBD + KBD_DATA_PORT, KBDC_ECHO);

		/* wait for a response */
		for (wait = PROBE_MAXWAIT; wait > 0; --wait) {
			if (inb(IO_KBD + KBD_STATUS_PORT) &
			    KBDS_ANY_BUFFER_FULL)
				break;
			delay1ms();
		}
		if (wait <= 0)
			continue;

		delay7();
		i = inb(IO_KBD + KBD_DATA_PORT);
#ifdef PROBE_KBD_BEBUG
		printf("probe_keyboard: got 0x%x.\n", i);
#endif
		if (i == KBD_ECHO) {
			/* got the right answer */
			return (1);
		}
	}

	return (0);
}
#endif /* KEYBOARD_PROBE */
