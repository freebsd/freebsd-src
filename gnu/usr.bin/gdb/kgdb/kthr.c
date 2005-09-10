/*
 * Copyright (c) 2004 Marcel Moolenaar
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/types.h>
#include <sys/signal.h>
#include <err.h>
#include <inttypes.h>
#include <kvm.h>
#include <stdio.h>
#include <stdlib.h>

#include <defs.h>
#include <frame-unwind.h>

#include "kgdb.h"

static uintptr_t dumppcb;
static int dumptid;

static struct kthr *first;
struct kthr *curkthr;

static uintptr_t
lookup(const char *sym)
{
	struct nlist nl[2];

	nl[0].n_name = (char *)(uintptr_t)sym;
	nl[1].n_name = NULL;
	if (kvm_nlist(kvm, nl) != 0) {
		warnx("kvm_nlist(%s): %s", sym, kvm_geterr(kvm));
		return (0);
	}
	return (nl[0].n_value);
}

struct kthr *
kgdb_thr_first(void)
{
	return (first);
}

struct kthr *
kgdb_thr_init(void)
{
	struct proc p;
	struct thread td;
	struct kthr *kt;
	uintptr_t addr, paddr;

	addr = lookup("_allproc");
	if (addr == 0)
		return (NULL);
	kvm_read(kvm, addr, &paddr, sizeof(paddr));

	dumppcb = lookup("_dumppcb");
	if (dumppcb == 0)
		return (NULL);

	addr = lookup("_dumptid");
	if (addr != 0)
		kvm_read(kvm, addr, &dumptid, sizeof(dumptid));
	else
		dumptid = -1;

	while (paddr != 0) {
		if (kvm_read(kvm, paddr, &p, sizeof(p)) != sizeof(p))
			warnx("kvm_read: %s", kvm_geterr(kvm));
		addr = (uintptr_t)TAILQ_FIRST(&p.p_threads);
		while (addr != 0) {
			if (kvm_read(kvm, addr, &td, sizeof(td)) != sizeof(td))
				warnx("kvm_read: %s", kvm_geterr(kvm));
			kt = malloc(sizeof(*kt));
			kt->next = first;
			kt->kaddr = addr;
			kt->pcb = (td.td_tid == dumptid) ? dumppcb :
			    (uintptr_t)td.td_pcb;
			kt->kstack = td.td_kstack;
			kt->tid = td.td_tid;
			kt->pid = p.p_pid;
			kt->paddr = paddr;
			first = kt;
			addr = (uintptr_t)TAILQ_NEXT(&td, td_plist);
		}
		paddr = (uintptr_t)LIST_NEXT(&p, p_list);
	}
	curkthr = kgdb_thr_lookup_tid(dumptid);
	if (curkthr == NULL)
		curkthr = first;
	return (first);
}

struct kthr *
kgdb_thr_lookup_tid(int tid)
{
	struct kthr *kt;

	kt = first;
	while (kt != NULL && kt->tid != tid)
		kt = kt->next;
	return (kt);
}

struct kthr *
kgdb_thr_lookup_taddr(uintptr_t taddr)
{
	struct kthr *kt;

	kt = first;
	while (kt != NULL && kt->kaddr != taddr)
		kt = kt->next;
	return (kt);
}

struct kthr *
kgdb_thr_lookup_pid(int pid)
{
	struct kthr *kt;

	kt = first;
	while (kt != NULL && kt->pid != pid)
		kt = kt->next;
	return (kt);
}

struct kthr *
kgdb_thr_lookup_paddr(uintptr_t paddr)
{
	struct kthr *kt;

	kt = first;
	while (kt != NULL && kt->paddr != paddr)
		kt = kt->next;
	return (kt);
}

struct kthr *
kgdb_thr_next(struct kthr *kt)
{
	return (kt->next);
}

struct kthr *
kgdb_thr_select(struct kthr *kt)
{
	struct kthr *pcur;

	pcur = curkthr;
	curkthr = kt;
	return (pcur);
}

char *
kgdb_thr_extra_thread_info(int tid)
{
	struct kthr *kt;
	struct proc *p;
	static char comm[MAXCOMLEN + 1];

	kt = kgdb_thr_lookup_tid(tid);
	if (kt == NULL)
		return (NULL);
	p = (struct proc *)kt->paddr;
	if (kvm_read(kvm, (uintptr_t)&p->p_comm[0], &comm, sizeof(comm)) !=
	    sizeof(comm))
		return (NULL);

	return (comm);
}
