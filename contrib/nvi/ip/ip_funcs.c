/*-
 * Copyright (c) 1996
 *	Keith Bostic.  All rights reserved.
 *
 * See the LICENSE file for redistribution information.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "@(#)ip_funcs.c	8.4 (Berkeley) 10/13/96";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <stdio.h>

#include "../common/common.h"
#include "../vi/vi.h"
#include "ip.h"

static int ip_send __P((SCR *, char *, IP_BUF *));

/*
 * ip_addstr --
 *	Add len bytes from the string at the cursor, advancing the cursor.
 *
 * PUBLIC: int ip_addstr __P((SCR *, const char *, size_t));
 */
int
ip_addstr(sp, str, len)
	SCR *sp;
	const char *str;
	size_t len;
{
	IP_BUF ipb;
	IP_PRIVATE *ipp;
	int iv, rval;

	ipp = IPP(sp);

	/*
	 * If ex isn't in control, it's the last line of the screen and
	 * it's a split screen, use inverse video.
	 */
	iv = 0;
	if (!F_ISSET(sp, SC_SCR_EXWROTE) &&
	    ipp->row == LASTLINE(sp) && IS_SPLIT(sp)) {
		iv = 1;
		ip_attr(sp, SA_INVERSE, 1);
	}
	ipb.code = IPO_ADDSTR;
	ipb.len = len;
	ipb.str = str;
	rval = ip_send(sp, "s", &ipb);

	if (iv)
		ip_attr(sp, SA_INVERSE, 0);
	return (rval);
}

/*
 * ip_attr --
 *	Toggle a screen attribute on/off.
 *
 * PUBLIC: int ip_attr __P((SCR *, scr_attr_t, int));
 */
int
ip_attr(sp, attribute, on)
	SCR *sp;
	scr_attr_t attribute;
	int on;
{
	IP_BUF ipb;

	ipb.code = IPO_ATTRIBUTE;
	ipb.val1 = attribute;
	ipb.val2 = on;

	return (ip_send(sp, "12", &ipb));
}

/*
 * ip_baud --
 *	Return the baud rate.
 *
 * PUBLIC: int ip_baud __P((SCR *, u_long *));
 */
int
ip_baud(sp, ratep)
	SCR *sp;
	u_long *ratep;
{
	*ratep = 9600;		/* XXX: Translation: fast. */
	return (0);
}

/*
 * ip_bell --
 *	Ring the bell/flash the screen.
 *
 * PUBLIC: int ip_bell __P((SCR *));
 */
int
ip_bell(sp)
	SCR *sp;
{
	IP_BUF ipb;

	ipb.code = IPO_BELL;

	return (ip_send(sp, NULL, &ipb));
}

/*
 * ip_busy --
 *	Display a busy message.
 *
 * PUBLIC: void ip_busy __P((SCR *, const char *, busy_t));
 */
void
ip_busy(sp, str, bval)
	SCR *sp;
	const char *str;
	busy_t bval;
{
	IP_BUF ipb;

	ipb.code = IPO_BUSY;
	if (str == NULL) {
		ipb.len = 0;
		ipb.str = "";
	} else {
		ipb.len = strlen(str);
		ipb.str = str;
	}
	ipb.val1 = bval;

	(void)ip_send(sp, "s1", &ipb);
}

/*
 * ip_clrtoeol --
 *	Clear from the current cursor to the end of the line.
 *
 * PUBLIC: int ip_clrtoeol __P((SCR *));
 */
int
ip_clrtoeol(sp)
	SCR *sp;
{
	IP_BUF ipb;

	ipb.code = IPO_CLRTOEOL;

	return (ip_send(sp, NULL, &ipb));
}

/*
 * ip_cursor --
 *	Return the current cursor position.
 *
 * PUBLIC: int ip_cursor __P((SCR *, size_t *, size_t *));
 */
int
ip_cursor(sp, yp, xp)
	SCR *sp;
	size_t *yp, *xp;
{
	IP_PRIVATE *ipp;

	ipp = IPP(sp);
	*yp = ipp->row;
	*xp = ipp->col;
	return (0);
}

/*
 * ip_deleteln --
 *	Delete the current line, scrolling all lines below it.
 *
 * PUBLIC: int ip_deleteln __P((SCR *));
 */
int
ip_deleteln(sp)
	SCR *sp;
{
	IP_BUF ipb;

	/*
	 * This clause is required because the curses screen uses reverse
	 * video to delimit split screens.  If the screen does not do this,
	 * this code won't be necessary.
	 *
	 * If the bottom line was in reverse video, rewrite it in normal
	 * video before it's scrolled.
	 */
	if (!F_ISSET(sp, SC_SCR_EXWROTE) && IS_SPLIT(sp)) {
		ipb.code = IPO_REWRITE;
		ipb.val1 = RLNO(sp, LASTLINE(sp));
		if (ip_send(sp, "1", &ipb))
			return (1);
	}

	/*
	 * The bottom line is expected to be blank after this operation,
	 * and other screens must support that semantic.
	 */
	ipb.code = IPO_DELETELN;
	return (ip_send(sp, NULL, &ipb));
}

/* 
 * ip_ex_adjust --
 *	Adjust the screen for ex.
 *
 * PUBLIC: int ip_ex_adjust __P((SCR *, exadj_t));
 */
int
ip_ex_adjust(sp, action)
	SCR *sp;
	exadj_t action;
{
	abort();
	/* NOTREACHED */
}

/*
 * ip_insertln --
 *	Push down the current line, discarding the bottom line.
 *
 * PUBLIC: int ip_insertln __P((SCR *));
 */
int
ip_insertln(sp)
	SCR *sp;
{
	IP_BUF ipb;

	ipb.code = IPO_INSERTLN;

	return (ip_send(sp, NULL, &ipb));
}

/*
 * ip_keyval --
 *	Return the value for a special key.
 *
 * PUBLIC: int ip_keyval __P((SCR *, scr_keyval_t, CHAR_T *, int *));
 */
int
ip_keyval(sp, val, chp, dnep)
	SCR *sp;
	scr_keyval_t val;
	CHAR_T *chp;
	int *dnep;
{
	/*
	 * VEOF, VERASE and VKILL are required by POSIX 1003.1-1990,
	 * VWERASE is a 4BSD extension.
	 */
	switch (val) {
	case KEY_VEOF:
		*dnep = '\004';		/* ^D */
		break;
	case KEY_VERASE:
		*dnep = '\b';		/* ^H */
		break;
	case KEY_VKILL:
		*dnep = '\025';		/* ^U */
		break;
#ifdef VWERASE
	case KEY_VWERASE:
		*dnep = '\027';		/* ^W */
		break;
#endif
	default:
		*dnep = 1;
		break;
	}
	return (0);
}

/*
 * ip_move --
 *	Move the cursor.
 *
 * PUBLIC: int ip_move __P((SCR *, size_t, size_t));
 */
int
ip_move(sp, lno, cno)
	SCR *sp;
	size_t lno, cno;
{
	IP_PRIVATE *ipp;
	IP_BUF ipb;

	ipp = IPP(sp);
	ipp->row = lno;
	ipp->col = cno;

	ipb.code = IPO_MOVE;
	ipb.val1 = RLNO(sp, lno);
	ipb.val2 = cno;
	return (ip_send(sp, "12", &ipb));
}

/*
 * ip_refresh --
 *	Refresh the screen.
 *
 * PUBLIC: int ip_refresh __P((SCR *, int));
 */
int
ip_refresh(sp, repaint)
	SCR *sp;
	int repaint;
{
	IP_BUF ipb;

	ipb.code = repaint ? IPO_REDRAW : IPO_REFRESH;

	return (ip_send(sp, NULL, &ipb));
}

/*
 * ip_rename --
 *	Rename the file.
 *
 * PUBLIC: int ip_rename __P((SCR *));
 */
int
ip_rename(sp)
	SCR *sp;
{
	IP_BUF ipb;

	ipb.code = IPO_RENAME;
	ipb.len = strlen(sp->frp->name);
	ipb.str = sp->frp->name;

	return (ip_send(sp, "s", &ipb));
}

/*
 * ip_suspend --
 *	Suspend a screen.
 *
 * PUBLIC: int ip_suspend __P((SCR *, int *));
 */
int
ip_suspend(sp, allowedp)
	SCR *sp;
	int *allowedp;
{
	*allowedp = 0;
	return (0);
}

/*      
 * ip_usage --
 *      Print out the ip usage messages.
 *
 * PUBLIC: void ip_usage __P((void));
 */
void    
ip_usage()
{       
#define USAGE "\
usage: vi [-eFlRrSv] [-c command] [-I ifd.ofd] [-t tag] [-w size] [file ...]\n"
        (void)fprintf(stderr, "%s", USAGE);
#undef  USAGE
}

/*
 * ip_send --
 *	Construct and send an IP buffer.
 */
static int
ip_send(sp, fmt, ipbp)
	SCR *sp;
	char *fmt;
	IP_BUF *ipbp;
{
	IP_PRIVATE *ipp;
	size_t blen, off;
	u_int32_t ilen;
	int nlen, n, nw, rval;
	char *bp, *p;
	
	ipp = IPP(sp);

	GET_SPACE_RET(sp, bp, blen, 128);

	p = bp;
	nlen = 0;
	*p++ = ipbp->code;
	nlen += IPO_CODE_LEN;

	if (fmt != NULL)
		for (; *fmt != '\0'; ++fmt)
			switch (*fmt) {
			case '1':			/* Value 1. */
				ilen = htonl(ipbp->val1);
				goto value;
			case '2':			/* Value 2. */
				ilen = htonl(ipbp->val2);
value:				nlen += IPO_INT_LEN;
				off = p - bp;
				ADD_SPACE_RET(sp, bp, blen, nlen);
				p = bp + off;
				memmove(p, &ilen, IPO_INT_LEN);
				p += IPO_INT_LEN;
				break;
			case 's':			/* String. */
				ilen = ipbp->len;	/* XXX: conversion. */
				ilen = htonl(ilen);
				nlen += IPO_INT_LEN + ipbp->len;
				off = p - bp;
				ADD_SPACE_RET(sp, bp, blen, nlen);
				p = bp + off;
				memmove(p, &ilen, IPO_INT_LEN);
				p += IPO_INT_LEN;
				memmove(p, ipbp->str, ipbp->len);
				p += ipbp->len;
				break;
			}


	rval = 0;
	for (n = p - bp, p = bp; n > 0; n -= nw, p += nw)
		if ((nw = write(ipp->o_fd, p, n)) < 0) {
			rval = 1;
			break;
		}

	FREE_SPACE(sp, bp, blen);

	return (rval);
}
