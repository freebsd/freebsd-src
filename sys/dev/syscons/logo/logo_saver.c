/*-
 * Copyright (c) 1998 Dag-Erling Coïdan Smørgrav
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
 *    derived from this software without specific prior written permission
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
 *	$Id: logo_saver.c,v 1.1 1998/12/28 14:22:57 des Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/syslog.h>

#include <machine/md_var.h>

#include <saver.h>

static u_char *vid;
static int banksize, scrmode, scrw, scrh;
static u_char save_pal[768];

#include "logo.c"

#define set_origin(scp, o) (*biosvidsw.set_win_org)((scp)->adp, o)

static void
logo_blit(int x, int y)
{
    int d, l, o, p;
    
    for (o = 0, p = y * scrw + x; p > banksize; p -= banksize)
	o += banksize;
    set_origin(cur_console, o);

    for (d = 0; d < sizeof logo_img; d += logo_w) {
	if (p + logo_w < banksize) {
	    bcopy(logo_img + d, vid + p, logo_w);
	    p += scrw;
	} else if (p < banksize) {
	    l = banksize - p;
	    bcopy(logo_img + d, vid + p, l);
	    set_origin(cur_console, (o += banksize));
	    bcopy(logo_img + d + l, vid, logo_w - l);
	    p += scrw - banksize;
	} else {
	    p -= banksize;
	    set_origin(cur_console, (o += banksize));
	    bcopy(logo_img + d, vid + p, logo_w);
	    p += scrw;
	}
    }
}

static void
logo_update(void)
{
    static int xpos = 0, ypos = 0;
    static int xinc = 1, yinc = 1;

    /* Turn when you hit the edge */
    if ((xpos + logo_w + xinc > scrw) || (xpos + xinc < 0))
	xinc = -xinc;
    if ((ypos + logo_h + yinc > scrh) || (ypos + yinc < 0))
	yinc = -yinc;
    xpos += xinc;
    ypos += yinc;
	
    /* XXX Relies on margin around logo to erase trail */
    logo_blit(xpos, ypos);
}

static void
logo_saver(int blank)
{
    scr_stat *scp = cur_console;
    static int saved_mode;
    int i, pl;

    if (blank) {
	/* switch to graphics mode */
	if (scrn_blanked <= 0) {
	    pl = splhigh();
	    saved_mode = scp->mode;
	    scp->mode = scrmode;
	    scp->status |= SAVER_RUNNING|GRAPHICS_MODE;
	    save_palette(scp, (char *)save_pal);
	    set_mode(scp);
	    load_palette(scp, (char *)logo_pal);
	    scrn_blanked++;
	    vid = (u_char *)Crtat;
	    splx(pl);
	    for (i = 0; i < scrw*scrh; i += banksize) {
		set_origin(scp, i);
		bzero(vid, banksize);
	    }
	}
	logo_update();
    } else {
	/* return to previous video mode */
	if (scrn_blanked > 0) {
	    if (saved_mode) {
		pl = splhigh();
		scrn_blanked = 0;
		scp->mode = saved_mode;
		scp->status &= ~(SAVER_RUNNING|GRAPHICS_MODE);
		set_mode(scp);
		load_palette(scp, (char *)save_pal);
		saved_mode = 0;
		splx(pl);
	    }
	}
    }
}

static int
logo_saver_load(void)
{
    video_info_t info;
    int adp;
    
    adp = cur_console->adp;
    if (!(*biosvidsw.get_info)(adp, M_VESA_CG800x600, &info)) {
	scrmode = M_VESA_CG800x600;
    } else if (!(*biosvidsw.get_info)(adp, M_VGA_CG320, &info)) {
	scrmode = M_VGA_CG320;
    } else {
        log(LOG_NOTICE, "logo_saver: no suitable graphics mode\n");
	return ENODEV;
    }
    
    banksize = info.vi_window_size;
    scrw = info.vi_width;
    scrh = info.vi_height;
    
    return add_scrn_saver(logo_saver);
}

static int
logo_saver_unload(void)
{
    return remove_scrn_saver(logo_saver);
}

SAVER_MODULE(logo_saver);
