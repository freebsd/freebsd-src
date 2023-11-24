/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019-2023, Juniper Networks, Inc.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/errno.h>
#include <sys/mac.h>

#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <syslog.h>

#include <security/mac_grantbylabel/mac_grantbylabel.h>

#include "libveriexec.h"

static char *
find_interpreter(const char *script)
{
	static const char ws[] = " \t\n\r";
	static char buf[MAXPATHLEN+4];	/* allow space for #! etc */
	char *cp;
	int fd;
	int n;

	cp = NULL;
	if ((fd = open(script, O_RDONLY)) >= 0) {
		if ((n = read(fd, buf, sizeof(buf))) > 0) {
			if (strncmp(buf, "#!", 2) == 0) {
				buf[sizeof(buf) - 1] = '\0';
				cp = &buf[2];
				if ((n = strspn(cp, ws)) > 0)
					cp += n;
				if ((n = strcspn(cp, ws)) > 0) {
					cp[n] = '\0';
				} else {
					cp = NULL;
				}
			}
		}
		close(fd);
	}
	return (cp);
}

/**
 * @brief exec a python or similar script
 *
 * Python and similar scripts must normally be signed and
 * run directly rather than fed to the interpreter which
 * is not normally allowed to be run directly.
 *
 * If direct execv of script fails due to EAUTH
 * and process has GBL_VERIEXEC syslog event and run via
 * interpreter.
 *
 * If interpreter is NULL look at first block of script
 * to find ``#!`` magic.
 *
 * @prarm[in] interpreter
 *	if NULL, extract from script if necessary
 *
 * @prarm[in] argv
 *	argv for execv(2)
 *	argv[0] must be full path.
 *	Python at least requires argv[1] to also be the script path.
 *
 * @return
 * error on failure usually EPERM or EAUTH
 */
int
execv_script(const char *interpreter, char * const *argv)
{
	const char *script;
	int rc;

	script = argv[0];
	if (veriexec_check_path(script) == 0) {
		rc = execv(script, argv);
	}
	/* still here? we might be allowed to run via interpreter */
	if (gbl_check_pid(0) & GBL_VERIEXEC) {
		if (!interpreter)
			interpreter = find_interpreter(script);
		if (interpreter) {
			syslog(LOG_NOTICE, "running %s via %s",
			    script, interpreter);
			rc = execv(interpreter, argv);
		}
	}
	return (rc);
}

#if defined(MAIN) || defined(UNIT_TEST)
#include <sys/wait.h>
#include <err.h>

int
main(int argc __unused, char *argv[])
{
	const char *interp;
	int c;
	int s;
	pid_t child;

	openlog("exec_script", LOG_PID|LOG_PERROR, LOG_DAEMON);

	interp = NULL;
	while ((c = getopt(argc, argv, "i:")) != -1) {
		switch (c) {
		case 'i':
			interp = optarg;
			break;
		default:
			errx(1, "unknown option: -%c", c);
			break;
		}
	}
	argc -= optind;
	argv += optind;
	/* we need a child */
	child = fork();
	if (child < 0)
		err(2, "fork");
	if (child == 0) {
		c = execv_script(interp, argv);
		err(2, "exec_script(%s,%s)", interp, argv[0]);
	}
	c = waitpid(child, &s, 0);
	printf("%s: exit %d\n", argv[0], WEXITSTATUS(s));
	return (0);
}
#endif
