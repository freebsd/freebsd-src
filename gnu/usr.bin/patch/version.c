/* $FreeBSD$
 *
 * $Log: version.c,v $
 * Revision 1.4  1998/01/21 14:37:26  ache
 * Resurrect patch 2.1 without FreeBSD Index: hack
 *
 * Revision 1.2  1995/05/30 05:02:39  rgrimes
 * Remove trailing whitespace.
 *
 * Revision 1.1.1.1  1993/06/19  14:21:52  paul
 * b-maked patch-2.10
 *
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

void my_exit();

/* Print out the version number and die. */

void
version()
{
    fprintf(stderr, "Patch version %s\n", PATCH_VERSION);
    my_exit(0);
}
