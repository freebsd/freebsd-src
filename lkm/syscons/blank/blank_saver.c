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
 *	$Id: blank_saver.c,v 1.6.2.2 1997/09/02 10:37:30 yokota Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/exec.h>
#include <sys/sysent.h>
#include <sys/lkm.h>

#include <i386/isa/isa.h>

#include <saver.h>

MOD_MISC(blank_saver);

static void
blank_saver(int blank)
{
	u_char val;
	if (blank) {
		scrn_blanked = 1;
		switch (crtc_type) {
		case KD_VGA:
			outb(TSIDX, 0x01); val = inb(TSREG);
			outb(TSIDX, 0x01); outb(TSREG, val | 0x20);
			break;
		case KD_EGA:
			/* not yet done XXX */
			break;
		case KD_CGA:
			outb(crtc_addr + 4, 0x25);
			break;
		case KD_MONO:
		case KD_HERCULES:
			outb(crtc_addr + 4, 0x21);
			break;
		default:
			break;
		}
	}
	else {
		switch (crtc_type) {
		case KD_VGA:
			outb(TSIDX, 0x01); val = inb(TSREG);
			outb(TSIDX, 0x01); outb(TSREG, val & 0xDF);
			break;
		case KD_EGA:
			/* not yet done XXX */
			break;
		case KD_CGA:
			outb(crtc_addr + 4, 0x2d);
			break;
		case KD_MONO:
		case KD_HERCULES:
			outb(crtc_addr + 4, 0x29);
			break;
		default:
			break;
		}
		scrn_blanked = 0;
	}
}

static int
blank_saver_load(struct lkm_table *lkmtp, int cmd)
{
	switch (crtc_type) {
	case KD_MONO:
	case KD_HERCULES:
	case KD_CGA:
	case KD_VGA:
		break;
	case KD_EGA:
		/* EGA is yet to be supported */
	default:
		return ENODEV;
	}
	return add_scrn_saver(blank_saver);
}

static int
blank_saver_unload(struct lkm_table *lkmtp, int cmd)
{
	return remove_scrn_saver(blank_saver);
}

int
blank_saver_mod(struct lkm_table *lkmtp, int cmd, int ver)
{
	MOD_DISPATCH(blank_saver, lkmtp, cmd, ver,
		blank_saver_load, blank_saver_unload, lkm_nullcmd);
}
