/*-
 * Copyright (c) 1995 Søren Schmidt
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software withough specific prior written permission
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
 *	$Id: star_saver.c,v 1.3 1995/05/30 06:06:20 rgrimes Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/exec.h>
#include <sys/sysent.h>
#include <sys/lkm.h>
#include <sys/errno.h>
#include <saver.h>

MOD_MISC("star_saver")

void (*current_saver)();
void (*old_saver)();

#define NUM_STARS	50

/*
 * Alternate saver that got its inspiration from a well known utility
 * package for an inferior^H^H^H^H^H^Hfamous OS.
 */
void
star_saver(int blank)
{
	scr_stat	*scp = cur_console;
	int		cell, i;
	char 		pattern[] = {"...........++++***   "};
	char		colors[] = {FG_DARKGREY, FG_LIGHTGREY,
				    FG_WHITE, FG_LIGHTCYAN};
	static u_short 	stars[NUM_STARS][2];

	if (blank) {
		if (!scrn_blanked) {
			scrn_blanked = 1;
			fillw((FG_LIGHTGREY|BG_BLACK)<<8|scr_map[0x20], Crtat,
			      scp->xsize * scp->ysize);
			set_border(0);
			for(i=0; i<NUM_STARS; i++) {
				stars[i][0] =
					random() % (scp->xsize*scp->ysize);
				stars[i][1] = 0;
			}
		}
		cell = random() % NUM_STARS;
		*((u_short*)(Crtat + stars[cell][0])) =
			scr_map[pattern[stars[cell][1]]] |
				colors[random()%sizeof(colors)] << 8;
		if ((stars[cell][1]+=(random()%4)) >= sizeof(pattern)-1) {
			stars[cell][0] = random() % (scp->xsize*scp->ysize);
			stars[cell][1] = 0;
		}
	}
	else {
		if (scrn_blanked) {
			set_border(scp->border);
			scrn_blanked = 0;
			scp->start = 0;
			scp->end = scp->xsize * scp->ysize;
		}
	}
}

star_saver_load(struct lkm_table *lkmtp, int cmd)
{
	(*current_saver)(0);
	old_saver = current_saver;
	current_saver = star_saver;
	uprintf("star screen saver installed\n");
	return 0;
}

star_saver_unload(struct lkm_table *lkmtp, int cmd)
{
	(*current_saver)(0);
	current_saver = old_saver;
	uprintf("star screen saver removed\n");
	return 0;
}

star_saver_mod(struct lkm_table *lkmtp, int cmd, int ver)
{
	DISPATCH(lkmtp, cmd, ver, star_saver_load, star_saver_unload, nosys);
}
