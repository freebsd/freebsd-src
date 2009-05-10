/* $NetBSD: pxa2x0reg.h,v 1.9 2006/04/10 04:13:58 simonb Exp $ */

/*
 * Copyright (c) 2002  Genetec Corporation.  All rights reserved.
 * Written by Hiroyuki Bessho for Genetec Corporation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed for the NetBSD Project by
 *	Genetec Corporation.
 * 4. The name of Genetec Corporation may not be used to endorse or 
 *    promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY GENETEC CORPORATION ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL GENETEC CORPORATION
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */


/*
 * Intel PXA2[15]0 processor is XScale based integrated CPU
 *
 * Reference:
 *  Intel(r) PXA250 and PXA210 Application Processors
 *   Developer's Manual
 *  (278522-001.pdf)
 */
#ifndef _ARM_XSCALE_PXAREG_H_
#define _ARM_XSCALE_PXAREG_H_

/* Borrow some register definitions from sa11x0 */
#include <arm/sa11x0/sa11x0_reg.h>

#ifndef _LOCORE
#include <sys/types.h>		/* for uint32_t */
#endif

/*
 * Chip select domains
 */
#define PXA2X0_CS0_START 0x00000000
#define PXA2X0_CS1_START 0x04000000
#define PXA2X0_CS2_START 0x08000000
#define PXA2X0_CS3_START 0x0c000000
#define PXA2X0_CS4_START 0x10000000
#define PXA2X0_CS5_START 0x14000000
#define	PXA2X0_CS_SIZE   0x04000000

#define PXA2X0_PCMCIA_SLOT0  0x20000000
#define PXA2X0_PCMCIA_SLOT1  0x30000000

#define PXA2X0_PERIPH_START 0x40000000
/* #define PXA2X0_MEMCTL_START 0x48000000 */
#define PXA270_PERIPH_END   0x530fffff
#define PXA250_PERIPH_END   0x480fffff
#define	PXA2X0_PERIPH_OFFSET 0xa8000000

#define PXA2X0_SDRAM0_START 0xa0000000
#define PXA2X0_SDRAM1_START 0xa4000000
#define PXA2X0_SDRAM2_START 0xa8000000
#define PXA2X0_SDRAM3_START 0xac000000
#define	PXA2X0_SDRAM_BANKS      4
#define	PXA2X0_SDRAM_BANK_SIZE  0x04000000

/*
 * Physical address of integrated peripherals
 */

#define PXA2X0_DMAC_BASE	0x40000000
#define PXA2X0_DMAC_SIZE	0x300
#define PXA2X0_FFUART_BASE	0x40100000 /* Full Function UART */
#define	PXA2X0_FFUART_SIZE	0x20
#define PXA2X0_BTUART_BASE	0x40200000 /* Bluetooth UART */
#define	PXA2X0_BTUART_SIZE	0x24
#define PXA2X0_I2C_BASE		0x40300000
#define PXA2X0_I2C_SIZE		0x000016a4
#define PXA2X0_I2S_BASE 	0x40400000
#define PXA2X0_AC97_BASE	0x40500000
#define PXA2X0_AC97_SIZE	0x600
#define PXA2X0_USBDC_BASE 	0x40600000 /* USB Client */
#define PXA2X0_USBDC_SIZE 	0x0e04
#define PXA2X0_STUART_BASE	0x40700000 /* Standard UART */
#define	PXA2X0_STUART_SIZE	0x24
#define PXA2X0_ICP_BASE 	0x40800000
#define PXA2X0_RTC_BASE 	0x40900000
#define PXA2X0_RTC_SIZE 	0x10
#define PXA2X0_OST_BASE 	0x40a00000 /* OS Timer */
#define	PXA2X0_OST_SIZE		0x20
#define PXA2X0_PWM0_BASE	0x40b00000
#define PXA2X0_PWM1_BASE	0x40c00000
#define PXA2X0_INTCTL_BASE	0x40d00000 /* Interrupt controller */
#define	PXA2X0_INTCTL_SIZE	0x20
#define PXA2X0_GPIO_BASE	0x40e00000

#define PXA270_GPIO_SIZE  	0x150
#define PXA250_GPIO_SIZE  	0x70
#define PXA2X0_POWMAN_BASE  	0x40f00000 /* Power management */
#define PXA2X0_SSP_BASE 	0x41000000
#define PXA2X0_MMC_BASE 	0x41100000 /* MultiMediaCard */
#define PXA2X0_MMC_SIZE		0x48
#define PXA2X0_CLKMAN_BASE  	0x41300000 /* Clock Manager */
#define PXA2X0_CLKMAN_SIZE	12
#define	PXA2X0_HWUART_BASE	0x41600000 /* Hardware UART */
#define	PXA2X0_HWUART_SIZE	0x30
#define PXA2X0_LCDC_BASE	0x44000000 /* LCD Controller */
#define PXA2X0_LCDC_SIZE	0x220
#define PXA2X0_MEMCTL_BASE	0x48000000 /* Memory Controller */
#define PXA2X0_MEMCTL_SIZE	0x48
#define PXA2X0_USBH_BASE	0x4c000000 /* USB Host controller */
#define PXA2X0_USBH_SIZE	0x70

/* Internal SRAM storage. PXA27x only */
#define PXA270_SRAM0_START 0x5c000000
#define PXA270_SRAM1_START 0x5c010000
#define PXA270_SRAM2_START 0x5c020000
#define PXA270_SRAM3_START 0x5c030000
#define	PXA270_SRAM_BANKS      4
#define	PXA270_SRAM_BANK_SIZE  0x00010000

/* width of interrupt controller */
#define ICU_LEN			32   /* but [0..7,15,16] is not used */
#define ICU_INT_HWMASK		0xffffff00
#define PXA250_IRQ_MIN 8	/* 0..7 are not used by integrated 
				   peripherals */
#define PXA270_IRQ_MIN 0

#define PXA2X0_INT_USBH1	3	/* USB host (OHCI) */

#define	PXA2X0_INT_HWUART	7
#define PXA2X0_INT_GPIO0	8
#define PXA2X0_INT_GPIO1	9
#define PXA2X0_INT_GPION	10	/* irq from GPIO[2..80] */
#define PXA2X0_INT_USB  	11
#define PXA2X0_INT_PMU  	12
#define PXA2X0_INT_I2S  	13
#define PXA2X0_INT_AC97  	14
#define PXA2X0_INT_LCD  	17
#define PXA2X0_INT_I2C  	18
#define PXA2X0_INT_ICP  	19
#define PXA2X0_INT_STUART  	20
#define PXA2X0_INT_BTUART  	21
#define PXA2X0_INT_FFUART  	22
#define PXA2X0_INT_MMC  	23
#define PXA2X0_INT_SSP  	24
#define PXA2X0_INT_DMA  	25
#define PXA2X0_INT_OST0  	26
#define PXA2X0_INT_OST1  	27
#define PXA2X0_INT_OST2  	28
#define PXA2X0_INT_OST3  	29
#define PXA2X0_INT_RTCHZ  	30
#define PXA2X0_INT_ALARM  	31	/* RTC Alarm interrupt */

/* DMAC */
#define DMAC_N_CHANNELS	16
#define	DMAC_N_PRIORITIES 3

#define DMAC_DCSR(n)	((n)*4)
#define  DCSR_BUSERRINTR    (1<<0)	/* bus error interrupt */
#define  DCSR_STARTINTR     (1<<1)	/* start interrupt */
#define  DCSR_ENDINTR       (1<<2)	/* end interrupt */
#define  DCSR_STOPSTATE     (1<<3)	/* channel is not running */
#define  DCSR_REQPEND       (1<<8)	/* request pending */
#define  DCSR_STOPIRQEN     (1<<29)     /* stop interrupt enable */
#define  DCSR_NODESCFETCH   (1<<30)	/* no-descriptor fetch mode */
#define  DCSR_RUN  	    (1<<31)
#define DMAC_DINT 	0x00f0		/* DAM interrupt */
#define  DMAC_DINT_MASK	0xffffu
#define DMAC_DRCMR(n)	(0x100+(n)*4)	/* Channel map register */
#define  DRCMR_CHLNUM	0x0f		/* channel number */
#define  DRCMR_MAPVLD	(1<<7)		/* map valid */
#define DMAC_DDADR(n)	(0x0200+(n)*16)
#define  DDADR_STOP	(1<<0)
#define DMAC_DSADR(n)	(0x0204+(n)*16)
#define DMAC_DTADR(n)	(0x0208+(n)*16)
#define DMAC_DCMD(n)	(0x020c+(n)*16)
#define  DCMD_LENGTH_MASK	0x1fff
#define  DCMD_WIDTH_SHIFT  14
#define  DCMD_WIDTH_0	(0<<DCMD_WIDTH_SHIFT)	/* for mem-to-mem transfer*/
#define  DCMD_WIDTH_1	(1<<DCMD_WIDTH_SHIFT)
#define  DCMD_WIDTH_2	(2<<DCMD_WIDTH_SHIFT)
#define  DCMD_WIDTH_4	(3<<DCMD_WIDTH_SHIFT)
#define  DCMD_SIZE_SHIFT  16
#define  DCMD_SIZE_8	(1<<DCMD_SIZE_SHIFT)
#define  DCMD_SIZE_16	(2<<DCMD_SIZE_SHIFT)
#define  DCMD_SIZE_32	(3<<DCMD_SIZE_SHIFT)
#define  DCMD_LITTLE_ENDIEN	(0<<18)
#define	 DCMD_ENDIRQEN	  (1<<21)
#define  DCMD_STARTIRQEN  (1<<22)
#define  DCMD_FLOWTRG     (1<<28)	/* flow control by target */
#define  DCMD_FLOWSRC     (1<<29)	/* flow control by source */
#define  DCMD_INCTRGADDR  (1<<30)	/* increment target address */
#define  DCMD_INCSRCADDR  (1<<31)	/* increment source address */

#ifndef __ASSEMBLER__
/* DMA descriptor */
struct pxa_dma_desc {
	volatile uint32_t	dd_ddadr;
#define	DMAC_DESC_LAST	0x1
	volatile uint32_t	dd_dsadr;
	volatile uint32_t	dd_dtadr;
	volatile uint32_t	dd_dcmd;		/* command and length */
};
#endif

/* UART */
#define PXA2X0_COM_FREQ   14745600L

/* I2C */
#define I2C_IBMR	0x1680		/* Bus monitor register */
#define I2C_IDBR	0x1688		/* Data buffer */
#define I2C_ICR  	0x1690		/* Control register */
#define  ICR_START	(1<<0)
#define  ICR_STOP	(1<<1)
#define  ICR_ACKNAK	(1<<2)
#define  ICR_TB  	(1<<3)
#define  ICR_MA  	(1<<4)
#define I2C_ISR  	0x1698		/* Status register */
#define I2C_ISAR	0x16a0		/* Slave address */

/* Clock Manager */
#define CLKMAN_CCCR	0x00	/* Core Clock Configuration */
#define  CCCR_TURBO_X1	 (2<<7)
#define  CCCR_TURBO_X15	 (3<<7)	/* x 1.5 */
#define  CCCR_TURBO_X2	 (4<<7)
#define  CCCR_TURBO_X25	 (5<<7)	/* x 2.5 */
#define  CCCR_TURBO_X3	 (6<<7)	/* x 3.0 */
#define  CCCR_RUN_X1	 (1<<5)
#define  CCCR_RUN_X2	 (2<<5)
#define  CCCR_RUN_X4	 (3<<5)
#define  CCCR_MEM_X27	 (1<<0)	/* x27, 99.53MHz */
#define  CCCR_MEM_X32	 (2<<0)	/* x32, 117,96MHz */
#define  CCCR_MEM_X36	 (3<<0)	/* x26, 132.71MHz */
#define  CCCR_MEM_X40	 (4<<0)	/* x27, 99.53MHz */
#define  CCCR_MEM_X45	 (5<<0)	/* x27, 99.53MHz */
#define  CCCR_MEM_X9	 (0x1f<<0)	/* x9, 33.2MHz */

#define CLKMAN_CKEN	0x04	/* Clock Enable Register */
#define CLKMAN_OSCC	0x08	/* Osillcator Configuration Register */

#define CCCR_N_SHIFT	7
#define CCCR_N_MASK	(0x07<<CCCR_N_SHIFT)
#define CCCR_M_SHIFT	5
#define CCCR_M_MASK	(0x03<<CCCR_M_SHIFT)
#define CCCR_L_MASK	0x1f

#define CKEN_PWM0	(1<<0)
#define CKEN_PWM1	(1<<1)
#define CKEN_AC97	(1<<2)
#define CKEN_SSP	(1<<3)
#define CKEN_STUART	(1<<5)
#define CKEN_FFUART	(1<<6)
#define CKEN_BTUART	(1<<7)
#define CKEN_I2S	(1<<8)
#define CKEN_USBH	(1<<10)
#define CKEN_USB	(1<<11)
#define CKEN_MMC	(1<<12)
#define CKEN_FICP	(1<<13)
#define CKEN_I2C	(1<<14)
#define CKEN_LCD	(1<<16)

#define OSCC_OOK	(1<<0)	/* 32.768 kHz oscillator status */
#define OSCC_OON	(1<<1)	/* 32.768 kHz oscillator */

/*
 * RTC
 */
#define RTC_RCNR	0x0000	/* count register */
#define RTC_RTAR	0x0004	/* alarm register */
#define RTC_RTSR	0x0008	/* status register */
#define RTC_RTTR	0x000c	/* trim register */
/*
 * GPIO
 */
#define GPIO_GPLR0  0x00	/* Level reg [31:0] */
#define GPIO_GPLR1  0x04	/* Level reg [63:32] */
#define GPIO_GPLR2  0x08	/* Level reg [80:64] */

#define GPIO_GPDR0  0x0c	/* dir reg [31:0] */
#define GPIO_GPDR1  0x10	/* dir reg [63:32] */
#define GPIO_GPDR2  0x14	/* dir reg [80:64] */

#define GPIO_GPSR0  0x18	/* set reg [31:0] */
#define GPIO_GPSR1  0x1c	/* set reg [63:32] */
#define GPIO_GPSR2  0x20	/* set reg [80:64] */

#define GPIO_GPCR0  0x24	/* clear reg [31:0] */
#define GPIO_GPCR1  0x28	/* clear reg [63:32] */
#define GPIO_GPCR2  0x2c	/* clear reg [80:64] */

#define GPIO_GPER0  0x30	/* rising edge [31:0] */
#define GPIO_GPER1  0x34	/* rising edge [63:32] */
#define GPIO_GPER2  0x38	/* rising edge [80:64] */

#define GPIO_GRER0  0x30	/* rising edge [31:0] */
#define GPIO_GRER1  0x34	/* rising edge [63:32] */
#define GPIO_GRER2  0x38	/* rising edge [80:64] */

#define GPIO_GFER0  0x3c	/* falling edge [31:0] */
#define GPIO_GFER1  0x40	/* falling edge [63:32] */
#define GPIO_GFER2  0x44	/* falling edge [80:64] */

#define GPIO_GEDR0  0x48	/* edge detect [31:0] */
#define GPIO_GEDR1  0x4c	/* edge detect [63:32] */
#define GPIO_GEDR2  0x50	/* edge detect [80:64] */

#define GPIO_GAFR0_L  0x54	/* alternate function [15:0] */
#define GPIO_GAFR0_U  0x58	/* alternate function [31:16] */
#define GPIO_GAFR1_L  0x5c	/* alternate function [47:32] */
#define GPIO_GAFR1_U  0x60	/* alternate function [63:48] */
#define GPIO_GAFR2_L  0x64	/* alternate function [79:64] */
#define GPIO_GAFR2_U  0x68	/* alternate function [80] */

/* Only for PXA270 */
#define GPIO_GAFR3_L  0x6c	/* alternate function [111:96] */
#define GPIO_GAFR3_U  0x70	/* alternate function [120:112] */

#define GPIO_GPLR3  0x100	/* Level reg [120:96] */
#define GPIO_GPDR3  0x10c	/* dir reg [120:96] */
#define GPIO_GPSR3  0x118	/* set reg [120:96] */
#define GPIO_GPCR3  0x124	/* clear reg [120:96] */
#define GPIO_GRER3  0x130	/* rising edge [120:96] */
#define GPIO_GFER3  0x13c	/* falling edge [120:96] */
#define GPIO_GEDR3  0x148	/* edge detect [120:96] */

/* a bit simpler if we don't support PXA270 */
#define	PXA250_GPIO_REG(r, pin)	((r) + (((pin) / 32) * 4))
#define	PXA250_GPIO_NPINS    85

#define	PXA270_GPIO_REG(r, pin) \
(pin < 96 ? PXA250_GPIO_REG(r,pin) : ((r) + 0x100 + ((((pin)-96) / 32) * 4)))
#define PXA270_GPIO_NPINS    121


#define	GPIO_BANK(pin)		((pin) / 32)
#define	GPIO_BIT(pin)		(1u << ((pin) & 0x1f))
#define	GPIO_FN_REG(pin)	(GPIO_GAFR0_L + (((pin) / 16) * 4))
#define	GPIO_FN_SHIFT(pin)	((pin & 0xf) * 2)

#define	GPIO_IN		  	0x00	/* Regular GPIO input pin */
#define	GPIO_OUT	  	0x10	/* Regular GPIO output pin */
#define	GPIO_ALT_FN_1_IN	0x01	/* Alternate function 1 input */
#define	GPIO_ALT_FN_1_OUT	0x11	/* Alternate function 1 output */
#define	GPIO_ALT_FN_2_IN	0x02	/* Alternate function 2 input */
#define	GPIO_ALT_FN_2_OUT	0x12	/* Alternate function 2 output */
#define	GPIO_ALT_FN_3_IN	0x03	/* Alternate function 3 input */
#define	GPIO_ALT_FN_3_OUT	0x13	/* Alternate function 3 output */
#define	GPIO_SET		0x20	/* Initial state is Set */
#define	GPIO_CLR		0x00	/* Initial state is Clear */

#define	GPIO_FN_MASK		0x03
#define	GPIO_FN_IS_OUT(n)	((n) & GPIO_OUT)
#define	GPIO_FN_IS_SET(n)	((n) & GPIO_SET)
#define	GPIO_FN(n)		((n) & GPIO_FN_MASK)
#define	GPIO_IS_GPIO(n)		(GPIO_FN(n) == 0)
#define	GPIO_IS_GPIO_IN(n)	(((n) & (GPIO_FN_MASK|GPIO_OUT)) == GPIO_IN)
#define	GPIO_IS_GPIO_OUT(n)	(((n) & (GPIO_FN_MASK|GPIO_OUT)) == GPIO_OUT)

#define	IRQ_GPIO0		64
#define	IRQ_NGPIO		128
#define	IRQ_GPIO_MAX		IRQ_GPIO0 + IRQ_NGPIO
#define	IRQ_TO_GPIO(x)		(x - IRQ_GPIO0)
#define	GPIO_TO_IRQ(x)		(x + IRQ_GPIO0)

/*
 * memory controller
 */

#define MEMCTL_MDCNFG	0x0000
#define  MDCNFG_DE0		(1<<0)
#define  MDCNFG_DE1		(1<<1)
#define  MDCNFD_DWID01_SHIFT	2
#define  MDCNFD_DCAC01_SHIFT	3
#define  MDCNFD_DRAC01_SHIFT	5
#define  MDCNFD_DNB01_SHIFT	7
#define  MDCNFG_DE2		(1<<16)
#define  MDCNFG_DE3		(1<<17)
#define  MDCNFD_DWID23_SHIFT	18
#define  MDCNFD_DCAC23_SHIFT	19
#define  MDCNFD_DRAC23_SHIFT	21
#define  MDCNFD_DNB23_SHIFT	23

#define  MDCNFD_DWID_MASK	0x1
#define  MDCNFD_DCAC_MASK	0x3
#define  MDCNFD_DRAC_MASK	0x3
#define  MDCNFD_DNB_MASK	0x1
	
#define MEMCTL_MDREFR   0x04	/* refresh control register */
#define  MDREFR_DRI	0xfff
#define  MDREFR_E0PIN	(1<<12)
#define  MDREFR_K0RUN   (1<<13)	/* SDCLK0 enable */
#define  MDREFR_K0DB2   (1<<14)	/* SDCLK0 1/2 freq */
#define  MDREFR_E1PIN	(1<<15)
#define  MDREFR_K1RUN   (1<<16)	/* SDCLK1 enable */
#define  MDREFR_K1DB2   (1<<17)	/* SDCLK1 1/2 freq */
#define  MDREFR_K2RUN   (1<<18)	/* SDCLK2 enable */
#define  MDREFR_K2DB2	(1<<19)	/* SDCLK2 1/2 freq */
#define	 MDREFR_APD	(1<<20)	/* Auto Power Down */
#define  MDREFR_SLFRSH	(1<<22)	/* Self Refresh */
#define  MDREFR_K0FREE	(1<<23)	/* SDCLK0 free run */
#define  MDREFR_K1FREE	(1<<24)	/* SDCLK1 free run */
#define  MDREFR_K2FREE	(1<<25)	/* SDCLK2 free run */

#define MEMCTL_MSC0	0x08	/* Asychronous Statis memory Control CS[01] */
#define MEMCTL_MSC1	0x0c	/* Asychronous Statis memory Control CS[23] */
#define MEMCTL_MSC2	0x10	/* Asychronous Statis memory Control CS[45] */
#define  MSC_RBUFF_SHIFT 15	/* return data buffer */
#define  MSC_RBUFF	(1<<MSC_RBUFF_SHIFT)
#define  MSC_RRR_SHIFT   12  	/* recovery time */
#define	 MSC_RRR	(7<<MSC_RRR_SHIFT)
#define  MSC_RDN_SHIFT    8	/* ROM delay next access */
#define  MSC_RDN	(0x0f<<MSC_RDN_SHIFT)
#define  MSC_RDF_SHIFT    4	/*  ROM delay first access*/
#define  MSC_RDF  	(0x0f<<MSC_RDF_SHIFT)
#define  MSC_RBW_SHIFT    3	/* 32/16 bit bus */
#define  MSC_RBW 	(1<<MSC_RBW_SHIFT)
#define  MSC_RT_SHIFT	   0	/* type */
#define  MSC_RT 	(7<<MSC_RT_SHIFT)
#define  MSC_RT_NONBURST	0
#define  MSC_RT_SRAM    	1
#define  MSC_RT_BURST4  	2
#define  MSC_RT_BURST8  	3
#define  MSC_RT_VLIO   	 	4

/* expansion memory timing configuration */
#define MEMCTL_MCMEM(n)	(0x28+4*(n))
#define MEMCTL_MCATT(n)	(0x30+4*(n))
#define MEMCTL_MCIO(n)	(0x38+4*(n))

#define  MC_HOLD_SHIFT	14
#define  MC_ASST_SHIFT	7
#define  MC_SET_SHIFT	0
#define  MC_TIMING_VAL(hold,asst,set)	(((hold)<<MC_HOLD_SHIFT)| \
		((asst)<<MC_ASST_SHIFT)|((set)<<MC_SET_SHIFT))

#define MEMCTL_MECR	0x14	/* Expansion memory configuration */
#define MECR_NOS	(1<<0)	/* Number of sockets */
#define MECR_CIT	(1<<1)	/* Card-is-there */

#define MEMCTL_MDMRS	0x0040

/*
 * LCD Controller
 */
#define LCDC_LCCR0	0x000	/* Controller Control Register 0 */
#define  LCCR0_ENB	(1U<<0)	/* LCD Controller Enable */
#define  LCCR0_CMS	(1U<<1)	/* Color/Mono select */
#define  LCCR0_SDS	(1U<<2)	/* Single/Dual -panel */
#define  LCCR0_LDM	(1U<<3)	/* LCD Disable Done Mask */
#define  LCCR0_SFM	(1U<<4)	/* Start of Frame Mask */
#define  LCCR0_IUM	(1U<<5)	/* Input FIFO Underrun Mask */
#define  LCCR0_EFM	(1U<<6)	/* End of Frame Mask */
#define  LCCR0_PAS	(1U<<7)	/* Passive/Active Display select */
#define  LCCR0_DPD	(1U<<9)	/* Double-Pixel Data pin mode */
#define  LCCR0_DIS	(1U<<10) /* LCD Disable */
#define  LCCR0_QDM	(1U<<11) /* LCD Quick Disable Mask */
#define  LCCR0_BM	(1U<<20) /* Branch Mask */
#define  LCCR0_OUM	(1U<<21) /* Output FIFO Underrun Mask */

#define  LCCR0_IMASK	(LCCR0_LDM|LCCR0_SFM|LCCR0_IUM|LCCR0_EFM|LCCR0_QDM|LCCR0_BM|LCCR0_OUM)


#define LCDC_LCCR1	0x004	/* Controller Control Register 1 */
#define LCDC_LCCR2	0x008	/* Controller Control Register 2 */
#define LCDC_LCCR3	0x00c	/* Controller Control Register 2 */
#define  LCCR3_BPP_SHIFT 24		/* Bits per pixel */
#define  LCCR3_BPP	(0x07<<LCCR3_BPP_SHIFT)
#define LCDC_LCCR4	0x010	/* Controller Control Register 4 */
#define LCDC_LCCR5	0x014	/* Controller Control Register 5 */
#define LCDC_FBR0	0x020	/* DMA ch0 frame branch register */
#define LCDC_FBR1	0x024	/* DMA ch1 frame branch register */
#define LCDC_FBR2	0x028	/* DMA ch2 frame branch register */
#define LCDC_FBR3	0x02c	/* DMA ch3 frame branch register */
#define LCDC_FBR4	0x030	/* DMA ch4 frame branch register */
#define LCDC_LCSR1	0x034	/* controller status register 1 PXA27x only */
#define LCDC_LCSR	0x038	/* controller status register */
#define  LCSR_LDD	(1U<<0) /* LCD disable done */
#define  LCSR_SOF	(1U<<1) /* Start of frame */
#define LCDC_LIIDR	0x03c	/* controller interrupt ID Register */
#define LCDC_TRGBR	0x040	/* TMED RGB Speed Register */
#define LCDC_TCR	0x044	/* TMED Control Register */
#define LCDC_OVL1C1	0x050	/* Overlay 1 control register 1 */
#define LCDC_OVL1C2	0x060	/* Overlay 1 control register 2 */
#define LCDC_OVL2C1	0x070	/* Overlay 1 control register 1 */
#define LCDC_OVL2C2	0x080	/* Overlay 1 control register 2 */
#define LCDC_CCR	0x090	/* Cursor control register */
#define LCDC_CMDCR	0x100	/* Command control register */
#define LCDC_PRSR	0x104	/* Panel read status register */
#define LCDC_FBR5	0x110	/* DMA ch5 frame branch register */
#define LCDC_FBR6	0x114	/* DMA ch6 frame branch register */
#define LCDC_FDADR0	0x200	/* DMA ch0 frame descriptor address */
#define LCDC_FSADR0	0x204	/* DMA ch0 frame source address */
#define LCDC_FIDR0	0x208	/* DMA ch0 frame ID register */
#define LCDC_LDCMD0	0x20c	/* DMA ch0 command register */
#define LCDC_FDADR1	0x210	/* DMA ch1 frame descriptor address */
#define LCDC_FSADR1	0x214	/* DMA ch1 frame source address */
#define LCDC_FIDR1	0x218	/* DMA ch1 frame ID register */
#define LCDC_LDCMD1	0x21c	/* DMA ch1 command register */
#define LCDC_FDADR2	0x220	/* DMA ch2 frame descriptor address */
#define LCDC_FSADR2	0x224	/* DMA ch2 frame source address */
#define LCDC_FIDR2	0x228	/* DMA ch2 frame ID register */
#define LCDC_LDCMD2	0x22c	/* DMA ch2 command register */
#define LCDC_FDADR3	0x230	/* DMA ch3 frame descriptor address */
#define LCDC_FSADR3	0x234	/* DMA ch3 frame source address */
#define LCDC_FIDR3	0x238	/* DMA ch3 frame ID register */
#define LCDC_LDCMD3	0x23c	/* DMA ch3 command register */
#define LCDC_FDADR4	0x240	/* DMA ch4 frame descriptor address */
#define LCDC_FSADR4	0x244	/* DMA ch4 frame source address */
#define LCDC_FIDR4	0x248	/* DMA ch4 frame ID register */
#define LCDC_LDCMD4	0x24c	/* DMA ch4 command register */
#define LCDC_FDADR5	0x250	/* DMA ch5 frame descriptor address */
#define LCDC_FSADR5	0x254	/* DMA ch5 frame source address */
#define LCDC_FIDR5	0x258	/* DMA ch5 frame ID register */
#define LCDC_LDCMD5	0x25c	/* DMA ch5 command register */
#define LCDC_FDADR6	0x260	/* DMA ch6 frame descriptor address */
#define LCDC_FSADR6	0x264	/* DMA ch6 frame source address */
#define LCDC_FIDR6	0x268	/* DMA ch6 frame ID register */
#define LCDC_LDCMD6	0x26c	/* DMA ch6 command register */
#define LCDC_LCDBSCNTR	0x054	/* LCD buffer strength control register */

/*
 * MMC/SD controller
 */
#define MMC_STRPCL	0x00	/* start/stop MMC clock */
#define  STRPCL_NOOP	0
#define  STRPCL_STOP	1	/* stop MMC clock */
#define  STRPCL_START	2	/* start MMC clock */
#define MMC_STAT	0x04	/* status register */
#define  STAT_READ_TIME_OUT   		(1<<0)
#define  STAT_TIMEOUT_RESPONSE		(1<<1)
#define  STAT_CRC_WRITE_ERROR		(1<<2)
#define  STAT_CRC_READ_ERROR		(1<<3)
#define  STAT_SPI_READ_ERROR_TOKEN	(1<<4)
#define  STAT_RES_CRC_ERR		(1<<5)
#define  STAT_XMIT_FIFO_EMPTY		(1<<6)
#define  STAT_RECV_FIFO_FULL		(1<<7)
#define  STAT_CLK_EN			(1<<8)
#define  STAT_DATA_TRAN_DONE		(1<<11)
#define  STAT_PRG_DONE			(1<<12)
#define  STAT_END_CMD_RES		(1<<13)
#define MMC_CLKRT	0x08	/* MMC clock rate */
#define  CLKRT_20M	0
#define  CLKRT_10M	1
#define  CLKRT_5M	2
#define  CLKRT_2_5M	3
#define  CLKRT_1_25M	4
#define  CLKRT_625K	5
#define  CLKRT_312K	6
#define MMC_SPI  	0x0c	/* SPI mode control */
#define  SPI_EN  	(1<<0)	/* enable SPI mode */
#define  SPI_CRC_ON	(1<<1)	/* enable CRC generation */
#define  SPI_CS_EN	(1<<2)	/* Enable CS[01] */
#define  SPI_CS_ADDRESS	(1<<3)	/* CS0/CS1 */
#define MMC_CMDAT	0x10	/* command/response/data */
#define  CMDAT_RESPONSE_FORMAT	0x03
#define  CMDAT_RESPONSE_FORMAT_NO 0 /* no response */
#define  CMDAT_RESPONSE_FORMAT_R1 1 /* R1, R1b, R4, R5 */
#define  CMDAT_RESPONSE_FORMAT_R2 2
#define  CMDAT_RESPONSE_FORMAT_R3 3
#define  CMDAT_DATA_EN		(1<<2)
#define  CMDAT_WRITE		(1<<3) /* 1=write 0=read operation */
#define  CMDAT_STREAM_BLOCK	(1<<4) /* stream mode */
#define  CMDAT_BUSY		(1<<5) /* busy signal is expected */
#define  CMDAT_INIT		(1<<6) /* preceede command with 80 clocks */
#define  CMDAT_MMC_DMA_EN	(1<<7) /* DMA enable */
#define MMC_RESTO	0x14	/* expected response time out */
#define MMC_RDTO 	0x18	/* expected data read time out */
#define MMC_BLKLEN	0x1c	/* block length of data transaction */
#define MMC_NOB  	0x20	/* number of blocks (block mode) */
#define MMC_PRTBUF	0x24	/* partial MMC_TXFIFO written */
#define  PRTBUF_BUF_PART_FULL (1<<0) /* buffer partially full */
#define MMC_I_MASK	0x28	/* interrupt mask */
#define MMC_I_REG	0x2c	/* interrupt register */
#define  MMC_I_DATA_TRAN_DONE	(1<<0)
#define  MMC_I_PRG_DONE		(1<<1)
#define  MMC_I_END_CMD_RES	(1<<2)
#define  MMC_I_STOP_CMD		(1<<3)
#define  MMC_I_CLK_IS_OFF	(1<<4)
#define  MMC_I_RXFIFO_RD_REQ	(1<<5)
#define  MMC_I_TXFIFO_WR_REQ	(1<<6)
#define MMC_CMD  	0x30	/* index of current command */
#define MMC_ARGH 	0x34	/* MSW part of the current command arg */
#define MMC_ARGL 	0x38	/* LSW part of the current command arg */
#define MMC_RES  	0x3c	/* response FIFO */
#define MMC_RXFIFO	0x40	/* receive FIFO */
#define MMC_TXFIFO	0x44 	/* transmit FIFO */

/*
 * AC97
 */
#define	AC97_N_CODECS	2
#define AC97_GCR 	0x000c	/* Global control register */
#define  GCR_GIE       	(1<<0)	/* interrupt enable */
#define  GCR_COLD_RST	(1<<1)
#define  GCR_WARM_RST	(1<<2)
#define  GCR_ACLINK_OFF	(1<<3)
#define  GCR_PRIRES_IEN	(1<<4)	/* Primary resume interrupt enable */
#define  GCR_SECRES_IEN	(1<<5)	/* Secondary resume interrupt enable */
#define  GCR_PRIRDY_IEN	(1<<8)	/* Primary ready interrupt enable */
#define  GCR_SECRDY_IEN	(1<<9)	/* Primary ready interrupt enable */
#define  GCR_SDONE_IE 	(1<<18)	/* Status done interrupt enable */
#define  GCR_CDONE_IE	(1<<19)	/* Command done interrupt enable */

#define AC97_GSR 	0x001c	/* Global status register */
#define  GSR_GSCI	(1<<0)	/* codec GPI status change interrupt */
#define  GSR_MIINT	(1<<1)	/* modem in interrupt */
#define  GSR_MOINT	(1<<2)	/* modem out interrupt */
#define  GSR_PIINT	(1<<5)	/* PCM in interrupt */
#define  GSR_POINT	(1<<6)	/* PCM out interrupt */
#define  GSR_MINT	(1<<7)	/* Mic in interrupt */
#define  GSR_PCR	(1<<8)	/* primary code ready */
#define  GSR_SCR	(1<<9)	/* secondary code ready */
#define  GSR_PRIRES	(1<<10)	/* primary resume interrupt */
#define  GSR_SECRES	(1<<11)	/* secondary resume interrupt */
#define  GSR_BIT1SLT12	(1<<12)	/* Bit 1 of slot 12 */
#define  GSR_BIT2SLT12	(1<<13)	/* Bit 2 of slot 12 */
#define  GSR_BIT3SLT12	(1<<14)	/* Bit 3 of slot 12 */
#define  GSR_RDCS 	(1<<15)	/* Read completion status */
#define  GSR_SDONE 	(1<<18)	/* status done */
#define  GSR_CDONE 	(1<<19)	/* command done */

#define AC97_POCR 	0x0000	/* PCM-out control */
#define AC97_PICR 	0x0004	/* PCM-in control */
#define AC97_POSR 	0x0010	/* PCM-out status */
#define AC97_PISR 	0x0014	/* PCM-out status */
#define AC97_MCCR	0x0008	/* MIC-in control register */
#define AC97_MCSR	0x0018	/* MIC-in status register */
#define AC97_MICR	0x0100	/* Modem-in control register */
#define AC97_MISR	0x0108	/* Modem-in status register */
#define AC97_MOCR	0x0110	/* Modem-out control register */
#define AC97_MOSR	0x0118	/* Modem-out status register */
#define  AC97_FEFIE	(1<<3)	/* fifo error interrupt enable */
#define  AC97_FIFOE	(1<<4)	/* fifo error */

#define AC97_CAR  	0x0020	/* Codec access register */
#define  CAR_CAIP  	(1<<0)	/* Codec access in progress */

#define AC97_PCDR	0x0040	/* PCM data register */
#define AC97_MCDR 	0x0060	/* MIC-in data register */
#define AC97_MODR 	0x0140	/* Modem data register */

/* address to access codec registers */
#define AC97_PRIAUDIO	0x0200	/* Primary audio codec */
#define AC97_SECAUDIO	0x0300	/* Secondary autio codec */
#define AC97_PRIMODEM	0x0400	/* Primary modem codec */
#define AC97_SECMODEM	0x0500	/* Secondary modem codec */
#define	AC97_CODEC_BASE(c)	(AC97_PRIAUDIO + ((c) * 0x100))

/*
 * USB device controller
 */
#define USBDC_UDCCR	0x0000  /* UDC control register    */
#define USBDC_UDCCS(n)	(0x0010+4*(n))  /* Endpoint Control/Status Registers */
#define USBDC_UICR0	0x0050  /* UDC Interrupt Control Register 0  */
#define USBDC_UICR1	0x0054  /* UDC Interrupt Control Register 1  */
#define USBDC_USIR0	0x0058  /* UDC Status Interrupt Register 0  */
#define USBDC_USIR1	0x005C  /* UDC Status Interrupt Register 1  */
#define USBDC_UFNHR	0x0060  /* UDC Frame Number Register High  */
#define USBDC_UFNLR	0x0064  /* UDC Frame Number Register Low  */
#define USBDC_UBCR2	0x0068  /* UDC Byte Count Register 2  */
#define USBDC_UBCR4	0x006C  /* UDC Byte Count Register 4  */
#define USBDC_UBCR7	0x0070  /* UDC Byte Count Register 7  */
#define USBDC_UBCR9	0x0074  /* UDC Byte Count Register 9  */
#define USBDC_UBCR12	0x0078  /* UDC Byte Count Register 12  */
#define USBDC_UBCR14	0x007C  /* UDC Byte Count Register 14  */
#define USBDC_UDDR0	0x0080  /* UDC Endpoint 0 Data Register  */
#define USBDC_UDDR1	0x0100  /* UDC Endpoint 1 Data Register  */
#define USBDC_UDDR2	0x0180  /* UDC Endpoint 2 Data Register  */
#define USBDC_UDDR3	0x0200  /* UDC Endpoint 3 Data Register  */
#define USBDC_UDDR4	0x0400  /* UDC Endpoint 4 Data Register  */
#define USBDC_UDDR5	0x00A0  /* UDC Endpoint 5 Data Register  */
#define USBDC_UDDR6	0x0600  /* UDC Endpoint 6 Data Register  */
#define USBDC_UDDR7	0x0680  /* UDC Endpoint 7 Data Register  */
#define USBDC_UDDR8	0x0700  /* UDC Endpoint 8 Data Register  */
#define USBDC_UDDR9	0x0900  /* UDC Endpoint 9 Data Register  */
#define USBDC_UDDR10	0x00C0  /* UDC Endpoint 10 Data Register  */
#define USBDC_UDDR11	0x0B00  /* UDC Endpoint 11 Data Register  */
#define USBDC_UDDR12	0x0B80  /* UDC Endpoint 12 Data Register  */
#define USBDC_UDDR13	0x0C00  /* UDC Endpoint 13 Data Register  */
#define USBDC_UDDR14	0x0E00  /* UDC Endpoint 14 Data Register  */
#define USBDC_UDDR15	0x00E0  /* UDC Endpoint 15 Data Register  */

#define USBHC_UHCRHDA	0x0048	/* UHC Root Hub Descriptor A */
#define  UHCRHDA_POTPGT_SHIFT	24	/* Power on to power good time */
#define  UHCRHDA_NOCP	(1<<12)	/* No over current protection */
#define  UHCRHDA_OCPM	(1<<11)	/* Over current protection mode */
#define  UHCRHDA_DT	(1<<10)	/* Device type */
#define  UHCRHDA_NPS	(1<<9)	/* No power switching */
#define  UHCRHDA_PSM	(1<<8)	/* Power switching mode */
#define  UHCRHDA_NDP_MASK	0xff	/* Number downstream ports */
#define USBHC_UHCRHDB	0x004c	/* UHC Root Hub Descriptor B */
#define USBHC_UHCRHS	0x0050	/* UHC Root Hub Stauts */
#define USBHC_UHCHR	0x0064	/* UHC Reset Register */
#define  UHCHR_SSEP3	(1<<11)	/* Sleep standby enable for port3 */
#define  UHCHR_SSEP2	(1<<10)	/* Sleep standby enable for port2 */
#define  UHCHR_SSEP1	(1<<9)	/* Sleep standby enable for port1 */
#define  UHCHR_PCPL	(1<<7)	/* Power control polarity low */
#define  UHCHR_PSPL	(1<<6)	/* Power sense polarity low */
#define  UHCHR_SSE	(1<<5)	/* Sleep standby enable */
#define  UHCHR_UIT	(1<<4)	/* USB interrupt test */
#define  UHCHR_SSDC	(1<<3)	/* Simulation scale down clock */
#define  UHCHR_CGR	(1<<2)	/* Clock generation reset */
#define  UHCHR_FHR	(1<<1)	/* Force host controller reset */
#define  UHCHR_FSBIR	(1<<0)	/* Force system bus interface reset */
#define  UHCHR_MASK	0xeff

/*
 * PWM controller
 */
#define PWM_PWMCR	0x0000	/* Control register */
#define PWM_PWMDCR	0x0004	/* Duty cycle register */
#define  PWM_FD		(1<<10)	/* Full duty */
#define PWM_PWMPCR	0x0008	/* Period register */

/*
 * OS timer
 */
#define	OST_MR0		0x00	/* Match register 0 */
#define	OST_MR1		0x04	/* Match register 1 */
#define	OST_MR2		0x08	/* Match register 2 */
#define	OST_MR3		0x0c	/* Match register 3 */
#define	OST_CR		0x10	/* Count register */
#define	OST_SR		0x14	/* Status register */
#define	 OST_SR_CH0	(1<<0)
#define	 OST_SR_CH1	(1<<1)
#define	 OST_SR_CH2	(1<<2)
#define	 OST_SR_CH3	(1<<3)
#define	OST_WR		0x18	/* Watchdog enable register */
#define	OST_IR		0x1c	/* Interrupt enable register */

/*
 * Interrupt controller
 */
#define	ICU_IP		0x00	/* IRQ pending register */
#define	ICU_MR		0x04	/* Mask register */
#define	ICU_LR		0x08	/* Level register */
#define	ICU_FP		0x0c	/* FIQ pending register */
#define	ICU_PR		0x10	/* Pending register */
#define	ICU_CR		0x14	/* Control register */

#endif /* _ARM_XSCALE_PXAREG_H_ */
