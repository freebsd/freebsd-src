/*
 * Copyright (c) 1988 Mark Nudleman
 * Copyright (c) 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
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
static char sccsid[] = "@(#)tags.c	8.1 (Berkeley) 6/6/93";
#endif /* not lint */

#ifndef lint
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "less.h"

#define	WHITESP(c)	((c)==' ' || (c)=='\t')

extern int sigs;
char *tagfile;		/* Name of source file containing current tag */

static enum { CTAGS, GTAGS } tagstyle = GTAGS;
static char *findctag(), *findgtag(), *nextgtag(), *prevgtag();
static ctagsearch(), gtagsearch();

/*
 * Load information about the tag.  The global variable tagfile will point to
 * the file that contains the tag, or will be NULL if information could not be
 * found (in which case an error message will have been printed).  After
 * loading the file named by tagfile, tagsearch() should be called to
 * set the current position to the tag.
 */
findtag(tag)
	char *tag;	/* The tag to load */
{
	/*
	 * Try using gtags or ctags first, as indicated by tagstyle.  If
	 * that fails, try the other.  Someday there may even be a way to
	 * assert a certain tagstyle...
	 */
	switch(tagstyle) {
	case CTAGS:
		tagfile = findctag(tag);
		if (!tagfile && (tagfile = findgtag(tag))) tagstyle = GTAGS;
		if (tagfile) return;
		break;
	case GTAGS:
		/* Would be nice to print the number of tag references
		 * we found (for nexttag() and prevtag()) in a (not-)error()
		 * message. */
		tagfile = findgtag(tag);
		if (!tagfile && (tagfile = findctag(tag))) tagstyle = CTAGS;
		if (tagfile) return;
		break;
	}

	error("could not find relevent tag information");
}

/*
 * Load information about the next number'th tag, if the last findtag() call
 * found multiple tag references.  The global variable tagfile will point to the
 * file that contains the tag, or will be NULL if information could not be
 * found (in which case an error message will have been printed).  After
 * loading the file named by tagfile, tagsearch() should be called to set
 * the current position to the tag.
 */
nexttag(number)
	int number;	/* How many tags to go forward by */
{
	if (number < 0) number = -number;  /* positive only, please */

	switch(tagstyle) {
	case CTAGS:
		break;
	case GTAGS:
		while (number--) tagfile = nextgtag();
		break;
	}
	if (!tagfile)
		error("no next tag");
}

/*
 * The antithesis to nexttag().
 */
prevtag(number)
	int number;	/* How many tags to go backwards by */
{
	if (number < 0) number = -number;  /* positive only, please */

	switch(tagstyle) {
	case CTAGS:
		break;
	case GTAGS:
		while (number--) tagfile = prevgtag();
		break;
	}
	if (!tagfile)
		error("no previous tag");
}

/*
 * Try and position the currently loaded file at the last tag that was
 * succesfully passed to findtag() or chosen with nexttag() and prevtag().
 * An error message will be printed if unsuccessful.
 */
tagsearch()
{
	switch(tagstyle) {
	case CTAGS:
		if (ctagsearch())
			error("could not locate ctag");
		return;
	case GTAGS:
		if (gtagsearch())
			error("could not locate gtag");
		return;
	}
}


/*******************************************************************************
 *
 * ctags
 *
 */

extern int linenums;
extern char *line;

static char *ctagpattern;
static int ctagflags;

/* ctag flags */
#define START_OF_LINE 0x01
#define END_OF_LINE   0x02

/*
 * Find specified tag in the ctags(1)-format tag file ctagfile.  Returns
 * pointer to a static buffer holding the name of the file containing
 * the tag.  Returns NULL on failure.  The next call to ctagsearch() will
 * position the currently loaded file at the tag.
 */
static char *
findctag(tag)
	register char *tag;	/* tag to search for */
{
	register char *p;
	register FILE *f;
	register int taglen;
	int search_char;
	static char tline[200];  /* XXX should be dynamic */
	const char *ctagfile = "tags";
	char *retr;

	if ((f = fopen(ctagfile, "r")) == NULL)
		return (NULL);

	taglen = strlen(tag);

	/*
	 * Search the tags file for the desired tag.
	 */
	while (fgets(tline, sizeof(tline), f) != NULL)
	{
		if (sigs)
			break;  /* abandon */

		if (strncmp(tag, tline, taglen) != 0 || !WHITESP(tline[taglen]))
			continue;

		/*
		 * Found it.
		 * The line contains the tag, the filename and the
		 * pattern, separated by white space.
		 * The pattern is surrounded by a pair of identical
		 * search characters.
		 * Parse the line and extract these parts.
		 */

		/*
		 * Skip over the tag and the whitespace after the tag name.
		 */
		for (p = tline;  !WHITESP(*p) && *p != '\0';  p++)
			continue;
		while (WHITESP(*p))
			p++;
		if (*p == '\0')
			/* File name is missing! */
			continue;

		/*
		 * Save the file name.
		 * Skip over the filename and whitespace after the file name.
		 */
		retr = p;
		while (!WHITESP(*p) && *p != '\0')
			p++;
		*p++ = '\0';
		while (WHITESP(*p))
			p++;
		if (*p == '\0')
			/* Pattern is missing! */
			continue;

		/*
		 * Save the pattern.
		 * Skip to the end of the pattern.
		 * Delete the initial "^" and the final "$" from the pattern.
		 */
		search_char = *p++;
		if (*p == '^') {
			p++;
			ctagflags |= START_OF_LINE;
		} else {
			ctagflags &= ~START_OF_LINE;
		}
		ctagpattern = p;  /* cock ctagsearch() */
		while (*p != search_char && *p != '\0')
			p++;
		if (p[-1] == '\n')
			p--;
		if (p[-1] == '$') {
			p--;
			ctagflags |= END_OF_LINE;
		} else {
			ctagflags &= ~END_OF_LINE;
		}
		*p = '\0';

		(void)fclose(f);
		return (retr);
	}
	(void)fclose(f);
	return (NULL);
}

/*
 * Locate the tag that was loaded by findctag().
 * This is a stripped-down version of search().
 * We don't use search() for several reasons:
 *   -	We don't want to blow away any search string we may have saved.
 *   -	The various regular-expression functions (from different systems:
 *	regcmp vs. re_comp) behave differently in the presence of
 *	parentheses (which are almost always found in a tag).
 *
 * Returns -1 if it was unable to position at the requested pattern,
 * 0 otherwise.
 */
static
ctagsearch()
{
	off_t pos, linepos, forw_raw_line();
	int linenum;

	pos = (off_t)0;
	linenum = find_linenum(pos);

	for (;;)
	{
		/*
		 * Get lines until we find a matching one or
		 * until we hit end-of-file.
		 */
		if (sigs)
			return (-1);

		/*
		 * Read the next line, and save the
		 * starting position of that line in linepos.
		 */
		linepos = pos;
		pos = forw_raw_line(pos);
		if (linenum != 0)
			linenum++;

		if (pos == NULL_POSITION)
			return (-1);  /* Tag not found. */

		/*
		 * If we're using line numbers, we might as well
		 * remember the information we have now (the position
		 * and line number of the current line).
		 */
		if (linenums)
			add_lnum(linenum, pos);

		/*
		 * Test the line to see if we have a match.  I don't know of
		 * any tags program that would use START_OF_LINE but not
		 * END_OF_LINE, or vice-a-versa, but we handle this case anyway.
		 */
		switch (ctagflags) {
		case 0: /* !START_OF_LINE and !END_OF_LINE */
			if (strstr(line, ctagpattern))
				goto found;
			break;
		case START_OF_LINE:  /* !END_OF_LINE */
			if (!strncmp(ctagpattern, line, strlen(ctagpattern)))
				goto found;
			break;
		case END_OF_LINE:  /* !START_OF_LINE */
		{
			char *x = strstr(line, ctagpattern);
			if (!x)
				break;
			if (x[strlen(ctagpattern)] != '\0')
				break;
			goto found;
		}
		case START_OF_LINE | END_OF_LINE:
			if (!strcmp(ctagpattern, line))
				goto found;
			break;
		}
	}

found:
	jump_loc(linepos);
	return (0);
}


/*******************************************************************************
 *
 * gtags
 *
 */

/*
 * The findgtag() and getentry() functions are stolen, more or less, from the
 * patches to nvi-1.79 included in Shigio Yamaguchi's global-3.42 distribution.
 */

/*
 * The queue of tags generated by the last findgtag() call.
 */
static CIRCLEQ_HEAD(gtag_q, gtag) gtag_q;
struct gtag {
	CIRCLEQ_ENTRY(gtag) ptrs;
	char *file;	/* source file containing the tag */
	int line;	/* appropriate line number of source file */
};
static struct gtag *curgtag;
static getentry();

/*
 * The findgtag() will try and load information about the requested tag.
 * It does this by calling "global -x tag; global -xr tag;" and storing the
 * parsed output for future use by gtagsearch_f() and gtagsearch_b().  A
 * pointer to a static buffer containing the name of the source file will
 * be returned, or NULL on failure.  The first filename printed by global is
 * returned (hopefully the function definition) and the other filenames may
 * be accessed by nextgtag() and prevgtag().
 */
static char *
findgtag(tag)
	char *tag;		/* tag to load */
{
	struct gtag *gtag_p1, *gtag_p2;
	char command[512];
	char buf[256];
	FILE *fp;

	if (!tag) return (NULL);  /* Sanity check */

	/* Clear any existing tag circle queue */
	/* XXX Ideally, we wouldn't do this until after we know that we
	 * can load some other tag information. */
	curgtag = NULL;
	gtag_p1 = gtag_q.cqh_first;
	if (gtag_p1) while (gtag_p1 != (void *)&gtag_q) {
		gtag_p2 = gtag_p1->ptrs.cqe_next;
		free(gtag_p1);
		gtag_p1 = gtag_p2;
	}

	/* Allocate and initialize the tag queue structure. */
	CIRCLEQ_INIT(&gtag_q);

	/* Get our data from global(1) */
	snprintf(command, sizeof(command),
	    "(global -x '%s'; global -xr '%s') 2>/dev/null", tag, tag);
	if (fp = popen(command, "r")) {
		while (fgets(buf, sizeof(buf), fp)) {
			char *name, *file, *line;

			if (sigs) {
				pclose(fp);
				return (NULL);
			}
				
			/* chop(buf) */
			if (buf[strlen(buf) - 1] == '\n')
				buf[strlen(buf) - 1] = 0;
			else
				while (fgetc(fp) != '\n')
					;

 			if (getentry(buf, &name, &file, &line)) {
				/*
				 * Couldn't parse this line for some reason.
				 * We'll just pretend it never happened.
				 */
				break;
			}

			/* Add to queue */
			gtag_p1 = malloc(sizeof(struct gtag));
			if (!gtag_p1) {
				pclose(fp);
				error("malloc() failed");
				return (NULL);
			}
			gtag_p1->file = malloc(strlen(file) + 1);
			if (!gtag_p1->file) {
				pclose(fp);
				error("malloc() failed");
				return (NULL);
			}
			strcpy(gtag_p1->file, file);
			gtag_p1->line = atoi(line);
			CIRCLEQ_INSERT_TAIL(&gtag_q, gtag_p1, ptrs);
		}
		pclose(fp);
	}

	/* Check to see if we found anything. */
	if (gtag_q.cqh_first == (void *)&gtag_q)
		return (NULL);  /* Nope! */

	curgtag = gtag_q.cqh_first;
	return (curgtag->file);
}

/*
 * Return the filename required for the next gtag in the queue that was setup
 * by findgtag().  The next call to gtagsearch() will try to position at the
 * appropriate tag.
 */
static char *
nextgtag()
{
	if (!curgtag) {
		/* No tag stack loaded */
		return (NULL);
	}

	curgtag = curgtag->ptrs.cqe_next;
	if (curgtag == (void *)&gtag_q) {
		/* Wrapped around to the head of the queue */
		curgtag = ((struct gtag_q *)curgtag)->cqh_first;
	}

	return (curgtag->file);
}

/*
 * Return the filename required for the previous gtag in the queue that was
 * setup by findgtat().  The next call to gtagsearch() will try to position
 * at the appropriate tag.
 */
static char *
prevgtag()
{
	if (!curgtag) {
		/* No tag stack loaded */
		return (NULL);
	}

	curgtag = curgtag->ptrs.cqe_prev;
	if (curgtag == (void *)&gtag_q) {
		/* Wrapped around to the head of the queue */
		curgtag = ((struct gtag_q *)curgtag)->cqh_last;
	}

	return (curgtag->file);
}

/*
 * Position the current file at at what is hopefully the tag that was chosen
 * using either findtag() or one of nextgtag() and prevgtag().  Returns -1
 * if it was unable to position at the tag, 0 if succesful.
 */
static
gtagsearch()
{
	if (!curgtag)
		return (-1);  /* No gtags loaded! */

	jump_back(curgtag->line);

	/*
	 * XXX We'll assume we were successful --- jump_back() will call error()
	 * if it fails, so the user will receive some kind of notification.
	 * Eventually, jump_back() should do its work silently and let us
	 * perform the error notification, eventually allowing our caller
	 * (presumably tagsearch()) to go error("Could not locate tag.");
	 */
	return (0);
}

/*
 * The getentry() parses output from the global(1) command.  The output
 * must be in the format described below.  Returns 0 on success, -1 on
 * error.  The tag, file, and line will each be NUL-terminated pointers
 * into buf.
 *
 * gtags temporary file format.
 * <tag>   <lineno>  <file>         <image>
 *
 * sample.
 * +------------------------------------------------
 * |main     30      main.c         main(argc, argv)
 * |func     21      subr.c         func(arg)
 */
static
getentry(buf, tag, file, line)
	char *buf;	/* output from global -x */
	char **tag;	/* name of the tag we actually found */
	char **file;	/* file in which to find this tag */
	char **line;	/* line number of file where this tag is found */
{
	char *p = buf;

	for (*tag = p; *p && !isspace(*p); p++)		/* tag name */
		;
	if (*p == 0)
		goto err;
	*p++ = 0;
	for (; *p && isspace(*p); p++)			/* (skip blanks) */
		;
	if (*p == 0)
		goto err;
	*line = p;					/* line no */
	for (*line = p; *p && !isspace(*p); p++)
		;
	if (*p == 0)
		goto err;
	*p++ = 0;
	for (; *p && isspace(*p); p++)			/* (skip blanks) */
		;
	if (*p == 0)
		goto err;
	*file = p;					/* file name */
	for (*file = p; *p && !isspace(*p); p++)
		;
	if (*p == 0)
		goto err;
	*p = 0;

	/* value check */
	if (strlen(*tag) && strlen(*line) && strlen(*file) && atoi(*line) > 0)
		return (0);	/* OK */
err:
	return (-1);		/* ERROR */
}
