/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 The FreeBSD Foundation
 *
 * This software was developed by Konstantin Belousov <kib@FreeBSD.org>
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
#define _RLIMIT_IDENT
#include <sys/resource.h>
#include <sys/user.h>

#include <err.h>
#include <errno.h>
#include <libprocstat.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <libutil.h>

#include "procstat.h"

static const char ru[] = "resource_usage";

void
procstat_rlimitusage(struct procstat *procstat, struct kinfo_proc *kipp)
{
	rlim_t *resuse;
	unsigned int cnt, i;

	if ((procstat_opts & PS_OPT_NOHEADER) == 0)
		xo_emit("{T:/%7s %12s %4s %18s}\n",
		    "PID", "RESOURCE", "ID", "USAGE");

	xo_emit("{ek:process_id/%d}", kipp->ki_pid);

	resuse = procstat_getrlimitusage(procstat, kipp, &cnt);
	if (resuse == NULL)
		return;
	xo_open_list(ru);
	for (i = 0; i < cnt; i++) {
		xo_open_instance(ru);
		xo_emit("{dk:process_id/%7d} ", kipp->ki_pid);
		xo_emit("{:resource/%12s} ", i < nitems(rlimit_ident) ?
		    rlimit_ident[i] : "unknown");
		xo_emit("{:resid/%4d} ", i);
		xo_emit("{:usage/%18jd}\n", (intmax_t)resuse[i]);
		xo_close_instance(ru);
	}
	xo_close_list(ru);
	procstat_freerlimitusage(procstat, resuse);
}
