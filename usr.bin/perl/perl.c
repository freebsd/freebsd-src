/*-
 * Copyright (c) 2002 Dag-Erling Coïdan Smørgrav
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/sysctl.h>

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pathnames.h"

extern char **environ;

int
main(int argc __unused, char *argv[])
{
	char path[PATH_MAX], *cp;
	const char *cmd, *p, *q, *self;
	size_t len;
	struct stat self_stat, perl_stat;

	self = argv[0];
	if (stat (self, &self_stat) != 0) {
		self_stat.st_dev = makedev (0, 0);
		self_stat.st_ino = 0;
	}
	if ((cmd = strrchr(self, '/')) == NULL)
		cmd = self;
	else
		cmd++;
	/* If null path (e. g. in mailfilter scripts), use default path. */
	if ((p = getenv("PATH")) == NULL) {
		if (sysctlbyname("user.cs_path", (void *)NULL, &len,
		    (void *)NULL, 0) == -1)
			err(1, "sysctlbyname(\"user.cs_path\")");
                if ((cp = malloc(len + 1)) == NULL)
			err(1, "malloc() failed");
		if (sysctlbyname("user.cs_path", cp, &len, (void *)NULL, 0) == -1)
			err(1, "sysctlbyname(\"user.cs_path\")");
		setenv("PATH", cp, 1);
	}
	/* If default package bindir not there, append it. */
	p = getenv("PATH");
	if (strstr(p, PATH_PKG_BINDIR) == NULL) {
		snprintf(path, sizeof path, "%s:%s", p, PATH_PKG_BINDIR);
		setenv("PATH", path, 1);
	}
	argv[0] = path;
	for (p = q = getenv("PATH"); p && *p && *q; p = q + 1) {
		for (q = p; *q && *q != ':'; ++q)
			/* nothing */ ;
		len = snprintf(path, sizeof path, "%.*s/%s", (int)(q - p), p, cmd);
		if (len >= PATH_MAX || strcmp(path, self) == 0)
			continue;
		if (stat (path, &perl_stat) == 0
		    && self_stat.st_dev == perl_stat.st_dev
		    && self_stat.st_ino == perl_stat.st_ino)
			continue;
		execve(path, argv, environ);
		if (errno != ENOENT)
			err(1, "%s", path);
	}
	errx(1, "Perl is not installed, try 'pkg_add -r perl'");
}
