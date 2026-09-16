/*
 * Copyright (c) 2013-2026 Devin Teske <dteske@FreeBSD.org>
 * Copyright (c) 2021-2026 Faraz Vahedi <kfv@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "bsdconf_formats.h"

/*
 * BSDCONF_FORMAT_MAKE: make.conf(5)
 *
 * make(1) syntax: assignment modifiers (`+=' `?=' `:=' `!=') are
 * recognized, matched, and rewritten (BSDCONF_OPERATOR_EQUALS), and values
 * are never quoted (BSDCONF_PUT_UNQUOTED) -- a value runs to the end of
 * the line, embedded whitespace included, so only a value that could not
 * round-trip verbatim (an embedded newline or comment marker) is rejected.
 * A put of `+=' appends a new `name+=value' line rather than rewriting a
 * prior assignment (see bsdconf_put()); callers that accumulate effective
 * values honor the same operators on read. sysconf(8) `name-=word' strikes
 * rewrite or remove a specific prior statement via match_line (make has no
 * `-=` operator of its own).
 */
const struct bsdconf_format_def bsdconf_format_make_def = {
	.keyword	= "make",
	.path		= "/etc/make.conf",
	.sources	= NULL, /* /etc/make.conf alone */
	.processing	= BSDCONF_BREAK_ON_EQUALS | BSDCONF_CASE_SENSITIVE |
			  BSDCONF_OPERATOR_EQUALS,
	.put		= BSDCONF_PUT_UNQUOTED | BSDCONF_PUT_ALLOW_EMPTY,
};
