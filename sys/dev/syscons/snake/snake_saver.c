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
 *	$Id: snake_saver.c,v 1.5 1995/09/04 03:02:08 peter Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/exec.h>
#include <sys/sysent.h>
#include <sys/lkm.h>
#include <sys/errno.h>
#include <saver.h>

MOD_MISC("snake_saver")

void (*current_saver)();
void (*old_saver)();

static void
snake_saver(int blank)
{
	const char	saves[] = {"FreeBSD-2.2-CURRENT"};
	static u_char	*savs[sizeof(saves)-1];
	static int	dirx, diry;
	int		f;
	scr_stat	*scp = cur_console;

	if (blank) {
		if (!scrn_blanked) {
			fillw((FG_LIGHTGREY|BG_BLACK)<<8 | scr_map[0x20],
			      Crtat, scp->xsize * scp->ysize);
			set_border(0);
			dirx = (scp->xpos ? 1 : -1);
			diry = (scp->ypos ?
				scp->xsize : -scp->xsize);
			for (f=0; f< sizeof(saves)-1; f++)
				savs[f] = (u_char *)Crtat + 2 *
					  (scp->xpos+scp->ypos*scp->xsize);
			*(savs[0]) = scr_map[*saves];
			f = scp->ysize * scp->xsize + 5;
			outb(crtc_addr, 14);
			outb(crtc_addr+1, f >> 8);
			outb(crtc_addr, 15);
			outb(crtc_addr+1, f & 0xff);
			scrn_blanked = 1;
		}
		if (scrn_blanked++ < 4)
			return;
		scrn_blanked = 1;
		*(savs[sizeof(saves)-2]) = scr_map[0x20];
		for (f=sizeof(saves)-2; f > 0; f--)
			savs[f] = savs[f-1];
		f = (savs[0] - (u_char *)Crtat) / 2;
		if ((f % scp->xsize) == 0 ||
		    (f % scp->xsize) == scp->xsize - 1 ||
		    (random() % 50) == 0)
			dirx = -dirx;
		if ((f / scp->xsize) == 0 ||
		    (f / scp->xsize) == scp->ysize - 1 ||
		    (random() % 20) == 0)
			diry = -diry;
		savs[0] += 2*dirx + 2*diry;
		for (f=sizeof(saves)-2; f>=0; f--)
			*(savs[f]) = scr_map[saves[f]];
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

snake_saver_load(struct lkm_table *lkmtp, int cmd)
{
	(*current_saver)(0);
	old_saver = current_saver;
	current_saver = snake_saver;
	uprintf("snake screen saver installed\n");
	return 0;
}

snake_saver_unload(struct lkm_table *lkmtp, int cmd)
{
	(*current_saver)(0);
	current_saver = old_saver;
	uprintf("snake screen saver removed\n");
	return 0;
}

snake_saver_mod(struct lkm_table *lkmtp, int cmd, int ver)
{
	DISPATCH(lkmtp, cmd, ver, snake_saver_load, snake_saver_unload, nosys);
}
