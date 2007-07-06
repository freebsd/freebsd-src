/*
 * Copyright (c) 1997, 1999 Hellmuth Michaelis. All rights reserved.
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
 *	isdntel - isdn4bsd telephone answering support
 *      ==============================================
 *
 *	$Id: defs.h,v 1.10 1999/12/13 21:25:26 hm Exp $ 
 *
 * $FreeBSD$
 *
 *      last edit-date: [Mon Dec 13 21:53:50 1999]
 *
 *----------------------------------------------------------------------------*/

#include <ncurses.h>
#include <stdio.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/time.h>
#if defined(__FreeBSD__) && __FreeBSD__ >= 3
#include <dirent.h>
#else
#include <sys/dir.h>
#endif
#include <sys/param.h>

#include <i4b/i4b_ioctl.h>

#define GOOD	0
#define	ERROR	(-1)
#define WARNING (-2)

#define	SPOOLDIR	"/var/isdn"
#define PLAYCMD		"cat %s | g711conv -a >/dev/audio"

/* reread timeout in seconds */

#define REREADTIMEOUT	60

/* window dimensions */

#define START_O		3	/* main window start  */

#define DAT_POS		0
#define TIM_POS		(DAT_POS+10)
#define DST_POS		(TIM_POS+8)
#define SRC_POS		(DST_POS+17)
#define ALI_POS		(SRC_POS+17)
#define SEC_POS		(ALI_POS+21)
#define LAST_POS	(SEC_POS+5)

/* fullscreen mode menu window */

#define WMITEMS 	5		/* no of items */
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

struct onefile {
	char	*fname;		/* filename */
	char	*date;
	char	*time;
	char	*srcnumber;
	char	*dstnumber;
	char	*seconds;
	char	*alias;
	int	len;
	struct onefile *next;	/* ptr to next entry */
	struct onefile *prev;	/* prt to previous entry */
};

#ifdef MAIN

int curses_ready = 0;		/* flag, curses display is initialized */

struct onefile *cur_file = NULL;/* the CURRENT filename */
struct onefile *first = NULL;	/* init dir-list head-ptr */
struct onefile *last = NULL;	/* init dir-list tail-ptr */

WINDOW *main_w;			/* curses main window pointer */

int nofiles = 0;
int cur_pos = 0;

char *spooldir = SPOOLDIR;
char *playstring = PLAYCMD;

#else

extern int curses_ready;

extern struct onefile *cur_file;
extern struct onefile *first;
extern struct onefile *last;

extern WINDOW *main_w;

extern int nofiles;
extern int cur_pos;

extern char *spooldir;
extern char *playstring;

#endif

extern void init_alias( char *filename );
extern void init_files( int inipos );
extern void init_screen ( void );
extern void do_menu ( void );
extern int fill_list( void );
extern char *get_alias( char *number );
extern int main ( int argc, char **argv );
extern void do_quit ( int exitval );
extern void fatal ( char *fmt, ... );
extern void error ( char *fmt, ... );
extern void play ( struct onefile * );
extern void delete ( struct onefile * );
extern void reread( void );

/* EOF */
