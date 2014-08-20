/*
 * Copyright 2003 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2004 John Tytgat <John.Tytgat@aaug.net>
 *
 * This file is part of NetSurf, http://www.netsurf-browser.org/
 * Licenced under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 */

#include <stdio.h>

#ifndef _LIBNSBMP_LOG_H_
#define _LIBNSBMP_LOG_H_

#ifdef NDEBUG
#  define LOG(x) ((void) 0)
#else
#  ifdef __GNUC__
#    define LOG(x) do { printf x, fputc('\n', stdout)); } while (0)
#  elif defined(__CC_NORCROFT)
#    define LOG(x) do { printf x, fputc('\n', stdout)); } while (0)
#  else
#    define LOG(x) do { printf x, fputc('\n', stdout)); } while (0)
#  endif
#endif

#endif
