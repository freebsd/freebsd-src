/*
 * Copyright (c) 2013-2026 Devin Teske <dteske@FreeBSD.org>
 * Copyright (c) 2021-2026 Faraz Vahedi <kfv@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * Edits that add or strike elements of a UCL array, one element at a
 * time, in the style of the elements around them.
 */


#include <ctype.h>
#include <stdckdint.h>

#include <ucl.h>

#include "sysconf_priv.h"
#include "sysconf_ucl.h"

/* One element located inside `[ ... ]' */
struct uelem {
	size_t	start;		/* first byte of the element */
	size_t	end;		/* past the element and its terminator */
	size_t	val_end;	/* past the element text */
	size_t	line_start;	/* start of the line holding it */
	size_t	line_end;	/* past the newline ending it */
	char	term;		/* ',' or 0 */
	bool	alone;		/* nothing else shares its lines */
	bool	quoted;		/* a quoted string */
	bool	keep;		/* survives the edit */
};

/*
 * Enumerate the elements of the array whose `[' is at `open'. Returns
 * the count (malloc'd into `*elemsp') and the position of `]' in
 * `*closep', or -1 on a lexing failure.
 */
static int
enum_array(const char *s, size_t n, size_t open, struct uelem **elemsp,
    size_t *closep)
{
	struct uelem *elems = NULL;
	struct uelem e;
	size_t cap = 0;
	size_t i;
	size_t j;
	size_t k;
	int count = 0;

	for (i = open + 1;;) {
		j = skip_ws(s, n, i, true, true);
		if (j >= n)
			return (-1);
		if (s[j] == ']') {
			*closep = j;
			break;
		}
		memset(&e, 0, sizeof(e));
		e.start = j;
		switch (s[j]) {
		case '{':
		case '[':
			k = skip_balanced(s, n, j);
			break;
		case '"':
		case '\'':
			e.quoted = true;
			k = skip_quoted(s, n, j);
			break;
		default:
			k = j;
			while (k < n && s[k] != '\n' && s[k] != ',' &&
			    s[k] != ']' && !comment_at(s, n, k))
				k++;
			while (k > j && (s[k - 1] == ' ' || s[k - 1] == '\t' ||
			    s[k - 1] == '\r'))
				k--;
			break;
		}
		if (k == j)
			return (-1);
		e.val_end = k;
		k = skip_ws(s, n, k, false, false);
		if (k < n && s[k] == ',') {
			e.term = ',';
			e.end = k + 1;
		} else
			e.end = e.val_end;
		e.line_start = line_begin(s, e.start);
		for (k = e.end; k < n && s[k] != '\n'; k++)
			continue;
		e.line_end = (k < n) ? k + 1 : k;
		e.alone = blank_span(s, e.line_start, e.start) &&
		    tail_span(s, n, e.end, k);
		if ((size_t)count == cap) {
			cap = cap ? cap * 2 : 8;
			if ((elems = reallocarray(elems, cap,
			    sizeof(*elems))) == NULL)
				err(EXIT_FAILURE, NULL);
		}
		elems[count++] = e;
		i = e.end;
	}
	*elemsp = elems;
	return (count);
}

/* What libucl makes of the element text `s[0,n)': NULL if nothing */
static ucl_object_t *
elem_object(const char *s, size_t n)
{
	struct ucl_parser *parser;
	ucl_object_t *top = NULL;
	ucl_object_t *val = NULL;
	const ucl_object_t *v;
	char *chunk;
	size_t size;

	if (ckd_add(&size, n, sizeof("v = \n")))
		errx(EXIT_FAILURE, "%s", strerror(EOVERFLOW));
	if ((chunk = malloc(size)) == NULL)
		err(EXIT_FAILURE, NULL);
	memcpy(chunk, "v = ", 4);
	memcpy(chunk + 4, s, n);
	memcpy(chunk + 4 + n, "\n", 2);
	if ((parser = ucl_parser_new(UCL_PARSE_FLAGS)) == NULL)
		err(EXIT_FAILURE, NULL);
	if (ucl_parser_add_string(parser, chunk, strlen(chunk)) &&
	    (top = ucl_parser_get_object(parser)) != NULL &&
	    (v = ucl_object_lookup(top, "v")) != NULL)
		val = ucl_object_copy(v);
	if (top != NULL)
		ucl_object_unref(top);
	ucl_parser_free(parser);
	free(chunk);
	return (val);
}

/* Byte range to cut from the text */
struct ucut {
	size_t	from;
	size_t	to;
};

static int
ucut_cmp(const void *a, const void *b)
{
	const struct ucut *x = a;
	const struct ucut *y = b;

	return (x->from < y->from ? -1 : x->from > y->from);
}

/*
 * Change the array statement `st' so it holds the elements of `want':
 * elements no longer wanted lose their own lines (or their slot in a
 * one-liner), new ones join after the last survivor in the style of the
 * element before them. Only an edit that keeps the survivors in their
 * old order and appends the rest is made this way; anything else, and
 * any text this lexer cannot follow, returns NULL so the caller falls
 * back to replacing the whole value. Returns the new text otherwise,
 * with `*changed' telling whether it differs.
 */
char *
array_edit(const char *text, size_t len, size_t open, size_t anchor,
    const ucl_object_t *want, char **wantr, size_t nwant,
    const char *indent, bool *changed)
{
	struct ucut *cuts = NULL;
	struct uelem *elems = NULL;
	struct uelem *after = NULL;
	struct ubuf b = { NULL, 0, 0 };
	const struct uelem *last;
	ucl_object_t *obj;
	const ucl_object_t *cur;
	ucl_object_iter_t it;
	char **oldr = NULL;
	char *model = NULL;
	char *out = NULL;
	char *prefix = NULL;
	char *tmp;
	char *vtext;
	char *er;
	size_t close;
	size_t ncuts = 0;
	size_t nkept = 0;
	size_t nnew;
	size_t at;
	size_t i;
	size_t k;
	size_t w;
	int n;
	int n2;
	bool anyterm = false;
	bool last_bare;
	bool oneline;
	bool quoted = false;
	bool choice = false;
	bool space_after = true;
	char term;

	*changed = false;
	if ((n = enum_array(text, len, open, &elems, &close)) < 0)
		return (NULL);
	if (n > 0 && (oldr = calloc((size_t)n, sizeof(*oldr))) == NULL)
		err(EXIT_FAILURE, NULL);
	for (i = 0; i < (size_t)n; i++) {
		if ((obj = elem_object(text + elems[i].start,
		    elems[i].val_end - elems[i].start)) == NULL)
			goto out;
		oldr[i] = ucl_render(obj);
		ucl_object_unref(obj);
		if (elems[i].term != 0) {
			anyterm = true;
			if (elems[i].end < len && text[elems[i].end] != ' ')
				space_after = false;
		}
		if (elems[i].quoted) {
			quoted = true;
			if (quoted_by_choice(text + elems[i].start,
			    elems[i].val_end - elems[i].start))
				choice = true;
		}
	}
	last_bare = (n > 1 && anyterm && elems[n - 1].term == 0);
	oneline = (memchr(text + open, '\n',
	    close - open) == NULL);

	/* Same count: the elements that differ change where they stand */
	if ((size_t)n == nwant) {
		if ((out = strdup(text)) == NULL)
			err(EXIT_FAILURE, NULL);
		for (i = (size_t)n; i > 0; i--) {
			if (strcmp(oldr[i - 1], wantr[i - 1]) == 0)
				continue;
			cur = ucl_array_find_index(want, (unsigned int)(i - 1));
			if (text[elems[i - 1].start] == '[' &&
			    ucl_object_type(cur) == UCL_ARRAY) {
				/* A nested array is edited in its own span */
				const ucl_object_t *sub;
				ucl_object_iter_t it2;
				char **subr;
				size_t nalloc;
				size_t nsub = 0;
				bool ch = false;

				if (ckd_add(&nalloc, ucl_array_size(cur), 1))
					errx(EXIT_FAILURE, "%s",
					    strerror(EOVERFLOW));
				if ((subr = calloc(nalloc,
				    sizeof(*subr))) == NULL ||
				    (it2 = ucl_object_iterate_new(cur)) == NULL)
					err(EXIT_FAILURE, NULL);
				while ((sub = ucl_object_iterate_safe(it2,
				    false)) != NULL)
					subr[nsub++] = ucl_render(sub);
				ucl_object_iterate_free(it2);
				tmp = array_edit(out, strlen(out),
				    elems[i - 1].start, elems[i - 1].start,
				    cur, subr, nsub, indent, &ch);
				for (w = 0; w < nsub; w++)
					free(subr[w]);
				free(subr);
				if (tmp != NULL) {
					free(out);
					out = tmp;
					if (ch)
						*changed = true;
					continue;
				}
			}
			/*
			 * Quotes the old content forced do not bind the
			 * new value unless a neighbour quotes by choice.
			 */
			er = ucl_render(cur);
			vtext = render_value(cur, er, text + elems[i - 1].start,
			    elems[i - 1].val_end - elems[i - 1].start,
			    elems[i - 1].quoted && choice);
			free(er);
			tmp = splice(out, strlen(out), elems[i - 1].start,
			    elems[i - 1].val_end, vtext);
			free(vtext);
			free(out);
			out = tmp;
			*changed = true;
		}
		goto out;
	}

	/* Survivors in their old order, then the newcomers */
	for (i = 0; i < (size_t)n; i++) {
		for (w = 0; w < nwant; w++)
			if (strcmp(oldr[i], wantr[w]) == 0)
				break;
		if (w == nwant)
			continue;
		if (nkept >= nwant || strcmp(oldr[i], wantr[nkept]) != 0)
			goto out;
		elems[i].keep = true;
		nkept++;
	}
	for (w = nkept; w < nwant; w++)
		for (i = 0; i < (size_t)n; i++)
			if (strcmp(oldr[i], wantr[w]) == 0)
				goto out;
	nnew = nwant - nkept;
	if (nnew == 0 && nkept == (size_t)n) {
		if ((out = strdup(text)) == NULL)
			err(EXIT_FAILURE, NULL);
		goto out;
	}

	/* The last element as it was models the newcomers' style */
	if (n > 0) {
		last = &elems[n - 1];
		if ((model = strndup(text + last->start,
		    last->val_end - last->start)) == NULL ||
		    (prefix = strndup(text + last->line_start,
		    last->start - last->line_start)) == NULL)
			err(EXIT_FAILURE, NULL);
	} else {
		k = line_begin(text, anchor);
		if (asprintf(&prefix, "%.*s%s", (int)(anchor - k),
		    text + k, indent) < 0)
			err(EXIT_FAILURE, NULL);
	}

	/* Cut what is not kept, from the end so earlier offsets hold */
	if ((cuts = calloc((size_t)n + 1, sizeof(*cuts))) == NULL)
		err(EXIT_FAILURE, NULL);
	last = NULL;
	for (i = 0; i < (size_t)n; i++) {
		if (elems[i].keep) {
			last = &elems[i];
			continue;
		}
		if (elems[i].alone) {
			cuts[ncuts].from = elems[i].line_start;
			cuts[ncuts].to = elems[i].line_end;
		} else {
			/*
			 * Sharing a line: take the element, the blanks
			 * after it, and -- for a bare last element -- the
			 * comma that preceded it.
			 */
			at = elems[i].start;
			k = elems[i].end;
			while (k < len && (text[k] == ' ' || text[k] == '\t'))
				k++;
			if (elems[i].term == 0) {
				while (at > elems[i].line_start &&
				    (text[at - 1] == ' ' || text[at - 1] == '\t'))
					at--;
				if (at > elems[i].line_start &&
				    text[at - 1] == ',') {
					at--;
					k = elems[i].end;
				} else
					at = elems[i].start;
			}
			cuts[ncuts].from = at;
			cuts[ncuts].to = k;
		}
		ncuts++;
	}
	/*
	 * In a style that terminates every element but the last, the new
	 * last survivor goes bare when nothing will follow it.
	 */
	if (last_bare && nnew == 0 && last != NULL && last->term != 0) {
		cuts[ncuts].from = last->end - 1;
		cuts[ncuts].to = last->end;
		ncuts++;
	}
	qsort(cuts, ncuts, sizeof(*cuts), ucut_cmp);
	for (i = 1; i < ncuts; i++) {
		/* Merge overlapping cuts (a shared comma, for instance) */
		if (cuts[i].from <= cuts[i - 1].to) {
			if (cuts[i].to > cuts[i - 1].to)
				cuts[i - 1].to = cuts[i].to;
			memmove(cuts + i, cuts + i + 1,
			    (ncuts - i - 1) * sizeof(*cuts));
			ncuts--;
			i--;
		}
	}
	if ((out = strdup(text)) == NULL)
		err(EXIT_FAILURE, NULL);
	for (i = ncuts; i > 0; i--) {
		tmp = splice(out, strlen(out), cuts[i - 1].from,
		    cuts[i - 1].to, "");
		free(out);
		out = tmp;
	}
	if (nnew == 0) {
		*changed = true;
		goto out;
	}

	/* Newcomers after the last survivor, or after `[' */
	if ((n2 = enum_array(out, strlen(out), open, &after,
	    &close)) < 0 || (size_t)n2 != nkept) {
		free(out);
		out = NULL;
		goto out;
	}
	last = (n2 > 0) ? &after[n2 - 1] : NULL;
	at = (last != NULL) ? last->end : open + 1;
	term = (last != NULL && last->term == 0 && (anyterm || oneline)) ?
	    ',' : '\0';
	if ((it = ucl_object_iterate_new(want)) == NULL)
		err(EXIT_FAILURE, NULL);
	w = 0;
	while ((cur = ucl_object_iterate_safe(it, false)) != NULL) {
		if (w++ < nkept)
			continue;
		er = ucl_render(cur);
		vtext = render_value(cur, er, model != NULL ? model : "",
		    model != NULL ? strlen(model) : 0, quoted);
		free(er);
		if (oneline) {
			if (space_after || last == NULL)
				ubuf_str(&b, " ");
			ubuf_str(&b, vtext);
			if (w < nwant)
				ubuf_str(&b, ",");
		} else {
			ubuf_str(&b, "\n");
			ubuf_str(&b, prefix);
			ubuf_str(&b, vtext);
			if (anyterm && (w < nwant || !last_bare))
				ubuf_str(&b, ",");
		}
		free(vtext);
	}
	ucl_object_iterate_free(it);
	if (oneline && last == NULL && at == close)
		ubuf_str(&b, " ");
	tmp = splice_after(out, strlen(out), at, term, b.p);
	free(out);
	out = tmp;
	*changed = true;
out:
	for (i = 0; i < (size_t)n; i++)
		free(oldr[i]);
	free(oldr);
	free(elems);
	free(after);
	free(cuts);
	free(model);
	free(prefix);
	free(b.p);
	return (out);
}

