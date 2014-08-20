/*
 * This file is part of Hubbub.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2007 John-Mark Bell <jmb@netsurf-browser.org>
 */

#ifndef hubbub_utils_h_
#define hubbub_utils_h_

#ifdef BUILD_TARGET_riscos
  /* If we're building with Norcroft, then we need to haul in 
   * unixlib.h from TCPIPLibs for useful things like strncasecmp
   */
  #ifdef __CC_NORCROFT
  #include <unixlib.h>
  #endif
#endif

#ifdef BUILD_TARGET_windows
  #define strncasecmp _strnicmp
#endif

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

/* Useful for iterating over arrays */
#define N_ELEMENTS(x)   sizeof((x)) / sizeof((x)[0])

#endif
