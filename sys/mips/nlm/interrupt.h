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

#ifndef _RMI_INTERRUPT_H_
#define _RMI_INTERRUPT_H_

/* Defines for the IRQ numbers */

#define	IRQ_IPI			41  /* 8-39 are used by PIC interrupts */
#define	IRQ_MSGRING		6
#define	IRQ_TIMER		7

#define	PIC_IRQ_BASE		8
#define	PIC_IRT_LAST_IRQ	39
#define	XLP_IRQ_IS_PICINTR(irq)	((irq) >= PIC_IRQ_BASE && \
				    (irq) <= PIC_IRT_LAST_IRQ)

#define	PIC_UART_0_IRQ		9
#define	PIC_PCIE_0_IRQ		11
#define	PIC_PCIE_1_IRQ		12
#define	PIC_PCIE_2_IRQ		13
#define	PIC_PCIE_3_IRQ		14
#define	PIC_EHCI_0_IRQ		16
#define	PIC_MMC_IRQ		21

/*
 * XLR needs custom pre and post handlers for PCI/PCI-e interrupts
 * XXX: maybe follow i386 intsrc model
 */
void xlp_establish_intr(const char *name, driver_filter_t filt,
    driver_intr_t handler, void *arg, int irq, int flags,
    void **cookiep, void (*busack)(int));
void xlp_enable_irq(int irq);

#endif				/* _RMI_INTERRUPT_H_ */
