/*
 * $Id: pam_radius.h,v 1.2 2000/11/19 23:54:05 agmorgan Exp $
 */

#ifndef PAM_RADIUS_H
#define PAM_RADIUS_H

#include <security/_pam_aconf.h>

#include <stdio.h>

#ifndef __USE_POSIX2
#define __USE_POSIX2
#endif /* __USE_POSIX2 */

#include <stdlib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/resource.h>

#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <syslog.h>
#include <stdarg.h>
#include <utmp.h>
#include <time.h>
#include <netdb.h>

#include <netinet/in.h>
#include <rpcsvc/ypclnt.h>
#include <rpc/rpc.h>

#include <pwdb/radius.h>
#include <pwdb/pwdb_radius.h>

/******************************************************************/

#endif /* PAM_RADIUS_H */
