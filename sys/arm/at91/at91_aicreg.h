/*-
 * Copyright (c) 2009 Sylvestre Gallon. All rights reserved.
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
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* $FreeBSD$ */

#ifndef ARM_AT91_AT91_AICREG_H
#define ARM_AT91_AT91_AICREG_H

/* Interrupt Controller */
#define IC_SMR			(0) /* Source mode register */
#define IC_SVR			(128) /* Source vector register */
#define IC_IVR			(256) /* IRQ vector register */
#define IC_FVR			(260) /* FIQ vector register */
#define IC_ISR			(264) /* Interrupt status register */
#define IC_IPR			(268) /* Interrupt pending register */
#define IC_IMR			(272) /* Interrupt status register */
#define IC_CISR			(276) /* Core interrupt status register */
#define IC_IECR			(288) /* Interrupt enable command register */
#define IC_IDCR			(292) /* Interrupt disable command register */
#define IC_ICCR			(296) /* Interrupt clear command register */
#define IC_ISCR			(300) /* Interrupt set command register */
#define IC_EOICR		(304) /* End of interrupt command register */
#define IC_SPU			(308) /* Spurious vector register */
#define IC_DCR			(312) /* Debug control register */
#define IC_FFER			(320) /* Fast forcing enable register */
#define IC_FFDR			(324) /* Fast forcing disable register */
#define IC_FFSR			(328) /* Fast forcing status register */

#endif /*ARM_AT91_AT91_AICREG_H*/
