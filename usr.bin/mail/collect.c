/*
 * Copyright (c) 1980 Regents of the University of California.
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
static char sccsid[] = "@(#)collect.c	5.24 (Berkeley) 4/1/91";
#endif /* not lint */

/*
 * Mail -- a mail program
 *
 * Collect input from standard input, handling
 * ~ escapes.
 */

#include "rcv.h"
#include <sys/stat.h>

/*
 * Read a message from standard output and return a read file to it
 * or NULL on error.
 */

/*
 * The following hokiness with global variables is so that on
 * receipt of an interrupt signal, the partial message can be salted
 * away on dead.letter.
 */

static	sig_t	saveint;		/* Previous SIGINT value */
static	sig_t	savehup;		/* Previous SIGHUP value */
static	sig_t	savetstp;		/* Previous SIGTSTP value */
static	sig_t	savettou;		/* Previous SIGTTOU value */
static	sig_t	savettin;		/* Previous SIGTTIN value */
static	FILE	*collf;			/* File for saving away */
static	int	hadintr;		/* Have seen one SIGINT so far */

static	jmp_buf	colljmp;		/* To get back to work */
static	int	colljmp_p;		/* whether to long jump */
static	jmp_buf	collabort;		/* To end collection with error */

FILE *
collect(hp, printheaders)
	struct header *hp;
{
	FILE *fbuf;
	int lc, cc, escape, eofcount;
	register int c, t;
	char linebuf[LINESIZE], *cp;
	extern char tempMail[];
	char getsub;
	int omask;
	void collint(), collhup(), collstop();

	collf = NULL;
	/*
	 * Start catching signals from here, but we're still die on interrupts
	 * until we're in the main loop.
	 */
	omask = sigblock(sigmask(SIGINT) | sigmask(SIGHUP));
	if ((saveint = signal(SIGINT, SIG_IGN)) != SIG_IGN)
		signal(SIGINT, collint);
	if ((savehup = signal(SIGHUP, SIG_IGN)) != SIG_IGN)
		signal(SIGHUP, collhup);
	savetstp = signal(SIGTSTP, collstop);
	savettou = signal(SIGTTOU, collstop);
	savettin = signal(SIGTTIN, collstop);
	if (setjmp(collabort) || setjmp(colljmp)) {
		rm(tempMail);
		goto err;
	}
	sigsetmask(omask & ~(sigmask(SIGINT) | sigmask(SIGHUP)));

	noreset++;
	if ((collf = Fopen(tempMail, "w+")) == NULL) {
		perror(tempMail);
		goto err;
	}
	unlink(tempMail);

	/*
	 * If we are going to prompt for a subject,
	 * refrain from printing a newline after
	 * the headers (since some people mind).
	 */
	t = GTO|GSUBJECT|GCC|GNL;
	getsub = 0;
	if (hp->h_subject == NOSTR && value("interactive") != NOSTR &&
	    (value("ask") != NOSTR || value("asksub") != NOSTR))
		t &= ~GNL, getsub++;
	if (printheaders) {
		puthead(hp, stdout, t);
		fflush(stdout);
	}
	if ((cp = value("escape")) != NOSTR)
		escape = *cp;
	else
		escape = ESCAPE;
	eofcount = 0;
	hadintr = 0;

	if (!setjmp(colljmp)) {
		if (getsub)
			grabh(hp, GSUBJECT);
	} else {
		/*
		 * Come here for printing the after-signal message.
		 * Duplicate messages won't be printed because
		 * the write is aborted if we get a SIGTTOU.
		 */
cont:
		if (hadintr) {
			fflush(stdout);
			fprintf(stderr,
			"\n(Interrupt -- one more to kill letter)\n");
		} else {
			printf("(continue)\n");
			fflush(stdout);
		}
	}
	for (;;) {
		colljmp_p = 1;
		c = readline(stdin, linebuf, LINESIZE);
		colljmp_p = 0;
		if (c < 0) {
			if (value("interactive") != NOSTR &&
			    value("ignoreeof") != NOSTR && ++eofcount < 25) {
				printf("Use \".\" to terminate letter\n");
				continue;
			}
			break;
		}
		eofcount = 0;
		hadintr = 0;
		if (linebuf[0] == '.' && linebuf[1] == '\0' &&
		    value("interactive") != NOSTR &&
		    (value("dot") != NOSTR || value("ignoreeof") != NOSTR))
			break;
		if (linebuf[0] != escape || value("interactive") == NOSTR) {
			if (putline(collf, linebuf) < 0)
				goto err;
			continue;
		}
		c = linebuf[1];
		switch (c) {
		default:
			/*
			 * On double escape, just send the single one.
			 * Otherwise, it's an error.
			 */
			if (c == escape) {
				if (putline(collf, &linebuf[1]) < 0)
					goto err;
				else
					break;
			}
			printf("Unknown tilde escape.\n");
			break;
		case 'C':
			/*
			 * Dump core.
			 */
			core();
			break;
		case '!':
			/*
			 * Shell escape, send the balance of the
			 * line to sh -c.
			 */
			shell(&linebuf[2]);
			break;
		case ':':
			/*
			 * Escape to command mode, but be nice!
			 */
			execute(&linebuf[2], 1);
			goto cont;
		case '.':
			/*
			 * Simulate end of file on input.
			 */
			goto out;
		case 'q':
			/*
			 * Force a quit of sending mail.
			 * Act like an interrupt happened.
			 */
			hadintr++;
			collint(SIGINT);
			exit(1);
		case 'h':
			/*
			 * Grab a bunch of headers.
			 */
			grabh(hp, GTO|GSUBJECT|GCC|GBCC);
			goto cont;
		case 't':
			/*
			 * Add to the To list.
			 */
			hp->h_to = cat(hp->h_to, extract(&linebuf[2], GTO));
			break;
		case 's':
			/*
			 * Set the Subject list.
			 */
			cp = &linebuf[2];
			while (isspace(*cp))
				cp++;
			hp->h_subject = savestr(cp);
			break;
		case 'c':
			/*
			 * Add to the CC list.
			 */
			hp->h_cc = cat(hp->h_cc, extract(&linebuf[2], GCC));
			break;
		case 'b':
			/*
			 * Add stuff to blind carbon copies list.
			 */
			hp->h_bcc = cat(hp->h_bcc, extract(&linebuf[2], GBCC));
			break;
		case 'd':
			strcpy(linebuf + 2, getdeadletter());
			/* fall into . . . */
		case 'r':
			/*
			 * Invoke a file:
			 * Search for the file name,
			 * then open it and copy the contents to collf.
			 */
			cp = &linebuf[2];
			while (isspace(*cp))
				cp++;
			if (*cp == '\0') {
				printf("Interpolate what file?\n");
				break;
			}
			cp = expand(cp);
			if (cp == NOSTR)
				break;
			if (isdir(cp)) {
				printf("%s: Directory\n", cp);
				break;
			}
			if ((fbuf = Fopen(cp, "r")) == NULL) {
				perror(cp);
				break;
			}
			printf("\"%s\" ", cp);
			fflush(stdout);
			lc = 0;
			cc = 0;
			while (readline(fbuf, linebuf, LINESIZE) >= 0) {
				lc++;
				if ((t = putline(collf, linebuf)) < 0) {
					Fclose(fbuf);
					goto err;
				}
				cc += t;
			}
			Fclose(fbuf);
			printf("%d/%d\n", lc, cc);
			break;
		case 'w':
			/*
			 * Write the message on a file.
			 */
			cp = &linebuf[2];
			while (*cp == ' ' || *cp == '\t')
				cp++;
			if (*cp == '\0') {
				fprintf(stderr, "Write what file!?\n");
				break;
			}
			if ((cp = expand(cp)) == NOSTR)
				break;
			rewind(collf);
			exwrite(cp, collf, 1);
			break;
		case 'm':
		case 'M':
		case 'f':
		case 'F':
			/*
			 * Interpolate the named messages, if we
			 * are in receiving mail mode.  Does the
			 * standard list processing garbage.
			 * If ~f is given, we don't shift over.
			 */
			if (forward(linebuf + 2, collf, c) < 0)
				goto err;
			goto cont;
		case '?':
			if ((fbuf = Fopen(_PATH_TILDE, "r")) == NULL) {
				perror(_PATH_TILDE);
				break;
			}
			while ((t = getc(fbuf)) != EOF)
				(void) putchar(t);
			Fclose(fbuf);
			break;
		case 'p':
			/*
			 * Print out the current state of the
			 * message without altering anything.
			 */
			rewind(collf);
			printf("-------\nMessage contains:\n");
			puthead(hp, stdout, GTO|GSUBJECT|GCC|GBCC|GNL);
			while ((t = getc(collf)) != EOF)
				(void) putchar(t);
			goto cont;
		case '|':
			/*
			 * Pipe message through command.
			 * Collect output as new message.
			 */
			rewind(collf);
			mespipe(collf, &linebuf[2]);
			goto cont;
		case 'v':
		case 'e':
			/*
			 * Edit the current message.
			 * 'e' means to use EDITOR
			 * 'v' means to use VISUAL
			 */
			rewind(collf);
			mesedit(collf, c);
			goto cont;
		}
	}
	goto out;
err:
	if (collf != NULL) {
		Fclose(collf);
		collf = NULL;
	}
out:
	if (collf != NULL)
		rewind(collf);
	noreset--;
	sigblock(sigmask(SIGINT) | sigmask(SIGHUP));
	signal(SIGINT, saveint);
	signal(SIGHUP, savehup);
	signal(SIGTSTP, savetstp);
	signal(SIGTTOU, savettou);
	signal(SIGTTIN, savettin);
	sigsetmask(omask);
	return collf;
}

/*
 * Write a file, ex-like if f set.
 */

exwrite(name, fp, f)
	char name[];
	FILE *fp;
{
	register FILE *of;
	register int c;
	long cc;
	int lc;
	struct stat junk;

	if (f) {
		printf("\"%s\" ", name);
		fflush(stdout);
	}
	if (stat(name, &junk) >= 0 && (junk.st_mode & S_IFMT) == S_IFREG) {
		if (!f)
			fprintf(stderr, "%s: ", name);
		fprintf(stderr, "File exists\n");
		return(-1);
	}
	if ((of = Fopen(name, "w")) == NULL) {
		perror(NOSTR);
		return(-1);
	}
	lc = 0;
	cc = 0;
	while ((c = getc(fp)) != EOF) {
		cc++;
		if (c == '\n')
			lc++;
		(void) putc(c, of);
		if (ferror(of)) {
			perror(name);
			Fclose(of);
			return(-1);
		}
	}
	Fclose(of);
	printf("%d/%ld\n", lc, cc);
	fflush(stdout);
	return(0);
}

/*
 * Edit the message being collected on fp.
 * On return, make the edit file the new temp file.
 */
mesedit(fp, c)
	FILE *fp;
{
	sig_t sigint = signal(SIGINT, SIG_IGN);
	FILE *nf = run_editor(fp, (off_t)-1, c, 0);

	if (nf != NULL) {
		fseek(nf, (off_t)0, 2);
		collf = nf;
		Fclose(fp);
	}
	(void) signal(SIGINT, sigint);
}

/*
 * Pipe the message through the command.
 * Old message is on stdin of command;
 * New message collected from stdout.
 * Sh -c must return 0 to accept the new message.
 */
mespipe(fp, cmd)
	FILE *fp;
	char cmd[];
{
	FILE *nf;
	sig_t sigint = signal(SIGINT, SIG_IGN);
	extern char tempEdit[];

	if ((nf = Fopen(tempEdit, "w+")) == NULL) {
		perror(tempEdit);
		goto out;
	}
	(void) unlink(tempEdit);
	/*
	 * stdin = current message.
	 * stdout = new message.
	 */
	if (run_command(cmd, 0, fileno(fp), fileno(nf), NOSTR) < 0) {
		(void) Fclose(nf);
		goto out;
	}
	if (fsize(nf) == 0) {
		fprintf(stderr, "No bytes from \"%s\" !?\n", cmd);
		(void) Fclose(nf);
		goto out;
	}
	/*
	 * Take new files.
	 */
	(void) fseek(nf, 0L, 2);
	collf = nf;
	(void) Fclose(fp);
out:
	(void) signal(SIGINT, sigint);
}

/*
 * Interpolate the named messages into the current
 * message, preceding each line with a tab.
 * Return a count of the number of characters now in
 * the message, or -1 if an error is encountered writing
 * the message temporary.  The flag argument is 'm' if we
 * should shift over and 'f' if not.
 */
forward(ms, fp, f)
	char ms[];
	FILE *fp;
{
	register int *msgvec;
	extern char tempMail[];
	struct ignoretab *ig;
	char *tabst;

	msgvec = (int *) salloc((msgCount+1) * sizeof *msgvec);
	if (msgvec == (int *) NOSTR)
		return(0);
	if (getmsglist(ms, msgvec, 0) < 0)
		return(0);
	if (*msgvec == 0) {
		*msgvec = first(0, MMNORM);
		if (*msgvec == NULL) {
			printf("No appropriate messages\n");
			return(0);
		}
		msgvec[1] = NULL;
	}
	if (f == 'f' || f == 'F')
		tabst = NOSTR;
	else if ((tabst = value("indentprefix")) == NOSTR)
		tabst = "\t";
	ig = isupper(f) ? NULL : ignore;
	printf("Interpolating:");
	for (; *msgvec != 0; msgvec++) {
		struct message *mp = message + *msgvec - 1;

		touch(mp);
		printf(" %d", *msgvec);
		if (send(mp, fp, ig, tabst) < 0) {
			perror(tempMail);
			return(-1);
		}
	}
	printf("\n");
	return(0);
}

/*
 * Print (continue) when continued after ^Z.
 */
/*ARGSUSED*/
void
collstop(s)
{
	sig_t old_action = signal(s, SIG_DFL);

	sigsetmask(sigblock(0) & ~sigmask(s));
	kill(0, s);
	sigblock(sigmask(s));
	signal(s, old_action);
	if (colljmp_p) {
		colljmp_p = 0;
		hadintr = 0;
		longjmp(colljmp, 1);
	}
}

/*
 * On interrupt, come here to save the partial message in ~/dead.letter.
 * Then jump out of the collection loop.
 */
/*ARGSUSED*/
void
collint(s)
{
	/*
	 * the control flow is subtle, because we can be called from ~q.
	 */
	if (!hadintr) {
		if (value("ignore") != NOSTR) {
			puts("@");
			fflush(stdout);
			clearerr(stdin);
			return;
		}
		hadintr = 1;
		longjmp(colljmp, 1);
	}
	rewind(collf);
	if (value("nosave") == NOSTR)
		savedeadletter(collf);
	longjmp(collabort, 1);
}

/*ARGSUSED*/
void
collhup(s)
{
	rewind(collf);
	savedeadletter(collf);
	/*
	 * Let's pretend nobody else wants to clean up,
	 * a true statement at this time.
	 */
	exit(1);
}

savedeadletter(fp)
	register FILE *fp;
{
	register FILE *dbuf;
	register int c;
	char *cp;

	if (fsize(fp) == 0)
		return;
	cp = getdeadletter();
	c = umask(077);
	dbuf = Fopen(cp, "a");
	(void) umask(c);
	if (dbuf == NULL)
		return;
	while ((c = getc(fp)) != EOF)
		(void) putc(c, dbuf);
	Fclose(dbuf);
	rewind(fp);
}
