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
static int blanked;

#define SCRW 320
#define SCRH 200
#define SPP 15
#define STARS (SPP*(1+2+4+8))

static int star[STARS];
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

static int
warp_saver(video_adapter_t *adp, int blank)
{
    int pl;

    if (blank) {
	/* switch to graphics mode */
	if (blanked <= 0) {
	    pl = splhigh();
	    set_video_mode(adp, M_VGA_CG320);
	    load_palette(adp, warp_pal);
#if 0 /* XXX conflict */
	    set_border(adp, 0);
#endif
	    blanked++;
	    vid = (u_char *)adp->va_window;
	    splx(pl);
	    bzero(vid, SCRW*SCRH);
	}

	/* update display */
	warp_update();
	
    } else {
	blanked = 0;
    }
    return 0;
}

static int
warp_init(video_adapter_t *adp)
{
    video_info_t info;
    int i;

    /* check that the console is capable of running in 320x200x256 */
    if (get_mode_info(adp, M_VGA_CG320, &info)) {
        log(LOG_NOTICE, "warp_saver: the console does not support M_VGA_CG320\n");
	return ENODEV;
    }

    /* randomize the star field */
    for (i = 0; i < STARS; i++) {
	star[i] = random() % (SCRW*SCRH);
    }
    
    blanked = 0;

    return 0;
}

static int
warp_term(video_adapter_t *adp)
{
    return 0;
}

static scrn_saver_t warp_module = {
    "warp_saver", warp_init, warp_term, warp_saver, NULL,
};

SAVER_MODULE(warp_saver, warp_module);
