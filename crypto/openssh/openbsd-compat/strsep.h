/* $Id: strsep.h,v 1.2 2001/02/09 01:55:36 djm Exp $ */

#ifndef _BSD_STRSEP_H
#define _BSD_STRSEP_H

#include "config.h"

#ifndef HAVE_STRSEP
char *strsep(char **stringp, const char *delim);
#endif /* HAVE_STRSEP */

#endif /* _BSD_STRSEP_H */
