/*
 * Do all necessary includes here, so that we don't have to worry about
 * overlapping includes in the files in missing.d.
 */
#include "config.h"
#include "awk.h"


#ifdef atarist
/*
 * this will work with gcc compiler - for other compilers you may
 * have to replace path separators in this file into backslashes
 */
#include "unsupported/atari/stack.c"
#include "unsupported/atari/tmpnam.c"
#endif /* atarist */

#ifndef HAVE_SYSTEM
#ifdef atarist
#include "unsupported/atari/system.c"
#else
#include "missing_d/system.c"
#endif
#endif /* HAVE_SYSTEM */

#ifndef HAVE_MEMCMP
#include "missing_d/memcmp.c"
#endif	/* HAVE_MEMCMP */

#ifndef HAVE_MEMCPY
#include "missing_d/memcpy.c"
#endif	/* HAVE_MEMCPY */

#ifndef HAVE_MEMSET
#include "missing_d/memset.c"
#endif	/* HAVE_MEMSET */

#ifndef HAVE_STRNCASECMP
#include "missing_d/strncasecmp.c"
#endif	/* HAVE_STRCASE */

#ifndef HAVE_STRERROR
#include "missing_d/strerror.c"
#endif	/* HAVE_STRERROR */

#ifndef HAVE_STRFTIME
#include "missing_d/strftime.c"
#endif	/* HAVE_STRFTIME */

#ifndef HAVE_STRCHR
#include "missing_d/strchr.c"
#endif	/* HAVE_STRCHR */

#if !defined(HAVE_STRTOD) || defined(STRTOD_NOT_C89)
#include "missing_d/strtod.c"
#endif	/* HAVE_STRTOD */

#ifndef HAVE_TZSET
#include "missing_d/tzset.c"
#endif /* HAVE_TZSET */

#ifndef HAVE_MKTIME
#include "missing_d/mktime.c"
#endif /* HAVE_MKTIME */

#if defined TANDEM
#include "strdupc"
#include "getidc"
#include "strnchkc"
#endif /* TANDEM */
