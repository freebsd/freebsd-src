/*-
 * Copyright 2021 Toomas Soome <tsoome@freebsd.org>
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * mips beri specific gfx stubs.
 */

#include <sys/types.h>
#include <pnglite.h>
#include "bootstrap.h"
#include "gfx_fb.h"

font_list_t fonts = STAILQ_HEAD_INITIALIZER(fonts);
teken_gfx_t gfx_state = { 0 };

void
gfx_fb_setpixel(uint32_t x __unused, uint32_t y __unused)
{
}

void
gfx_fb_drawrect(uint32_t x1 __unused, uint32_t y1 __unused,
    uint32_t x2 __unused, uint32_t y2 __unused, uint32_t fill __unused)
{
}

void
gfx_term_drawrect(uint32_t x1 __unused, uint32_t y1 __unused,
    uint32_t x2 __unused, uint32_t y2 __unused)
{
}

void
gfx_fb_line(uint32_t x0 __unused, uint32_t y0 __unused,
    uint32_t x1 __unused, uint32_t y1 __unused, uint32_t w __unused)
{
}

void
gfx_fb_bezier(uint32_t x0 __unused, uint32_t y0 __unused,
    uint32_t x1 __unused, uint32_t y1 __unused, uint32_t x2 __unused,
    uint32_t y2 __unused, uint32_t w __unused)
{
}

int
gfx_fb_putimage(png_t *png __unused, uint32_t ux1 __unused,
    uint32_t uy1 __unused, uint32_t ux2 __unused, uint32_t uy2 __unused,
    uint32_t flags __unused)
{
	return (1);
}
