/*-
 * Copyright (c) 2021 The FreeBSD Foundation
 *
 * This software were developed by Konstantin Belousov <kib@FreeBSD.org>
 * under sponsorship from the FreeBSD Foundation.
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

#define	_WANT_P_OSREL
#include <sys/param.h>
#include <sys/sysctl.h>
#include <errno.h>
#include <sched.h>
#include <string.h>

#include "libc_private.h"

int
sched_setaffinity(pid_t pid, size_t cpusetsz, const cpuset_t *cpuset)
{
	static int mp_maxid;
	cpuwhich_t which;
	cpuset_t c;
	int error, lbs, cpu;
	size_t len, sz;

	if (__getosreldate() < P_OSREL_TIDPID) {
		if (pid == 0 || pid > _PID_MAX)
			which = CPU_WHICH_TID;
		else
			which = CPU_WHICH_PID;
	} else
		which = CPU_WHICH_TIDPID;

	sz = cpusetsz > sizeof(cpuset_t) ? sizeof(cpuset_t) : cpusetsz;
	memset(&c, 0, sizeof(c));
	memcpy(&c, cpuset, sz);

	/* Linux ignores high bits */
	if (mp_maxid == 0) {
		len = sizeof(mp_maxid);
		error = sysctlbyname("kern.smp.maxid", &mp_maxid, &len,
		    NULL, 0);
		if (error == -1)
			return (error);
	}
	lbs = CPU_FLS(&c) - 1;
	if (lbs > mp_maxid) {
		CPU_FOREACH_ISSET(cpu, &c)
			if (cpu > mp_maxid)
				CPU_CLR(cpu, &c);
	}
	error = cpuset_setaffinity(CPU_LEVEL_WHICH, which,
	    pid == 0 ? -1 : pid, sizeof(cpuset_t), &c);
	if (error == -1 && errno == EDEADLK)
		errno = EINVAL;

	return (error);
}
