/* $Id: mktemp.h,v 1.2 2001/02/09 01:55:36 djm Exp $ */

#ifndef _BSD_MKTEMP_H
#define _BSD_MKTEMP_H

#include "config.h"
#ifndef HAVE_MKDTEMP
int mkstemps(char *path, int slen);
int mkstemp(char *path);
char *mkdtemp(char *path);
#endif /* !HAVE_MKDTEMP */

#endif /* _BSD_MKTEMP_H */
