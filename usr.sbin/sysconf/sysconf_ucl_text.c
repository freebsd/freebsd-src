/*
 * Copyright (c) 2013-2026 Devin Teske <dteske@FreeBSD.org>
 * Copyright (c) 2021-2026 Faraz Vahedi <kfv@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * Surgical edits to UCL text for the ucl target.
 *
 * libucl(3) reads the file and decides what it means; it does not keep
 * where in the text a value came from, and its emitter lays a file out
 * its own way. Rewriting a whole file through the emitter would change
 * every line a user did not ask about, so a write instead locates the
 * one statement in the original bytes and changes only that span: the
 * separator, the terminator, the quoting, the indentation, and even the
 * spelling of a boolean are taken from the file, never from here. A
 * missing key is added beside its siblings in their style; a missing
 * object is appended in the style of the objects already there (or, for
 * a new file, of a style source such as the file being overridden). A
 * removal takes the statement's own lines and, when that empties the
 * enclosing object, the object too.
 *
 * The lexer here only finds spans. It is deliberately not a parser of
 * record: after every edit the caller re-reads the result with libucl
 * and refuses the write unless the parsed tree differs from the
 * original in exactly the requested key (see ucl_do_writes() in
 * sysconf_ucl_write.c). Any construct this lexer misjudges therefore fails
 * loudly and leaves the file alone rather than being silently mangled.
 */

#include <ctype.h>
#include <stdckdint.h>

#include <ucl.h>

#include "sysconf_priv.h"
#include "sysconf_ucl.h"

/*
 * Apply one edit to `text': set dotted `name' to `val' (as typed in
 * `raw'), or remove it when `val' is NULL. `style_src' supplies the
 * style for a file with nothing to mimic (NULL for none). Returns the
 * new text (caller frees); `*changed' tells whether anything differs.
 * A path through a scalar is fatal; a key not found for removal returns
 * the text unchanged.
 */
char *
ucl_text_edit(const char *text, size_t len, const char *name,
    const ucl_object_t *val, const char *raw, const char *style_src,
    size_t style_len, bool *changed)
{
	struct ustyle sty;
	struct uobj objs[32];
	struct ustmt stmts[32];
	struct ustmt match;
	struct uobj obj;
	struct ubuf b;
	char *comps[32];
	char *path;
	char *out;
	char *vtext;
	char *p;
	size_t at;
	size_t interior;
	size_t k;
	int depth;
	int found;
	int level;
	int ncomps = 0;
	bool oneline;

	*changed = false;
	if ((path = strdup(name)) == NULL)
		err(EXIT_FAILURE, NULL);
	for (p = path; ncomps < 32 && (comps[ncomps] = strsep(&p, ".")) !=
	    NULL; ncomps++)
		continue;
	if (ncomps == 0 || p != NULL)
		errx(EXIT_FAILURE, "%s: path too deep", name);

	/* Top level: the file, or one JSON-style outer object */
	interior = 0;
	objs[0].open = SIZE_MAX;
	k = skip_ws(text, len, 0, true, false);
	if (k < len && text[k] == '{') {
		objs[0].open = k;
		interior = k + 1;
	}

	/* Walk down the path, recording each container and statement */
	for (level = 0; level < ncomps; level++) {
		found = enum_object(text, len, interior, comps[level], &match,
		    &obj);
		if (found < 0)
			errx(EXIT_FAILURE, "%s: cannot locate `%s' in text",
			    name, comps[level]);
		obj.open = objs[level].open;
		objs[level] = obj;
		if (!found)
			break;
		stmts[level] = match;
		if (level + 1 == ncomps)
			break;
		if (!match.is_obj)
			errx(EXIT_FAILURE, "%s: `%s' is not an object", name,
			    comps[level]);
		objs[level + 1].open = match.obj_open;
		interior = match.obj_open + 1;
	}
	/* `level' is the deepest container entered; `found' tells leaf */

	if (val == NULL) {
		struct ustmt victim;
		size_t prev_term;

		if (!found) {
			free(path);
			return (strdup(text));
		}
		/* Remove the statement, and any object it alone filled */
		victim = stmts[level];
		for (depth = level; depth > 0; depth--) {
			if (objs[depth].nstmts != 1)
				break;
			victim = stmts[depth - 1];
		}
		/*
		 * In a style that terminates every member but the last
		 * (`a: 1,\n  b: 2\n}'), removing the bare last member
		 * must also take the terminator its predecessor now
		 * carries in vain.
		 */
		obj = objs[depth];
		if (obj.nstmts > 1 && victim.term == 0 &&
		    victim.start == obj.last.start && obj.prev.term != 0 &&
		    obj.prev.end > 0 && obj.prev.end <= victim.start)
			prev_term = obj.prev.end - 1;
		else
			prev_term = SIZE_MAX;
		if (victim.alone) {
			out = splice(text, len, victim.line_start,
			    victim.line_end, "");
			if (prev_term != SIZE_MAX) {
				char *out2;

				out2 = splice(out, strlen(out), prev_term,
				    prev_term + 1, "");
				free(out);
				out = out2;
			}
		} else {
			/*
			 * Sharing a line: take the statement, the blanks
			 * after it, and -- for a bare last member -- the
			 * separator that preceded it (`a: 1, b: 2' minus b
			 * is `a: 1').
			 */
			at = victim.start;
			k = victim.end;
			while (k < len && (text[k] == ' ' || text[k] == '\t'))
				k++;
			if (prev_term != SIZE_MAX)
				at = prev_term;
			else if (victim.term == 0) {
				while (at > victim.line_start &&
				    (text[at - 1] == ' ' || text[at - 1] == '\t'))
					at--;
				if (!(at > victim.line_start &&
				    (text[at - 1] == ',' || text[at - 1] == ';')))
					at = victim.start;
				else
					at--;
			}
			/* Took the separator before it: keep what follows */
			if (at < victim.start)
				k = victim.end;
			out = splice(text, len, at, k, "");
		}
		*changed = true;
		free(path);
		return (out);
	}

	if (found && match.is_arr && ucl_object_type(val) == UCL_ARRAY) {
		/* Element by element when the array only grew or shrank */
		const ucl_object_t *cur;
		ucl_object_iter_t it;
		char **wantr;
		size_t nalloc;
		size_t nwant = 0;
		size_t w;

		if (ckd_add(&nalloc, ucl_array_size(val), 1))
			errx(EXIT_FAILURE, "%s", strerror(EOVERFLOW));
		if ((wantr = calloc(nalloc, sizeof(*wantr))) == NULL)
			err(EXIT_FAILURE, NULL);
		if ((it = ucl_object_iterate_new(val)) == NULL)
			err(EXIT_FAILURE, NULL);
		while ((cur = ucl_object_iterate_safe(it, false)) != NULL)
			wantr[nwant++] = ucl_render(cur);
		ucl_object_iterate_free(it);
		detect_indent(text, len, sty.indent, sizeof(sty.indent));
		out = array_edit(text, len, match.val_start, match.start,
		    val, wantr, nwant, sty.indent, changed);
		for (w = 0; w < nwant; w++)
			free(wantr[w]);
		free(wantr);
		if (out != NULL) {
			free(path);
			return (out);
		}
		*changed = false;
	}

	if (found) {
		/* Replace the value text in place */
		vtext = render_value(val, raw, text + match.val_start,
		    match.val_end - match.val_start, match.quoted);
		if (match.val_end - match.val_start == strlen(vtext) &&
		    strncmp(text + match.val_start, vtext,
		    strlen(vtext)) == 0) {
			free(vtext);
			free(path);
			return (strdup(text));
		}
		out = splice(text, len, match.val_start, match.val_end, vtext);
		free(vtext);
		free(path);
		*changed = true;
		return (out);
	}

	/*
	 * Insert into container `level' the components from `level' on.
	 * The file's own style wins; one with nothing to say about
	 * terminators yet (new, or only single-member objects so far)
	 * borrows from the style source so that an override file keeps
	 * looking like the file it overrides.
	 */
	if (len > 0 && !blank_span(text, 0, len))
		detect_style(text, len, &sty);
	else
		detect_style("", 0, &sty);
	if (sty.term == 0 && style_src != NULL) {
		struct ustyle src;

		detect_style(style_src, style_len, &src);
		if (len == 0 || blank_span(text, 0, len))
			sty = src;
		else if (src.term != 0) {
			sty.term = src.term;
			sty.last_bare = src.last_bare;
		}
	}
	obj = objs[level];
	if (obj.has_scalar) {
		/* Mimic the scalar siblings */
		sty.sep = obj.scalar.sep;
		if (obj.scalar.term != 0 ||
		    obj.scalar.start != obj.last.start)
			sty.term = obj.scalar.term;
	}
	if (obj.nstmts > 0) {
		sty.quote_keys = (text[obj.first.start] == '"');
		if (obj.first.is_obj) {
			sty.obj_sep = obj.first.sep;
			if (!obj.has_scalar && obj.first.term != 0)
				sty.term = obj.first.term;
		}
	}
	/* A file that quotes its keys (JSON) gets quoted strings too */
	vtext = render_value(val, raw, "", 0, sty.quote_keys);
	memset(&b, 0, sizeof(b));
	if (obj.open != SIZE_MAX) {
		const char *prefix;
		size_t prefix_len;
		char sep1 = '\0';
		struct ustmt last;

		/* Inside `{ ... }': after the last member, or after `{' */
		oneline = (memchr(text + obj.open, '\n',
		    obj.close - obj.open) == NULL);
		if (obj.nstmts == 0) {
			/*
			 * An empty object: members indent one unit past
			 * the line that opens it (the file's outer `{}'
			 * has no statement opening it).
			 */
			at = obj.open + 1;
			if (level > 0) {
				prefix = text + stmts[level - 1].line_start;
				prefix_len = stmts[level - 1].start -
				    stmts[level - 1].line_start;
			} else {
				prefix = "";
				prefix_len = 0;
			}
			if (oneline) {
				ubuf_str(&b, " ");
				emit_new(&b, comps + level, ncomps - level,
				    vtext, &sty, "", 0, 0, true);
				if (blank_span(text, at, obj.close) &&
				    obj.close == at)
					ubuf_str(&b, " ");
			} else {
				ubuf_str(&b, "\n");
				emit_new(&b, comps + level, ncomps - level,
				    vtext, &sty, prefix, prefix_len, 1, false);
			}
		} else {
			/*
			 * After the last member. A style that terminates
			 * every member but the last (`a: 1,\n  b: 2\n}')
			 * gets the terminator added to the old last member
			 * and the new one left bare; a one-liner needs a
			 * comma between members regardless.
			 */
			last = obj.last;
			at = last.end;
			if (last.term == 0) {
				if (sty.term != 0 || oneline)
					sep1 = (sty.term != 0) ? sty.term : ',';
				sty.term = 0;
			} else
				sty.term = last.term;
			sty.last_bare = false;	/* decided above */
			prefix = text + last.line_start;
			prefix_len = last.start - last.line_start;
			if (oneline) {
				ubuf_str(&b, " ");
				emit_new(&b, comps + level, ncomps - level,
				    vtext, &sty, "", 0, 0, true);
			} else {
				ubuf_str(&b, "\n");
				emit_new(&b, comps + level, ncomps - level,
				    vtext, &sty, prefix, prefix_len, 0, false);
			}
		}
		out = splice_after(text, len, at, sep1, b.p);
	} else {
		/* Append at the end of the file */
		at = len;
		if (len > 0 && text[len - 1] != '\n')
			ubuf_str(&b, "\n");
		if (obj.nstmts > 0 && obj.blank_between &&
		    (obj.last.is_obj || ncomps - level > 1))
			ubuf_str(&b, "\n");
		emit_new(&b, comps + level, ncomps - level, vtext, &sty, "",
		    0, 0, false);
		ubuf_str(&b, "\n");
		out = splice(text, len, at, at, b.p);
	}
	free(b.p);
	free(vtext);
	free(path);
	*changed = true;
	return (out);
}

/*
 * Record every bare key under the container at `i' that libucl would
 * refuse (`*', `**', `[1m', ...), then the keys of nested objects.
 */
static void
collect_unsafe_keys(const char *s, size_t n, size_t i, size_t **at,
    size_t **kl, size_t *nk)
{
	struct ustmt st;
	size_t j;
	size_t klen;
	size_t *tmp;

	for (;;) {
		j = skip_ws(s, n, i, true, true);
		if (j >= n || s[j] == '}')
			return;
		if (parse_stmt(s, n, j, &st) != 0)
			return;
		if (s[st.start] != '"') {
			klen = 0;
			while (st.start + klen < n &&
			    key_char(s[st.start + klen]))
				klen++;
			if (klen > 0 &&
			    !key_bare_safe_len(s + st.start, klen)) {
				if ((tmp = reallocarray(*at, *nk + 1,
				    sizeof(**at))) == NULL)
					err(EXIT_FAILURE, NULL);
				*at = tmp;
				if ((tmp = reallocarray(*kl, *nk + 1,
				    sizeof(**kl))) == NULL)
					err(EXIT_FAILURE, NULL);
				*kl = tmp;
				(*at)[*nk] = st.start;
				(*kl)[*nk] = klen;
				(*nk)++;
			}
		}
		if (st.is_obj)
			collect_unsafe_keys(s, n, st.obj_open + 1, at, kl, nk);
		i = st.end;
	}
}

/*
 * A copy of `text' whose keys libucl will accept: a bare `*' (as
 * uclcmd(1) may emit) and any other key that cannot stand bare is
 * quoted. The file on disk is not changed; the caller parses the
 * result and frees it.
 */
char *
ucl_text_ready(const char *text, size_t len, size_t *outlen)
{
	size_t *at = NULL;
	size_t *kl = NULL;
	size_t nk = 0;
	size_t i;
	size_t k;
	char *out;
	char *q;
	char *tmp;
	char *key;

	k = skip_ws(text, len, 0, true, false);
	if (k < len && text[k] == '{')
		collect_unsafe_keys(text, len, k + 1, &at, &kl, &nk);
	else
		collect_unsafe_keys(text, len, 0, &at, &kl, &nk);
	if ((out = strndup(text, len)) == NULL)
		err(EXIT_FAILURE, NULL);
	for (i = nk; i > 0; i--) {
		if ((key = strndup(out + at[i - 1], kl[i - 1])) == NULL)
			err(EXIT_FAILURE, NULL);
		q = quote_value(key);
		free(key);
		tmp = splice(out, strlen(out), at[i - 1],
		    at[i - 1] + kl[i - 1], q);
		free(q);
		free(out);
		out = tmp;
	}
	free(at);
	free(kl);
	*outlen = strlen(out);
	return (out);
}
