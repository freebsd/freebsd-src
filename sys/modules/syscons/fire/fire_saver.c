/*-
 * Copyright (c) 1999 Brad Forschinger
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

/*
 * brad forschinger, 19990504 <retch@flag.blackened.net>
 * 
 * written with much help from warp_saver.c
 * 
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

#define X_SIZE 320
#define Y_SIZE 200

static int      blanked;
static u_char   fire_pal[768];
static u_char   buf[X_SIZE * (Y_SIZE + 1)];
static u_char  *vid;

static int
fire_saver(video_adapter_t *adp, int blank)
{
    int             x, y;

    if (blank) {
	if (blanked <= 0) {
	    int             red, green, blue;
	    int             palette_index;

	    set_video_mode(adp, M_VGA_CG320);

	    /* build and load palette */
	    red = green = blue = 0;
	    for (palette_index = 0; palette_index < 256; palette_index++) {
		red++;
		if (red > 128)
		    green += 2;

		fire_pal[(palette_index * 3) + 0] = red;
		fire_pal[(palette_index * 3) + 1] = green;
		fire_pal[(palette_index * 3) + 2] = blue;
	    }
	    load_palette(adp, fire_pal);

	    blanked++;
	    vid = (u_char *) adp->va_window;
	}
	/* make a new bottom line */
	for (x = 0, y = Y_SIZE; x < X_SIZE; x++)
	    buf[x + (y * X_SIZE)] = random() % 160 + 96;

	/* fade the flames out */
	for (y = 0; y < Y_SIZE; y++) {
	    for (x = 0; x < X_SIZE; x++) {
		buf[x + (y * X_SIZE)] = (buf[(x + 0) + ((y + 0) * X_SIZE)] +
					 buf[(x - 1) + ((y + 1) * X_SIZE)] +
					 buf[(x + 0) + ((y + 1) * X_SIZE)] +
				     buf[(x + 1) + ((y + 1) * X_SIZE)]) / 4;
		if (buf[x + (y * X_SIZE)] > 0)
		    buf[x + (y * X_SIZE)]--;
	    }
	}

	/* blit our buffer into video ram */
	memcpy(vid, buf, X_SIZE * Y_SIZE);
    } else {
	blanked = 0;
    }

    return 0;
}

static int
fire_initialise(video_adapter_t *adp)
{
    video_info_t    info;

    /* check that the console is capable of running in 320x200x256 */
    if (get_mode_info(adp, M_VGA_CG320, &info)) {
	log(LOG_NOTICE, "fire_saver: the console does not support M_VGA_CG320\n");
	return (ENODEV);
    }
    blanked = 0;

    return 0;
}

static int
fire_terminate(video_adapter_t *adp)
{
    return 0;
}

static scrn_saver_t fire_module = {
    "fire_saver", fire_initialise, fire_terminate, fire_saver, NULL
};

SAVER_MODULE(fire_saver, fire_module);
