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
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/syslog.h>
#include <sys/consio.h>
#include <sys/fbio.h>
#include <sys/random.h>

#include <dev/fb/fbreg.h>
#include <dev/fb/splashreg.h>
#include <dev/syscons/syscons.h>

static u_char *vid;

#define SCRW 320
#define SCRH 200
#define MAX 63

static u_char rain_pal[768];
static int blanked;

static void
rain_update(video_adapter_t *adp)
{
    int i, t;

    t = rain_pal[(MAX*3+2)];
    for (i = (MAX*3+2); i > 5; i -= 3)
	rain_pal[i] = rain_pal[i-3];
    rain_pal[5] = t;
    load_palette(adp, rain_pal);
}

static int
rain_saver(video_adapter_t *adp, int blank)
{
    int i, j, k, pl;

    if (blank) {
	/* switch to graphics mode */
	if (blanked <= 0) {
	    pl = splhigh();
	    set_video_mode(adp, M_VGA_CG320);
	    load_palette(adp, rain_pal);
#if 0 /* XXX conflict */
	    set_border(adp, 0);
#endif
	    blanked++;
	    vid = (u_char *)adp->va_window;
	    splx(pl);
	    bzero(vid, SCRW*SCRH);
	    for (i = 0; i < SCRW; i += 2)
		vid[i] = 1 + (random() % MAX);
	    for (j = 1, k = SCRW; j < SCRH; j++)
		for (i = 0; i < SCRW; i += 2, k += 2)
		    vid[k] = (vid[k-SCRW] < MAX) ? 1 + vid[k-SCRW] : 1;
	}

	/* update display */
	rain_update(adp);
	
    } else {
	blanked = 0;
    }
    return 0;
}

static int
rain_init(video_adapter_t *adp)
{
    video_info_t info;
    int i;

    /* check that the console is capable of running in 320x200x256 */
    if (get_mode_info(adp, M_VGA_CG320, &info)) {
        log(LOG_NOTICE, "rain_saver: the console does not support M_VGA_CG320\n");
	return ENODEV;
    }

    /* intialize the palette */
    for (i = 3; i < (MAX+1)*3; i += 3)
	rain_pal[i+2] = rain_pal[i-1] + 4;

    blanked = 0;

    return 0;
}

static int
rain_term(video_adapter_t *adp)
{
    return 0;
}

static scrn_saver_t rain_module = {
    "rain_saver", rain_init, rain_term, rain_saver, NULL,
};

SAVER_MODULE(rain_saver, rain_module);
