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
 *	$Id: snake_saver.c,v 1.15 1997/07/15 14:49:35 yokota Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/exec.h>
#include <sys/sysent.h>
#include <sys/lkm.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>

#include <machine/md_var.h>
#include <machine/pc/display.h>

#include <saver.h>

MOD_MISC(snake_saver);

static char	*message;
static u_char	**messagep;
static int	messagelen;

static void
snake_saver(int blank)
{
	static int	dirx, diry;
	int		f;
	scr_stat	*scp = cur_console;

/* XXX hack for minimal changes. */
#define	save	message
#define	savs	messagep

	if (blank) {
		if (scrn_blanked <= 0) {
			fillw((FG_LIGHTGREY|BG_BLACK)<<8 | scr_map[0x20],
			      Crtat, scp->xsize * scp->ysize);
			set_border(0);
			dirx = (scp->xpos ? 1 : -1);
			diry = (scp->ypos ?
				scp->xsize : -scp->xsize);
			for (f=0; f< messagelen; f++)
				savs[f] = (u_char *)Crtat + 2 *
					  (scp->xpos+scp->ypos*scp->xsize);
			*(savs[0]) = scr_map[*save];
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
		*(savs[messagelen-1]) = scr_map[0x20];
		for (f=messagelen-1; f > 0; f--)
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
		for (f=messagelen-1; f>=0; f--)
			*(savs[f]) = scr_map[save[f]];
	}
	else {
		if (scrn_blanked > 0) {
			set_border(scp->border);
			scrn_blanked = 0;
		}
	}
}

static int
snake_saver_load(struct lkm_table *lkmtp, int cmd)
{
	int err;

	messagelen = strlen(ostype) + 1 + strlen(osrelease);
	message = malloc(messagelen + 1, M_DEVBUF, M_WAITOK);
	sprintf(message, "%s %s", ostype, osrelease);
	messagep = malloc(messagelen * sizeof *messagep, M_DEVBUF, M_WAITOK);

	err = add_scrn_saver(snake_saver);
	if (err != 0) {
		free(message, M_DEVBUF);
		free(messagep, M_DEVBUF);
	}
	return err;
}

static int
snake_saver_unload(struct lkm_table *lkmtp, int cmd)
{
	int err;

	err = remove_scrn_saver(snake_saver);
	if (err == 0) {
		free(message, M_DEVBUF);
		free(messagep, M_DEVBUF);
	}
	return err;
}

int
snake_saver_mod(struct lkm_table *lkmtp, int cmd, int ver)
{
	MOD_DISPATCH(snake_saver, lkmtp, cmd, ver,
		snake_saver_load, snake_saver_unload, lkm_nullcmd);
}
