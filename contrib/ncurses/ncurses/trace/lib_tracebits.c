/****************************************************************************
 * Copyright (c) 1998,1999,2000 Free Software Foundation, Inc.              *
 *                                                                          *
 * Permission is hereby granted, free of charge, to any person obtaining a  *
 * copy of this software and associated documentation files (the            *
 * "Software"), to deal in the Software without restriction, including      *
 * without limitation the rights to use, copy, modify, merge, publish,      *
 * distribute, distribute with modifications, sublicense, and/or sell       *
 * copies of the Software, and to permit persons to whom the Software is    *
 * furnished to do so, subject to the following conditions:                 *
 *                                                                          *
 * The above copyright notice and this permission notice shall be included  *
 * in all copies or substantial portions of the Software.                   *
 *                                                                          *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS  *
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF               *
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.   *
 * IN NO EVENT SHALL THE ABOVE COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,   *
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR    *
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR    *
 * THE USE OR OTHER DEALINGS IN THE SOFTWARE.                               *
 *                                                                          *
 * Except as contained in this notice, the name(s) of the above copyright   *
 * holders shall not be used in advertising or otherwise to promote the     *
 * sale, use or other dealings in this Software without prior written       *
 * authorization.                                                           *
 ****************************************************************************/

/****************************************************************************
 *  Author: Zeyd M. Ben-Halim <zmbenhal@netcom.com> 1992,1995               *
 *     and: Eric S. Raymond <esr@snark.thyrsus.com>                         *
 ****************************************************************************/

#include <curses.priv.h>
#include <term.h>		/* cur_term */

MODULE_ID("$Id: lib_tracebits.c,v 1.5 2000/02/13 01:01:55 tom Exp $")

#if defined(SVR4_TERMIO) && !defined(_POSIX_SOURCE)
#define _POSIX_SOURCE
#endif

#if HAVE_SYS_TERMIO_H
#include <sys/termio.h>		/* needed for ISC */
#endif

#ifdef __EMX__
#include <io.h>
#endif

/* may be undefined if we're using termio.h */
#ifndef TOSTOP
#define TOSTOP 0
#endif
#ifndef IEXTEN
#define IEXTEN 0
#endif

#ifdef TRACE

typedef struct {
    unsigned int val;
    const char *name;
} BITNAMES;

static void
lookup_bits(char *buf, const BITNAMES * table, const char *label, unsigned int val)
{
    const BITNAMES *sp;

    (void) strcat(buf, label);
    (void) strcat(buf, ": {");
    for (sp = table; sp->name; sp++)
	if (sp->val != 0
	    && (val & sp->val) == sp->val) {
	    (void) strcat(buf, sp->name);
	    (void) strcat(buf, ", ");
	}
    if (buf[strlen(buf) - 2] == ',')
	buf[strlen(buf) - 2] = '\0';
    (void) strcat(buf, "} ");
}

char *
_nc_tracebits(void)
/* describe the state of the terminal control bits exactly */
{
    char *buf;

#ifdef TERMIOS
    static const BITNAMES iflags[] =
    {
	{BRKINT, "BRKINT"},
	{IGNBRK, "IGNBRK"},
	{IGNPAR, "IGNPAR"},
	{PARMRK, "PARMRK"},
	{INPCK, "INPCK"},
	{ISTRIP, "ISTRIP"},
	{INLCR, "INLCR"},
	{IGNCR, "IGNC"},
	{ICRNL, "ICRNL"},
	{IXON, "IXON"},
	{IXOFF, "IXOFF"},
	{0, NULL}
#define ALLIN	(BRKINT|IGNBRK|IGNPAR|PARMRK|INPCK|ISTRIP|INLCR|IGNCR|ICRNL|IXON|IXOFF)
    }, oflags[] =
    {
	{OPOST, "OPOST"},
	{0, NULL}
#define ALLOUT	(OPOST)
    }, cflags[] =
    {
	{CLOCAL, "CLOCAL"},
	{CREAD, "CREAD"},
	{CSTOPB, "CSTOPB"},
#if !defined(CS5) || !defined(CS8)
	{CSIZE, "CSIZE"},
#endif
	{HUPCL, "HUPCL"},
	{PARENB, "PARENB"},
	{PARODD | PARENB, "PARODD"},	/* concession to readability */
	{0, NULL}
#define ALLCTRL	(CLOCAL|CREAD|CSIZE|CSTOPB|HUPCL|PARENB|PARODD)
    }, lflags[] =
    {
	{ECHO, "ECHO"},
	{ECHOE | ECHO, "ECHOE"},	/* concession to readability */
	{ECHOK | ECHO, "ECHOK"},	/* concession to readability */
	{ECHONL, "ECHONL"},
	{ICANON, "ICANON"},
	{ISIG, "ISIG"},
	{NOFLSH, "NOFLSH"},
	{TOSTOP, "TOSTOP"},
	{IEXTEN, "IEXTEN"},
	{0, NULL}
#define ALLLOCAL	(ECHO|ECHONL|ICANON|ISIG|NOFLSH|TOSTOP|IEXTEN)
    };

    buf = _nc_trace_buf(0,
	8 + sizeof(iflags) +
	8 + sizeof(oflags) +
	8 + sizeof(cflags) +
	8 + sizeof(lflags) +
	8);

    if (cur_term->Nttyb.c_iflag & ALLIN)
	lookup_bits(buf, iflags, "iflags", cur_term->Nttyb.c_iflag);

    if (cur_term->Nttyb.c_oflag & ALLOUT)
	lookup_bits(buf, oflags, "oflags", cur_term->Nttyb.c_oflag);

    if (cur_term->Nttyb.c_cflag & ALLCTRL)
	lookup_bits(buf, cflags, "cflags", cur_term->Nttyb.c_cflag);

#if defined(CS5) && defined(CS8)
    switch (cur_term->Nttyb.c_cflag & CSIZE) {
#if defined(CS5) && (CS5 != 0)
    case CS5:
	strcat(buf, "CS5 ");
	break;
#endif
#if defined(CS6) && (CS6 != 0)
    case CS6:
	strcat(buf, "CS6 ");
	break;
#endif
#if defined(CS7) && (CS7 != 0)
    case CS7:
	strcat(buf, "CS7 ");
	break;
#endif
#if defined(CS8) && (CS8 != 0)
    case CS8:
	strcat(buf, "CS8 ");
	break;
#endif
    default:
	strcat(buf, "CSIZE? ");
	break;
    }
#endif

    if (cur_term->Nttyb.c_lflag & ALLLOCAL)
	lookup_bits(buf, lflags, "lflags", cur_term->Nttyb.c_lflag);

#else
    /* reference: ttcompat(4M) on SunOS 4.1 */
#ifndef EVENP
#define EVENP 0
#endif
#ifndef LCASE
#define LCASE 0
#endif
#ifndef LLITOUT
#define LLITOUT 0
#endif
#ifndef ODDP
#define ODDP 0
#endif
#ifndef TANDEM
#define TANDEM 0
#endif

    static const BITNAMES cflags[] =
    {
	{CBREAK, "CBREAK"},
	{CRMOD, "CRMOD"},
	{ECHO, "ECHO"},
	{EVENP, "EVENP"},
	{LCASE, "LCASE"},
	{LLITOUT, "LLITOUT"},
	{ODDP, "ODDP"},
	{RAW, "RAW"},
	{TANDEM, "TANDEM"},
	{XTABS, "XTABS"},
	{0, NULL}
#define ALLCTRL	(CBREAK|CRMOD|ECHO|EVENP|LCASE|LLITOUT|ODDP|RAW|TANDEM|XTABS)
    };

    buf = _nc_trace_buf(0,
	8 + sizeof(cflags));

    if (cur_term->Nttyb.sg_flags & ALLCTRL) {
	lookup_bits(buf, cflags, "cflags", cur_term->Nttyb.sg_flags);
    }
#endif
    return (buf);
}
#else
char *
_nc_tracebits(void)
{
    static char tmp[] = "";
    return tmp;
}
#endif /* TRACE */
