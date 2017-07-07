/*
 * Copyright 1988 by the Massachusetts Institute of Technology.
 *
 * For copying and distribution information, please see the file
 * <mit-copyright.h>.
 *
 * Configuration info for operating system, hardware description,
 * language implementation, C library, etc.
 *
 * This file should be included in (almost) every file in the Kerberos
 * sources, and probably should *not* be needed outside of those
 * sources.  (How do we deal with /usr/include/des.h and
 * /usr/include/krb.h?)
 */

#ifndef _CONF_H_
#define _CONF_H_

#include "osconf.h"

#ifdef SHORTNAMES
#include "names.h"
#endif

/*
 * Language implementation-specific definitions
 */

/* special cases */
#ifdef __HIGHC__
/* broken implementation of ANSI C */
#undef __STDC__
#endif

#if !defined(__STDC__) && !defined(PC)
#define const
#define volatile
#define signed
typedef char *pointer;          /* pointer to generic data */
#ifndef PROTOTYPE
#define PROTOTYPE(p) ()
#endif
#else
typedef void *pointer;
#ifndef PROTOTYPE
#define PROTOTYPE(p) p
#endif
#endif

/* Does your compiler understand "void"? */
#ifdef notdef
#define void int
#endif

/*
 * A few checks to see that necessary definitions are included.
 */

#ifndef MSBFIRST
#ifndef LSBFIRST
#error byte order not defined
#endif
#endif

/* machine size */
#ifndef BITS16
#ifndef BITS32
#error number of bits?
#endif
#endif

/* end of checks */

#endif /* _CONF_H_ */
