/*-
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
 */
/*
 * 	Author: David B. Golub, Carnegie Mellon University
 *	Date:	7/90
 */

/*
 * Printf and character output for debugger.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/cons.h>
#include <sys/kdb.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>

#include <machine/stdarg.h>

#include <ddb/ddb.h>
#include <ddb/db_output.h>

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
static int	db_output_position = 0;		/* output column */
static int	db_last_non_space = 0;		/* last non-space character */
db_expr_t	db_tab_stop_width = 8;		/* how wide are tab stops? */
#define	NEXT_TAB(i) \
	((((i) + db_tab_stop_width) / db_tab_stop_width) * db_tab_stop_width)
db_expr_t	db_max_width = 79;		/* output line width */
db_expr_t	db_lines_per_page = 20;		/* lines per page */
static int	db_newlines;			/* # lines this page */
static int	db_maxlines = -1;		/* max lines/page when paging */
static db_page_calloutfcn_t *db_page_callout = NULL;
static void	*db_page_callout_arg = NULL;
static int	ddb_use_printf = 0;
SYSCTL_INT(_debug, OID_AUTO, ddb_use_printf, CTLFLAG_RW, &ddb_use_printf, 0,
    "use printf for all ddb output");

static void	db_putchar(int c, void *arg);

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
static void
db_putchar(c, arg)
	int	c;		/* character to output */
	void *	arg;
{

	/*
	 * If not in the debugger or the user requests it, output data to
	 * both the console and the message buffer.
	 */
	if (!kdb_active || ddb_use_printf) {
		printf("%c", c);
		if (!kdb_active)
			return;
		if (c == '\r' || c == '\n')
			db_check_interrupt();
		if (c == '\n' && db_maxlines > 0 && db_page_callout != NULL) {
			db_newlines++;
			if (db_newlines >= db_maxlines) {
				db_maxlines = -1;
				db_page_callout(db_page_callout_arg);
			}
		}
		return;
	}

	/* Otherwise, output data directly to the console. */
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
	    /* Newline */
	    db_force_whitespace();
	    cnputc(c);
	    db_output_position = 0;
	    db_last_non_space = 0;
	    db_check_interrupt();
	    if (db_maxlines > 0 && db_page_callout != NULL) {
		    db_newlines++;
		    if (db_newlines >= db_maxlines) {
			    db_maxlines = -1;
			    db_page_callout(db_page_callout_arg);
		    }
	    }
	}
	else if (c == '\r') {
	    /* Return */
	    db_force_whitespace();
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
 * Register callout for providing a pager for output.
 */
void
db_setup_paging(db_page_calloutfcn_t *callout, void *arg, int maxlines)
{

	db_page_callout = callout;
	db_page_callout_arg = arg;
	db_maxlines = maxlines;
	db_newlines = 0;
}

/*
 * A simple paging callout function.  If the argument is not null, it
 * points to an integer that will be set to 1 if the user asks to quit.
 */
void
db_simple_pager(void *arg)
{
	int c, done;

	db_printf("--More--\r");
	done = 0;
	while (!done) {
		c = cngetc();
		switch (c) {
		case 'e':
		case 'j':
		case '\n':
			/* Just one more line. */
			db_setup_paging(db_simple_pager, arg, 1);
			done++;
			break;
		case 'd':
			/* Half a page. */
			db_setup_paging(db_simple_pager, arg,
			    db_lines_per_page / 2);
			done++;
			break;
		case 'f':
		case ' ':
			/* Another page. */
			db_setup_paging(db_simple_pager, arg,
			    db_lines_per_page);
			done++;
			break;
		case 'q':
		case 'Q':
		case 'x':
		case 'X':
			/* Quit */
			if (arg != NULL) {
				*(int *)arg = 1;
				done++;
				break;
			}
#if 0
			/* FALLTHROUGH */
		default:
			cnputc('\007');
#endif
		}
	}
	db_printf("        \r");
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
#if __STDC__
db_printf(const char *fmt, ...)
#else
db_printf(fmt)
	const char *fmt;
#endif
{
	va_list	listp;

	va_start(listp, fmt);
	kvprintf (fmt, db_putchar, NULL, db_radix, listp);
	va_end(listp);
}

int db_indent;

void
#if __STDC__
db_iprintf(const char *fmt,...)
#else
db_iprintf(fmt)
	const char *fmt;
#endif
{
	register int i;
	va_list listp;

	for (i = db_indent; i >= 8; i -= 8)
		db_printf("\t");
	while (--i >= 0)
		db_printf(" ");
	va_start(listp, fmt);
	kvprintf (fmt, db_putchar, NULL, db_radix, listp);
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
