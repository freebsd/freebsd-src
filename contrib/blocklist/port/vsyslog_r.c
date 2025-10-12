#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <syslog.h>
#include <stdarg.h>

void
vsyslog_r(int priority, struct syslog_data *sd __unused, const char *fmt, va_list ap)
{
	vsyslog(priority, fmt, ap);
}

