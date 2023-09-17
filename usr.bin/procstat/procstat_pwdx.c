/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020 Juraj Lutter <juraj@lutter.sk>
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

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/capsicum.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/un.h>
#include <sys/user.h>

#include <netinet/in.h>

#include <arpa/inet.h>

#include <err.h>
#include <libprocstat.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "procstat.h"

void
procstat_pwdx(struct procstat *procstat, struct kinfo_proc *kipp)
{
	struct filestat_list *head;
	struct filestat *fst;

	head = procstat_getfiles(procstat, kipp, 0);
	if (head == NULL)
		return;
	STAILQ_FOREACH(fst, head, next) {
		if ((fst->fs_uflags & PS_FST_UFLAG_CDIR) &&
		    (fst->fs_path != NULL)) {
			xo_emit("{k:process_id/%d}{P::  }", kipp->ki_pid);
			xo_emit("{:cwd/%s}", fst->fs_path);
			xo_emit("\n");
		}
	}
	procstat_freefiles(procstat, head);
}
