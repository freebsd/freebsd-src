/*-
 * Copyright (c) 2024 Netflix, Inc
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/* Copied from a file that likely shoulve have had this at the top */
/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright 2020 Toomas Soome
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
 */

/*******************************************************************
** g f x _ l o a d e r . c
** Additional FICL words designed for FreeBSD's loader
** for graphics
*******************************************************************/

#include <stand.h>
#include "bootstrap.h"
#include <gfx_fb.h>
#include <pnglite.h>
#include "ficl.h"

/*		FreeBSD's loader interaction words and extras
 *		for graphics
 *		fb-bezier	( x0 y0 x1 y1 x2 y2 wd -- )
 *		fb-drawrect	( x1 y1 x2 y2 fill -- )
 *		fb-line		( x0 y0 x1 y1 wd -- )
 * 		fb-putimage	( flags x1 y1 x2 y2 -- flag )
 *		fb-setpixel	( x y -- )
 *		term-drawrect	( x1 y1 x2 y2 fill -- )
 *		term-putimage	( flags x1 y1 x2 y2 -- flag )
 */

/* ( flags x1 y1 x2 y2 -- flag ) */
void
ficl_term_putimage(FICL_VM *pVM)
{
        char *namep, *name;
        int names;
        unsigned long ret = FICL_FALSE;
        uint32_t x1, y1, x2, y2, f;
        png_t png;
	int error;

#if FICL_ROBUST > 1
	vmCheckStack(pVM, 7, 1);
#endif
        names = stackPopINT(pVM->pStack);
        namep = (char *) stackPopPtr(pVM->pStack);
        y2 = stackPopINT(pVM->pStack);
        x2 = stackPopINT(pVM->pStack);
        y1 = stackPopINT(pVM->pStack);
        x1 = stackPopINT(pVM->pStack);
        f = stackPopINT(pVM->pStack);

	x1 = gfx_state.tg_origin.tp_col + x1 * gfx_state.tg_font.vf_width;
	y1 = gfx_state.tg_origin.tp_row + y1 * gfx_state.tg_font.vf_height;
	if (x2 != 0) {
		x2 = gfx_state.tg_origin.tp_col +
		    x2 * gfx_state.tg_font.vf_width;
	}
	if (y2 != 0) {
		y2 = gfx_state.tg_origin.tp_row +
		    y2 * gfx_state.tg_font.vf_height;
	}

        name = ficlMalloc(names + 1);
        if (!name)
		vmThrowErr(pVM, "Error: out of memory");
        (void) strncpy(name, namep, names);
        name[names] = '\0';

        if ((error = png_open(&png, name)) != PNG_NO_ERROR) {
		if (f & FL_PUTIMAGE_DEBUG)
			printf("%s\n", png_error_string(error));
	} else {
                if (gfx_fb_putimage(&png, x1, y1, x2, y2, f) == 0)
                        ret = FICL_TRUE;        /* success */
                (void) png_close(&png);
	}
        ficlFree(name);
	stackPushUNS(pVM->pStack, ret);
}

/* ( flags x1 y1 x2 y2 -- flag ) */
void
ficl_fb_putimage(FICL_VM *pVM)
{
        char *namep, *name;
        int names;
        unsigned long ret = FICL_FALSE;
        uint32_t x1, y1, x2, y2, f;
        png_t png;
	int error;

#if FICL_ROBUST > 1
	vmCheckStack(pVM, 7, 1);
#endif
        names = stackPopINT(pVM->pStack);
        namep = (char *) stackPopPtr(pVM->pStack);
        y2 = stackPopINT(pVM->pStack);
        x2 = stackPopINT(pVM->pStack);
        y1 = stackPopINT(pVM->pStack);
        x1 = stackPopINT(pVM->pStack);
        f = stackPopINT(pVM->pStack);

        name = ficlMalloc(names + 1);
        if (!name)
		vmThrowErr(pVM, "Error: out of memory");
        (void) strncpy(name, namep, names);
        name[names] = '\0';

        if ((error = png_open(&png, name)) != PNG_NO_ERROR) {
		if (f & FL_PUTIMAGE_DEBUG)
			printf("%s\n", png_error_string(error));
	} else {
                if (gfx_fb_putimage(&png, x1, y1, x2, y2, f) == 0)
                        ret = FICL_TRUE;        /* success */
                (void) png_close(&png);
	}
        ficlFree(name);
	stackPushUNS(pVM->pStack, ret);
}

void
ficl_fb_setpixel(FICL_VM *pVM)
{
        FICL_UNS x, y;

#if FICL_ROBUST > 1
	vmCheckStack(pVM, 2, 0);
#endif

        y = stackPopUNS(pVM->pStack);
        x = stackPopUNS(pVM->pStack);
        gfx_fb_setpixel(x, y);
}

void
ficl_fb_line(FICL_VM *pVM)
{
	FICL_UNS x0, y0, x1, y1, wd;

#if FICL_ROBUST > 1
	vmCheckStack(pVM, 5, 0);
#endif

	wd = stackPopUNS(pVM->pStack);
	y1 = stackPopUNS(pVM->pStack);
	x1 = stackPopUNS(pVM->pStack);
	y0 = stackPopUNS(pVM->pStack);
	x0 = stackPopUNS(pVM->pStack);
	gfx_fb_line(x0, y0, x1, y1, wd);
}

void
ficl_fb_bezier(FICL_VM *pVM)
{
	FICL_UNS x0, y0, x1, y1, x2, y2, width;

#if FICL_ROBUST > 1
	vmCheckStack(pVM, 7, 0);
#endif

	width = stackPopUNS(pVM->pStack);
	y2 = stackPopUNS(pVM->pStack);
	x2 = stackPopUNS(pVM->pStack);
	y1 = stackPopUNS(pVM->pStack);
	x1 = stackPopUNS(pVM->pStack);
	y0 = stackPopUNS(pVM->pStack);
	x0 = stackPopUNS(pVM->pStack);
	gfx_fb_bezier(x0, y0, x1, y1, x2, y2, width);
}

void
ficl_fb_drawrect(FICL_VM *pVM)
{
	FICL_UNS x1, x2, y1, y2, fill;

#if FICL_ROBUST > 1
	vmCheckStack(pVM, 5, 0);
#endif

	fill = stackPopUNS(pVM->pStack);
	y2 = stackPopUNS(pVM->pStack);
	x2 = stackPopUNS(pVM->pStack);
	y1 = stackPopUNS(pVM->pStack);
	x1 = stackPopUNS(pVM->pStack);
	gfx_fb_drawrect(x1, y1, x2, y2, fill);
}

void
ficl_term_drawrect(FICL_VM *pVM)
{
	FICL_UNS x1, x2, y1, y2;

#if FICL_ROBUST > 1
	vmCheckStack(pVM, 4, 0);
#endif

	y2 = stackPopUNS(pVM->pStack);
	x2 = stackPopUNS(pVM->pStack);
	y1 = stackPopUNS(pVM->pStack);
	x1 = stackPopUNS(pVM->pStack);
	gfx_term_drawrect(x1, y1, x2, y2);
}

/**************************************************************************
                        f i c l C o m p i l e G f x
** Build FreeBSD platform extensions into the system dictionary
** for gfx
**************************************************************************/
static void ficlCompileGfx(FICL_SYSTEM *pSys)
{
    ficlCompileFcn **fnpp;
    FICL_DICT *dp = pSys->dp;
    assert (dp);

    dictAppendWord(dp, "fb-setpixel", ficl_fb_setpixel, FW_DEFAULT);
    dictAppendWord(dp, "fb-line", ficl_fb_line, FW_DEFAULT);
    dictAppendWord(dp, "fb-bezier", ficl_fb_bezier, FW_DEFAULT);
    dictAppendWord(dp, "fb-drawrect", ficl_fb_drawrect, FW_DEFAULT);
    dictAppendWord(dp, "fb-putimage", ficl_fb_putimage, FW_DEFAULT);
    dictAppendWord(dp, "term-drawrect", ficl_term_drawrect, FW_DEFAULT);
    dictAppendWord(dp, "term-putimage", ficl_term_putimage, FW_DEFAULT);

    return;
}
FICL_COMPILE_SET(ficlCompileGfx);

void
gfx_interp_ref(void)
{
}
