/*
 * FreeBSD install - a package for the installation and maintenance
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
 * Eitan Adler
 *
 * detect pkgng's existence and warn
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "lib.h"
#include <err.h>

void warnpkgng(void) {
	char pkgngpath[MAXPATHLEN];
	char *pkgngdir;

	pkgngdir = getenv("PKG_DBDIR");
	if (pkgngdir == NULL)
		pkgngdir = "/var/db/pkg";
	strcpy(pkgngpath, pkgngdir);
	strcat(pkgngpath, "/local.sqlite");

	if (access(pkgngpath, F_OK) == 0)
		warnx("Don't use the pkg_ tools if you are using pkgng");
}
