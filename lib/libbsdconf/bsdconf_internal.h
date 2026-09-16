/*
 * Copyright (c) 2001-2026 Devin Teske <dteske@FreeBSD.org>
 * Copyright (c) 2021-2026 Faraz Vahedi <kfv@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef _BSDCONF_INTERNAL_H_
#define _BSDCONF_INTERNAL_H_

#include <sys/stat.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "bsdconf.h"

/*
 * Private interfaces shared between the libbsdconf translation units.
 * This header is deliberately not installed.
 */

/* String manipulation routines (bsdconf_string.c) */
void		bsdconf_strunexpand(char *_dst, const char *_src);
void		bsdconf_strtolower(char *_buf);
int		bsdconf_replaceall(char *_buf, size_t _buflen,
		    const char *_find, const char *_replace);
unsigned int	bsdconf_strcount(const char *_source, const char *_find);

/*
 * Parsed extent of a single statement within the in-memory copy of the file.
 * All members are byte offsets into the buffer except as noted. A statement
 * runs from the first byte of its directive to its terminator (newline, an
 * inline `#' comment, or a semicolon when enabled).
 */
struct bsdconf_stmt {
	size_t line_start; /* start of the physical line (for removal) */
	size_t dir_start;  /* first byte of the directive */
	size_t dir_end;    /* one past the last byte of the directive */
	size_t op_start;   /* first byte of the assignment operator */
	size_t eq;         /* index of the `=' (valid iff have_equals) */
	size_t val_start;  /* first byte of the value (== val_end if none) */
	size_t val_end;    /* one past last byte of value (ws trimmed) */
	size_t term;       /* index of the terminator (`\n', `#', `;', EOF) */
	size_t line_end;   /* one past the terminating newline (for removal) */
	uint32_t line;     /* 1-based line number of the directive */
	enum bsdconf_op op; /* assignment operator of the statement */
	bool have_value;   /* a value was present */
	bool have_equals;  /* an `=' separated directive/value */
};

/* Statement scan / put helpers (bsdconf_stmt.c) */
const char	*bsdconf_op_token(enum bsdconf_op _op);
int		 bsdconf_writeall(int _fd, const void *_data, size_t _len);
char		*bsdconf_readfile(int _fd, size_t _size, size_t *_lenp);
int		 bsdconf_emit(int _fd, const void *_data, size_t _len,
		    int *_last);
int		 bsdconf_ensure_tmp(int *_tmpfdp, char *_tpath, size_t _tpathsz,
		    const char *_rpath, const struct stat *_sb);
char		*bsdconf_format_value(const struct bsdconf_option *_option,
		    bool _unquoted, bool _quote_always, bool _bsemicolon);
int		 bsdconf_value_empty(const struct bsdconf_option *_option);
int		 bsdconf_dir_matches(const char *_buf, size_t _start,
		    size_t _end, const char *_directive, bool _case_sensitive);
int		 bsdconf_scan(const char *_buf, size_t _len, size_t *_ip,
		    uint32_t *_linep, bool _bequals, bool _bsemicolon,
		    bool _strict_equals, bool _operator_equals,
		    struct bsdconf_stmt *_st);

#endif /* !_BSDCONF_INTERNAL_H_ */
