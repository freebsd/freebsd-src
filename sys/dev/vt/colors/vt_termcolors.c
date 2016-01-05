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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>

#include <dev/vt/colors/vt_termcolors.h>

static const struct {
	unsigned char r;	/* Red percentage value. */
	unsigned char g;	/* Green percentage value. */
	unsigned char b;	/* Blue percentage value. */
} color_def[16] = {
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

/*
 * Between console's palette and VGA's one:
 *   - blue and red are swapped (1 <-> 4)
 *   - yellow ad cyan are swapped (3 <-> 6)
 */
static const int cons_to_vga_colors[16] = {
	0, 4, 2, 6, 1, 5, 3, 7,
	0, 4, 2, 6, 1, 5, 3, 7
};

int
vt_generate_cons_palette(uint32_t *palette, int format, uint32_t rmax,
    int roffset, uint32_t gmax, int goffset, uint32_t bmax, int boffset)
{
	int i;

#define	CF(_f, _i) ((_f ## max * color_def[(_i)]._f / 100) << _f ## offset)
	for (i = 0; i < 16; i++) {
		switch (format) {
		case COLOR_FORMAT_VGA:
			palette[i] = cons_to_vga_colors[i];
			break;
		case COLOR_FORMAT_RGB:
			palette[i] = CF(r, i) | CF(g, i) | CF(b, i);
			break;
		default:
			return (ENODEV);
		}
	}
#undef	CF
	return (0);
}
