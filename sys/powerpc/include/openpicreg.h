/*-
 * Copyright (c) 2000 Tsubai Masanari.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * from NetBSD: openpicreg.h,v 1.3 2001/08/30 03:08:52 briggs Exp
 * $FreeBSD: src/sys/powerpc/include/openpicreg.h,v 1.3.24.1 2008/11/25 02:59:29 kensmith Exp $
 */

/*
 * Size of OpenPIC register space
 */
#define	OPENPIC_SIZE			0x40000

/*
 * GLOBAL/TIMER register (IDU base + 0x1000)
 */

/* feature reporting reg 0 */
#define OPENPIC_FEATURE			0x1000
#define	OPENPIC_FEATURE_VERSION_MASK	0x000000ff
#define	OPENPIC_FEATURE_LAST_CPU_MASK	0x00001f00
#define	OPENPIC_FEATURE_LAST_CPU_SHIFT	8
#define	OPENPIC_FEATURE_LAST_IRQ_MASK	0x07ff0000
#define	OPENPIC_FEATURE_LAST_IRQ_SHIFT	16

/* global config reg 0 */
#define OPENPIC_CONFIG			0x1020
#define  OPENPIC_CONFIG_RESET			0x80000000
#define  OPENPIC_CONFIG_8259_PASSTHRU_DISABLE	0x20000000

/* interrupt configuration mode (direct or serial) */
#define OPENPIC_ICR			0x1030
#define  OPENPIC_ICR_SERIAL_MODE	(1 << 27)
#define  OPENPIC_ICR_SERIAL_RATIO_MASK	(0x7 << 28)
#define  OPENPIC_ICR_SERIAL_RATIO_SHIFT	28

/* vendor ID */
#define OPENPIC_VENDOR_ID		0x1080

/* processor initialization reg */
#define OPENPIC_PROC_INIT		0x1090

/* IPI vector/priority reg */
#define OPENPIC_IPI_VECTOR(ipi)		(0x10a0 + (ipi) * 0x10)

/* spurious intr. vector */
#define OPENPIC_SPURIOUS_VECTOR		0x10e0


/*
 * INTERRUPT SOURCE register (IDU base + 0x10000)
 */

/* interrupt vector/priority reg */
#ifndef OPENPIC_SRC_VECTOR
#define OPENPIC_SRC_VECTOR(irq)		(0x10000 + (irq) * 0x20)
#endif
#define  OPENPIC_SENSE_LEVEL			0x00400000
#define  OPENPIC_SENSE_EDGE			0x00000000
#define  OPENPIC_POLARITY_POSITIVE		0x00800000
#define  OPENPIC_POLARITY_NEGATIVE		0x00000000
#define  OPENPIC_IMASK				0x80000000
#define  OPENPIC_ACTIVITY			0x40000000
#define  OPENPIC_PRIORITY_MASK			0x000f0000
#define  OPENPIC_PRIORITY_SHIFT			16
#define  OPENPIC_VECTOR_MASK			0x000000ff

/* interrupt destination cpu */
#ifndef OPENPIC_IDEST
#define OPENPIC_IDEST(irq)		(0x10010 + (irq) * 0x20)
#endif

/*
 * PROCESSOR register (IDU base + 0x20000)
 */

/* IPI command reg */
#define OPENPIC_IPI(cpu, ipi)		(0x20040 + (cpu) * 0x1000 + (ipi))

/* current task priority reg */
#define OPENPIC_CPU_PRIORITY(cpu)	(0x20080 + (cpu) * 0x1000)
#define  OPENPIC_CPU_PRIORITY_MASK		0x0000000f

/* interrupt acknowledge reg */
#define OPENPIC_IACK(cpu)		(0x200a0 + (cpu) * 0x1000)

/* end of interrupt reg */
#define OPENPIC_EOI(cpu)		(0x200b0 + (cpu) * 0x1000)
