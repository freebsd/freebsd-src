/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
static char sccsid[] = "@(#)tty.c	8.6 (Berkeley) 1/10/95";
#endif /* not lint */

#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

#include "curses.h"

/*
 * In general, curses should leave tty hardware settings alone (speed, parity,
 * word size).  This is most easily done in BSD by using TCSASOFT on all
 * tcsetattr calls.  On other systems, it would be better to get and restore
 * those attributes at each change, or at least when stopped and restarted.
 * See also the comments in getterm().
 */
#ifdef TCSASOFT
int __tcaction = 1;			/* Ignore hardware settings. */
#else
int __tcaction = 0;
#endif

struct termios __orig_termios, __baset;
static struct termios cbreakt, rawt, *curt;
static int useraw;

#ifndef	OXTABS
#ifdef	XTABS			/* SMI uses XTABS. */
#define	OXTABS	XTABS
#else
#define	OXTABS	0
#endif
#endif

/*
 * gettmode --
 *	Do terminal type initialization.
 */
int
gettmode()
{
	useraw = 0;
	
	if (tcgetattr(STDIN_FILENO, &__orig_termios))
		return (ERR);

	__baset = __orig_termios;
	__baset.c_oflag &= ~OXTABS;

	GT = 0;		/* historical. was used before we wired OXTABS off */
	NONL = (__baset.c_oflag & ONLCR) == 0;

	/*
	 * XXX
	 * System V and SMI systems overload VMIN and VTIME, such that
	 * VMIN is the same as the VEOF element, and VTIME is the same
	 * as the VEOL element.  This means that, if VEOF was ^D, the
	 * default VMIN is 4.  Majorly stupid.
	 */
	cbreakt = __baset;
	cbreakt.c_lflag &= ~ICANON;
	cbreakt.c_cc[VMIN] = 1;
	cbreakt.c_cc[VTIME] = 0;

	rawt = cbreakt;
	rawt.c_iflag &= ~(IGNBRK|BRKINT|PARMRK|INLCR|IGNCR|ICRNL|IXON);
	rawt.c_oflag &= ~OPOST;
	rawt.c_lflag &= ~(ECHO|ECHONL|ICANON|ISIG|IEXTEN);

	/*
	 * In general, curses should leave hardware-related settings alone.
	 * This includes parity and word size.  Older versions set the tty
	 * to 8 bits, no parity in raw(), but this is considered to be an
	 * artifact of the old tty interface.  If it's desired to change
	 * parity and word size, the TCSASOFT bit has to be removed from the
	 * calls that switch to/from "raw" mode.
	 */
	if (!__tcaction) {
		rawt.c_iflag &= ~ISTRIP;
		rawt.c_cflag &= ~(CSIZE|PARENB);
		rawt.c_cflag |= CS8;
	}

	curt = &__baset;
	return (tcsetattr(STDIN_FILENO, __tcaction ?
	    TCSASOFT | TCSADRAIN : TCSADRAIN, curt) ? ERR : OK);
}

int
raw()
{
	useraw = __pfast = __rawmode = 1;
	curt = &rawt;
	return (tcsetattr(STDIN_FILENO, __tcaction ?
	    TCSASOFT | TCSADRAIN : TCSADRAIN, curt) ? ERR : OK);
}

int
noraw()
{
	useraw = __pfast = __rawmode = 0;
	curt = &__baset;
	return (tcsetattr(STDIN_FILENO, __tcaction ?
	    TCSASOFT | TCSADRAIN : TCSADRAIN, curt) ? ERR : OK);
}

int
cbreak()
{

	__rawmode = 1;
	curt = useraw ? &rawt : &cbreakt;
	return (tcsetattr(STDIN_FILENO, __tcaction ?
	    TCSASOFT | TCSADRAIN : TCSADRAIN, curt) ? ERR : OK);
}

int
nocbreak()
{

	__rawmode = 0;
	curt = useraw ? &rawt : &__baset;
	return (tcsetattr(STDIN_FILENO, __tcaction ?
	    TCSASOFT | TCSADRAIN : TCSADRAIN, curt) ? ERR : OK);
}
	
int
echo()
{
	rawt.c_lflag |= ECHO;
	cbreakt.c_lflag |= ECHO;
	__baset.c_lflag |= ECHO;
	
	__echoit = 1;
	return (tcsetattr(STDIN_FILENO, __tcaction ?
	    TCSASOFT | TCSADRAIN : TCSADRAIN, curt) ? ERR : OK);
}

int
noecho()
{
	rawt.c_lflag &= ~ECHO;
	cbreakt.c_lflag &= ~ECHO;
	__baset.c_lflag &= ~ECHO;
	
	__echoit = 0;
	return (tcsetattr(STDIN_FILENO, __tcaction ?
	    TCSASOFT | TCSADRAIN : TCSADRAIN, curt) ? ERR : OK);
}

int
nl()
{
	rawt.c_iflag |= ICRNL;
	rawt.c_oflag |= ONLCR;
	cbreakt.c_iflag |= ICRNL;
	cbreakt.c_oflag |= ONLCR;
	__baset.c_iflag |= ICRNL;
	__baset.c_oflag |= ONLCR;

	__pfast = __rawmode;
	return (tcsetattr(STDIN_FILENO, __tcaction ?
	    TCSASOFT | TCSADRAIN : TCSADRAIN, curt) ? ERR : OK);
}

int
nonl()
{
	rawt.c_iflag &= ~ICRNL;
	rawt.c_oflag &= ~ONLCR;
	cbreakt.c_iflag &= ~ICRNL;
	cbreakt.c_oflag &= ~ONLCR;
	__baset.c_iflag &= ~ICRNL;
	__baset.c_oflag &= ~ONLCR;

	__pfast = 1;
	return (tcsetattr(STDIN_FILENO, __tcaction ?
	    TCSASOFT | TCSADRAIN : TCSADRAIN, curt) ? ERR : OK);
}

void
__startwin()
{
	static char *stdbuf;
	static size_t len;

	(void)fflush(stdout);

	/*
	 * Some C libraries default to a 1K buffer when talking to a tty.
	 * With a larger screen, especially across a network, we'd like
	 * to get it to all flush in a single write.  Make it twice as big
	 * as just the characters (so that we have room for cursor motions
	 * and standout information) but no more than 8K.
	 */
	if (stdbuf == NULL) {
		if ((len = LINES * COLS * 2) > 8192)
			len = 8192;
		if ((stdbuf = malloc(len)) == NULL)
			len = 0;
	}
	(void)setvbuf(stdout, stdbuf, _IOFBF, len);

	tputs(TI, 0, __cputchar);
	tputs(VS, 0, __cputchar);
}

int
endwin()
{
	__restore_stophandler();

	if (curscr != NULL) {
		if (curscr->flags & __WSTANDOUT) {
			tputs(SE, 0, __cputchar);
			curscr->flags &= ~__WSTANDOUT;
		}
		__mvcur(curscr->cury, curscr->curx, curscr->maxy - 1, 0, 0);
	}

	(void)tputs(VE, 0, __cputchar);
	(void)tputs(TE, 0, __cputchar);
	(void)fflush(stdout);
	(void)setvbuf(stdout, NULL, _IOLBF, 0);

	return (tcsetattr(STDIN_FILENO, __tcaction ?
	    TCSASOFT | TCSADRAIN : TCSADRAIN, &__orig_termios) ? ERR : OK);
}

/*
 * The following routines, savetty and resetty are completely useless and
 * are left in only as stubs.  If people actually use them they will almost
 * certainly screw up the state of the world.
 */
static struct termios savedtty;
int
savetty()
{
	return (tcgetattr(STDIN_FILENO, &savedtty) ? ERR : OK);
}

int
resetty()
{
	return (tcsetattr(STDIN_FILENO, __tcaction ?
	    TCSASOFT | TCSADRAIN : TCSADRAIN, &savedtty) ? ERR : OK);
}
