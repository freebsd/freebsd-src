/*
 * This file is part of Hubbub.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2007 John-Mark Bell <jmb@netsurf-browser.org>
 */

#ifndef hubbub_errors_h_
#define hubbub_errors_h_

#ifdef __cplusplus
extern "C"
{
#endif

#include <stddef.h>

typedef enum hubbub_error {
	HUBBUB_OK               = 0, /**< No error */
	HUBBUB_REPROCESS	= 1,
	HUBBUB_ENCODINGCHANGE	= 2,
	HUBBUB_PAUSED		= 3, /**< tokenisation is paused */

	HUBBUB_NOMEM            = 5,
	HUBBUB_BADPARM          = 6,
	HUBBUB_INVALID          = 7,
	HUBBUB_FILENOTFOUND     = 8,
	HUBBUB_NEEDDATA         = 9,
	HUBBUB_BADENCODING      = 10,

	HUBBUB_UNKNOWN		= 11
} hubbub_error;

/* Convert a hubbub error value to a string */
const char *hubbub_error_to_string(hubbub_error error);

#ifdef __cplusplus
}
#endif

#endif

