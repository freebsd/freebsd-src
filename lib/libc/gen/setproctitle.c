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
 * $Id: setproctitle.c,v 1.5 1997/02/22 15:08:33 peter Exp $
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

#if defined(__STDC__)		/* from other parts of sendmail */
#include <stdarg.h>
#else
#include <varargs.h>
#endif


#define SPT_BUFSIZE 128	/* from other parts of sendmail */
extern char * __progname;	/* is this defined in a .h anywhere? */

static struct ps_strings *ps_strings;

void
#if defined(__STDC__)
setproctitle(const char *fmt, ...)
#else
setproctitle(fmt, va_alist)
	const char *fmt;
	va_dcl
#endif
{
	static char buf[SPT_BUFSIZE];
	static char *ps_argv[2];
	va_list ap;
	int mib[2];
	size_t len;

#if defined(__STDC__)
	va_start(ap, fmt);
#else
	va_start(ap);
#endif

	buf[sizeof(buf) - 1] = '\0';
	if (fmt) {

		/* print program name heading for grep */
		(void) snprintf(buf, sizeof(buf) - 1, "%s: ", __progname);

		/*
		 * can't use return from sprintf, as that is the count of how
		 * much it wanted to write, not how much it actually did.
		 */

		len = strlen(buf);

		/* print the argument string */
		(void) vsnprintf(buf + len, sizeof(buf) - 1 - len, fmt, ap);
	} else {
		/* Idea from NetBSD - reset the title on fmt == NULL */
		strncpy(buf, __progname, sizeof(buf) - 1);
	}

	va_end(ap);

	if (ps_strings == NULL) {
		mib[0] = CTL_KERN;
		mib[1] = KERN_PS_STRINGS;
		len = sizeof(ps_strings);
		if (sysctl(mib, 2, &ps_strings, &len, NULL, 0) < 0 ||
		    ps_strings == NULL)
			ps_strings = PS_STRINGS;
	}

	/* PS_STRINGS points to zeroed memory on a style #2 kernel */
	if (ps_strings->ps_argvstr) {
		/* style #3 */
		ps_argv[0] = buf;
		ps_argv[1] = NULL;
		ps_strings->ps_nargvstr = 1;
		ps_strings->ps_argvstr = ps_argv;
	} else {
		/* style #2 */
		OLD_PS_STRINGS->old_ps_nargvstr = 1;
		OLD_PS_STRINGS->old_ps_argvstr = buf;
	}
}
