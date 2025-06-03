/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 The FreeBSD Foundation
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
#include <sys/event.h>
#include <sys/sysctl.h>
#include <sys/user.h>

#include <err.h>
#include <errno.h>
#include <libprocstat.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "procstat.h"

static const char kqs[] = "kqueues";
static const char kq[] = "kqueue";

#define FILT_ELEM(name)	[-EVFILT_##name] = #name,
static const char *const filter_names[] = {
	[0] = "invalid",
	FILT_ELEM(READ)
	FILT_ELEM(WRITE)
	FILT_ELEM(AIO)
	FILT_ELEM(VNODE)
	FILT_ELEM(PROC)
	FILT_ELEM(SIGNAL)
	FILT_ELEM(TIMER)
	FILT_ELEM(PROCDESC)
	FILT_ELEM(FS)
	FILT_ELEM(LIO)
	FILT_ELEM(USER)
	FILT_ELEM(SENDFILE)
	FILT_ELEM(EMPTY)
};
#undef FILT_ELEM

#define	PK_FLAG_ELEM(fname, str) { .flag = fname, .dispstr = str }
#define	PK_NAME_ELEM(prefix, fname) { .flag = prefix##fname, .dispstr = #fname }
#define PK_FLAG_LAST_ELEM() { .flag = -1, .dispstr = NULL }
struct pk_elem {
	unsigned int flag;
	const char *dispstr;
};

static const struct pk_elem kn_status_names[] = {
	PK_FLAG_ELEM(KNOTE_STATUS_ACTIVE, "A"),
	PK_FLAG_ELEM(KNOTE_STATUS_QUEUED, "Q"),
	PK_FLAG_ELEM(KNOTE_STATUS_DISABLED, "D"),
	PK_FLAG_ELEM(KNOTE_STATUS_DETACHED, "d"),
	PK_FLAG_ELEM(KNOTE_STATUS_KQUEUE, "K"),
	PK_FLAG_LAST_ELEM(),
};

static const struct pk_elem ev_flags_names[] = {
	PK_FLAG_ELEM(EV_ONESHOT, "O"),
	PK_FLAG_ELEM(EV_CLEAR, "C"),
	PK_FLAG_ELEM(EV_RECEIPT, "R"),
	PK_FLAG_ELEM(EV_DISPATCH, "D"),
	PK_FLAG_ELEM(EV_DROP, "d"),
	PK_FLAG_ELEM(EV_FLAG1, "1"),
	PK_FLAG_ELEM(EV_FLAG2, "2"),
	PK_FLAG_LAST_ELEM(),
};

static char *
procstat_kqueue_flags(const struct pk_elem *names, unsigned flags, bool commas)
{
	char *res;
	const struct pk_elem *pl;
	size_t len;
	int i;
	bool first;

	first = true;
	len = 0;
	for (i = 0;; i++) {
		pl = &names[i];
		if (pl->flag == (unsigned)-1)
			break;
		if ((flags & pl->flag) != 0) {
			if (first)
				first = false;
			else if (commas)
				len += sizeof(",");
			len += strlen(pl->dispstr);
		}
	}
	len++;

	res = malloc(len);
	first = true;
	res[0] = '\0';
	for (i = 0;; i++) {
		pl = &names[i];
		if (pl->flag == (unsigned)-1)
			break;
		if ((flags & pl->flag) != 0) {
			if (first)
				first = false;
			else if (commas)
				strlcat(res, ",", len);
			strlcat(res, pl->dispstr, len);
		}
	}

	if (strlen(res) == 0)
		return (strdup("-"));
	return (res);
}

static const struct pk_elem rw_filter_names[] = {
	PK_NAME_ELEM(NOTE_, LOWAT),
	PK_NAME_ELEM(NOTE_, FILE_POLL),
	PK_FLAG_LAST_ELEM(),
};

static const struct pk_elem user_filter_names[] = {
	PK_NAME_ELEM(NOTE_, FFAND),
	PK_NAME_ELEM(NOTE_, FFOR),
	PK_NAME_ELEM(NOTE_, TRIGGER),
	PK_FLAG_LAST_ELEM(),
};

static const struct pk_elem vnode_filter_names[] = {
	PK_NAME_ELEM(NOTE_, DELETE),
	PK_NAME_ELEM(NOTE_, WRITE),
	PK_NAME_ELEM(NOTE_, EXTEND),
	PK_NAME_ELEM(NOTE_, ATTRIB),
	PK_NAME_ELEM(NOTE_, LINK),
	PK_NAME_ELEM(NOTE_, RENAME),
	PK_NAME_ELEM(NOTE_, REVOKE),
	PK_NAME_ELEM(NOTE_, OPEN),
	PK_NAME_ELEM(NOTE_, CLOSE),
	PK_NAME_ELEM(NOTE_, CLOSE_WRITE),
	PK_NAME_ELEM(NOTE_, READ),
	PK_FLAG_LAST_ELEM(),
};

static const struct pk_elem proc_filter_names[] = {
	PK_NAME_ELEM(NOTE_, EXIT),
	PK_NAME_ELEM(NOTE_, FORK),
	PK_NAME_ELEM(NOTE_, EXEC),
	PK_NAME_ELEM(NOTE_, TRACK),
	PK_NAME_ELEM(NOTE_, TRACKERR),
	PK_NAME_ELEM(NOTE_, CHILD),
	PK_FLAG_LAST_ELEM(),
};

static const struct pk_elem timer_filter_names[] = {
	PK_NAME_ELEM(NOTE_, SECONDS),
	PK_NAME_ELEM(NOTE_, MSECONDS),
	PK_NAME_ELEM(NOTE_, USECONDS),
	PK_NAME_ELEM(NOTE_, NSECONDS),
	PK_NAME_ELEM(NOTE_, ABSTIME),
	PK_FLAG_LAST_ELEM(),
};

#define FILT_ELEM(name)	[-EVFILT_##name] = "EVFILT_"#name
static const struct pk_elem *const filter_pk_names[] = {
	[0] = 			NULL,
	[-EVFILT_READ] =	rw_filter_names,
	[-EVFILT_WRITE] =	rw_filter_names,
	[-EVFILT_AIO] =		rw_filter_names,
	[-EVFILT_VNODE] =	vnode_filter_names,
	[-EVFILT_PROC] =	proc_filter_names,
	[-EVFILT_SIGNAL] =	NULL,
	[-EVFILT_TIMER] =	timer_filter_names,
	[-EVFILT_PROCDESC] =	proc_filter_names,
	[-EVFILT_FS] =		NULL,
	[-EVFILT_LIO] =		rw_filter_names,
	[-EVFILT_USER] =	user_filter_names,
	[-EVFILT_SENDFILE] =	NULL,
	[-EVFILT_EMPTY] =	NULL,
};

static char *
procstat_kqueue_fflags(int filter, unsigned fflags)
{
	const struct pk_elem *names;

	names = NULL;
	if (filter < 0 && -filter < (int)nitems(filter_pk_names))
		names = filter_pk_names[-filter];
	if (names == NULL)
		return (strdup("-"));
	return (procstat_kqueue_flags(names, fflags, true));
}

static const char *
procstat_kqueue_get_filter_name(int filter)
{
	filter = -filter;
	if (filter < 0 || filter >= (int)nitems(filter_names))
		filter = 0;
	return (filter_names[filter]);
}

static void
procstat_kqueue(struct procstat *procstat, struct kinfo_proc *kipp, int fd,
    bool verbose)
{
	struct kinfo_knote *kni, *knis;
	char *flags, *fflags, *status;
	unsigned int count, i, j;
	char errbuf[_POSIX2_LINE_MAX];

	errbuf[0] = '\0';
	knis = procstat_get_kqueue_info(procstat, kipp, fd, &count, errbuf);
	if (knis == NULL) {
		warnx("%s\n", errbuf);
		return;
	}

	for (i = 0; i < count; i++) {
		kni = &knis[i];
		flags = procstat_kqueue_flags(ev_flags_names,
		    kni->knt_event.flags, false);
		fflags = procstat_kqueue_fflags(kni->knt_event.filter,
		    kni->knt_event.fflags);
		status = procstat_kqueue_flags(kn_status_names,
		    kni->knt_status, false);
		xo_open_instance(kq);
		xo_emit("{dk:process_id/%7d} ", kipp->ki_pid);
		xo_emit("{:kqueue_fd/%10d} ", fd);
		xo_emit("{:filter/%8s} ", procstat_kqueue_get_filter_name(
		    kni->knt_event.filter));
		xo_emit("{:ident/%10d} ", kni->knt_event.ident);
		xo_emit("{:flags/%10s} ", flags);
		xo_emit("{:fflags/%10s} ", fflags);
		xo_emit("{:data/%#10jx} ", (uintmax_t)kni->knt_event.data);
		xo_emit("{:udata/%10p} ", (uintmax_t)kni->knt_event.udata);
		if (verbose) {
			for (j = 0; j < nitems(kni->knt_event.ext); j++) {
				xo_emit("{:ext%u/%#10jx} ", j,
				    (uintmax_t)kni->knt_event.ext[j]);
			}
		}
		xo_emit("{:status/%10s}\n", status);
		free(flags);
		free(fflags);
		free(status);
		xo_close_instance(kq);
	}

	procstat_freekqinfo(procstat, knis);
}

void
procstat_kqueues(struct procstat *procstat, struct kinfo_proc *kipp)
{
	struct filestat_list *fl;
	struct filestat *f;
	bool verbose;

	verbose = (procstat_opts & PS_OPT_VERBOSE) != 0;
	if ((procstat_opts & PS_OPT_NOHEADER) == 0) {
		if (verbose) {
			xo_emit("{T:/%7s %10s %8s %10s %10s %10s %10s %10s "
			    "%10s %10s %10s %10s %10s}\n",
			    "PID", "KQFD", "FILTER", "IDENT", "FLAGS", "FFLAGS",
			    "DATA", "UDATA", "EXT0", "EXT1","EXT2","EXT3",
			    "STATUS");
		} else {
			xo_emit("{T:/%7s %10s %8s %10s %10s %10s %10s %10s "
			    "%10s}\n",
			    "PID", "KQFD", "FILTER", "IDENT", "FLAGS", "FFLAGS",
			    "DATA", "UDATA", "STATUS");
		}
	}

	xo_emit("{ek:process_id/%d}", kipp->ki_pid);

	fl = procstat_getfiles(procstat, kipp, 0);
	if (fl == NULL)
		return;
	xo_open_list(kqs);
	STAILQ_FOREACH(f, fl, next) {
		if (f->fs_type != PS_FST_TYPE_KQUEUE)
			continue;
		xo_emit("{ek:kqueue/%d}", f->fs_fd);
		xo_open_list(kq);
		procstat_kqueue(procstat, kipp, f->fs_fd, verbose);
		xo_close_list(kq);
	}
	xo_close_list(kqs);
	procstat_freefiles(procstat, fl);
}
