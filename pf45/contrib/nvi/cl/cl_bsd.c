/*-
 * Copyright (c) 1995, 1996
 *	Keith Bostic.  All rights reserved.
 *
 * See the LICENSE file for redistribution information.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "@(#)cl_bsd.c	8.29 (Berkeley) 7/1/96";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <ctype.h>
#include <curses.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "../common/common.h"
#include "../vi/vi.h"
#include "cl.h"

static char	*ke;				/* Keypad on. */
static char	*ks;				/* Keypad off. */
static char	*vb;				/* Visible bell string. */

/*
 * HP's support the entire System V curses package except for the tigetstr
 * and tigetnum functions.  Ultrix supports the BSD curses package except
 * for the idlok function.  Cthulu only knows why.  Break things up into a
 * minimal set of functions.
 */

#ifndef HAVE_CURSES_ADDNSTR
/*
 * addnstr --
 *
 * PUBLIC: #ifndef HAVE_CURSES_ADDNSTR
 * PUBLIC: int addnstr __P((char *, int));
 * PUBLIC: #endif
 */
int
addnstr(s, n)
	char *s;
	int n;
{
	int ch;

	while (n-- && (ch = *s++))
		addch(ch);
	return (OK);
}
#endif

#ifndef	HAVE_CURSES_BEEP
/*
 * beep --
 *
 * PUBLIC: #ifndef HAVE_CURSES_BEEP
 * PUBLIC: void beep __P((void));
 * PUBLIC: #endif
 */
void
beep()
{
	(void)write(1, "\007", 1);	/* '\a' */
}
#endif /* !HAVE_CURSES_BEEP */

#ifndef	HAVE_CURSES_FLASH
/*
 * flash --
 *	Flash the screen.
 *
 * PUBLIC: #ifndef HAVE_CURSES_FLASH
 * PUBLIC: void flash __P((void));
 * PUBLIC: #endif
 */
void
flash()
{
	if (vb != NULL) {
		(void)tputs(vb, 1, cl_putchar);
		(void)fflush(stdout);
	} else
		beep();
}
#endif /* !HAVE_CURSES_FLASH */

#ifndef	HAVE_CURSES_IDLOK
/*
 * idlok --
 *	Turn on/off hardware line insert/delete.
 *
 * PUBLIC: #ifndef HAVE_CURSES_IDLOK
 * PUBLIC: void idlok __P((WINDOW *, int));
 * PUBLIC: #endif
 */
void
idlok(win, bf)
	WINDOW *win;
	int bf;
{
	return;
}
#endif /* !HAVE_CURSES_IDLOK */

#ifndef	HAVE_CURSES_KEYPAD
/*
 * keypad --
 *	Put the keypad/cursor arrows into or out of application mode.
 *
 * PUBLIC: #ifndef HAVE_CURSES_KEYPAD
 * PUBLIC: int keypad __P((void *, int));
 * PUBLIC: #endif
 */
int
keypad(a, on)
	void *a;
	int on;
{
	char *p;

	if ((p = tigetstr(on ? "smkx" : "rmkx")) != (char *)-1) {
		(void)tputs(p, 0, cl_putchar);
		(void)fflush(stdout);
	}
	return (0);
}
#endif /* !HAVE_CURSES_KEYPAD */

#ifndef	HAVE_CURSES_NEWTERM
/*
 * newterm --
 *	Create a new curses screen.
 *
 * PUBLIC: #ifndef HAVE_CURSES_NEWTERM
 * PUBLIC: void *newterm __P((const char *, FILE *, FILE *));
 * PUBLIC: #endif
 */
void *
newterm(a, b, c)
	const char *a;
	FILE *b, *c;
{
	return (initscr());
}
#endif /* !HAVE_CURSES_NEWTERM */

#ifndef	HAVE_CURSES_SETUPTERM
/*
 * setupterm --
 *	Set up terminal.
 *
 * PUBLIC: #ifndef HAVE_CURSES_SETUPTERM
 * PUBLIC: void setupterm __P((char *, int, int *));
 * PUBLIC: #endif
 */
void
setupterm(ttype, fno, errp)
	char *ttype;
	int fno, *errp;
{
	static char buf[2048];
	char *p;

	if ((*errp = tgetent(buf, ttype)) > 0) {
		if (ke != NULL)
			free(ke);
		ke = ((p = tigetstr("rmkx")) == (char *)-1) ?
		    NULL : strdup(p);
		if (ks != NULL)
			free(ks);
		ks = ((p = tigetstr("smkx")) == (char *)-1) ?
		    NULL : strdup(p);
		if (vb != NULL)
			free(vb);
		vb = ((p = tigetstr("flash")) == (char *)-1) ?
		    NULL : strdup(p);
	}
}
#endif /* !HAVE_CURSES_SETUPTERM */

#ifndef	HAVE_CURSES_TIGETSTR
/* Terminfo-to-termcap translation table. */
typedef struct _tl {
	char *terminfo;			/* Terminfo name. */
	char *termcap;			/* Termcap name. */
} TL;
static const TL list[] = {
	"cols",		"co",		/* Terminal columns. */
	"cup",		"cm",		/* Cursor up. */
	"cuu1",		"up",		/* Cursor up. */
	"el",		"ce",		/* Clear to end-of-line. */
	"flash",	"vb",		/* Visible bell. */
	"kcub1",  	"kl",		/* Cursor left. */
	"kcud1",	"kd",		/* Cursor down. */
	"kcuf1",	"kr",		/* Cursor right. */
	"kcuu1",  	"ku",		/* Cursor up. */
	"kdch1",	"kD",		/* Delete character. */
	"kdl1",		"kL",		/* Delete line. */
	"ked",		"kS",		/* Delete to end of screen. */
	"kel",		"kE",		/* Delete to eol. */
	"kend",		"@7",		/* Go to eol. */
	"khome",	"kh",		/* Go to sol. */
	"kich1",	"kI",		/* Insert at cursor. */
	"kil1",		"kA",		/* Insert line. */
	"kind",		"kF",		/* Scroll down. */
	"kll",		"kH",		/* Go to eol. */
	"knp",		"kN",		/* Page down. */
	"kpp",		"kP",		/* Page up. */
	"kri",		"kR",		/* Scroll up. */
	"lines",	"li",		/* Terminal lines. */
	"rmcup",	"te",		/* Terminal end string. */
	"rmkx",		"ke",		/* Exit "keypad-transmit" mode. */
	"rmso",		"se",		/* Standout end. */
	"smcup",	"ti",		/* Terminal initialization string. */
	"smkx",		"ks",		/* Enter "keypad-transmit" mode. */
	"smso",		"so",		/* Standout begin. */
};

#ifdef _AIX
/*
 * AIX's implementation for function keys greater than 10 is different and
 * only goes as far as 36.
 */
static const char codes[] = {
/*  0-10 */ '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', ';',
/* 11-20 */ '<', '>', '!', '@', '#', '$', '%', '^', '&', '*',
/* 21-30 */ '(', ')', '-', '_', '+', ',', ':', '?', '[', ']',
/* 31-36 */ '{', '}', '|', '~', '/', '='
};

#else

/*
 * !!!
 * Historically, the 4BSD termcap code didn't support functions keys greater
 * than 9.  This was silently enforced -- asking for key k12 would return the
 * value for k1.  We try and get around this by using the tables specified in
 * the terminfo(TI_ENV) man page from the 3rd Edition SVID.  This assumes the
 * implementors of any System V compatibility code or an extended termcap used
 * those codes.
 */
static const char codes[] = {
/*  0-10 */ '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', ';',
/* 11-19 */ '1', '2', '3', '4', '5', '6', '7', '8', '9',
/* 20-63 */ 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
	    'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
	    'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
	    'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
};
#endif /* _AIX */

/*
 * lcmp --
 *	list comparison routine for bsearch.
 */
static int
lcmp(a, b)
	const void *a, *b;
{
	return (strcmp(a, ((TL *)b)->terminfo));
}

/*
 * tigetstr --
 *
 * Vendors put the prototype for tigetstr into random include files, including
 * <term.h>, which we can't include because it makes other systems unhappy.
 * Try and work around the problem, since we only care about the return value.
 *
 * PUBLIC: #ifdef HAVE_CURSES_TIGETSTR
 * PUBLIC: char *tigetstr();
 * PUBLIC: #else
 * PUBLIC: char *tigetstr __P((char *));
 * PUBLIC: #endif
 */
char *
tigetstr(name)
	char *name;
{
	static char sbuf[256];
	TL *tlp;
	int n;
	char *p, keyname[3];

	if ((tlp = bsearch(name,
	    list, sizeof(list) / sizeof(TL), sizeof(TL), lcmp)) == NULL) {
#ifdef _AIX
		if (name[0] == 'k' &&
		    name[1] == 'f' && (n = atoi(name + 2)) <= 36) {
			keyname[0] = 'k';
			keyname[1] = codes[n];
			keyname[2] = '\0';
#else
		if (name[0] == 'k' &&
		    name[1] == 'f' && (n = atoi(name + 2)) <= 63) {
			keyname[0] = n <= 10 ? 'k' : 'F';
			keyname[1] = codes[n];
			keyname[2] = '\0';
#endif
			name = keyname;
		}
	} else
		name = tlp->termcap;

	p = sbuf;
#ifdef _AIX
	return ((p = tgetstr(name, &p)) == NULL ? (char *)-1 : strcpy(sbuf, p));
#else
	return (tgetstr(name, &p) == NULL ? (char *)-1 : sbuf);
#endif
}

/*
 * tigetnum --
 *
 * PUBLIC: #ifndef HAVE_CURSES_TIGETSTR
 * PUBLIC: int tigetnum __P((char *));
 * PUBLIC: #endif
 */
int
tigetnum(name)
	char *name;
{
	TL *tlp;
	int val;

	if ((tlp = bsearch(name,
	    list, sizeof(list) / sizeof(TL), sizeof(TL), lcmp)) != NULL) {
		name = tlp->termcap;
	}

	return ((val = tgetnum(name)) == -1 ? -2 : val);
}
#endif /* !HAVE_CURSES_TIGETSTR */
