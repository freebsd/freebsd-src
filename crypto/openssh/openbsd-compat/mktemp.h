/* $Id: mktemp.h,v 1.3 2003/01/07 04:18:33 djm Exp $ */

#ifndef _BSD_MKTEMP_H
#define _BSD_MKTEMP_H

#include "config.h"
#if !defined(HAVE_MKDTEMP) || defined(HAVE_STRICT_MKSTEMP)
int mkstemps(char *path, int slen);
int mkstemp(char *path);
char *mkdtemp(char *path);
#endif /* !defined(HAVE_MKDTEMP) || defined(HAVE_STRICT_MKSTEMP) */

#endif /* _BSD_MKTEMP_H */
