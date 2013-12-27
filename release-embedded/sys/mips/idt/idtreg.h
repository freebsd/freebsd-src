/*-
 * Copyright (C) 2007 by Oleksandr Tymoshenko. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR HIS RELATIVES BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF MIND, USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 *
 */
#ifndef __IDTREG_H__
#define __IDTREG_H__

/* Interrupt controller */
#define	IDT_BASE_ICU	0x18038000
#define		ICU_IPEND2	0x00
#define		ICU_ITEST2	0x04
#define		ICU_IMASK2	0x08
#define		ICU_IPEND3	0x0C	
#define		ICU_ITEST3	0x10
#define		ICU_IMASK3	0x14
#define		ICU_IPEND4	0x18	
#define		ICU_ITEST4	0x1c
#define		ICU_IMASK4	0x20
#define		ICU_IPEND5	0x24	
#define		ICU_ITEST5	0x28
#define		ICU_IMASK5	0x2c
#define		ICU_IPEND6	0x30	
#define		ICU_ITEST6	0x34
#define		ICU_IMASK6	0x38
#define		ICU_NMIPS	0x3c

#define	IDT_BASE_GPIO	0x18050000
#define		GPIO_FUNC	0x00
#define		GPIO_CFG	0x04
#define		GPIO_DATA	0x08
#define		GPIO_ILEVEL	0x0C
#define		GPIO_ISTAT	0x10
#define		GPIO_NMIEN	0x14

#define	IDT_BASE_UART0	0x18058000

/* PCI controller */
#define	IDT_BASE_PCI	0x18080000
#define		IDT_PCI_CNTL		0x00
#define			IDT_PCI_CNTL_EN		0x001
#define			IDT_PCI_CNTL_TNR	0x002
#define			IDT_PCI_CNTL_SCE	0x004
#define			IDT_PCI_CNTL_IEN	0x008
#define			IDT_PCI_CNTL_AAA	0x010
#define			IDT_PCI_CNTL_EAP	0x020
#define			IDT_PCI_CNTL_IGM	0x200
#define		IDT_PCI_STATUS		0x04
#define			IDT_PCI_STATUS_RIP	0x20000
#define		IDT_PCI_STATUS_MASK	0x08
#define		IDT_PCI_CFG_ADDR	0x0C
#define		IDT_PCI_CFG_DATA	0x10
/* LBA stuff */
#define		IDT_PCI_LBA0		0x14
#define		IDT_PCI_LBA0_CNTL	0x18
#define			IDT_PCI_LBA_MSI		0x01
#define			IDT_PCI_LBA_SIZE_1MB	(0x14 << 2)
#define			IDT_PCI_LBA_SIZE_2MB	(0x15 << 2)
#define			IDT_PCI_LBA_SIZE_4MB	(0x16 << 2)
#define			IDT_PCI_LBA_SIZE_8MB	(0x17 << 2)
#define			IDT_PCI_LBA_SIZE_16MB	(0x18 << 2)
#define			IDT_PCI_LBA_SIZE_32MB	(0x19 << 2)
#define			IDT_PCI_LBA_SIZE_64MB	(0x1A << 2)
#define			IDT_PCI_LBA_SIZE_128MB	(0x1B << 2)
#define			IDT_PCI_LBA_SIZE_256MB	(0x1C << 2)
#define			IDT_PCI_LBA_FE		0x80
#define			IDT_PCI_LBA_RT		0x100
#define		IDT_PCI_LBA0_MAP	0x1C
#define		IDT_PCI_LBA1		0x20
#define		IDT_PCI_LBA1_CNTL	0x24
#define		IDT_PCI_LBA1_MAP	0x28
#define		IDT_PCI_LBA2		0x2C
#define		IDT_PCI_LBA2_CNTL	0x30
#define		IDT_PCI_LBA2_MAP	0x34
#define		IDT_PCI_LBA3		0x38
#define		IDT_PCI_LBA3_CNTL	0x3C
#define		IDT_PCI_LBA3_MAP	0x40
/* decoupled registers */
#define		IDT_PCI_DAC		0x44
#define		IDT_PCI_DAS		0x48
#define		IDT_PCI_DASM		0x4C

#define		IDT_PCI_TC		0x5C
#define			IDT_PCI_TC_RTIMER	0x10
#define			IDT_PCI_TC_DTIMER	0x08
/* Messaging unit of PCI controller */
#define		IDT_PCI_IIC		0x8024
#define		IDT_PCI_IIM		0x8028
#define		IDT_PCI_OIC		0x8030
#define		IDT_PCI_OIM		0x8034

/* PCI-related stuff */
#define	IDT_PCIMEM0_BASE	0x50000000
#define	IDT_PCIMEM0_SIZE	0x01000000

#define	IDT_PCIMEM1_BASE	0x60000000
#define	IDT_PCIMEM1_SIZE	0x10000000

#define	IDT_PCIMEM2_BASE	0x18C00000
#define	IDT_PCIMEM2_SIZE	0x00400000

#define	IDT_PCIMEM3_BASE	0x18800000
#define	IDT_PCIMEM3_SIZE	0x00100000

/* Interrupts-related stuff */
#define	IRQ_BASE	8
/* Convert <IPbit, irq_offset> pair to IRQ number */
#define	IP_IRQ(IPbit, offset) ((IPbit - 2) * 32 + (offset) + IRQ_BASE)
/* The last one available IRQ */
#define	IRQ_END		IP_IRQ(6, 31)
#define	ICU_GROUP_REG_OFFSET	0x0C

#define	ICU_IP(irq)	(((irq) - IRQ_BASE) & 0x1f)
#define	ICU_IP_BIT(irq)	(1 << ICU_IP(irq))
#define	ICU_GROUP(irq)	(((irq) - IRQ_BASE) >> 5)

#define	ICU_GROUP_MASK_REG(group)	\
    (ICU_IMASK2 + ((((group) - 2) * ICU_GROUP_REG_OFFSET)))
#define	ICU_GROUP_IPEND_REG(group)	\
    (ICU_IPEND2 + ((((group) - 2) * ICU_GROUP_REG_OFFSET)))

#define	ICU_IRQ_MASK_REG(irq)	\
    (ICU_IMASK2 + ((ICU_GROUP(irq) * ICU_GROUP_REG_OFFSET)))
#define	ICU_IRQ_IPEND_REG(irq)	\
    (ICU_IPEND2 + ((ICU_GROUP(irq) * ICU_GROUP_REG_OFFSET)))

#define	PCI_IRQ_BASE		IP_IRQ(6, 4)
#define	PCI_IRQ_END		IP_IRQ(6, 7)

#endif /* __IDTREG_H__ */

