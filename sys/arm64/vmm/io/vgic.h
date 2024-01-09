/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023 Arm Ltd
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
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _VGIC_H_
#define	_VGIC_H_

struct hyp;
struct hypctx;
struct vm_vgic_descr;

extern device_t vgic_dev;

bool vgic_present(void);
void vgic_init(void);
int vgic_attach_to_vm(struct hyp *hyp, struct vm_vgic_descr *descr);
void vgic_detach_from_vm(struct hyp *hyp);
void vgic_vminit(struct hyp *hyp);
void vgic_cpuinit(struct hypctx *hypctx);
void vgic_cpucleanup(struct hypctx *hypctx);
void vgic_vmcleanup(struct hyp *hyp);
int vgic_max_cpu_count(struct hyp *hyp);
bool vgic_has_pending_irq(struct hypctx *hypctx);
int vgic_inject_irq(struct hyp *hyp, int vcpuid, uint32_t irqid, bool level);
int vgic_inject_msi(struct hyp *hyp, uint64_t msg, uint64_t addr);
void vgic_flush_hwstate(struct hypctx *hypctx);
void vgic_sync_hwstate(struct hypctx *hypctx);

#endif /* _VGIC_H_ */
