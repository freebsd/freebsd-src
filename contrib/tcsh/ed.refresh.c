/* $Header: /src/pub/tcsh/ed.refresh.c,v 3.29 2002/03/08 17:36:45 christos Exp $ */
/*
 * ed.refresh.c: Lower level screen refreshing functions
 */
/*-
 * Copyright (c) 1980, 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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
#include "sh.h"

RCSID("$Id: ed.refresh.c,v 3.29 2002/03/08 17:36:45 christos Exp $")

#include "ed.h"
/* #define DEBUG_UPDATE */
/* #define DEBUG_REFRESH */
/* #define DEBUG_LITERAL */

/* refresh.c -- refresh the current set of lines on the screen */

Char   *litptr[256];
static int vcursor_h, vcursor_v;
static int rprompt_h, rprompt_v;

static	void	Draw 			__P((int));
static	void	Vdraw 			__P((int));
static	void	RefreshPromptpart	__P((Char *));
static	void	update_line 		__P((Char *, Char *, int));
static	void	str_insert		__P((Char *, int, int, Char *, int));
static	void	str_delete		__P((Char *, int, int, int));
static	void	str_cp			__P((Char *, Char *, int));
static	void	PutPlusOne		__P((int));
static	void	cpy_pad_spaces		__P((Char *, Char *, int));
#if defined(DSPMBYTE)
static	Char 	*update_line_fix_mbyte_point __P((Char *, Char *, int));
#endif
#if defined(DEBUG_UPDATE) || defined(DEBUG_REFRESH) || defined(DEBUG_LITERAL)
static	void	dprintf			__P((char *, ...));
#ifdef DEBUG_UPDATE
static	void	dprintstr		__P((char *, Char *, Char *));

static void
dprintstr(str, f, t)
char *str;
Char *f, *t;
{
    dprintf("%s:\"", str);
    while (f < t)
	dprintf("%c", *f++ & ASCII);
    dprintf("\"\r\n");
} 
#endif /* DEBUG_UPDATE */

/* dprintf():
 *	Print to $DEBUGTTY, so that we can test editing on one pty, and 
 *      print debugging stuff on another. Don't interrupt the shell while
 *	debugging cause you'll mangle up the file descriptors!
 */
static void
#ifdef FUNCPROTO
dprintf(char *fmt, ...)
#else
dprintf(va_list)
    va_dcl
#endif /* __STDC__ */
{
    static int fd = -1;
    char *dtty;

    if ((dtty = getenv("DEBUGTTY"))) {
	int o;
	va_list va;
#ifdef FUNCPROTO
	va_start(va, fmt);
#else
	char *fmt;
	va_start(va);
	fmt = va_arg(va, char *);
#endif /* __STDC__ */

	if (fd == -1)
	    fd = open(dtty, O_RDWR);
	o = SHOUT;
	flush();
	SHOUT = fd;
	xvprintf(fmt, va);
	va_end(va);
	flush();
	SHOUT = o;
    }
}
#endif  /* DEBUG_UPDATE || DEBUG_REFRESH || DEBUG_LITERAL */

static void
Draw(c)				/* draw c, expand tabs, ctl chars */
    register int c;
{
    register Char ch = c & CHAR;

    if (Isprint(ch)) {
	Vdraw(c);
	return;
    }
    /* from wolman%crltrx.DEC@decwrl.dec.com (Alec Wolman) */
    if (ch == '\n') {		/* expand the newline	 */
	/*
	 * Don't force a newline if Vdraw does it (i.e. we're at end of line)
	 * - or we will get two newlines and possibly garbage in between
	 */
	int oldv = vcursor_v;

	Vdraw('\0');		/* assure end of line	 */
	if (oldv == vcursor_v) {
	    vcursor_h = 0;	/* reset cursor pos	 */
	    vcursor_v++;
	}
	return;
    }
    if (ch == '\t') {		/* expand the tab 	 */
	for (;;) {
	    Vdraw(' ');
	    if ((vcursor_h & 07) == 0)
		break;		/* go until tab stop	 */
	}
    }
    else if (Iscntrl(ch)) {
#ifdef IS_ASCII
	Vdraw('^');
	if (ch == CTL_ESC('\177')) {
	    Vdraw('?');
	}
	else {
	    /* uncontrolify it; works only for iso8859-1 like sets */
	    Vdraw((c | 0100));
#else
	if (ch == CTL_ESC('\177')) {
	    Vdraw('^');
	    Vdraw('?');
	}
	else {
	    if (Isupper(_toebcdic[_toascii[c]|0100])
		|| strchr("@[\\]^_", _toebcdic[_toascii[c]|0100]) != NULL)
	    {
		Vdraw('^');
		Vdraw(_toebcdic[_toascii[c]|0100]);
	    }
	    else
	    {
		Vdraw('\\');
		Vdraw(((c >> 6) & 7) + '0');
		Vdraw(((c >> 3) & 7) + '0');
		Vdraw((c & 7) + '0');
	    }
#endif
	}
    }
#ifdef KANJI
    else if (
#ifdef DSPMBYTE
	     _enable_mbdisp &&
#endif
	     !adrof(STRnokanji)) {
	Vdraw(c);
	return;
    }
#endif
    else {
	Vdraw('\\');
	Vdraw(((c >> 6) & 7) + '0');
	Vdraw(((c >> 3) & 7) + '0');
	Vdraw((c & 7) + '0');
    }
}

static void
Vdraw(c)			/* draw char c onto V lines */
    register int c;
{
#ifdef DEBUG_REFRESH
# ifdef SHORT_STRINGS
    dprintf("Vdrawing %6.6o '%c'\r\n", c, c & ASCII);
# else
    dprintf("Vdrawing %3.3o '%c'\r\n", c, c);
# endif /* SHORT_STRNGS */
#endif  /* DEBUG_REFRESH */

    Vdisplay[vcursor_v][vcursor_h] = (Char) c;
    vcursor_h++;		/* advance to next place */
    if (vcursor_h >= TermH) {
	Vdisplay[vcursor_v][TermH] = '\0';	/* assure end of line */
	vcursor_h = 0;		/* reset it. */
	vcursor_v++;
#ifdef DEBUG_REFRESH
	if (vcursor_v >= TermV) {	/* should NEVER happen. */
	    dprintf("\r\nVdraw: vcursor_v overflow! Vcursor_v == %d > %d\r\n",
		    vcursor_v, TermV);
	    abort();
	}
#endif /* DEBUG_REFRESH */
    }
}

/*
 *  RefreshPromptpart()
 *	draws a prompt element, expanding literals (we know it's ASCIZ)
 */
static void
RefreshPromptpart(buf)
    Char *buf;
{
    register Char *cp;
    static unsigned int litnum = 0;
    if (buf == NULL)
    {
      litnum = 0;
      return;
    }

    for (cp = buf; *cp; cp++) {
	if (*cp & LITERAL) {
	    if (litnum < (sizeof(litptr) / sizeof(litptr[0]))) {
		litptr[litnum] = cp;
#ifdef DEBUG_LITERAL
		dprintf("litnum = %d, litptr = %x:\r\n",
			litnum, litptr[litnum]);
#endif /* DEBUG_LITERAL */
	    }
	    while (*cp & LITERAL)
		cp++;
	    if (*cp)
		Vdraw((int) (litnum++ | LITERAL));
	    else {
		/*
		 * XXX: This is a bug, we lose the last literal, if it is not
		 * followed by a normal character, but it is too hard to fix
		 */
		break;
	    }
	}
	else
	    Draw(*cp);
    }
}

/*
 *  Refresh()
 *	draws the new virtual screen image from the current input
 *  	line, then goes line-by-line changing the real image to the new
 *	virtual image. The routine to re-draw a line can be replaced
 *	easily in hopes of a smarter one being placed there.
 */
static int OldvcV = 0;
void
Refresh()
{
    register int cur_line;
    register Char *cp;
    int     cur_h, cur_v = 0, new_vcv;
    int     rhdiff;
    Char    oldgetting;

#ifdef DEBUG_REFRESH
    dprintf("PromptBuf = :%s:\r\n", short2str(PromptBuf));
    dprintf("InputBuf = :%s:\r\n", short2str(InputBuf));
#endif /* DEBUG_REFRESH */
    oldgetting = GettingInput;
    GettingInput = 0;		/* avoid re-entrance via SIGWINCH */

    /* reset the Vdraw cursor, temporarily draw rprompt to calculate its size */
    vcursor_h = 0;
    vcursor_v = 0;
    RefreshPromptpart(NULL);
    RefreshPromptpart(RPromptBuf);
    rprompt_h = vcursor_h;
    rprompt_v = vcursor_v;

    /* reset the Vdraw cursor, draw prompt */
    vcursor_h = 0;
    vcursor_v = 0;
    RefreshPromptpart(NULL);
    RefreshPromptpart(PromptBuf);
    cur_h = -1;			/* set flag in case I'm not set */

    /* draw the current input buffer */
    for (cp = InputBuf; (cp < LastChar); cp++) {
	if (cp == Cursor) {
	    cur_h = vcursor_h;	/* save for later */
	    cur_v = vcursor_v;
	}
	Draw(*cp);
    }

    if (cur_h == -1) {		/* if I haven't been set yet, I'm at the end */
	cur_h = vcursor_h;
	cur_v = vcursor_v;
    }

    rhdiff = TermH - vcursor_h - rprompt_h;
    if (rprompt_h != 0 && rprompt_v == 0 && vcursor_v == 0 && rhdiff > 1) {
			/*
			 * have a right-hand side prompt that will fit on
			 * the end of the first line with at least one
			 * character gap to the input buffer.
			 */
	while (--rhdiff > 0)		/* pad out with spaces */
	    Draw(' ');
	RefreshPromptpart(RPromptBuf);
    }
    else {
	rprompt_h = 0;			/* flag "not using rprompt" */
	rprompt_v = 0;
    }

    new_vcv = vcursor_v;	/* must be done BEFORE the NUL is written */
    Vdraw('\0');		/* put NUL on end */

#ifdef DEBUG_REFRESH
    dprintf("TermH=%d, vcur_h=%d, vcur_v=%d, Vdisplay[0]=\r\n:%80.80s:\r\n",
	    TermH, vcursor_h, vcursor_v, short2str(Vdisplay[0]));
#endif /* DEBUG_REFRESH */

#ifdef DEBUG_UPDATE
    dprintf("updating %d lines.\r\n", new_vcv);
#endif  /* DEBUG_UPDATE */
    for (cur_line = 0; cur_line <= new_vcv; cur_line++) {
	/* NOTE THAT update_line MAY CHANGE Display[cur_line] */
	update_line(Display[cur_line], Vdisplay[cur_line], cur_line);
#ifdef WINNT_NATIVE
	flush();
#endif /* WINNT_NATIVE */

	/*
	 * Copy the new line to be the current one, and pad out with spaces
	 * to the full width of the terminal so that if we try moving the
	 * cursor by writing the character that is at the end of the
	 * screen line, it won't be a NUL or some old leftover stuff.
	 */
	cpy_pad_spaces(Display[cur_line], Vdisplay[cur_line], TermH);
#ifdef notdef
	(void) Strncpy(Display[cur_line], Vdisplay[cur_line], (size_t) TermH);
	Display[cur_line][TermH] = '\0';	/* just in case */
#endif
    }
#ifdef DEBUG_REFRESH
    dprintf("\r\nvcursor_v = %d, OldvcV = %d, cur_line = %d\r\n",
	    vcursor_v, OldvcV, cur_line);
#endif /* DEBUG_REFRESH */
    if (OldvcV > new_vcv) {
	for (; cur_line <= OldvcV; cur_line++) {
	    update_line(Display[cur_line], STRNULL, cur_line);
	    *Display[cur_line] = '\0';
	}
    }
    OldvcV = new_vcv;		/* set for next time */
#ifdef DEBUG_REFRESH
    dprintf("\r\nCursorH = %d, CursorV = %d, cur_h = %d, cur_v = %d\r\n",
	    CursorH, CursorV, cur_h, cur_v);
#endif /* DEBUG_REFRESH */
#ifdef WINNT_NATIVE
    flush();
#endif /* WINNT_NATIVE */
    MoveToLine(cur_v);		/* go to where the cursor is */
    MoveToChar(cur_h);
    SetAttributes(0);		/* Clear all attributes */
    flush();			/* send the output... */
    GettingInput = oldgetting;	/* reset to old value */
}

#ifdef notdef
GotoBottom()
{				/* used to go to last used screen line */
    MoveToLine(OldvcV);
}

#endif 

void
PastBottom()
{				/* used to go to last used screen line */
    MoveToLine(OldvcV);
    (void) putraw('\r');
    (void) putraw('\n');
    ClearDisp();
    flush();
}


/* insert num characters of s into d (in front of the character) at dat,
   maximum length of d is dlen */
static void
str_insert(d, dat, dlen, s, num)
    register Char *d;
    register int dat, dlen;
    register Char *s;
    register int num;
{
    register Char *a, *b;

    if (num <= 0)
	return;
    if (num > dlen - dat)
	num = dlen - dat;

#ifdef DEBUG_REFRESH
    dprintf("str_insert() starting: %d at %d max %d, d == \"%s\"\n",
	    num, dat, dlen, short2str(d));
    dprintf("s == \"%s\"n", short2str(s));
#endif /* DEBUG_REFRESH */

    /* open up the space for num chars */
    if (num > 0) {
	b = d + dlen - 1;
	a = b - num;
	while (a >= &d[dat])
	    *b-- = *a--;
	d[dlen] = '\0';		/* just in case */
    }
#ifdef DEBUG_REFRESH
    dprintf("str_insert() after insert: %d at %d max %d, d == \"%s\"\n",
	    num, dat, dlen, short2str(d));
    dprintf("s == \"%s\"n", short2str(s));
#endif /* DEBUG_REFRESH */

    /* copy the characters */
    for (a = d + dat; (a < d + dlen) && (num > 0); num--)
	*a++ = *s++;

#ifdef DEBUG_REFRESH
    dprintf("str_insert() after copy: %d at %d max %d, d == \"%s\"\n",
	    num, dat, dlen, d, short2str(s));
    dprintf("s == \"%s\"n", short2str(s));
#endif /* DEBUG_REFRESH */
}

/* delete num characters d at dat, maximum length of d is dlen */
static void
str_delete(d, dat, dlen, num)
    register Char *d;
    register int dat, dlen, num;
{
    register Char *a, *b;

    if (num <= 0)
	return;
    if (dat + num >= dlen) {
	d[dat] = '\0';
	return;
    }

#ifdef DEBUG_REFRESH
    dprintf("str_delete() starting: %d at %d max %d, d == \"%s\"\n",
	    num, dat, dlen, short2str(d));
#endif /* DEBUG_REFRESH */

    /* open up the space for num chars */
    if (num > 0) {
	b = d + dat;
	a = b + num;
	while (a < &d[dlen])
	    *b++ = *a++;
	d[dlen] = '\0';		/* just in case */
    }
#ifdef DEBUG_REFRESH
    dprintf("str_delete() after delete: %d at %d max %d, d == \"%s\"\n",
	    num, dat, dlen, short2str(d));
#endif /* DEBUG_REFRESH */
}

static void
str_cp(a, b, n)
    register Char *a, *b;
    register int n;
{
    while (n-- && *b)
	*a++ = *b++;
}


#if defined(DSPMBYTE) /* BY TAGA Nayuta VERY THANKS */
static Char *
update_line_fix_mbyte_point(start, target, d)
     Char *start, *target;
     int d;
{
    if (_enable_mbdisp) {
	while (*start) {
	    if (target == start)
		break;
	    if (target < start)
		return target + d;
	    if (Ismbyte1(*start) && Ismbyte2(*(start + 1)))
		start++;
	    start++;
	}
    }
    return target;
}
#endif

/* ****************************************************************
    update_line() is based on finding the middle difference of each line
    on the screen; vis:

			     /old first difference
	/beginning of line   |              /old last same       /old EOL
	v		     v              v                    v
old:	eddie> Oh, my little gruntle-buggy is to me, as lurgid as
new:	eddie> Oh, my little buggy says to me, as lurgid as
	^		     ^        ^			   ^
	\beginning of line   |        \new last same	   \new end of line
			     \new first difference

    all are character pointers for the sake of speed.  Special cases for
    no differences, as well as for end of line additions must be handled.
**************************************************************** */

/* Minimum at which doing an insert it "worth it".  This should be about
 * half the "cost" of going into insert mode, inserting a character, and
 * going back out.  This should really be calculated from the termcap
 * data...  For the moment, a good number for ANSI terminals.
 */
#define MIN_END_KEEP	4

static void			/* could be changed to make it smarter */
update_line(old, new, cur_line)
    register Char *old, *new;
    int     cur_line;
{
    register Char *o, *n, *p, c;
    Char   *ofd, *ols, *oe, *nfd, *nls, *ne;
    Char   *osb, *ose, *nsb, *nse;
    int     fx, sx;

    /*
     * find first diff
     */
    for (o = old, n = new; *o && (*o == *n); o++, n++)
	continue;
    ofd = o;
    nfd = n;

    /*
     * Find the end of both old and new
     */
    while (*o)
	o++;
    /* 
     * Remove any trailing blanks off of the end, being careful not to
     * back up past the beginning.
     */
    while (ofd < o) {
	if (o[-1] != ' ')
	    break;
	o--;
    }
    oe = o;
    *oe = (Char) 0;
  
    while (*n)
	n++;

    /* remove blanks from end of new */
    while (nfd < n) {
	if (n[-1] != ' ')
	    break;
	n--;
    }
    ne = n;
    *ne = (Char) 0;
  
    /*
     * if no diff, continue to next line of redraw
     */
    if (*ofd == '\0' && *nfd == '\0') {
#ifdef DEBUG_UPDATE
	dprintf("no difference.\r\n");
#endif /* DEBUG_UPDATE */
	return;
    }

    /*
     * find last same pointer
     */
    while ((o > ofd) && (n > nfd) && (*--o == *--n))
	continue;
    ols = ++o;
    nls = ++n;

    /*
     * find same begining and same end
     */
    osb = ols;
    nsb = nls;
    ose = ols;
    nse = nls;

    /*
     * case 1: insert: scan from nfd to nls looking for *ofd
     */
    if (*ofd) {
	for (c = *ofd, n = nfd; n < nls; n++) {
	    if (c == *n) {
		for (o = ofd, p = n; p < nls && o < ols && *o == *p; o++, p++)
		    continue;
		/*
		 * if the new match is longer and it's worth keeping, then we
		 * take it
		 */
		if (((nse - nsb) < (p - n)) && (2 * (p - n) > n - nfd)) {
		    nsb = n;
		    nse = p;
		    osb = ofd;
		    ose = o;
		}
	    }
	}
    }

    /*
     * case 2: delete: scan from ofd to ols looking for *nfd
     */
    if (*nfd) {
	for (c = *nfd, o = ofd; o < ols; o++) {
	    if (c == *o) {
		for (n = nfd, p = o; p < ols && n < nls && *p == *n; p++, n++)
		    continue;
		/*
		 * if the new match is longer and it's worth keeping, then we
		 * take it
		 */
		if (((ose - osb) < (p - o)) && (2 * (p - o) > o - ofd)) {
		    nsb = nfd;
		    nse = n;
		    osb = o;
		    ose = p;
		}
	    }
	}
    }
#ifdef notdef
    /*
     * If `last same' is before `same end' re-adjust
     */
    if (ols < ose)
	ols = ose;
    if (nls < nse)
	nls = nse;
#endif

    /*
     * Pragmatics I: If old trailing whitespace or not enough characters to
     * save to be worth it, then don't save the last same info.
     */
    if ((oe - ols) < MIN_END_KEEP) {
	ols = oe;
	nls = ne;
    }

    /*
     * Pragmatics II: if the terminal isn't smart enough, make the data dumber
     * so the smart update doesn't try anything fancy
     */

    /*
     * fx is the number of characters we need to insert/delete: in the
     * beginning to bring the two same begins together
     */
    fx = (int) ((nsb - nfd) - (osb - ofd));
    /*
     * sx is the number of characters we need to insert/delete: in the end to
     * bring the two same last parts together
     */
    sx = (int) ((nls - nse) - (ols - ose));

    if (!T_CanIns) {
	if (fx > 0) {
	    osb = ols;
	    ose = ols;
	    nsb = nls;
	    nse = nls;
	}
	if (sx > 0) {
	    ols = oe;
	    nls = ne;
	}
	if ((ols - ofd) < (nls - nfd)) {
	    ols = oe;
	    nls = ne;
	}
    }
    if (!T_CanDel) {
	if (fx < 0) {
	    osb = ols;
	    ose = ols;
	    nsb = nls;
	    nse = nls;
	}
	if (sx < 0) {
	    ols = oe;
	    nls = ne;
	}
	if ((ols - ofd) > (nls - nfd)) {
	    ols = oe;
	    nls = ne;
	}
    }

    /*
     * Pragmatics III: make sure the middle shifted pointers are correct if
     * they don't point to anything (we may have moved ols or nls).
     */
    /* if the change isn't worth it, don't bother */
    /* was: if (osb == ose) */
    if ((ose - osb) < MIN_END_KEEP) {
	osb = ols;
	ose = ols;
	nsb = nls;
	nse = nls;
    }

    /*
     * Now that we are done with pragmatics we recompute fx, sx
     */
    fx = (int) ((nsb - nfd) - (osb - ofd));
    sx = (int) ((nls - nse) - (ols - ose));

#ifdef DEBUG_UPDATE
    dprintf("\n");
    dprintf("ofd %d, osb %d, ose %d, ols %d, oe %d\n",
	    ofd - old, osb - old, ose - old, ols - old, oe - old);
    dprintf("nfd %d, nsb %d, nse %d, nls %d, ne %d\n",
	    nfd - new, nsb - new, nse - new, nls - new, ne - new);
    dprintf("xxx-xxx:\"00000000001111111111222222222233333333334\"\r\n");
    dprintf("xxx-xxx:\"01234567890123456789012345678901234567890\"\r\n");
    dprintstr("old- oe", old, oe);
    dprintstr("new- ne", new, ne);
    dprintstr("old-ofd", old, ofd);
    dprintstr("new-nfd", new, nfd);
    dprintstr("ofd-osb", ofd, osb);
    dprintstr("nfd-nsb", nfd, nsb);
    dprintstr("osb-ose", osb, ose);
    dprintstr("nsb-nse", nsb, nse);
    dprintstr("ose-ols", ose, ols);
    dprintstr("nse-nls", nse, nls);
    dprintstr("ols- oe", ols, oe);
    dprintstr("nls- ne", nls, ne);
#endif /* DEBUG_UPDATE */

    /*
     * CursorV to this line cur_line MUST be in this routine so that if we
     * don't have to change the line, we don't move to it. CursorH to first
     * diff char
     */
    MoveToLine(cur_line);

#if defined(DSPMBYTE) /* BY TAGA Nayuta VERY THANKS */
    ofd = update_line_fix_mbyte_point(old, ofd, -1);
    osb = update_line_fix_mbyte_point(old, osb,  1);
    ose = update_line_fix_mbyte_point(old, ose, -1);
    ols = update_line_fix_mbyte_point(old, ols,  1);
    nfd = update_line_fix_mbyte_point(new, nfd, -1);
    nsb = update_line_fix_mbyte_point(new, nsb,  1);
    nse = update_line_fix_mbyte_point(new, nse, -1);
    nls = update_line_fix_mbyte_point(new, nls,  1);
#endif

    /*
     * at this point we have something like this:
     * 
     * /old                  /ofd    /osb               /ose    /ols     /oe
     * v.....................v       v..................v       v........v
     * eddie> Oh, my fredded gruntle-buggy is to me, as foo var lurgid as
     * eddie> Oh, my fredded quiux buggy is to me, as gruntle-lurgid as
     * ^.....................^     ^..................^       ^........^ 
     * \new                  \nfd  \nsb               \nse     \nls    \ne
     * 
     * fx is the difference in length between the the chars between nfd and
     * nsb, and the chars between ofd and osb, and is thus the number of
     * characters to delete if < 0 (new is shorter than old, as above),
     * or insert (new is longer than short).
     *
     * sx is the same for the second differences.
     */

    /*
     * if we have a net insert on the first difference, AND inserting the net
     * amount ((nsb-nfd) - (osb-ofd)) won't push the last useful character
     * (which is ne if nls != ne, otherwise is nse) off the edge of the screen
     * (TermH - 1) else we do the deletes first so that we keep everything we
     * need to.
     */

    /*
     * if the last same is the same like the end, there is no last same part,
     * otherwise we want to keep the last same part set p to the last useful
     * old character
     */
    p = (ols != oe) ? oe : ose;

    /*
     * if (There is a diffence in the beginning) && (we need to insert
     * characters) && (the number of characters to insert is less than the term
     * width) We need to do an insert! else if (we need to delete characters)
     * We need to delete characters! else No insert or delete
     */
    if ((nsb != nfd) && fx > 0 && ((p - old) + fx < TermH)) {
#ifdef DEBUG_UPDATE
	dprintf("first diff insert at %d...\r\n", nfd - new);
#endif  /* DEBUG_UPDATE */
	/*
	 * Move to the first char to insert, where the first diff is.
	 */
	MoveToChar(nfd - new);
	/*
	 * Check if we have stuff to keep at end
	 */
	if (nsb != ne) {
#ifdef DEBUG_UPDATE
	    dprintf("with stuff to keep at end\r\n");
#endif  /* DEBUG_UPDATE */
	    /*
	     * insert fx chars of new starting at nfd
	     */
	    if (fx > 0) {
#ifdef DEBUG_UPDATE
		if (!T_CanIns)
		    dprintf("   ERROR: cannot insert in early first diff\n");
#endif  /* DEBUG_UPDATE */
		Insert_write(nfd, fx);
		str_insert(old, (int) (ofd - old), TermH, nfd, fx);
	    }
	    /*
	     * write (nsb-nfd) - fx chars of new starting at (nfd + fx)
	     */
	    so_write(nfd + fx, (nsb - nfd) - fx);
	    str_cp(ofd + fx, nfd + fx, (int) ((nsb - nfd) - fx));
	}
	else {
#ifdef DEBUG_UPDATE
	    dprintf("without anything to save\r\n");
#endif  /* DEBUG_UPDATE */
	    so_write(nfd, (nsb - nfd));
	    str_cp(ofd, nfd, (int) (nsb - nfd));
	    /*
	     * Done
	     */
	    return;
	}
    }
    else if (fx < 0) {
#ifdef DEBUG_UPDATE
	dprintf("first diff delete at %d...\r\n", ofd - old);
#endif  /* DEBUG_UPDATE */
	/*
	 * move to the first char to delete where the first diff is
	 */
	MoveToChar(ofd - old);
	/*
	 * Check if we have stuff to save
	 */
	if (osb != oe) {
#ifdef DEBUG_UPDATE
	    dprintf("with stuff to save at end\r\n");
#endif  /* DEBUG_UPDATE */
	    /*
	     * fx is less than zero *always* here but we check for code
	     * symmetry
	     */
	    if (fx < 0) {
#ifdef DEBUG_UPDATE
		if (!T_CanDel)
		    dprintf("   ERROR: cannot delete in first diff\n");
#endif /* DEBUG_UPDATE */
		DeleteChars(-fx);
		str_delete(old, (int) (ofd - old), TermH, -fx);
	    }
	    /*
	     * write (nsb-nfd) chars of new starting at nfd
	     */
	    so_write(nfd, (nsb - nfd));
	    str_cp(ofd, nfd, (int) (nsb - nfd));

	}
	else {
#ifdef DEBUG_UPDATE
	    dprintf("but with nothing left to save\r\n");
#endif  /* DEBUG_UPDATE */
	    /*
	     * write (nsb-nfd) chars of new starting at nfd
	     */
	    so_write(nfd, (nsb - nfd));
#ifdef DEBUG_REFRESH
	    dprintf("cleareol %d\n", (oe - old) - (ne - new));
#endif  /* DEBUG_UPDATE */
#ifndef WINNT_NATIVE
	    ClearEOL((oe - old) - (ne - new));
#else
	    /*
	     * The calculation above does not work too well on NT
	     */
	    ClearEOL(TermH - CursorH);
#endif /*WINNT_NATIVE*/
	    /*
	     * Done
	     */
	    return;
	}
    }
    else
	fx = 0;

    if (sx < 0) {
#ifdef DEBUG_UPDATE
	dprintf("second diff delete at %d...\r\n", (ose - old) + fx);
#endif  /* DEBUG_UPDATE */
	/*
	 * Check if we have stuff to delete
	 */
	/*
	 * fx is the number of characters inserted (+) or deleted (-)
	 */

	MoveToChar((ose - old) + fx);
	/*
	 * Check if we have stuff to save
	 */
	if (ols != oe) {
#ifdef DEBUG_UPDATE
	    dprintf("with stuff to save at end\r\n");
#endif  /* DEBUG_UPDATE */
	    /*
	     * Again a duplicate test.
	     */
	    if (sx < 0) {
#ifdef DEBUG_UPDATE
		if (!T_CanDel)
		    dprintf("   ERROR: cannot delete in second diff\n");
#endif  /* DEBUG_UPDATE */
		DeleteChars(-sx);
	    }

	    /*
	     * write (nls-nse) chars of new starting at nse
	     */
	    so_write(nse, (nls - nse));
	}
	else {
	    int olen = (int) (oe - old + fx);
	    if (olen > TermH)
		olen = TermH;
#ifdef DEBUG_UPDATE
	    dprintf("but with nothing left to save\r\n");
#endif /* DEBUG_UPDATE */
	    so_write(nse, (nls - nse));
#ifdef DEBUG_REFRESH
	    dprintf("cleareol %d\n", olen - (ne - new));
#endif /* DEBUG_UPDATE */
#ifndef WINNT_NATIVE
	    ClearEOL(olen - (ne - new));
#else
	    /*
	     * The calculation above does not work too well on NT
	     */
	    ClearEOL(TermH - CursorH);
#endif /*WINNT_NATIVE*/
	}
    }

    /*
     * if we have a first insert AND WE HAVEN'T ALREADY DONE IT...
     */
    if ((nsb != nfd) && (osb - ofd) <= (nsb - nfd) && (fx == 0)) {
#ifdef DEBUG_UPDATE
	dprintf("late first diff insert at %d...\r\n", nfd - new);
#endif /* DEBUG_UPDATE */

	MoveToChar(nfd - new);
	/*
	 * Check if we have stuff to keep at the end
	 */
	if (nsb != ne) {
#ifdef DEBUG_UPDATE
	    dprintf("with stuff to keep at end\r\n");
#endif /* DEBUG_UPDATE */
	    /* 
	     * We have to recalculate fx here because we set it
	     * to zero above as a flag saying that we hadn't done
	     * an early first insert.
	     */
	    fx = (int) ((nsb - nfd) - (osb - ofd));
	    if (fx > 0) {
		/*
		 * insert fx chars of new starting at nfd
		 */
#ifdef DEBUG_UPDATE
		if (!T_CanIns)
		    dprintf("   ERROR: cannot insert in late first diff\n");
#endif /* DEBUG_UPDATE */
		Insert_write(nfd, fx);
		str_insert(old, (int) (ofd - old), TermH, nfd, fx);
	    }

	    /*
	     * write (nsb-nfd) - fx chars of new starting at (nfd + fx)
	     */
	    so_write(nfd + fx, (nsb - nfd) - fx);
	    str_cp(ofd + fx, nfd + fx, (int) ((nsb - nfd) - fx));
	}
	else {
#ifdef DEBUG_UPDATE
	    dprintf("without anything to save\r\n");
#endif /* DEBUG_UPDATE */
	    so_write(nfd, (nsb - nfd));
	    str_cp(ofd, nfd, (int) (nsb - nfd));
	}
    }

    /*
     * line is now NEW up to nse
     */
    if (sx >= 0) {
#ifdef DEBUG_UPDATE
	dprintf("second diff insert at %d...\r\n", nse - new);
#endif /* DEBUG_UPDATE */
	MoveToChar(nse - new);
	if (ols != oe) {
#ifdef DEBUG_UPDATE
	    dprintf("with stuff to keep at end\r\n");
#endif /* DEBUG_UPDATE */
	    if (sx > 0) {
		/* insert sx chars of new starting at nse */
#ifdef DEBUG_UPDATE
		if (!T_CanIns)
		    dprintf("   ERROR: cannot insert in second diff\n");
#endif /* DEBUG_UPDATE */
		Insert_write(nse, sx);
	    }

	    /*
	     * write (nls-nse) - sx chars of new starting at (nse + sx)
	     */
	    so_write(nse + sx, (nls - nse) - sx);
	}
	else {
#ifdef DEBUG_UPDATE
	    dprintf("without anything to save\r\n");
#endif /* DEBUG_UPDATE */
	    so_write(nse, (nls - nse));

	    /*
             * No need to do a clear-to-end here because we were doing
	     * a second insert, so we will have over written all of the
	     * old string.
	     */
	}
    }
#ifdef DEBUG_UPDATE
    dprintf("done.\r\n");
#endif /* DEBUG_UPDATE */
}


static void
cpy_pad_spaces(dst, src, width)
    register Char *dst, *src;
    register int width;
{
    register int i;

    for (i = 0; i < width; i++) {
	if (*src == (Char) 0)
	    break;
	*dst++ = *src++;
    }

    while (i < width) {
	*dst++ = ' ';
	i++;
    }
    *dst = (Char) 0;
}

void
RefCursor()
{				/* only move to new cursor pos */
    register Char *cp, c;
    register int h, th, v;

    /* first we must find where the cursor is... */
    h = 0;
    v = 0;
    th = TermH;			/* optimize for speed */

    for (cp = PromptBuf; *cp; cp++) {	/* do prompt */
	if (*cp & LITERAL)
	    continue;
	c = *cp & CHAR;		/* extra speed plus strip the inverse */
	h++;			/* all chars at least this long */

	/* from wolman%crltrx.DEC@decwrl.dec.com (Alec Wolman) */
	/* lets handle newline as part of the prompt */

	if (c == '\n') {
	    h = 0;
	    v++;
	}
	else {
	    if (c == '\t') {	/* if a tab, to next tab stop */
		while (h & 07) {
		    h++;
		}
	    }
	    else if (Iscntrl(c)) {	/* if control char */
		h++;
		if (h > th) {	/* if overflow, compensate */
		    h = 1;
		    v++;
		}
	    }
	    else if (!Isprint(c)) {
		h += 3;
		if (h > th) {	/* if overflow, compensate */
		    h = h - th;
		    v++;
		}
	    }
	}

	if (h >= th) {		/* check, extra long tabs picked up here also */
	    h = 0;
	    v++;
	}
    }

    for (cp = InputBuf; cp < Cursor; cp++) {	/* do input buffer to Cursor */
	c = *cp & CHAR;		/* extra speed plus strip the inverse */
	h++;			/* all chars at least this long */

	if (c == '\n') {	/* handle newline in data part too */
	    h = 0;
	    v++;
	}
	else {
	    if (c == '\t') {	/* if a tab, to next tab stop */
		while (h & 07) {
		    h++;
		}
	    }
	    else if (Iscntrl(c)) {	/* if control char */
		h++;
		if (h > th) {	/* if overflow, compensate */
		    h = 1;
		    v++;
		}
	    }
	    else if (!Isprint(c)) {
		h += 3;
		if (h > th) {	/* if overflow, compensate */
		    h = h - th;
		    v++;
		}
	    }
	}

	if (h >= th) {		/* check, extra long tabs picked up here also */
	    h = 0;
	    v++;
	}
    }

    /* now go there */
    MoveToLine(v);
    MoveToChar(h);
    flush();
}

static void
PutPlusOne(c)
    int    c;
{
    (void) putraw(c);
    Display[CursorV][CursorH++] = (Char) c;
    if (CursorH >= TermH) {	/* if we must overflow */
	CursorH = 0;
	CursorV++;
	OldvcV++;
	if (T_Margin & MARGIN_AUTO) {
	    if (T_Margin & MARGIN_MAGIC) {
		(void) putraw(' ');
		(void) putraw('\b');
	    }
	}
	else {
	    (void) putraw('\r');
	    (void) putraw('\n');
	}
    }
}

void
RefPlusOne()
{				/* we added just one char, handle it fast.
				 * assumes that screen cursor == real cursor */
    register Char c, mc;

    c = Cursor[-1] & CHAR;	/* the char we just added */

    if (c == '\t' || Cursor != LastChar) {
	Refresh();		/* too hard to handle */
	return;
    }

    if (rprompt_h != 0 && (TermH - CursorH - rprompt_h < 3)) {
	Refresh();		/* clear out rprompt if less than one char gap*/
	return;
    }				/* else (only do at end of line, no TAB) */

    if (Iscntrl(c)) {		/* if control char, do caret */
#ifdef IS_ASCII
	mc = (c == '\177') ? '?' : (c | 0100);
	PutPlusOne('^');
	PutPlusOne(mc);
#else
	if (_toascii[c] == '\177' || Isupper(_toebcdic[_toascii[c]|0100])
		|| strchr("@[\\]^_", _toebcdic[_toascii[c]|0100]) != NULL)
	{
	    mc = (_toascii[c] == '\177') ? '?' : _toebcdic[_toascii[c]|0100];
	    PutPlusOne('^');
	    PutPlusOne(mc);
	}
	else
	{
	    PutPlusOne('\\');
	    PutPlusOne(((c >> 6) & 7) + '0');
	    PutPlusOne(((c >> 3) & 7) + '0');
	    PutPlusOne((c & 7) + '0');
	}
#endif
    }
    else if (Isprint(c)) {	/* normal char */
	PutPlusOne(c);
    }
#ifdef KANJI
    else if (
#ifdef DSPMBYTE
	     _enable_mbdisp &&
#endif
	     !adrof(STRnokanji)) {
	PutPlusOne(c);
    }
#endif
    else {
	PutPlusOne('\\');
	PutPlusOne(((c >> 6) & 7) + '0');
	PutPlusOne(((c >> 3) & 7) + '0');
	PutPlusOne((c & 7) + '0');
    }
    flush();
}

/* clear the screen buffers so that new new prompt starts fresh. */

void
ClearDisp()
{
    register int i;

    CursorV = 0;		/* clear the display buffer */
    CursorH = 0;
    for (i = 0; i < TermV; i++)
	(void) memset(Display[i], 0, TermH * sizeof(Display[0][0]));
    OldvcV = 0;
}

void
ClearLines()
{				/* Make sure all lines are *really* blank */
    register int i;

    if (T_CanCEOL) {
	/*
	 * Clear the lines from the bottom up so that if we try moving
	 * the cursor down by writing the character that is at the end
	 * of the screen line, we won't rewrite a character that shouldn't
	 * be there.
	 */
	for (i = OldvcV; i >= 0; i--) {	/* for each line on the screen */
	    MoveToLine(i);
	    MoveToChar(0);
	    ClearEOL(TermH);
	}
    }
    else {
	MoveToLine(OldvcV);	/* go to last line */
	(void) putraw('\r');	/* go to BOL */
	(void) putraw('\n');	/* go to new line */
    }
}
