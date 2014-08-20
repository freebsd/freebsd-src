/*
 * This file is part of LibParserUtils.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2007 John-Mark Bell <jmb@netsurf-browser.org>
 */

#ifndef parserutils_charset_aliases_h_
#define parserutils_charset_aliases_h_

#include <inttypes.h>

#include <parserutils/charset/mibenum.h>

typedef struct parserutils_charset_aliases_canon {
	/* Do not change the ordering here without changing make-aliases.pl */
	uint16_t mib_enum;
	uint16_t name_len;
	const char *name;
} parserutils_charset_aliases_canon;

/* Canonicalise an alias name */
parserutils_charset_aliases_canon *parserutils__charset_alias_canonicalise(
		const char *alias, size_t len);

#endif
