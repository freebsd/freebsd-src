/*
 * Copyright (c) 1995 Peter Wemm <peter@freebsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, is permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, without modification,
 *    this list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Absolutely no warranty of function or purpose is made by the author
 *    Peter Wemm.
 *
 * $FreeBSD$
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/exec.h>
#include <sys/sysctl.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

/*
 * Older FreeBSD 2.0, 2.1 and 2.2 had different ps_strings structures and
 * in different locations.
 * 1: old_ps_strings at the very top of the stack.
 * 2: old_ps_strings at SPARE_USRSPACE below the top of the stack.
 * 3: ps_strings at the very top of the stack.
 * This attempts to support a kernel built in the #2 and #3 era.
 */

struct old_ps_strings {
	char	*old_ps_argvstr;
	int	old_ps_nargvstr;
	char	*old_ps_envstr;
	int	old_ps_nenvstr;
};
#define	OLD_PS_STRINGS ((struct old_ps_strings *) \
	(USRSTACK - SPARE_USRSPACE - sizeof(struct old_ps_strings)))

#include <stdarg.h>

#define SPT_BUFSIZE 2048	/* from other parts of sendmail */
extern char * __progname;	/* is this defined in a .h anywhere? */

void
setproctitle(const char *fmt, ...)
{
	static struct ps_strings *ps_strings;
	static char buf[SPT_BUFSIZE];
	static char obuf[SPT_BUFSIZE];
	static char **oargv, *kbuf;
	static int oargc = -1;
	static char *nargv[2] = { buf, NULL };
	char **nargvp;
	int nargc;
	int i;
	va_list ap;
	size_t len;
	unsigned long ul_ps_strings;
	int oid[4];

	va_start(ap, fmt);

	if (fmt) {
		buf[sizeof(buf) - 1] = '\0';

		if (fmt[0] == '-') {
			/* skip program name prefix */
			fmt++;
			len = 0;
		} else {
			/* print program name heading for grep */
			(void) snprintf(buf, sizeof(buf), "%s: ", __progname);
			len = strlen(buf);
		}

		/* print the argument string */
		(void) vsnprintf(buf + len, sizeof(buf) - len, fmt, ap);

		nargvp = nargv;
		nargc = 1;
		kbuf = buf;
	} else if (*obuf != '\0') {
  		/* Idea from NetBSD - reset the title on fmt == NULL */
		nargvp = oargv;
		nargc = oargc;
		kbuf = obuf;
	} else
		/* Nothing to restore */
		return;

	va_end(ap);

	/* Set the title into the kernel cached command line */
	oid[0] = CTL_KERN;
	oid[1] = KERN_PROC;
	oid[2] = KERN_PROC_ARGS;
	oid[3] = getpid();
	sysctl(oid, 4, 0, 0, kbuf, strlen(kbuf) + 1);

	if (ps_strings == NULL) {
		len = sizeof(ul_ps_strings);
		if (sysctlbyname("kern.ps_strings", &ul_ps_strings, &len, NULL,
		    0) == -1)
			ul_ps_strings = PS_STRINGS;
		ps_strings = (struct ps_strings *)ul_ps_strings;
	}

	/* PS_STRINGS points to zeroed memory on a style #2 kernel */
	if (ps_strings->ps_argvstr) {
		/* style #3 */
		if (oargc == -1) {
			/* Record our original args */
			oargc = ps_strings->ps_nargvstr;
			oargv = ps_strings->ps_argvstr;
			for (i = len = 0; i < oargc; i++) {
				snprintf(obuf + len, sizeof(obuf) - len, "%s%s",
				    len ? " " : "", oargv[i]);
				if (len)
					len++;
				len += strlen(oargv[i]);
				if (len >= sizeof(obuf))
					break;
			}
		}
		ps_strings->ps_nargvstr = nargc;
		ps_strings->ps_argvstr = nargvp;
	} else {
		/* style #2 - we can only restore our first arg :-( */
		if (*obuf == '\0')
			strncpy(obuf, OLD_PS_STRINGS->old_ps_argvstr,
			    sizeof(obuf) - 1);
		OLD_PS_STRINGS->old_ps_nargvstr = 1;
		OLD_PS_STRINGS->old_ps_argvstr = nargvp[0];
	}
}
