/****************************************************************************
 * Copyright 2025 Thomas E. Dickey                                          *
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
 *  Author: Thomas E. Dickey                                                *
 ****************************************************************************/

#include <curses.priv.h>

MODULE_ID("$Id: report_ctype.c,v 1.1 2025/10/25 19:21:11 tom Exp $")

#include <ctype.h>
#include <wctype.h>
#include <locale.h>

#if HAVE_LANGINFO_CODESET
#include <langinfo.h>
#endif

#define PER_LINE  32

static void
report(char *locale)
{
    int ch;
    wint_t wch;
    char *dot;
    printf("Locale \"%s\"", locale);
    if (setlocale(LC_CTYPE, locale) != NULL) {
#if HAVE_LANGINFO_CODESET
	char *codeset = nl_langinfo(CODESET);
	if (codeset != NULL) {
	    printf("\nCodeset \"%s\"", codeset);
	}
#endif
	for (ch = 0; ch < 256; ++ch) {
	    int code = '?';
	    wch = ch;
	    if (isprint(ch) && iswprint(wch))
		code = '=';
	    if (!isprint(ch) && iswprint(wch))
		code = '+';
	    if (isprint(ch) && !iswprint(wch))
		code = '-';
	    if ((ch & (PER_LINE - 1)) == 0)
		printf("\n%02X: ", ch);
	    putchar(code);
	}
	putchar('\n');
    } else {
	fprintf(stderr, "Cannot set locale\n");
    }
    if ((dot = strchr(locale, '.')) != NULL) {
	*dot = '\0';
	report(locale);
    }
}

int
main(int argc, char *argv[])
{
    if (argc > 1) {
	int n;
	for (n = 1; n < argc; ++n) {
	    report(argv[n]);
	}
    } else {
	static char empty[1];
	char *locale = getenv("LC_CTYPE");
	if (locale == NULL)
	    locale = getenv("LC_ALL");
	if (locale == NULL)
	    locale = getenv("LANG");
	if (locale == NULL)
	    locale = empty;
	report(locale);
    }
    return EXIT_SUCCESS;
}
