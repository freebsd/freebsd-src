/****************************************************************************
 * Copyright (c) 1998,1999 Free Software Foundation, Inc.                   *
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
 *  Author: Thomas E. Dickey <dickey@clark.net> 1997                        *
 ****************************************************************************/

#include <curses.priv.h>
#include <ctype.h>

MODULE_ID("$Id: safe_sprintf.c,v 1.11 1999/09/11 18:03:27 tom Exp $")

#if USE_SAFE_SPRINTF

typedef enum { Flags, Width, Prec, Type, Format } PRINTF;

#define VA_INTGR(type) ival = va_arg(ap, type)
#define VA_FLOAT(type) fval = va_arg(ap, type)
#define VA_POINT(type) pval = (void *)va_arg(ap, type)

/*
 * Scan a variable-argument list for printf to determine the number of
 * characters that would be emitted.
 */
static int
_nc_printf_length(const char *fmt, va_list ap)
{
	size_t length = BUFSIZ;
	char *buffer;
	char *format;
	int len = 0;

	if (fmt == 0 || *fmt == '\0')
		return -1;
	if ((format = typeMalloc(char, strlen(fmt)+1)) == 0)
		return -1;
	if ((buffer = typeMalloc(char, length)) == 0) {
		free(format);
		return -1;
	}

	while (*fmt != '\0') {
		if (*fmt == '%') {
			static char dummy[] = "";
			PRINTF state = Flags;
			char *pval   = dummy;	/* avoid const-cast */
			double fval  = 0.0;
			int done     = FALSE;
			int ival     = 0;
			int prec     = -1;
			int type     = 0;
			int used     = 0;
			int width    = -1;
			size_t f     = 0;

			format[f++] = *fmt;
			while (*++fmt != '\0' && len >= 0 && !done) {
				format[f++] = *fmt;

				if (isdigit(*fmt)) {
					int num = *fmt - '0';
					if (state == Flags && num != 0)
						state = Width;
					if (state == Width) {
						if (width < 0)
							width = 0;
						width = (width * 10) + num;
					} else if (state == Prec) {
						if (prec < 0)
							prec = 0;
						prec = (prec * 10) + num;
					}
				} else if (*fmt == '*') {
					VA_INTGR(int);
					if (state == Flags)
						state = Width;
					if (state == Width) {
						width = ival;
					} else if (state == Prec) {
						prec = ival;
					}
					sprintf(&format[--f], "%d", ival);
					f = strlen(format);
				} else if (isalpha(*fmt)) {
					done = TRUE;
					switch (*fmt) {
					case 'Z': /* FALLTHRU */
					case 'h': /* FALLTHRU */
					case 'l': /* FALLTHRU */
						done = FALSE;
						type = *fmt;
						break;
					case 'i': /* FALLTHRU */
					case 'd': /* FALLTHRU */
					case 'u': /* FALLTHRU */
					case 'x': /* FALLTHRU */
					case 'X': /* FALLTHRU */
						if (type == 'l')
							VA_INTGR(long);
						else if (type == 'Z')
							VA_INTGR(size_t);
						else
							VA_INTGR(int);
						used = 'i';
						break;
					case 'f': /* FALLTHRU */
					case 'e': /* FALLTHRU */
					case 'E': /* FALLTHRU */
					case 'g': /* FALLTHRU */
					case 'G': /* FALLTHRU */
						VA_FLOAT(double);
						used = 'f';
						break;
					case 'c':
						VA_INTGR(int);
						used = 'i';
						break;
					case 's':
						VA_POINT(char *);
						if (prec < 0)
							prec = strlen(pval);
						if (prec > (int)length) {
							length = length + prec;
							buffer = typeRealloc(char, length, buffer);
							if (buffer == 0) {
								free(format);
								return -1;
							}
						}
						used = 'p';
						break;
					case 'p':
						VA_POINT(void *);
						used = 'p';
						break;
					case 'n':
						VA_POINT(int *);
						used = 0;
						break;
					default:
						break;
					}
				} else if (*fmt == '.') {
					state = Prec;
				} else if (*fmt == '%') {
					done = TRUE;
					used = 'p';
				}
			}
			format[f] = '\0';
			switch (used) {
			case 'i':
				sprintf(buffer, format, ival);
				break;
			case 'f':
				sprintf(buffer, format, fval);
				break;
			default:
				sprintf(buffer, format, pval);
				break;
			}
			len += (int)strlen(buffer);
		} else {
			fmt++;
			len++;
		}
	}

	free(buffer);
	free(format);
	return len;
}
#endif

/*
 * Wrapper for vsprintf that allocates a buffer big enough to hold the result.
 */
char *
_nc_printf_string(const char *fmt, va_list ap)
{
#if USE_SAFE_SPRINTF
	char *buf = 0;
	int len = _nc_printf_length(fmt, ap);

	if (len > 0) {
		if ((buf = typeMalloc(char, len+1)) == 0)
			return(0);
		vsprintf(buf, fmt, ap);
	}
#else
	static int rows, cols;
	static char *buf;
	static size_t len;

	if (screen_lines > rows || screen_columns > cols) {
		if (screen_lines   > rows) rows = screen_lines;
		if (screen_columns > cols) cols = screen_columns;
		len = (rows * (cols + 1)) + 1;
		buf = typeRealloc(char, len, buf);
		if (buf == 0) {
			return(0);
		}
	}

	if (buf != 0) {
# if HAVE_VSNPRINTF
		vsnprintf(buf, len, fmt, ap);	/* GNU extension */
# else
		vsprintf(buf, fmt, ap);		/* ANSI */
# endif
	}
#endif
	return buf;
}
