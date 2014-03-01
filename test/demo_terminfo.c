/****************************************************************************
 * Copyright (c) 2009,2010 Free Software Foundation, Inc.                   *
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

/*
 * Author: Thomas E. Dickey
 *
 * $Id: demo_terminfo.c,v 1.9 2010/11/28 00:15:27 tom Exp $
 *
 * A simple demo of the terminfo interface.
 */
#define USE_TINFO
#include <test.priv.h>

#ifdef NCURSES_VERSION
#if !(defined(HAVE_TERM_ENTRY_H) && HAVE_TERM_ENTRY_H)
#undef NCURSES_XNAMES
#define NCURSES_XNAMES 0
#endif
#if NCURSES_XNAMES
#include <term_entry.h>
#endif
#endif

#if HAVE_TIGETSTR
#if defined(HAVE_CURSES_DATA_BOOLNAMES) || defined(DECL_CURSES_DATA_BOOLNAMES)

static bool b_opt = FALSE;
static bool f_opt = FALSE;
static bool n_opt = FALSE;
static bool s_opt = FALSE;
static bool x_opt = FALSE;

#define FCOLS 8
#define FNAME(type) "%s %-*s = ", #type, FCOLS

static void
dumpit(NCURSES_CONST char *cap)
{
    /*
     * One of the limitations of the termcap interface is that the library
     * cannot determine the size of the buffer passed via tgetstr(), nor the
     * amount of space remaining.  This demo simply reuses the whole buffer
     * for each call; a normal termcap application would try to use the buffer
     * to hold all of the strings extracted from the terminal entry.
     */
    const char *str;
    int num;

    if ((str = tigetstr(cap)) != 0 && (str != (char *) -1)) {
	/*
	 * Note that the strings returned are mostly terminfo format, since
	 * ncurses does not convert except for a handful of special cases.
	 */
	printf(FNAME(str), cap);
	while (*str != 0) {
	    int ch = UChar(*str++);
	    switch (ch) {
	    case '\177':
		fputs("^?", stdout);
		break;
	    case '\033':
		fputs("\\E", stdout);
		break;
	    case '\b':
		fputs("\\b", stdout);
		break;
	    case '\f':
		fputs("\\f", stdout);
		break;
	    case '\n':
		fputs("\\n", stdout);
		break;
	    case '\r':
		fputs("\\r", stdout);
		break;
	    case ' ':
		fputs("\\s", stdout);
		break;
	    case '\t':
		fputs("\\t", stdout);
		break;
	    case '^':
		fputs("\\^", stdout);
		break;
	    case ':':
		fputs("\\072", stdout);
		break;
	    case '\\':
		fputs("\\\\", stdout);
		break;
	    default:
		if (isgraph(ch))
		    fputc(ch, stdout);
		else if (ch < 32)
		    printf("^%c", ch + '@');
		else
		    printf("\\%03o", ch);
		break;
	    }
	}
	printf("\n");
    } else if ((num = tigetnum(cap)) >= 0) {
	printf(FNAME(num), cap);
	printf(" %d\n", num);
    } else if ((num = tigetflag(cap)) >= 0) {
	printf(FNAME(flg), cap);
	printf("%s\n", num ? "true" : "false");
    }
    fflush(stdout);
}

static void
demo_terminfo(char *name)
{
    unsigned n;
    NCURSES_CONST char *cap;

    printf("Terminal type \"%s\"\n", name);
    setupterm(name, 1, (int *) 0);

    if (b_opt) {
	for (n = 0;; ++n) {
	    cap = f_opt ? boolfnames[n] : boolnames[n];
	    if (cap == 0)
		break;
	    dumpit(cap);
	}
    }

    if (n_opt) {
	for (n = 0;; ++n) {
	    cap = f_opt ? numfnames[n] : numnames[n];
	    if (cap == 0)
		break;
	    dumpit(cap);
	}
    }

    if (s_opt) {
	for (n = 0;; ++n) {
	    cap = f_opt ? strfnames[n] : strnames[n];
	    if (cap == 0)
		break;
	    dumpit(cap);
	}
    }
#ifdef NCURSES_VERSION
    if (x_opt) {
	int mod;
	if (f_opt) {
#if NCURSES_XNAMES
	    TERMTYPE *term = &(cur_term->type);
	    if (term != 0
		&& ((NUM_BOOLEANS(term) != BOOLCOUNT)
		    || (NUM_NUMBERS(term) != NUMCOUNT)
		    || (NUM_STRINGS(term) != STRCOUNT))) {
		for (n = BOOLCOUNT; n < NUM_BOOLEANS(term); ++n) {
		    dumpit(ExtBoolname(term, (int) n, boolnames));
		}
		for (n = NUMCOUNT; n < NUM_NUMBERS(term); ++n) {
		    dumpit(ExtNumname(term, (int) n, numnames));
		}
		for (n = STRCOUNT; n < NUM_STRINGS(term); ++n) {
		    dumpit(ExtStrname(term, (int) n, strnames));
		}
	    }
#endif
	} else {
	    char temp[10];
	    static const char *xterm_keys[] =
	    {
		"kDC", "kDN", "kEND", "kHOM", "kIC",
		"kLFT", "kNXT", "kPRV", "kRIT", "kUP",
	    };
	    for (n = 0; n < SIZEOF(xterm_keys); ++n) {
		for (mod = 0; mod < 8; ++mod) {
		    if (mod == 0)
			strcpy(temp, xterm_keys[n]);
		    else
			sprintf(temp, "%s%d", xterm_keys[n], mod);
		    dumpit(temp);
		}
	    }
	}
    }
#endif

}

static void
usage(void)
{
    static const char *msg[] =
    {
	"Usage: demo_terminfo [options] [terminal]",
	"",
	"If no options are given, print all (boolean, numeric, string)",
	"capabilities for the given terminal, using short names.",
	"",
	"Options:",
	" -b       print boolean-capabilities",
	" -f       print full names",
	" -n       print numeric-capabilities",
	" -r COUNT repeat for given count",
	" -s       print string-capabilities",
#ifdef NCURSES_VERSION
	" -x       print extended capabilities",
#endif
    };
    unsigned n;
    for (n = 0; n < SIZEOF(msg); ++n) {
	fprintf(stderr, "%s\n", msg[n]);
    }
    ExitProgram(EXIT_FAILURE);
}

int
main(int argc, char *argv[])
{
    int n;
    int repeat;
    char *name;
    int r_opt = 1;

    while ((n = getopt(argc, argv, "bfnr:sx")) != -1) {
	switch (n) {
	case 'b':
	    b_opt = TRUE;
	    break;
	case 'f':
	    f_opt = TRUE;
	    break;
	case 'n':
	    n_opt = TRUE;
	    break;
	case 'r':
	    if ((r_opt = atoi(optarg)) <= 0)
		usage();
	    break;
	case 's':
	    s_opt = TRUE;
	    break;
#ifdef NCURSES_VERSION
	case 'x':
	    x_opt = TRUE;
	    use_extended_names(TRUE);
	    break;
#endif
	default:
	    usage();
	    break;
	}
    }

    if (!(b_opt || n_opt || s_opt || x_opt)) {
	b_opt = TRUE;
	n_opt = TRUE;
	s_opt = TRUE;
    }

    for (repeat = 0; repeat < r_opt; ++repeat) {
	if (optind < argc) {
	    for (n = optind; n < argc; ++n) {
		demo_terminfo(argv[n]);
	    }
	} else if ((name = getenv("TERM")) != 0) {
	    demo_terminfo(name);
	} else {
	    static char dumb[] = "dumb";
	    demo_terminfo(dumb);
	}
    }

    ExitProgram(EXIT_SUCCESS);
}

#else
int
main(int argc GCC_UNUSED, char *argv[]GCC_UNUSED)
{
    printf("This program requires the terminfo arrays\n");
    ExitProgram(EXIT_FAILURE);
}
#endif
#else /* !HAVE_TIGETSTR */
int
main(int argc GCC_UNUSED, char *argv[]GCC_UNUSED)
{
    printf("This program requires the terminfo functions such as tigetstr\n");
    ExitProgram(EXIT_FAILURE);
}
#endif /* HAVE_TIGETSTR */
