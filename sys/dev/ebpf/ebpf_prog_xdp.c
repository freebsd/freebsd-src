/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020 Ankur Kothiwal
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

#include <sys/types.h>
#include <sys/param.h>
#include <sys/event.h>
#include <sys/smp.h>
#include <sys/lock.h>
#include <sys/resource.h>
#include <sys/sx.h>

#include <sys/ebpf.h>
#include <sys/ebpf_elf.h>
#include <sys/ebpf_param.h>
#include <sys/ebpf_probe.h>
#include <dev/ebpf/ebpf_dev_freebsd.h>
#include <dev/ebpf/ebpf_internal.h>
#include <dev/ebpf/ebpf_dev_probe.h>


static void
xdp_init(void){}

static void
xdp_deinit(void){}

static int
xdp_reserve_cpu(struct ebpf_vm_state *vm_state)
{
	critical_enter();
	vm_state->cpu = curcpu;
	return (0);
}

static void
xdp_release_cpu(struct ebpf_vm_state *vm_state)
{
	critical_exit();
}

static bool
xdp_is_map_usable(struct ebpf_map_type *emt)
{
	return (1);
}

static bool
xdp_is_helper_usable(struct ebpf_helper_type *eht)
{
	switch (eht->id) {
		case EBPF_FUNC_map_update_elem:
		case EBPF_FUNC_map_lookup_elem:
		case EBPF_FUNC_map_delete_elem:
		case EBPF_FUNC_map_path_lookup:
		case EBPF_FUNC_map_enqueue:
		case EBPF_FUNC_map_dequeue:

		return (true);
	}

	return (false);
}

const struct ebpf_probe_ops xdp_probe_ops = {
	.init = xdp_init,
	.fini = xdp_deinit,
	.reserve_cpu = xdp_reserve_cpu,
	.release_cpu = xdp_release_cpu,
};

const struct ebpf_prog_type ept_xdp = {
	.name = "xdp",
	.ops = {
		.is_map_usable = xdp_is_map_usable,
		.is_helper_usable = xdp_is_helper_usable,
	},
};
