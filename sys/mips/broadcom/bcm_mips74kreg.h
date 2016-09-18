/*-
 * Copyright (c) 2016 Landon Fuller <landonf@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    similar to the "NO WARRANTY" disclaimer below ("Disclaimer") and any
 *    redistribution must be conditioned upon including a substantially
 *    similar Disclaimer requirement for further binary redistribution.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF NONINFRINGEMENT, MERCHANTIBILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGES.
 * 
 * $FreeBSD$
 */

#ifndef	_MIPS_BROADCOM_MIPS74KREG_H_
#define	_MIPS_BROADCOM_MIPS74KREG_H_

#define	BCM_MIPS74K_CORECTL		0x00	/**< core control */
#define	BCM_MIPS74K_EXCBASE		0x04	/**< exception base */

#define	BCM_MIPS74K_BIST_STATUS		0x0C	/**< built-in self-test status */
#define	BCM_MIPS74K_INTR_STATUS		0x10	/**< interrupt status */

/* INTR(0-5)_MASK map bcma(4) OOB interrupt bus lines to MIPS hardware
 * interrupts. */
#define	BCM_MIPS74K_INTR0_SEL		0x14	/**< IRQ0 OOBSEL mask */
#define	BCM_MIPS74K_INTR1_SEL		0x18	/**< IRQ1 OOBSEL mask */
#define	BCM_MIPS74K_INTR2_SEL		0x1C	/**< IRQ2 OOBSEL mask */
#define	BCM_MIPS74K_INTR3_SEL		0x20	/**< IRQ3 OOBSEL mask */
#define	BCM_MIPS74K_INTR4_SEL		0x24	/**< IRQ4 OOBSEL mask */
#define	BCM_MIPS74K_INTR5_SEL		0x28	/**< IRQ5 OOBSEL mask */

#define	BCM_MIPS74K_INTR_SEL(_intr)	\
	(BCM_MIPS74K_INTR0_SEL + ((_intr) * 4))

#define	BCM_MIPS74K_NMI_MASK		0x2C	/**< nmi mask */

#define	BCM_MIPS74K_GPIO_SEL		0x40	/**< gpio select */
#define	BCM_MIPS74K_GPIO_OUT		0x44	/**< gpio output enable */
#define	BCM_MIPS74K_GPIO_EN		0x48	/**< gpio enable */


#define	BCM_MIPS74K_TIMER_IVEC		31	/**< MIPS timer OOBSEL value */

#endif /* _MIPS_BROADCOM_MIPS74KREG_H_ */
