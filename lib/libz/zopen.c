/*
 * Public domain stdio wrapper for libz, written by Johan Danielsson.
 */

#ifndef lint
static const char rcsid[] = 
  "$FreeBSD$";
#endif /* not lint */

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

FILE *
zopen(const char *fname, const char *mode)
{
    gzFile gz = gzopen(fname, mode);
    if(gz == NULL)
	return NULL;

    if(*mode == 'r')
	return (funopen(gz, xgzread, NULL, NULL, gzclose));
    else
	return (funopen(gz, NULL, xgzwrite, NULL, gzclose));
}
