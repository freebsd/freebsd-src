/*-
 * Copyright (c) 1995-1998 Søren Schmidt
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *	$Id: star_saver.c,v 1.18 1999/01/11 03:18:53 yokota Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>

#include <machine/md_var.h>
#include <machine/pc/display.h>

#include <saver.h>

#define NUM_STARS	50

static u_short *window;
static int blanked;

/*
 * Alternate saver that got its inspiration from a well known utility
 * package for an inferior^H^H^H^H^H^Hfamous OS.
 */
static int
star_saver(video_adapter_t *adp, int blank)
{
	scr_stat	*scp = cur_console;
	int		cell, i;
	char 		pattern[] = {"...........++++***   "};
	char		colors[] = {FG_DARKGREY, FG_LIGHTGREY,
				    FG_WHITE, FG_LIGHTCYAN};
	static u_short 	stars[NUM_STARS][2];

	if (blank) {
		if (adp->va_mode_flags & V_INFO_GRAPHICS)
			return EAGAIN;
		if (!blanked) {
			window = (u_short *)adp->va_window;
			/* clear the screen and set the border color */
			fillw(((FG_LIGHTGREY|BG_BLACK) << 8) | scr_map[0x20],
			      window, scp->xsize * scp->ysize);
			set_border(scp, 0);
			blanked = TRUE;
			for(i=0; i<NUM_STARS; i++) {
				stars[i][0] =
					random() % (scp->xsize*scp->ysize);
				stars[i][1] = 0;
			}
		}
		cell = random() % NUM_STARS;
		*((u_short*)(window + stars[cell][0])) =
			scr_map[pattern[stars[cell][1]]] |
				colors[random()%sizeof(colors)] << 8;
		if ((stars[cell][1]+=(random()%4)) >= sizeof(pattern)-1) {
			stars[cell][0] = random() % (scp->xsize*scp->ysize);
			stars[cell][1] = 0;
		}
	}
	else {
		blanked = FALSE;
	}
	return 0;
}

static int
star_init(video_adapter_t *adp)
{
	blanked = FALSE;
	return 0;
}

static int
star_term(video_adapter_t *adp)
{
	return 0;
}

static scrn_saver_t star_module = {
	"star_saver", star_init, star_term, star_saver, NULL,
};

SAVER_MODULE(star_saver, star_module);
