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
 *	$Id: fade_saver.c,v 1.3 1995/10/28 12:35:10 peter Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/exec.h>
#include <sys/sysent.h>
#include <sys/lkm.h>
#include <sys/errno.h>
#include <saver.h>

MOD_MISC("fade_saver")

void (*current_saver)();
void (*old_saver)();

static void
fade_saver(int blank)
{
	static int count = 0;
	int i;

	if (blank) {
		scrn_blanked = 1;
		if (count < 64) {
			outb(PIXMASK, 0xFF);		/* no pixelmask */
			outb(PALWADR, 0x00);
			outb(PALDATA, 0);
			outb(PALDATA, 0);
			outb(PALDATA, 0);
			for (i = 3; i < 768; i++) {
				if (palette[i] - count > 15)
					outb(PALDATA, palette[i]-count);
				else
					outb(PALDATA, 15);
			}
			inb(crtc_addr+6);		/* reset flip/flop */
			outb(ATC, 0x20);		/* enable palette */
			count++;
		}
	}
	else {
		load_palette();
		count = scrn_blanked = 0;
	}
}

fade_saver_load(struct lkm_table *lkmtp, int cmd)
{
	(*current_saver)(0);
	old_saver = current_saver;
	current_saver = fade_saver;
	uprintf("fade screen saver installed\n");
	return 0;
}

fade_saver_unload(struct lkm_table *lkmtp, int cmd)
{
	(*current_saver)(0);
	current_saver = old_saver;
	uprintf("fade screen saver removed\n");
	return 0;
}

fade_saver_mod(struct lkm_table *lkmtp, int cmd, int ver)
{
	DISPATCH(lkmtp, cmd, ver, fade_saver_load, fade_saver_unload,
		 lkm_nullcmd);
}
