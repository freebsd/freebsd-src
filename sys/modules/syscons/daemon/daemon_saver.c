/*-
 * Copyright (c) 1997 Sandro Sigala, Brescia, Italy.
 * Copyright (c) 1997 Chris Shenton
 * Copyright (c) 1995 S ren Schmidt
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
 *	$Id: daemon_saver.c,v 1.2 1997/05/24 01:44:39 yokota Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/exec.h>
#include <sys/sysent.h>
#include <sys/lkm.h>
#include <sys/errno.h>

#include <machine/md_var.h>

#include "saver.h"

MOD_MISC(daemon_saver);

void (*current_saver)(int blank);
void (*old_saver)(int blank);

#define CONSOLE_VECT(x, y) \
	*((u_short*)(Crtat + (y)*cur_console->xsize + (x)))

#define DAEMON_MAX_WIDTH	32
#define DAEMON_MAX_HEIGHT	19

/*
 * Define this to disable the bouncing text.
 */
#undef DAEMON_ONLY

/* Who is the author of this ASCII pic? */

static char *daemon_pic[] = {
        "             ,        ,",
	"            /(        )`",
	"            \\ \\___   / |",
	"            /- _  `-/  '",
	"           (/\\/ \\ \\   /\\",
	"           / /   | `    \\",
	"           O O   ) /    |",
	"           `-^--'`<     '",
	"          (_.)  _  )   /",
	"           `.___/`    /",
	"             `-----' /",
	"<----.     __ / __   \\",
	"<----|====O)))==) \\) /====",
	"<----'    `--' `.__,' \\",
	"             |        |",
	"              \\       /       /\\",
	"         ______( (_  / \\______/",
	"       ,'  ,-----'   |",
	"       `--{__________)",
	NULL
};

static char *daemon_attr[] = {
        "             R        R",
	"            RR        RR",
	"            R RRRR   R R",
	"            RR W  RRR  R",
	"           RWWW W R   RR",
	"           W W   W R    R",
	"           B B   W R    R",
	"           WWWWWWRR     R",
	"          RRRR  R  R   R",
	"           RRRRRRR    R",
	"             RRRRRRR R",
	"YYYYYY     RR R RR   R",
	"YYYYYYYYYYRRRRYYR RR RYYYY",
	"YYYYYY    RRRR RRRRRR R",
	"             R        R",
	"              R       R       RR",
	"         CCCCCCR RR  R RRRRRRRR",
	"       CC  CCCCCCC   C",
	"       CCCCCCCCCCCCCCC",
	NULL
};

/*
 * Reverse a graphics character, or return unaltered if no mirror;
 * should do alphanumerics too, but I'm too lazy. <cshenton@it.hq.nasa.gov>
 */

static char
xflip_symbol(char symbol)
{
	static const char lchars[] = "`'(){}[]\\/<>";
	static const char rchars[] = "'`)(}{][/\\><";
	int pos;

	for (pos = 0; lchars[pos] != '\0'; pos++)
		if (lchars[pos] == symbol)
			return rchars[pos];

	return symbol;
}

static void
draw_daemon(int xpos, int ypos, int dxdir)
{
	int x, y;
	int attr;

	for (y = 0; daemon_pic[y] != NULL; y++)
		for (x = 0; daemon_pic[y][x] != '\0'; x++) {
			switch (daemon_attr[y][x]) {
			case 'R': attr = (FG_LIGHTRED|BG_BLACK)<<8; break;
			case 'Y': attr = (FG_YELLOW|BG_BLACK)<<8; break;
			case 'B': attr = (FG_LIGHTBLUE|BG_BLACK)<<8; break;
			case 'W': attr = (FG_LIGHTGREY|BG_BLACK)<<8; break;
			case 'C': attr = (FG_CYAN|BG_BLACK)<<8; break;
			default: attr = (FG_WHITE|BG_BLACK)<<8; break;
			}
			if (dxdir < 0) {	/* Moving left */
				CONSOLE_VECT(xpos + x, ypos + y) =
					scr_map[daemon_pic[y][x]]|attr;
			} else {		/* Moving right */
				CONSOLE_VECT(xpos + DAEMON_MAX_WIDTH - x - 1, ypos + y) =
					scr_map[xflip_symbol(daemon_pic[y][x])]|attr;
			}
		}
}

#ifndef DAEMON_ONLY
static void
draw_string(int xpos, int ypos, char *s, int len)
{
	int x;

	for (x = 0; x < len; x++)
		CONSOLE_VECT(xpos + x, ypos) =
			scr_map[s[x]]|(FG_LIGHTGREEN|BG_BLACK)<<8;
}
#endif

static void
daemon_saver(int blank)
{
#ifndef DAEMON_ONLY
	static const char message[] = {"FreeBSD 3.0 CURRENT"};
	static int txpos = 10, typos = 10;
	static int txdir = -1, tydir = -1;
#endif
	static int dxpos = 0, dypos = 0;
	static int dxdir = 1, dydir = 1;
	static int moved_daemon = 0;
	scr_stat *scp = cur_console;

	if (blank) {
		if (scrn_blanked++ < 2)
			return;
		fillw((FG_LIGHTGREY|BG_BLACK)<<8|scr_map[0x20], Crtat,
		      scp->xsize * scp->ysize);
		set_border(0);
		scrn_blanked = 1;

		if (++moved_daemon) {
			if (dxdir > 0) {
				if (dxpos == scp->xsize - DAEMON_MAX_WIDTH)
					dxdir = -1;
			} else {
				if (dxpos == 0) dxdir = 1;
			}
			if (dydir > 0) {
				if (dypos == scp->ysize - DAEMON_MAX_HEIGHT)
					dydir = -1;
			} else {
				if (dypos == 0) dydir = 1;
			}
			moved_daemon = -1;
			dxpos += dxdir; dypos += dydir;
		}

#ifndef DAEMON_ONLY
		if (txdir > 0) {
			if (txpos == scp->xsize - sizeof(message)-1)
				txdir = -1;
		} else {
			if (txpos == 0) txdir = 1;
		}
		if (tydir > 0) {
			if (typos == scp->ysize - 1)
				tydir = -1;
		} else {
			if (typos == 0) tydir = 1;
		}
		txpos += txdir; typos += tydir;
#endif

 		draw_daemon(dxpos, dypos, dxdir);
#ifndef DAEMON_ONLY
		draw_string(txpos, typos, (char *)message, sizeof(message)-1);
#endif
	} else {
		if (scrn_blanked) {
			set_border(scp->border);
			scrn_blanked = 0;
			scp->start = 0;
			scp->end = scp->xsize * scp->ysize;
		}
	}
}

static int
daemon_saver_load(struct lkm_table *lkmtp, int cmd)
{
	(*current_saver)(0);
	old_saver = current_saver;
	current_saver = daemon_saver;
	return 0;
}

static int
daemon_saver_unload(struct lkm_table *lkmtp, int cmd)
{
	(*current_saver)(0);
	current_saver = old_saver;
	return 0;
}

int
daemon_saver_mod(struct lkm_table *lkmtp, int cmd, int ver)
{
	MOD_DISPATCH(daemon_saver, lkmtp, cmd, ver,
		daemon_saver_load, daemon_saver_unload, lkm_nullcmd);
}
