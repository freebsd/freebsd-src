/* $Id: strlcat.h,v 1.2 2001/02/09 01:55:36 djm Exp $ */

#ifndef _BSD_STRLCAT_H
#define _BSD_STRLCAT_H

#include "config.h"
#ifndef HAVE_STRLCAT
#include <sys/types.h>
size_t strlcat(char *dst, const char *src, size_t siz);
#endif /* !HAVE_STRLCAT */

#endif /* _BSD_STRLCAT_H */
