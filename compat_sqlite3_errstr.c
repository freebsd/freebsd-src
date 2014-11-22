#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_SQLITE3_ERRSTR

int dummy;

#else

const char *
sqlite3_errstr(int rc)
{

	return(rc ? "unknown error" : "not an error");
}

#endif
