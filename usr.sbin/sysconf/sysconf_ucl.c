/*
 * Copyright (c) 2013-2026 Devin Teske <dteske@FreeBSD.org>
 * Copyright (c) 2021-2026 Faraz Vahedi <kfv@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * The ucl target: UCL files read with libucl(3), the same private library
 * pkg(8) links. libbsdconf supplies only the target bookkeeping (keyword,
 * default paths, ordered sources); it never sees the file text. A bug in
 * the parser is therefore the bug pkg already has, and reaching it from
 * here is part of the point.
 *
 * Each backing file is parsed on its own, in sourcing order, and every
 * scalar is reported under its dotted path (`FreeBSD-ports.enabled'). The
 * last file to define a path is authoritative, as with every other target;
 * the files are deliberately not merged by one parser, which would hide
 * where a value came from and make -F a lie. Arrays are leaves and read
 * back in compact JSON; nested objects are walked, not reported.
 *
 * A write changes only the bytes it must. The statement is located in
 * the original text and its value replaced in the file's own style (see
 * sysconf_ucl_text.c); nothing else moves. libucl then re-reads the
 * result, and the write is refused unless the parsed tree differs from
 * the original in exactly the requested key. Macros (`.include' and
 * friends) are not expanded, so a parse never pulls in another file, and
 * no variable handler is installed, so `${ABI}' stays literal.
 *
 * Values given on the command line are parsed as UCL so that `yes', `10',
 * `[ a, b ]', and `{ k = v }' land with the types pkg would read; the
 * file receives the text as typed (booleans in the spelling the file
 * already uses). A value that does not parse cleanly to the very string
 * typed is a string, quoted in the file, so a URL or anything the grammar
 * finds special still round-trips. Reads report the library's rendering
 * (`yes' comes back `true').
 *
 * A key that itself contains `.' cannot be addressed; the separator is the
 * path. libucl keeps no line numbers, so trails show line 0.
 */

#include <ctype.h>
#include <ucl.h>

#include "sysconf_priv.h"
#include "sysconf_ucl.h"

enum bsdconf_format	ucl_format;
bool			ucl_target;

/*
 * Register the ucl target (and, derived from it, pkg) with libbsdconf so
 * the usual target resolution applies. Idempotent.
 */
int
ucl_register(void)
{
	static bool done;
	struct bsdconf_format_def def;

	if (done)
		return (0);
	if (bsdconf_format_derive(BSDCONF_FORMAT_GENERIC, &def) != 0)
		return (-1);
	def.keyword = "ucl";
	def.path = NULL;
	def.sources = NULL;
	if (bsdconf_format_register(&def, &ucl_format) != 0)
		return (-1);
	if (pkg_register() != 0)
		return (-1);
	done = true;
	return (0);
}

/*
 * Render a leaf for sysconf's string world: strings as themselves,
 * booleans and numbers as the library prints them, arrays as compact
 * JSON. Returns newly allocated storage.
 */
char *
ucl_render(const ucl_object_t *obj)
{
	char *out;
	unsigned char *emitted;

	switch (ucl_object_type(obj)) {
	case UCL_ARRAY:
	case UCL_OBJECT:
		emitted = ucl_object_emit(obj, UCL_EMIT_JSON_COMPACT);
		if (emitted == NULL)
			err(EXIT_FAILURE, NULL);
		return ((char *)emitted);
	default:
		if ((out = strdup(ucl_object_tostring_forced(obj))) == NULL)
			err(EXIT_FAILURE, NULL);
		return (out);
	}
}

/*
 * Hand every scalar under `obj' to `cb' with its dotted path and type.
 * A key repeated in one file is an implicit array to libucl, which
 * presents its first element; that is the value reported (and the
 * statement a write edits), as pkg(8) would see it.
 */
int
ucl_walk(const ucl_object_t *obj, const char *prefix,
    int (*cb)(const char *, const char *, ucl_type_t, void *), void *arg)
{
	const char *key;
	const ucl_object_t *cur;
	ucl_object_iter_t it;
	char *value;
	char qname[4096];
	int rv = 0;

	if ((it = ucl_object_iterate_new(obj)) == NULL)
		err(EXIT_FAILURE, NULL);
	while (rv == 0 && (cur = ucl_object_iterate_safe(it, true)) != NULL) {
		if ((key = ucl_object_key(cur)) == NULL)
			continue;
		if (prefix == NULL)
			rv = snprintf(qname, sizeof(qname), "%s", key);
		else
			rv = snprintf(qname, sizeof(qname), "%s.%s", prefix,
			    key);
		if (rv < 0 || rv >= (int)sizeof(qname)) {
			errno = ENAMETOOLONG;
			rv = -1;
			break;
		}
		rv = 0;
		if (ucl_object_type(cur) == UCL_OBJECT) {
			rv = ucl_walk(cur, qname, cb, arg);
			continue;
		}
		value = ucl_render(cur);
		rv = cb(qname, value, ucl_object_type(cur), arg);
		free(value);
	}
	ucl_object_iterate_free(it);
	return (rv);
}

static int
scan_record_cb(const char *name, const char *value, ucl_type_t type,
    void *arg __unused)
{

	/* A path into an array is an index chain, not a key */
	if (type == UCL_ARRAY)
		ucl_split_index(name);
	return (scan_record(0, name, value));
}

/*
 * scan_pass() body for one backing file already open on `fd'. Returns
 * zero, or -1 with errno set; a syntax error is fatal with the parser's
 * message, as a bsdconf parse error would be.
 */
int
ucl_scan_fd(int fd)
{
	struct ucl_parser *parser;
	ucl_object_t *top;
	const char *perr;
	char *ready;
	char *text;
	size_t len;
	size_t rlen;
	int rv;

	text = slurp_fd(fd, conf_files[conf_scanidx], &len);
	ready = ucl_text_ready(text, len, &rlen);
	free(text);
	if ((parser = ucl_parser_new(UCL_PARSE_FLAGS)) == NULL)
		err(EXIT_FAILURE, NULL);
	if (rlen > 0 && !ucl_parser_add_string(parser, ready, rlen)) {
		perr = ucl_parser_get_error(parser);
		errx(EXIT_FAILURE, "%s: %s", conf_files[conf_scanidx],
		    perr != NULL ? perr : "parse error");
	}
	free(ready);
	if (rlen == 0 || (top = ucl_parser_get_object(parser)) == NULL) {
		ucl_parser_free(parser);
		return (0);
	}
	rv = ucl_walk(top, NULL, scan_record_cb, NULL);
	ucl_object_unref(top);
	ucl_parser_free(parser);
	return (rv);
}

/*
 * True when `s', outside its quoted strings, holds a comment, a newline,
 * or a `.' where a statement could begin (the start, or after `{', `;'
 * or `,'). The parse here runs with macros off, which skips a `.include'
 * like a comment, but pkg(8) expands them; a value that could carry one
 * must not be written as typed.
 */
static bool
has_extra(const char *s)
{
	size_t i;
	size_t n;
	char prev = '{';

	n = strlen(s);
	for (i = 0; i < n; i++) {
		if (s[i] == '"' || s[i] == '\'') {
			i = skip_quoted(s, n, i) - 1;
			prev = '"';
		} else if (comment_at(s, n, i) || s[i] == '\n' ||
		    (s[i] == '.' && (prev == '{' || prev == ';' ||
		    prev == ',')))
			return (true);
		else if (s[i] != ' ' && s[i] != '\t' && s[i] != '\r')
			prev = s[i];
	}
	return (false);
}

/*
 * Parse `text' as a UCL value. A clean parse to one value whose string
 * form is the text itself (or any non-string type with nothing extra;
 * see has_extra()) is taken as typed; anything else (a `#' the grammar
 * read as a comment, quotes it stripped, a second statement after a
 * newline) is kept as a raw string so what was typed is what lands in
 * the file, and nothing more than one value ever does.
 */
ucl_object_t *
ucl_value(const char *text)
{
	struct ucl_parser *parser;
	ucl_object_t *top = NULL;
	ucl_object_t *val = NULL;
	const ucl_object_t *v;
	char *chunk;

	if (asprintf(&chunk, "v = %s\n", text) < 0)
		err(EXIT_FAILURE, NULL);
	if ((parser = ucl_parser_new(UCL_PARSE_FLAGS)) == NULL)
		err(EXIT_FAILURE, NULL);
	/* The grammar reads nothing as null; nothing typed is "" */
	if (*text != '\0' &&
	    ucl_parser_add_string(parser, chunk, strlen(chunk)) &&
	    (top = ucl_parser_get_object(parser)) != NULL &&
	    (v = ucl_object_lookup(top, "v")) != NULL &&
	    top->len == 1 && v->next == NULL) {
		if (ucl_object_type(v) != UCL_STRING ?
		    !has_extra(text) :
		    strcmp(ucl_object_tostring(v), text) == 0)
			val = ucl_object_copy(v);
	}
	if (top != NULL)
		ucl_object_unref(top);
	ucl_parser_free(parser);
	free(chunk);
	if (val == NULL &&
	    (val = ucl_object_fromstring_common(text, 0, UCL_STRING_RAW)) ==
	    NULL)
		err(EXIT_FAILURE, NULL);
	return (val);
}

