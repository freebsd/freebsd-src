/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 Jules
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
 */

#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/user.h>

#include <err.h>
#include <errno.h>
#include <libprocstat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "procstat.h"

void
procstat_page_faults(struct procstat *prstat __unused,
    struct kinfo_proc *kipp)
{
	long minflt, majflt;
	char mibname[256];
	size_t len;
	int error;

	if ((procstat_opts & PS_OPT_NOHEADER) == 0) {
		xo_emit("{T:/%5s %-19s %10s %10s}\\n", "PID", "COMM",
		    "MINFLT", "MAJFLT");
	}

	/* Get minor page faults */
	snprintf(mibname, sizeof(mibname), "kern.proc.pid.%d.minflt",
	    kipp->ki_pid);
	len = sizeof(minflt);
	error = sysctlbyname(mibname, &minflt, &len, NULL, 0);
	if (error != 0 && errno != ESRCH) { /* ESRCH means process gone */
		xo_warn("sysctlbyname(%s) failed", mibname);
		minflt = -1; /* Indicate error */
	} else if (error != 0 && errno == ESRCH) {
		return; /* Process disappeared, nothing to print */
	}


	/* Get major page faults */
	snprintf(mibname, sizeof(mibname), "kern.proc.pid.%d.majflt",
	    kipp->ki_pid);
	len = sizeof(majflt);
	error = sysctlbyname(mibname, &majflt, &len, NULL, 0);
	if (error != 0 && errno != ESRCH) {
		xo_warn("sysctlbyname(%s) failed", mibname);
		majflt = -1; /* Indicate error */
	} else if (error != 0 && errno == ESRCH) {
		return; /* Process disappeared */
	}

	xo_emit("{L:/%5d %-19s %10ld %10ld}\\n", kipp->ki_pid,
	    kipp->ki_comm, minflt, majflt);
}
