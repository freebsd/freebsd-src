/*
 * Copyright (c) 2013-2026 Devin Teske <dteske@FreeBSD.org>
 * Copyright (c) 2021-2026 Faraz Vahedi <kfv@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "bsdconf_formats.h"

/*
 * BSDCONF_FORMAT_GENERIC: `name[=value]' assignments with values quoted
 * only when required to round-trip (embedded whitespace or a comment
 * character). The fallback when nothing more specific is known about a
 * file, and the default base for derived formats. It has a target keyword
 * so that a sysconf(8)-style consumer can apply it to an arbitrary file
 * explicitly, but no default path; the caller must always name the file.
 *
 * NB: Formats whose statements are a space-separated directive and value
 * with no equals sign at all (the httpd.conf style the ancestral figpar
 * engine was raised on) need no descriptor of their own to parse: with
 * BSDCONF_BREAK_ON_EQUALS omitted, the first whitespace-delimited token is
 * the directive and the remainder of the line is the raw value, and
 * directive matching is case insensitive unless BSDCONF_CASE_SENSITIVE is
 * set.
 */
const struct bsdconf_format_def bsdconf_format_generic_def = {
	.keyword	= "generic",
	.path		= NULL,		/* no default path */
	.sources	= NULL,		/* no source list */
	.processing	= BSDCONF_BREAK_ON_EQUALS | BSDCONF_CASE_SENSITIVE,
	.put		= 0,
};
