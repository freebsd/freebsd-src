/*-
 * Copyright (c) 2016 Microsoft Corp.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
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

#ifndef _HYPERV_VAR_H_
#define _HYPERV_VAR_H_

extern u_int	hyperv_recommends;

struct hypercall_ctx {
    void            *hc_addr;
    vm_paddr_t      hc_paddr;
};

uint64_t	hypercall_post_message(bus_addr_t msg_paddr);
uint64_t	hypercall_signal_event(bus_addr_t monprm_paddr);
uint64_t	hypercall_do_md(uint64_t input, uint64_t in_addr,
		    uint64_t out_addr);
struct hv_vpset;
struct vmbus_softc;

uint64_t
hv_do_rep_hypercall(uint16_t code, uint16_t rep_count, uint16_t varhead_size,
    uint64_t input, uint64_t output);
int
hv_cpumask_to_vpset(struct hv_vpset *vpset, const cpuset_t *cpus,
    struct vmbus_softc *sc);
#endif	/* !_HYPERV_VAR_H_ */
