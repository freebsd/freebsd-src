/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Donn Seeley at Berkeley Software Design, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/mount.h>
#include <sys/reboot.h>
#include <sys/sysctl.h>

#include <err.h>
#include <kenv.h>
#include <paths.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

static void emergency(const char *, ...) __printflike(1, 2);

static void
emergency(const char *message, ...)
{
	va_list ap;
	va_start(ap, message);

	vsyslog(LOG_EMERG, message, ap);
	va_end(ap);
}

int
main(void)
{
	char init_path[PATH_MAX], *path, *path_component;
	size_t init_path_len;
	int nbytes, error;

	openlog("init", LOG_CONS, LOG_AUTH);

	/*
	 * Ask the kernel to mount the new rootfs.
	 */
	error = reboot(RB_REROOT);
	if (error != 0) {
		emergency("RB_REBOOT failed: %m");
		_exit(1);
	}

	/*
	 * Figure out where the destination init(8) binary is.  Note that
	 * the path could be different than what we've started with.  Use
	 * the value from kenv, if set, or the one from sysctl otherwise.
	 * The latter defaults to a hardcoded value, but can be overridden
	 * by a build time option.
	 */
	nbytes = kenv(KENV_GET, "init_path", init_path, sizeof(init_path));
	if (nbytes <= 0) {
		init_path_len = sizeof(init_path);
		error = sysctlbyname("kern.init_path",
		    init_path, &init_path_len, NULL, 0);
		if (error != 0) {
			emergency("failed to retrieve kern.init_path: %m");
			_exit(1);
		}
	}

	/*
	 * Repeat the init search logic from sys/kern/init_path.c
	 */
	path_component = init_path;
	while ((path = strsep(&path_component, ":")) != NULL) {
		/*
		 * Execute init(8) from the new rootfs.
		 */
		execl(path, path, NULL);
	}
	emergency("cannot exec init from %s: %m", init_path);
	_exit(1);
}
