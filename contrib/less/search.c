/* $FreeBSD$ */
/*
 * Copyright (C) 1984-2000  Mark Nudelman
 *
 * You may distribute under the terms of either the GNU General Public
 * License or the Less License, as specified in the README file.
 *
 * For more information about less, or for information on how to 
 * contact the author, see the README file.
 */


/*
 * Routines to search a file for a pattern.
 */

#include "less.h"
#include "position.h"

#define	MINPOS(a,b)	(((a) < (b)) ? (a) : (b))
#define	MAXPOS(a,b)	(((a) > (b)) ? (a) : (b))

#if HAVE_POSIX_REGCOMP
#include <regex.h>
#ifdef REG_EXTENDED
#define	REGCOMP_FLAG	(more_mode ? 0 : REG_EXTENDED)
#else
#define	REGCOMP_FLAG	0
#endif
#endif
#if HAVE_PCRE
#include <pcre.h>
#endif
#if HAVE_RE_COMP
char *re_comp();
int re_exec();
#endif
#if HAVE_REGCMP
char *regcmp();
char *regex();
extern char *__loc1;
#endif
#if HAVE_V8_REGCOMP
#include "regexp.h"
#endif

static int match();

extern int sigs;
extern int how_search;
extern int caseless;
extern int linenums;
extern int sc_height;
extern int jump_sline;
extern int bs_mode;
extern int more_mode;
extern int status_col;
extern POSITION start_attnpos;
extern POSITION end_attnpos;
#if HILITE_SEARCH
extern int hilite_search;
extern int screen_trashed;
extern int size_linebuf;
extern int squished;
extern int can_goto_line;
static int hide_hilite;
static POSITION prep_startpos;
static POSITION prep_endpos;

struct hilite
{
	struct hilite *hl_next;
	POSITION hl_startpos;
	POSITION hl_endpos;
};
static struct hilite hilite_anchor = { NULL, NULL_POSITION, NULL_POSITION };
#define	hl_first	hl_next
#endif

/*
 * These are the static variables that represent the "remembered"
 * search pattern.  
 */
#if HAVE_POSIX_REGCOMP
static regex_t *regpattern = NULL;
#endif
#if HAVE_PCRE
pcre *regpattern = NULL;
#endif
#if HAVE_RE_COMP
int re_pattern = 0;
#endif
#if HAVE_REGCMP
static char *cpattern = NULL;
#endif
#if HAVE_V8_REGCOMP
static struct regexp *regpattern = NULL;
#endif

static int is_caseless;
static int is_ucase_pattern;
static int last_search_type;
static char *last_pattern = NULL;

/*
 * Convert text.  Perform one or more of these transformations:
 */
#define	CVT_TO_LC	01	/* Convert upper-case to lower-case */
#define	CVT_BS		02	/* Do backspace processing */
#define	CVT_CRLF	04	/* Remove CR after LF */

	static void
cvt_text(odst, osrc, ops)
	char *odst;
	char *osrc;
	int ops;
{
	register char *dst;
	register char *src;

	for (src = osrc, dst = odst;  *src != '\0';  src++, dst++)
	{
		if ((ops & CVT_TO_LC) && isupper((unsigned char) *src))
			/* Convert uppercase to lowercase. */
			*dst = tolower((unsigned char) *src);
		else if ((ops & CVT_BS) && *src == '\b' && dst > odst)
			/* Delete BS and preceding char. */
			dst -= 2;
		else 
			/* Just copy. */
			*dst = *src;
	}
	if ((ops & CVT_CRLF) && dst > odst && dst[-1] == '\r')
		dst--;
	*dst = '\0';
}

/*
 * Are there any uppercase letters in this string?
 */
	static int
is_ucase(s)
	char *s;
{
	register char *p;

	for (p = s;  *p != '\0';  p++)
		if (isupper((unsigned char) *p))
			return (1);
	return (0);
}

/*
 * Is there a previous (remembered) search pattern?
 */
	static int
prev_pattern()
{
	if (last_search_type & SRCH_NO_REGEX)
		return (last_pattern != NULL);
#if HAVE_POSIX_REGCOMP
	return (regpattern != NULL);
#endif
#if HAVE_PCRE
	return (regpattern != NULL);
#endif
#if HAVE_RE_COMP
	return (re_pattern != 0);
#endif
#if HAVE_REGCMP
	return (cpattern != NULL);
#endif
#if HAVE_V8_REGCOMP
	return (regpattern != NULL);
#endif
#if NO_REGEX
	return (last_pattern != NULL);
#endif
}

#if HILITE_SEARCH
/*
 * Repaint the hilites currently displayed on the screen.
 * Repaint each line which contains highlighted text.
 * If on==0, force all hilites off.
 */
	public void
repaint_hilite(on)
	int on;
{
	int slinenum;
	POSITION pos;
	POSITION epos;
	int save_hide_hilite;

	if (squished)
		repaint();

	save_hide_hilite = hide_hilite;
	if (!on)
	{
		if (hide_hilite)
			return;
		hide_hilite = 1;
	}

	if (!can_goto_line)
	{
		repaint();
		hide_hilite = save_hide_hilite;
		return;
	}

	for (slinenum = TOP;  slinenum < TOP + sc_height-1;  slinenum++)
	{
		pos = position(slinenum);
		if (pos == NULL_POSITION)
			continue;
		epos = position(slinenum+1);
		/*
		 * If any character in the line is highlighted, 
		 * repaint the line.
		 */
		if (is_hilited(pos, epos, 1))
		{
			(void) forw_line(pos);
			goto_line(slinenum);
			put_line();
		}
	}
	hide_hilite = save_hide_hilite;
}

/*
 * Clear the attn hilite.
 */
	public void
clear_attn()
{
	int slinenum;
	POSITION old_start_attnpos;
	POSITION old_end_attnpos;
	POSITION pos;
	POSITION epos;

	if (start_attnpos == NULL_POSITION)
		return;
	old_start_attnpos = start_attnpos;
	old_end_attnpos = end_attnpos;
	start_attnpos = end_attnpos = NULL_POSITION;

	if (!can_goto_line)
	{
		repaint();
		return;
	}
	if (squished)
		repaint();

	for (slinenum = TOP;  slinenum < TOP + sc_height-1;  slinenum++)
	{
		pos = position(slinenum);
		if (pos == NULL_POSITION)
			continue;
		epos = position(slinenum+1);
		if (pos < old_end_attnpos &&
		     (epos == NULL_POSITION || epos > old_start_attnpos))
		{
			(void) forw_line(pos);
			goto_line(slinenum);
			put_line();
		}
	}
}
#endif

/*
 * Hide search string highlighting.
 */
	public void
undo_search()
{
	if (!prev_pattern())
	{
		error("No previous regular expression", NULL_PARG);
		return;
	}
#if HILITE_SEARCH
	hide_hilite = !hide_hilite;
	repaint_hilite(1);
#endif
}

/*
 * Compile a search pattern, for future use by match_pattern.
 */
	static int
compile_pattern(pattern, search_type)
	char *pattern;
	int search_type;
{
	if ((search_type & SRCH_NO_REGEX) == 0)
	{
#if HAVE_POSIX_REGCOMP
		regex_t *s = (regex_t *) ecalloc(1, sizeof(regex_t));
		if (regcomp(s, pattern, REGCOMP_FLAG))
		{
			free(s);
			error("Invalid pattern", NULL_PARG);
			return (-1);
		}
		if (regpattern != NULL)
			regfree(regpattern);
		regpattern = s;
#endif
#if HAVE_PCRE
		pcre *comp;
		const char *errstring;
		int erroffset;
		PARG parg;
		comp = pcre_compile(pattern, 0,
				&errstring, &erroffset, NULL);
		if (comp == NULL)
		{
			parg.p_string = (char *) errstring;
			error("%s", &parg);
			return (-1);
		}
		regpattern = comp;
#endif
#if HAVE_RE_COMP
		PARG parg;
		if ((parg.p_string = re_comp(pattern)) != NULL)
		{
			error("%s", &parg);
			return (-1);
		}
		re_pattern = 1;
#endif
#if HAVE_REGCMP
		char *s;
		if ((s = regcmp(pattern, 0)) == NULL)
		{
			error("Invalid pattern", NULL_PARG);
			return (-1);
		}
		if (cpattern != NULL)
			free(cpattern);
		cpattern = s;
#endif
#if HAVE_V8_REGCOMP
		struct regexp *s;
		if ((s = regcomp(pattern)) == NULL)
		{
			/*
			 * regcomp has already printed an error message 
			 * via regerror().
			 */
			return (-1);
		}
		if (regpattern != NULL)
			free(regpattern);
		regpattern = s;
#endif
	}

	if (last_pattern != NULL)
		free(last_pattern);
	last_pattern = (char *) calloc(1, strlen(pattern)+1);
	if (last_pattern != NULL)
		strcpy(last_pattern, pattern);

	last_search_type = search_type;
	return (0);
}

/*
 * Forget that we have a compiled pattern.
 */
	static void
uncompile_pattern()
{
#if HAVE_POSIX_REGCOMP
	if (regpattern != NULL)
		regfree(regpattern);
	regpattern = NULL;
#endif
#if HAVE_PCRE
	if (regpattern != NULL)
		pcre_free(regpattern);
	regpattern = NULL;
#endif
#if HAVE_RE_COMP
	re_pattern = 0;
#endif
#if HAVE_REGCMP
	if (cpattern != NULL)
		free(cpattern);
	cpattern = NULL;
#endif
#if HAVE_V8_REGCOMP
	if (regpattern != NULL)
		free(regpattern);
	regpattern = NULL;
#endif
	last_pattern = NULL;
}

/*
 * Perform a pattern match with the previously compiled pattern.
 * Set sp and ep to the start and end of the matched string.
 */
	static int
match_pattern(line, sp, ep, notbol)
	char *line;
	char **sp;
	char **ep;
	int notbol;
{
	int matched;

	if (last_search_type & SRCH_NO_REGEX)
		return (match(last_pattern, line, sp, ep));

#if HAVE_POSIX_REGCOMP
	{
		regmatch_t rm;
		int flags = (notbol) ? REG_NOTBOL : 0;
		matched = !regexec(regpattern, line, 1, &rm, flags);
		if (!matched)
			return (0);
#ifndef __WATCOMC__
		*sp = line + rm.rm_so;
		*ep = line + rm.rm_eo;
#else
		*sp = rm.rm_sp;
		*ep = rm.rm_ep;
#endif
	}
#endif
#if HAVE_PCRE
	{
		int flags = (notbol) ? PCRE_NOTBOL : 0;
		int ovector[3];
		matched = pcre_exec(regpattern, NULL, line, strlen(line),
			0, flags, ovector, 3) >= 0;
		if (!matched)
			return (0);
		*sp = line + ovector[0];
		*ep = line + ovector[1];
	}
#endif
#if HAVE_RE_COMP
	matched = (re_exec(line) == 1);
	/*
	 * re_exec doesn't seem to provide a way to get the matched string.
	 */
	*sp = *ep = NULL;
#endif
#if HAVE_REGCMP
	*ep = regex(cpattern, line);
	matched = (*ep != NULL);
	if (!matched)
		return (0);
	*sp = __loc1;
#endif
#if HAVE_V8_REGCOMP
#if HAVE_REGEXEC2
	matched = regexec2(regpattern, line, notbol);
#else
	matched = regexec(regpattern, line);
#endif
	if (!matched)
		return (0);
	*sp = regpattern->startp[0];
	*ep = regpattern->endp[0];
#endif
#if NO_REGEX
	matched = match(last_pattern, line, sp, ep);
#endif
	return (matched);
}

#if HILITE_SEARCH
/*
 * Clear the hilite list.
 */
	public void
clr_hilite()
{
	struct hilite *hl;
	struct hilite *nexthl;

	for (hl = hilite_anchor.hl_first;  hl != NULL;  hl = nexthl)
	{
		nexthl = hl->hl_next;
		free((void*)hl);
	}
	hilite_anchor.hl_first = NULL;
	prep_startpos = prep_endpos = NULL_POSITION;
}

/*
 * Should any characters in a specified range be highlighted?
 * If nohide is nonzero, don't consider hide_hilite.
 */
	public int
is_hilited(pos, epos, nohide)
	POSITION pos;
	POSITION epos;
	int nohide;
{
	struct hilite *hl;

	if (!status_col &&
	    start_attnpos != NULL_POSITION && 
	    pos < end_attnpos &&
	     (epos == NULL_POSITION || epos > start_attnpos))
		/*
		 * The attn line overlaps this range.
		 */
		return (1);

	if (hilite_search == 0)
		/*
		 * Not doing highlighting.
		 */
		return (0);

	if (!nohide && hide_hilite)
		/*
		 * Highlighting is hidden.
		 */
		return (0);

	/*
	 * Look at each highlight and see if any part of it falls in the range.
	 */
	for (hl = hilite_anchor.hl_first;  hl != NULL;  hl = hl->hl_next)
	{
		if (hl->hl_endpos > pos &&
		    (epos == NULL_POSITION || epos > hl->hl_startpos))
			return (1);
	}
	return (0);
}

/*
 * Add a new hilite to a hilite list.
 */
	static void
add_hilite(anchor, hl)
	struct hilite *anchor;
	struct hilite *hl;
{
	struct hilite *ihl;

	/*
	 * Hilites are sorted in the list; find where new one belongs.
	 * Insert new one after ihl.
	 */
	for (ihl = anchor;  ihl->hl_next != NULL;  ihl = ihl->hl_next)
	{
		if (ihl->hl_next->hl_startpos > hl->hl_startpos)
			break;
	}

	/*
	 * Truncate hilite so it doesn't overlap any existing ones
	 * above and below it.
	 */
	if (ihl != anchor)
		hl->hl_startpos = MAXPOS(hl->hl_startpos, ihl->hl_endpos);
	if (ihl->hl_next != NULL)
		hl->hl_endpos = MINPOS(hl->hl_endpos, ihl->hl_next->hl_startpos);
	if (hl->hl_startpos >= hl->hl_endpos)
	{
		/*
		 * Hilite was truncated out of existence.
		 */
		free(hl);
		return;
	}
	hl->hl_next = ihl->hl_next;
	ihl->hl_next = hl;
}

/*
 * Adjust hl_startpos & hl_endpos to account for backspace processing.
 */
	static void
adj_hilite(anchor, linepos)
	struct hilite *anchor;
	POSITION linepos;
{
	char *line;
	struct hilite *hl;
	int checkstart;
	POSITION opos;
	POSITION npos;

	/*
	 * The line was already scanned and hilites were added (in hilite_line).
	 * But it was assumed that each char position in the line 
	 * correponds to one char position in the file.
	 * This may not be true if there are backspaces in the line.
	 * Get the raw line again.  Look at each character.
	 */
	(void) forw_raw_line(linepos, &line);
	opos = npos = linepos;
	hl = anchor->hl_first;
	checkstart = TRUE;
	while (hl != NULL)
	{
		/*
		 * See if we need to adjust the current hl_startpos or 
		 * hl_endpos.  After adjusting startpos[i], move to endpos[i].
		 * After adjusting endpos[i], move to startpos[i+1].
		 * The hilite list must be sorted thus: 
		 * startpos[0] < endpos[0] <= startpos[1] < endpos[1] <= etc.
		 */
		if (checkstart && hl->hl_startpos == opos)
		{
			hl->hl_startpos = npos;
			checkstart = FALSE;
			continue; /* {{ not really necessary }} */
		} else if (!checkstart && hl->hl_endpos == opos)
		{
			hl->hl_endpos = npos;
			checkstart = TRUE;
			hl = hl->hl_next;
			continue; /* {{ necessary }} */
		}
		if (*line == '\0')
			break;
		opos++;
		npos++;
		line++;
		while (line[0] == '\b' && line[1] != '\0')
		{
			/*
			 * Found a backspace.  The file position moves
			 * forward by 2 relative to the processed line
			 * which was searched in hilite_line.
			 */
			npos += 2;
			line += 2;
		}
	}
}

/*
 * Make a hilite for each string in a physical line which matches 
 * the current pattern.
 * sp,ep delimit the first match already found.
 */
	static void
hilite_line(linepos, line, sp, ep)
	POSITION linepos;
	char *line;
	char *sp;
	char *ep;
{
	char *searchp;
	struct hilite *hl;
	struct hilite hilites;

	if (sp == NULL || ep == NULL)
		return;
	/*
	 * sp and ep delimit the first match in the line.
	 * Mark the corresponding file positions, then
	 * look for further matches and mark them.
	 * {{ This technique, of calling match_pattern on subsequent
	 *    substrings of the line, may mark more than is correct
	 *    if the pattern starts with "^".  This bug is fixed
	 *    for those regex functions that accept a notbol parameter
	 *    (currently POSIX and V8-with-regexec2). }}
	 */
	searchp = line;
	/*
	 * Put the hilites into a temporary list until they're adjusted.
	 */
	hilites.hl_first = NULL;
	do {
		if (ep > sp)
		{
			/*
			 * Assume that each char position in the "line"
			 * buffer corresponds to one char position in the file.
			 * This is not quite true; we need to adjust later.
			 */
			hl = (struct hilite *) ecalloc(1, sizeof(struct hilite));
			hl->hl_startpos = linepos + (sp-line);
			hl->hl_endpos = linepos + (ep-line);
			add_hilite(&hilites, hl);
		}
		/*
		 * If we matched more than zero characters,
		 * move to the first char after the string we matched.
		 * If we matched zero, just move to the next char.
		 */
		if (ep > searchp)
			searchp = ep;
		else if (*searchp != '\0')
			searchp++;
		else /* end of line */
			break;
	} while (match_pattern(searchp, &sp, &ep, 1));

	if (bs_mode == BS_SPECIAL) 
	{
		/*
		 * If there were backspaces in the original line, they
		 * were removed, and hl_startpos/hl_endpos are not correct.
		 * {{ This is very ugly. }}
		 */
		adj_hilite(&hilites, linepos);
	}
	/*
	 * Now put the hilites into the real list.
	 */
	while ((hl = hilites.hl_next) != NULL)
	{
		hilites.hl_next = hl->hl_next;
		add_hilite(&hilite_anchor, hl);
	}
}
#endif

/*
 * Change the caseless-ness of searches.  
 * Updates the internal search state to reflect a change in the -i flag.
 */
	public void
chg_caseless()
{
	if (!is_ucase_pattern)
		/*
		 * Pattern did not have uppercase.
		 * Just set the search caselessness to the global caselessness.
		 */
		is_caseless = caseless;
	else
		/*
		 * Pattern did have uppercase.
		 * Discard the pattern; we can't change search caselessness now.
		 */
		uncompile_pattern();
}

#if HILITE_SEARCH
/*
 * Find matching text which is currently on screen and highlight it.
 */
	static void
hilite_screen()
{
	struct scrpos scrpos;

	get_scrpos(&scrpos);
	if (scrpos.pos == NULL_POSITION)
		return;
	prep_hilite(scrpos.pos, position(BOTTOM_PLUS_ONE), -1);
	repaint_hilite(1);
}

/*
 * Change highlighting parameters.
 */
	public void
chg_hilite()
{
	/*
	 * Erase any highlights currently on screen.
	 */
	clr_hilite();
	hide_hilite = 0;

	if (hilite_search == OPT_ONPLUS)
		/*
		 * Display highlights.
		 */
		hilite_screen();
}
#endif

/*
 * Figure out where to start a search.
 */
	static POSITION
search_pos(search_type)
	int search_type;
{
	POSITION pos;
	int linenum;

	if (empty_screen())
	{
		/*
		 * Start at the beginning (or end) of the file.
		 * The empty_screen() case is mainly for 
		 * command line initiated searches;
		 * for example, "+/xyz" on the command line.
		 * Also for multi-file (SRCH_PAST_EOF) searches.
		 */
		if (search_type & SRCH_FORW)
		{
			return (ch_zero());
		} else
		{
			pos = ch_length();
			if (pos == NULL_POSITION)
			{
				(void) ch_end_seek();
				pos = ch_length();
			}
			return (pos);
		}
	}
	if (how_search)
	{
		/*
		 * Search does not include current screen.
		 */
		if (search_type & SRCH_FORW)
			linenum = BOTTOM_PLUS_ONE;
		else
			linenum = TOP;
		pos = position(linenum);
	} else
	{
		/*
		 * Search includes current screen.
		 * It starts at the jump target (if searching backwards),
		 * or at the jump target plus one (if forwards).
		 */
		linenum = adjsline(jump_sline);
		pos = position(linenum);
		if (search_type & SRCH_FORW)
		{
			pos = forw_raw_line(pos, (char **)NULL);
			while (pos == NULL_POSITION)
			{
				if (++linenum >= sc_height)
					break;
				pos = position(linenum);
			}
		} else 
		{
			while (pos == NULL_POSITION)
			{
				if (--linenum < 0)
					break;
				pos = position(linenum);
			}
		}
	}
	return (pos);
}

/*
 * Search a subset of the file, specified by start/end position.
 */
	static int
search_range(pos, endpos, search_type, matches, maxlines, plinepos, pendpos)
	POSITION pos;
	POSITION endpos;
	int search_type;
	int matches;
	int maxlines;
	POSITION *plinepos;
	POSITION *pendpos;
{
	char *line;
	int linenum;
	char *sp, *ep;
	int line_match;
	POSITION linepos, oldpos;

	linenum = find_linenum(pos);
	oldpos = pos;
	for (;;)
	{
		/*
		 * Get lines until we find a matching one or until
		 * we hit end-of-file (or beginning-of-file if we're 
		 * going backwards), or until we hit the end position.
		 */
		if (ABORT_SIGS())
		{
			/*
			 * A signal aborts the search.
			 */
			return (-1);
		}

		if ((endpos != NULL_POSITION && pos >= endpos) || maxlines == 0)
		{
			/*
			 * Reached end position without a match.
			 */
			if (pendpos != NULL)
				*pendpos = pos;
			return (matches);
		}
		if (maxlines > 0)
			maxlines--;

		if (search_type & SRCH_FORW)
		{
			/*
			 * Read the next line, and save the 
			 * starting position of that line in linepos.
			 */
			linepos = pos;
			pos = forw_raw_line(pos, &line);
			if (linenum != 0)
				linenum++;
		} else
		{
			/*
			 * Read the previous line and save the
			 * starting position of that line in linepos.
			 */
			pos = back_raw_line(pos, &line);
			linepos = pos;
			if (linenum != 0)
				linenum--;
		}

		if (pos == NULL_POSITION)
		{
			/*
			 * Reached EOF/BOF without a match.
			 */
			if (pendpos != NULL)
				*pendpos = oldpos;
			return (matches);
		}

		/*
		 * If we're using line numbers, we might as well
		 * remember the information we have now (the position
		 * and line number of the current line).
		 * Don't do it for every line because it slows down
		 * the search.  Remember the line number only if
		 * we're "far" from the last place we remembered it.
		 */
		if (linenums && abs((int)(pos - oldpos)) > 1024)
			add_lnum(linenum, pos);
		oldpos = pos;

		/*
		 * If it's a caseless search, convert the line to lowercase.
		 * If we're doing backspace processing, delete backspaces.
		 */
		if (is_caseless || bs_mode == BS_SPECIAL)
		{
			int ops = 0;
			if (is_caseless) 
				ops |= CVT_TO_LC;
			if (bs_mode == BS_SPECIAL)
				ops |= CVT_BS;
			if (bs_mode != BS_CONTROL)
				ops |= CVT_CRLF;
			cvt_text(line, line, ops);
		} else if (bs_mode != BS_CONTROL)
		{
			cvt_text(line, line, CVT_CRLF);
		}

		/*
		 * Test the next line to see if we have a match.
		 * We are successful if we either want a match and got one,
		 * or if we want a non-match and got one.
		 */
		line_match = match_pattern(line, &sp, &ep, 0);
		line_match = (!(search_type & SRCH_NO_MATCH) && line_match) ||
				((search_type & SRCH_NO_MATCH) && !line_match);
		if (!line_match)
			continue;
		/*
		 * Got a match.
		 */
		if (search_type & SRCH_FIND_ALL)
		{
#if HILITE_SEARCH
			/*
			 * We are supposed to find all matches in the range.
			 * Just add the matches in this line to the 
			 * hilite list and keep searching.
			 */
			if (line_match)
				hilite_line(linepos, line, sp, ep);
#endif
		} else if (--matches <= 0)
		{
			/*
			 * Found the one match we're looking for.
			 * Return it.
			 */
#if HILITE_SEARCH
			if (hilite_search == 1)
			{
				/*
				 * Clear the hilite list and add only
				 * the matches in this one line.
				 */
				clr_hilite();
				if (line_match)
					hilite_line(linepos, line, sp, ep);
			}
#endif
			if (plinepos != NULL)
				*plinepos = linepos;
			return (0);
		}
	}
}

/*
 * Search for the n-th occurrence of a specified pattern, 
 * either forward or backward.
 * Return the number of matches not yet found in this file
 * (that is, n minus the number of matches found).
 * Return -1 if the search should be aborted.
 * Caller may continue the search in another file 
 * if less than n matches are found in this file.
 */
	public int
search(search_type, pattern, n)
	int search_type;
	char *pattern;
	int n;
{
	POSITION pos;
	int ucase;

	if (pattern == NULL || *pattern == '\0')
	{
		/*
		 * A null pattern means use the previously compiled pattern.
		 */
		if (!prev_pattern())
		{
			error("No previous regular expression", NULL_PARG);
			return (-1);
		}
		if ((search_type & SRCH_NO_REGEX) != 
		    (last_search_type & SRCH_NO_REGEX))
		{
			error("Please re-enter search pattern", NULL_PARG);
			return -1;
		}
#if HILITE_SEARCH
		if (hilite_search == OPT_ON)
		{
			/*
			 * Erase the highlights currently on screen.
			 * If the search fails, we'll redisplay them later.
			 */
			repaint_hilite(0);
		}
		if (hilite_search == OPT_ONPLUS && hide_hilite)
		{
			/*
			 * Highlight any matches currently on screen,
			 * before we actually start the search.
			 */
			hide_hilite = 0;
			hilite_screen();
		}
		hide_hilite = 0;
#endif
	} else
	{
		/*
		 * Compile the pattern.
		 */
		ucase = is_ucase(pattern);
		if (caseless == OPT_ONPLUS)
			cvt_text(pattern, pattern, CVT_TO_LC);
		if (compile_pattern(pattern, search_type) < 0)
			return (-1);
		/*
		 * Ignore case if -I is set OR
		 * -i is set AND the pattern is all lowercase.
		 */
		is_ucase_pattern = ucase;
		if (is_ucase_pattern && caseless != OPT_ONPLUS)
			is_caseless = 0;
		else
			is_caseless = caseless;
#if HILITE_SEARCH
		if (hilite_search)
		{
			/*
			 * Erase the highlights currently on screen.
			 * Also permanently delete them from the hilite list.
			 */
			repaint_hilite(0);
			hide_hilite = 0;
			clr_hilite();
		}
		if (hilite_search == OPT_ONPLUS)
		{
			/*
			 * Highlight any matches currently on screen,
			 * before we actually start the search.
			 */
			hilite_screen();
		}
#endif
	}

	/*
	 * Figure out where to start the search.
	 */
	pos = search_pos(search_type);
	if (pos == NULL_POSITION)
	{
		/*
		 * Can't find anyplace to start searching from.
		 */
		if (search_type & SRCH_PAST_EOF)
			return (n);
		/* repaint(); -- why was this here? */
		error("Nothing to search", NULL_PARG);
		return (-1);
	}

	n = search_range(pos, NULL_POSITION, search_type, n, -1,
			&pos, (POSITION*)NULL);
	if (n != 0)
	{
		/*
		 * Search was unsuccessful.
		 */
#if HILITE_SEARCH
		if (hilite_search == OPT_ON && n > 0)
			/*
			 * Redisplay old hilites.
			 */
			repaint_hilite(1);
#endif
		return (n);
	}

	if (!(search_type & SRCH_NO_MOVE))
	{
		/*
		 * Go to the matching line.
		 */
		jump_loc(pos, jump_sline);
	}

#if HILITE_SEARCH
	if (hilite_search == OPT_ON)
		/*
		 * Display new hilites in the matching line.
		 */
		repaint_hilite(1);
#endif
	return (0);
}


#if HILITE_SEARCH
/*
 * Prepare hilites in a given range of the file.
 *
 * The pair (prep_startpos,prep_endpos) delimits a contiguous region
 * of the file that has been "prepared"; that is, scanned for matches for
 * the current search pattern, and hilites have been created for such matches.
 * If prep_startpos == NULL_POSITION, the prep region is empty.
 * If prep_endpos == NULL_POSITION, the prep region extends to EOF.
 * prep_hilite asks that the range (spos,epos) be covered by the prep region.
 */
	public void
prep_hilite(spos, epos, maxlines)
	POSITION spos;
	POSITION epos;
	int maxlines;
{
	POSITION nprep_startpos = prep_startpos;
	POSITION nprep_endpos = prep_endpos;
	POSITION new_epos;
	POSITION max_epos;
	int result;
	int i;
/*
 * Search beyond where we're asked to search, so the prep region covers
 * more than we need.  Do one big search instead of a bunch of small ones.
 */
#define	SEARCH_MORE (3*size_linebuf)

	if (!prev_pattern())
		return;

	/*
	 * If we're limited to a max number of lines, figure out the
	 * file position we should stop at.
	 */
	if (maxlines < 0)
		max_epos = NULL_POSITION;
	else
	{
		max_epos = spos;
		for (i = 0;  i < maxlines;  i++)
			max_epos = forw_raw_line(max_epos, (char **)NULL);
	}

	/*
	 * Find two ranges:
	 * The range that we need to search (spos,epos); and the range that
	 * the "prep" region will then cover (nprep_startpos,nprep_endpos).
	 */

	if (prep_startpos == NULL_POSITION ||
	    (epos != NULL_POSITION && epos < prep_startpos) ||
	    spos > prep_endpos)
	{
		/*
		 * New range is not contiguous with old prep region.
		 * Discard the old prep region and start a new one.
		 */
		clr_hilite();
		if (epos != NULL_POSITION)
			epos += SEARCH_MORE;
		nprep_startpos = spos;
	} else
	{
		/*
		 * New range partially or completely overlaps old prep region.
		 */
		if (epos == NULL_POSITION)
		{
			/*
			 * New range goes to end of file.
			 */
			;
		} else if (epos > prep_endpos)
		{
			/*
			 * New range ends after old prep region.
			 * Extend prep region to end at end of new range.
			 */
			epos += SEARCH_MORE;
		} else /* (epos <= prep_endpos) */
		{
			/*
			 * New range ends within old prep region.
			 * Truncate search to end at start of old prep region.
			 */
			epos = prep_startpos;
		}

		if (spos < prep_startpos)
		{
			/*
			 * New range starts before old prep region.
			 * Extend old prep region backwards to start at 
			 * start of new range.
			 */
			if (spos < SEARCH_MORE)
				spos = 0;
			else
				spos -= SEARCH_MORE;
			nprep_startpos = spos;
		} else /* (spos >= prep_startpos) */
		{
			/*
			 * New range starts within or after old prep region.
			 * Trim search to start at end of old prep region.
			 */
			spos = prep_endpos;
		}
	}

	if (epos != NULL_POSITION && max_epos != NULL_POSITION &&
	    epos > max_epos)
		/*
		 * Don't go past the max position we're allowed.
		 */
		epos = max_epos;

	if (epos == NULL_POSITION || epos > spos)
	{
		result = search_range(spos, epos, SRCH_FORW|SRCH_FIND_ALL, 0,
				maxlines, (POSITION*)NULL, &new_epos);
		if (result < 0)
			return;
		if (prep_endpos == NULL_POSITION || new_epos > prep_endpos)
			nprep_endpos = new_epos;
	}
	prep_startpos = nprep_startpos;
	prep_endpos = nprep_endpos;
}
#endif

/*
 * Simple pattern matching function.
 * It supports no metacharacters like *, etc.
 */
	static int
match(pattern, buf, pfound, pend)
	char *pattern, *buf;
	char **pfound, **pend;
{
	register char *pp, *lp;

	for ( ;  *buf != '\0';  buf++)
	{
		for (pp = pattern, lp = buf;  *pp == *lp;  pp++, lp++)
			if (*pp == '\0' || *lp == '\0')
				break;
		if (*pp == '\0')
		{
			if (pfound != NULL)
				*pfound = buf;
			if (pend != NULL)
				*pend = lp;
			return (1);
		}
	}
	return (0);
}

#if HAVE_V8_REGCOMP
/*
 * This function is called by the V8 regcomp to report 
 * errors in regular expressions.
 */
	void 
regerror(s) 
	char *s; 
{
	PARG parg;

	parg.p_string = s;
	error("%s", &parg);
}
#endif

