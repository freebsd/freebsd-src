/*-
 * Copyright (c) 2023 Dmitry Chagin <dchagin@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/user.h>

#include <assert.h>
#include <libprocstat.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int	checkstack(void);

#define	_STACK_FLAG_GROWS	KVME_FLAG_GROWS_UP | KVME_FLAG_GROWS_DOWN
int
checkstack(void)
{
	struct kinfo_vmentry *freep, *kve;
	struct kinfo_proc *p;
	struct procstat *prstat;
	uint64_t stack;
	int i, cnt;

	prstat = procstat_open_sysctl();
	assert(prstat != NULL);
	p = procstat_getprocs(prstat, KERN_PROC_PID, getpid(), &cnt);
	assert(p != NULL);
	freep = procstat_getvmmap(prstat, p, &cnt);
	assert(freep != NULL);

	stack = (uint64_t)&i;
	for (i = 0; i < cnt; i++) {
		kve = &freep[i];
		if (stack < kve->kve_start || stack > kve->kve_end)
			continue;
		if ((kve->kve_flags & _STACK_FLAG_GROWS) != 0 &&
		    (kve->kve_protection & KVME_PROT_EXEC) != 0)
			stack = 0;
		break;
	}

	free(freep);
	procstat_freeprocs(prstat, p);
	procstat_close(prstat);
	return (stack != 0);
}
