/*
 * Copyright 1988 by the Massachusetts Institute of Technology.
 * For copying and distribution information, please see the file
 * <Copyright.MIT>.
 *
 * Configuration info for operating system, hardware description,
 * language implementation, C library, etc.
 *
 * This file should be included in (almost) every file in the Kerberos
 * sources, and probably should *not* be needed outside of those
 * sources.  (How do we deal with /usr/include/des.h and
 * /usr/include/krb.h?)
 *
 *	from: conf.h,v 4.0 89/01/23 09:58:40 jtkohl Exp $
 *	$FreeBSD$
 */

#ifndef _CONF_H_

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

#ifndef __STDC__
#define const
#define volatile
#define signed
typedef char *pointer;		/* pointer to generic data */
#define PROTOTYPE(p) ()
#else
typedef void *pointer;
#define PROTOTYPE(p) p
#endif

/* Does your compiler understand "void"? */
#ifdef notdef
#define void int
#endif

/*
 * A few checks to see that necessary definitions are included.
 */

/* byte order */

#ifndef MSBFIRST
#ifndef LSBFIRST
Error: byte order not defined.
#endif
#endif

/* machine size */
#ifndef BITS16
#ifndef BITS32
Error: how big is this machine anyways?
#endif
#endif

/* end of checks */

#endif /* _CONF_H_ */
