/*
 * FreeBSD install - a package for the installation and maintainance
 * of non-core utilities.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * Maxim Sobolev
 * 31 July 2001
 *
 * Routines to assist with PLIST_FMT_VER numbers in the packing
 * lists.
 *
 * Following is the PLIST_FMT_VER history:
 * 1.0 - Initial revision;
 * 1.1 - When recording/checking checksum of symlink use hash of readlink()
 *	 value insted of the hash of an object this links points to.
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "lib.h"
#include <err.h>

int
verscmp(Package *pkg, int major, int minor)
{
    int rval = 0;

    if ((pkg->fmtver_maj < major) || (pkg->fmtver_maj == major &&
	pkg->fmtver_mnr < minor))
	rval = -1;
    else if ((pkg->fmtver_maj > major) || (pkg->fmtver_maj == major &&
	     pkg->fmtver_mnr > minor))
	rval = 1;

    return rval;
}
