/* ntp_syslog.h,v 3.1 1993/07/06 01:06:59 jbj Exp
 * A hack for platforms which require specially built syslog facilities
 */
#ifdef GIZMO
#include "gizmo_syslog.h"
#else /* !GIZMO */
#include <syslog.h>
#ifdef SYSLOG_FILE
#include <stdio.h>
#endif
#endif /* GIZMO */
#ifdef SYSLOG_FILE
extern FILE *syslog_file;
#define syslog msyslog
#endif
