/*
 * view.c -- a silly little viewer program
 *
 * written by Eric S. Raymond <esr@snark.thyrsus.com> December 1994
 * to test the scrolling code in ncurses.
 *
 * modified by Thomas Dickey <dickey@clark.net> July 1995 to demonstrate
 * the use of 'resizeterm()', and May 2000 to illustrate wide-character
 * handling.
 *
 * Takes a filename argument.  It's a simple file-viewer with various
 * scroll-up and scroll-down commands.
 *
 * n	-- scroll one line forward
 * p	-- scroll one line back
 *
 * Either command accepts a numeric prefix interpreted as a repeat count.
 * Thus, typing `5n' should scroll forward 5 lines in the file.
 *
 * The way you can tell this is working OK is that, in the trace file,
 * there should be one scroll operation plus a small number of line
 * updates, as opposed to a whole-page update.  This means the physical
 * scroll operation worked, and the refresh() code only had to do a
 * partial repaint.
 *
 * $Id: view.c,v 1.29 2000/05/21 01:43:03 tom Exp $
 */

#include <test.priv.h>

#include <string.h>
#include <ctype.h>
#include <signal.h>

#if HAVE_TERMIOS_H
# include <termios.h>
#else
# include <sgtty.h>
#endif

#if !defined(sun) || !HAVE_TERMIOS_H
# if HAVE_SYS_IOCTL_H
#  include <sys/ioctl.h>
# endif
#endif

/* This is needed to compile 'struct winsize' */
#if NEED_PTEM_H
#include <sys/stream.h>
#include <sys/ptem.h>
#endif

static RETSIGTYPE finish(int sig) GCC_NORETURN;
static void show_all(void);

#if defined(SIGWINCH) && defined(TIOCGWINSZ) && defined(HAVE_RESIZETERM)
#define CAN_RESIZE 1
#else
#define CAN_RESIZE 0
#endif

#if CAN_RESIZE
static RETSIGTYPE adjust(int sig);
static int interrupted;
#endif

static int waiting;
static int shift;
static int utf8_mode = FALSE;

static char *fname;
static chtype **lines;
static chtype **lptr;

static void
usage(void)
{
    static const char *msg[] =
    {
	"Usage: view [options] file"
	,""
	,"Options:"
	," -n NUM   specify maximum number of lines (default 1000)"
#if defined(KEY_RESIZE)
	," -r       use experimental KEY_RESIZE rather than our own handler"
#endif
#ifdef TRACE
	," -t       trace screen updates"
	," -T NUM   specify trace mask"
#endif
	," -u       translate UTF-8 data"
    };
    size_t n;
    for (n = 0; n < SIZEOF(msg); n++)
	fprintf(stderr, "%s\n", msg[n]);
    exit(EXIT_FAILURE);
}

static int
ch_len(chtype * src)
{
    int result = 0;
    while (*src++)
	result++;
    return result;
}

/*
 * Allocate a string into an array of chtype's.  If UTF-8 mode is
 * active, translate the string accordingly.
 */
static chtype *
ch_dup(char *src)
{
    unsigned len = strlen(src);
    chtype *dst = typeMalloc(chtype, len + 1);
    unsigned j, k;
    unsigned utf_count = 0;
    unsigned utf_char = 0;

#define UCS_REPL 0xfffd

    for (j = k = 0; j < len; j++) {
	if (utf8_mode) {
	    unsigned c = src[j] & 0xff;
	    /* Combine UTF-8 into Unicode */
	    if (c < 0x80) {
		/* We received an ASCII character */
		if (utf_count > 0)
		    dst[k++] = UCS_REPL;	/* prev. sequence incomplete */
		dst[k++] = c;
		utf_count = 0;
	    } else if (c < 0xc0) {
		/* We received a continuation byte */
		if (utf_count < 1) {
		    dst[k++] = UCS_REPL;	/* ... unexpectedly */
		} else {
		    if (!utf_char && !((c & 0x7f) >> (7 - utf_count))) {
			utf_char = UCS_REPL;
		    }
		    /* characters outside UCS-2 become UCS_REPL */
		    if (utf_char > 0x03ff) {
			/* value would be >0xffff */
			utf_char = UCS_REPL;
		    } else {
			utf_char <<= 6;
			utf_char |= (c & 0x3f);
		    }
		    utf_count--;
		    if (utf_count == 0)
			dst[k++] = utf_char;
		}
	    } else {
		/* We received a sequence start byte */
		if (utf_count > 0)
		    dst[k++] = UCS_REPL;	/* prev. sequence incomplete */
		if (c < 0xe0) {
		    utf_count = 1;
		    utf_char = (c & 0x1f);
		    if (!(c & 0x1e))
			utf_char = UCS_REPL;	/* overlong sequence */
		} else if (c < 0xf0) {
		    utf_count = 2;
		    utf_char = (c & 0x0f);
		} else if (c < 0xf8) {
		    utf_count = 3;
		    utf_char = (c & 0x07);
		} else if (c < 0xfc) {
		    utf_count = 4;
		    utf_char = (c & 0x03);
		} else if (c < 0xfe) {
		    utf_count = 5;
		    utf_char = (c & 0x01);
		} else {
		    dst[k++] = UCS_REPL;
		    utf_count = 0;
		}
	    }
	} else {
	    dst[k++] = src[j];
	}
    }
    dst[k] = 0;
    return dst;
}

int
main(int argc, char *argv[])
{
    int MAXLINES = 1000;
    FILE *fp;
    char buf[BUFSIZ];
    int i;
    chtype **olptr;
    int done = FALSE;
    int length = 0;
#if CAN_RESIZE
    bool use_resize = TRUE;
#endif

    while ((i = getopt(argc, argv, "n:rtT:u")) != EOF) {
	switch (i) {
	case 'n':
	    if ((MAXLINES = atoi(optarg)) < 1)
		usage();
	    break;
#if CAN_RESIZE
	case 'r':
	    use_resize = FALSE;
	    break;
#endif
#ifdef TRACE
	case 'T':
	    trace(atoi(optarg));
	    break;
	case 't':
	    trace(TRACE_CALLS);
	    break;
#endif
	case 'u':
	    utf8_mode = TRUE;
	    break;
	default:
	    usage();
	}
    }
    if (optind + 1 != argc)
	usage();

    if ((lines = typeMalloc(chtype *, MAXLINES + 2)) == 0)
	usage();

    fname = argv[optind];
    if ((fp = fopen(fname, "r")) == 0) {
	perror(fname);
	return EXIT_FAILURE;
    }

    (void) signal(SIGINT, finish);	/* arrange interrupts to terminate */
#if CAN_RESIZE
    if (use_resize)
	(void) signal(SIGWINCH, adjust);	/* arrange interrupts to resize */
#endif

    /* slurp the file */
    for (lptr = &lines[0]; (lptr - lines) < MAXLINES; lptr++) {
	char temp[BUFSIZ], *s, *d;
	int col;

	if (fgets(buf, sizeof(buf), fp) == 0)
	    break;

	/* convert tabs so that shift will work properly */
	for (s = buf, d = temp, col = 0; (*d = *s) != '\0'; s++) {
	    if (*d == '\n') {
		*d = '\0';
		break;
	    } else if (*d == '\t') {
		col = (col | 7) + 1;
		while ((d - temp) != col)
		    *d++ = ' ';
	    } else if (isprint(*d) || utf8_mode) {
		col++;
		d++;
	    } else {
		sprintf(d, "\\%03o", *s & 0xff);
		d += strlen(d);
		col = (d - temp);
	    }
	}
	*lptr = ch_dup(temp);
    }
    (void) fclose(fp);
    length = lptr - lines;

    (void) initscr();		/* initialize the curses library */
    keypad(stdscr, TRUE);	/* enable keyboard mapping */
    (void) nonl();		/* tell curses not to do NL->CR/NL on output */
    (void) cbreak();		/* take input chars one at a time, no wait for \n */
    (void) noecho();		/* don't echo input */
    idlok(stdscr, TRUE);	/* allow use of insert/delete line */

    lptr = lines;
    while (!done) {
	int n, c;
	bool got_number;

	show_all();

	got_number = FALSE;
	n = 0;
	for (;;) {
#if CAN_RESIZE
	    if (interrupted)
		adjust(0);
#endif
	    waiting = TRUE;
	    c = getch();
	    waiting = FALSE;
	    if ((c < 127) && isdigit(c)) {
		if (!got_number) {
		    mvprintw(0, 0, "Count: ");
		    clrtoeol();
		}
		addch(c);
		n = 10 * n + (c - '0');
		got_number = TRUE;
	    } else
		break;
	}
	if (!got_number && n == 0)
	    n = 1;

	switch (c) {
	case KEY_DOWN:
	case 'n':
	    olptr = lptr;
	    for (i = 0; i < n; i++)
		if ((lptr - lines) < (length - LINES + 1))
		    lptr++;
		else
		    break;
	    wscrl(stdscr, lptr - olptr);
	    break;

	case KEY_UP:
	case 'p':
	    olptr = lptr;
	    for (i = 0; i < n; i++)
		if (lptr > lines)
		    lptr--;
		else
		    break;
	    wscrl(stdscr, lptr - olptr);
	    break;

	case 'h':
	case KEY_HOME:
	    lptr = lines;
	    break;

	case 'e':
	case KEY_END:
	    if (length > LINES)
		lptr = lines + length - LINES + 1;
	    else
		lptr = lines;
	    break;

	case 'r':
	case KEY_RIGHT:
	    shift++;
	    break;

	case 'l':
	case KEY_LEFT:
	    if (shift)
		shift--;
	    else
		beep();
	    break;

	case 'q':
	    done = TRUE;
	    break;

#ifdef KEY_RESIZE
	case KEY_RESIZE:	/* ignore this; ncurses will repaint */
	    break;
#endif
#if CAN_RESIZE
	case ERR:
	    break;
#endif
	default:
	    beep();
	}
    }

    finish(0);			/* we're done */
}

static RETSIGTYPE
finish(int sig)
{
    endwin();
    exit(sig != 0 ? EXIT_FAILURE : EXIT_SUCCESS);
}

#if CAN_RESIZE
/*
 * This uses functions that are "unsafe", but it seems to work on SunOS and
 * Linux.  The 'wrefresh(curscr)' is needed to force the refresh to start from
 * the top of the screen -- some xterms mangle the bitmap while resizing.
 */
static RETSIGTYPE
adjust(int sig)
{
    if (waiting || sig == 0) {
	struct winsize size;

	if (ioctl(fileno(stdout), TIOCGWINSZ, &size) == 0) {
	    resizeterm(size.ws_row, size.ws_col);
	    wrefresh(curscr);	/* Linux needs this */
	    show_all();
	}
	interrupted = FALSE;
    } else {
	interrupted = TRUE;
    }
    (void) signal(SIGWINCH, adjust);	/* some systems need this */
}
#endif /* CAN_RESIZE */

static void
show_all(void)
{
    int i;
    char temp[BUFSIZ];
    chtype *s;

#if CAN_RESIZE
    sprintf(temp, "(%3dx%3d) col %d ", LINES, COLS, shift);
    i = strlen(temp);
    sprintf(temp + i, "view %.*s", (int) (sizeof(temp) - 7 - i), fname);
#else
    sprintf(temp, "view %.*s", (int) sizeof(temp) - 7, fname);
#endif
    move(0, 0);
    printw("%.*s", COLS, temp);
    clrtoeol();

    scrollok(stdscr, FALSE);	/* prevent screen from moving */
    for (i = 1; i < LINES; i++) {
	move(i, 0);
	printw("%3d:", (lptr + i - lines));
	clrtoeol();
	if ((s = lptr[i - 1]) != 0) {
	    int len = ch_len(s);
	    if (len > shift)
		addchstr(s + shift);
	}
    }
    setscrreg(1, LINES - 1);
    scrollok(stdscr, TRUE);
    refresh();
}
