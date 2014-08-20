
/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2009 Bo Yang <struggleyb.nku@gmail.com>
 */

#ifndef dom_internal_core_string_h_
#define dom_internal_core_string_h_

#include <dom/core/string.h>

/* Map the lwc_error to dom_exception */
dom_exception _dom_exception_from_lwc_error(lwc_error err);

enum dom_whitespace_op {
	DOM_WHITESPACE_STRIP_LEADING	= (1 << 0),
	DOM_WHITESPACE_STRIP_TRAILING	= (1 << 1),
	DOM_WHITESPACE_STRIP		= DOM_WHITESPACE_STRIP_LEADING |
					  DOM_WHITESPACE_STRIP_TRAILING,
	DOM_WHITESPACE_COLLAPSE		= (1 << 2),
	DOM_WHITESPACE_STRIP_COLLAPSE	= DOM_WHITESPACE_STRIP |
					  DOM_WHITESPACE_COLLAPSE
};

/** Perform whitespace operations on given string
 *
 * \param s	Given string
 * \param op	Whitespace operation(s) to perform
 * \param ret	New string with whitespace ops performed.  Caller owns ref
 *
 * \return DOM_NO_ERR on success.
 *
 * \note Right now, will return DOM_NOT_SUPPORTED_ERR if ascii_only is false.
 */
dom_exception dom_string_whitespace_op(dom_string *s,
		enum dom_whitespace_op op, dom_string **ret);

#endif

