/*
 * Copyright (c) 1988 Mark Nudleman
 * Copyright (c) 1988 Regents of the University of California.
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
static char sccsid[] = "@(#)tags.c	5.5 (Berkeley) 6/1/90";
#endif /* not lint */

#include <sys/types.h>
#include <stdio.h>
#include <less.h>

#define	WHITESP(c)	((c)==' ' || (c)=='\t')

char *tagfile;
char *tagpattern;

static char *tags = "tags";

extern int linenums;
extern int sigs;
extern char *line;

/*
 * Find a tag in the "tags" file.
 * Sets "tagfile" to the name of the file containing the tag,
 * and "tagpattern" to the search pattern which should be used
 * to find the tag.
 */
findtag(tag)
	register char *tag;
{
	register char *p;
	register FILE *f;
	register int taglen;
	int search_char;
	static char tline[200];

	if ((f = fopen(tags, "r")) == NULL)
	{
		error("No tags file");
		tagfile = NULL;
		return;
	}

	taglen = strlen(tag);

	/*
	 * Search the tags file for the desired tag.
	 */
	while (fgets(tline, sizeof(tline), f) != NULL)
	{
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
		tagfile = tagpattern = NULL;

		/*
		 * Skip over the whitespace after the tag name.
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
		 * Skip over the whitespace after the file name.
		 */
		tagfile = p;
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
		if (*p == '^')
			p++;
		tagpattern = p;
		while (*p != search_char && *p != '\0')
			p++;
		if (p[-1] == '$')
			p--;
		*p = '\0';

		(void)fclose(f);
		return;
	}
	(void)fclose(f);
	error("No such tag in tags file");
	tagfile = NULL;
}

/*
 * Search for a tag.
 * This is a stripped-down version of search().
 * We don't use search() for several reasons:
 *   -	We don't want to blow away any search string we may have saved.
 *   -	The various regular-expression functions (from different systems:
 *	regcmp vs. re_comp) behave differently in the presence of 
 *	parentheses (which are almost always found in a tag).
 */
tagsearch()
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
			return (1);

		/*
		 * Read the next line, and save the 
		 * starting position of that line in linepos.
		 */
		linepos = pos;
		pos = forw_raw_line(pos);
		if (linenum != 0)
			linenum++;

		if (pos == NULL_POSITION)
		{
			/*
			 * We hit EOF without a match.
			 */
			error("Tag not found");
			return (1);
		}

		/*
		 * If we're using line numbers, we might as well
		 * remember the information we have now (the position
		 * and line number of the current line).
		 */
		if (linenums)
			add_lnum(linenum, pos);

		/*
		 * Test the line to see if we have a match.
		 */
		if (strcmp(tagpattern, line) == 0)
			break;
	}

	jump_loc(linepos);
	return (0);
}
