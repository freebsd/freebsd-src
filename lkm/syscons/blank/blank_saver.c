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
 *	$Id: blank_saver.c,v 1.9 1997/04/06 10:48:10 dufault Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/exec.h>
#include <sys/sysent.h>
#include <sys/lkm.h>
#include <sys/errno.h>
#include <saver.h>

MOD_MISC(blank_saver);

void (*current_saver)(int blank);
void (*old_saver)(int blank);

static void
blank_saver(int blank)
{
	u_char val;
	if (blank) {
		scrn_blanked = 1;
		outb(TSIDX, 0x01); val = inb(TSREG);
		outb(TSIDX, 0x01); outb(TSREG, val | 0x20);
	}
	else {
		outb(TSIDX, 0x01); val = inb(TSREG);
		outb(TSIDX, 0x01); outb(TSREG, val & 0xDF);
		scrn_blanked = 0;
	}
}

static int
blank_saver_load(struct lkm_table *lkmtp, int cmd)
{
	(*current_saver)(0);
	old_saver = current_saver;
	current_saver = blank_saver;
	return 0;
}

static int
blank_saver_unload(struct lkm_table *lkmtp, int cmd)
{
	(*current_saver)(0);
	current_saver = old_saver;
	return 0;
}

int
blank_saver_mod(struct lkm_table *lkmtp, int cmd, int ver)
{
	MOD_DISPATCH(blank_saver, lkmtp, cmd, ver,
		blank_saver_load, blank_saver_unload, lkm_nullcmd);
}
