/* $Header: /home/ncvs/src/gnu/usr.bin/patch/version.c,v 1.1.1.1 1993/06/19 14:21:52 paul Exp $
 *
 * $Log: version.c,v $
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
