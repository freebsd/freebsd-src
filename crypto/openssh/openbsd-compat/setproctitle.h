/* $Id: setproctitle.h,v 1.3 2003/01/09 22:53:13 djm Exp $ */

#ifndef _BSD_SETPROCTITLE_H
#define _BSD_SETPROCTITLE_H

#include "config.h"

#ifndef HAVE_SETPROCTITLE
void setproctitle(const char *fmt, ...);
void compat_init_setproctitle(int argc, char *argv[]);
#endif

#endif /* _BSD_SETPROCTITLE_H */
