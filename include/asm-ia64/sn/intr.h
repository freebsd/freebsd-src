/* $Id$
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1992 - 1997, 2000-2003 Silicon Graphics, Inc. All rights reserved.
 */
#ifndef _ASM_IA64_SN_INTR_H
#define _ASM_IA64_SN_INTR_H

#include <linux/config.h>
#include <asm/sn/sn2/intr.h>

extern void sn_send_IPI_phys(long, int, int);

#define CPU_VECTOR_TO_IRQ(cpuid,vector) (vector)
#define SN_CPU_FROM_IRQ(irq)	(0)
#define SN_IVEC_FROM_IRQ(irq)	(irq)

#endif /* _ASM_IA64_SN_INTR_H */
