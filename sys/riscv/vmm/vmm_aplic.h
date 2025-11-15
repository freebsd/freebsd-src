/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 Ruslan Bukin <br@bsdpad.com>
 *
 * This software was developed by the University of Cambridge Computer
 * Laboratory (Department of Computer Science and Technology) under Innovate
 * UK project 105694, "Digital Security by Design (DSbD) Technology Platform
 * Prototype".
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
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _VMM_APLIC_H_
#define _VMM_APLIC_H_

struct hyp;
struct hypctx;
struct vm_aplic_descr;

int aplic_attach_to_vm(struct hyp *hyp, struct vm_aplic_descr *descr);
void aplic_detach_from_vm(struct hyp *hyp);
int aplic_inject_irq(struct hyp *hyp, int vcpuid, uint32_t irqid, bool level);
int aplic_inject_msi(struct hyp *hyp, uint64_t msg, uint64_t addr);
void aplic_vminit(struct hyp *hyp);
void aplic_vmcleanup(struct hyp *hyp);
int aplic_check_pending(struct hypctx *hypctx);

void aplic_cpuinit(struct hypctx *hypctx);
void aplic_cpucleanup(struct hypctx *hypctx);
void aplic_flush_hwstate(struct hypctx *hypctx);
void aplic_sync_hwstate(struct hypctx *hypctx);

#endif /* !_VMM_APLIC_H_ */
