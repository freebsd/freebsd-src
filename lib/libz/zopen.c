/*
 * Public domain stdio wrapper for libz, written by Johan Danielsson.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: head/lib/libz/zopen.c 84228 2001-09-30 22:39:00Z dillon $");

#include <stdio.h>
#include <zlib.h>

FILE *zopen(const char *fname, const char *mode);

/* convert arguments */
static int
xgzread(void *cookie, char *data, int size)
{
    return gzread(cookie, data, size);
}

static int
xgzwrite(void *cookie, const char *data, int size)
{
    return gzwrite(cookie, (void*)data, size);
}

static int
xgzclose(void *cookie)
{
    return gzclose(cookie);
}

FILE *
zopen(const char *fname, const char *mode)
{
    gzFile gz = gzopen(fname, mode);
    if(gz == NULL)
	return NULL;

    if(*mode == 'r')
	return (funopen(gz, xgzread, NULL, NULL, xgzclose));
    else
	return (funopen(gz, NULL, xgzwrite, NULL, xgzclose));
}
