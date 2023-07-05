/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021 Yoshihiro Ota <ota@j.email.ne.jp>
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

#include <curses.h>
#include <libprocstat.h>
#include <libutil.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "systat.h"
#include "extern.h"

/*
 * vm objects of swappable types
 */
static struct swapvm {
	uint64_t kvo_me;
	uint32_t swapped; /* in pages */
	uint64_t next;
	pid_t pid; /* to avoid double counting */
} *swobj = NULL;
static int nswobj = 0;

static struct procstat *prstat = NULL;
/*
 *procstat_getvmmap() is an expensive call and the number of processes running
 * may also be high.  So, maintain an array of pointers for ease of expanding
 * an array and also swapping pointers are faster than struct.
 */
static struct proc_usage {
	pid_t pid;
	uid_t uid;
	char command[COMMLEN + 1];
	uint64_t total;
	uint32_t pages;
} **pu = NULL;
static int nproc;
static int proc_compar(const void *, const void *);

static void
display_proc_line(int idx, int y, uint64_t totalswappages)
{
	int offset = 0, rate;
	const char *uname;
	char buf[30];
	uint64_t swapbytes;

	wmove(wnd, y, 0);
	wclrtoeol(wnd);
	if (idx >= nproc)
		return;

	uname = user_from_uid(pu[idx]->uid, 0);
	swapbytes = ptoa(pu[idx]->pages);

	snprintf(buf, sizeof(buf), "%6d %-10s %-10.10s", pu[idx]->pid, uname,
	    pu[idx]->command);
	offset = 6 + 1 + 10 + 1 + 10 + 1;
	mvwaddstr(wnd, y, 0, buf);
	sysputuint64(wnd, y, offset, 4, swapbytes, 0);
	offset += 4;
	mvwaddstr(wnd, y, offset, " / ");
	offset += 3;
	sysputuint64(wnd, y, offset, 4, pu[idx]->total, 0);
	offset += 4;

	rate = pu[idx]->total > 1 ? 100 * swapbytes / pu[idx]->total : 0;
	snprintf(buf, sizeof(buf), "%3d%%", rate);
	mvwaddstr(wnd, y, offset, buf);
	if (rate > 100) /* avoid running over the screen */
		rate = 100;
	sysputXs(wnd, y, offset + 5, rate / 10);

	rate = 100 * pu[idx]->pages / totalswappages;
	snprintf(buf, sizeof(buf), "%3d%%", rate);
	mvwaddstr(wnd, y, offset + 16, buf);
	if (rate > 100) /* avoid running over the screen */
		rate = 100;
	sysputXs(wnd, y, offset + 21, rate / 10);
}

static int
swobj_search(const void *a, const void *b)
{
	const uint64_t *aa = a;
	const struct swapvm *bb = b;

	if (*aa == bb->kvo_me)
		return (0);
	return (*aa > bb->kvo_me ? -1 : 1);
}

static int
swobj_sort(const void *a, const void *b)
{

	return ((((const struct swapvm *) a)->kvo_me >
	    ((const struct swapvm *) b)->kvo_me) ? -1 : 1);
}

static bool
get_swap_vmobjects(void)
{
	static int maxnobj;
	int cnt, i, next_i, last_nswobj;
	struct kinfo_vmobject *kvo;

	next_i = nswobj = 0;
	kvo = kinfo_getswapvmobject(&cnt);
	if (kvo == NULL) {
		error("kinfo_getswapvmobject()");
		return (false);
	}
	do {
		for (i = next_i; i < cnt; i++) {
			if (kvo[i].kvo_type != KVME_TYPE_DEFAULT &&
			    kvo[i].kvo_type != KVME_TYPE_SWAP)
				continue;
			if (nswobj < maxnobj) {
				swobj[nswobj].kvo_me = kvo[i].kvo_me;
				swobj[nswobj].swapped = kvo[i].kvo_swapped;
				swobj[nswobj].next = kvo[i].kvo_backing_obj;
				swobj[nswobj].pid = 0;
				next_i = i + 1;
			}
			nswobj++;
		}
		if (nswobj <= maxnobj)
			break;
		/* allocate memory and fill skipped elements */
		last_nswobj = maxnobj;
		maxnobj = nswobj;
		nswobj = last_nswobj;
		/* allocate more memory and fill missed ones */
		if ((swobj = reallocf(swobj, maxnobj * sizeof(*swobj))) ==
		    NULL) {
			error("Out of memory");
			die(0);
		}
	} while (i <= cnt); /* extra safety guard */
	free(kvo);
	if (nswobj > 1)
		qsort(swobj, nswobj, sizeof(swobj[0]), swobj_sort);
	return (nswobj > 0);
}

/* This returns the number of swap pages a process uses. */
static uint32_t
per_proc_swap_usage(struct kinfo_proc *kipp)
{
	int i, cnt;
	uint32_t pages = 0;
	uint64_t vmobj;
	struct kinfo_vmentry *freep, *kve;
	struct swapvm *vm;

	freep = procstat_getvmmap(prstat, kipp, &cnt);
	if (freep == NULL)
		return (pages);

	for (i = 0; i < cnt; i++) {
		kve = &freep[i];
		if (kve->kve_type == KVME_TYPE_DEFAULT ||
		    kve->kve_type == KVME_TYPE_SWAP) {
			vmobj = kve->kve_obj;
			do {
				vm = bsearch(&vmobj, swobj, nswobj,
				    sizeof(swobj[0]), swobj_search);
				if (vm != NULL && vm->pid != kipp->ki_pid) {
					pages += vm->swapped;
					vmobj = vm->next;
					vm->pid = kipp->ki_pid;
				} else
					break;
			} while (vmobj != 0);
		}
	}
	free(freep);
	return (pages);
}

void
procshow(int lcol, int hight, uint64_t totalswappages)
{
	int i, y;

	for (i = 0, y = lcol + 1 /* HEADING */; i < hight; i++, y++)
		display_proc_line(i, y, totalswappages);
}

int
procinit(void)
{

	if (prstat == NULL)
		prstat = procstat_open_sysctl();
	return (prstat != NULL);
}

void
procgetinfo(void)
{
	static int maxnproc = 0;
	int cnt, i;
	uint32_t pages;
	struct kinfo_proc *kipp;

	nproc = 0;
	if ( ! get_swap_vmobjects() ) /* call failed or nothing is paged-out */
		return;

	kipp = procstat_getprocs(prstat, KERN_PROC_PROC, 0, &cnt);
	if (kipp == NULL) {
		error("procstat_getprocs()");
		return;
	}
	if (maxnproc < cnt) {
		if ((pu = realloc(pu, cnt * sizeof(*pu))) == NULL) {
			error("Out of memory");
			die(0);
		}
		memset(&pu[maxnproc], 0, (cnt - maxnproc) * sizeof(pu[0]));
		maxnproc = cnt;
	}

	for (i = 0; i < cnt; i++) {
		pages = per_proc_swap_usage(&kipp[i]);
		if (pages == 0)
			continue;
		if (pu[nproc] == NULL &&
		    (pu[nproc] = malloc(sizeof(**pu))) == NULL) {
			error("Out of memory");
			die(0);
		}
		strlcpy(pu[nproc]->command, kipp[i].ki_comm,
		    sizeof(pu[nproc]->command));
		pu[nproc]->pid = kipp[i].ki_pid;
		pu[nproc]->uid = kipp[i].ki_uid;
		pu[nproc]->pages = pages;
		pu[nproc]->total = kipp[i].ki_size;
		nproc++;
	}
	if (nproc > 1)
		qsort(pu, nproc, sizeof(*pu), proc_compar);
}

void
proclabel(int lcol)
{

	wmove(wnd, lcol, 0);
	wclrtoeol(wnd);
	mvwaddstr(wnd, lcol, 0,
	    "Pid    Username   Command     Swap/Total "
	    "Per-Process    Per-System");
}

int
proc_compar(const void *a, const void *b)
{
	const struct proc_usage *aa = *((const struct proc_usage **)a);
	const struct proc_usage *bb = *((const struct proc_usage **)b);

	return (aa->pages > bb->pages ? -1 : 1);
}
