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
 *	$Id: daemon_saver.c,v 1.2.2.3 1997/09/02 10:37:36 yokota Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/exec.h>
#include <sys/sysent.h>
#include <sys/lkm.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>

#include <machine/md_var.h>
#include <i386/include/pc/display.h>

#include <saver.h>

#define CONSOLE_VECT(x, y) \
	((u_short*)(Crtat + (y)*cur_console->xsize + (x)))

#define DAEMON_MAX_WIDTH	32
#define DAEMON_MAX_HEIGHT	19

MOD_MISC(daemon_saver);

static char *message;
static int messagelen;

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
clear_daemon(int xpos, int ypos, int dxdir, int xoff, int yoff, 
	    int xlen, int ylen)
{
	int y;

	if (xlen <= 0)
		return;
	for (y = yoff; y < ylen; y++)
		fillw(((FG_LIGHTGREY|BG_BLACK) << 8) | scr_map[0x20], 
		      CONSOLE_VECT(xpos + xoff, ypos + y), xlen - xoff);
}

static void
draw_daemon(int xpos, int ypos, int dxdir, int xoff, int yoff, 
	    int xlen, int ylen)
{
	int x, y;
	int px;
	int attr;

	for (y = yoff; y < ylen; y++) {
		if (dxdir < 0)
			px = xoff;
		else
			px = DAEMON_MAX_WIDTH - xlen;
		if (px >= strlen(daemon_pic[y]))
			continue;
		for (x = xoff; (x < xlen) && (daemon_pic[y][px] != '\0'); x++, px++) {
			switch (daemon_attr[y][px]) {
			case 'R': attr = (FG_LIGHTRED|BG_BLACK)<<8; break;
			case 'Y': attr = (FG_YELLOW|BG_BLACK)<<8; break;
			case 'B': attr = (FG_LIGHTBLUE|BG_BLACK)<<8; break;
			case 'W': attr = (FG_LIGHTGREY|BG_BLACK)<<8; break;
			case 'C': attr = (FG_CYAN|BG_BLACK)<<8; break;
			default: attr = (FG_WHITE|BG_BLACK)<<8; break;
			}
			if (dxdir < 0) {	/* Moving left */
				*CONSOLE_VECT(xpos + x, ypos + y) =
					scr_map[daemon_pic[y][px]]|attr;
			} else {		/* Moving right */
				*CONSOLE_VECT(xpos + DAEMON_MAX_WIDTH - px - 1, ypos + y) =
					scr_map[xflip_symbol(daemon_pic[y][px])]|attr;
			}
		}
	}
}

static void
clear_string(int xpos, int ypos, int xoff, char *s, int len)
{
	if (len <= 0)
		return;
	fillw(((FG_LIGHTGREY|BG_BLACK) << 8) | scr_map[0x20], 
	      CONSOLE_VECT(xpos + xoff, ypos), len - xoff);
}

static void
draw_string(int xpos, int ypos, int xoff, char *s, int len)
{
	int x;

	for (x = xoff; x < len; x++)
		*CONSOLE_VECT(xpos + x, ypos) =
			scr_map[s[x]]|(FG_LIGHTGREEN|BG_BLACK)<<8;
}

static void
daemon_saver(int blank)
{
	static int txpos = 10, typos = 10;
	static int txdir = -1, tydir = -1;
	static int dxpos = 0, dypos = 0;
	static int dxdir = 1, dydir = 1;
	static int moved_daemon = 0;
	static int xoff, yoff, toff;
	static int xlen, ylen, tlen;
	scr_stat *scp = cur_console;
	int min, max;

	if (blank) {
		if (scrn_blanked == 0) {
			/* clear the screen and set the border color */
			fillw(((FG_LIGHTGREY|BG_BLACK) << 8) | scr_map[0x20],
			      Crtat, scp->xsize * scp->ysize);
			set_border(0);
			xlen = ylen = tlen = 0;
		}
		if (scrn_blanked++ < 2)
			return;
		scrn_blanked = 1;

 		clear_daemon(dxpos, dypos, dxdir, xoff, yoff, xlen, ylen);
		clear_string(txpos, typos, toff, (char *)message, tlen);

		if (++moved_daemon) {
			/*
			 * The daemon picture may be off the screen, if
			 * screen size is chagened while the screen
			 * saver is inactive. Make sure the origin of
			 * the picture is between min and max.
			 */
			if (scp->xsize <= DAEMON_MAX_WIDTH) {
				/*
				 * If the screen width is too narrow, we
				 * allow part of the picture go off
				 * the screen so that the daemon won't
				 * flip too often.
				 */
				min = scp->xsize - DAEMON_MAX_WIDTH - 10;
				max = 10;
			} else {
				min = 0;
				max = scp->xsize - DAEMON_MAX_WIDTH;
			}
			if (dxpos <= min) {
				dxpos = min;
				dxdir = 1;
			} else if (dxpos >= max) {
				dxpos = max;
				dxdir = -1;
			}

			if (scp->ysize <= DAEMON_MAX_HEIGHT) {
				min = scp->ysize - DAEMON_MAX_HEIGHT - 10;
				max = 10;
			} else {
				min = 0;
				max = scp->ysize - DAEMON_MAX_HEIGHT;
			}
			if (dypos <= min) {
				dypos = min;
				dydir = 1;
			} else if (dypos >= max) {
				dypos = max;
				dydir = -1;
			}

			moved_daemon = -1;
			dxpos += dxdir; dypos += dydir;

			/* clip the picture */
			xoff = 0;
			xlen = DAEMON_MAX_WIDTH;
			if (dxpos + xlen <= 0)
				xlen = 0;
			else if (dxpos < 0)
				xoff = -dxpos;
			if (dxpos >= scp->xsize)
				xlen = 0;
			else if (dxpos + xlen > scp->xsize)
				xlen = scp->xsize - dxpos;
			yoff = 0;
			ylen = DAEMON_MAX_HEIGHT;
			if (dypos + ylen <= 0)
				ylen = 0;
			else if (dypos < 0)
				yoff = -dypos;
			if (dypos >= scp->ysize)
				ylen = 0;
			else if (dypos + ylen > scp->ysize)
				ylen = scp->ysize - dypos;
		}

		if (scp->xsize <= messagelen) {
			min = scp->xsize - messagelen - 10;
			max = 10;
		} else {
			min = 0;
			max = scp->xsize - messagelen;
		}
		if (txpos <= min) {
			txpos = min;
			txdir = 1;
		} else if (txpos >= max) {
			txpos = max;
			txdir = -1;
		}
		if (typos <= 0) {
			typos = 0;
			tydir = 1;
		} else if (typos >= scp->ysize - 1) {
			typos = scp->ysize - 1;
			tydir = -1;
		}
		txpos += txdir; typos += tydir;

		toff = 0;
		tlen = messagelen;
		if (txpos + tlen <= 0)
			tlen = 0;
		else if (txpos < 0)
			toff = -txpos;
		if (txpos >= scp->xsize)
			tlen = 0;
		else if (txpos + tlen > scp->xsize)
			tlen = scp->xsize - txpos;

 		draw_daemon(dxpos, dypos, dxdir, xoff, yoff, xlen, ylen);
		draw_string(txpos, typos, toff, (char *)message, tlen);
	} else {
		if (scrn_blanked > 0) {
			set_border(scp->border);
			scrn_blanked = 0;
		}
	}
}

static int
daemon_saver_load(struct lkm_table *lkmtp, int cmd)
{
	int err;

	messagelen = strlen(hostname) + 3 + strlen(ostype) + 1 + 
	    strlen(osrelease);
	message = malloc(messagelen + 1, M_DEVBUF, M_WAITOK);
	sprintf(message, "%s - %s %s", hostname, ostype, osrelease);

	err = add_scrn_saver(daemon_saver);
	if (err != 0)
		free(message, M_DEVBUF);
	return err;
}

static int
daemon_saver_unload(struct lkm_table *lkmtp, int cmd)
{
	int err;

	err = remove_scrn_saver(daemon_saver);
	if (err == 0)
		free(message, M_DEVBUF);
	return err;
}

int
daemon_saver_mod(struct lkm_table *lkmtp, int cmd, int ver)
{
	MOD_DISPATCH(daemon_saver, lkmtp, cmd, ver,
		daemon_saver_load, daemon_saver_unload, lkm_nullcmd);
}
