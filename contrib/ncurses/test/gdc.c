/*
 * Grand digital clock for curses compatible terminals
 * Usage: gdc [-s] [n]   -- run for n seconds (default infinity)
 * Flags: -s: scroll
 *
 * modified 10-18-89 for curses (jrl)
 * 10-18-89 added signal handling
 *
 * $Id: gdc.c,v 1.10 1997/10/18 20:06:06 tom Exp $
 */

#include <test.priv.h>

#include <time.h>
#include <signal.h>
#include <string.h>

#define YBASE	10
#define XBASE	10
#define XLENGTH	54
#define YDEPTH	5

/* it won't be */
static time_t now; /* yeah! */
static struct tm *tm;

static short disp[11] = {
	075557, 011111, 071747, 071717, 055711,
	074717, 074757, 071111, 075757, 075717, 002020
};
static long older[6], next[6], newer[6], mask;
static char scrol;

static int sigtermed = 0;

static int hascolor = 0;

static void set(int, int);
static void standt(int);
static void movto(int, int);

static
RETSIGTYPE sighndl(int signo)
{
	signal(signo, sighndl);
	sigtermed=signo;
}

static void
drawbox(void)
{
	chtype bottom[XLENGTH+1];
	int n;

	if(hascolor)
		attrset(COLOR_PAIR(3));

	mvaddch(YBASE - 1,  XBASE - 1, ACS_ULCORNER);
	hline(ACS_HLINE, XLENGTH);
	mvaddch(YBASE - 1,  XBASE + XLENGTH, ACS_URCORNER);

	mvaddch(YBASE + YDEPTH,  XBASE - 1, ACS_LLCORNER);
	mvinchnstr(YBASE + YDEPTH, XBASE, bottom, XLENGTH);
	for (n = 0; n < XLENGTH; n++)
		bottom[n] = ACS_HLINE | (bottom[n] & (A_ATTRIBUTES | A_COLOR));
	mvaddchnstr(YBASE + YDEPTH, XBASE, bottom, XLENGTH);
	mvaddch(YBASE + YDEPTH,  XBASE + XLENGTH, ACS_LRCORNER);

	move(YBASE,  XBASE - 1);
	vline(ACS_VLINE, YDEPTH);

	move(YBASE,  XBASE + XLENGTH);
	vline(ACS_VLINE, YDEPTH);

	if(hascolor)
		attrset(COLOR_PAIR(2));
}

int
main(int argc, char *argv[])
{
long t, a;
int i, j, s, k;
int n = 0;

	signal(SIGINT,sighndl);
	signal(SIGTERM,sighndl);
	signal(SIGKILL,sighndl);

	initscr();
	cbreak();
	noecho();
	nodelay(stdscr, 1);
    	curs_set(0);

	hascolor = has_colors();

	if(hascolor) {
		int bg = COLOR_BLACK;
		start_color();
#ifdef NCURSES_VERSION
		if (use_default_colors() == OK)
			bg = -1;
#endif
		init_pair(1, COLOR_BLACK, COLOR_RED);
		init_pair(2, COLOR_RED,   bg);
		init_pair(3, COLOR_WHITE, bg);
		attrset(COLOR_PAIR(2));
	}

	clear();
	refresh();
	while(--argc > 0) {
		if(**++argv == '-')
			scrol = 1;
		else
			n = atoi(*argv);
	}

	drawbox();
	do {
		char	buf[30];

		mask = 0;
		time(&now);
		tm = localtime(&now);
		set(tm->tm_sec%10, 0);
		set(tm->tm_sec/10, 4);
		set(tm->tm_min%10, 10);
		set(tm->tm_min/10, 14);
		set(tm->tm_hour%10, 20);
		set(tm->tm_hour/10, 24);
		set(10, 7);
		set(10, 17);
		for(k=0; k<6; k++) {
			if(scrol) {
				for(i=0; i<5; i++)
					newer[i] = (newer[i]&~mask) | (newer[i+1]&mask);
				newer[5] = (newer[5]&~mask) | (next[k]&mask);
			} else
				newer[k] = (newer[k]&~mask) | (next[k]&mask);
			next[k] = 0;
			for(s=1; s>=0; s--) {
				standt(s);
				for(i=0; i<6; i++) {
					if((a = (newer[i]^older[i])&(s ? newer : older)[i]) != 0) {
						for(j=0,t=1<<26; t; t>>=1,j++) {
							if(a&t) {
								if(!(a&(t<<1))) {
									movto(YBASE + i, XBASE + 2*j);
								}
								addstr("  ");
							}
						}
					}
					if(!s) {
						older[i] = newer[i];
					}
				}
				if(!s) {
					if (scrol)
						drawbox();
					refresh();
					if (scrol)
						napms(150);
				}
			}
		}

		/* this depends on the detailed format of ctime(3) */
		(void) strcpy(buf, ctime(&now));
		(void) strcpy(buf + 10, buf + 19);
		mvaddstr(16, 30, buf);

		movto(6, 0);
		drawbox();
		refresh();
		sleep(1);
		while(wgetch(stdscr) != ERR)
			continue;
		if (sigtermed) {
			standend();
			clear();
			refresh();
    			curs_set(1);
			endwin();
			fprintf(stderr, "gdc terminated by signal %d\n", sigtermed);
			return EXIT_FAILURE;
		}
	} while(--n);
	standend();
	clear();
	refresh();
    	curs_set(1);
	endwin();
	return EXIT_SUCCESS;
}

static void
set(int t, int n)
{
int i, m;

	m = 7<<n;
	for(i=0; i<5; i++) {
		next[i] |= ((disp[t]>>(4-i)*3)&07)<<n;
		mask |= (next[i]^older[i])&m;
	}
	if(mask&m)
		mask |= m;
}

static void
standt(int on)
{
	if (on) {
		if(hascolor) {
			attron(COLOR_PAIR(1));
		} else {
			attron(A_STANDOUT);
		}
	} else {
		if(hascolor) {
			attron(COLOR_PAIR(2));
		} else {
			attroff(A_STANDOUT);
		}
	}
}

static void
movto(int line, int col)
{
	move(line, col);
}
