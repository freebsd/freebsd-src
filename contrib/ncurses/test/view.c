/*
 * view.c -- a silly little viewer program
 *
 * written by Eric S. Raymond <esr@snark.thyrsus.com> December 1994
 * to test the scrolling code in ncurses.
 *
 * modified by Thomas Dickey <dickey@clark.net> July 1995 to demonstrate
 * the use of 'resizeterm()'.
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
 * $Id: view.c,v 1.27 1998/08/22 18:33:41 tom Exp $
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

#if defined(SIGWINCH) && defined(TIOCGWINSZ) && defined(NCURSES_VERSION)
#define CAN_RESIZE 1
#else
#define CAN_RESIZE 0
#endif

#if CAN_RESIZE
static RETSIGTYPE adjust(int sig);
static int          interrupted;
#endif

static int          waiting;
static int          shift;

static char        *fname;
static char        **lines;
static char        **lptr;

#if !HAVE_STRDUP
#define strdup my_strdup
static char *strdup (char *s)
{
  char *p;

  p = malloc(strlen(s)+1);
  if (p)
    strcpy(p,s);
  return(p);
}
#endif /* not HAVE_STRDUP */

static void usage(void)
{
    static const char *msg[] = {
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
    };
    size_t n;
    for (n = 0; n < SIZEOF(msg); n++)
	fprintf(stderr, "%s\n", msg[n]);
    exit (EXIT_FAILURE);
}

int main(int argc, char *argv[])
{
int         MAXLINES = 1000;
FILE        *fp;
char        buf[BUFSIZ];
int         i;
char        **olptr;
int         done = FALSE;
int         length = 0;
#if CAN_RESIZE
bool        use_resize = TRUE;
#endif

    while ((i = getopt(argc, argv, "n:rtT:")) != EOF) {
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
	default:
	    usage();
	}
    }
    if (optind + 1 != argc)
	usage();

    if ((lines = (char **)calloc(MAXLINES+2, sizeof(*lines))) == 0)
	usage();

    fname = argv[optind];
    if ((fp = fopen(fname, "r")) == 0) {
	perror(fname);
	return EXIT_FAILURE;
    }

    (void) signal(SIGINT, finish);      /* arrange interrupts to terminate */
#if CAN_RESIZE
    if (use_resize)
	(void) signal(SIGWINCH, adjust); /* arrange interrupts to resize */
#endif

    /* slurp the file */
    for (lptr = &lines[0]; (lptr - lines) < MAXLINES; lptr++) {
	char temp[BUFSIZ], *s, *d;
	int  col;

	if (fgets(buf, sizeof(buf), fp) == 0)
	    break;

	/* convert tabs so that shift will work properly */
	for (s = buf, d = temp, col = 0; (*d = *s) != '\0'; s++) {
	    if (*d == '\n') {
		*d = '\0';
		break;
	    } else if (*d == '\t') {
		col = (col | 7) + 1;
		while ((d-temp) != col)
		    *d++ = ' ';
	    } else if (isprint(*d)) {
		col++;
		d++;
	    } else {
		sprintf(d, "\\%03o", *s & 0xff);
		d += strlen(d);
		col = (d - temp);
	    }
	}
	*lptr = strdup(temp);
    }
    (void) fclose(fp);
    length = lptr - lines;

    (void) initscr();      /* initialize the curses library */
    keypad(stdscr, TRUE);  /* enable keyboard mapping */
    (void) nonl();         /* tell curses not to do NL->CR/NL on output */
    (void) cbreak();       /* take input chars one at a time, no wait for \n */
    (void) noecho();       /* don't echo input */
    idlok(stdscr, TRUE);   /* allow use of insert/delete line */

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
		    mvprintw(0,0, "Count: ");
		    clrtoeol();
		}
		addch(c);
		n = 10 * n + (c - '0');
		got_number = TRUE;
	    }
	    else
		break;
	}
	if (!got_number && n == 0)
	    n = 1;

	switch(c) {
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
	case KEY_RESIZE: 	/* ignore this; ncurses will repaint */
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

static RETSIGTYPE finish(int sig)
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
static RETSIGTYPE adjust(int sig)
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
#endif	/* CAN_RESIZE */

static void show_all(void)
{
	int i;
	char temp[BUFSIZ];
	char *s;

#if CAN_RESIZE
	sprintf(temp, "(%3dx%3d) col %d ", LINES, COLS, shift);
	i = strlen(temp);
	sprintf(temp+i, "view %.*s", (int)(sizeof(temp)-7-i), fname);
#else
	sprintf(temp, "view %.*s", (int)sizeof(temp)-7, fname);
#endif
	move(0,0);
	printw("%.*s", COLS, temp);
	clrtoeol();

	scrollok(stdscr, FALSE); /* prevent screen from moving */
	for (i = 1; i < LINES; i++) {
	    move(i, 0);
	    if ((s = lptr[i-1]) != 0 && (int)strlen(s) > shift)
		printw("%3ld:%.*s", (long) (lptr+i-lines), COLS-4, s + shift);
	    else
		printw("%3ld:", (long) (lptr+i-lines));
	    clrtoeol();
	}
	setscrreg(1, LINES-1);
	scrollok(stdscr, TRUE);
	refresh();
}

/* view.c ends here */

