/*	$Id: complete.c,v 1.2 1997/06/27 09:30:04 ache Exp $ */
/*	$NetBSD: complete.c,v 1.8 1997/05/24 16:34:30 lukem Exp $	*/

/*-
 * Copyright (c) 1997 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Luke Mewburn.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef SMALL
#ifndef lint
static char rcsid[] = "$Id: complete.c,v 1.2 1997/06/27 09:30:04 ache Exp $";
#endif /* not lint */

/*
 * FTP user program - command and file completion routines
 */

#include <sys/types.h>
#include <ctype.h>
#include <err.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ftp_var.h"

static int
comparstr(a, b)
	const void *a, *b;
{
	return (strcoll(*(char **)a, *(char **)b));
}

/*
 * Determine if complete is ambiguous. If unique, insert.
 * If no choices, error. If unambiguous prefix, insert that.
 * Otherwise, list choices. words is assumed to be filtered
 * to only contain possible choices.
 * Args:
 *	word	word which started the match
 *	list	list by default
 *	words	stringlist containing possible matches
 */
static unsigned char
complete_ambiguous(word, list, words)
	char *word;
	int list;
	StringList *words;
{
	char insertstr[MAXPATHLEN];
	char *lastmatch;
	int i, j, matchlen, wordlen;

	wordlen = strlen(word);
	if (words->sl_cur == 0)
		return (CC_ERROR);	/* no choices available */

	if (words->sl_cur == 1) {	/* only once choice available */
		(void)strcpy(insertstr, words->sl_str[0]);
		if (el_insertstr(el, insertstr + wordlen) == -1)
			return (CC_ERROR);
		else
			return (CC_REFRESH);
	}

	if (!list) {
		matchlen = 0;
		lastmatch = words->sl_str[0];
		matchlen = strlen(lastmatch);
		for (i = 1 ; i < words->sl_cur ; i++) {
			for (j = wordlen ; j < strlen(words->sl_str[i]); j++)
				if (lastmatch[j] != words->sl_str[i][j])
					break;
			if (j < matchlen)
				matchlen = j;
		}
		if (matchlen > wordlen) {
			(void)strncpy(insertstr, lastmatch, matchlen);
			insertstr[matchlen] = '\0';
			if (el_insertstr(el, insertstr + wordlen) == -1)
				return (CC_ERROR);
			else	
					/*
					 * XXX: really want CC_REFRESH_BEEP
					 */
				return (CC_REFRESH);
		}
	}

	putchar('\n');
	qsort(words->sl_str, words->sl_cur, sizeof(char *), comparstr);
	list_vertical(words);
	return (CC_REDISPLAY);
}

/*
 * Complete a command
 */
static unsigned char
complete_command(word, list)
	char *word;
	int list;
{
	struct cmd *c;
	StringList *words;
	int wordlen;
	unsigned char rv;

	words = sl_init();
	wordlen = strlen(word);

	for (c = cmdtab; c->c_name != NULL; c++) {
		if (wordlen > strlen(c->c_name))
			continue;
		if (strncmp(word, c->c_name, wordlen) == 0)
			sl_add(words, c->c_name);
	}

	rv = complete_ambiguous(word, list, words);
	sl_free(words, 0);
	return (rv);
}

/*
 * Complete a local file
 */
static unsigned char
complete_local(word, list)
	char *word;
	int list;
{
	StringList *words;
	char dir[MAXPATHLEN];
	char *file;
	DIR *dd;
	struct dirent *dp;
	unsigned char rv;

	if ((file = strrchr(word, '/')) == NULL) {
		dir[0] = '.';
		dir[1] = '\0';
		file = word;
	} else {
		if (file == word) {
			dir[0] = '/';
			dir[1] = '\0';
		} else {
			(void)strncpy(dir, word, file - word);
			dir[file - word] = '\0';
		}
		file++;
	}

	if ((dd = opendir(dir)) == NULL)
		return (CC_ERROR);

	words = sl_init();

	for (dp = readdir(dd); dp != NULL; dp = readdir(dd)) {
		if (!strcmp(dp->d_name, ".") || !strcmp(dp->d_name, ".."))
			continue;
		if (strlen(file) > dp->d_namlen)
			continue;
		if (strncmp(file, dp->d_name, strlen(file)) == 0) {
			char *tcp;

			tcp = strdup(dp->d_name);
			if (tcp == NULL)
				errx(1, "Can't allocate memory for local dir");
			sl_add(words, tcp);
		}
	}
	closedir(dd);

	rv = complete_ambiguous(file, list, words);
	sl_free(words, 1);
	return (rv);
}

/*
 * Complete a remote file
 */
static unsigned char
complete_remote(word, list)
	char *word;
	int list;
{
	static StringList *dirlist;
	static char	 lastdir[MAXPATHLEN];
	StringList	*words;
	char		 dir[MAXPATHLEN];
	char		*file, *cp;
	int		 i;
	unsigned char	 rv;

	char *dummyargv[] = { "complete", dir, NULL };

	if ((file = strrchr(word, '/')) == NULL) {
		dir[0] = '.';
		dir[1] = '\0';
		file = word;
	} else {
		cp = file;
		while (*cp == '/' && cp > word)
			cp--;
		(void)strncpy(dir, word, cp - word + 1);
		dir[cp - word + 1] = '\0';
		file++;
	}

	if (dirchange || strcmp(dir, lastdir) != 0) {	/* dir not cached */
		char *emesg;

		if (dirlist != NULL)
			sl_free(dirlist, 1);
		dirlist = sl_init();

		mflag = 1;
		emesg = NULL;
		while ((cp = remglob(dummyargv, 0, &emesg)) != NULL) {
			char *tcp;

			if (!mflag)
				continue;
			if (*cp == '\0') {
				mflag = 0;
				continue;
			}
			tcp = strrchr(cp, '/');
			if (tcp)
				tcp++;
			else
				tcp = cp;
			tcp = strdup(tcp);
			if (tcp == NULL)
				errx(1, "Can't allocate memory for remote dir");
			sl_add(dirlist, tcp);
		}
		if (emesg != NULL) {
			printf("\n%s\n", emesg);
			return (CC_REDISPLAY);
		}
		(void)strcpy(lastdir, dir);
		dirchange = 0;
	}

	words = sl_init();
	for (i = 0; i < dirlist->sl_cur; i++) {
		cp = dirlist->sl_str[i];
		if (strlen(file) > strlen(cp))
			continue;
		if (strncmp(file, cp, strlen(file)) == 0)
			sl_add(words, cp);
	}
	rv = complete_ambiguous(file, list, words);
	sl_free(words, 0);
	return (rv);
}

/*
 * Generic complete routine
 */
unsigned char
complete(el, ch)
	EditLine *el;
	int ch;
{
	static char word[FTPBUFLEN];
	static int lastc_argc, lastc_argo;

	struct cmd *c;
	const LineInfo *lf;
	int len, celems, dolist;

	lf = el_line(el);
	len = lf->lastchar - lf->buffer;
	if (len >= sizeof(line))
		return (CC_ERROR);
	(void)strncpy(line, lf->buffer, len);
	line[len] = '\0';
	cursor_pos = line + (lf->cursor - lf->buffer);
	lastc_argc = cursor_argc;	/* remember last cursor pos */
	lastc_argo = cursor_argo;
	makeargv();			/* build argc/argv of current line */

	if (cursor_argo >= sizeof(word))
		return (CC_ERROR);

	dolist = 0;
			/* if cursor and word is same, list alternatives */
	if (lastc_argc == cursor_argc && lastc_argo == cursor_argo
	    && strncmp(word, margv[cursor_argc], cursor_argo) == 0)
		dolist = 1;
	else
	    (void)strncpy(word, margv[cursor_argc], cursor_argo);
	word[cursor_argo] = '\0';

	if (cursor_argc == 0)
		return (complete_command(word, dolist));

	c = getcmd(margv[0]);
	if (c == (struct cmd *)-1 || c == 0)
		return (CC_ERROR);
	celems = strlen(c->c_complete);

		/* check for 'continuation' completes (which are uppercase) */
	if ((cursor_argc > celems) && (celems > 0)
	    && isupper((unsigned char)c->c_complete[celems-1]))
		cursor_argc = celems;

	if (cursor_argc > celems)
		return (CC_ERROR);

	switch (c->c_complete[cursor_argc - 1]) {
		case 'l':			/* local complete */
		case 'L':
			return (complete_local(word, dolist));
		case 'r':			/* remote complete */
		case 'R':
			if (connected != -1) {
				puts("\nMust be logged in to complete.");
				return (CC_REDISPLAY);
			}
			return (complete_remote(word, dolist));
		case 'c':			/* command complete */
		case 'C':
			return (complete_command(word, dolist));
		case 'n':			/* no complete */
		default:
			return (CC_ERROR);
	}

	return (CC_ERROR);
}
#endif
