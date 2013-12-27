/*-
 * Copyright (c) 2011 NetApp, Inc.
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
 * THIS SOFTWARE IS PROVIDED BY NETAPP, INC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL NETAPP, INC OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _VLAPIC_H_
#define	_VLAPIC_H_

struct vm;
enum x2apic_state;

int vlapic_write(struct vlapic *vlapic, uint64_t offset, uint64_t data,
    bool *retu);
int vlapic_read(struct vlapic *vlapic, uint64_t offset, uint64_t *data,
    bool *retu);

/*
 * Returns a vector between 32 and 255 if an interrupt is pending in the
 * IRR that can be delivered based on the current state of ISR and TPR.
 *
 * Note that the vector does not automatically transition to the ISR as a
 * result of calling this function.
 *
 * Returns -1 if there is no eligible vector that can be delivered to the
 * guest at this time.
 */
int vlapic_pending_intr(struct vlapic *vlapic);

/*
 * Transition 'vector' from IRR to ISR. This function is called with the
 * vector returned by 'vlapic_pending_intr()' when the guest is able to
 * accept this interrupt (i.e. RFLAGS.IF = 1 and no conditions exist that
 * block interrupt delivery).
 */
void vlapic_intr_accepted(struct vlapic *vlapic, int vector);

void vlapic_set_intr_ready(struct vlapic *vlapic, int vector, bool level);
void vlapic_set_error(struct vlapic *vlapic, uint32_t mask);
void vlapic_fire_cmci(struct vlapic *vlapic);
int vlapic_trigger_lvt(struct vlapic *vlapic, int vector);

uint64_t vlapic_get_apicbase(struct vlapic *vlapic);
void vlapic_set_apicbase(struct vlapic *vlapic, uint64_t val);
void vlapic_set_x2apic_state(struct vm *vm, int vcpuid, enum x2apic_state s);
bool vlapic_enabled(struct vlapic *vlapic);

void vlapic_deliver_intr(struct vm *vm, bool level, uint32_t dest, bool phys,
    int delmode, int vec);
void vlapic_post_intr(struct vlapic *vlapic, int hostcpu);

/* APIC write handlers */
void vlapic_id_write_handler(struct vlapic *vlapic);
void vlapic_ldr_write_handler(struct vlapic *vlapic);
void vlapic_dfr_write_handler(struct vlapic *vlapic);
void vlapic_svr_write_handler(struct vlapic *vlapic);
#endif	/* _VLAPIC_H_ */
