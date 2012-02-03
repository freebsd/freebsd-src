/*
 * Copyright (C) 1984-2011  Mark Nudelman
 *
 * You may distribute under the terms of either the GNU General Public
 * License or the Less License, as specified in the README file.
 *
 * For more information about less, or for information on how to 
 * contact the author, see the README file.
 */

/*
 * Routines to do pattern matching.
 */

#include "less.h"
#include "pattern.h"

extern int caseless;

/*
 * Compile a search pattern, for future use by match_pattern.
 */
	static int
compile_pattern2(pattern, search_type, comp_pattern)
	char *pattern;
	int search_type;
	void **comp_pattern;
{
	if ((search_type & SRCH_NO_REGEX) == 0)
	{
#if HAVE_POSIX_REGCOMP
		regex_t *comp = (regex_t *) ecalloc(1, sizeof(regex_t));
		regex_t **pcomp = (regex_t **) comp_pattern;
		if (regcomp(comp, pattern, REGCOMP_FLAG))
		{
			free(comp);
			error("Invalid pattern", NULL_PARG);
			return (-1);
		}
		if (*pcomp != NULL)
			regfree(*pcomp);
		*pcomp = comp;
#endif
#if HAVE_PCRE
		pcre *comp;
		pcre **pcomp = (pcre **) comp_pattern;
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
		*pcomp = comp;
#endif
#if HAVE_RE_COMP
		PARG parg;
		int *pcomp = (int *) comp_pattern;
		if ((parg.p_string = re_comp(pattern)) != NULL)
		{
			error("%s", &parg);
			return (-1);
		}
		*pcomp = 1;
#endif
#if HAVE_REGCMP
		char *comp;
		char **pcomp = (char **) comp_pattern;
		if ((comp = regcmp(pattern, 0)) == NULL)
		{
			error("Invalid pattern", NULL_PARG);
			return (-1);
		}
		if (pcomp != NULL)
			free(*pcomp);
		*pcomp = comp;
#endif
#if HAVE_V8_REGCOMP
		struct regexp *comp;
		struct regexp **pcomp = (struct regexp **) comp_pattern;
		if ((comp = regcomp(pattern)) == NULL)
		{
			/*
			 * regcomp has already printed an error message 
			 * via regerror().
			 */
			return (-1);
		}
		if (*pcomp != NULL)
			free(*pcomp);
		*pcomp = comp;
#endif
	}
	return (0);
}

/*
 * Like compile_pattern2, but convert the pattern to lowercase if necessary.
 */
	public int
compile_pattern(pattern, search_type, comp_pattern)
	char *pattern;
	int search_type;
	void **comp_pattern;
{
	char *cvt_pattern;
	int result;

	if (caseless != OPT_ONPLUS)
		cvt_pattern = pattern;
	else
	{
		cvt_pattern = (char*) ecalloc(1, cvt_length(strlen(pattern), CVT_TO_LC));
		cvt_text(cvt_pattern, pattern, (int *)NULL, (int *)NULL, CVT_TO_LC);
	}
	result = compile_pattern2(cvt_pattern, search_type, comp_pattern);
	if (cvt_pattern != pattern)
		free(cvt_pattern);
	return (result);
}

/*
 * Forget that we have a compiled pattern.
 */
	public void
uncompile_pattern(pattern)
	void **pattern;
{
#if HAVE_POSIX_REGCOMP
	regex_t **pcomp = (regex_t **) pattern;
	if (*pcomp != NULL)
		regfree(*pcomp);
	*pcomp = NULL;
#endif
#if HAVE_PCRE
	pcre **pcomp = (pcre **) pattern;
	if (*pcomp != NULL)
		pcre_free(*pcomp);
	*pcomp = NULL;
#endif
#if HAVE_RE_COMP
	int *pcomp = (int *) pattern;
	*pcomp = 0;
#endif
#if HAVE_REGCMP
	char **pcomp = (char **) pattern;
	if (*pcomp != NULL)
		free(*pcomp);
	*pcomp = NULL;
#endif
#if HAVE_V8_REGCOMP
	struct regexp **pcomp = (struct regexp **) pattern;
	if (*pcomp != NULL)
		free(*pcomp);
	*pcomp = NULL;
#endif
}

/*
 * Is a compiled pattern null?
 */
	public int
is_null_pattern(pattern)
	void *pattern;
{
#if HAVE_POSIX_REGCOMP
	return (pattern == NULL);
#endif
#if HAVE_PCRE
	return (pattern == NULL);
#endif
#if HAVE_RE_COMP
	return (pattern == 0);
#endif
#if HAVE_REGCMP
	return (pattern == NULL);
#endif
#if HAVE_V8_REGCOMP
	return (pattern == NULL);
#endif
#if NO_REGEX
	return (search_pattern != NULL);
#endif
}

/*
 * Simple pattern matching function.
 * It supports no metacharacters like *, etc.
 */
	static int
match(pattern, pattern_len, buf, buf_len, pfound, pend)
	char *pattern;
	int pattern_len;
	char *buf;
	int buf_len;
	char **pfound, **pend;
{
	register char *pp, *lp;
	register char *pattern_end = pattern + pattern_len;
	register char *buf_end = buf + buf_len;

	for ( ;  buf < buf_end;  buf++)
	{
		for (pp = pattern, lp = buf;  *pp == *lp;  pp++, lp++)
			if (pp == pattern_end || lp == buf_end)
				break;
		if (pp == pattern_end)
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

/*
 * Perform a pattern match with the previously compiled pattern.
 * Set sp and ep to the start and end of the matched string.
 */
	public int
match_pattern(pattern, tpattern, line, line_len, sp, ep, notbol, search_type)
	void *pattern;
	char *tpattern;
	char *line;
	int line_len;
	char **sp;
	char **ep;
	int notbol;
	int search_type;
{
	int matched;
#if HAVE_POSIX_REGCOMP
	regex_t *spattern = (regex_t *) pattern;
#endif
#if HAVE_PCRE
	pcre *spattern = (pcre *) pattern;
#endif
#if HAVE_RE_COMP
	int spattern = (int) pattern;
#endif
#if HAVE_REGCMP
	char *spattern = (char *) pattern;
#endif
#if HAVE_V8_REGCOMP
	struct regexp *spattern = (struct regexp *) pattern;
#endif

	if (search_type & SRCH_NO_REGEX)
		matched = match(tpattern, strlen(tpattern), line, line_len, sp, ep);
	else
	{
#if HAVE_POSIX_REGCOMP
	{
		regmatch_t rm;
		int flags = (notbol) ? REG_NOTBOL : 0;
		matched = !regexec(spattern, line, 1, &rm, flags);
		if (matched)
		{
#ifndef __WATCOMC__
			*sp = line + rm.rm_so;
			*ep = line + rm.rm_eo;
#else
			*sp = rm.rm_sp;
			*ep = rm.rm_ep;
#endif
		}
	}
#endif
#if HAVE_PCRE
	{
		int flags = (notbol) ? PCRE_NOTBOL : 0;
		int ovector[3];
		matched = pcre_exec(spattern, NULL, line, line_len,
			0, flags, ovector, 3) >= 0;
		if (matched)
		{
			*sp = line + ovector[0];
			*ep = line + ovector[1];
		}
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
	*ep = regex(spattern, line);
	matched = (*ep != NULL);
	if (matched)
		*sp = __loc1;
#endif
#if HAVE_V8_REGCOMP
#if HAVE_REGEXEC2
	matched = regexec2(spattern, line, notbol);
#else
	matched = regexec(spattern, line);
#endif
	if (matched)
	{
		*sp = spattern->startp[0];
		*ep = spattern->endp[0];
	}
#endif
#if NO_REGEX
	matched = match(tpattern, strlen(tpattern), line, line_len, sp, ep);
#endif
	}
	matched = (!(search_type & SRCH_NO_MATCH) && matched) ||
			((search_type & SRCH_NO_MATCH) && !matched);
	return (matched);
}

