/*
 * Copyright (C) 1984-2025  Mark Nudelman
 *
 * You may distribute under the terms of either the GNU General Public
 * License or the Less License, as specified in the README file.
 *
 * For more information, see the README file.
 */

/*
 * Code to support expanding environment variables in text.
 */

#include "less.h"
#include "xbuf.h"

struct replace {
	struct replace *r_next;
	char *r_fm;
	char *r_to;
};

/*
 * Skip to the next unescaped slash or right curly bracket in a string.
 */
static size_t skipsl(constant char *buf, size_t len, size_t e)
{
	lbool esc = FALSE;
	while (e < len && buf[e] != '\0' && (esc || (buf[e] != '/' && buf[e] != '}')))
	{
		esc = (!esc && buf[e] == '\\' && buf[e+1] != '\0');
		++e;
	}
	return e;
}

/*
 * Parse a replacement string: one or more instances of
 * (slash, pattern, slash, replacement), followed by right curly bracket.
 * Replacement may be empty in which case the second slash is optional.
 */
static struct replace * make_replaces(mutable char *buf, size_t len, size_t *pe, char term)
{
	size_t e = *pe;
	struct replace *replaces = NULL;

	while (term == '/')
	{
		struct replace *repl;
		size_t to;
		size_t fm = e;
		e = skipsl(buf, len, e);
		if (e >= len) break;
		if (e == fm) /* missing fm string; we're done */
		{
			while (e < len)
				if (buf[e++] == '}') break;
			break;
		}
		term = buf[e];
		buf[e++] = '\0'; /* terminate the fm string */
		if (term != '/') /* missing to string */
		{
			to = e-1;
		} else
		{
			to = e;
			e = skipsl(buf, len, e);
			if (e >= len) break;
			term = buf[e];
			buf[e++] = '\0'; /* terminate the to string */
		}
		repl = ecalloc(1, sizeof(struct replace));
		repl->r_fm = &buf[fm];
		repl->r_to = &buf[to];
		repl->r_next = replaces;
		replaces = repl;
	}
	*pe = e;
	return replaces;
}

/*
 * Free a list of replace structs.
 */
static void free_replaces(struct replace *replaces)
{
	while (replaces != NULL)
	{
		struct replace *r = replaces;
		replaces = r->r_next;
		free(r);
	}
}

/*
 * See if the initial substring of a string matches a pattern.
 * Backslash escapes in the pattern are ignored.
 * Return the length of the matched substring, or 0 if no match.
 */
static size_t evar_match(constant char *str, constant char *pat)
{
	size_t len = 0;
	while (*pat != '\0')
	{
		if (*pat == '\\') ++pat;
		if (*str++ != *pat++) return 0;
		++len;
	}
	return len;
}

/*
 * Find the replacement for a string (&evar[*pv]),
 * given a list of replace structs.
 */
static constant char * find_replace(constant struct replace *repl, constant char *evar, size_t *pv)
{
	for (;  repl != NULL;  repl = repl->r_next)
	{
		size_t len = evar_match(&evar[*pv], repl->r_fm);
		if (len > 0)
		{
			*pv += len;
			return repl->r_to;
		}
	}
	return NULL;
}

/*
 * With buf[e] positioned just after NAME in "${NAME" and 
 * term containing the character at that point, parse the rest
 * of the environment var string (including the final right curly bracket).
 * Write evar to xbuf, performing any specified text replacements.
 * Return the new value of e to point just after the final right curly bracket.
 */
static size_t add_evar(struct xbuffer *xbuf, mutable char *buf, size_t len, size_t e, constant char *evar, char term)
{
	struct replace *replaces = make_replaces(buf, len, &e, term);
	size_t v;

	for (v = 0;  evar[v] != '\0'; )
	{
		constant char *repl = find_replace(replaces, evar, &v);
		if (repl == NULL)
			xbuf_add_char(xbuf, evar[v++]);
		else
		{
			size_t r;
			for (r = 0;  repl[r] != '\0';  r++)
			{
				if (repl[r] == '\\' && repl[r+1] != '\0') ++r;
				xbuf_add_char(xbuf, repl[r]);
			}
		}
	}
	free_replaces(replaces);
	return e;
}

/*
 * Expand env variables in a string.
 * Writes expanded output to xbuf. Corrupts buf.
 */
public void expand_evars(mutable char *buf, size_t len, struct xbuffer *xbuf)
{
	size_t i;
	for (i = 0;  i < len; )
	{
		if (i+1 < len && buf[i] == '$' && buf[i+1] == '{')
		{
			constant char *evar;
			char term;
			size_t e;
			i += 2; /* skip "${" */
			for (e = i;  e < len;  e++)
				if (buf[e] == '\0' || buf[e] == '}' || buf[e] == '/')
					break;
			if (e >= len || buf[e] == '\0')
				break; /* missing right curly bracket; ignore var */
			term = buf[e];
			buf[e++] = '\0';
			evar = lgetenv_ext(&buf[i], xbuf->data, xbuf->end);
			if (evar == NULL) evar = "";
			i = add_evar(xbuf, buf, len, e, evar, term);
		} else
		{
			xbuf_add_char(xbuf, buf[i++]);
		}
	}
}
