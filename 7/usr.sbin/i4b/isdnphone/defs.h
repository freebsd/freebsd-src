/*
 * Copyright (c) 1999 Hellmuth Michaelis. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *---------------------------------------------------------------------------
 *
 *	isdnphone - header file
 *      =======================
 *
 *	$Id: defs.h,v 1.6 1999/12/13 21:25:26 hm Exp $ 
 *
 * $FreeBSD$
 *
 *      last edit-date: [Mon Dec 13 21:52:46 1999]
 *
 *----------------------------------------------------------------------------*/

#include <ncurses.h>
#include <stdio.h>
#include <stdarg.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>

#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <sys/soundcard.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/param.h>

#include <i4b/i4b_ioctl.h>
#include <i4b/i4b_tel_ioctl.h>

/* device file prefixes */

#define I4BTELDEVICE	"/dev/i4btel"
#define I4BTELDDEVICE	"/dev/i4bteld"
#define AUDIODEVICE	"/dev/audio"

#define GOOD	0
#define	ERROR	(-1)
#define WARNING (-2)

/* main window dimensions */

#define MW_ROW		5
#define MW_COL		8

#define MW_WIDTH	60
#define MW_HEIGHT	8

#define DB_ROW		15
#define DB_COL		1
#define DB_WID		79
#define DB_HGT		9

#define MW_STATEY	2
#define MW_STATEX	1
#define MW_STX		10

#define MW_NUMY		4
#define MW_NUMX		1
#define MW_NUX		10

#define MW_MSGY		6
#define MW_MSGX		1
#define MW_MSX		10

/* fullscreen mode menu window */

#define WMITEMS 	4		/* no of items */
#define WMENU_LEN 	18		/* window width */
#define WMENU_HGT 	(WMITEMS+4)	/* window height */
#define WMENU_TITLE 	"Command"
#define WMENU_POSLN	8		/* window position: lines */
#define WMENU_POSCO	20		/* window position: columns */

#define CR		0x0d
#define LF		0x0a
#define	TAB		0x09
#define	CNTRL_D		0x04
#define CNTRL_L		0x0c

#define ST_IDLE		0
#define ST_DIALING	1
#define ST_ACTIVE	2
#define ST_MAX		2

#define AUDIORATE	8000

#ifdef MAIN

WINDOW *main_w;			/* curses main window pointer */
WINDOW *dbg_w;

int curses_ready = 0;		/* flag, curses display is initialized */
int state = ST_IDLE;

char *states[] = {
	"IDLE",
	"DIALING",
	"ACTIVE"
};

int dialerfd = -1;
int audiofd = -1;
int telfd = -1;
int curx;
char numberbuffer[TELNO_MAX];

int play_fmt = AFMT_MU_LAW;
int rec_fmt = AFMT_MU_LAW;

int opt_unit = 0;
int opt_d = 0;
#else

extern WINDOW *main_w;
extern WINDOW *dbg_w;

extern int curses_ready;
extern int state;

extern char *states[];

extern int dialerfd;
extern int audiofd;
extern int telfd;
extern int curx;
extern char numberbuffer[];

extern int play_fmt;
extern int rec_fmt;

int opt_unit;
int opt_d;

#endif

extern void audio_hdlr ( void );
extern void tel_hdlr ( void );
extern void init_mainw ( void );
extern int init_audio ( char * );
extern void do_menu ( void );
extern int main ( int argc, char **argv );
extern void do_quit ( int exitval );
extern void fatal ( char *fmt, ... );
extern void message ( char *fmt, ... );
extern void do_dial ( char *number );
extern void do_hangup ( void );

extern void audiowrite ( int, unsigned char * );
extern void telwrite ( int, unsigned char * );

extern void newstate ( int newstate );

int init_dial(char *device);
void dial_hdlr(void);
int init_tel(char *device);

extern void debug ( char *fmt, ... );

/* EOF */
