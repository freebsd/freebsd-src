/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Jim R. Oldroyd at The Instruction Set and Keith Gabryelski at
 * Commodore Business Machines.
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
static char copyright[] =
"@(#) Copyright (c) 1991, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)quiz.c	8.3 (Berkeley) 5/4/95";
#endif /* not lint */

#include <sys/types.h>

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "quiz.h"
#include "pathnames.h"

static QE qlist;
static int catone, cattwo, tflag;
static u_int qsize;

char	*appdstr __P((char *, char *, size_t));
void	 downcase __P((char *));
void	 err __P((const char *, ...));
void	 get_cats __P((char *, char *));
void	 get_file __P((char *));
char	*next_cat __P((char *));
void	 quiz __P((void));
void	 score __P((u_int, u_int, u_int));
void	 show_index __P((void));
void	 usage __P((void));

int
main(argc, argv)
	int argc;
	char *argv[];
{
	register int ch;
	char *indexfile;

	/* revoke */
	setegid(getgid());
	setgid(getgid());

	indexfile = _PATH_QUIZIDX;
	while ((ch = getopt(argc, argv, "i:t")) != EOF)
		switch(ch) {
		case 'i':
			indexfile = optarg;
			break;
		case 't':
			tflag = 1;
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	switch(argc) {
	case 0:
		get_file(indexfile);
		show_index();
		break;
	case 2:
		get_file(indexfile);
		get_cats(argv[0], argv[1]);
		quiz();
		break;
	default:
		usage();
	}
	exit(0);
}

void
get_file(file)
	char *file;
{
	register FILE *fp;
	register QE *qp;
	size_t len;
	char *lp;

	if ((fp = fopen(file, "r")) == NULL)
		err("%s: %s", file, strerror(errno));

	/*
	 * XXX
	 * Should really free up space from any earlier read list
	 * but there are no reverse pointers to do so with.
	 */
	qp = &qlist;
	qsize = 0;
	while ((lp = fgetln(fp, &len)) != NULL) {
		if (qp->q_text && qp->q_text[strlen(qp->q_text) - 1] == '\\')
			qp->q_text = appdstr(qp->q_text, lp, len);
		else {
			if ((qp->q_next = malloc(sizeof(QE))) == NULL)
				err("%s", strerror(errno));
			qp = qp->q_next;
			lp[len - 1] = '\0';
			if ((qp->q_text = strdup(lp)) == NULL)
				err("%s", strerror(errno));
			qp->q_asked = qp->q_answered = FALSE;
			qp->q_next = NULL;
			++qsize;
		}
	}
	(void)fclose(fp);
}

void
show_index()
{
	register QE *qp;
	register char *p, *s;
	FILE *pf;

	if ((pf = popen(_PATH_PAGER, "w")) == NULL)
		err("%s: %s", _PATH_PAGER, strerror(errno));
	(void)fprintf(pf, "Subjects:\n\n");
	for (qp = qlist.q_next; qp; qp = qp->q_next) {
		for (s = next_cat(qp->q_text); s; s = next_cat(s)) {
			if (!rxp_compile(s))
				err("%s", rxperr);
			if (p = rxp_expand())
				(void)fprintf(pf, "%s ", p);
		}
		(void)fprintf(pf, "\n");
	}
	(void)fprintf(pf, "\n%s\n%s\n%s\n",
"For example, \"quiz victim killer\" prints a victim's name and you reply",
"with the killer, and \"quiz killer victim\" works the other way around.",
"Type an empty line to get the correct answer.");
	(void)pclose(pf);
}

void
get_cats(cat1, cat2)
	char *cat1, *cat2;
{
	register QE *qp;
	int i;
	char *s;

	downcase(cat1);
	downcase(cat2);
	for (qp = qlist.q_next; qp; qp = qp->q_next) {
		s = next_cat(qp->q_text);
		catone = cattwo = i = 0;
		while (s) {
			if (!rxp_compile(s))
				err("%s", rxperr);
			i++;
			if (rxp_match(cat1))
				catone = i;
			if (rxp_match(cat2))
				cattwo = i;
			s = next_cat(s);
		}
		if (catone && cattwo && catone != cattwo) {
			if (!rxp_compile(qp->q_text))
				err("%s", rxperr);
			get_file(rxp_expand());
			return;
		}
	}
	err("invalid categories");
}

void
quiz()
{
	register QE *qp;
	register int i;
	size_t len;
	u_int guesses, rights, wrongs;
	int next;
	char *answer, *s, *t, question[LINE_SZ];

	srandom(time(NULL));
	guesses = rights = wrongs = 0;
	for (;;) {
		if (qsize == 0)
			break;
		next = random() % qsize;
		qp = qlist.q_next;
		for (i = 0; i < next; i++)
			qp = qp->q_next;
		while (qp && qp->q_answered)
			qp = qp->q_next;
		if (!qp) {
			qsize = next;
			continue;
		}
		if (tflag && random() % 100 > 20) {
			/* repeat questions in tutorial mode */
			while (qp && (!qp->q_asked || qp->q_answered))
				qp = qp->q_next;
			if (!qp)
				continue;
		}
		s = qp->q_text;
		for (i = 0; i < catone - 1; i++)
			s = next_cat(s);
		if (!rxp_compile(s))
			err("%s", rxperr);
		t = rxp_expand();
		if (!t || *t == '\0') {
			qp->q_answered = TRUE;
			continue;
		}
		(void)strcpy(question, t);
		s = qp->q_text;
		for (i = 0; i < cattwo - 1; i++)
			s = next_cat(s);
		if (!rxp_compile(s))
			err("%s", rxperr);
		t = rxp_expand();
		if (!t || *t == '\0') {
			qp->q_answered = TRUE;
			continue;
		}
		qp->q_asked = TRUE;
		(void)printf("%s?\n", question);
		for (;; ++guesses) {
			if ((answer = fgetln(stdin, &len)) == NULL) {
				score(rights, wrongs, guesses);
				exit(0);
			}
			answer[len - 1] = '\0';
			downcase(answer);
			if (rxp_match(answer)) {
				(void)printf("Right!\n");
				++rights;
				qp->q_answered = TRUE;
				break;
			}
			if (*answer == '\0') {
				(void)printf("%s\n", t);
				++wrongs;
				if (!tflag)
					qp->q_answered = TRUE;
				break;
			}
			(void)printf("What?\n");
		}
	}
	score(rights, wrongs, guesses);
}

char *
next_cat(s)
	register char *	s;
{
	for (;;)
		switch (*s++) {
		case '\0':
			return (NULL);
		case '\\':
			s++;
			break;
		case ':':
			return (s);
		}
	/* NOTREACHED */
}

char *
appdstr(s, tp, len)
	char *s;
	register char *tp;
	size_t len;
{
	register char *mp, *sp;
	register int ch;
	char *m;

	if ((m = malloc(strlen(s) + len + 1)) == NULL)
		err("%s", strerror(errno));
	for (mp = m, sp = s; *mp++ = *sp++;);
	mp--;

	if (*(mp - 1) == '\\')
		--mp;
	memcpy(mp, tp, len);
	mp[len] = '\0';
	if (mp[len - 1] == '\n')
		mp[len - 1] = '\0';

	free(s);
	return (m);
}

void
score(r, w, g)
	u_int r, w, g;
{
	(void)printf("Rights %d, wrongs %d,", r, w);
	if (g)
		(void)printf(" extra guesses %d,", g);
	(void)printf(" score %d%%\n", (r + w + g) ? r * 100 / (r + w + g) : 0);
}

void
downcase(p)
	register char *p;
{
	register int ch;

	for (; ch = *p; ++p)
		if (isascii(ch) && isupper(ch))
			*p = tolower(ch);
}

void
usage()
{
	(void)fprintf(stderr, "quiz [-t] [-i file] category1 category2\n");
	exit(1);
}

#if __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

void
#if __STDC__
err(const char *fmt, ...)
#else
err(fmt, va_alist)
	char *fmt;
        va_dcl
#endif
{
	va_list ap;
#if __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
#endif
	(void)fprintf(stderr, "quiz: ");
	(void)vfprintf(stderr, fmt, ap);
	va_end(ap);
	(void)fprintf(stderr, "\n");
	exit(1);
}
