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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <kvm.h>

#include "kgdb.h"

#include <defs.h>
#include <gdbthread.h>
#include <inferior.h>
#include <target.h>

static struct target_ops kgdb_trgt_ops;

static char *
kgdb_trgt_extra_thread_info(struct thread_info *ti __unused)
{
	return (NULL);
}

static void
kgdb_trgt_find_new_threads(void)
{
}

static char *
kgdb_trgt_pid_to_str(ptid_t ptid)
{
	static char buf[16];

	snprintf(buf, sizeof(buf), "TID %d", ptid_get_pid(ptid));
	return (buf);
}

static int
kgdb_trgt_thread_alive(ptid_t ptid)
{
	return (kgdb_thr_lookup(ptid_get_pid(ptid)) != NULL);
}

static int
kgdb_trgt_xfer_memory(CORE_ADDR memaddr, char *myaddr, int len, int write,
    struct mem_attrib *attrib __unused, struct target_ops *target __unused)
{
	if (len == 0)
		return (0);

	if (!write)
		return (kvm_read(kvm, memaddr, myaddr, len));
	else
		return (kvm_write(kvm, memaddr, myaddr, len));
}

void
kgdb_target(void)
{
	struct kthr *kt;
	struct thread_info *ti;

	kgdb_trgt_ops.to_magic = OPS_MAGIC;
	kgdb_trgt_ops.to_shortname = "kernel";
	kgdb_trgt_ops.to_longname = "kernel core files.";
	kgdb_trgt_ops.to_doc = "Kernel core files.";
	kgdb_trgt_ops.to_stratum = thread_stratum;
	kgdb_trgt_ops.to_has_memory = 1;
	kgdb_trgt_ops.to_has_registers = 1;
	kgdb_trgt_ops.to_has_stack = 1;

	kgdb_trgt_ops.to_extra_thread_info = kgdb_trgt_extra_thread_info;
	kgdb_trgt_ops.to_fetch_registers = kgdb_trgt_fetch_registers;
	kgdb_trgt_ops.to_find_new_threads = kgdb_trgt_find_new_threads;
	kgdb_trgt_ops.to_pid_to_str = kgdb_trgt_pid_to_str;
	kgdb_trgt_ops.to_store_registers = kgdb_trgt_store_registers;
	kgdb_trgt_ops.to_thread_alive = kgdb_trgt_thread_alive;
	kgdb_trgt_ops.to_xfer_memory = kgdb_trgt_xfer_memory;
	add_target(&kgdb_trgt_ops);
	push_target(&kgdb_trgt_ops);

	kt = kgdb_thr_first();
	while (kt != NULL) {
		ti = add_thread(ptid_build(kt->tid, 0, 0));
		kt = kgdb_thr_next(kt);
	}
	inferior_ptid = ptid_build(curkthr->tid, 0, 0);
}
