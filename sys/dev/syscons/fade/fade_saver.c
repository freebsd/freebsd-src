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
 *	$Id: fade_saver.c,v 1.15 1998/11/04 03:49:38 peter Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>

#include <i386/isa/isa.h>

#include <saver.h>

static u_char palette[256*3];
static int blanked;

static int
fade_saver(video_adapter_t *adp, int blank)
{
	static int count = 0;
	u_char pal[256*3];
	int i;

	if (blank) {
		blanked = TRUE;
		switch (adp->va_type) {
		case KD_VGA:
			if (count <= 0)
				save_palette(adp, palette);
			if (count < 64) {
				pal[0] = pal[1] = pal[2] = 0;
				for (i = 3; i < 256*3; i++) {
					if (palette[i] - count > 60)
						pal[i] = palette[i] - count;
					else
						pal[i] = 60;
				}
				load_palette(adp, pal);
				count++;
			}
			break;
		case KD_EGA:
			/* not yet done XXX */
			break;
		case KD_CGA:
			outb(adp->va_crtc_addr + 4, 0x25);
			break;
		case KD_MONO:
		case KD_HERCULES:
			outb(adp->va_crtc_addr + 4, 0x21);
			break;
		default:
			break;
		}
	}
	else {
		switch (adp->va_type) {
		case KD_VGA:
			load_palette(adp, palette);
			count = 0;
			break;
		case KD_EGA:
			/* not yet done XXX */
			break;
		case KD_CGA:
			outb(adp->va_crtc_addr + 4, 0x2d);
			break;
		case KD_MONO:
		case KD_HERCULES:
			outb(adp->va_crtc_addr + 4, 0x29);
			break;
		default:
			break;
		}
		blanked = FALSE;
	}
	return 0;
}

static int
fade_init(video_adapter_t *adp)
{
	switch (adp->va_type) {
	case KD_MONO:
	case KD_HERCULES:
	case KD_CGA:
		/* 
		 * `fade' saver is not fully implemented for MDA and CGA.
		 * It simply blanks the display instead.
		 */
	case KD_VGA:
		break;
	case KD_EGA:
		/* EGA is yet to be supported */
	default:
		return ENODEV;
	}
	blanked = FALSE;
	return 0;
}

static int
fade_term(video_adapter_t *adp)
{
	return 0;
}

static scrn_saver_t fade_module = {
	"fade_saver", fade_init, fade_term, fade_saver, NULL,
};

SAVER_MODULE(fade_saver, fade_module);
