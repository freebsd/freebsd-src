/* $Id: basename.h,v 1.3 2003/02/25 03:32:16 djm Exp $ */

#ifndef _BASENAME_H 
#define _BASENAME_H
#include "config.h"

#if !defined(HAVE_BASENAME)

char *basename(const char *path);

#endif /* !defined(HAVE_BASENAME) */
#endif /* _BASENAME_H */
