/* $Id: daemon.h,v 1.2 2001/02/09 01:55:36 djm Exp $ */

#ifndef _BSD_DAEMON_H
#define _BSD_DAEMON_H

#include "config.h"
#ifndef HAVE_DAEMON
int daemon(int nochdir, int noclose);
#endif /* !HAVE_DAEMON */

#endif /* _BSD_DAEMON_H */
