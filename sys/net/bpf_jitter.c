/*-
 * Copyright (c) 2002 - 2003 NetGroup, Politecnico di Torino (Italy)
 * Copyright (c) 2005 Jung-uk Kim <jkim@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Politecnico di Torino nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS intERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_bpf.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/sysctl.h>

#include <net/bpf.h>
#include <net/bpf_jitter.h>

MALLOC_DEFINE(M_BPFJIT, "BPF_JIT", "BPF JIT compiler");

bpf_filter_func	bpf_jit_compile(struct bpf_insn *, u_int, int *);

SYSCTL_NODE(_net, OID_AUTO, bpf_jitter, CTLFLAG_RW, 0, "BPF JIT compiler");
int bpf_jitter_enable = 1;
SYSCTL_INT(_net_bpf_jitter, OID_AUTO, enable, CTLFLAG_RW,
    &bpf_jitter_enable, 0, "enable BPF JIT compiler");

bpf_jit_filter *
bpf_jitter(struct bpf_insn *fp, int nins)
{
	bpf_jit_filter *filter;

	/* Allocate the filter structure */
	filter = (struct bpf_jit_filter *)malloc(sizeof(struct bpf_jit_filter),
	    M_BPFJIT, M_NOWAIT);
	if (filter == NULL)
		return NULL;

	/* Allocate the filter's memory */
	filter->mem = (int *)malloc(BPF_MEMWORDS * sizeof(int),
	    M_BPFJIT, M_NOWAIT);
	if (filter->mem == NULL) {
		free(filter, M_BPFJIT);
		return NULL;
	}

	/* Create the binary */
	if ((filter->func = bpf_jit_compile(fp, nins, filter->mem)) == NULL) {
		free(filter->mem, M_BPFJIT);
		free(filter, M_BPFJIT);
		return NULL;
	}

	return filter;
}

void
bpf_destroy_jit_filter(bpf_jit_filter *filter)
{

	free(filter->mem, M_BPFJIT);
	free(filter->func, M_BPFJIT);
	free(filter, M_BPFJIT);
}
