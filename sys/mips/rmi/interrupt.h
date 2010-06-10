/*-
 * Copyright (c) 2003-2009 RMI Corporation
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
 * 3. Neither the name of RMI Corporation, nor the names of its contributors,
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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
 *__FBSDID("$FreeBSD$")
 * RMI_BSD */
#ifndef _RMI_INTERRUPT_H_
#define _RMI_INTERRUPT_H_

/* Defines for the IRQ numbers */

#define IRQ_IPI			41  /* 8-39 are mapped by PIC intr 0-31 */
#define IRQ_MSGRING             6
#define IRQ_TIMER               7

/*
 * XLR needs custom pre and post handlers for PCI/PCI-e interrupts
 * XXX: maybe follow i386 intsrc model
 */
void xlr_cpu_establish_hardintr(const char *, driver_filter_t *,
    driver_intr_t *, void *, int, int, void **, void (*)(void *),
    void (*)(void *), void (*)(void *), int (*)(void *, u_char));
void xlr_mask_hard_irq(void *);
void xlr_unmask_hard_irq(void *);

#endif				/* _RMI_INTERRUPT_H_ */
