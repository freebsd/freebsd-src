/*
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
