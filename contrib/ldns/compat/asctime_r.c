#ifdef HAVE_CONFIG_H
#include <ldns/config.h>
#endif

#ifdef HAVE_TIME_H
#include <time.h>
#endif

char *asctime_r(const struct tm *tm, char *buf)
{
	/* no thread safety. */
	char* result = asctime(tm);
	if(buf && result)
		strcpy(buf, result);
	return result;
}
