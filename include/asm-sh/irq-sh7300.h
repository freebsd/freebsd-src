#ifndef __ASM_SH_IRQ_SH7300_H
#define __ASM_SH_IRQ_SH7300_H

/*
 * linux/include/asm-sh/irq-sh7300.h
 *
 * Copyright (C) 2003 Takashi Kusuda <kusuda-takashi@hitachi-ul.co.jp>
 */

#include <linux/config.h>
#include <asm/machvec.h>
#include <asm/ptrace.h>		/* for pt_regs */

#define INTC_IPRA  	0xA414FEE2UL
#define INTC_IPRB  	0xA414FEE4UL
#define INTC_IPRC  	0xA4140016UL
#define INTC_IPRD  	0xA4140018UL
#define INTC_IPRE  	0xA414001AUL
#define INTC_IPRF  	0xA4080000UL
#define INTC_IPRG  	0xA4080002UL
#define INTC_IPRH  	0xA4080004UL
#define INTC_IPRI  	0xA4080006UL
#define INTC_IPRJ  	0xA4080008UL

#define INTC_IMR0	0xA4080040UL
#define INTC_IMR1	0xA4080042UL
#define INTC_IMR2	0xA4080044UL
#define INTC_IMR3	0xA4080046UL
#define INTC_IMR4	0xA4080048UL
#define INTC_IMR5	0xA408004AUL
#define INTC_IMR6	0xA408004CUL
#define INTC_IMR7	0xA408004EUL
#define INTC_IMR8	0xA4080050UL
#define INTC_IMR9	0xA4080052UL
#define INTC_IMR10	0xA4080054UL

#define INTC_IMCR0	0xA4080060UL
#define INTC_IMCR1	0xA4080062UL
#define INTC_IMCR2	0xA4080064UL
#define INTC_IMCR3	0xA4080066UL
#define INTC_IMCR4	0xA4080068UL
#define INTC_IMCR5	0xA408006AUL
#define INTC_IMCR6	0xA408006CUL
#define INTC_IMCR7	0xA408006EUL
#define INTC_IMCR8	0xA4080070UL
#define INTC_IMCR9	0xA4080072UL
#define INTC_IMCR10	0xA4080074UL

#define INTC_ICR0	0xA414FEE0UL
#define INTC_ICR1	0xA4140010UL

#define INTC_IRR0	0xA4140004UL

/* TMU0 */
#define TMU0_IRQ	16
#define TMU0_IPR_ADDR	INTC_IPRA
#define TMU0_IPR_POS	 3
#define TMU0_PRIORITY	 2

#define TIMER_IRQ       16
#define TIMER_IPR_ADDR  INTC_IPRA
#define TIMER_IPR_POS    3
#define TIMER_PRIORITY   2

/* TMU1 */
#define TMU1_IRQ	17
#define TMU1_IPR_ADDR	INTC_IPRA
#define TMU1_IPR_POS	 2
#define TMU1_PRIORITY	 2

/* TMU2 */
#define TMU2_IRQ	18
#define TMU2_IPR_ADDR	INTC_IPRA
#define TMU2_IPR_POS	 1
#define TMU2_PRIORITY	 2

/* WDT */
#define WDT_IRQ		27
#define WDT_IPR_ADDR	INTC_IPRB
#define WDT_IPR_POS	 3
#define WDT_PRIORITY	 2

/* SIM (SIM Card Module) */
#define SIM_ERI_IRQ	23
#define SIM_RXI_IRQ	24
#define SIM_TXI_IRQ	25
#define SIM_TEND_IRQ	26
#define SIM_IPR_ADDR	INTC_IPRB
#define SIM_IPR_POS	 1
#define SIM_PRIORITY	 2

/* VIO (Video I/O) */
#define VIO_IRQ		52
#define VIO_IPR_ADDR	INTC_IPRE
#define VIO_IPR_POS	 2
#define VIO_PRIORITY	 2

/* MFI (Multi Functional Interface) */
#define MFI_IRQ		56
#define MFI_IPR_ADDR	INTC_IPRE
#define MFI_IPR_POS	 1
#define MFI_PRIORITY	 2

/* VPU (Video Processing Unit) */
#define VPU_IRQ		60
#define VPU_IPR_ADDR	INTC_IPRE
#define VPU_IPR_POS	 0
#define VPU_PRIORITY	 2

/* KEY (Key Scan Interface) */
#define KEY_IRQ		79
#define KEY_IPR_ADDR	INTC_IPRF
#define KEY_IPR_POS	 3
#define KEY_PRIORITY	 2

/* CMT (Compare Match Timer) */
#define CMT_IRQ		104
#define CMT_IPR_ADDR	INTC_IPRF
#define CMT_IPR_POS	 0
#define CMT_PRIORITY	 2

/* DMAC(1) */
#define DMTE0_IRQ	48
#define DMTE1_IRQ	49
#define DMTE2_IRQ	50
#define DMTE3_IRQ	51
#define DMA1_IPR_ADDR	INTC_IPRE
#define DMA1_IPR_POS	3
#define DMA1_PRIORITY	7

/* DMAC(2) */
#define DMTE4_IRQ	76
#define DMTE5_IRQ	77
#define DMA2_IPR_ADDR	INTC_IPRF
#define DMA2_IPR_POS	2
#define DMA2_PRIORITY	7

/* SCIF0 */
#define SCIF0_IRQ	80
#define SCIF0_IPR_ADDR	INTC_IPRG
#define SCIF0_IPR_POS	3
#define SCIF0_PRIORITY	3

/* SIOF0 */
#define SIOF0_IRQ	55
#define SIOF0_IPR_ADDR	INTC_IPRH
#define SIOF0_IPR_POS	3
#define SIOF0_PRIORITY	3

/* FLCTL (Flash Memory Controller) */
#define FLSTE_IRQ	92
#define FLTEND_IRQ	93
#define FLTRQ0_IRQ	94
#define FLTRQ1_IRQ	95	
#define FLCTL_IPR_ADDR	INTC_IPRH
#define FLCTL_IPR_POS	1
#define FLCTL_PRIORITY	3

/* IIC (IIC Bus Interface) */
#define IIC_ALI_IRQ	96
#define IIC_TACKI_IRQ	97
#define IIC_WAITI_IRQ	98
#define IIC_DTEI_IRQ	99
#define IIC_IPR_ADDR	INTC_IPRH
#define IIC_IPR_POS	0
#define IIC_PRIORITY	3

/* SIO0 */
#define SIO0_IRQ	88
#define SIO0_IPR_ADDR	INTC_IPRI
#define SIO0_IPR_POS	3
#define SIO0_PRIORITY	3

/* SIU (Sound Interface Unit) */
#define SIU_IRQ		108
#define SIU_IPR_ADDR	INTC_IPRJ
#define SIU_IPR_POS	1
#define SIU_PRIORITY	3


/* ONCHIP_NR_IRQS */
#define NR_IRQS 109

/* In a generic kernel, NR_IRQS is an upper bound, and we should use
 * ACTUAL_NR_IRQS (which uses the machine vector) to get the correct value.
 */
#define ACTUAL_NR_IRQS NR_IRQS


extern void disable_irq(unsigned int);
extern void disable_irq_nosync(unsigned int);
extern void enable_irq(unsigned int);

/*
 * Simple Mask Register Support
 */
extern void make_maskreg_irq(unsigned int irq);
extern unsigned short *irq_mask_register;

/*
 * Function for "on chip support modules".
 */
extern void make_ipr_irq(unsigned int irq, unsigned int addr,
			 int pos,  int priority);
extern void make_imask_irq(unsigned int irq);

#define PORT_PACR	0xA4050100UL
#define PORT_PBCR	0xA4050102UL
#define PORT_PCCR	0xA4050104UL
#define PORT_PDCR	0xA4050106UL
#define PORT_PECR	0xA4050108UL
#define PORT_PFCR	0xA405010AUL
#define PORT_PGCR	0xA405010CUL
#define PORT_PHCR	0xA405010EUL
#define PORT_PJCR	0xA4050110UL
#define PORT_PKCR	0xA4050112UL
#define PORT_PLCR	0xA4050114UL
#define PORT_SCPCR	0xA4050116UL
#define PORT_PMCR	0xA4050118UL
#define PORT_PNCR	0xA405011AUL
#define PORT_PQCR	0xA405011CUL

#define PORT_PSELA	0xA4050140UL
#define PORT_PSELB	0xA4050142UL
#define PORT_PSELC	0xA4050144UL

#define PORT_HIZCRA	0xA4050146UL
#define PORT_HIZCRB	0xA4050148UL
#define PORT_DRVCR	0xA4050150UL

#define PORT_PADR  	0xA4050120UL
#define PORT_PBDR  	0xA4050122UL
#define PORT_PCDR  	0xA4050124UL
#define PORT_PDDR  	0xA4050126UL
#define PORT_PEDR  	0xA4050128UL
#define PORT_PFDR  	0xA405012AUL
#define PORT_PGDR  	0xA405012CUL
#define PORT_PHDR  	0xA405012EUL
#define PORT_PJDR  	0xA4050130UL
#define PORT_PKDR  	0xA4050132UL
#define PORT_PLDR  	0xA4050134UL
#define PORT_SCPDR  	0xA4050136UL
#define PORT_PMDR  	0xA4050138UL
#define PORT_PNDR  	0xA405013AUL
#define PORT_PQDR  	0xA405013CUL

#define IRQ0_IRQ	32
#define IRQ1_IRQ	33
#define IRQ2_IRQ	34
#define IRQ3_IRQ	35
#define IRQ4_IRQ	36
#define IRQ5_IRQ	37

#define IRQ0_IPR_ADDR	INTC_IPRC
#define IRQ1_IPR_ADDR	INTC_IPRC
#define IRQ2_IPR_ADDR	INTC_IPRC
#define IRQ3_IPR_ADDR	INTC_IPRC
#define IRQ4_IPR_ADDR	INTC_IPRD
#define IRQ5_IPR_ADDR	INTC_IPRD

#define IRQ0_IPR_POS	0
#define IRQ1_IPR_POS	1
#define IRQ2_IPR_POS	2
#define IRQ3_IPR_POS	3
#define IRQ4_IPR_POS	0
#define IRQ5_IPR_POS	1

#define IRQ0_PRIORITY	1
#define IRQ1_PRIORITY	1
#define IRQ2_PRIORITY	1
#define IRQ3_PRIORITY	1
#define IRQ4_PRIORITY	1
#define IRQ5_PRIORITY	1

extern int ipr_irq_demux(int irq);
#define __irq_demux(irq) ipr_irq_demux(irq)
#define irq_demux(irq) __irq_demux(irq)

#endif /* __ASM_SH_IRQ_SH7300_H */
