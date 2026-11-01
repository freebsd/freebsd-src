/* Copyright (c) 2013, Vsevolod Stakhov
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *       * Redistributions of source code must retain the above copyright
 *         notice, this list of conditions and the following disclaimer.
 *       * Redistributions in binary form must reproduce the above copyright
 *         notice, this list of conditions and the following disclaimer in the
 *         documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED ''AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <math.h>
#include "ucl.h"
#include "ucl_internal.h"
#include "ucl_chartable.h"

/**
 * @file ucl_parser.c
 * The implementation of ucl parser
 */

struct ucl_parser_saved_state {
	unsigned int line;
	unsigned int column;
	size_t remain;
	const unsigned char *pos;
};

/**
 * Move up to len characters
 * @param parser
 * @param begin
 * @param len
 * @return new position in chunk
 */
#define ucl_chunk_skipc(chunk, p) \
	do {                          \
		if (p == chunk->end) {    \
			break;                \
		}                         \
		if (*(p) == '\n') {       \
			(chunk)->line++;      \
			(chunk)->column = 0;  \
		}                         \
		else                      \
			(chunk)->column++;    \
		(p++);                    \
		(chunk)->pos++;           \
		(chunk)->remain--;        \
	} while (0)

static inline void
ucl_set_err(struct ucl_parser *parser, int code, const char *str, UT_string **err)
{
	const char *fmt_string, *filename;
	struct ucl_chunk *chunk = parser->chunks;

	if (parser->cur_file) {
		filename = parser->cur_file;
	}
	else {
		filename = "<unknown>";
	}

	if (chunk->pos < chunk->end) {
		if (isgraph(*chunk->pos)) {
			fmt_string = "error while parsing %s: "
						 "line: %d, column: %d - '%s', character: '%c'";
		}
		else {
			fmt_string = "error while parsing %s: "
						 "line: %d, column: %d - '%s', character: '0x%02x'";
		}
		ucl_create_err(err, fmt_string,
					   filename, chunk->line, chunk->column,
					   str, *chunk->pos);
	}
	else {
		ucl_create_err(err, "error while parsing %s: at the end of chunk: %s",
					   filename, str);
	}

	parser->err_code = code;
	parser->state = UCL_STATE_ERROR;
}

/**
 * Record a generic failure, but only if nothing more specific was recorded
 * already. Used where a helper returning NULL means "syntax error" in the
 * common case yet may also have failed for a reason of its own, such as a
 * parser limit being hit.
 */
static void
ucl_set_err_fallback(struct ucl_parser *parser, int code, const char *str)
{
	if (parser->err_code == UCL_EOK) {
		ucl_set_err(parser, code, str, &parser->err);
	}
	else {
		parser->state = UCL_STATE_ERROR;
	}
}

static void
ucl_save_comment(struct ucl_parser *parser, const char *begin, size_t len)
{
	ucl_object_t *nobj;

	if (len > 0 && begin != NULL) {
		nobj = ucl_object_fromstring_common(begin, len, 0);

		if (parser->last_comment) {
			/* We need to append data to an existing object */
			DL_APPEND(parser->last_comment, nobj);
		}
		else {
			parser->last_comment = nobj;
		}
	}
}

static void
ucl_attach_comment(struct ucl_parser *parser, ucl_object_t *obj, bool before)
{
	if (parser->last_comment) {
		ucl_object_insert_key(parser->comments, parser->last_comment,
							  (const char *) &obj, sizeof(void *), true);

		if (before) {
			parser->last_comment->flags |= UCL_OBJECT_INHERITED;
		}

		parser->last_comment = NULL;
	}
}

/* Attach the comments read inside a container that holds nothing else */
static void
ucl_attach_inner_comment(struct ucl_parser *parser, ucl_object_t *obj)
{
	if (parser->last_comment) {
		parser->last_comment->flags |= UCL_OBJECT_INNER_COMMENT;
		ucl_attach_comment(parser, obj, false);
	}
}

/**
 * Skip all comments from the current pos resolving nested and multiline comments
 * @param parser
 * @return
 */
static bool
ucl_skip_comments(struct ucl_parser *parser)
{
	struct ucl_chunk *chunk = parser->chunks;
	const unsigned char *p, *beg = NULL;
	int comments_nested = 0;
	bool quoted = false;

	p = chunk->pos;

start:
	if (chunk->remain > 0 && *p == '#') {
		if (parser->state != UCL_STATE_SCOMMENT &&
			parser->state != UCL_STATE_MCOMMENT) {
			beg = p;

			while (p < chunk->end) {
				if (*p == '\n') {
					if (parser->flags & UCL_PARSER_SAVE_COMMENTS) {
						ucl_save_comment(parser, beg, p - beg);
						beg = NULL;
					}

					ucl_chunk_skipc(chunk, p);

					goto start;
				}
				ucl_chunk_skipc(chunk, p);
			}
		}
	}
	else if (chunk->remain >= 2 && *p == '/' && p[1] == '*') {
		beg = p;
		comments_nested++;
		ucl_chunk_skipc(chunk, p);
		ucl_chunk_skipc(chunk, p);
		while (p < chunk->end) {
			if (*p == '"' && *(p - 1) != '\\') {
				/* begin or end double-quoted string */
				quoted = !quoted;
				ucl_chunk_skipc(chunk, p);
			}
			else if (quoted) {
				/* quoted character */
				ucl_chunk_skipc(chunk, p);
			}
			else if (chunk->remain >= 2 && *p == '*' && p[1] == '/') {
				/* end of comment */
				ucl_chunk_skipc(chunk, p);
				ucl_chunk_skipc(chunk, p);
				comments_nested--;
				if (comments_nested == 0) {
					if (parser->flags & UCL_PARSER_SAVE_COMMENTS) {
						/* p is past the closing star and slash already */
						ucl_save_comment(parser, beg, p - beg);
						beg = NULL;
					}
					goto start;
				}
			}
			else if (chunk->remain >= 2 && *p == '/' && p[1] == '*') {
				/* start of nested comment */
				comments_nested++;
				ucl_chunk_skipc(chunk, p);
				ucl_chunk_skipc(chunk, p);
			}
			else {
				/* anything else */
				ucl_chunk_skipc(chunk, p);
			}
		}
		if (comments_nested != 0) {
			ucl_set_err(parser, UCL_ENESTED,
						"unfinished multiline comment", &parser->err);
			return false;
		}
	}

	if (beg && p > beg && (parser->flags & UCL_PARSER_SAVE_COMMENTS)) {
		ucl_save_comment(parser, beg, p - beg);
	}

	return true;
}

/**
 * Return multiplier for a character
 * @param c multiplier character
 * @param is_bytes if true use 1024 multiplier
 * @return multiplier
 */
static inline unsigned long
ucl_lex_num_multiplier(const unsigned char c, bool is_bytes)
{
	const struct {
		char c;
		long mult_normal;
		long mult_bytes;
	} multipliers[] = {
		{'m', 1000 * 1000, 1024 * 1024},
		{'k', 1000, 1024},
		{'g', 1000 * 1000 * 1000, 1024 * 1024 * 1024}};
	int i;

	for (i = 0; i < 3; i++) {
		if (tolower(c) == multipliers[i].c) {
			if (is_bytes) {
				return multipliers[i].mult_bytes;
			}
			return multipliers[i].mult_normal;
		}
	}

	return 1;
}


/**
 * Multiply a parsed integer by a suffix multiplier, failing on overflow
 * @param lv
 * @param mult positive multiplier
 * @return false if the result does not fit int64_t
 */
static inline bool
ucl_lex_num_mul(int64_t *lv, long mult)
{
	if (*lv > INT64_MAX / mult || *lv < INT64_MIN / mult) {
		return false;
	}

	*lv *= mult;

	return true;
}

/**
 * Convert a parsed double to an integer, failing if it does not fit
 * @param dv
 * @param lv
 * @return false for NaN and for values out of the int64_t range
 */
static inline bool
ucl_lex_double_to_int(double dv, int64_t *lv)
{
	/* -2^63 and 2^63 are exact doubles, INT64_MAX is not */
	if (!(dv >= -9223372036854775808.0 && dv < 9223372036854775808.0)) {
		return false;
	}

	*lv = (int64_t) dv;

	return true;
}

/**
 * Return multiplier for time scaling
 * @param c
 * @return
 */
static inline double
ucl_lex_time_multiplier(const unsigned char c)
{
	const struct {
		char c;
		double mult;
	} multipliers[] = {
		{'m', 60},
		{'h', 60 * 60},
		{'d', 60 * 60 * 24},
		{'w', 60 * 60 * 24 * 7},
		{'y', 60 * 60 * 24 * 365}};
	int i;

	for (i = 0; i < 5; i++) {
		if (tolower(c) == multipliers[i].c) {
			return multipliers[i].mult;
		}
	}

	return 1;
}

/**
 * Return true if a character is a end of an atom
 * @param c
 * @return
 */
static inline bool
ucl_lex_is_atom_end(const unsigned char c)
{
	return ucl_test_character(c, UCL_CHARACTER_VALUE_END);
}

static inline bool
ucl_lex_is_comment(const unsigned char c1, const unsigned char c2)
{
	if (c1 == '/') {
		if (c2 == '*') {
			return true;
		}
	}
	else if (c1 == '#') {
		return true;
	}
	return false;
}

/*
 * A number ends with the input, with an atom or where a comment starts,
 * possibly after spaces
 */
static inline bool
ucl_lex_is_number_end(const char *p, const char *end)
{
	while (p < end && ucl_test_character(*p, UCL_CHARACTER_WHITESPACE)) {
		p++;
	}

	return p >= end || ucl_lex_is_atom_end(*p) ||
		   (end - p >= 2 && ucl_lex_is_comment(p[0], p[1]));
}

/**
 * Check variable found
 * @param parser
 * @param ptr
 * @param remain
 * @param out_len
 * @param strict
 * @param found
 * @return
 */
static inline const char *
ucl_check_variable_safe(struct ucl_parser *parser, const char *ptr, size_t remain,
						size_t *out_len, bool strict, bool *found)
{
	struct ucl_variable *var;
	unsigned char *dst;
	size_t dstlen;
	bool need_free = false;

	LL_FOREACH(parser->variables, var)
	{
		if (strict) {
			if (remain == var->var_len) {
				if (memcmp(ptr, var->var, var->var_len) == 0) {
					*out_len += var->value_len;
					*found = true;
					return (ptr + var->var_len);
				}
			}
		}
		else {
			if (remain >= var->var_len) {
				if (memcmp(ptr, var->var, var->var_len) == 0) {
					*out_len += var->value_len;
					*found = true;
					return (ptr + var->var_len);
				}
			}
		}
	}

	/* XXX: can only handle ${VAR} */
	if (!(*found) && parser->var_handler != NULL && strict) {
		/* Call generic handler */
		if (parser->var_handler(ptr, remain, &dst, &dstlen, &need_free,
								parser->var_data)) {
			*found = true;
			*out_len += dstlen;

			if (need_free) {
				free(dst);
			}
			return (ptr + remain);
		}
	}

	return ptr;
}

/**
 * Check for a variable in a given string
 * @param parser
 * @param ptr
 * @param remain
 * @param out_len
 * @param vars_found
 * @return
 */
static const char *
ucl_check_variable(struct ucl_parser *parser, const char *ptr,
				   size_t remain, size_t *out_len, bool *vars_found)
{
	const char *p, *end, *ret = ptr;
	bool found = false;

	if (*ptr == '{') {
		/* We need to match the variable enclosed in braces */
		p = ptr + 1;
		end = ptr + remain;
		while (p < end) {
			if (*p == '}') {
				ret = ucl_check_variable_safe(parser, ptr + 1, p - ptr - 1,
											  out_len, true, &found);
				if (found) {
					/* {} must be excluded actually */
					ret++;
					if (!*vars_found) {
						*vars_found = true;
					}
				}
				else {
					*out_len += 2;
				}
				break;
			}
			p++;
		}
		if (p == end) {
			(*out_len)++;
		}
	}
	else if (*ptr != '$') {
		/* Not count escaped dollar sign */
		ret = ucl_check_variable_safe(parser, ptr, remain, out_len, false, &found);
		if (found && !*vars_found) {
			*vars_found = true;
		}
		if (!found) {
			(*out_len)++;
		}
	}
	else {
		ret++;
		(*out_len)++;
	}

	return ret;
}

/**
 * Expand a single variable
 * @param parser
 * @param ptr
 * @param in_len
 * @param dest
 * @param out_len
 * @return
 */
static const char *
ucl_expand_single_variable(struct ucl_parser *parser, const char *ptr,
						   size_t in_len, unsigned char **dest, size_t out_len)
{
	unsigned char *d = *dest, *dst;
	const char *p = ptr + 1, *ret, *var_end;
	struct ucl_variable *var;
	size_t dstlen;
	bool need_free = false;
	bool found = false;
	bool strict = false;

	ret = ptr + 1;
	/* For the $ sign */
	in_len--;

	if (*p == '$') {
		*d++ = *p++;
		*dest = d;
		return p;
	}
	else if (*p == '{') {
		p++;
		in_len--;
		strict = true;
		ret += 2;
	}

	LL_FOREACH(parser->variables, var)
	{
		if (out_len >= var->value_len && in_len >= (var->var_len + (strict ? 1 : 0))) {
			if (memcmp(p, var->var, var->var_len) == 0) {
				if (!strict || p[var->var_len] == '}') {
					memcpy(d, var->value, var->value_len);
					ret += var->var_len;
					d += var->value_len;
					found = true;
					break;
				}
			}
		}
	}

	if (!found) {
		if (strict && parser->var_handler != NULL &&
			(var_end = memchr(p, '}', in_len)) != NULL) {
			dstlen = out_len;

			/* Use the same variable name as the length-counting pass. */
			if (parser->var_handler(p, var_end - p, &dst, &dstlen, &need_free,
									parser->var_data)) {
				if (dstlen > out_len) {
					/* We do not have enough space! */
					if (need_free) {
						free(dst);
					}
				}
				else {
					memcpy(d, dst, dstlen);
					ret = var_end + 1;
					d += dstlen;
					found = true;

					if (need_free) {
						free(dst);
					}
				}
			}
		}

		/* Leave variable as is, in this case we use dest */
		if (!found) {
			if (strict && out_len >= 2) {
				/* Copy '${' */
				memcpy(d, ptr, 2);
				d += 2;
				ret--;
			}
			else {
				memcpy(d, ptr, 1);
				d++;
			}
		}
	}

	*dest = d;
	return ret;
}

/**
 * Expand variables in string
 * @param parser
 * @param dst
 * @param src
 * @param in_len
 * @return
 */
static ssize_t
ucl_expand_variable(struct ucl_parser *parser, unsigned char **dst,
					const char *src, size_t in_len)
{
	const char *p, *end = src + in_len;
	unsigned char *d, *d_end;
	size_t out_len = 0, alloc_len;
	bool vars_found = false;

	if (parser->flags & UCL_PARSER_DISABLE_MACRO) {
		*dst = NULL;
		return in_len;
	}

	p = src;
	while (p != end) {
		if (*p == '$' && p + 1 != end) {
			p = ucl_check_variable(parser, p + 1, end - p - 1, &out_len, &vars_found);
		}
		else {
			p++;
			out_len++;
		}
	}

	if (!vars_found) {
		/* Trivial case */
		*dst = NULL;
		return in_len;
	}

	*dst = NULL;
	if (ucl_size_add(out_len, 1, &alloc_len)) {
		*dst = UCL_ALLOC(alloc_len);
	}
	if (*dst == NULL) {
		return in_len;
	}

	d = *dst;
	d_end = d + out_len;
	p = src;
	while (p != end && d != d_end) {
		if (*p == '$' && p + 1 != end) {
			p = ucl_expand_single_variable(parser, p, end - p, &d, d_end - d);
		}
		else {
			*d++ = *p++;
		}
	}

	*d = '\0';

	return out_len;
}

bool ucl_parser_account_alloc(struct ucl_parser *parser, uint64_t bytes)
{
	parser->cur_alloc += bytes;

	if (parser->limits.max_alloc > 0 &&
		parser->cur_alloc > parser->limits.max_alloc) {
		ucl_set_err(parser, UCL_ELIMIT,
					"input requires too much memory to represent",
					&parser->err);

		return false;
	}

	return true;
}

bool ucl_parser_account_node(struct ucl_parser *parser)
{
	parser->cur_nodes++;

	if (parser->limits.max_nodes > 0 &&
		parser->cur_nodes > parser->limits.max_nodes) {
		ucl_set_err(parser, UCL_ELIMIT, "input has too many elements",
					&parser->err);

		return false;
	}

	return ucl_parser_account_alloc(parser,
									sizeof(ucl_object_t) + UCL_NODE_OVERHEAD);
}

/**
 * Store or copy pointer to the trash stack
 * @param parser parser object
 * @param src src string
 * @param dst destination buffer (trash stack pointer)
 * @param dst_const const destination pointer (e.g. value of object)
 * @param in_len input length
 * @param max_len maximum accepted length, 0 for unlimited
 * @param need_unescape need to unescape source (and copy it)
 * @param need_lowercase need to lowercase value (and copy)
 * @param need_expand need to expand variables (and copy as well)
 * @param unescape_squote unescape single quoted string
 * @return output length (excluding \0 symbol)
 */
static inline ssize_t
ucl_copy_or_store_ptr(struct ucl_parser *parser,
					  const unsigned char *src, unsigned char **dst,
					  const char **dst_const, size_t in_len, uint64_t max_len,
					  bool need_unescape, bool need_lowercase, bool need_expand,
					  bool unescape_squote)
{
	ssize_t ret = -1, tret;
	unsigned char *tmp;
	size_t alloc_len;

	/*
	 * Checked before allocating: `in_len` comes straight from the input and
	 * is an upper bound on the result, as neither unescaping nor lowercasing
	 * can make a string longer.
	 */
	if (max_len > 0 && (uint64_t) in_len > max_len) {
		ucl_set_err(parser, UCL_ELIMIT, "string is too long", &parser->err);

		return -1;
	}

	if (need_unescape || need_lowercase ||
		(need_expand && parser->variables != NULL) ||
		!(parser->flags & UCL_PARSER_ZEROCOPY)) {
		/* Copy string */
		*dst = NULL;
		if (ucl_size_add(in_len, 1, &alloc_len)) {
			*dst = UCL_ALLOC(alloc_len);
		}
		if (*dst == NULL) {
			ucl_set_err(parser, UCL_EINTERNAL, "cannot allocate memory for a string",
						&parser->err);
			return -1;
		}

		/*
		 * Charge what was actually allocated, not what the decoded string ends
		 * up being: unescaping shrinks the contents but keeps the buffer, so
		 * charging the decoded length would let escape-heavy input claim more
		 * memory than the budget accounts for.
		 */
		if (!ucl_parser_account_alloc(parser, (uint64_t) in_len + 1)) {
			return -1;
		}

		if (need_lowercase) {
			ret = ucl_strlcpy_tolower(*dst, src, alloc_len);
		}
		else {
			ret = ucl_strlcpy_unsafe(*dst, src, alloc_len);
		}

		if (need_unescape) {
			if (!unescape_squote) {
				ret = ucl_unescape_json_string(*dst, ret);
			}
			else {
				ret = ucl_unescape_squoted_string(*dst, ret);
			}
		}

		if (need_expand) {
			tmp = *dst;
			tret = ret;
			ret = ucl_expand_variable(parser, dst, tmp, ret);
			if (*dst == NULL) {
				/* Nothing to expand */
				*dst = tmp;
				ret = tret;
			}
			else {
				/* Free unexpanded value */
				UCL_FREE(in_len + 1, tmp);
			}

			/* Expansion is the one transform that can grow a string */
			if (max_len > 0 && ret > 0 && (uint64_t) ret > max_len) {
				ucl_set_err(parser, UCL_ELIMIT,
							"string is too long after variables expansion",
							&parser->err);

				return -1;
			}

			/* Charge whatever expansion added on top of the original buffer */
			if (ret > 0 && (uint64_t) ret > (uint64_t) in_len &&
				!ucl_parser_account_alloc(parser,
										  (uint64_t) ret - (uint64_t) in_len)) {
				return -1;
			}
		}
		*dst_const = *dst;
	}
	else {
		*dst_const = src;
		ret = in_len;
	}

	return ret;
}

/**
 * Create and append an object at the specified level
 * @param parser
 * @param is_array
 * @param level
 * @return
 */
static inline ucl_object_t *
ucl_parser_add_container(ucl_object_t *obj, struct ucl_parser *parser,
						 bool is_array, uint32_t level, bool has_obrace)
{
	struct ucl_stack *st;
	ucl_object_t *nobj;

	/*
	 * `level` below is the UCL dotted-key nesting level, which says nothing
	 * about how deep the containers actually are; the stack depth does, and
	 * that is what every recursive walk over the resulting tree (emitting,
	 * copying, and destruction itself) is proportional to. Checked before
	 * anything is allocated.
	 */
	if (parser->limits.max_depth > 0 &&
		(uint64_t) parser->cur_depth >= parser->limits.max_depth) {
		ucl_set_err(parser, UCL_ENESTED, "data is nested too deep",
					&parser->err);

		return NULL;
	}

	if (obj == NULL) {
		nobj = ucl_object_new_full(is_array ? UCL_ARRAY : UCL_OBJECT, parser->chunks->priority);
		if (nobj == NULL) {
			goto enomem0;
		}

		if (!ucl_parser_account_node(parser)) {
			ucl_object_unref(nobj);

			return NULL;
		}
	}
	else {
		if (obj->type == (is_array ? UCL_OBJECT : UCL_ARRAY)) {
			/* Bad combination for merge: array and object */
			ucl_set_err(parser, UCL_EMERGE,
						"cannot merge an object with an array",
						&parser->err);

			return NULL;
		}
		if ((obj->type != UCL_NULL || obj->value.ov != NULL) &&
			obj->type != (is_array ? UCL_ARRAY : UCL_OBJECT)) {
			/*
			 * Only an empty placeholder or a container of the same kind can
			 * become this container: retyping a scalar in place would reuse
			 * its value as the container's storage. A UCL_NULL object counts as
			 * a placeholder only if it carries no storage - parsing `null` over
			 * a scalar sets the type and leaves value.* in place.
			 */
			ucl_set_err(parser, UCL_EMERGE,
						"cannot turn a scalar value into a container",
						&parser->err);

			return NULL;
		}
		nobj = obj;
		nobj->type = is_array ? UCL_ARRAY : UCL_OBJECT;
	}

	if (!is_array) {
		if (nobj->value.ov == NULL) {
			nobj->value.ov = ucl_hash_create(parser->flags & UCL_PARSER_KEY_LOWERCASE);
			if (nobj->value.ov == NULL) {
				goto enomem1;
			}
		}
		parser->state = UCL_STATE_KEY;
	}
	else {
		parser->state = UCL_STATE_VALUE;
	}

	st = UCL_ALLOC(sizeof(struct ucl_stack));

	if (st == NULL) {
		goto enomem1;
	}

	st->obj = nobj;

	if (level >= UINT16_MAX) {
		ucl_set_err(parser, UCL_ENESTED,
					"objects are nesting too deep (over 65535 limit)",
					&parser->err);
		if (nobj != obj) {
			ucl_object_unref(obj);
		}

		UCL_FREE(sizeof(struct ucl_stack), st);

		return NULL;
	}


	st->e.params.level = level;
	st->e.params.line = parser->chunks->line;
	st->chunk = parser->chunks;

	if (has_obrace) {
		st->e.params.flags = UCL_STACK_HAS_OBRACE;
	}
	else {
		st->e.params.flags = 0;
	}

	LL_PREPEND(parser->stack, st);
	parser->cur_depth++;
	parser->cur_obj = nobj;

	return nobj;
enomem1:
	if (nobj != obj)
		ucl_object_unref(nobj);
enomem0:
	ucl_set_err(parser, UCL_EINTERNAL, "cannot allocate memory for an object",
				&parser->err);
	return NULL;
}

/**
 * Pop the head frame off the parser stack, keeping the depth counter in sync
 * @param parser parser object
 * @param st frame to pop, must be the current head of parser->stack
 */
void ucl_parser_pop_container(struct ucl_parser *parser, struct ucl_stack *st)
{
	parser->stack = st->next;

	if (parser->cur_depth > 0) {
		parser->cur_depth--;
	}

	UCL_FREE(sizeof(struct ucl_stack), st);
}

/**
 * Open the frame of the { that starts a chunk going on in the object on top
 * of the stack. The brace is the chunk's, not the object's: on the object's
 * frame, the } of the chunk would close the object's own brace as well. The
 * frame holds that object rather than a container of its own, so it takes no
 * depth.
 * @param parser parser object
 * @param chunk the chunk that starts with the brace
 * @return true if the frame has been pushed
 */
static bool
ucl_parser_add_chunk_brace(struct ucl_parser *parser, struct ucl_chunk *chunk)
{
	struct ucl_stack *st;

	st = UCL_ALLOC(sizeof(struct ucl_stack));
	if (st == NULL) {
		ucl_set_err(parser, UCL_EINTERNAL,
					"cannot allocate memory for an object", &parser->err);
		return false;
	}

	st->obj = parser->stack->obj;
	st->e.params.level = parser->stack->e.params.level;
	st->e.params.flags = UCL_STACK_HAS_OBRACE | UCL_STACK_AUTOMATIC;
	st->e.params.line = chunk->line;
	st->chunk = chunk;
	LL_PREPEND(parser->stack, st);

	return true;
}

int ucl_maybe_parse_number(ucl_object_t *obj,
						   const char *start, const char *end, const char **pos,
						   bool allow_double, bool number_bytes, bool allow_time)
{
	const char *p = start, *c = start;
	char *endptr;
	bool got_dot = false, got_exp = false, need_double = false,
		 is_time = false, valid_start = false, is_hex = false;
	int is_neg = 0;
	double dv = 0;
	int64_t lv = 0;

	if (*p == '-') {
		is_neg = 1;
		c++;
		p++;
	}
	while (p < end) {
		if (is_hex && isxdigit(*p)) {
			p++;
		}
		else if (isdigit(*p)) {
			valid_start = true;
			p++;
		}
		else if (!is_hex && (*p == 'x' || *p == 'X')) {
			is_hex = true;
			allow_double = false;
			c = p + 1;
			p++;
		}
		else if (allow_double) {
			if (p == c) {
				/* Empty digits sequence, not a number */
				*pos = start;
				return EINVAL;
			}
			else if (*p == '.') {
				if (got_dot) {
					/* Double dots, not a number */
					*pos = start;
					return EINVAL;
				}
				else {
					got_dot = true;
					need_double = true;
					p++;
				}
			}
			else if (*p == 'e' || *p == 'E') {
				if (got_exp) {
					/* Double exp, not a number */
					*pos = start;
					return EINVAL;
				}
				else {
					got_exp = true;
					need_double = true;
					p++;
					if (p >= end) {
						*pos = start;
						return EINVAL;
					}
					if (!isdigit(*p) && *p != '+' && *p != '-') {
						/* Wrong exponent sign */
						*pos = start;
						return EINVAL;
					}
					else {
						p++;
					}
				}
			}
			else {
				/* Got the end of the number, need to check */
				break;
			}
		}
		else if (!allow_double && *p == '.') {
			/* Unexpected dot */
			*pos = start;
			return EINVAL;
		}
		else {
			break;
		}
	}

	if (!valid_start || p == c) {
		*pos = start;
		return EINVAL;
	}

	char numbuf[128];

	if ((size_t) (p - c + 1) >= sizeof(numbuf)) {
		*pos = start;
		return EINVAL;
	}

	if (is_neg) {
		numbuf[0] = '-';
		ucl_strlcpy(&numbuf[1], c, p - c + 1);
	}
	else {
		ucl_strlcpy(numbuf, c, p - c + 1);
	}

	errno = 0;
	if (need_double) {
		dv = strtod(numbuf, &endptr);
	}
	else {
		if (is_hex) {
			lv = strtoimax(numbuf, &endptr, 16);
		}
		else {
			lv = strtoimax(numbuf, &endptr, 10);
		}
	}
	if (errno == ERANGE) {
		*pos = start;
		return ERANGE;
	}

	/* Now check endptr and move it from numbuf to the real ending */
	if (endptr != NULL) {
		long shift = endptr - numbuf - is_neg;
		endptr = (char *) c + shift;
	}
	if (endptr >= end) {
		p = end;
		goto set_obj;
	}
	if (endptr == NULL || ucl_lex_is_number_end(endptr, end) || *endptr == '\0') {
		p = endptr;
		goto set_obj;
	}

	if (endptr < end && endptr != start) {
		p = endptr;
		switch (*p) {
		case 'm':
		case 'M':
		case 'g':
		case 'G':
		case 'k':
		case 'K':
			if (end - p >= 2) {
				if (p[1] == 's' || p[1] == 'S') {
					/* Milliseconds */
					if (!need_double) {
						need_double = true;
						dv = lv;
					}
					is_time = true;
					if (p[0] == 'm' || p[0] == 'M') {
						dv /= 1000.;
					}
					else {
						dv *= ucl_lex_num_multiplier(*p, false);
					}
					p += 2;
					if (!ucl_lex_is_number_end(p, end)) {
						*pos = start;
						return EINVAL;
					}
					goto set_obj;
				}
				else if (number_bytes || (p[1] == 'b' || p[1] == 'B')) {
					/* Bytes */
					if (need_double) {
						need_double = false;
						if (!ucl_lex_double_to_int(dv, &lv)) {
							*pos = start;
							return ERANGE;
						}
					}
					if (!ucl_lex_num_mul(&lv, ucl_lex_num_multiplier(*p, true))) {
						*pos = start;
						return ERANGE;
					}
					p += 2;
					if (!ucl_lex_is_number_end(p, end)) {
						*pos = start;
						return EINVAL;
					}
					goto set_obj;
				}
				else if (ucl_lex_is_number_end(p + 1, end)) {
					if (need_double) {
						dv *= ucl_lex_num_multiplier(*p, false);
					}
					else if (!ucl_lex_num_mul(&lv, ucl_lex_num_multiplier(*p, number_bytes))) {
						*pos = start;
						return ERANGE;
					}
					p++;
					goto set_obj;
				}
				else if (allow_time && end - p >= 3) {
					if (tolower(p[0]) == 'm' &&
						tolower(p[1]) == 'i' &&
						tolower(p[2]) == 'n') {
						/* Minutes */
						if (!need_double) {
							need_double = true;
							dv = lv;
						}
						is_time = true;
						dv *= 60.;
						p += 3;
						if (!ucl_lex_is_number_end(p, end)) {
							*pos = start;
							return EINVAL;
						}
						goto set_obj;
					}
				}
			}
			else {
				if (need_double) {
					dv *= ucl_lex_num_multiplier(*p, false);
				}
				else if (!ucl_lex_num_mul(&lv, ucl_lex_num_multiplier(*p, number_bytes))) {
					*pos = start;
					return ERANGE;
				}
				p++;
				if (!ucl_lex_is_number_end(p, end)) {
					*pos = start;
					return EINVAL;
				}
				goto set_obj;
			}
			break;
		case 'S':
		case 's':
			if (allow_time &&
				ucl_lex_is_number_end(p + 1, end)) {
				if (!need_double) {
					need_double = true;
					dv = lv;
				}
				p++;
				is_time = true;
				goto set_obj;
			}
			break;
		case 'h':
		case 'H':
		case 'd':
		case 'D':
		case 'w':
		case 'W':
		case 'Y':
		case 'y':
			if (allow_time &&
				ucl_lex_is_number_end(p + 1, end)) {
				if (!need_double) {
					need_double = true;
					dv = lv;
				}
				is_time = true;
				dv *= ucl_lex_time_multiplier(*p);
				p++;
				goto set_obj;
			}
			break;
		case '\t':
		case ' ':
			while (p < end && ucl_test_character(*p, UCL_CHARACTER_WHITESPACE)) {
				p++;
			}
			if (ucl_lex_is_number_end(p, end))
				goto set_obj;
			break;
		}
	}
	else if (endptr == end) {
		/* Just a number at the end of chunk */
		p = end;
		goto set_obj;
	}

	*pos = c;
	return EINVAL;

set_obj:
	if (obj != NULL) {
		if (allow_double && (need_double || is_time)) {
			if (!is_time) {
				obj->type = UCL_FLOAT;
			}
			else {
				obj->type = UCL_TIME;
			}
			obj->value.dv = dv;
		}
		else {
			obj->type = UCL_INT;
			obj->value.iv = lv;
		}
	}
	*pos = p;
	return 0;
}

/**
 * Parse possible number
 * @param parser
 * @param chunk
 * @param obj
 * @return true if a number has been parsed
 */
static bool
ucl_lex_number(struct ucl_parser *parser,
			   struct ucl_chunk *chunk, ucl_object_t *obj)
{
	const unsigned char *pos;
	int ret;

	ret = ucl_maybe_parse_number(obj, chunk->pos, chunk->end, (const char **) &pos,
								 true, false, ((parser->flags & UCL_PARSER_NO_TIME) == 0));

	if (ret == 0) {
		chunk->remain -= pos - chunk->pos;
		chunk->column += pos - chunk->pos;
		chunk->pos = pos;
		return true;
	}
	else if (ret == ERANGE) {
		ucl_set_err(parser, UCL_ESYNTAX, "numeric value out of range",
					&parser->err);
	}

	return false;
}

/**
 * Parse quoted string with possible escapes
 * @param parser
 * @param chunk
 * @param need_unescape
 * @param ucl_escape
 * @param var_expand
 * @return true if a string has been parsed
 */
static bool
ucl_lex_json_string(struct ucl_parser *parser,
					struct ucl_chunk *chunk,
					bool *need_unescape,
					bool *ucl_escape,
					bool *var_expand)
{
	const unsigned char *p = chunk->pos;
	unsigned char c;
	int i;

	while (p < chunk->end) {
		c = *p;
		if (c < 0x1F) {
			/* Unmasked control character */
			if (c == '\n') {
				ucl_set_err(parser, UCL_ESYNTAX, "unexpected newline",
							&parser->err);
			}
			else {
				ucl_set_err(parser, UCL_ESYNTAX, "unexpected control character",
							&parser->err);
			}
			return false;
		}
		else if (c == '\\') {
			ucl_chunk_skipc(chunk, p);
			if (p >= chunk->end) {
				ucl_set_err(parser, UCL_ESYNTAX, "unfinished escape character",
							&parser->err);
				return false;
			}
			c = *p;
			if (ucl_test_character(c, UCL_CHARACTER_ESCAPE)) {
				if (c == 'u') {
					ucl_chunk_skipc(chunk, p);
					for (i = 0; i < 4 && p < chunk->end; i++) {
						if (!isxdigit(*p)) {
							ucl_set_err(parser, UCL_ESYNTAX, "invalid utf escape",
										&parser->err);
							return false;
						}
						ucl_chunk_skipc(chunk, p);
					}
					if (p >= chunk->end) {
						ucl_set_err(parser, UCL_ESYNTAX,
									"unfinished escape character",
									&parser->err);
						return false;
					}
				}
				else {
					ucl_chunk_skipc(chunk, p);
				}
			}
			*need_unescape = true;
			*ucl_escape = true;
			continue;
		}
		else if (c == '"') {
			ucl_chunk_skipc(chunk, p);
			return true;
		}
		else if (ucl_test_character(c, UCL_CHARACTER_UCL_UNSAFE)) {
			*ucl_escape = true;
		}
		else if (c == '$') {
			*var_expand = true;
		}
		ucl_chunk_skipc(chunk, p);
	}

	ucl_set_err(parser, UCL_ESYNTAX,
				"no quote at the end of json string",
				&parser->err);
	return false;
}

/**
 * Process single quoted string
 * @param parser
 * @param chunk
 * @param need_unescape
 * @return
 */
static bool
ucl_lex_squoted_string(struct ucl_parser *parser,
					   struct ucl_chunk *chunk, bool *need_unescape)
{
	const unsigned char *p = chunk->pos;
	unsigned char c;

	while (p < chunk->end) {
		c = *p;
		if (c == '\\') {
			ucl_chunk_skipc(chunk, p);

			if (p >= chunk->end) {
				ucl_set_err(parser, UCL_ESYNTAX,
							"unfinished escape character",
							&parser->err);
				return false;
			}
			else {
				ucl_chunk_skipc(chunk, p);
			}

			*need_unescape = true;
			continue;
		}
		else if (c == '\'') {
			ucl_chunk_skipc(chunk, p);
			return true;
		}

		ucl_chunk_skipc(chunk, p);
	}

	ucl_set_err(parser, UCL_ESYNTAX,
				"no quote at the end of single quoted string",
				&parser->err);
	return false;
}

static bool
ucl_parser_append_elt(struct ucl_parser *parser, ucl_hash_t *cont,
					  ucl_object_t *top,
					  ucl_object_t *elt)
{
	ucl_object_t *nobj;

	if ((parser->flags & UCL_PARSER_NO_IMPLICIT_ARRAYS) == 0) {
		/* Implicit array */
		top->flags |= UCL_OBJECT_MULTIVALUE;
		DL_APPEND(top, elt);
		parser->stack->obj->len++;
	}
	else {
		if ((top->flags & UCL_OBJECT_MULTIVALUE) != 0) {
			/* Just add to the explicit array */
			if (!ucl_array_append(top, elt)) {
				ucl_set_err(parser, UCL_EINTERNAL,
							"cannot allocate memory for an array",
							&parser->err);

				return false;
			}
		}
		else {
			/*
			 * Converting to an explicit array costs one more element, so it has
			 * to be charged as well: otherwise a run of duplicate keys grows the
			 * tree without ever touching the budget.
			 */
			if (!ucl_parser_account_node(parser)) {
				return false;
			}

			/*
			 * The array stands for its values in priority checks, as the head
			 * of an implicit array does. With priority 0, a later value of the
			 * key replaced them all whenever their priority was above 0.
			 */
			nobj = ucl_object_new_full(UCL_ARRAY, ucl_object_get_priority(top));

			if (nobj == NULL) {
				ucl_set_err(parser, UCL_EINTERNAL,
							"cannot allocate memory for an object",
							&parser->err);

				return false;
			}

			nobj->key = top->key;
			nobj->keylen = top->keylen;
			nobj->flags |= UCL_OBJECT_MULTIVALUE;

			if (!ucl_array_append(nobj, top) ||
				!ucl_array_append(nobj, elt)) {
				/*
				 * `top` is still owned by the container, so the half-built
				 * array must be dropped without releasing what it holds.
				 */
				ucl_array_detach_elements(nobj);
				ucl_object_unref(nobj);
				ucl_set_err(parser, UCL_EINTERNAL,
							"cannot allocate memory for an array",
							&parser->err);

				return false;
			}

			ucl_hash_replace(cont, top, nobj);
		}
	}

	return true;
}

/*
 * Replace the old value of a key with a new one. With comments saved, the
 * comment of the key goes to the new value, and those of everything released
 * with the old one are dropped rather than left to objects reusing addresses.
 */
static void
ucl_parser_replace_elt(struct ucl_parser *parser, ucl_hash_t *cont,
					   ucl_object_t *old, ucl_object_t *nobj)
{
	ucl_hash_replace(cont, old, nobj);

	if (parser->comments != NULL) {
		ucl_comments_move(parser->comments, old, nobj);
		ucl_comments_forget(parser->comments, old);
	}

	ucl_object_unref(old);
}

/*
 * What the value of a new element of an object is known to be, as only a
 * container is merged into an old one. The text parser adds a key before it
 * reads the value, which may even start in a later chunk.
 */
enum ucl_value_kind {
	UCL_VALUE_SCALAR = 0,
	UCL_VALUE_CONTAINER,
	UCL_VALUE_UNKNOWN
};

static bool
ucl_parser_add_object_element(struct ucl_parser *parser, ucl_object_t *nobj,
							  enum ucl_duplicate_strategy strategy,
							  enum ucl_value_kind kind)
{
	ucl_hash_t *container;
	ucl_object_t *tobj = NULL, *cur, *target;
	char errmsg[256];

	if (parser->pending_merge != NULL) {
		/*
		 * Something else has been added before the value of the pending key,
		 * as ucl_parser_insert_chunk can do: leave that key merged
		 */
		ucl_object_unref(parser->pending_merge);
		parser->pending_merge = NULL;
	}

	container = parser->stack->obj->value.ov;

	DL_FOREACH(parser->stack->obj, cur)
	{
		if (cur->type == UCL_OBJECT) {
			tobj = __DECONST(ucl_object_t *, ucl_hash_search_obj(cur->value.ov, nobj));

			if (tobj != NULL) {
				/*
				 * Check if we have found an object in the same container.
				 * If not, we should probably ignore it as we cannot replace it
				 * effectively and we definitely should not unref it.
				 */
				if (cur->value.ov != container) {
					tobj = NULL;
					continue;
				}
				break;
			}
		}
	}


	if (tobj == NULL) {
		container = ucl_hash_insert_object(container, nobj,
										   parser->flags & UCL_PARSER_KEY_LOWERCASE);
		if (container == NULL) {
			ucl_set_err(parser, UCL_EINTERNAL, "cannot allocate memory for an object",
						&parser->err);
			return false;
		}
		nobj->prev = nobj;
		nobj->next = NULL;
		parser->stack->obj->len++;
	}
	else {
		unsigned priold = ucl_object_get_priority(tobj),
				 prinew = ucl_object_get_priority(nobj);
		switch (strategy) {

		case UCL_DUPLICATE_APPEND:
			/*
			 * The logic here is the following:
			 *
			 * - if we have two objects with the same priority, then we form an
			 * implicit or explicit array
			 * - if a new object has bigger priority, then we overwrite an old one
			 * - if a new object has lower priority, then we ignore it
			 */
			/* Special case for inherited objects */
			if (tobj->flags & UCL_OBJECT_INHERITED) {
				prinew = priold + 1;
			}

			if (priold == prinew) {
				if (!ucl_parser_append_elt(parser, container, tobj, nobj)) {
					return false;
				}
			}
			else if (priold > prinew) {
				/*
				 * We add this new object to a list of trash objects just to ensure
				 * that it won't come to any real object
				 * XXX: rather inefficient approach
				 */
				DL_APPEND(parser->trash_objs, nobj);
			}
			else {
				ucl_parser_replace_elt(parser, container, tobj, nobj);
			}

			break;

		case UCL_DUPLICATE_REWRITE:
			/* We just rewrite old values regardless of priority */
			ucl_parser_replace_elt(parser, container, tobj, nobj);

			break;

		case UCL_DUPLICATE_ERROR:
			snprintf(errmsg, sizeof(errmsg),
					 "duplicate element for key '%.*s' found",
					 (int) (nobj->keylen < sizeof(errmsg) ? nobj->keylen : sizeof(errmsg)),
					 nobj->key);
			ucl_set_err(parser, UCL_EMERGE, errmsg, &parser->err);
			/* nobj is in no container, and the caller releases it */
			return false;

		case UCL_DUPLICATE_MERGE:
			/*
			 * Here we do have some old object so we just push it on top of objects stack
			 * Check priority and then perform the merge on the remaining objects
			 *
			 * Only a container can be merged into a container: a scalar would
			 * be parsed over the old one, retyping it in place and leaking its
			 * elements, so it is subject to priorities like any other scalar.
			 * A container of the other kind still gets the old one, so that
			 * opening it reports the mismatch.
			 *
			 * Without implicit arrays, the values of a repeated key are kept
			 * in an explicit array instead: merge into the first of them, as
			 * an implicit array is merged into its head.
			 */
			target = tobj;

			if ((parser->flags & UCL_PARSER_NO_IMPLICIT_ARRAYS) &&
				tobj->type == UCL_ARRAY &&
				(tobj->flags & UCL_OBJECT_MULTIVALUE)) {
				target = __DECONST(ucl_object_t *, ucl_array_head(tobj));
			}

			if (target != NULL &&
				(target->type == UCL_OBJECT || target->type == UCL_ARRAY) &&
				kind != UCL_VALUE_SCALAR) {
				if (kind == UCL_VALUE_UNKNOWN) {
					/*
					 * Take the old container for the value until
					 * ucl_parse_value sees it start, keeping the key to add
					 * it as a scalar if it is one. Its comment goes to the
					 * object it ends up in.
					 */
					parser->pending_merge = nobj;
					parser->cur_obj = target;

					return true;
				}

				ucl_object_unref(nobj);
				nobj = target;
			}
			else if (priold == prinew) {
				if (!ucl_parser_append_elt(parser, container, tobj, nobj)) {
					return false;
				}
			}
			else if (priold > prinew) {
				/*
				 * We add this new object to a list of trash objects just to ensure
				 * that it won't come to any real object
				 * XXX: rather inefficient approach
				 */
				DL_APPEND(parser->trash_objs, nobj);
			}
			else {
				ucl_parser_replace_elt(parser, container, tobj, nobj);
			}
			break;
		}
	}

	parser->stack->obj->value.ov = container;
	parser->cur_obj = nobj;
	ucl_attach_comment(parser, nobj, false);

	return true;
}

bool ucl_parser_process_object_element(struct ucl_parser *parser, ucl_object_t *nobj,
									   bool is_container)
{
	return ucl_parser_add_object_element(parser, nobj, parser->chunks->strategy,
										 is_container ? UCL_VALUE_CONTAINER : UCL_VALUE_SCALAR);
}

/**
 * Finish adding the key whose merge has been left pending, now that its value
 * starts
 * @param parser
 * @param is_container true if the value is an object or an array
 * @return true on success
 */
static bool
ucl_parser_settle_merge(struct ucl_parser *parser, bool is_container)
{
	ucl_object_t *nobj = parser->pending_merge;

	parser->pending_merge = NULL;

	if (is_container || parser->stack == NULL ||
		parser->stack->obj->type != UCL_OBJECT) {
		/*
		 * Keep the merge, as well when data inserted after the key has closed
		 * the object that holds it
		 */
		ucl_object_unref(nobj);
		ucl_attach_comment(parser, parser->cur_obj, false);

		return true;
	}

	/* Pending only with the merge strategy, whatever this chunk uses */
	if (!ucl_parser_add_object_element(parser, nobj, UCL_DUPLICATE_MERGE,
									   UCL_VALUE_SCALAR)) {
		/* Not taken on failure */
		ucl_object_unref(nobj);

		return false;
	}

	return true;
}

static bool ucl_skip_macro_as_comment(struct ucl_parser *parser,
									  struct ucl_chunk *chunk);

/**
 * Parse a key in an object
 * @param parser
 * @param chunk
 * @param next_key
 * @param end_of_object
 * @return true if a key has been parsed
 */
static bool
ucl_parse_key(struct ucl_parser *parser, struct ucl_chunk *chunk,
			  bool *next_key, bool *end_of_object, bool *got_content)
{
	const unsigned char *p, *c = NULL, *end, *t;
	const char *key = NULL;
	bool got_quote = false, got_eq = false, got_semicolon = false,
		 need_unescape = false, ucl_escape = false, var_expand = false,
		 got_sep = false;
	ucl_object_t *nobj;
	ssize_t keylen;

	p = chunk->pos;

	if (p >= chunk->end) {
		ucl_set_err(parser, UCL_ESYNTAX,
					"unexpected end of input while parsing key",
					&parser->err);
		*end_of_object = true;
		return true;
	}

	if (*p == '.' && !(parser->flags & UCL_PARSER_DISABLE_MACRO)) {
		ucl_chunk_skipc(chunk, p);
		parser->prev_state = parser->state;
		parser->state = UCL_STATE_MACRO_NAME;
		*end_of_object = false;
		return true;
	}
	while (p < chunk->end) {
		/*
		 * A key must start with alpha, number, '/' or '_' and end with space character
		 */
		if (c == NULL) {
			if (chunk->remain >= 2 && ucl_lex_is_comment(p[0], p[1])) {
				if (!ucl_skip_comments(parser)) {
					return false;
				}
				p = chunk->pos;
			}
			else if (ucl_test_character(*p, UCL_CHARACTER_WHITESPACE_UNSAFE)) {
				ucl_chunk_skipc(chunk, p);
			}
			else if (ucl_test_character(*p, UCL_CHARACTER_KEY_START)) {
				/* The first symbol */
				c = p;
				ucl_chunk_skipc(chunk, p);
				*got_content = true;
			}
			else if (*p == '"') {
				/* JSON style key */
				c = p + 1;
				got_quote = true;
				*got_content = true;
				ucl_chunk_skipc(chunk, p);
			}
			else if (*p == '}') {
				/* We have actually end of an object */
				*end_of_object = true;
				return true;
			}
			else if (*p == '.' && (parser->flags & UCL_PARSER_DISABLE_MACRO)) {
				/* Macros are comments then, so look for a key after it */
				if (!ucl_skip_macro_as_comment(parser, chunk)) {
					return false;
				}
				p = chunk->pos;
			}
			else if (*p == '.') {
				ucl_chunk_skipc(chunk, p);
				parser->prev_state = parser->state;
				parser->state = UCL_STATE_MACRO_NAME;
				return true;
			}
			else {
				/* Invalid identifier */
				ucl_set_err(parser, UCL_ESYNTAX, "key must begin with a letter",
							&parser->err);
				return false;
			}
		}
		else {
			/* Parse the body of a key */
			if (!got_quote) {
				if (ucl_test_character(*p, UCL_CHARACTER_KEY)) {
					*got_content = true;
					ucl_chunk_skipc(chunk, p);
				}
				else if (ucl_test_character(*p, UCL_CHARACTER_KEY_SEP)) {
					end = p;
					break;
				}
				else {
					ucl_set_err(parser, UCL_ESYNTAX, "invalid character in a key",
								&parser->err);
					return false;
				}
			}
			else {
				/* We need to parse json like quoted string */
				if (!ucl_lex_json_string(parser, chunk, &need_unescape, &ucl_escape, &var_expand)) {
					return false;
				}
				/* Always escape keys obtained via json */
				end = chunk->pos - 1;
				p = chunk->pos;
				break;
			}
		}
	}

	if (p >= chunk->end && *got_content) {
		ucl_set_err(parser, UCL_ESYNTAX, "unfinished key", &parser->err);
		return false;
	}
	else if (!*got_content) {
		return true;
	}
	*end_of_object = false;
	/* We are now at the end of the key, need to parse the rest */
	while (p < chunk->end) {
		if (ucl_test_character(*p, UCL_CHARACTER_WHITESPACE)) {
			ucl_chunk_skipc(chunk, p);
		}
		else if (*p == '=') {
			if (!got_eq && !got_semicolon) {
				ucl_chunk_skipc(chunk, p);
				got_eq = true;
			}
			else {
				ucl_set_err(parser, UCL_ESYNTAX, "unexpected '=' character",
							&parser->err);
				return false;
			}
		}
		else if (*p == ':') {
			if (!got_eq && !got_semicolon) {
				ucl_chunk_skipc(chunk, p);
				got_semicolon = true;
			}
			else {
				ucl_set_err(parser, UCL_ESYNTAX, "unexpected ':' character",
							&parser->err);
				return false;
			}
		}
		else if (chunk->remain >= 2 && ucl_lex_is_comment(p[0], p[1])) {
			/* Check for comment */
			if (!ucl_skip_comments(parser)) {
				return false;
			}
			p = chunk->pos;
		}
		else {
			/* Start value */
			break;
		}
	}

	if (p >= chunk->end && got_content) {
		ucl_set_err(parser, UCL_ESYNTAX, "unfinished key", &parser->err);
		return false;
	}

	got_sep = got_semicolon || got_eq;

	if (!got_sep) {
		/*
		 * Maybe we have more keys nested, so search for termination character.
		 * Possible choices:
		 * 1) key1 key2 ... keyN [:=] value <- we treat that as error
		 * 2) key1 ... keyN {} or [] <- we treat that as nested objects
		 * 3) key1 value[;,\n] <- we treat that as linear object
		 */
		t = p;
		*next_key = false;
		while (ucl_test_character(*t, UCL_CHARACTER_WHITESPACE)) {
			t++;
		}
		/* Check first non-space character after a key */
		if (*t != '{' && *t != '[') {
			while (t < chunk->end) {
				if (*t == ',' || *t == ';' || *t == '\n' || *t == '\r') {
					break;
				}
				else if (*t == '{' || *t == '[') {
					*next_key = true;
					break;
				}
				t++;
			}
		}
	}

	/* Create a new object */
	nobj = ucl_object_new_full(UCL_NULL, parser->chunks->priority);
	if (nobj == NULL) {
		ucl_set_err(parser, UCL_EINTERNAL, "cannot allocate memory for an object",
					&parser->err);
		return false;
	}

	if (!ucl_parser_account_node(parser)) {
		ucl_object_unref(nobj);
		return false;
	}

	keylen = ucl_copy_or_store_ptr(parser, c, &nobj->trash_stack[UCL_TRASH_KEY],
								   &key, end - c, parser->limits.max_key_length,
								   need_unescape, parser->flags & UCL_PARSER_KEY_LOWERCASE,
								   false, false);
	if (keylen == -1) {
		ucl_object_unref(nobj);
		return false;
	}
	else if (keylen == 0) {
		ucl_set_err(parser, UCL_ESYNTAX, "empty keys are not allowed", &parser->err);
		ucl_object_unref(nobj);
		return false;
	}

	nobj->key = key;
	nobj->keylen = keylen;

	/*
	 * Must be set before processing: on a merge into an older object of the
	 * same key, ucl_parser_add_object_element releases nobj, or keeps it
	 * aside until the value shows whether to merge
	 */
	if (ucl_escape) {
		nobj->flags |= UCL_OBJECT_NEED_KEY_ESCAPE;
	}

	/*
	 * The caller nests the following keys into the value whenever next_key is
	 * set, even by an earlier key of the chain. Otherwise the value is not
	 * read yet, and it is up to ucl_parse_value whether it is a container.
	 */
	if (!ucl_parser_add_object_element(parser, nobj, parser->chunks->strategy,
									   *next_key ? UCL_VALUE_CONTAINER : UCL_VALUE_UNKNOWN)) {
		/* Not taken on failure */
		ucl_object_unref(nobj);

		return false;
	}

	return true;
}

/**
 * Parse a cl string
 * @param parser
 * @param chunk
 * @param var_expand
 * @param need_unescape
 * @return true if a key has been parsed
 */
static bool
ucl_parse_string_value(struct ucl_parser *parser,
					   struct ucl_chunk *chunk, bool *var_expand, bool *need_unescape)
{
	const unsigned char *p;
	enum {
		UCL_BRACE_ROUND = 0,
		UCL_BRACE_SQUARE,
		UCL_BRACE_FIGURE
	};
	int braces[3][2] = {{0, 0}, {0, 0}, {0, 0}};

	p = chunk->pos;

	while (p < chunk->end) {

		/* Skip pairs of figure braces */
		if (*p == '{') {
			braces[UCL_BRACE_FIGURE][0]++;
		}
		else if (*p == '}') {
			braces[UCL_BRACE_FIGURE][1]++;
			if (braces[UCL_BRACE_FIGURE][1] <= braces[UCL_BRACE_FIGURE][0]) {
				/* This is not a termination symbol, continue */
				ucl_chunk_skipc(chunk, p);
				continue;
			}
		}
		/* Skip pairs of square braces */
		else if (*p == '[') {
			braces[UCL_BRACE_SQUARE][0]++;
		}
		else if (*p == ']') {
			braces[UCL_BRACE_SQUARE][1]++;
			if (braces[UCL_BRACE_SQUARE][1] <= braces[UCL_BRACE_SQUARE][0]) {
				/* This is not a termination symbol, continue */
				ucl_chunk_skipc(chunk, p);
				continue;
			}
		}
		else if (*p == '$') {
			*var_expand = true;
		}
		else if (*p == '\\') {
			*need_unescape = true;
			ucl_chunk_skipc(chunk, p);
			if (p < chunk->end) {
				ucl_chunk_skipc(chunk, p);
			}
			continue;
		}

		if (ucl_lex_is_atom_end(*p) || (chunk->remain >= 2 && ucl_lex_is_comment(p[0], p[1]))) {
			break;
		}
		ucl_chunk_skipc(chunk, p);
	}

	return true;
}

/**
 * Parse multiline string ending with \n{term}\n
 * @param parser
 * @param chunk
 * @param term
 * @param term_len
 * @param beg
 * @param var_expand
 * @return size of multiline string or 0 in case of error
 */
static int
ucl_parse_multiline_string(struct ucl_parser *parser,
						   struct ucl_chunk *chunk, const unsigned char *term,
						   int term_len, unsigned char const **beg,
						   bool *var_expand)
{
	const unsigned char *p, *c, *tend;
	bool newline = false;
	int len = 0;

	p = chunk->pos;

	c = p;

	while (p < chunk->end) {
		if (newline) {
			if (chunk->end - p < term_len) {
				return 0;
			}
			else if (memcmp(p, term, term_len) == 0) {
				tend = p + term_len;
				if (tend < chunk->end && *tend != '\n' && *tend != ';' && *tend != ',') {
					/* Incomplete terminator */
					ucl_chunk_skipc(chunk, p);
					continue;
				}
				len = p - c;
				chunk->remain -= term_len;
				chunk->pos = p + term_len;
				chunk->column = term_len;
				*beg = c;
				break;
			}
		}
		if (*p == '\n') {
			newline = true;
		}
		else {
			if (*p == '$') {
				*var_expand = true;
			}
			newline = false;
		}
		ucl_chunk_skipc(chunk, p);
	}

	return len;
}

static inline ucl_object_t *
ucl_parser_get_container(struct ucl_parser *parser)
{
	ucl_object_t *t, *obj = NULL;

	if (parser == NULL || parser->stack == NULL || parser->stack->obj == NULL) {
		return NULL;
	}

	if (parser->stack->obj->type == UCL_ARRAY) {
		/* Object must be allocated */
		obj = ucl_object_new_full(UCL_NULL, parser->chunks->priority);
		t = parser->stack->obj;

		if (obj == NULL) {
			ucl_set_err(parser, UCL_EINTERNAL, "cannot allocate memory for an object",
						&parser->err);
			return NULL;
		}

		if (!ucl_parser_account_node(parser)) {
			ucl_object_unref(obj);
			return NULL;
		}

		if (!ucl_array_append(t, obj)) {
			ucl_object_unref(obj);
			ucl_set_err(parser, UCL_EINTERNAL, "cannot allocate memory for an array",
						&parser->err);
			return NULL;
		}

		parser->cur_obj = obj;
		ucl_attach_comment(parser, obj, false);
	}
	else {
		/* Object has been already allocated */
		obj = parser->cur_obj;
	}

	return obj;
}

/**
 * Handle value data
 * @param parser
 * @param chunk
 * @return
 */
static bool
ucl_parse_value(struct ucl_parser *parser, struct ucl_chunk *chunk)
{
	const unsigned char *p, *c;
	ucl_object_t *obj = NULL;
	unsigned int stripped_spaces;
	ssize_t str_len;
	bool need_unescape = false, ucl_escape = false, var_expand = false;

	p = chunk->pos;

	/*
	 * Skip any spaces and comments, in any order: an indented value after
	 * a comment, as the first one of an array often is, must not take
	 * the indent
	 */
	while (p < chunk->end) {
		if (ucl_test_character(*p, UCL_CHARACTER_WHITESPACE_UNSAFE)) {
			ucl_chunk_skipc(chunk, p);
		}
		else if (chunk->remain >= 2 && ucl_lex_is_comment(p[0], p[1])) {
			if (!ucl_skip_comments(parser)) {
				return false;
			}
			p = chunk->pos;
		}
		else {
			break;
		}
	}

	if (parser->pending_merge != NULL && p < chunk->end &&
		!ucl_parser_settle_merge(parser, *p == '{' || *p == '[')) {
		return false;
	}

	while (p < chunk->end) {
		c = p;
		switch (*p) {
		case '"':
			ucl_chunk_skipc(chunk, p);

			if (!ucl_lex_json_string(parser, chunk, &need_unescape, &ucl_escape,
									 &var_expand)) {
				return false;
			}

			obj = ucl_parser_get_container(parser);
			if (!obj) {
				return false;
			}

			str_len = chunk->pos - c - 2;
			obj->type = UCL_STRING;
			if ((str_len = ucl_copy_or_store_ptr(parser, c + 1,
												 &obj->trash_stack[UCL_TRASH_VALUE],
												 &obj->value.sv, str_len,
												 parser->limits.max_string_length,
												 need_unescape, false,
												 var_expand, false)) == -1) {
				return false;
			}

			obj->len = str_len;
			parser->state = UCL_STATE_AFTER_VALUE;

			return true;
			break;
		case '\'':
			ucl_chunk_skipc(chunk, p);

			if (!ucl_lex_squoted_string(parser, chunk, &need_unescape)) {
				return false;
			}

			obj = ucl_parser_get_container(parser);
			if (!obj) {
				return false;
			}

			str_len = chunk->pos - c - 2;
			obj->type = UCL_STRING;
			obj->flags |= UCL_OBJECT_SQUOTED;

			if ((str_len = ucl_copy_or_store_ptr(parser, c + 1,
												 &obj->trash_stack[UCL_TRASH_VALUE],
												 &obj->value.sv, str_len,
												 parser->limits.max_string_length,
												 need_unescape, false,
												 var_expand, true)) == -1) {
				return false;
			}

			obj->len = str_len;

			parser->state = UCL_STATE_AFTER_VALUE;

			return true;
			break;
		case '{':
			obj = ucl_parser_get_container(parser);
			if (obj == NULL) {
				ucl_set_err_fallback(parser, UCL_ESYNTAX,
									 "object value must be a part of an object");
				return false;
			}
			/* We have a new object */
			if (parser->stack) {
				obj = ucl_parser_add_container(obj, parser, false,
											   parser->stack->e.params.level, true);
			}
			else {
				return false;
			}
			if (obj == NULL) {
				return false;
			}

			ucl_chunk_skipc(chunk, p);

			return true;
			break;
		case '[':
			obj = ucl_parser_get_container(parser);
			if (obj == NULL) {
				ucl_set_err_fallback(parser, UCL_ESYNTAX,
									 "array value must be a part of an object");
				return false;
			}
			/* We have a new array */
			if (parser->stack) {
				obj = ucl_parser_add_container(obj, parser, true,
											   parser->stack->e.params.level, true);
			}
			else {
				return false;
			}

			if (obj == NULL) {
				return false;
			}

			ucl_chunk_skipc(chunk, p);

			return true;
			break;
		case ']':
			/* We have the array ending */
			if (parser->stack && parser->stack->obj->type == UCL_ARRAY) {
				parser->state = UCL_STATE_AFTER_VALUE;
				return true;
			}
			else {
				goto parse_string;
			}
			break;
		case '<':
			obj = ucl_parser_get_container(parser);
			if (obj == NULL) {
				ucl_set_err_fallback(parser, UCL_ESYNTAX,
									 "multiline value must be a part of an object");
				return false;
			}
			/* We have something like multiline value, which must be <<[A-Z]+\n */
			if (chunk->end - p > 3) {
				if (memcmp(p, "<<", 2) == 0) {
					p += 2;
					/* We allow only uppercase characters in multiline definitions */
					while (p < chunk->end && *p >= 'A' && *p <= 'Z') {
						p++;
					}
					if (p == chunk->end) {
						ucl_set_err(parser, UCL_ESYNTAX,
									"unterminated multiline value", &parser->err);
						return false;
					}
					if (*p == '\n') {
						/* Set chunk positions and start multiline parsing */
						chunk->remain -= p - c + 1;
						c += 2;
						chunk->pos = p + 1;
						chunk->column = 0;
						chunk->line++;
						if ((str_len = ucl_parse_multiline_string(parser, chunk, c,
																  p - c, &c, &var_expand)) == 0) {
							ucl_set_err(parser, UCL_ESYNTAX,
										"unterminated multiline value", &parser->err);
							return false;
						}

						obj->type = UCL_STRING;
						obj->flags |= UCL_OBJECT_MULTILINE;
						if ((str_len = ucl_copy_or_store_ptr(parser, c,
															 &obj->trash_stack[UCL_TRASH_VALUE],
															 &obj->value.sv, str_len - 1,
															 parser->limits.max_string_length,
															 false,
															 false, var_expand, false)) == -1) {
							return false;
						}
						obj->len = str_len;

						parser->state = UCL_STATE_AFTER_VALUE;

						return true;
					}
				}
			}
			/* Fallback to ordinary strings */
			/* FALLTHRU */
		default:
		parse_string:
			if (obj == NULL) {
				obj = ucl_parser_get_container(parser);
			}

			if (obj == NULL) {
				ucl_set_err_fallback(parser, UCL_ESYNTAX,
									 "value must be a part of an object");
				return false;
			}

			/* Parse atom */
			if (ucl_test_character(*p, UCL_CHARACTER_VALUE_DIGIT_START)) {
				if (!ucl_lex_number(parser, chunk, obj)) {
					if (parser->state == UCL_STATE_ERROR) {
						return false;
					}
				}
				else {
					parser->state = UCL_STATE_AFTER_VALUE;
					return true;
				}
				/* Fallback to normal string */
			}

			if (!ucl_parse_string_value(parser, chunk, &var_expand,
										&need_unescape)) {
				return false;
			}
			/* Cut trailing spaces */
			stripped_spaces = 0;
			while (chunk->pos - stripped_spaces > c &&
				   ucl_test_character(*(chunk->pos - 1 - stripped_spaces),
									  UCL_CHARACTER_WHITESPACE)) {
				stripped_spaces++;
			}
			str_len = chunk->pos - c - stripped_spaces;
			if (str_len <= 0) {
				ucl_set_err(parser, UCL_ESYNTAX, "string value must not be empty",
							&parser->err);
				return false;
			}
			else if (str_len == 4 && memcmp(c, "null", 4) == 0) {
				obj->len = 0;
				obj->type = UCL_NULL;
			}
			else if (str_len == 3 && memcmp(c, "nan", 3) == 0) {
				obj->len = 0;
				obj->type = UCL_FLOAT;
				obj->value.dv = NAN;
			}
			else if (str_len == 3 && memcmp(c, "inf", 3) == 0) {
				obj->len = 0;
				obj->type = UCL_FLOAT;
				obj->value.dv = INFINITY;
			}
			else if (!ucl_maybe_parse_boolean(obj, c, str_len)) {
				obj->type = UCL_STRING;
				if ((str_len = ucl_copy_or_store_ptr(parser, c,
													 &obj->trash_stack[UCL_TRASH_VALUE],
													 &obj->value.sv, str_len,
													 parser->limits.max_string_length,
													 need_unescape,
													 false, var_expand, false)) == -1) {
					return false;
				}
				obj->len = str_len;
			}

			parser->state = UCL_STATE_AFTER_VALUE;

			return true;
			break;
		}
	}

	return true;
}

/**
 * Handle after value data
 * @param parser
 * @param chunk
 * @return
 */
static bool
ucl_parse_after_value(struct ucl_parser *parser, struct ucl_chunk *chunk)
{
	const unsigned char *p;
	bool got_sep = false;
	struct ucl_stack *st;
	ucl_object_t *closed;

	p = chunk->pos;

	while (p < chunk->end) {
		if (ucl_test_character(*p, UCL_CHARACTER_WHITESPACE)) {
			/* Skip whitespaces */
			ucl_chunk_skipc(chunk, p);
		}
		else if (chunk->remain >= 2 && ucl_lex_is_comment(p[0], p[1])) {
			/* Skip comment */
			if (!ucl_skip_comments(parser)) {
				return false;
			}
			/* Treat comment as a separator */
			got_sep = true;
			p = chunk->pos;
		}
		else if (ucl_test_character(*p, UCL_CHARACTER_VALUE_END)) {
			if (*p == '}' || *p == ']') {
				if (parser->stack == NULL) {
					ucl_set_err(parser, UCL_ESYNTAX,
								"end of array or object detected without corresponding start",
								&parser->err);
					return false;
				}
				if ((*p == '}' && parser->stack->obj->type == UCL_OBJECT) ||
					(*p == ']' && parser->stack->obj->type == UCL_ARRAY)) {

					/* Pop all nested objects from a stack */
					st = parser->stack;

					if (!(st->e.params.flags & UCL_STACK_HAS_OBRACE)) {
						parser->err_code = UCL_EUNPAIRED;
						ucl_create_err(&parser->err,
									   "%s:%d object closed with } is not opened with { at line %d",
									   chunk->fname ? chunk->fname : "memory",
									   parser->chunks->line, st->e.params.line);

						return false;
					}

					if ((st->e.params.flags & UCL_STACK_AUTOMATIC)) {
						/* A chunk's brace, which has taken no depth */
						parser->stack = st->next;
						UCL_FREE(sizeof(struct ucl_stack), st);
						closed = NULL;
					}
					else {
						closed = st->obj;
						ucl_parser_pop_container(parser, st);
					}

					if (closed != NULL && parser->cur_obj == closed) {
						/* Nothing has been read inside the container */
						ucl_attach_inner_comment(parser, closed);
					}
					else if (parser->cur_obj) {
						ucl_attach_comment(parser, parser->cur_obj, true);
					}

					/*
					 * Comments after the container that no key or value
					 * takes follow it rather than the last value inside it,
					 * as a macro read as a comment is none inside an array
					 */
					if (closed != NULL) {
						parser->cur_obj = closed;
					}

					while (parser->stack != NULL) {
						st = parser->stack;

						if (st->next == NULL) {
							break;
						}
						else if (st->next->e.params.level == st->e.params.level) {
							break;
						}


						parser->cur_obj = st->obj;
						ucl_parser_pop_container(parser, st);
					}
				}
				else {
					ucl_set_err(parser, UCL_ESYNTAX,
								"unexpected terminating symbol detected",
								&parser->err);
					return false;
				}

				if (parser->stack == NULL) {
					/* Ignore everything after a top object */
					return true;
				}
				else {
					ucl_chunk_skipc(chunk, p);
				}
				got_sep = true;
			}
			else {
				/* Got a separator */
				got_sep = true;
				ucl_chunk_skipc(chunk, p);
			}
		}
		else {
			/* Anything else */
			if (!got_sep) {
				ucl_set_err(parser, UCL_ESYNTAX, "delimiter is missing",
							&parser->err);
				return false;
			}
			return true;
		}
	}

	return true;
}

/**
 * Handle macro data
 * @param parser
 * @param chunk
 * @param marco
 * @param macro_start
 * @param macro_len
 * @return
 */
static bool
ucl_parse_macro_value(struct ucl_parser *parser,
					  struct ucl_chunk *chunk, struct ucl_macro *macro,
					  unsigned char const **macro_start, size_t *macro_len)
{
	const unsigned char *p, *c;
	bool need_unescape = false, ucl_escape = false, var_expand = false;

	p = chunk->pos;

	if (p >= chunk->end) {
		ucl_set_err(parser, UCL_ESYNTAX,
					"unexpected end of input while parsing macro value",
					&parser->err);
		return false;
	}

	switch (*p) {
	case '"':
		/* We have macro value encoded in quotes */
		c = p;
		ucl_chunk_skipc(chunk, p);
		if (!ucl_lex_json_string(parser, chunk, &need_unescape, &ucl_escape, &var_expand)) {
			return false;
		}

		*macro_start = c + 1;
		*macro_len = chunk->pos - c - 2;
		p = chunk->pos;
		break;
	case '{':
		/* We got a multiline macro body */
		ucl_chunk_skipc(chunk, p);
		/* Skip spaces at the beginning */
		while (p < chunk->end) {
			if (ucl_test_character(*p, UCL_CHARACTER_WHITESPACE_UNSAFE)) {
				ucl_chunk_skipc(chunk, p);
			}
			else {
				break;
			}
		}
		c = p;
		while (p < chunk->end) {
			if (*p == '}') {
				break;
			}
			ucl_chunk_skipc(chunk, p);
		}
		if (p >= chunk->end) {
			ucl_set_err(parser, UCL_ESYNTAX,
						"unterminated macro value, missing '}'",
						&parser->err);
			return false;
		}
		*macro_start = c;
		*macro_len = p - c;
		ucl_chunk_skipc(chunk, p);
		break;
	default:
		/* Macro is not enclosed in quotes or braces */
		c = p;
		while (p < chunk->end) {
			if (ucl_lex_is_atom_end(*p)) {
				break;
			}
			ucl_chunk_skipc(chunk, p);
		}
		*macro_start = c;
		*macro_len = p - c;
		break;
	}

	/* We are at the end of a macro */
	/* Skip ';' and space characters and return to previous state */
	while (p < chunk->end) {
		if (!ucl_test_character(*p, UCL_CHARACTER_WHITESPACE_UNSAFE) && *p != ';') {
			break;
		}
		ucl_chunk_skipc(chunk, p);
	}
	return true;
}

/**
 * Parse macro arguments as UCL object
 * @param parser parser structure
 * @param chunk the current data chunk
 * @return
 */
static ucl_object_t *
ucl_parse_macro_arguments(struct ucl_parser *parser,
						  struct ucl_chunk *chunk)
{
	ucl_object_t *res = NULL;
	struct ucl_parser *params_parser;
	int obraces = 1, ebraces = 0, state = 0;
	const unsigned char *p, *c;
	size_t args_len = 0;
	struct ucl_parser_saved_state saved;

	saved.column = chunk->column;
	saved.line = chunk->line;
	saved.pos = chunk->pos;
	saved.remain = chunk->remain;
	p = chunk->pos;

	if (chunk->remain < 2 || *p != '(') {
		return NULL;
	}

	/* Set begin and start */
	ucl_chunk_skipc(chunk, p);
	c = p;

	while ((p) < (chunk)->end) {
		switch (state) {
		case 0:
			/* Parse symbols and check for '(', ')' and '"' */
			if (*p == '(') {
				obraces++;
			}
			else if (*p == ')') {
				ebraces++;
			}
			else if (*p == '"') {
				state = 1;
			}
			/* Check pairing */
			if (obraces == ebraces) {
				state = 99;
			}
			else {
				args_len++;
			}
			/* Check overflow */
			if (chunk->remain == 0) {
				goto restore_chunk;
			}
			ucl_chunk_skipc(chunk, p);
			break;
		case 1:
			/* We have quote character, so skip all but quotes */
			if (*p == '"' && *(p - 1) != '\\') {
				state = 0;
			}
			if (chunk->remain == 0) {
				goto restore_chunk;
			}
			args_len++;
			ucl_chunk_skipc(chunk, p);
			break;
		case 99:
			/*
			 * We have read the full body of arguments, so we need to parse and set
			 * object from that
			 */
			params_parser = ucl_parser_new(parser->flags);
			if (!ucl_parser_add_chunk(params_parser, c, args_len)) {
				ucl_set_err(parser, UCL_ESYNTAX, "macro arguments parsing error",
							&parser->err);
			}
			else {
				res = ucl_parser_get_object(params_parser);
			}
			ucl_parser_free(params_parser);

			return res;

			break;
		}
	}

	return res;

restore_chunk:
	chunk->column = saved.column;
	chunk->line = saved.line;
	chunk->pos = saved.pos;
	chunk->remain = saved.remain;

	return NULL;
}

#define SKIP_SPACES_COMMENTS(parser, chunk, p)                                    \
	do {                                                                          \
		while ((p) < (chunk)->end) {                                              \
			if (!ucl_test_character(*(p), UCL_CHARACTER_WHITESPACE_UNSAFE)) {     \
				if ((chunk)->remain >= 2 && ucl_lex_is_comment((p)[0], (p)[1])) { \
					if (!ucl_skip_comments(parser)) {                             \
						return false;                                             \
					}                                                             \
					p = (chunk)->pos;                                             \
				}                                                                 \
				break;                                                            \
			}                                                                     \
			ucl_chunk_skipc(chunk, p);                                            \
		}                                                                         \
	} while (0)

/*
 * Skip a macro from its '.' to the end of its value as UCL_STATE_MACRO_NAME
 * and UCL_STATE_MACRO read it, without looking its name up, parsing its
 * arguments or calling it
 */
static bool
ucl_skip_macro(struct ucl_parser *parser, struct ucl_chunk *chunk)
{
	const unsigned char *p = chunk->pos, *name, *value;
	size_t value_len;
	int obraces = 1, ebraces = 0;
	bool quoted = false;

	ucl_chunk_skipc(chunk, p);
	name = p;

	while (p < chunk->end &&
		   !ucl_test_character(*p, UCL_CHARACTER_WHITESPACE_UNSAFE) &&
		   *p != '(') {
		ucl_chunk_skipc(chunk, p);
	}

	if (p == name) {
		ucl_set_err(parser, UCL_ESYNTAX, "invalid macro name", &parser->err);
		return false;
	}

	SKIP_SPACES_COMMENTS(parser, chunk, p);

	/* Arguments end at their closing parenthesis, outside of strings */
	if (chunk->remain >= 2 && *p == '(') {
		ucl_chunk_skipc(chunk, p);

		while (p < chunk->end && obraces != ebraces) {
			if (quoted) {
				if (*p == '"' && *(p - 1) != '\\') {
					quoted = false;
				}
			}
			else if (*p == '"') {
				quoted = true;
			}
			else if (*p == '(') {
				obraces++;
			}
			else if (*p == ')') {
				ebraces++;
			}

			ucl_chunk_skipc(chunk, p);
		}

		SKIP_SPACES_COMMENTS(parser, chunk, p);
	}

	return ucl_parse_macro_value(parser, chunk, NULL, &value, &value_len);
}

/**
 * With UCL_PARSER_DISABLE_MACRO, read a macro as a comment: skip it, and with
 * UCL_PARSER_SAVE_COMMENTS save it as it is written, comments inside it
 * included
 * @param parser
 * @param chunk the current chunk, at the '.' that starts the macro
 * @return true if a whole macro has been skipped
 */
static bool
ucl_skip_macro_as_comment(struct ucl_parser *parser, struct ucl_chunk *chunk)
{
	const unsigned char *beg = chunk->pos, *end;
	int flags = parser->flags;
	bool ret;

	parser->flags &= ~UCL_PARSER_SAVE_COMMENTS;
	ret = ucl_skip_macro(parser, chunk);
	parser->flags = flags;

	if (ret && (flags & UCL_PARSER_SAVE_COMMENTS)) {
		/* Without the spaces and semicolons skipped after the value */
		end = chunk->pos;

		while (end > beg &&
			   (ucl_test_character(end[-1], UCL_CHARACTER_WHITESPACE_UNSAFE) ||
				end[-1] == ';')) {
			end--;
		}

		ucl_save_comment(parser, (const char *) beg, end - beg);
	}

	return ret;
}

/**
 * Handle the main states of rcl parser
 * @param parser parser structure
 * @return true if chunk has been parsed and false in case of error
 */
static bool
ucl_state_machine(struct ucl_parser *parser)
{
	ucl_object_t *obj, *macro_args;
	struct ucl_chunk *chunk = parser->chunks;
	const unsigned char *p, *c = NULL, *macro_start = NULL;
	unsigned char *macro_escaped;
	size_t macro_len = 0;
	struct ucl_macro *macro = NULL;
	bool next_key = false, end_of_object = false, got_content = false, ret;

	if (parser->top_obj == NULL) {
		parser->state = UCL_STATE_INIT;
	}

	p = chunk->pos;
	while (chunk->pos < chunk->end) {
		switch (parser->state) {
		case UCL_STATE_INIT:
			/*
			 * At the init state we can either go to the parse array or object
			 * if we got [ or { correspondingly or can just treat new data as
			 * a key of newly created object
			 */
			if (!ucl_skip_comments(parser)) {
				parser->prev_state = parser->state;
				parser->state = UCL_STATE_ERROR;
				return false;
			}
			else {
				bool seen_obrace = false;

				/* Skip any spaces */
				while (p < chunk->end && ucl_test_character(*p,
															UCL_CHARACTER_WHITESPACE_UNSAFE)) {
					ucl_chunk_skipc(chunk, p);
				}

				p = chunk->pos;

				if (p < chunk->end) {
					if (*p == '[') {
						parser->state = UCL_STATE_VALUE;
						ucl_chunk_skipc(chunk, p);
						seen_obrace = true;
					}
					else {

						if (*p == '{') {
							ucl_chunk_skipc(chunk, p);
							parser->state = UCL_STATE_KEY_OBRACE;
							seen_obrace = true;
						}
						else {
							parser->state = UCL_STATE_KEY;
						}
					}
				}

				if (parser->top_obj == NULL) {
					if (parser->state == UCL_STATE_VALUE) {
						obj = ucl_parser_add_container(NULL, parser, true, 0,
													   seen_obrace);
					}
					else {
						obj = ucl_parser_add_container(NULL, parser, false, 0,
													   seen_obrace);
					}

					if (obj == NULL) {
						return false;
					}

					parser->top_obj = obj;
					parser->cur_obj = obj;
				}
				else if (parser->state == UCL_STATE_VALUE) {
					/*
					 * An inserted or included chunk that starts with [ has no
					 * key to attach its value to. Falling through would let
					 * ucl_parser_get_container() hand back the stale
					 * parser->cur_obj, i.e. the previous key's value: an
					 * inserted [5 silently overwrites it, and [{ or [[ reuses
					 * its scalar storage as the new container's, which crashes
					 * in ucl_hash_destroy() when the tree is freed.
					 * Only an array frame can take a value with no key.
					 */
					if (parser->stack == NULL || parser->stack->obj == NULL ||
						parser->stack->obj->type != UCL_ARRAY) {
						ucl_set_err(parser, UCL_ESYNTAX,
									"array value without a key",
									&parser->err);
						parser->prev_state = parser->state;
						parser->state = UCL_STATE_ERROR;

						return false;
					}
				}
				else if (parser->state == UCL_STATE_KEY_OBRACE &&
						 (parser->stack == NULL || parser->stack->obj == NULL ||
						  parser->stack->obj->type != UCL_OBJECT)) {
					/*
					 * The mirror image: { opens the body of the current
					 * object, so only an object frame can take it. On an
					 * array frame UCL_STATE_KEY_OBRACE would mark the array
					 * as braced before the key check in UCL_STATE_KEY could
					 * reject it, and the array's ] would no longer close it.
					 */
					ucl_set_err(parser, UCL_ESYNTAX,
								"key outside of an object",
								&parser->err);
					parser->prev_state = parser->state;
					parser->state = UCL_STATE_ERROR;

					return false;
				}
				else if (parser->state == UCL_STATE_KEY_OBRACE) {
					/*
					 * A chunk that starts with { in an open object, as an
					 * included file can, adds its keys to that object, as in
					 * an unbraced root. Marked on the object's frame, the
					 * brace would be closed by the object's own } or take that
					 * one with it. Its own frame keeps the two apart, and is
					 * checked at the end of the chunk like any other brace the
					 * chunk opens.
					 */
					if (!ucl_parser_add_chunk_brace(parser, chunk)) {
						parser->prev_state = parser->state;
						parser->state = UCL_STATE_ERROR;

						return false;
					}
				}
			}
			break;
		case UCL_STATE_KEY_OBRACE:
			/*
			 * The brace has its frame already, see UCL_STATE_INIT. A macro
			 * at the start of the chunk returns to this state.
			 */
			/* FALLTHROUGHT */
		case UCL_STATE_KEY:
			/* Skip any spaces */
			while (p < chunk->end && ucl_test_character(*p, UCL_CHARACTER_WHITESPACE_UNSAFE)) {
				ucl_chunk_skipc(chunk, p);
			}
			if (p == chunk->end) {
				/*
				 * The key can start the next chunk, which needs no separator
				 * before it as UCL_STATE_AFTER_VALUE would
				 */
				break;
			}
			if (*p == '}') {
				/* We have the end of an object */
				parser->state = UCL_STATE_AFTER_VALUE;
				continue;
			}
			if (parser->stack == NULL) {
				/* No objects are on stack, but we want to parse a key */
				ucl_set_err(parser, UCL_ESYNTAX, "top object is finished but the parser "
												 "expects a key",
							&parser->err);
				parser->prev_state = parser->state;
				parser->state = UCL_STATE_ERROR;
				return false;
			}
			if (parser->stack->obj->type != UCL_OBJECT) {
				/*
				 * Only an object frame can take a key. An array can be on top
				 * here: an inserted chunk starts in UCL_STATE_INIT, which
				 * picks this state for anything but [ or {, and .include
				 * restores the state it was called in even if the included
				 * chunk closed the enclosing object. ucl_parse_key() would
				 * then pass the array's storage to ucl_hash_insert().
				 */
				ucl_set_err(parser, UCL_ESYNTAX, "key outside of an object",
							&parser->err);
				parser->prev_state = parser->state;
				parser->state = UCL_STATE_ERROR;
				return false;
			}

			got_content = false;

			if (!ucl_parse_key(parser, chunk, &next_key, &end_of_object, &got_content)) {
				parser->prev_state = parser->state;
				parser->state = UCL_STATE_ERROR;
				return false;
			}

			if (end_of_object) {
				p = chunk->pos;
				parser->state = UCL_STATE_AFTER_VALUE;
				continue;
			}
			else if (parser->state != UCL_STATE_MACRO_NAME) {
				if (next_key && parser->stack->obj->type == UCL_OBJECT) {
					/* Parse more keys and nest objects accordingly */
					obj = ucl_parser_add_container(parser->cur_obj,
												   parser,
												   false,
												   parser->stack->e.params.level + 1,
												   parser->state == UCL_STATE_KEY_OBRACE);
					if (obj == NULL) {
						return false;
					}
				}
				else if (got_content) {
					/* Do not switch state if we have not read any content */
					parser->state = UCL_STATE_VALUE;
				}
			}
			else {
				c = chunk->pos;
			}
			p = chunk->pos;
			break;
		case UCL_STATE_VALUE:
			/* We need to check what we do have */
			if (!parser->cur_obj || !ucl_parse_value(parser, chunk)) {
				parser->prev_state = parser->state;
				parser->state = UCL_STATE_ERROR;
				return false;
			}
			/* State is set in ucl_parse_value call */
			p = chunk->pos;
			break;
		case UCL_STATE_AFTER_VALUE:
			if (!ucl_parse_after_value(parser, chunk)) {
				parser->prev_state = parser->state;
				parser->state = UCL_STATE_ERROR;
				return false;
			}

			if (parser->stack != NULL) {
				if (parser->stack->obj->type == UCL_OBJECT) {
					parser->state = UCL_STATE_KEY;
				}
				else {
					/* Array */
					parser->state = UCL_STATE_VALUE;
				}
			}
			else {
				/* Skip everything at the end */
				return true;
			}

			p = chunk->pos;
			break;
		case UCL_STATE_MACRO_NAME:
			if (!ucl_test_character(*p, UCL_CHARACTER_WHITESPACE_UNSAFE) &&
				*p != '(') {
				ucl_chunk_skipc(chunk, p);
			}
			else {
				if (c != NULL && p - c > 0) {
					/* We got macro name */
					macro_len = (size_t) (p - c);
					HASH_FIND(hh, parser->macroes, c, macro_len, macro);
					if (macro == NULL) {
						ucl_create_err(&parser->err,
									   "error at %s:%d at column %d: "
									   "unknown macro: '%.*s', character: '%c'",
									   chunk->fname ? chunk->fname : "memory",
									   chunk->line,
									   chunk->column,
									   (int) (p - c),
									   c,
									   *chunk->pos);
						parser->state = UCL_STATE_ERROR;
						return false;
					}
					/* Now we need to skip all spaces */
					SKIP_SPACES_COMMENTS(parser, chunk, p);
					parser->state = UCL_STATE_MACRO;
				}
				else {
					/* We have invalid macro name */
					ucl_create_err(&parser->err,
								   "error at %s:%d at column %d: invalid macro name",
								   chunk->fname ? chunk->fname : "memory",
								   chunk->line,
								   chunk->column);
					parser->state = UCL_STATE_ERROR;
					return false;
				}
			}
			break;
		case UCL_STATE_MACRO:
			if (*chunk->pos == '(') {
				macro_args = ucl_parse_macro_arguments(parser, chunk);
				p = chunk->pos;
				if (macro_args) {
					SKIP_SPACES_COMMENTS(parser, chunk, p);
				}
			}
			else {
				macro_args = NULL;
			}
			if (!ucl_parse_macro_value(parser, chunk, macro,
									   &macro_start, &macro_len)) {
				parser->prev_state = parser->state;
				parser->state = UCL_STATE_ERROR;
				return false;
			}
			macro_len = ucl_expand_variable(parser, &macro_escaped,
											macro_start, macro_len);
			parser->state = parser->prev_state;

			if (macro_escaped == NULL && macro != NULL) {
				if (macro->is_context) {
					ret = macro->h.context_handler(macro_start, macro_len,
												   macro_args,
												   parser->top_obj,
												   macro->ud);
				}
				else {
					ret = macro->h.handler(macro_start, macro_len, macro_args,
										   macro->ud);
				}
			}
			else if (macro != NULL) {
				if (macro->is_context) {
					ret = macro->h.context_handler(macro_escaped, macro_len,
												   macro_args,
												   parser->top_obj,
												   macro->ud);
				}
				else {
					ret = macro->h.handler(macro_escaped, macro_len, macro_args,
										   macro->ud);
				}

				UCL_FREE(macro_len + 1, macro_escaped);
			}
			else {
				ret = false;
				ucl_set_err(parser, UCL_EINTERNAL,
							"internal error: parser has macro undefined", &parser->err);
			}

			/*
			 * Chunk can be modified within macro handler
			 */
			chunk = parser->chunks;
			p = chunk->pos;

			if (macro_args) {
				ucl_object_unref(macro_args);
			}

			if (!ret) {
				return false;
			}
			break;
		case UCL_STATE_ERROR:
			/* Already in the error state */
			return false;
		default:
			ucl_set_err(parser, UCL_EINTERNAL,
						"internal error: parser is in an unknown state", &parser->err);
			parser->state = UCL_STATE_ERROR;
			return false;
		}
	}

	/*
	 * A key whose merge is pending takes the comments before it once its
	 * value starts, in whichever object that value ends up
	 */
	if (parser->last_comment && parser->pending_merge == NULL) {
		if (parser->cur_obj) {
			ucl_attach_comment(parser, parser->cur_obj, true);
		}
		else if (parser->stack && parser->stack->obj) {
			ucl_attach_comment(parser, parser->stack->obj, true);
		}
		else if (parser->top_obj) {
			ucl_attach_comment(parser, parser->top_obj, true);
		}
		else {
			ucl_object_unref(parser->last_comment);
			parser->last_comment = NULL;
		}
	}

	if (parser->stack != NULL && parser->state != UCL_STATE_ERROR) {
		struct ucl_stack *st;
		bool has_error = false;

		LL_FOREACH(parser->stack, st)
		{
			if (st->chunk != parser->chunks) {
				break; /* Not our chunk, give up */
			}
			if (st->e.params.flags & UCL_STACK_HAS_OBRACE) {
				if (parser->err == NULL) {
					utstring_new(parser->err);
				}

				utstring_printf(parser->err, "%s:%d unmatched open brace at %d; ",
								chunk->fname ? chunk->fname : "memory",
								parser->chunks->line,
								st->e.params.line);

				has_error = true;
			}
		}

		if (has_error) {
			parser->err_code = UCL_EUNPAIRED;

			return false;
		}
	}

	return true;
}

#define UPRM_SAFE(fn, a, b, c, el) \
	do {                           \
		if (!fn(a, b, c, a))       \
			goto el;               \
	} while (0)

struct ucl_parser *
ucl_parser_new(int flags)
{
	struct ucl_parser *parser;

	parser = UCL_ALLOC(sizeof(struct ucl_parser));
	if (parser == NULL) {
		return NULL;
	}

	memset(parser, 0, sizeof(struct ucl_parser));

	UPRM_SAFE(ucl_parser_register_macro, parser, "include", ucl_include_handler, e0);
	UPRM_SAFE(ucl_parser_register_macro, parser, "try_include", ucl_try_include_handler, e0);
	UPRM_SAFE(ucl_parser_register_macro, parser, "includes", ucl_includes_handler, e0);
	UPRM_SAFE(ucl_parser_register_macro, parser, "priority", ucl_priority_handler, e0);
	UPRM_SAFE(ucl_parser_register_macro, parser, "load", ucl_load_handler, e0);
	UPRM_SAFE(ucl_parser_register_context_macro, parser, "inherit", ucl_inherit_handler, e0);

	parser->flags = flags;
	parser->includepaths = NULL;

	/*
	 * Only the depth limit is on by default: it is the one that protects
	 * against stack growth in the recursive walks over the resulting tree,
	 * and 1024 levels is far beyond any legitimate configuration. The
	 * remaining budgets would break large but perfectly valid inputs (maps,
	 * language models, big JSON replies), so callers that deal with
	 * untrusted data opt into them explicitly via ucl_parser_set_limits().
	 */
	parser->limits.max_depth = UCL_MAX_NESTING;

	if (flags & UCL_PARSER_SAVE_COMMENTS) {
		parser->comments = ucl_object_typed_new(UCL_OBJECT);
	}

	if (!(flags & UCL_PARSER_NO_FILEVARS)) {
		/* Initial assumption about filevars */
		ucl_parser_set_filevars(parser, NULL, false);
	}

	return parser;
e0:
	ucl_parser_free(parser);
	return NULL;
}

bool ucl_parser_set_default_priority(struct ucl_parser *parser, unsigned prio)
{
	if (parser == NULL) {
		return false;
	}

	parser->default_priority = prio;

	return true;
}

int ucl_parser_get_default_priority(struct ucl_parser *parser)
{
	if (parser == NULL) {
		return -1;
	}

	return parser->default_priority;
}

void ucl_parser_set_limits(struct ucl_parser *parser,
						   const struct ucl_parser_limits *limits)
{
	if (parser == NULL) {
		return;
	}

	if (limits == NULL) {
		memset(&parser->limits, 0, sizeof(parser->limits));
		parser->limits.max_depth = UCL_MAX_NESTING;
	}
	else {
		memcpy(&parser->limits, limits, sizeof(parser->limits));
	}
}

void ucl_parser_get_limits(struct ucl_parser *parser,
						   struct ucl_parser_limits *limits)
{
	if (parser == NULL || limits == NULL) {
		return;
	}

	memcpy(limits, &parser->limits, sizeof(*limits));
}

bool ucl_parser_register_macro(struct ucl_parser *parser, const char *macro,
							   ucl_macro_handler handler, void *ud)
{
	struct ucl_macro *new;

	if (macro == NULL || handler == NULL) {
		return false;
	}

	new = UCL_ALLOC(sizeof(struct ucl_macro));
	if (new == NULL) {
		return false;
	}

	memset(new, 0, sizeof(struct ucl_macro));
	new->h.handler = handler;
	new->name = UCL_STRDUP(macro);
	if (new->name == NULL) {
		UCL_FREE(sizeof(struct ucl_macro), new);
		return false;
	}
	new->ud = ud;
	HASH_ADD_KEYPTR(hh, parser->macroes, new->name, strlen(new->name), new);
	return true;
}

bool ucl_parser_register_context_macro(struct ucl_parser *parser, const char *macro,
									   ucl_context_macro_handler handler, void *ud)
{
	struct ucl_macro *new;

	if (macro == NULL || handler == NULL) {
		return false;
	}

	new = UCL_ALLOC(sizeof(struct ucl_macro));
	if (new == NULL) {
		return false;
	}

	memset(new, 0, sizeof(struct ucl_macro));
	new->h.context_handler = handler;
	new->name = UCL_STRDUP(macro);
	if (new->name == NULL) {
		UCL_FREE(sizeof(struct ucl_macro), new);
		return false;
	}
	new->ud = ud;
	new->is_context = true;
	HASH_ADD_KEYPTR(hh, parser->macroes, new->name, strlen(new->name), new);
	return true;
}

void ucl_parser_register_variable(struct ucl_parser *parser, const char *var,
								  const char *value)
{
	struct ucl_variable *new = NULL, *cur;

	if (var == NULL) {
		return;
	}

	/* Find whether a variable already exists */
	LL_FOREACH(parser->variables, cur)
	{
		if (strcmp(cur->var, var) == 0) {
			new = cur;
			break;
		}
	}

	if (value == NULL) {

		if (new != NULL) {
			/* Remove variable */
			DL_DELETE(parser->variables, new);
			UCL_FREE(new->var_len + 1, new->var);
			UCL_FREE(new->value_len + 1, new->value);
			UCL_FREE(sizeof(struct ucl_variable), new);
		}
		else {
			/* Do nothing */
			return;
		}
	}
	else {
		if (new == NULL) {
			new = UCL_ALLOC(sizeof(struct ucl_variable));
			if (new == NULL) {
				return;
			}
			memset(new, 0, sizeof(struct ucl_variable));
			new->var = UCL_STRDUP(var);
			new->var_len = strlen(var);
			new->value = UCL_STRDUP(value);
			new->value_len = strlen(value);

			DL_APPEND(parser->variables, new);
		}
		else {
			UCL_FREE(new->value_len + 1, new->value);
			new->value = UCL_STRDUP(value);
			new->value_len = strlen(value);
		}
	}
}

void ucl_parser_set_variables_handler(struct ucl_parser *parser,
									  ucl_variable_handler handler, void *ud)
{
	parser->var_handler = handler;
	parser->var_data = ud;
}

bool ucl_parser_add_chunk_full(struct ucl_parser *parser, const unsigned char *data,
							   size_t len, unsigned priority, enum ucl_duplicate_strategy strat,
							   enum ucl_parse_type parse_type)
{
	struct ucl_chunk *chunk;
	struct ucl_parser_special_handler *special_handler;

	if (parser == NULL) {
		return false;
	}

	if (data == NULL && len != 0) {
		ucl_create_err(&parser->err, "invalid chunk added");
		return false;
	}

	if (parser->state != UCL_STATE_ERROR) {
		chunk = UCL_ALLOC(sizeof(struct ucl_chunk));
		if (chunk == NULL) {
			ucl_create_err(&parser->err, "cannot allocate chunk structure");
			return false;
		}

		memset(chunk, 0, sizeof(*chunk));

		/* Apply all matching handlers from the first to the last */
		LL_FOREACH(parser->special_handlers, special_handler)
		{
			if ((special_handler->flags & UCL_SPECIAL_HANDLER_PREPROCESS_ALL) ||
				(len >= special_handler->magic_len &&
				 memcmp(data, special_handler->magic, special_handler->magic_len) == 0)) {
				unsigned char *ndata = NULL;
				size_t nlen = 0;

				if (!special_handler->handler(parser, data, len, &ndata, &nlen,
											  special_handler->user_data)) {
					/* Also releases the outputs of the previous handlers */
					ucl_chunk_free(chunk);
					ucl_create_err(&parser->err, "call for external handler failed");

					return false;
				}

				struct ucl_parser_special_handler_chain *nchain;
				nchain = UCL_ALLOC(sizeof(*nchain));
				if (nchain == NULL) {
					/* Release ndata the way ucl_chunk_free would have */
					if (special_handler->free_function) {
						special_handler->free_function(ndata, nlen,
													   special_handler->user_data);
					}
					else {
						UCL_FREE(nlen, ndata);
					}
					ucl_chunk_free(chunk);
					ucl_create_err(&parser->err,
								   "cannot allocate memory for special handler chain");

					return false;
				}
				nchain->begin = ndata;
				nchain->len = nlen;
				nchain->special_handler = special_handler;

				/* Free order is reversed */
				LL_PREPEND(chunk->special_handlers, nchain);

				data = ndata;
				len = nlen;
			}
		}

		if (parse_type == UCL_PARSE_AUTO && len > 0) {
			/* We need to detect parse type by the first symbol */
			if (len >= 3 && data[0] == 0xd9 && data[1] == 0xd9 &&
				data[2] == 0xf7) {
				/*
				 * The self-described cbor tag, 55799. Cbor and msgpack are
				 * otherwise indistinguishable from their first byte - both
				 * spend 0x80 to 0xbf on short containers and strings - so
				 * this prefix is the only thing that tells them apart, and
				 * cbor without it has to be requested explicitly.
				 */
				parse_type = UCL_PARSE_CBOR;
			}
			else if ((*data & 0x80) == 0x80) {
				parse_type = UCL_PARSE_MSGPACK;
			}
			else if (*data == '(') {
				parse_type = UCL_PARSE_CSEXP;
			}
			else {
				parse_type = UCL_PARSE_UCL;
			}
		}

		chunk->begin = data;
		chunk->remain = len;
		chunk->pos = chunk->begin;
		chunk->end = chunk->begin + len;
		chunk->line = 1;
		chunk->column = 0;
		chunk->priority = priority;
		chunk->strategy = strat;
		chunk->parse_type = parse_type;

		if (parser->cur_file) {
			chunk->fname = UCL_STRDUP(parser->cur_file);
		}

		LL_PREPEND(parser->chunks, chunk);
		parser->recursion++;

		if (parser->recursion > UCL_MAX_RECURSION) {
			ucl_create_err(&parser->err, "maximum include nesting limit is reached: %d",
						   parser->recursion);
			return false;
		}

		if (len > 0) {
			/*
			 * An empty first chunk exposes an object without starting a parse.
			 * Replace that placeholder so the first nonempty chunk determines
			 * the root type and priority, and accounts for the root's cost.
			 * Real roots, including completed empty containers, stay intact.
			 */
			if (parser->top_obj_is_placeholder) {
				ucl_object_unref(parser->top_obj);
				parser->top_obj = NULL;
				parser->top_obj_is_placeholder = false;
			}

			/* Need to parse something */
			switch (parse_type) {
			default:
			case UCL_PARSE_UCL:
				return ucl_state_machine(parser);
			case UCL_PARSE_MSGPACK:
				return ucl_parse_msgpack(parser);
			case UCL_PARSE_CBOR:
				return ucl_parse_cbor(parser);
			case UCL_PARSE_CSEXP:
				return ucl_parse_csexp(parser);
			}
		}
		else {
			/* Just add empty chunk and go forward */
			if (parser->top_obj == NULL) {
				/*
				 * In case of empty object, create one to indicate that we've
				 * read something
				 */
				parser->top_obj = ucl_object_new_full(UCL_OBJECT, priority);
				parser->top_obj_is_placeholder = true;
			}

			return true;
		}
	}

	ucl_create_err(&parser->err, "a parser is in an invalid state");

	return false;
}

bool ucl_parser_add_chunk_priority(struct ucl_parser *parser,
								   const unsigned char *data, size_t len, unsigned priority)
{
	/* We dereference parser, so this check is essential */
	if (parser == NULL) {
		return false;
	}

	return ucl_parser_add_chunk_full(parser, data, len,
									 priority, UCL_DUPLICATE_APPEND, UCL_PARSE_UCL);
}

bool ucl_parser_add_chunk(struct ucl_parser *parser, const unsigned char *data,
						  size_t len)
{
	if (parser == NULL) {
		return false;
	}

	return ucl_parser_add_chunk_full(parser, data, len,
									 parser->default_priority, UCL_DUPLICATE_APPEND, UCL_PARSE_UCL);
}

bool ucl_parser_insert_chunk(struct ucl_parser *parser, const unsigned char *data,
							 size_t len)
{
	if (parser == NULL || parser->top_obj == NULL || parser->stack == NULL) {
		return false;
	}

	if (parser->state == UCL_STATE_ERROR) {
		/* As in ucl_parser_add_chunk_full(): a failed parse stays failed */
		ucl_create_err(&parser->err, "a parser is in an invalid state");
		return false;
	}

	bool res;
	struct ucl_chunk *chunk, *outer = parser->chunks;
	struct ucl_stack *st;
	/* The frame the outer chunk stopped in */
	const ucl_object_t *frame = parser->stack->obj;
	ucl_type_t frame_type = frame->type;
	unsigned int depth, new_depth;

	int state = parser->state;
	parser->state = UCL_STATE_INIT;
	LL_COUNT(parser->stack, st, depth);

	/* Prevent inserted chunks from unintentionally closing the current object */
	if (parser->stack != NULL && parser->stack->next != NULL) {
		parser->stack->e.params.level = parser->stack->next->e.params.level;
	}

	res = ucl_parser_add_chunk_full(parser, data, len, outer->priority,
									outer->strategy, outer->parse_type);

	if (parser->state == UCL_STATE_MACRO_NAME ||
		parser->state == UCL_STATE_MACRO) {
		/*
		 * A macro cannot go on in the outer chunk: its name and handler are
		 * locals of the parse that has just returned, and the outer parse
		 * would use its own on the data that follows. Keep the error of a
		 * chunk that has failed already.
		 */
		ucl_set_err_fallback(parser, UCL_ESYNTAX, "unfinished macro");
		res = false;
	}

	/*
	 * Remove the inserted chunk and resume the outer one. The insert can fail
	 * before its chunk is added (a special handler refusing the data, for
	 * one), and a failed .include in it leaves the included chunk on top, so
	 * remove everything above the outer chunk rather than the head alone.
	 * Frames left open by a removed chunk now belong to the outer one: the
	 * check for unmatched braces at the end of a chunk walks the frames of
	 * that chunk only, and must not compare them with a freed one.
	 */
	while (parser->chunks != outer) {
		chunk = parser->chunks;
		parser->chunks = chunk->next;
		LL_FOREACH(parser->stack, st)
		{
			if (st->chunk == chunk) {
				st->chunk = outer;
			}
		}
		ucl_chunk_free(chunk);
		parser->recursion--;
	}

	/*
	 * Resume the outer chunk only on the frame it stopped in. The inserted
	 * chunk can close that frame (a keyless [2] closes the array it was
	 * inserted into, as its [ opens no container) or leave new frames open,
	 * and then the saved state belongs to another frame: UCL_STATE_VALUE on
	 * an object frame hands the stale parser->cur_obj to the next value.
	 * Keep the state the inserted chunk ended in instead, an error included.
	 * The type check guards against a new frame object reusing the address of
	 * a frame object that a rewrite has freed.
	 */
	LL_COUNT(parser->stack, st, new_depth);
	if (new_depth == depth && parser->stack->obj == frame &&
		parser->stack->obj->type == frame_type) {
		parser->state = state;
	}
	else if (parser->state == UCL_STATE_AFTER_VALUE &&
			 state != UCL_STATE_AFTER_VALUE && parser->stack != NULL) {
		/*
		 * The outer chunk has its separator already (a macro value takes
		 * the one after it), so the end of the inserted chunk separates its
		 * last value from what follows, as on the same frame, where the
		 * restored state expects no separator either
		 */
		if (parser->stack->obj->type == UCL_OBJECT) {
			parser->state = UCL_STATE_KEY;
		}
		else {
			parser->state = UCL_STATE_VALUE;
		}
	}

	return res;
}

bool ucl_parser_add_string_priority(struct ucl_parser *parser, const char *data,
									size_t len, unsigned priority)
{
	if (data == NULL) {
		ucl_create_err(&parser->err, "invalid string added");
		return false;
	}
	if (len == 0) {
		len = strlen(data);
	}

	return ucl_parser_add_chunk_priority(parser,
										 (const unsigned char *) data, len, priority);
}

bool ucl_parser_add_string(struct ucl_parser *parser, const char *data,
						   size_t len)
{
	if (parser == NULL) {
		return false;
	}

	return ucl_parser_add_string_priority(parser,
										  (const unsigned char *) data, len, parser->default_priority);
}

bool ucl_set_include_path(struct ucl_parser *parser, ucl_object_t *paths)
{
	if (parser == NULL || paths == NULL || paths->type != UCL_ARRAY) {
		return false;
	}

	if (parser->includepaths == NULL) {
		parser->includepaths = ucl_object_copy(paths);
	}
	else {
		ucl_object_unref(parser->includepaths);
		parser->includepaths = ucl_object_copy(paths);
	}

	if (parser->includepaths == NULL) {
		return false;
	}

	return true;
}

unsigned char ucl_parser_chunk_peek(struct ucl_parser *parser)
{
	if (parser == NULL || parser->chunks == NULL || parser->chunks->pos == NULL || parser->chunks->end == NULL ||
		parser->chunks->pos == parser->chunks->end) {
		return 0;
	}

	return (*parser->chunks->pos);
}

bool ucl_parser_chunk_skip(struct ucl_parser *parser)
{
	if (parser == NULL || parser->chunks == NULL || parser->chunks->pos == NULL || parser->chunks->end == NULL ||
		parser->chunks->pos == parser->chunks->end) {
		return false;
	}

	const unsigned char *p = parser->chunks->pos;
	ucl_chunk_skipc(parser->chunks, p);
	if (parser->chunks->pos != NULL) return true;
	return false;
}

ucl_object_t *
ucl_parser_get_current_stack_object(struct ucl_parser *parser, unsigned int depth)
{
	ucl_object_t *obj;

	if (parser == NULL || parser->stack == NULL) {
		return NULL;
	}

	struct ucl_stack *stack = parser->stack;
	if (stack == NULL || stack->obj == NULL || ucl_object_type(stack->obj) != UCL_OBJECT) {
		return NULL;
	}

	for (unsigned int i = 0; i < depth; ++i) {
		/* The frame of a chunk's { holds the object of the frame below it */
		while (stack->next != NULL &&
			   (stack->e.params.flags & UCL_STACK_AUTOMATIC)) {
			stack = stack->next;
		}
		stack = stack->next;
		if (stack == NULL || stack->obj == NULL || ucl_object_type(stack->obj) != UCL_OBJECT) {
			return NULL;
		}
	}

	obj = ucl_object_ref(stack->obj);
	return obj;
}
