/*-
 * Copyright (c) 2001 Marcel Moolenaar
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
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/sbuf.h>

#include <machine/../linux/linux.h>
#include <machine/../linux/linux_proto.h>

#include <compat/linux/linux_util.h>

#define	LINUX_CTL_KERN		1
#define	LINUX_CTL_VM		2
#define	LINUX_CTL_NET		3
#define	LINUX_CTL_PROC		4
#define	LINUX_CTL_FS		5
#define	LINUX_CTL_DEBUG		6
#define	LINUX_CTL_DEV		7
#define	LINUX_CTL_BUS		8

/* CTL_KERN names */
#define	LINUX_KERN_OSTYPE	1
#define	LINUX_KERN_OSRELEASE	2
#define	LINUX_KERN_OSREV	3
#define	LINUX_KERN_VERSION	4

static int
handle_string(struct l___sysctl_args *la, char *value)
{
	int error;

	if (la->oldval != NULL) {
		l_int len = strlen(value);
		error = copyout(value, la->oldval, len + 1);
		if (!error && la->oldlenp != NULL)
			error = copyout(&len, la->oldlenp, sizeof(len));
		if (error)
			return (error);
	}

	if (la->newval != NULL)
		return (ENOTDIR);

	return (0);
}

int
linux_sysctl(struct thread *td, struct linux_sysctl_args *args)
{
	struct l___sysctl_args la;
	struct sbuf *sb;
	l_int *mib;
	int error, i;

	error = copyin((caddr_t)args->args, &la, sizeof(la));
	if (error)
		return (error);

	if (la.nlen <= 0 || la.nlen > LINUX_CTL_MAXNAME)
		return (ENOTDIR);

	mib = malloc(la.nlen * sizeof(l_int), M_TEMP, 0);
	error = copyin(la.name, mib, la.nlen * sizeof(l_int));
	if (error) {
		free(mib, M_TEMP);
		return (error);
	}

	switch (mib[0]) {
	case LINUX_CTL_KERN:
		if (la.nlen < 2)
			break;

		switch (mib[1]) {
		case LINUX_KERN_VERSION:
			error = handle_string(&la, version);
			free(mib, M_TEMP);
			return (error);
		default:
			break;
		}
		break;
	default:
		break;
	}

	sb = sbuf_new(NULL, NULL, 20 + la.nlen * 5, SBUF_AUTOEXTEND);
	if (sb == NULL) {
		linux_msg(td, "sysctl is not implemented");
	} else {
		sbuf_printf(sb, "sysctl ");
		for (i = 0; i < la.nlen; i++)
			sbuf_printf(sb, "%c%d", (i) ? ',' : '{', mib[i]);
		sbuf_printf(sb, "} is not implemented");
		sbuf_finish(sb);
		linux_msg(td, "%s", sbuf_data(sb));
		sbuf_delete(sb);
	}

	free(mib, M_TEMP);
	return (ENOTDIR);
}
