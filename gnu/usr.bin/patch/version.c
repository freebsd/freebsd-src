/* $FreeBSD: src/gnu/usr.bin/patch/version.c,v 1.7.36.1.8.1 2012/03/03 06:15:13 kensmith Exp $
 *
 * $Log: version.c,v $
 * Revision 2.0  86/09/17  15:40:11  lwall
 * Baseline for netwide release.
 *
 */

#include "EXTERN.h"
#include "common.h"
#include "util.h"
#include "INTERN.h"
#include "patchlevel.h"
#include "version.h"

void	my_exit(int _status);		/* in patch.c */

/* Print out the version number and die. */

void
version(void)
{
    fprintf(stderr, "Patch version %s\n", PATCH_VERSION);
    my_exit(0);
}
