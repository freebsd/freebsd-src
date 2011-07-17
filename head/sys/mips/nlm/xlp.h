/*-
 * Copyright 2003-2011 Netlogic Microsystems (Netlogic). All rights
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY Netlogic Microsystems ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL NETLOGIC OR CONTRIBUTORS BE 
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 * NETLOGIC_BSD */

#ifndef __NLM_XLP_H__
#define __NLM_XLP_H__
#include <mips/nlm/hal/pic.h>

#define XLP_PIC_IRT_UART0_IRQ	9
#define XLP_PIC_IRT_UART1_IRQ	10

#define XLP_PIC_IRT_PCIE0_IRQ	11
#define XLP_PIC_IRT_PCIE1_IRQ	12
#define XLP_PIC_IRT_PCIE2_IRQ	13
#define XLP_PIC_IRT_PCIE3_IRQ	14

#define XLP_PIC_IRT_EHCI0_IRQ	39 
#define XLP_PIC_IRT_EHCI1_IRQ	42 
#define XLP_PIC_IRT_MMC_IRQ	43


#ifndef LOCORE
/*
 * FreeBSD can be started with few threads and cores turned off,
 * so have a hardware thread id to FreeBSD cpuid mapping.
 */
extern int xlp_ncores;
extern int xlp_threads_per_core;
extern uint32_t xlp_hw_thread_mask;
extern int xlp_cpuid_to_hwtid[];
extern int xlp_hwtid_to_cpuid[];
#ifdef SMP
extern void xlp_enable_threads(int code);
#endif

extern uint64_t xlp_pic_base; /* TODO just for node 0 now */

static __inline__ int
xlp_irt_to_irq(int irt)
{
	switch (irt) {
		case XLP_PIC_IRT_MMC_INDEX :
			return XLP_PIC_IRT_MMC_IRQ;
		case XLP_PIC_IRT_EHCI0_INDEX :
 			return XLP_PIC_IRT_EHCI0_IRQ;
 		case XLP_PIC_IRT_EHCI1_INDEX :
 			return XLP_PIC_IRT_EHCI1_IRQ;
		case XLP_PIC_IRT_UART0_INDEX :
			return XLP_PIC_IRT_UART0_IRQ;
		case XLP_PIC_IRT_UART1_INDEX :
			return XLP_PIC_IRT_UART1_IRQ;
		case XLP_PIC_IRT_PCIE_LINK0_INDEX :
			return XLP_PIC_IRT_PCIE0_IRQ;
		case XLP_PIC_IRT_PCIE_LINK1_INDEX :
			return XLP_PIC_IRT_PCIE1_IRQ;
		case XLP_PIC_IRT_PCIE_LINK2_INDEX :
			return XLP_PIC_IRT_PCIE2_IRQ;
		case XLP_PIC_IRT_PCIE_LINK3_INDEX :
			return XLP_PIC_IRT_PCIE3_IRQ;
		default: panic("Bad IRT %d\n", irt);
	}
}

static __inline__ int
xlp_irq_to_irt(int irq)
{
	switch (irq) {
		case XLP_PIC_IRT_MMC_IRQ :
			return XLP_PIC_IRT_MMC_INDEX;
 		case XLP_PIC_IRT_EHCI0_IRQ :
 			return XLP_PIC_IRT_EHCI0_INDEX;
 		case XLP_PIC_IRT_EHCI1_IRQ :
 			return XLP_PIC_IRT_EHCI1_INDEX;
		case XLP_PIC_IRT_UART0_IRQ :
			return XLP_PIC_IRT_UART0_INDEX;
		case XLP_PIC_IRT_UART1_IRQ :
			return XLP_PIC_IRT_UART1_INDEX;
		case XLP_PIC_IRT_PCIE0_IRQ :
			return XLP_PIC_IRT_PCIE_LINK0_INDEX;
		case XLP_PIC_IRT_PCIE1_IRQ :
			return XLP_PIC_IRT_PCIE_LINK1_INDEX;
		case XLP_PIC_IRT_PCIE2_IRQ :
			return XLP_PIC_IRT_PCIE_LINK2_INDEX;
		case XLP_PIC_IRT_PCIE3_IRQ :
			return XLP_PIC_IRT_PCIE_LINK3_INDEX;
		default: panic("Bad IRQ %d\n", irq);
	}
}

static __inline__ int
xlp_irq_is_picintr(int irq)
{
	switch (irq) {
		case XLP_PIC_IRT_MMC_IRQ : return 1;
 		case XLP_PIC_IRT_EHCI0_IRQ : return 1;
 		case XLP_PIC_IRT_EHCI1_IRQ : return 1;
		case XLP_PIC_IRT_UART0_IRQ : return 1;
		case XLP_PIC_IRT_UART1_IRQ : return 1;
		case XLP_PIC_IRT_PCIE0_IRQ : return 1;
		case XLP_PIC_IRT_PCIE1_IRQ : return 1;
		case XLP_PIC_IRT_PCIE2_IRQ : return 1;
		case XLP_PIC_IRT_PCIE3_IRQ : return 1;
		default: return 0;
	}
}
#endif /* LOCORE */
#endif /* __NLM_XLP_H__ */
