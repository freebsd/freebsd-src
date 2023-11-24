/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 The FreeBSD Foundation
 *
 * Portions of this software were developed by Konstantin Belousov
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

#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/user.h>

#include <err.h>
#include <libprocstat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "procstat.h"

void
procstat_advlocks(struct procstat *prstat, struct kinfo_proc *kipp __unused)
{
	struct advlock_list *advl;
	struct advlock *a;
	static const char advisory_lock_item[] = "advisory_lock";

	if ((procstat_opts & PS_OPT_NOHEADER) == 0)
		xo_emit("{T:/%2s %5s %5s %5s %18s %18s %8s %9s %9s %s}\n",
		    "RW", "TYPE", "PID", "SYSID", "FSID", "RDEV", "INO",
		    "START", "LEN", "PATH");

	xo_open_list(advisory_lock_item);
	advl = procstat_getadvlock(prstat);
	if (advl == NULL) {
		xo_close_list(advisory_lock_item);
		return;
	}

	STAILQ_FOREACH(a, advl, next) {
		xo_open_instance(advisory_lock_item);
		switch (a->rw) {
		case PS_ADVLOCK_RO:
			xo_emit("{:rw/%s} ", "RO");
			break;
		case PS_ADVLOCK_RW:
			xo_emit("{:rw/%s} ", "RW");
			break;
		default:
			xo_emit("{:rw/%s} ", "??");
			break;
		}
		switch (a->type) {
		case PS_ADVLOCK_TYPE_FLOCK:
			xo_emit("{:type/%s} ", "FLOCK");
			break;
		case PS_ADVLOCK_TYPE_PID:
			xo_emit("{:type/%s} ", "FCNTL");
			break;
		case PS_ADVLOCK_TYPE_REMOTE:
			xo_emit("{:type/%s} ", "LOCKD");
			break;
		default:
			xo_emit("{:type/%s} ", "?????");
			break;
		}
		xo_emit("{:pid/%5d} ", a->pid);
		xo_emit("{:sysid/%5d} ", a->sysid);
		xo_emit("{:fsid/%18#jx} ", (uintmax_t)a->file_fsid);
		xo_emit("{:rdev/%#18jx} ", (uintmax_t)a->file_rdev);
		xo_emit("{:ino/%8ju} ", (uintmax_t)a->file_fileid);
		xo_emit("{:start/%9ju} ", (uintmax_t)a->start);
		xo_emit("{:len/%9ju} ", (uintmax_t)a->len);
		xo_emit("{:path/%s}", a->path == NULL ? "" : a->path);
		xo_emit("\n");
		xo_close_instance(advisory_lock_item);
	}
	xo_close_list(advisory_lock_item);
	procstat_freeadvlock(prstat, advl);
}
