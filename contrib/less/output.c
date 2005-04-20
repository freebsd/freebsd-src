/*
 * Copyright (C) 1984-2002  Mark Nudelman
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

#if MSDOS_COMPILER==BORLANDC || MSDOS_COMPILER==DJGPPC
extern int ctldisp;
extern int nm_fg_color, nm_bg_color;
extern int bo_fg_color, bo_bg_color;
extern int ul_fg_color, ul_bg_color;
extern int so_fg_color, so_bg_color;
extern int bl_fg_color, bl_bg_color;
#endif

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
		char *op;
		DWORD nwritten = 0;
		CONSOLE_SCREEN_BUFFER_INFO scr;
		int row;
		int col;
		int olen;
		extern HANDLE con_out;

		olen = ob - obuf;
		/*
		 * There is a bug in Win32 WriteConsole() if we're
		 * writing in the last cell with a different color.
		 * To avoid color problems in the bottom line,
		 * we scroll the screen manually, before writing.
		 */
		GetConsoleScreenBufferInfo(con_out, &scr);
		col = scr.dwCursorPosition.X;
		row = scr.dwCursorPosition.Y;
		for (op = obuf;  op < obuf + olen;  op++)
		{
			if (*op == '\n')
			{
				col = 0;
				row++;
			} else if (*op == '\r')
			{
				col = 0;
			} else
			{
				col++;
				if (col >= sc_width)
				{
					col = 0;
					row++;
				}
			}
		}
		if (row > scr.srWindow.Bottom)
			win32_scroll_up(row - scr.srWindow.Bottom);
		WriteConsole(con_out, obuf, olen, &nwritten, NULL);
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
		if (ctldisp != OPT_ONPLUS)
			cputs(obuf);
		else
		{
			/*
			 * Look for SGR escape sequences, and convert them
			 * to color commands.  Replace bold, underline,
			 * and italic escapes into colors specified via
			 * the -D command-line option.
			 */
			char *anchor, *p, *p_next;
			int buflen = ob - obuf;
			unsigned char fg, bg, norm_attr;
			/*
			 * Only dark colors mentioned here, so that
			 * bold has visible effect.
			 */
			static enum COLORS screen_color[] = {
				BLACK, RED, GREEN, BROWN,
				BLUE, MAGENTA, CYAN, LIGHTGRAY
			};

			/* Normal text colors are used as baseline. */
			bg = nm_bg_color & 0xf;
			fg = nm_fg_color & 0xf;
			norm_attr = (bg << 4) | fg;
			for (anchor = p_next = obuf;
			     (p_next = memchr (p_next, ESC,
					       buflen - (p_next - obuf)))
			       != NULL; )
			{
				p = p_next;

				/*
				 * Handle the null escape sequence
				 * (ESC-[m), which is used to restore
				 * the original color.
				 */
				if (p[1] == '[' && is_ansi_end(p[2]))
				{
					textattr(norm_attr);
					p += 3;
					anchor = p_next = p;
					continue;
				}

				if (p[1] == '[')	/* "Esc-[" sequence */
				{
					/*
					 * If some chars seen since
					 * the last escape sequence,
					 * write it out to the screen
					 * using current text attributes.
					 */
					if (p > anchor)
					{
						*p = '\0';
						cputs (anchor);
						*p = ESC;
						anchor = p;
					}
					p += 2;
					p_next = p;
					while (!is_ansi_end(*p))
					{
						char *q;
						long code = strtol(p, &q, 10);

						if (!*q)
						{
							/*
							 * Incomplete sequence.
							 * Leave it unprocessed
							 * in the buffer.
							 */
							int slop = q - anchor;
							strcpy(obuf, anchor);
							ob = &obuf[slop];
							return;
						}

						if (q == p
						    || code > 49 || code < 0
						    || (!is_ansi_end(*q)
							&& *q != ';'))
						{
							p_next = q;
							break;
						}
						if (*q == ';')
							q++;

						switch (code)
						{
						case 1:	/* bold on */
							fg = bo_fg_color;
							bg = bo_bg_color;
							break;
						case 3:	/* italic on */
							fg = so_fg_color;
							bg = so_bg_color;
							break;
						case 4:	/* underline on */
							fg = ul_fg_color;
							bg = ul_bg_color;
							break;
						case 8:	/* concealed on */
							fg = (bg & 7) | 8;
							break;
						case 0:	/* all attrs off */
						case 22:/* bold off */
						case 23:/* italic off */
						case 24:/* underline off */
							fg = nm_fg_color;
							bg = nm_bg_color;
							break;
						case 30: case 31: case 32:
						case 33: case 34: case 35:
						case 36: case 37:
							fg = (fg & 8) | (screen_color[code - 30]);
							break;
						case 39: /* default fg */
							fg = nm_fg_color;
							break;
						case 40: case 41: case 42:
						case 43: case 44: case 45:
						case 46: case 47:
							bg = (bg & 8) | (screen_color[code - 40]);
							break;
						case 49: /* default fg */
							bg = nm_bg_color;
							break;
						}
						p = q;
					}
					if (is_ansi_end(*p) && p > p_next)
					{
						bg &= 15;
						fg &= 15;
						textattr ((bg << 4)| fg);
						p_next = anchor = p + 1;
					} else
						break;
				} else
					p_next++;
			}

			/* Output what's left in the buffer.  */
			cputs (anchor);
		}
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
 * Convert an integral type to a string.
 */
#define TYPE_TO_A_FUNC(funcname, type) \
void funcname(num, buf) \
	type num; \
	char *buf; \
{ \
	int neg = (num < 0); \
	char tbuf[INT_STRLEN_BOUND(num)+2]; \
	register char *s = tbuf + sizeof(tbuf); \
	if (neg) num = -num; \
	*--s = '\0'; \
	do { \
		*--s = (num % 10) + '0'; \
	} while ((num /= 10) != 0); \
	if (neg) *--s = '-'; \
	strcpy(buf, s); \
}

TYPE_TO_A_FUNC(postoa, POSITION)
TYPE_TO_A_FUNC(linenumtoa, LINENUM)
TYPE_TO_A_FUNC(inttoa, int)

/*
 * Output an integer in a given radix.
 */
	static int
iprint_int(num)
	int num;
{
	char buf[INT_STRLEN_BOUND(num)];

	inttoa(num, buf);
	putstr(buf);
	return (strlen(buf));
}

/*
 * Output a line number in a given radix.
 */
	static int
iprint_linenum(num)
	LINENUM num;
{
	char buf[INT_STRLEN_BOUND(num)];

	linenumtoa(num, buf);
	putstr(buf);
	return (strlen(buf));
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
			switch (*fmt++)
			{
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
				col += iprint_int(parg->p_int);
				parg++;
				break;
			case 'n':
				col += iprint_linenum(parg->p_linenum);
				parg++;
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
