/*
 * Copyright (C) 1984-2000  Mark Nudelman
 *
 * You may distribute under the terms of either the GNU General Public
 * License or the Less License, as specified in the README file.
 *
 * For more information about less, or for information on how to 
 * contact the author, see the README file.
 */


/*
 * High level routines dealing with the output to the screen.
 */

#include "less.h"
#if MSDOS_COMPILER==WIN32C
#include "windows.h"
#endif

public int errmsgs;	/* Count of messages displayed by error() */
public int need_clr;
public int final_attr;

extern int sigs;
extern int sc_width;
extern int so_s_width, so_e_width;
extern int screen_trashed;
extern int any_display;
extern int is_tty;

/*
 * Display the line which is in the line buffer.
 */
	public void
put_line()
{
	register int c;
	register int i;
	int a;
	int curr_attr;

	if (ABORT_SIGS())
	{
		/*
		 * Don't output if a signal is pending.
		 */
		screen_trashed = 1;
		return;
	}

	curr_attr = AT_NORMAL;

	for (i = 0;  (c = gline(i, &a)) != '\0';  i++)
	{
		if (a != curr_attr)
		{
			/*
			 * Changing attributes.
			 * Display the exit sequence for the old attribute
			 * and the enter sequence for the new one.
			 */
			switch (curr_attr)
			{
			case AT_UNDERLINE:	ul_exit();	break;
			case AT_BOLD:		bo_exit();	break;
			case AT_BLINK:		bl_exit();	break;
			case AT_STANDOUT:	so_exit();	break;
			}
			switch (a)
			{
			case AT_UNDERLINE:	ul_enter();	break;
			case AT_BOLD:		bo_enter();	break;
			case AT_BLINK:		bl_enter();	break;
			case AT_STANDOUT:	so_enter();	break;
			}
			curr_attr = a;
		}
		if (curr_attr == AT_INVIS)
			continue;
		if (c == '\b')
			putbs();
		else
			putchr(c);
	}

	switch (curr_attr)
	{
	case AT_UNDERLINE:	ul_exit();	break;
	case AT_BOLD:		bo_exit();	break;
	case AT_BLINK:		bl_exit();	break;
	case AT_STANDOUT:	so_exit();	break;
	}
	final_attr = curr_attr;
}

static char obuf[OUTBUF_SIZE];
static char *ob = obuf;

/*
 * Flush buffered output.
 *
 * If we haven't displayed any file data yet,
 * output messages on error output (file descriptor 2),
 * otherwise output on standard output (file descriptor 1).
 *
 * This has the desirable effect of producing all
 * error messages on error output if standard output
 * is directed to a file.  It also does the same if
 * we never produce any real output; for example, if
 * the input file(s) cannot be opened.  If we do
 * eventually produce output, code in edit() makes
 * sure these messages can be seen before they are
 * overwritten or scrolled away.
 */
	public void
flush()
{
	register int n;
	register int fd;

	n = ob - obuf;
	if (n == 0)
		return;
#if MSDOS_COMPILER==WIN32C
	if (is_tty && any_display)
	{
		char *p;
		char *op;
		DWORD nwritten = 0;
		CONSOLE_SCREEN_BUFFER_INFO scr;
		DWORD nchars;
		COORD cpos;
		WORD nm_attr;
		int olen;
		extern HANDLE con_out;
		extern int nm_fg_color;
		extern int nm_bg_color;
#define	MAKEATTR(fg,bg)		((WORD)((fg)|((bg)<<4)))

		*ob = '\0';
		olen = ob - obuf;
		/*
		 * To avoid color problems, if we're scrolling the screen,
		 * we write only up to the char that causes the scroll,
		 * (a newline or a char in the last column), then fill 
		 * the bottom line with the "normal" attribute, then 
		 * write the rest.
		 * When Windows scrolls, it takes the attributes for the 
		 * new line from the first char of the (previously) 
		 * bottom line.
		 *
		 * {{ This still doesn't work correctly in all cases! }}
		 */
		nm_attr = MAKEATTR(nm_fg_color, nm_bg_color);
		for (op = obuf;  *op != '\0';  )
		{
			GetConsoleScreenBufferInfo(con_out, &scr);
			/* Find the next newline. */
			p = strchr(op, '\n');
			if (p == NULL &&
			    scr.dwCursorPosition.X + olen >= sc_width)
			{
				/*
				 * No newline, but writing in the 
				 * last column causes scrolling.
				 */
				p = op + sc_width - scr.dwCursorPosition.X - 1;
			}
			if (scr.dwCursorPosition.Y != scr.srWindow.Bottom ||
			    p == NULL)
			{
				/* Write the entire buffer. */
				WriteConsole(con_out, op, olen,
						&nwritten, NULL);
				op += olen;
			} else
			{
				/* Write only up to the scrolling char. */
				WriteConsole(con_out, op, p - op + 1, 
						&nwritten, NULL);
				cpos.X = 0;
				cpos.Y = scr.dwCursorPosition.Y;
				FillConsoleOutputAttribute(con_out, nm_attr,
						sc_width, cpos, &nchars);
				olen -= p - op + 1;
				op = p + 1;
			}
		}
		ob = obuf;
		return;
	}
#else
#if MSDOS_COMPILER==MSOFTC
	if (is_tty && any_display)
	{
		*ob = '\0';
		_outtext(obuf);
		ob = obuf;
		return;
	}
#else
#if MSDOS_COMPILER==BORLANDC || MSDOS_COMPILER==DJGPPC
	if (is_tty && any_display)
	{
		*ob = '\0';
		cputs(obuf);
		ob = obuf;
		return;
	}
#endif
#endif
#endif
	fd = (any_display) ? 1 : 2;
	if (write(fd, obuf, n) != n)
		screen_trashed = 1;
	ob = obuf;
}

/*
 * Output a character.
 */
	public int
putchr(c)
	int c;
{
	if (need_clr)
	{
		need_clr = 0;
		clear_bot();
	}
#if MSDOS_COMPILER
	if (c == '\n' && is_tty)
	{
		/* remove_top(1); */
		putchr('\r');
	}
#else
#ifdef _OSK
	if (c == '\n' && is_tty)  /* In OS-9, '\n' == 0x0D */
		putchr(0x0A);
#endif
#endif
	/*
	 * Some versions of flush() write to *ob, so we must flush
	 * when we are still one char from the end of obuf.
	 */
	if (ob >= &obuf[sizeof(obuf)-1])
		flush();
	*ob++ = c;
	return (c);
}

/*
 * Output a string.
 */
	public void
putstr(s)
	register char *s;
{
	while (*s != '\0')
		putchr(*s++);
}


/*
 * Output an integer in a given radix.
 */
	static int
iprintnum(num, radix)
	int num;
	int radix;
{
	register char *s;
	int r;
	int neg;
	char buf[10];

	neg = (num < 0);
	if (neg)
		num = -num;

	s = buf;
	do
	{
		*s++ = (num % radix) + '0';
	} while ((num /= radix) != 0);

	if (neg)
		*s++ = '-';
	r = s - buf;

	while (s > buf)
		putchr(*--s);
	return (r);
}

/*
 * This function implements printf-like functionality
 * using a more portable argument list mechanism than printf's.
 */
	static int
less_printf(fmt, parg)
	register char *fmt;
	PARG *parg;
{
	register char *s;
	register int n;
	register int col;

	col = 0;
	while (*fmt != '\0')
	{
		if (*fmt != '%')
		{
			putchr(*fmt++);
			col++;
		} else
		{
			++fmt;
			switch (*fmt++) {
			case 's':
				s = parg->p_string;
				parg++;
				while (*s != '\0')
				{
					putchr(*s++);
					col++;
				}
				break;
			case 'd':
				n = parg->p_int;
				parg++;
				col += iprintnum(n, 10);
				break;
			}
		}
	}
	return (col);
}

/*
 * Get a RETURN.
 * If some other non-trivial char is pressed, unget it, so it will
 * become the next command.
 */
	public void
get_return()
{
	int c;

#if ONLY_RETURN
	while ((c = getchr()) != '\n' && c != '\r')
		bell();
#else
	c = getchr();
	if (c != '\n' && c != '\r' && c != ' ' && c != READ_INTR)
		ungetcc(c);
#endif
}

/*
 * Output a message in the lower left corner of the screen
 * and wait for carriage return.
 */
	public void
error(fmt, parg)
	char *fmt;
	PARG *parg;
{
	int col = 0;
	static char return_to_continue[] = "  (press RETURN)";

	errmsgs++;

	if (any_display && is_tty)
	{
		clear_bot();
		so_enter();
		col += so_s_width;
	}

	col += less_printf(fmt, parg);

	if (!(any_display && is_tty))
	{
		putchr('\n');
		return;
	}

	putstr(return_to_continue);
	so_exit();
	col += sizeof(return_to_continue) + so_e_width;

	get_return();
	lower_left();

	if (col >= sc_width)
		/*
		 * Printing the message has probably scrolled the screen.
		 * {{ Unless the terminal doesn't have auto margins,
		 *    in which case we just hammered on the right margin. }}
		 */
		screen_trashed = 1;

	flush();
}

static char intr_to_abort[] = "... (interrupt to abort)";

/*
 * Output a message in the lower left corner of the screen
 * and don't wait for carriage return.
 * Usually used to warn that we are beginning a potentially
 * time-consuming operation.
 */
	public void
ierror(fmt, parg)
	char *fmt;
	PARG *parg;
{
	clear_bot();
	so_enter();
	(void) less_printf(fmt, parg);
	putstr(intr_to_abort);
	so_exit();
	flush();
	need_clr = 1;
}

/*
 * Output a message in the lower left corner of the screen
 * and return a single-character response.
 */
	public int
query(fmt, parg)
	char *fmt;
	PARG *parg;
{
	register int c;
	int col = 0;

	if (any_display && is_tty)
		clear_bot();

	(void) less_printf(fmt, parg);
	c = getchr();

	if (!(any_display && is_tty))
	{
		putchr('\n');
		return (c);
	}

	lower_left();
	if (col >= sc_width)
		screen_trashed = 1;
	flush();

	return (c);
}
