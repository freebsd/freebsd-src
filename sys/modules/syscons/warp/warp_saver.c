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
 *	$Id: warp_saver.c,v 1.2 1998/12/28 14:20:13 des Exp $
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
#define SPP 15
#define STARS (SPP*(1+2+4+8))

static int star[STARS];
static u_char save_pal[768];
static u_char warp_pal[768] = {
    0x00, 0x00, 0x00,
    0x66, 0x66, 0x66,
    0x99, 0x99, 0x99,
    0xcc, 0xcc, 0xcc,
    0xff, 0xff, 0xff
    /* the rest is zero-filled by the compiler */
};

static void
warp_update(void)
{
    int i, j, k, n;

    for (i = 1, k = 0, n = SPP*8; i < 5; i++, n /= 2)
	for (j = 0; j < n; j++, k++) {
	    vid[star[k]] = 0;
	    star[k] += i;
	    if (star[k] > SCRW*SCRH)
		star[k] -= SCRW*SCRH;
	    vid[star[k]] = i;
	}
}

static void
warp_saver(int blank)
{
    scr_stat *scp = cur_console;
    static int saved_mode;
    int pl;

    if (blank) {
	/* switch to graphics mode */
	if (scrn_blanked <= 0) {
	    pl = splhigh();
	    saved_mode = scp->mode;
	    scp->mode = M_VGA_CG320;
	    scp->status |= SAVER_RUNNING|GRAPHICS_MODE;
	    save_palette(scp, save_pal);
	    set_mode(scp);
	    load_palette(scp, warp_pal);
	    scrn_blanked++;
	    vid = (u_char *)Crtat;
	    splx(pl);
	    bzero(vid, SCRW*SCRH);
	}

	/* update display */
	warp_update();
	
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
warp_saver_load(void)
{
    video_info_t info;
    int i;

    /* check that the console is capable of running in 320x200x256 */
    if ((*biosvidsw.get_info)(cur_console->adp, M_VGA_CG320, &info)) {
        log(LOG_NOTICE, "warp_saver: the console does not support M_VGA_CG320\n");
	return ENODEV;
    }

    /* randomize the star field */
    for (i = 0; i < STARS; i++) {
	star[i] = random() % (SCRW*SCRH);
    }
    
    return add_scrn_saver(warp_saver);
}

static int
warp_saver_unload(void)
{
    return remove_scrn_saver(warp_saver);
}

SAVER_MODULE(warp_saver);
