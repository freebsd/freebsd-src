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
 *	i4b remote monitor - private header
 *	-----------------------------------
 *
 *	$Id: monprivate.h,v 1.10 1999/12/13 21:25:26 hm Exp $
 *
 * $FreeBSD$
 *
 *      last edit-date: [Mon Dec 13 21:52:25 1999]
 *
 *---------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <signal.h>

#ifndef WIN32
#include <unistd.h>
#include <syslog.h>
#include <regex.h>
#include <curses.h>
#include <fcntl.h>

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#endif

/*---------------------------------------------------------------------------*
 *	definitions in i4b_ioctl.h, do something for other systems
 *---------------------------------------------------------------------------*/
#if defined (__FreeBSD__) || defined(__NetBSD__) || \
    defined (__OpenBSD__) || defined(__bsdi__)

#include <i4b/i4b_ioctl.h>

#else

#define FOREIGN 1	/* we are running on a "foreign" OS */

#define I4B_TIME_FORMAT "%d.%m.%Y %H:%M:%S"
#define VERSION 0
#define REL	0
#define STEP	0

#endif

/*---------------------------------------------------------------------------*
 *	some general definitions
 *---------------------------------------------------------------------------*/
#define GOOD	0		/* general "good" or "ok" return*/
#define ERROR	(-1)		/* general error return		*/
#define WARNING (-2)		/* warning return		*/
#define INVALID (-1)		/* an invalid integer		*/

/*---------------------------------------------------------------------------*
 *	state definitions
 *---------------------------------------------------------------------------*/
#define ST_INIT		0	/* initial data */
#define ST_ICTRL	1	/* initial controller list */
#define ST_IDEV		2	/* initial entry devicename list */
#define ST_ANYEV	3	/* any event */
#define ST_RIGHT	4	/* one record in a list of monitor rights */
#define ST_CONNS	5	/* monitor connections */

/*---------------------------------------------------------------------------*
 *	curses fullscreen display definitions
 *---------------------------------------------------------------------------*/
/* window dimensions */
#define UPPER_B		2		/* upper window start  */

/* horizontal positions for upper window */
#define H_CNTL		0		/* controller		*/
#define H_TEI		2		/* TEI			*/
#define H_CHAN		(H_TEI+4)	/* channel		*/
#define H_TELN		(H_CHAN+2)	/* telephone number	*/
#define H_IFN		(H_TELN+23)	/* interfacename	*/
#define H_IO		(H_IFN+7)	/* incoming or outgoing */
#define H_OUT		(H_IO+4)	/* # of bytes out	*/
#define H_OUTBPS	(H_OUT+11)	/* bytes per second out	*/
#define H_IN		(H_OUTBPS+5)	/* # of bytes in	*/
#define H_INBPS		(H_IN+11)	/* bytes per second in	*/
#define H_UNITS		(H_INBPS+6)	/* # of charging units	*/

/* fullscreen mode menu window */
#define WMENU_LEN 	35		/* width of menu window */
#define WMENU_TITLE 	"Command"	/* title string */
#define WMENU_POSLN	10		/* menu position, line */
#define WMENU_POSCO	5		/* menu position, col */
#define WMITEMS 	4		/* no of menu items */
#define WMENU_HGT 	(WMITEMS + 4)	/* menu window height */

#define WREFRESH	0
#define WHANGUP		1
#define WREREAD		2
#define WQUIT		3

#define WMTIMEOUT	5		/* timeout in seconds */

/*---------------------------------------------------------------------------*
 *	misc
 *---------------------------------------------------------------------------*/
#define CHPOS(uctlr, uchan) (((uctlr)*2) + (uchan))

/*---------------------------------------------------------------------------*
 *	remote state
 *---------------------------------------------------------------------------*/

#define MAX_CTRL 4

typedef struct remstate {
       int ch1state;
       int ch2state;
} remstate_t;
	                
/*---------------------------------------------------------------------------*
 *	global variables
 *---------------------------------------------------------------------------*/
#ifdef MAIN

remstate_t remstate[MAX_CTRL];

int nctrl = 0;			/* # of controllers available */
int curses_ready = 0;		/* curses initialized */
int do_bell = 0;
int nentries = 0;
int fullscreen = 0;
int debug_noscreen = 0;

#ifndef WIN32
WINDOW *upper_w;		/* curses upper window pointer */
WINDOW *mid_w;			/* curses mid window pointer */
WINDOW *lower_w;		/* curses lower window pointer */
#endif

char devbuf[256];

char *sockpath = NULL;
char *hostname = NULL;
int portno;

#else /* !MAIN */

remstate_t remstate[MAX_CTRL];

int nctrl;
int curses_ready;
int do_bell;
int nentries;
int fullscreen;
int debug_noscreen;

WINDOW *upper_w;
WINDOW *mid_w;
WINDOW *lower_w;

char devbuf[256];

char *sockpath;
char *hostname;
int portno;

#endif

extern void do_exit ( int exitval );
extern void do_menu ( void );
extern void init_screen ( void );
extern void display_charge ( int pos, int charge );
extern void display_ccharge ( int pos, int units );
extern void display_connect(int pos, int dir, char *name, char *remtel, char *dev);
extern void display_acct ( int pos, int obyte, int obps, int ibyte, int ibps );
extern void display_disconnect ( int pos );
extern void display_updown ( int pos, int updown, char *device );
extern void display_l12stat ( int controller, int layer, int state );
extern void display_tei ( int controller, int tei );

extern void reread(void);
extern void hangup(int ctrl, int chan);


