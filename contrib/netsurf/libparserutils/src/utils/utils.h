/*
 * This file is part of LibParserUtils.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2007 John-Mark Bell <jmb@netsurf-browser.org>
 */

#ifndef parserutils_utils_h_
#define parserutils_utils_h_

#ifndef max
#define max(a,b) ((a)>(b)?(a):(b))
#endif

#ifndef min
#define min(a,b) ((a)<(b)?(a):(b))
#endif

#ifndef SLEN
/* Calculate length of a string constant */
#define SLEN(s) (sizeof((s)) - 1) /* -1 for '\0' */
#endif

#ifndef UNUSED
#define UNUSED(x) ((x)=(x))
#endif

#ifndef N_ELEMENTS
#define N_ELEMENTS(s) (sizeof((s)) / sizeof((s)[0]))
#endif

#ifndef ALIGN
#define ALIGN(val) (((val) + 3) & ~(3))
#endif

#endif
