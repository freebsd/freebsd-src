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
static char sccsid[] = "@(#)tty.c	8.4 (Berkeley) 5/18/94";
#endif /* not lint */

#include <termios.h>
#include <unistd.h>
#include <fcntl.h>

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

int __tty_fileno, __noterm = 0;
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
	
	if (tcgetattr(__tty_fileno = STDIN_FILENO, &__orig_termios))
	    if (tcgetattr(__tty_fileno = STDOUT_FILENO, &__orig_termios))
		__noterm = 1;

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
	return (!__noterm && tcsetattr(__tty_fileno, __tcaction ?
	    TCSASOFT | TCSADRAIN : TCSADRAIN, curt) ? ERR : OK);
	return OK;
}

int
raw()
{
	useraw = __pfast = __rawmode = 1;
	curt = &rawt;
	return (!__noterm && tcsetattr(__tty_fileno, __tcaction ?
	    TCSASOFT | TCSADRAIN : TCSADRAIN, curt) ? ERR : OK);
}

int
noraw()
{
	useraw = __pfast = __rawmode = 0;
	curt = &__baset;
	return (!__noterm && tcsetattr(__tty_fileno, __tcaction ?
	    TCSASOFT | TCSADRAIN : TCSADRAIN, curt) ? ERR : OK);
}

int
cbreak()
{

	__rawmode = 1;
	curt = useraw ? &rawt : &cbreakt;
	return (!__noterm && tcsetattr(__tty_fileno, __tcaction ?
	    TCSASOFT | TCSADRAIN : TCSADRAIN, curt) ? ERR : OK);
}

int
nocbreak()
{

	__rawmode = 0;
	curt = useraw ? &rawt : &__baset;
	return (!__noterm && tcsetattr(__tty_fileno, __tcaction ?
	    TCSASOFT | TCSADRAIN : TCSADRAIN, curt) ? ERR : OK);
}
	
int
echo()
{
	rawt.c_lflag |= ECHO;
	cbreakt.c_lflag |= ECHO;
	__baset.c_lflag |= ECHO;
	
	__echoit = 1;
	return (!__noterm && tcsetattr(__tty_fileno, __tcaction ?
	    TCSASOFT | TCSADRAIN : TCSADRAIN, curt) ? ERR : OK);
}

int
noecho()
{
	rawt.c_lflag &= ~ECHO;
	cbreakt.c_lflag &= ~ECHO;
	__baset.c_lflag &= ~ECHO;
	
	__echoit = 0;
	return (!__noterm && tcsetattr(__tty_fileno, __tcaction ?
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
	return (!__noterm && tcsetattr(__tty_fileno, __tcaction ?
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
	return (!__noterm && tcsetattr(__tty_fileno, __tcaction ?
	    TCSASOFT | TCSADRAIN : TCSADRAIN, curt) ? ERR : OK);
}

void
__startwin()
{
	(void)fflush(stdout);
	(void)setvbuf(stdout, NULL, _IOFBF, 0);

	tputs(TI, 0, __cputchar);
	tputs(VS, 0, __cputchar);
	if (curscr != NULL && __usecs)
		__set_scroll_region(0, curscr->maxy - 1, 0, 0);
}

void
__set_scroll_region(top, bot, ox, oy)
int top, bot, ox, oy;
{
	if (SC != NULL && RC != NULL)
		tputs(SC, 0, __cputchar);
	tputs(__tscroll(CS, top, bot), 1, __cputchar);
	if (SC != NULL && RC != NULL)
		tputs(RC, 0, __cputchar);
	else {
		tputs(HO, 1, __cputchar);
		__mvcur(0, 0, oy, ox, 1);
	}
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
		if (__usecs)
			__set_scroll_region(0, curscr->maxy - 1, 0, 0);
		__mvcur(curscr->curx, curscr->cury, curscr->maxy - 1, 0, 0);
	}

	(void)tputs(VE, 0, __cputchar);
	(void)tputs(TE, 0, __cputchar);
	(void)fflush(stdout);
	(void)setvbuf(stdout, NULL, _IOLBF, 0);

	return (!__noterm && tcsetattr(__tty_fileno, __tcaction ?
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
	return (!__noterm && tcgetattr(__tty_fileno, &savedtty) ? ERR : OK);
}

int
resetty()
{
	return (!__noterm && tcsetattr(__tty_fileno, __tcaction ?
	    TCSASOFT | TCSADRAIN : TCSADRAIN, &savedtty) ? ERR : OK);
}
