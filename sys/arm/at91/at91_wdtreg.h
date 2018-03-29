/*-
 * Copyright (c) 2005 Gallon Sylvestre.  All rights reserved.
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

/*
 * $FreeBSD$
 */

#ifndef ARM_AT91_AT91WDTREG_H
#define ARM_AT91_AT91WDTREG_H

#ifndef WDT_CLOCK
#define WDT_CLOCK (32768)
#endif
#define WDT_DIV (128)	/* Clock is slow clock / 128 */

#define WDT_CR		0x0 /* Control Register */
#define WDT_MR		0x4 /* Mode Register */
#define WDT_SR		0x8 /* Status Register */

/* WDT_CR */
#define WDT_KEY		(0xa5<<24)
#define WDT_WDRSTT	0x1

/* WDT_MR */
#define WDT_WDV(x)	(x & 0xfff) /* counter value*/
#define WDT_WDFIEN	(1<<12) /* enable interrupt */
#define WDT_WDRSTEN	(1<<13) /* enable reset */
#define WDT_WDRPROC	(1<<14) /* processor reset */
#define WDT_WDDIS	(1<<15) /* disable */
#define WDT_WDD(x)	((x & 0xfff) << 16) /* delta value */
#define WDT_WDDBGHLT	(1<<28) /* halt in debug */
#define WDT_WDIDLEHLT	(1<<29) /* halt in idle */

/* WDT_SR */
#define WDT_WDUNF	0x1
#define WDT_WDERR	0x2

#endif /* ARM_AT91_AT91WDTREG_H */
