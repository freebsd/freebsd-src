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
 
/*
 * Map of APIC Registers:       Offset  Description          		 	Access
 */
#define APIC_OFFSET_ID 		0x20    // Local APIC ID               		R/W
#define APIC_OFFSET_VER 	0x30    // Local APIC Version              	R
#define APIC_OFFSET_TPR 	0x80    // Task Priority Register          	R/W
#define APIC_OFFSET_APR 	0x90    // Arbitration Priority Register   	R
#define APIC_OFFSET_PPR 	0xA0    // Processor Priority Register     	R
#define APIC_OFFSET_EOI 	0xB0    // EOI Register                    	W
#define APIC_OFFSET_RRR 	0xC0    // Remote read                     	R
#define APIC_OFFSET_LDR 	0xD0    // Logical Destination             	R/W
#define APIC_OFFSET_DFR 	0xE0    // Destination Format Register     	0..27 R;  28..31 R/W
#define APIC_OFFSET_SVR 	0xF0    // Spurious Interrupt Vector Reg.  	0..3  R;  4..9   R/W
#define APIC_OFFSET_ISR0 	0x100   // ISR  000-031                    	R
#define APIC_OFFSET_ISR1 	0x110   // ISR  032-063                    	R
#define APIC_OFFSET_ISR2 	0x120   // ISR  064-095                    	R
#define APIC_OFFSET_ISR3 	0x130   // ISR  095-128                    	R
#define APIC_OFFSET_ISR4 	0x140   // ISR  128-159                    	R
#define APIC_OFFSET_ISR5 	0x150   // ISR  160-191                    	R
#define APIC_OFFSET_ISR6 	0x160   // ISR  192-223                    	R
#define APIC_OFFSET_ISR7 	0x170   // ISR  224-255                    	R
#define APIC_OFFSET_TMR0 	0x180   // TMR  000-031                    	R
#define APIC_OFFSET_TMR1 	0x190   // TMR  032-063                    	R
#define APIC_OFFSET_TMR2 	0x1A0   // TMR  064-095                    	R
#define APIC_OFFSET_TMR3 	0x1B0   // TMR  095-128                    	R
#define APIC_OFFSET_TMR4 	0x1C0   // TMR  128-159                    	R
#define APIC_OFFSET_TMR5 	0x1D0   // TMR  160-191                    	R
#define APIC_OFFSET_TMR6 	0x1E0   // TMR  192-223                    	R
#define APIC_OFFSET_TMR7 	0x1F0   // TMR  224-255                    	R
#define APIC_OFFSET_IRR0 	0x200   // IRR  000-031                    	R
#define APIC_OFFSET_IRR1 	0x210   // IRR  032-063                    	R
#define APIC_OFFSET_IRR2 	0x220   // IRR  064-095                    	R
#define APIC_OFFSET_IRR3 	0x230   // IRR  095-128                    	R
#define APIC_OFFSET_IRR4 	0x240   // IRR  128-159                    	R
#define APIC_OFFSET_IRR5 	0x250   // IRR  160-191                    	R
#define APIC_OFFSET_IRR6 	0x260   // IRR  192-223                    	R
#define APIC_OFFSET_IRR7 	0x270   // IRR  224-255                    	R
#define APIC_OFFSET_ESR		0x280   // Error Status Register           	R
#define APIC_OFFSET_ICR_LOW 	0x300   // Interrupt Command Reg. (0-31)   	R/W
#define APIC_OFFSET_ICR_HI 	0x310   // Interrupt Command Reg. (32-63)  	R/W
#define APIC_OFFSET_TIMER_LVT 	0x320   // Local Vector Table (Timer)      	R/W
#define APIC_OFFSET_THERM_LVT 	0x330   // Local Vector Table (Thermal)    	R/W (PIV+)
#define APIC_OFFSET_PERF_LVT 	0x340   // Local Vector Table (Performance) 	R/W (P6+)
#define APIC_OFFSET_LINT0_LVT 	0x350   // Local Vector Table (LINT0)      	R/W
#define APIC_OFFSET_LINT1_LVT 	0x360 	// Local Vector Table (LINT1)      	R/W
#define APIC_OFFSET_ERROR_LVT 	0x370   // Local Vector Table (ERROR)      	R/W
#define APIC_OFFSET_ICR 	0x380   // Initial Count Reg. for Timer    	R/W
#define APIC_OFFSET_CCR 	0x390   // Current Count of Timer          	R
#define APIC_OFFSET_DCR 	0x3E0   // Timer Divide Configuration Reg. 	R/W

/*
 * 16 priority levels with at most one vector injected per level.
 */
#define	ISRVEC_STK_SIZE		(16 + 1)

enum x2apic_state;

struct vlapic *vlapic_init(struct vm *vm, int vcpuid);
void vlapic_cleanup(struct vlapic *vlapic);
int vlapic_write(struct vlapic *vlapic, uint64_t offset, uint64_t data,
    bool *retu);
int vlapic_read(struct vlapic *vlapic, uint64_t offset, uint64_t *data,
    bool *retu);
int vlapic_pending_intr(struct vlapic *vlapic);
void vlapic_intr_accepted(struct vlapic *vlapic, int vector);
void vlapic_set_intr_ready(struct vlapic *vlapic, int vector, bool level);

uint64_t vlapic_get_apicbase(struct vlapic *vlapic);
void vlapic_set_apicbase(struct vlapic *vlapic, uint64_t val);
void vlapic_set_x2apic_state(struct vm *vm, int vcpuid, enum x2apic_state s);
bool vlapic_enabled(struct vlapic *vlapic);

void vlapic_deliver_intr(struct vm *vm, bool level, uint32_t dest, bool phys,
    int delmode, int vec);
#endif	/* _VLAPIC_H_ */
