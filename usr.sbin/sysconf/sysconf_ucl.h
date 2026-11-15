/*
 * Copyright (c) 2013-2026 Devin Teske <dteske@FreeBSD.org>
 * Copyright (c) 2021-2026 Faraz Vahedi <kfv@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * Shared by the ucl target's files (sysconf_ucl*.c). Not for the rest
 * of sysconf. Include after sysconf_priv.h.
 */

#ifndef _SYSCONF_UCL_H_
#define _SYSCONF_UCL_H_

#include <stdbool.h>
#include <stddef.h>

#include <ucl.h>

#define UCL_PARSE_FLAGS	(UCL_PARSER_DISABLE_MACRO | UCL_PARSER_NO_FILEVARS)

/* A flattened tree: dotted path -> rendering, last definition wins. */
struct flat {
	char	**names;
	char	**values;
	size_t	n;
};

/* One statement located in the text */
struct ustmt {
	size_t	start;		/* first byte of the key */
	size_t	end;		/* past the value and its terminator */
	size_t	val_start;	/* value text (after the separator) */
	size_t	val_end;	/* past the value text */
	size_t	line_start;	/* start of the line holding the key */
	size_t	line_end;	/* past the newline ending the statement */
	size_t	obj_open;	/* position of `{' when is_obj */
	char	sep;		/* ':' or '=' or 0 (bare `key value') */
	char	term;		/* ',' or ';' or 0 */
	bool	alone;		/* nothing else shares its lines */
	bool	is_obj;		/* value is `{ ... }' */
	bool	is_arr;		/* value is `[ ... ]' */
	bool	quoted;		/* value is a quoted string */
};

/* One container (the file, or the inside of `{ ... }') */
struct uobj {
	size_t	open;		/* position of `{' (SIZE_MAX for the file) */
	size_t	close;		/* position of `}' (or text length) */
	size_t	nstmts;		/* statements directly inside */
	struct ustmt first;	/* first statement (when nstmts > 0) */
	struct ustmt prev;	/* statement before last (nstmts > 1) */
	struct ustmt last;	/* last statement (when nstmts > 0) */
	struct ustmt scalar;	/* first non-object statement */
	bool	has_scalar;	/* `scalar' is valid */
	bool	blank_between;	/* a blank line separates statements */
};

/* Style a new statement should take */
struct ustyle {
	char	sep;		/* ':' '=' or 0 */
	char	term;		/* ',' ';' or 0 */
	char	obj_sep;	/* separator used before `{' */
	bool	last_bare;	/* members terminated except the last */
	bool	quote_keys;	/* keys are written "quoted" */
	char	indent[64];	/* one indentation unit */
};

/* Growable output buffer */
struct ubuf {
	char	*p;
	size_t	len;
	size_t	cap;
};

/* sysconf_ucl.c */
int		 ucl_walk(const ucl_object_t *_obj, const char *_prefix,
		    int (*_cb)(const char *, const char *, ucl_type_t,
		    void *), void *_arg);
ucl_object_t	*ucl_value(const char *_text);

/* sysconf_ucl_index.c */
long		 ucl_resolve_index(long _idx, unsigned int _size);
char		*ucl_index_label(const struct request *_req, size_t _depth);
void		 ucl_relabel(struct request *_req);
void		 ucl_split_index(const char *_base);

/* sysconf_ucl_list.c */
ucl_object_t	*ucl_list_apply(const char *_curtext, const char *_words,
		    int _op);

/* sysconf_ucl_file.c */
char		*slurp(const char *_path, size_t *_lenp);
char		*slurp_fd(int _fd, const char *_path, size_t *_lenp);
ucl_object_t	*ucl_parse_text(const char *_path, const char *_text,
		    size_t _len, const char **_errp);
void		 ucl_replace_file(const char *_path, const char *_text,
		    size_t _len);
void		 flat_set(struct flat *_f, const char *_name,
		    const char *_value);
void		 flat_remove(struct flat *_f, const char *_name);
int		 flat_cb(const char *_name, const char *_value,
		    ucl_type_t _type, void *_arg);
void		 flat_free(struct flat *_f);
bool		 flat_equal(const struct flat *_a, const struct flat *_b);
size_t		 ucl_write_target(const struct request *_req);
ucl_object_t	*ucl_reparse(ucl_object_t *_top, const char *_path,
		    const char *_text, size_t _len);

/* sysconf_ucl_lex.c */
size_t		 skip_ws(const char *_s, size_t _n, size_t _i, bool _newlines,
		    bool _seps);
size_t		 skip_quoted(const char *_s, size_t _n, size_t _i);
size_t		 skip_balanced(const char *_s, size_t _n, size_t _i);
bool		 comment_at(const char *_s, size_t _n, size_t _i);
bool		 key_char(char _c);
size_t		 line_begin(const char *_s, size_t _i);
bool		 blank_span(const char *_s, size_t _from, size_t _to);
bool		 tail_span(const char *_s, size_t _n, size_t _from,
		    size_t _to);
int		 parse_stmt(const char *_s, size_t _n, size_t _i,
		    struct ustmt *_st);
int		 enum_object(const char *_s, size_t _n, size_t _i,
		    const char *_want, struct ustmt *_match, struct uobj *_obj);

/* sysconf_ucl_style.c */
void		 detect_indent(const char *_s, size_t _n, char *_out,
		    size_t _outsize);
void		 detect_style(const char *_s, size_t _n, struct ustyle *_sty);
bool		 key_bare_safe_len(const char *_k, size_t _n);
char		*quote_value(const char *_v);
bool		 quoted_by_choice(const char *_oldtext, size_t _oldlen);
char		*render_value(const ucl_object_t *_val, const char *_raw,
		    const char *_oldtext, size_t _oldlen, bool _oldquoted);
void		 ubuf_add(struct ubuf *_b, const char *_s, size_t _n);
void		 ubuf_str(struct ubuf *_b, const char *_s);
char		*splice(const char *_s, size_t _n, size_t _at, size_t _from,
		    const char *_ins);
char		*splice_after(const char *_s, size_t _n, size_t _at, char _term,
		    const char *_ins);
void		 emit_new(struct ubuf *_b, char **_comps, int _ncomps,
		    const char *_vtext, const struct ustyle *_sty,
		    const char *_prefix, size_t _prefix_len, int _depth,
		    bool _oneline);

/* sysconf_ucl_array.c */
char		*array_edit(const char *_text, size_t _len, size_t _open,
		    size_t _anchor, const ucl_object_t *_want, char **_wantr,
		    size_t _nwant, const char *_indent, bool *_changed);

#endif /* !_SYSCONF_UCL_H_ */
