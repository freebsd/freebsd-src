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
 * NETLOGIC_BSD
 * $FreeBSD$
 */

#ifndef __NLM_XLP_H__
#define __NLM_XLP_H__
#include <mips/nlm/hal/pic.h>

#define PIC_UART_0_IRQ	9
#define PIC_UART_1_IRQ	10

#define PIC_PCIE_0_IRQ	11
#define PIC_PCIE_1_IRQ	12
#define PIC_PCIE_2_IRQ	13
#define PIC_PCIE_3_IRQ	14

#define PIC_EHCI_0_IRQ	39 
#define PIC_EHCI_1_IRQ	42 
#define PIC_MMC_IRQ		43

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

static __inline__ int
xlp_irt_to_irq(int irt)
{
	switch (irt) {
		case PIC_IRT_MMC_INDEX :
			return PIC_MMC_IRQ;
		case PIC_IRT_EHCI_0_INDEX :
 			return PIC_EHCI_0_IRQ;
 		case PIC_IRT_EHCI_1_INDEX :
 			return PIC_EHCI_1_IRQ;
		case PIC_IRT_UART_0_INDEX :
			return PIC_UART_0_IRQ;
		case PIC_IRT_UART_1_INDEX :
			return PIC_UART_1_IRQ;
		case PIC_IRT_PCIE_LINK_0_INDEX :
			return PIC_PCIE_0_IRQ;
		case PIC_IRT_PCIE_LINK_1_INDEX :
			return PIC_PCIE_1_IRQ;
		case PIC_IRT_PCIE_LINK_2_INDEX :
			return PIC_PCIE_2_IRQ;
		case PIC_IRT_PCIE_LINK_3_INDEX :
			return PIC_PCIE_3_IRQ;
		default: panic("Bad IRT %d\n", irt);
	}
}

static __inline__ int
xlp_irq_to_irt(int irq)
{
	switch (irq) {
		case PIC_MMC_IRQ :
			return PIC_IRT_MMC_INDEX;
 		case PIC_EHCI_0_IRQ :
 			return PIC_IRT_EHCI_0_INDEX;
 		case PIC_EHCI_1_IRQ :
 			return PIC_IRT_EHCI_1_INDEX;
		case PIC_UART_0_IRQ :
			return PIC_IRT_UART_0_INDEX;
		case PIC_UART_1_IRQ :
			return PIC_IRT_UART_1_INDEX;
		case PIC_PCIE_0_IRQ :
			return PIC_IRT_PCIE_LINK_0_INDEX;
		case PIC_PCIE_1_IRQ :
			return PIC_IRT_PCIE_LINK_1_INDEX;
		case PIC_PCIE_2_IRQ :
			return PIC_IRT_PCIE_LINK_2_INDEX;
		case PIC_PCIE_3_IRQ :
			return PIC_IRT_PCIE_LINK_3_INDEX;
		default: panic("Bad IRQ %d\n", irq);
	}
}

static __inline__ int
xlp_irq_is_picintr(int irq)
{
	switch (irq) {
		case PIC_MMC_IRQ : return 1;
 		case PIC_EHCI_0_IRQ : return 1;
 		case PIC_EHCI_1_IRQ : return 1;
		case PIC_UART_0_IRQ : return 1;
		case PIC_UART_1_IRQ : return 1;
		case PIC_PCIE_0_IRQ : return 1;
		case PIC_PCIE_1_IRQ : return 1;
		case PIC_PCIE_2_IRQ : return 1;
		case PIC_PCIE_3_IRQ : return 1;
		default: return 0;
	}
}
#endif /* LOCORE */
#endif /* __NLM_XLP_H__ */
