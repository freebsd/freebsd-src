/*
 * Do all necessary includes here, so that we don't have to worry about
 * overlapping includes in the files in missing.d.
 */
#include "awk.h"


#ifdef atarist
/*
 * this will work with gcc compiler - for other compilers you may
 * have to replace path separators in this file into backslashes
 */
#include "atari/stack.c"
#include "atari/tmpnam.c"
#endif /* atarist */

#ifndef HAVE_SYSTEM
#ifdef atarist
#include "atari/system.c"
#else
#include "missing/system.c"
#endif
#endif /* HAVE_SYSTEM */

#ifndef HAVE_MEMCMP
#include "missing/memcmp.c"
#endif	/* HAVE_MEMCMP */

#ifndef HAVE_MEMCPY
#include "missing/memcpy.c"
#endif	/* HAVE_MEMCPY */

#ifndef HAVE_MEMSET
#include "missing/memset.c"
#endif	/* HAVE_MEMSET */

#ifndef HAVE_STRNCASECMP
#include "missing/strncasecmp.c"
#endif	/* HAVE_STRCASE */

#ifndef HAVE_STRERROR
#include "missing/strerror.c"
#endif	/* HAVE_STRERROR */

#ifndef HAVE_STRFTIME
#include "missing/strftime.c"
#endif	/* HAVE_STRFTIME */

#ifndef HAVE_STRCHR
#include "missing/strchr.c"
#endif	/* HAVE_STRCHR */

#ifndef HAVE_STRTOD
#include "missing/strtod.c"
#endif	/* HAVE_STRTOD */

#ifndef HAVE_TZSET
#include "missing/tzset.c"
#endif /* HAVE_TZSET */
