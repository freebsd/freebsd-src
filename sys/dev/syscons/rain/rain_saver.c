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
 *	$Id$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/syslog.h>

#include <machine/md_var.h>
#include <machine/random.h>

#include <saver.h>

static u_char *vid;

#define SCRW 320
#define SCRH 200
#define MAX 63

static u_char save_pal[768];
static u_char rain_pal[768];

static void
rain_update(void)
{
    int i, t;

    t = rain_pal[(MAX*3+2)];
    for (i = (MAX*3+2); i > 5; i -= 3)
	rain_pal[i] = rain_pal[i-3];
    rain_pal[5] = t;
    load_palette(cur_console, rain_pal);
}

static void
rain_saver(int blank)
{
    scr_stat *scp = cur_console;
    static int saved_mode;
    int i, j, k, pl;

    if (blank) {
	/* switch to graphics mode */
	if (scrn_blanked <= 0) {
	    pl = splhigh();
	    saved_mode = scp->mode;
	    scp->mode = M_VGA_CG320;
	    scp->status |= SAVER_RUNNING|GRAPHICS_MODE;
	    save_palette(scp, save_pal);
	    set_mode(scp);
	    load_palette(scp, rain_pal);
	    scrn_blanked++;
	    vid = (u_char *)Crtat;
	    splx(pl);
	    bzero(vid, SCRW*SCRH);
	    for (i = 0; i < SCRW; i += 2)
		vid[i] = 1 + (random() % MAX);
	    for (j = 1, k = SCRW; j < SCRH; j++)
		for (i = 0; i < SCRW; i += 2, k += 2)
		    vid[k] = (vid[k-SCRW] < MAX) ? 1 + vid[k-SCRW] : 1;
	}

	/* update display */
	rain_update();
	
    } else {
	/* return to previous video mode */
	if (scrn_blanked > 0) {
	    if (saved_mode) {
		pl = splhigh();
		scrn_blanked = 0;
		scp->mode = saved_mode;
		scp->status &= ~(SAVER_RUNNING|GRAPHICS_MODE);
		set_mode(scp);
		load_palette(scp, save_pal);
		saved_mode = 0;
		splx(pl);
	    }
	}
    }
}

static int
rain_saver_load(void)
{
    video_info_t info;
    int i;

    /* check that the console is capable of running in 320x200x256 */
    if ((*biosvidsw.get_info)(cur_console->adp, M_VGA_CG320, &info)) {
        log(LOG_NOTICE, "rain_saver: the console does not support M_VGA_CG320\n");
	return ENODEV;
    }

    /* intialize the palette */
    for (i = 3; i < (MAX+1)*3; i += 3)
	rain_pal[i+2] = rain_pal[i-1] + 4;
	    
    return add_scrn_saver(rain_saver);
}

static int
rain_saver_unload(void)
{
    return remove_scrn_saver(rain_saver);
}

SAVER_MODULE(rain_saver);
