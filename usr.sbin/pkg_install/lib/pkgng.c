/*-
 * Copyright (c) 2012 Eitan Adler
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <sys/cdefs.h>

#include "lib.h"
#include <err.h>

static const char message[] = "You appear to be using the newer pkg(1) tool on \
this system for package management, rather than the legacy package \
management tools (pkg_*).  The legacy tools should no longer be used on \
this system.";

void warnpkgng(void)
{
	char pkgngpath[MAXPATHLEN + 1];
	char *pkgngdir;
	char *dontwarn;
	int rc;

	dontwarn = getenv("PKG_OLD_NOWARN");
	if (dontwarn != NULL)
		return;
	pkgngdir = getenv("PKG_DBDIR");
	if (pkgngdir == NULL)
		pkgngdir = "/var/db/pkg";

	rc = snprintf(pkgngpath, sizeof(pkgngpath), "%s/local.sqlite", pkgngdir);
	if ((size_t)rc >= sizeof(pkgngpath)) {
		warnx("path too long: %s/local.sqlite", pkgngdir);
		return;
	}

	if (access(pkgngpath, F_OK) == 0)
		warnx(message);
}
