/*-
 * Copyright (c) 1988 The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
static char sccsid[] = "@(#)hack.tty.c	5.3 (Berkeley) 5/13/91";
#endif /* not lint */

/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* hack.tty.c - version 1.0.3 */
/* With thanks to the people who sent code for SYSV - hpscdi!jon,
   arnold@ucsf-cgl, wcs@bo95b, cbcephus!pds and others. */

#include	"hack.h"
#include	<stdio.h>

/*
 * The distinctions here are not BSD - rest but rather USG - rest, as
 * BSD still has the old sgttyb structure, but SYSV has termio. Thus:
 */
#ifdef BSD
#define	V7
#else
#define USG
#endif BSD

/*
 * Some systems may have getchar() return EOF for various reasons, and
 * we should not quit before seeing at least NR_OF_EOFS consecutive EOFs.
 */
#ifndef BSD
#define	NR_OF_EOFS	20
#endif BSD


#ifdef USG

#include	<termio.h>
#define termstruct	termio
#define kill_sym	c_cc[VKILL]
#define erase_sym	c_cc[VERASE]
#define EXTABS		TAB3
#define tabflgs		c_oflag
#define echoflgs	c_lflag
#define cbrkflgs	c_lflag
#define CBRKMASK	ICANON
#define CBRKON		! /* reverse condition */
#define OSPEED(x)	((x).c_cflag & CBAUD)
#define GTTY(x)		(ioctl(0, TCGETA, x))
#define STTY(x)		(ioctl(0, TCSETA, x))	/* TCSETAF? TCSETAW? */

#else	/* V7 */

#include	<sgtty.h>
#define termstruct	sgttyb
#define	kill_sym	sg_kill
#define	erase_sym	sg_erase
#define EXTABS		XTABS
#define tabflgs		sg_flags
#define echoflgs	sg_flags
#define cbrkflgs	sg_flags
#define CBRKMASK	CBREAK
#define CBRKON		/* empty */
#define OSPEED(x)	(x).sg_ospeed
#define GTTY(x)		(gtty(0, x))
#define STTY(x)		(stty(0, x))

#endif USG

extern short ospeed;
static char erase_char, kill_char;
static boolean settty_needed = FALSE;
struct termstruct inittyb, curttyb;

/*
 * Get initial state of terminal, set ospeed (for termcap routines)
 * and switch off tab expansion if necessary.
 * Called by startup() in termcap.c and after returning from ! or ^Z
 */
gettty(){
	if(GTTY(&inittyb) < 0)
		perror("Hack (gettty)");
	curttyb = inittyb;
	ospeed = OSPEED(inittyb);
	erase_char = inittyb.erase_sym;
	kill_char = inittyb.kill_sym;
	getioctls();

	/* do not expand tabs - they might be needed inside a cm sequence */
	if(curttyb.tabflgs & EXTABS) {
		curttyb.tabflgs &= ~EXTABS;
		setctty();
	}
	settty_needed = TRUE;
}

/* reset terminal to original state */
settty(s) char *s; {
	clear_screen();
	end_screen();
	if(s) printf(s);
	(void) fflush(stdout);
	if(STTY(&inittyb) < 0)
		perror("Hack (settty)");
	flags.echo = (inittyb.echoflgs & ECHO) ? ON : OFF;
	flags.cbreak = (CBRKON(inittyb.cbrkflgs & CBRKMASK)) ? ON : OFF;
	setioctls();
}

setctty(){
	if(STTY(&curttyb) < 0)
		perror("Hack (setctty)");
}


setftty(){
register int ef = 0;			/* desired value of flags & ECHO */
register int cf = CBRKON(CBRKMASK);	/* desired value of flags & CBREAK */
register int change = 0;
	flags.cbreak = ON;
	flags.echo = OFF;
	/* Should use (ECHO|CRMOD) here instead of ECHO */
	if((curttyb.echoflgs & ECHO) != ef){
		curttyb.echoflgs &= ~ECHO;
/*		curttyb.echoflgs |= ef;					*/
		change++;
	}
	if((curttyb.cbrkflgs & CBRKMASK) != cf){
		curttyb.cbrkflgs &= ~CBRKMASK;
		curttyb.cbrkflgs |= cf;
#ifdef USG
		/* be satisfied with one character; no timeout */
		curttyb.c_cc[VMIN] = 1;		/* was VEOF */
		curttyb.c_cc[VTIME] = 0;	/* was VEOL */
#endif USG
		change++;
	}
	if(change){
		setctty();
	}
	start_screen();
}


/* fatal error */
/*VARARGS1*/
error(s,x,y) char *s; {
	if(settty_needed)
		settty((char *) 0);
	printf(s,x,y);
	putchar('\n');
	exit(1);
}

/*
 * Read a line closed with '\n' into the array char bufp[BUFSZ].
 * (The '\n' is not stored. The string is closed with a '\0'.)
 * Reading can be interrupted by an escape ('\033') - now the
 * resulting string is "\033".
 */
getlin(bufp)
register char *bufp;
{
	register char *obufp = bufp;
	register int c;

	flags.toplin = 2;		/* nonempty, no --More-- required */
	for(;;) {
		(void) fflush(stdout);
		if((c = getchar()) == EOF) {
			*bufp = 0;
			return;
		}
		if(c == '\033') {
			*obufp = c;
			obufp[1] = 0;
			return;
		}
		if(c == erase_char || c == '\b') {
			if(bufp != obufp) {
				bufp--;
				putstr("\b \b"); /* putsym converts \b */
			} else	bell();
		} else if(c == '\n') {
			*bufp = 0;
			return;
		} else if(' ' <= c && c < '\177') {
				/* avoid isprint() - some people don't have it
				   ' ' is not always a printing char */
			*bufp = c;
			bufp[1] = 0;
			putstr(bufp);
			if(bufp-obufp < BUFSZ-1 && bufp-obufp < COLNO)
				bufp++;
		} else if(c == kill_char || c == '\177') { /* Robert Viduya */
				/* this test last - @ might be the kill_char */
			while(bufp != obufp) {
				bufp--;
				putstr("\b \b");
			}
		} else
			bell();
	}
}

getret() {
	cgetret("");
}

cgetret(s)
register char *s;
{
	putsym('\n');
	if(flags.standout)
		standoutbeg();
	putstr("Hit ");
	putstr(flags.cbreak ? "space" : "return");
	putstr(" to continue: ");
	if(flags.standout)
		standoutend();
	xwaitforspace(s);
}

char morc;	/* tell the outside world what char he used */

xwaitforspace(s)
register char *s;	/* chars allowed besides space or return */
{
register int c;

	morc = 0;

	while((c = readchar()) != '\n') {
	    if(flags.cbreak) {
		if(c == ' ') break;
		if(s && index(s,c)) {
			morc = c;
			break;
		}
		bell();
	    }
	}
}

char *
parse()
{
	static char inputline[COLNO];
	register foo;

	flags.move = 1;
	if(!Invisible) curs_on_u(); else home();
	while((foo = readchar()) >= '0' && foo <= '9')
		multi = 10*multi+foo-'0';
	if(multi) {
		multi--;
		save_cm = inputline;
	}
	inputline[0] = foo;
	inputline[1] = 0;
	if(foo == 'f' || foo == 'F'){
		inputline[1] = getchar();
#ifdef QUEST
		if(inputline[1] == foo) inputline[2] = getchar(); else
#endif QUEST
		inputline[2] = 0;
	}
	if(foo == 'm' || foo == 'M'){
		inputline[1] = getchar();
		inputline[2] = 0;
	}
	clrlin();
	return(inputline);
}

char
readchar() {
	register int sym;

	(void) fflush(stdout);
	if((sym = getchar()) == EOF)
#ifdef NR_OF_EOFS
	{ /*
	   * Some SYSV systems seem to return EOFs for various reasons
	   * (?like when one hits break or for interrupted systemcalls?),
	   * and we must see several before we quit.
	   */
		register int cnt = NR_OF_EOFS;
		while (cnt--) {
		    clearerr(stdin);	/* omit if clearerr is undefined */
		    if((sym = getchar()) != EOF) goto noteof;
		}
		end_of_input();
	     noteof:	;
	}
#else
		end_of_input();
#endif NR_OF_EOFS
	if(flags.toplin == 1)
		flags.toplin = 2;
	return((char) sym);
}

end_of_input()
{
	settty("End of input?\n");
	clearlocks();
	exit(0);
}
