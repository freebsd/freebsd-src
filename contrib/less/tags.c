/*
 * Copyright (C) 1984-2000  Mark Nudelman
 *
 * You may distribute under the terms of either the GNU General Public
 * License or the Less License, as specified in the README file.
 *
 * For more information about less, or for information on how to 
 * contact the author, see the README file.
 */


#include "less.h"

#define	WHITESP(c)	((c)==' ' || (c)=='\t')

#if TAGS

public char *tags = "tags";

static char *tagfile;
static char *tagpattern;
static int taglinenum;
static int tagendline;

extern int linenums;
extern int sigs;
extern int jump_sline;

/*
 * Find a tag in the "tags" file.
 * Sets "tagfile" to the name of the file containing the tag,
 * and "tagpattern" to the search pattern which should be used
 * to find the tag.
 */
	public void
findtag(tag)
	register char *tag;
{
	char *p;
	char *q;
	register FILE *f;
	register int taglen;
	int search_char;
	int err;
	char tline[TAGLINE_SIZE];

	p = unquote_file(tags);
	f = fopen(p, "r");
	free(p);
	if (f == NULL)
	{
		error("No tags file", NULL_PARG);
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
		 * location in the file, separated by white space.
		 * The location is either a decimal line number, 
		 * or a search pattern surrounded by a pair of delimiters.
		 * Parse the line and extract these parts.
		 */
		tagfile = tagpattern = NULL;
		taglinenum = 0;

		/*
		 * Skip over the whitespace after the tag name.
		 */
		p = skipsp(tline+taglen);
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
		p = skipsp(p);
		if (*p == '\0')
			/* Pattern is missing! */
			continue;
		tagfile = save(tagfile);

		/*
		 * First see if it is a line number. 
		 */
		taglinenum = getnum(&p, 0, &err);
		if (err)
		{
			/*
			 * No, it must be a pattern.
			 * Delete the initial "^" (if present) and 
			 * the final "$" from the pattern.
			 * Delete any backslash in the pattern.
			 */
			taglinenum = 0;
			search_char = *p++;
			if (*p == '^')
				p++;
			tagpattern = (char *) ecalloc(strlen(p)+1, sizeof(char));
			q = tagpattern;
			while (*p != search_char && *p != '\0')
			{
				if (*p == '\\')
					p++;
				*q++ = *p++;
			}
			tagendline = (q[-1] == '$');
			if (tagendline)
				q--;
			*q = '\0';
		}

		fclose(f);
		return;
	}
	fclose(f);
	error("No such tag in tags file", NULL_PARG);
	tagfile = NULL;
}

	public int
edit_tagfile()
{
	int r;

	if (tagfile == NULL)
		return (1);
	r = edit(tagfile);
	free(tagfile);
	tagfile = NULL;
	return (r);
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
	public POSITION
tagsearch()
{
	POSITION pos, linepos;
	int linenum;
	int len;
	char *line;

	/*
	 * If we have the line number of the tag instead of the pattern,
	 * just use find_pos.
	 */
	if (taglinenum)
		return (find_pos(taglinenum));

	pos = ch_zero();
	linenum = find_linenum(pos);

	for (;;)
	{
		/*
		 * Get lines until we find a matching one or 
		 * until we hit end-of-file.
		 */
		if (ABORT_SIGS())
			return (NULL_POSITION);

		/*
		 * Read the next line, and save the 
		 * starting position of that line in linepos.
		 */
		linepos = pos;
		pos = forw_raw_line(pos, &line);
		if (linenum != 0)
			linenum++;

		if (pos == NULL_POSITION)
		{
			/*
			 * We hit EOF without a match.
			 */
			error("Tag not found", NULL_PARG);
			return (NULL_POSITION);
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
		 * Use strncmp because the pattern may be
		 * truncated (in the tags file) if it is too long.
		 * If tagendline is set, make sure we match all
		 * the way to end of line (no extra chars after the match).
		 */
		len = strlen(tagpattern);
		if (strncmp(tagpattern, line, len) == 0 &&
		    (!tagendline || line[len] == '\0' || line[len] == '\r'))
			break;
	}

	free(tagpattern);
	tagpattern = NULL;
	return (linepos);
}

#endif
