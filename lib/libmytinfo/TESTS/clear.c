/*
 * clear.c
 *
 * By Ross Ridge
 * Public Domain
 * 92/02/01 07:29:47
 *
 * clear
 * 
 * clears the terminal's screen
 *
 */

#include "defs.h"

const char SCCSid[] = "@(#) mytinfo clear.c 3.2 92/02/01 public domain, By Ross Ridge";

#ifndef USE_TERMINFO

#ifdef USE_SGTTY
#include <sgtty.h>
#endif
#ifdef USE_TERMIO
#include <termio.h>
#endif
#ifdef USE_TERMIOS
#include <termios.h>
#endif
#ifdef __FreeBSD__
int	 tgetent __P((char *, char *));
int	 tgetnum __P((char *));
int      tputs __P((char *, int, int (*)(char)));
#endif

char PC;
short ospeed;

int
#ifdef USE_PROTOTYPES
putch(char c)
#else
putch(c)
int c;
#endif
{
	return putchar(c);
}

int
main(argc, argv)
int argc;
char **argv; {
#ifdef USE_PROTOTYPES
	char *tgetstr(char *, char **);
#else
	char *tgetstr();
#endif
	char *term;
	char buf[MAX_BUF];
	char CL[MAX_LINE];
	char *s, *pc;

#ifdef USE_SGTTY
	struct sgttyb tty;

	gtty(1, &tty);
	ospeed = tty.sg_ospeed;
#else
#ifdef USE_TERMIO
	struct termio tty;

	ioctl(1, TCGETA, &tty);
	ospeed = tty.c_cflag & CBAUD;
#else
#ifdef USE_TERMIOS
	struct termios tty;

	tcgetattr(1, &tty);
	switch(cfgetospeed(&tty)) {
		case B0: ospeed = 0; break;
		case B50: ospeed = 1; break;
		case B75: ospeed = 2; break;
		case B110: ospeed = 3; break;
		case B134: ospeed = 4; break;
		case B150: ospeed = 5; break;
		case B200: ospeed = 6; break;
		case B300: ospeed = 7; break;
		case B600: ospeed = 8; break;
		case B1200: ospeed = 9; break;
		case B1800: ospeed = 10; break;
		case B2400: ospeed = 11; break;
		case B4800: ospeed = 12; break;
		case B9600: ospeed = 13; break;
		case EXTA: ospeed = 14; break;
		case EXTB: ospeed = 15; break;
#ifdef B57600
		case B57600: ospeed = 16; break;
#endif
#ifdef B115200
		case B115200: ospeed = 17; break;
#endif
	}
#else
	ospeed = 0;
#endif
#endif
#endif

	term = getenv("TERM");
	if (term == NULL)
		exit(1);

	if (tgetent(buf, term) != 1)
		exit(1);

	s = CL;
	pc = tgetstr("pc", &s);
	if (pc != NULL)
		PC = *pc;

	s = CL;
	tgetstr("cl", &s);

	if (CL != NULL) {
		tputs(CL, tgetnum("li"), putch);
		exit(1);
	}

	return 0;
}

#else /* USE_TERMINFO */

#include <term.h>

int
#ifdef USE_PROTOTYPES
putch(char c)
#else
putch(c)
int c;
#endif
{
	return putchar(c);
}

int
main() {
	setupterm((char *) 0, 1, (int *) 0); 
	if (clear_screen == (char *) 0)
		exit(1);
	tputs(clear_screen, lines > 0 ? lines : 1, putch);
	return 0;
}

#endif /* USE_TERMINFO */
