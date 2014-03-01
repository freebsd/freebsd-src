/****************************************************************************
 * Copyright (c) 2005-2012,2013 Free Software Foundation, Inc.              *
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
 * $Id: demo_termcap.c,v 1.24 2013/06/08 16:58:49 tom Exp $
 *
 * A simple demo of the termcap interface.
 */
#define USE_TINFO
#include <test.priv.h>

#if HAVE_TGETENT

#if defined(HAVE_CURSES_DATA_BOOLNAMES) || defined(DECL_CURSES_DATA_BOOLNAMES)
#define USE_CODE_LISTS 1
#else
#define USE_CODE_LISTS 0
#endif

#define FCOLS 8
#define FNAME(type) "%s %-*s = ", #type, FCOLS

#if USE_CODE_LISTS
static bool b_opt = FALSE;
static bool n_opt = FALSE;
static bool q_opt = FALSE;
static bool s_opt = FALSE;
#endif

static char *d_opt;
static char *e_opt;
static char **db_list;
static int db_item;

static long total_values;

#define isCapName(c) (isgraph(c) && strchr("^#=:\\", c) == 0)

static char *
make_dbitem(char *p, char *q)
{
    char *result = malloc(strlen(e_opt) + 2 + (size_t) (p - q));
    sprintf(result, "%s=%.*s", e_opt, (int) (p - q), q);
    return result;
}

static void
make_dblist(void)
{
    if (d_opt && e_opt) {
	int pass;

	for (pass = 0; pass < 2; ++pass) {
	    char *p, *q;
	    size_t count = 0;

	    for (p = q = d_opt; *p != '\0'; ++p) {
		if (*p == ':') {
		    if (p != q + 1) {
			if (pass) {
			    db_list[count] = make_dbitem(p, q);
			}
			count++;
		    }
		    q = p + 1;
		}
	    }
	    if (p != q + 1) {
		if (pass) {
		    db_list[count] = make_dbitem(p, q);
		}
		count++;
	    }
	    if (!pass) {
		db_list = typeCalloc(char *, count + 1);
	    }
	}
    }
}

static char *
next_dbitem(void)
{
    char *result = 0;

    if (db_list) {
	if ((result = db_list[db_item]) == 0) {
	    db_item = 0;
	    result = db_list[0];
	} else {
	    db_item++;
	}
    }
    printf("** %s\n", result);
    return result;
}

static void
free_dblist(void)
{
    if (db_list) {
	int n;
	for (n = 0; db_list[n]; ++n)
	    free(db_list[n]);
	free(db_list);
	db_list = 0;
    }
}

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
    char area[1024], *ap = area;
    char *str;
    int num;

    if ((str = tgetstr(cap, &ap)) != 0) {
	total_values++;
	if (!q_opt) {
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
	}
    } else if ((num = tgetnum(cap)) >= 0) {
	total_values++;
	if (!q_opt) {
	    printf(FNAME(num), cap);
	    printf(" %d\n", num);
	}
    } else if (tgetflag(cap) > 0) {
	++total_values;
	if (!q_opt) {
	    printf(FNAME(flg), cap);
	    printf("%s\n", "true");
	}
    }

    if (!q_opt)
	fflush(stdout);
}

static void
brute_force(const char *name)
{
    char buffer[1024];

    if (db_list) {
	putenv(next_dbitem());
    }
    printf("Terminal type %s\n", name);
    if (tgetent(buffer, name) >= 0) {
	char cap[3];
	int c1, c2;

	cap[2] = 0;
	for (c1 = 0; c1 < 256; ++c1) {
	    cap[0] = (char) c1;
	    if (isCapName(c1)) {
		for (c2 = 0; c2 < 256; ++c2) {
		    cap[1] = (char) c2;
		    if (isCapName(c2)) {
			dumpit(cap);
		    }
		}
	    }
	}
    }
}

#if USE_CODE_LISTS
static void
demo_termcap(NCURSES_CONST char *name)
{
    unsigned n;
    NCURSES_CONST char *cap;
    char buffer[1024];

    if (db_list) {
	putenv(next_dbitem());
    }
    printf("Terminal type \"%s\"\n", name);
    if (tgetent(buffer, name) >= 0) {

	if (b_opt) {
	    for (n = 0;; ++n) {
		cap = boolcodes[n];
		if (cap == 0)
		    break;
		dumpit(cap);
	    }
	}

	if (n_opt) {
	    for (n = 0;; ++n) {
		cap = numcodes[n];
		if (cap == 0)
		    break;
		dumpit(cap);
	    }
	}

	if (s_opt) {
	    for (n = 0;; ++n) {
		cap = strcodes[n];
		if (cap == 0)
		    break;
		dumpit(cap);
	    }
	}
    }
}

static void
usage(void)
{
    static const char *msg[] =
    {
	"Usage: demo_termcap [options] [terminal]",
	"",
	"If no options are given, print all (boolean, numeric, string)",
	"capabilities for the given terminal, using short names.",
	"",
	"Options:",
	" -a       try all names, print capabilities found",
	" -b       print boolean-capabilities",
	" -d LIST  colon-separated list of databases to use",
	" -e NAME  environment variable to set with -d option",
	" -n       print numeric-capabilities",
	" -q       quiet (prints only counts)",
	" -r COUNT repeat for given count",
	" -s       print string-capabilities",
#ifdef NCURSES_VERSION
	" -y       disable extended capabilities",
#endif
    };
    unsigned n;
    for (n = 0; n < SIZEOF(msg); ++n) {
	fprintf(stderr, "%s\n", msg[n]);
    }
    ExitProgram(EXIT_FAILURE);
}
#endif

int
main(int argc, char *argv[])
{
    int n;
    char *name;
    bool a_opt = FALSE;

#if USE_CODE_LISTS
    int repeat;
    int r_opt = 1;

    while ((n = getopt(argc, argv, "abd:e:nqr:sy")) != -1) {
	switch (n) {
	case 'a':
	    a_opt = TRUE;
	    break;
	case 'b':
	    b_opt = TRUE;
	    break;
	case 'd':
	    d_opt = optarg;
	    break;
	case 'e':
	    e_opt = optarg;
	    break;
	case 'n':
	    n_opt = TRUE;
	    break;
	case 'q':
	    q_opt = TRUE;
	    break;
	case 'r':
	    if ((r_opt = atoi(optarg)) <= 0)
		usage();
	    break;
	case 's':
	    s_opt = TRUE;
	    break;
#if NCURSES_XNAMES
	case 'y':
	    use_extended_names(FALSE);
	    break;
#endif
	default:
	    usage();
	    break;
	}
    }

    if (!(b_opt || n_opt || s_opt)) {
	b_opt = TRUE;
	n_opt = TRUE;
	s_opt = TRUE;
    }
#else
    a_opt = TRUE;
#endif

    make_dblist();

    if (a_opt) {
	if (optind < argc) {
	    for (n = optind; n < argc; ++n) {
		brute_force(argv[n]);
	    }
	} else if ((name = getenv("TERM")) != 0) {
	    brute_force(name);
	} else {
	    static char dumb[] = "dumb";
	    brute_force(dumb);
	}
    }
#if USE_CODE_LISTS
    else {
	for (repeat = 0; repeat < r_opt; ++repeat) {
	    if (optind < argc) {
		for (n = optind; n < argc; ++n) {
		    demo_termcap(argv[n]);
		}
	    } else if ((name = getenv("TERM")) != 0) {
		demo_termcap(name);
	    } else {
		static char dumb[] = "dumb";
		demo_termcap(dumb);
	    }
	}
    }
#endif /* USE_CODE_LISTS */

    printf("%ld values\n", total_values);

    free_dblist();

    ExitProgram(EXIT_SUCCESS);
}

#else
int
main(int argc GCC_UNUSED,
     char *argv[]GCC_UNUSED)
{
    printf("This program requires termcap\n");
    ExitProgram(EXIT_FAILURE);
}
#endif
