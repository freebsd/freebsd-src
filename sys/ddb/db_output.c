/* 
 * Mach Operating System
 * Copyright (c) 1991,1990 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 * 
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 * 
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 *
 *	$Id: db_output.c,v 1.5 1993/11/25 01:30:08 wollman Exp $
 */

/*
 * 	Author: David B. Golub, Carnegie Mellon University
 *	Date:	7/90
 */

/*
 * Printf and character output for debugger.
 */

#include "param.h"
#include "systm.h"
#include "machine/stdarg.h"
#include "ddb/ddb.h"
#include "machine/cons.h"

/*
 *	Character output - tracks position in line.
 *	To do this correctly, we should know how wide
 *	the output device is - then we could zero
 *	the line position when the output device wraps
 *	around to the start of the next line.
 *
 *	Instead, we count the number of spaces printed
 *	since the last printing character so that we
 *	don't print trailing spaces.  This avoids most
 *	of the wraparounds.
 */
int	db_output_position = 0;		/* output column */
int	db_last_non_space = 0;		/* last non-space character */
int	db_tab_stop_width = 8;		/* how wide are tab stops? */
#define	NEXT_TAB(i) \
	((((i) + db_tab_stop_width) / db_tab_stop_width) * db_tab_stop_width)
int	db_max_width = 80;		/* output line width */


static void db_printf_guts(const char *, va_list);

/*
 * Force pending whitespace.
 */
void
db_force_whitespace()
{
	register int last_print, next_tab;

	last_print = db_last_non_space;
	while (last_print < db_output_position) {
	    next_tab = NEXT_TAB(last_print);
	    if (next_tab <= db_output_position) {
		while (last_print < next_tab) { /* DON'T send a tab!!! */
			cnputc(' ');
			last_print++;
		}
	    }
	    else {
		cnputc(' ');
		last_print++;
	    }
	}
	db_last_non_space = db_output_position;
}

/*
 * Output character.  Buffer whitespace.
 */
void
db_putchar(c)
	int	c;		/* character to output */
{
	if (c > ' ' && c <= '~') {
	    /*
	     * Printing character.
	     * If we have spaces to print, print them first.
	     * Use tabs if possible.
	     */
	    db_force_whitespace();
	    cnputc(c);
	    db_output_position++;
	    db_last_non_space = db_output_position;
	}
	else if (c == '\n') {
	    /* Return */
	    cnputc(c);
	    db_output_position = 0;
	    db_last_non_space = 0;
	    db_check_interrupt();
	}
	else if (c == '\t') {
	    /* assume tabs every 8 positions */
	    db_output_position = NEXT_TAB(db_output_position);
	}
	else if (c == ' ') {
	    /* space */
	    db_output_position++;
	}
	else if (c == '\007') {
	    /* bell */
	    cnputc(c);
	}
	/* other characters are assumed non-printing */
}

/*
 * Return output position
 */
int
db_print_position()
{
	return (db_output_position);
}

/*
 * Printing
 */
void
db_printf(const char *fmt, ...)
{
	va_list	listp;
	va_start(listp, fmt);
	db_printf_guts (fmt, listp);
	va_end(listp);
}

/* alternate name */

/*VARARGS1*/
void
kdbprintf(char *fmt, ...)
{
	va_list	listp;
	va_start(listp, fmt);
	db_printf_guts (fmt, listp);
	va_end(listp);
}

/*
 * End line if too long.
 */
void
db_end_line()
{
	if (db_output_position >= db_max_width)
	    db_printf("\n");
}

/*
 * Put a number (base <= 16) in a buffer in reverse order; return an
 * optional length and a pointer to the NULL terminated (preceded?)
 * buffer.
 */
static char *
db_ksprintn(ul, base, lenp)
	register u_long ul;
	register int base, *lenp;
{					/* A long in base 8, plus NULL. */
	static char buf[sizeof(long) * NBBY / 3 + 2];
	register char *p;

	p = buf;
	do {
		*++p = "0123456789abcdef"[ul % base];
	} while (ul /= base);
	if (lenp)
		*lenp = p - buf;
	return (p);
}

static void
db_printf_guts(fmt, ap)
	register const char *fmt;
	va_list ap;
{
	register char *p;
	register int ch, n;
	u_long ul;
	int base, lflag, tmp, width;
	char padc;
	int ladjust;
	int sharpflag;
	int neg;

	for (;;) {
		padc = ' ';
		width = 0;
		while ((ch = *(u_char *)fmt++) != '%') {
			if (ch == '\0')
				return;
			db_putchar(ch);
		}
		lflag = 0;
		ladjust = 0;
		sharpflag = 0;
		neg = 0;
reswitch:	switch (ch = *(u_char *)fmt++) {
		case '0':
			padc = '0';
			goto reswitch;
		case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			for (width = 0;; ++fmt) {
				width = width * 10 + ch - '0';
				ch = *fmt;
				if (ch < '0' || ch > '9')
					break;
			}
			goto reswitch;
		case 'l':
			lflag = 1;
			goto reswitch;
		case '-':
			ladjust = 1;
			goto reswitch;
		case '#':
			sharpflag = 1;
			goto reswitch;
		case 'b':
			ul = va_arg(ap, int);
			p = va_arg(ap, char *);
			for (p = db_ksprintn(ul, *p++, NULL); ch = *p--;)
				db_putchar(ch);

			if (!ul)
				break;

			for (tmp = 0; n = *p++;) {
				if (ul & (1 << (n - 1))) {
					db_putchar(tmp ? ',' : '<');
					for (; (n = *p) > ' '; ++p)
						db_putchar(n);
					tmp = 1;
				} else
					for (; *p > ' '; ++p);
			}
			if (tmp)
				db_putchar('>');
			break;
		case '*':
			width = va_arg (ap, int);
			if (width < 0) {
				ladjust = !ladjust;
				width = -width;
			}
			goto reswitch;
		case 'c':
			db_putchar(va_arg(ap, int));
			break;
		case 's':
			p = va_arg(ap, char *);
			width -= strlen (p);
			if (!ladjust && width > 0)
				while (width--)
					db_putchar (padc);
			while (ch = *p++)
				db_putchar(ch);
			if (ladjust && width > 0)
				while (width--)
					db_putchar (padc);
			break;
		case 'r':
			ul = lflag ? va_arg(ap, u_long) : va_arg(ap, u_int);
			if ((long)ul < 0) {
				neg = 1;
				ul = -(long)ul;
			}
			base = db_radix;
			if (base < 8 || base > 16)
				base = 10;
			goto number;
		case 'n':
			ul = lflag ? va_arg(ap, u_long) : va_arg(ap, u_int);
			base = db_radix;
			if (base < 8 || base > 16)
				base = 10;
			goto number;
		case 'd':
			ul = lflag ? va_arg(ap, long) : va_arg(ap, int);
			if ((long)ul < 0) {
				neg = 1;
				ul = -(long)ul;
			}
			base = 10;
			goto number;
		case 'o':
			ul = lflag ? va_arg(ap, u_long) : va_arg(ap, u_int);
			base = 8;
			goto number;
		case 'u':
			ul = lflag ? va_arg(ap, u_long) : va_arg(ap, u_int);
			base = 10;
			goto number;
		case 'z':
			ul = lflag ? va_arg(ap, u_long) : va_arg(ap, u_int);
			if ((long)ul < 0) {
				neg = 1;
				ul = -(long)ul;
			}
			base = 16;
			goto number;
		case 'x':
			ul = lflag ? va_arg(ap, u_long) : va_arg(ap, u_int);
			base = 16;
number:			p = (char *)db_ksprintn(ul, base, &tmp);
			if (sharpflag && ul != 0) {
				if (base == 8)
					tmp++;
				else if (base == 16)
					tmp += 2;
			}
			if (neg)
				tmp++;

			if (!ladjust && width && (width -= tmp) > 0)
				while (width--)
					db_putchar(padc);
			if (neg)
				db_putchar ('-');
			if (sharpflag && ul != 0) {
				if (base == 8) {
					db_putchar ('0');
				} else if (base == 16) {
					db_putchar ('0');
					db_putchar ('x');
				}
			}
			if (ladjust && width && (width -= tmp) > 0)
				while (width--)
					db_putchar(padc);

			while (ch = *p--)
				db_putchar(ch);
			break;
		default:
			db_putchar('%');
			if (lflag)
				db_putchar('l');
			/* FALLTHROUGH */
		case '%':
			db_putchar(ch);
		}
	}
}

