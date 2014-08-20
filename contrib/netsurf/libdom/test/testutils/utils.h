/*
 * This file is part of libdom test suite.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2007 John-Mark Bell <jmb@netsurf-browser.org>
 */

#ifndef utils_h_
#define utils_h_

#include <stddef.h>
#include <inttypes.h>

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
#define UNUSED(x) ((x) = (x))
#endif

void *myrealloc(void *ptr, size_t len, void *pw);
void mymsg(uint32_t severity, void *ctx, const char *msg, ...); 

char *domts_strndup(const char *s, size_t len);

#endif

