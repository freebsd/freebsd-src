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

#include <sys/param.h>

#include <dev/vt/colors/vt_termcolors.h>

static struct {
	unsigned char r;	/* Red percentage value. */
	unsigned char g;	/* Green percentage value. */
	unsigned char b;	/* Blue percentage value. */
} color_def[16] = {
	{0,	0,	0},	/* black */
	{0,	0,	50},	/* dark blue */
	{0,	50,	0},	/* dark green */
	{0,	50,	50},	/* dark cyan */
	{50,	0,	0},	/* dark red */
	{50,	0,	50},	/* dark magenta */
	{50,	50,	0},	/* brown */
	{75,	75,	75},	/* light gray */
	{50,	50,	50},	/* dark gray */
	{0,	0,	100},	/* light blue */
	{0,	100,	0},	/* light green */
	{0,	100,	100},	/* light cyan */
	{100,	0,	0},	/* light red */
	{100,	0,	100},	/* light magenta */
	{100,	100,	0},	/* yellow */
	{100,	100,	100},	/* white */
};

int
vt_generate_vga_palette(uint32_t *palette, int format, uint32_t rmax, int roffset,
    uint32_t gmax, int goffset, uint32_t bmax, int boffset)
{
	int i;

#define	CF(_f, _i) ((_f ## max * color_def[(_i)]._f / 100) << _f ## offset)
	for (i = 0; i < 16; i++) {
		switch (format) {
		case COLOR_FORMAT_VGA:
			palette[i] = i;
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
