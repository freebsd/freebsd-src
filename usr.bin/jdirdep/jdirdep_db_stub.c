/* $FreeBSD$ */

#include <sys/types.h>
#include <stdlib.h>
#include "jdirdep.h"

void
jdirdep_db_close(void)
{
}

void *
jdirdep_db_command_res(const char *fmt __unused, ...)
{
	return (NULL);
}

void
jdirdep_db_command(db_cb_func func __unused, void *vp __unused, const char *fmt __unused, ...)
{
}

void
jdirdep_db_open(const char *name __unused)
{
}

int64_t
jdirdep_db_rowid(void)
{
	return (0);
}
