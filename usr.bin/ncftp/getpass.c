/* Getpass.c */

/*  $RCSfile: getpass.c,v $
 *  $Revision: 14020.11 $
 *  $Date: 93/05/21 05:44:36 $
 */

#include "sys.h"

#include <signal.h>

#include "util.h"
#include "cmds.h"
#include "getpass.h"
#include "copyright.h"

#ifndef GETPASS

#ifndef sun	/* ...both unnecessary, and conflicting with <termios.h> */
#include <sys/ioctl.h>
#endif

#ifdef TERMIOS
#		include <termios.h>
#else
#	ifdef SGTTYB
#		include <sgtty.h>
#	else
#		include <termio.h>
#	endif
#endif /* !TERMIOS */

#ifdef STRICT_PROTOS
int ioctl(int, int, ...);
#endif

#endif	/* GETPASS */




void Echo(FILE *fp, int on)
{
#ifndef GETPASS		/* Otherwise just do nothing which is ok. */

#ifdef TERMIOS
	static struct termios orig, noecho, *tp;
#else
#	ifdef SGTTYB
	static struct sgttyb orig, noecho, *tp;
#	else
	static struct termio orig, noecho, *tp;
#	endif
#endif
	static int state = 0;
	int fd = fileno(fp);
	
	if (!isatty(fd))
		return;

	if (state == 0) {
#ifdef TERMIOS
		if (tcgetattr(fd, &orig) < 0)
			PERROR("echo", "tcgetattr");
		noecho = orig;
		noecho.c_lflag &= ~ECHO;
#else
#	ifdef SGTTYB
		if (ioctl(fd, TIOCGETP, &orig) < 0)
			PERROR("echo", "ioctl");
		noecho = orig;
		noecho.sg_flags &= ~ECHO;
#	else
		if (ioctl(fd, TCGETA, &orig) < 0)
			PERROR("echo", "ioctl");
		noecho = orig;
		noecho.c_lflag &= ~ECHO;
#	endif
#endif
		state = 1;
	}
	tp = NULL;
	if (on && state == 2) {
		/* Turn echo back on. */
		tp = &orig;
		state = 1;
	} else if (!on && state == 1) {
		/* Turn echo off. */
		tp = &noecho;
		state = 2;
	}
	if (tp != NULL) {
#ifdef TERMIOS
		if (tcsetattr(fd, TCSANOW, tp) < 0)
			PERROR("echo", "tcsetattr");
#else
#	ifdef SGTTYB
		if (ioctl(fd, TIOCSETP, tp) < 0)
			PERROR("echo", "ioctl");
#	else
		if (ioctl(fd, TCSETA, tp) < 0)
			PERROR("echo", "ioctl");
#	endif
#endif	/* !TERMIOS */
	}

#endif	/* GETPASS */
}	/* Echo */



#ifndef GETPASS

char *Getpass(char *promptstr)
{
	register int ch;
	register char *p;
	FILE *fp, *outfp;
	Sig_t oldintr;
	static char buf[kMaxPassLen + 1];

	/*
	 * read and write to /dev/tty if possible; else read from
	 * stdin and write to stderr.
	 */
#if !defined(BOTCHED_FOPEN_RW)
  	if ((outfp = fp = fopen("/dev/tty", "w+")) == NULL) {
  		outfp = stderr;
  		fp = stdin;
  	}
#else
	/* SCO 32v2 botches "w+" open */
	if ((fp = fopen("/dev/tty", "r")) == NULL)
		fp = stdin;
	if ((outfp = fopen("/dev/tty", "w")) == NULL)
		outfp = stderr;
#endif
	oldintr = Signal(SIGINT, SIG_IGN);
	Echo(fp, 0);		/* Turn echoing off. */
	(void) fputs(promptstr, outfp);
	(void) rewind(outfp);			/* implied flush */
	for (p = buf; (ch = getc(fp)) != EOF && ch != '\n';)
		if (p < buf + kMaxPassLen)
			*p++ = ch;
	*p = '\0';
	(void)write(fileno(outfp), "\n", 1);
	Echo(fp, 1);
	(void) Signal(SIGINT, oldintr);
	if (fp != stdin)
		(void)fclose(fp);
#if defined(BOTCHED_FOPEN_RW)
	if (outfp != stderr)
		(void)fclose(outfp);
#endif
	return(buf);
}	/* Getpass */

#endif /* GETPASS */

/* eof Getpass.c */
