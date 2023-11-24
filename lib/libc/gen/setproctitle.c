/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 1995 Peter Wemm
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
 */

#include "namespace.h"
#include <sys/param.h>
#include <sys/elf_common.h>
#include <sys/exec.h>
#include <sys/sysctl.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "un-namespace.h"

#include "libc_private.h"

/*
 * Older FreeBSD 2.0, 2.1 and 2.2 had different ps_strings structures and
 * in different locations.
 * 1: old_ps_strings at the very top of the stack.
 * 2: old_ps_strings at SPARE_USRSPACE below the top of the stack.
 * 3: ps_strings at the very top of the stack.
 * We only support a kernel providing #3 style ps_strings.
 *
 * For historical purposes, a definition of the old ps_strings structure
 * and location is preserved below:
struct old_ps_strings {
	char	*old_ps_argvstr;
	int	old_ps_nargvstr;
	char	*old_ps_envstr;
	int	old_ps_nenvstr;
};
#define	OLD_PS_STRINGS ((struct old_ps_strings *) \
	(USRSTACK - SPARE_USRSPACE - sizeof(struct old_ps_strings)))
 */

#include <stdarg.h>

#define SPT_BUFSIZE 2048	/* from other parts of sendmail */

static char *
setproctitle_internal(const char *fmt, va_list ap)
{
	static struct ps_strings *ps_strings;
	static char *buf = NULL;
	static char *obuf = NULL;
	static char **oargv;
	static int oargc = -1;
	static char *nargv[2] = { NULL, NULL };
	char **nargvp;
	int nargc;
	int i;
	size_t len;
	unsigned long ul_ps_strings;

	if (buf == NULL) {
		buf = malloc(SPT_BUFSIZE);
		if (buf == NULL)
			return (NULL);
		nargv[0] = buf;
	}

	if (obuf == NULL ) {
		obuf = malloc(SPT_BUFSIZE);
		if (obuf == NULL)
			return (NULL);
		*obuf = '\0';
	}

	if (fmt) {
		buf[SPT_BUFSIZE - 1] = '\0';

		if (fmt[0] == '-') {
			/* skip program name prefix */
			fmt++;
			len = 0;
		} else {
			/* print program name heading for grep */
			(void)snprintf(buf, SPT_BUFSIZE, "%s: ", _getprogname());
			len = strlen(buf);
		}

		/* print the argument string */
		(void)vsnprintf(buf + len, SPT_BUFSIZE - len, fmt, ap);

		nargvp = nargv;
		nargc = 1;
	} else if (*obuf != '\0') {
		/* Idea from NetBSD - reset the title on fmt == NULL */
		nargvp = oargv;
		nargc = oargc;
	} else
		/* Nothing to restore */
		return (NULL);

	if (ps_strings == NULL)
		(void)_elf_aux_info(AT_PS_STRINGS, &ps_strings,
		    sizeof(ps_strings));

	if (ps_strings == NULL) {
		len = sizeof(ul_ps_strings);
		if (sysctlbyname("kern.ps_strings", &ul_ps_strings, &len, NULL,
		    0) == -1)
			return (NULL);
		ps_strings = (struct ps_strings *)ul_ps_strings;
	}

	if (ps_strings == NULL)
		return (NULL);

	/*
	 * PS_STRINGS points to zeroed memory on a style #2 kernel.
	 * Should not happen.
	 */
	if (ps_strings->ps_argvstr == NULL)
		return (NULL);

	/* style #3 */
	if (oargc == -1) {
		/* Record our original args */
		oargc = ps_strings->ps_nargvstr;
		oargv = ps_strings->ps_argvstr;
		for (i = len = 0; i < oargc; i++) {
			/*
			 * The program may have scribbled into its
			 * argv array, e.g., to remove some arguments.
			 * If that has happened, break out before
			 * trying to call strlen on a NULL pointer.
			 */
			if (oargv[i] == NULL) {
				oargc = i;
				break;
			}
			snprintf(obuf + len, SPT_BUFSIZE - len, "%s%s",
			    len != 0 ? " " : "", oargv[i]);
			if (len != 0)
				len++;
			len += strlen(oargv[i]);
			if (len >= SPT_BUFSIZE)
				break;
		}
	}
	ps_strings->ps_nargvstr = nargc;
	ps_strings->ps_argvstr = nargvp;

	return (nargvp[0]);
}

static int fast_update = 0;

void
setproctitle_fast(const char *fmt, ...)
{
	va_list ap;
	char *buf;
	int oid[4];

	va_start(ap, fmt);
	buf = setproctitle_internal(fmt, ap);
	va_end(ap);

	if (buf && !fast_update) {
		/* Tell the kernel to start looking in user-space */
		oid[0] = CTL_KERN;
		oid[1] = KERN_PROC;
		oid[2] = KERN_PROC_ARGS;
		oid[3] = -1;
		sysctl(oid, 4, 0, 0, "", 0);
		fast_update = 1;
	}
}

void
setproctitle(const char *fmt, ...)
{
	va_list ap;
	char *buf;
	int oid[4];

	va_start(ap, fmt);
	buf = setproctitle_internal(fmt, ap);
	va_end(ap);

	if (buf != NULL) {
		/* Set the title into the kernel cached command line */
		oid[0] = CTL_KERN;
		oid[1] = KERN_PROC;
		oid[2] = KERN_PROC_ARGS;
		oid[3] = -1;
		sysctl(oid, 4, 0, 0, buf, strlen(buf) + 1);
		fast_update = 0;
	}
}
