/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* hack.pager.c - version 1.0.3 */
/* $FreeBSD: src/games/hack/hack.pager.c,v 1.7 1999/11/16 02:57:09 billf Exp $ */

/* This file contains the command routine dowhatis() and a pager. */
/* Also readmail() and doshell(), and generally the things that
   contact the outside world. */

#include	<sys/types.h>
#include	<sys/signal.h>
#include	<stdio.h>
#include        <stdlib.h>
#include        <unistd.h>
#include "hack.h"
extern int CO, LI;	/* usually COLNO and ROWNO+2 */
extern char *CD;
extern char quitchars[];
void done1();

dowhatis()
{
	FILE *fp;
	char bufr[BUFSZ+6];
	char *buf = &bufr[6], *ep, q;
	extern char readchar();

	if(!(fp = fopen(DATAFILE, "r")))
		pline("Cannot open data file!");
	else {
		pline("Specify what? ");
		q = readchar();
		if(q != '\t')
		while(fgets(buf,BUFSZ,fp))
		    if(*buf == q) {
			ep = index(buf, '\n');
			if(ep) *ep = 0;
			/* else: bad data file */
			/* Expand tab 'by hand' */
			if(buf[1] == '\t'){
				buf = bufr;
				buf[0] = q;
				(void) strncpy(buf+1, "       ", 7);
			}
			pline(buf);
			if(ep[-1] == ';') {
				pline("More info? ");
				if(readchar() == 'y') {
					page_more(fp,1); /* does fclose() */
					return(0);
				}
			}
			(void) fclose(fp); 	/* kopper@psuvax1 */
			return(0);
		    }
		pline("I've never heard of such things.");
		(void) fclose(fp);
	}
	return(0);
}

/* make the paging of a file interruptible */
static int got_intrup;

void
intruph(){
	got_intrup++;
}

/* simple pager, also used from dohelp() */
page_more(fp,strip)
FILE *fp;
int strip;	/* nr of chars to be stripped from each line (0 or 1) */
{
	char *bufr, *ep;
	sig_t prevsig = signal(SIGINT, intruph);

	set_pager(0);
	bufr = (char *) alloc((unsigned) CO);
	bufr[CO-1] = 0;
	while(fgets(bufr,CO-1,fp) && (!strip || *bufr == '\t') && !got_intrup){
		ep = index(bufr, '\n');
		if(ep)
			*ep = 0;
		if(page_line(bufr+strip)) {
			set_pager(2);
			goto ret;
		}
	}
	set_pager(1);
ret:
	free(bufr);
	(void) fclose(fp);
	(void) signal(SIGINT, prevsig);
	got_intrup = 0;
}

static boolean whole_screen = TRUE;
#define	PAGMIN	12	/* minimum # of lines for page below level map */

set_whole_screen() {	/* called in termcap as soon as LI is known */
	whole_screen = (LI-ROWNO-2 <= PAGMIN || !CD);
}

#ifdef NEWS
readnews() {
	int ret;

	whole_screen = TRUE;	/* force a docrt(), our first */
	ret = page_file(NEWS, TRUE);
	set_whole_screen();
	return(ret);		/* report whether we did docrt() */
}
#endif NEWS

set_pager(mode)
int mode;	/* 0: open  1: wait+close  2: close */
{
	static boolean so;
	if(mode == 0) {
		if(!whole_screen) {
			/* clear topline */
			clrlin();
			/* use part of screen below level map */
			curs(1, ROWNO+4);
		} else {
			cls();
		}
		so = flags.standout;
		flags.standout = 1;
	} else {
		if(mode == 1) {
			curs(1, LI);
			more();
		}
		flags.standout = so;
		if(whole_screen)
			docrt();
		else {
			curs(1, ROWNO+4);
			cl_eos();
		}
	}
}

page_line(s)		/* returns 1 if we should quit */
char *s;
{
	extern char morc;

	if(cury == LI-1) {
		if(!*s)
			return(0);	/* suppress blank lines at top */
		putchar('\n');
		cury++;
		cmore("q\033");
		if(morc) {
			morc = 0;
			return(1);
		}
		if(whole_screen)
			cls();
		else {
			curs(1, ROWNO+4);
			cl_eos();
		}
	}
	puts(s);
	cury++;
	return(0);
}

/*
 * Flexible pager: feed it with a number of lines and it will decide
 * whether these should be fed to the pager above, or displayed in a
 * corner.
 * Call:
 *	cornline(0, title or 0)	: initialize
 *	cornline(1, text)	: add text to the chain of texts
 *	cornline(2, morcs)	: output everything and cleanup
 *	cornline(3, 0)		: cleanup
 */

cornline(mode, text)
int mode;
char *text;
{
	static struct line {
		struct line *next_line;
		char *line_text;
	} *texthead, *texttail;
	static int maxlen;
	static int linect;
	struct line *tl;

	if(mode == 0) {
		texthead = 0;
		maxlen = 0;
		linect = 0;
		if(text) {
			cornline(1, text);	/* title */
			cornline(1, "");	/* blank line */
		}
		return;
	}

	if(mode == 1) {
	    int len;

	    if(!text) return;	/* superfluous, just to be sure */
	    linect++;
	    len = strlen(text);
	    if(len > maxlen)
		maxlen = len;
	    tl = (struct line *)
		alloc((unsigned)(len + sizeof(struct line) + 1));
	    tl->next_line = 0;
	    tl->line_text = (char *)(tl + 1);
	    (void) strcpy(tl->line_text, text);
	    if(!texthead)
		texthead = tl;
	    else
		texttail->next_line = tl;
	    texttail = tl;
	    return;
	}

	/* --- now we really do it --- */
	if(mode == 2 && linect == 1)			    /* topline only */
		pline(texthead->line_text);
	else
	if(mode == 2) {
	    int curline, lth;

	    if(flags.toplin == 1) more();	/* ab@unido */
	    remember_topl();

	    lth = CO - maxlen - 2;		   /* Use full screen width */
	    if (linect < LI && lth >= 10) {		     /* in a corner */
		home ();
		cl_end ();
		flags.toplin = 0;
		curline = 1;
		for (tl = texthead; tl; tl = tl->next_line) {
		    curs (lth, curline);
		    if(curline > 1)
			cl_end ();
		    putsym(' ');
		    putstr (tl->line_text);
		    curline++;
		}
		curs (lth, curline);
		cl_end ();
		cmore (text);
		home ();
		cl_end ();
		docorner (lth, curline-1);
	    } else {					/* feed to pager */
		set_pager(0);
		for (tl = texthead; tl; tl = tl->next_line) {
		    if (page_line (tl->line_text)) {
			set_pager(2);
			goto cleanup;
		    }
		}
		if(text) {
			cgetret(text);
			set_pager(2);
		} else
			set_pager(1);
	    }
	}

cleanup:
	while(tl = texthead) {
		texthead = tl->next_line;
		free((char *) tl);
	}
}

dohelp()
{
	char c;

	pline ("Long or short help? ");
	while (((c = readchar ()) != 'l') && (c != 's') && !index(quitchars,c))
		bell ();
	if (!index(quitchars, c))
		(void) page_file((c == 'l') ? HELP : SHELP, FALSE);
	return(0);
}

page_file(fnam, silent)	/* return: 0 - cannot open fnam; 1 - otherwise */
char *fnam;
boolean silent;
{
#ifdef DEF_PAGER			/* this implies that UNIX is defined */
      {
	/* use external pager; this may give security problems */

	int fd = open(fnam, 0);

	if(fd < 0) {
		if(!silent) pline("Cannot open %s.", fnam);
		return(0);
	}
	if(child(1)){
		extern char *catmore;

		/* Now that child() does a setuid(getuid()) and a chdir(),
		   we may not be able to open file fnam anymore, so make
		   it stdin. */
		(void) close(0);
		if(dup(fd)) {
			if(!silent) printf("Cannot open %s as stdin.\n", fnam);
		} else {
			execl(catmore, "page", (char *) 0);
			if(!silent) printf("Cannot exec %s.\n", catmore);
		}
		exit(1);
	}
	(void) close(fd);
      }
#else DEF_PAGER
      {
	FILE *f;			/* free after Robert Viduya */

	if ((f = fopen (fnam, "r")) == (FILE *) 0) {
		if(!silent) {
			home(); perror (fnam); flags.toplin = 1;
			pline ("Cannot open %s.", fnam);
		}
		return(0);
	}
	page_more(f, 0);
      }
#endif DEF_PAGER

	return(1);
}

#ifdef UNIX
#ifdef SHELL
dosh(){
char *str;
	if(child(0)) {
		if(str = getenv("SHELL"))
			execl(str, str, (char *) 0);
		else
			execl("/bin/sh", "sh", (char *) 0);
		pline("sh: cannot execute.");
		exit(1);
	}
	return(0);
}
#endif SHELL

#ifdef NOWAITINCLUDE
union wait {		/* used only for the cast  (union wait *) 0  */
	int w_status;
	struct {
		unsigned short w_Termsig:7;
		unsigned short w_Coredump:1;
		unsigned short w_Retcode:8;
	} w_T;
};

#else

#ifdef BSD
#include	<sys/wait.h>
#else
#include	<wait.h>
#endif BSD
#endif NOWAITINCLUDE

child(wt) {
	int status;
	int f;

	f = fork();
	if(f == 0){		/* child */
		settty((char *) 0);		/* also calls end_screen() */
		/* revoke */
		setgid(getgid());
#ifdef CHDIR
		(void) chdir(getenv("HOME"));
#endif CHDIR
		return(1);
	}
	if(f == -1) {	/* cannot fork */
		pline("Fork failed. Try again.");
		return(0);
	}
	/* fork succeeded; wait for child to exit */
	(void) signal(SIGINT,SIG_IGN);
	(void) signal(SIGQUIT,SIG_IGN);
	(void) wait(&status);
	gettty();
	setftty();
	(void) signal(SIGINT,done1);
#ifdef WIZARD
	if(wizard) (void) signal(SIGQUIT,SIG_DFL);
#endif WIZARD
	if(wt) getret();
	docrt();
	return(0);
}
#endif UNIX
