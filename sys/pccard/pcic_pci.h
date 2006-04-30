/*-
 * Copyright (c) 2001 M. Warner Losh. All rights reserved.
 * Copyright (c) 1997 Ted Faber. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, without modification,
 *    this list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Absolutely no warranty of function or purpose is made by the author
 *    Ted Faber.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

/* Share the devid database with NEWCARD */
#include <dev/pccbb/pccbbdevid.h>

/* CL-PD683x CardBus defines */
#define	CLPD6833_CFG_MISC_1		0x98

/* Configuration constants */
#define	CLPD6832_BCR_MGMT_IRQ_ENA	0x0800
#define CLPD6833_CM1_MGMT_EXCA_ENA	0x0001	/* Set ExCA, Clr PCI */

/* End of CL-PD6832 defines */
/* Texas Instruments PCI-1130/1131 CardBus Controller */
#define TI113X_PCI_SYSTEM_CONTROL	0x80	/* System Control */
#define TI12XX_PCI_MULTIMEDIA_CONTROL	0x84	/* Zoom Video */
#define TI12XX_PCI_MFUNC		0x8c	/* multifunction pins */
#define TI113X_PCI_RETRY_STATUS		0x90	/* Retry Status */
#define TI113X_PCI_CARD_CONTROL		0x91	/* Card Control */
#define TI113X_PCI_DEVICE_CONTROL	0x92	/* Device Control */
#define TI113X_PCI_BUFFER_CONTROL	0x93	/* Buffer Control */
#define TI12XX_PCI_DIAGNOSTIC		0x93	/* Diagnostic register */
#define TI113X_PCI_SOCKET_DMA0		0x94	/* Socket DMA Register 0 */
#define TI113X_PCI_SOCKET_DMA1		0x98	/* Socket DMA Register 1 */

/* Card control register (TI113X_SYSTEM_CONTROL == 0x80) */
#define TI113X_SYSCNTL_INTRTIE		0x20000000u
#define TI12XX_SYSCNTL_PCI_CLOCK	0x08000000u
#define TI113X_SYSCNTL_SMIENB		0x00800000u
#define TI113X_SYSCNTL_VCC_PROTECT	0x00200000u
#define TI113X_SYSCNTL_CLKRUN_SEL	0x00000080u
#define	TI113X_SYSCNTL_PWRSAVINGS	0x00000040u
#define TI113X_SYSCNTL_KEEP_CLK		0x00000002u
#define TI113X_SYSCNTL_CLKRUN_ENA	0x00000001u

/* MFUNC register (TI12XX_MFUNC == 0x8c) */
#define TI12XX_MFUNC_PIN0		0x0000000fu
#define   TI12XX_MFUNC_PIN0_INTA	0x2
#define TI12XX_MFUNC_PIN1		0x000000f0u
#define   TI12XX_MFUNC_PIN1_INTB	0x20
#define TI12XX_MFUNC_PIN2		0x00000f00u
#define TI12XX_MFUNC_PIN3		0x0000f000u
#define TI12XX_MFUNC_PIN4		0x000f0000u
#define TI12XX_MFUNC_PIN5		0x00f00000u
#define TI12XX_MFUNC_PIN6		0x0f000000u

/* Card control register (TI113X_CARD_CONTROL == 0x91) */
#define TI113X_CARDCNTL_RING_ENA	0x80u
#define TI113X_CARDCNTL_ZOOM_VIDEO	0x40u
#define TI113X_CARDCNTL_PCI_IRQ_ENA	0x20u
#define TI113X_CARDCNTL_PCI_IREQ	0x10u
#define TI113X_CARDCNTL_PCI_CSC		0x08u
#define	TI113X_CARDCNTL_MASK		(TI113X_CARDCNTL_PCI_IRQ_ENA | TI113X_CARDCNTL_PCI_IREQ | TI113X_CARDCNTL_PCI_CSC)
#define	TI113X_FUNC0_VALID		TI113X_CARDCNTL_MASK
#define	TI113X_FUNC1_VALID		(TI113X_CARDCNTL_PCI_IREQ | TI113X_CARDCNTL_PCI_CSC)
/* Reserved bit				0x04u */
#define TI113X_CARDCNTL_SPKR_ENA	0x02u
#define TI113X_CARDCNTL_INT		0x01u

/* Device control register (TI113X_DEVICE_CONTROL == 0x92) */
#define	TI113X_DEVCNTL_5V_SOCKET	0x40u
#define	TI113X_DEVCNTL_3V_SOCKET	0x20u
#define	TI113X_DEVCNTL_INTR_MASK	0x06u
#define	TI113X_DEVCNTL_INTR_NONE	0x00u
#define	TI113X_DEVCNTL_INTR_ISA		0x02u
#define	TI113X_DEVCNTL_INTR_SERIAL	0x04u
/* TI12XX specific code */
#define	TI12XX_DEVCNTL_INTR_ALLSERIAL	0x06u

/* Diagnostic register (misnamed) TI12XX_PCI_DIAGNOSTIC == 0x93 */
#define TI12XX_DIAG_CSC_INTR		0x20	/* see datasheet */

/* Texas Instruments PCI-1130/1131 CardBus Controller */
#define	TI113X_ExCA_IO_OFFSET0		0x36	/* Offset of I/O window */
#define	TI113X_ExCA_IO_OFFSET1		0x38	/* Offset of I/O window */
#define	TI113X_ExCA_MEM_WINDOW_PAGE	0x3C	/* Memory Window Page */

/*
 * Ricoh R5C47[5678] parts have these registers.  Maybe the 46x also use
 * them, but I can't find out for sure without datasheets...
 */
#define	R5C47X_MISC_CONTROL_REGISTER_2	0xa0
#define R5C47X_MCR2_CSC_TO_INTX_DISABLE 0x0010	/* Bit 7 */

/*
 * Special resister definition for Toshiba ToPIC95/97
 * These values are borrowed from pcmcia-cs/Linux.
 */
#define TOPIC_SOCKET_CTRL  0x90
# define TOPIC_SOCKET_CTRL_SCR_IRQSEL 0x00000001 /* PCI intr */

#define TOPIC_SLOT_CTRL    0xa0
# define TOPIC_SLOT_CTRL_SLOTON       0x00000080
# define TOPIC_SLOT_CTRL_SLOTEN       0x00000040
# define TOPIC_SLOT_CTRL_ID_LOCK      0x00000020
# define TOPIC_SLOT_CTRL_ID_WP        0x00000010
# define TOPIC_SLOT_CTRL_PORT_MASK    0x0000000c
# define TOPIC_SLOT_CTRL_PORT_SHIFT            2
# define TOPIC_SLOT_CTRL_OSF_MASK     0x00000003
# define TOPIC_SLOT_CTRL_OSF_SHIFT             0

# define TOPIC_SLOT_CTRL_INTB         0x00002000
# define TOPIC_SLOT_CTRL_INTA         0x00001000
# define TOPIC_SLOT_CTRL_INT_MASK     0x00003000
# define TOPIC_SLOT_CTRL_CLOCK_MASK   0x00000c00
# define TOPIC_SLOT_CTRL_CLOCK_2      0x00000800 /* PCI Clock/2 */
# define TOPIC_SLOT_CTRL_CLOCK_1      0x00000400 /* PCI Clock */
# define TOPIC_SLOT_CTRL_CLOCK_0      0x00000000 /* no clock */
# define TOPIC97_SLOT_CTRL_STSIRQP    0x00000400 /* status change intr pulse */
# define TOPIC97_SLOT_CTRL_IRQP       0x00000200 /* function intr pulse */
# define TOPIC97_SLOT_CTRL_PCIINT     0x00000100 /* intr routing to PCI INT */

# define TOPIC_SLOT_CTRL_CARDBUS      0x80000000
# define TOPIC_SLOT_CTRL_VS1          0x04000000
# define TOPIC_SLOT_CTRL_VS2          0x02000000
# define TOPIC_SLOT_CTRL_SWDETECT     0x01000000

#define TOPIC_REG_CTRL     0x00a4
# define TOPIC_REG_CTRL_RESUME_RESET  0x80000000
# define TOPIC_REG_CTRL_REMOVE_RESET  0x40000000
# define TOPIC97_REG_CTRL_CLKRUN_ENA  0x20000000
# define TOPIC97_REG_CTRL_TESTMODE    0x10000000
# define TOPIC97_REG_CTRL_IOPLUP      0x08000000
# define TOPIC_REG_CTRL_BUFOFF_PWROFF 0x02000000
# define TOPIC_REG_CTRL_BUFOFF_SIGOFF 0x01000000
# define TOPIC97_REG_CTRL_CB_DEV_MASK 0x0000f800
# define TOPIC97_REG_CTRL_CB_DEV_SHIFT 11
# define TOPIC97_REG_CTRL_RI_DISABLE  0x00000004
# define TOPIC97_REG_CTRL_CAUDIO_OFF  0x00000002
# define TOPIC_REG_CTRL_CAUDIO_INVERT 0x00000001

/* For Bridge Control register (CB_PCI_BRIDGE_CTRL) */
#define CB_BCR_MASTER_ABORT	0x0020
#define CB_BCR_CB_RESET		0x0040
#define CB_BCR_INT_EXCA		0x0080
#define CB_BCR_WRITE_POST_EN	0x0400
  /* additional bits for Ricoh's cardbus products */
#define CB_BCR_RL_3E0_EN	0x0800
#define CB_BCR_RL_3E2_EN	0x1000

/* PCI Configuration Registers (common) */
#define	CB_PCI_VENDOR_ID	0x00	/* vendor ID */
#define	CB_PCI_DEVICE_ID	0x02	/* device ID */
#define	CB_PCI_COMMAND		0x04	/* PCI command */
#define	CB_PCI_STATUS		0x06	/* PCI status */
#define	CB_PCI_REVISION_ID	0x08	/* PCI revision ID */
#define	CB_PCI_CLASS		0x09	/* PCI class code */
#define	CB_PCI_CACHE_LINE_SIZE	0x0c	/* Cache line size */
#define	CB_PCI_LATENCY		0x0d	/* PCI latency timer */
#define	CB_PCI_HEADER_TYPE	0x0e	/* PCI header type */
#define	CB_PCI_BIST		0x0f	/* Built-in self test */
#define	CB_PCI_SOCKET_BASE	0x10	/* Socket/ExCA base address reg. */
#define	CB_PCI_CB_STATUS	0x16	/* CardBus Status */
#define	CB_PCI_PCI_BUS_NUM	0x18	/* PCI bus number */
#define	CB_PCI_CB_BUS_NUM	0x19	/* CardBus bus number */
#define	CB_PCI_CB_SUB_BUS_NUM	0x1A	/* Subordinate CardBus bus number */
#define	CB_PCI_CB_LATENCY	0x1A	/* CardBus latency timer */
#define	CB_PCI_MEMBASE0		0x1C	/* Memory base register 0 */
#define	CB_PCI_MEMLIMIT0	0x20	/* Memory limit register 0 */
#define	CB_PCI_MEMBASE1		0x24	/* Memory base register 1 */
#define	CB_PCI_MEMLIMIT1	0x28	/* Memory limit register 1 */
#define	CB_PCI_IOBASE0		0x2C	/* I/O base register 0 */
#define	CB_PCI_IOLIMIT0		0x30	/* I/O limit register 0 */
#define	CB_PCI_IOBASE1		0x34	/* I/O base register 1 */
#define	CB_PCI_IOLIMIT1		0x38	/* I/O limit register 1 */
#define	CB_PCI_INT_LINE		0x3C	/* Interrupt Line */
#define	CB_PCI_INT_PIN		0x3D	/* Interrupt Pin */
#define	CB_PCI_BRIDGE_CTRL	0x3E	/* Bridge Control */
#define	CB_PCI_SUBSYS_VENDOR_ID	0x40	/* Subsystem Vendor ID */
#define	CB_PCI_SUBSYS_ID	0x42	/* Subsystem ID */
#define	CB_PCI_LEGACY16_IOADDR	0x44	/* Legacy 16bit I/O address */
#define	CB_PCI_LEGACY16_IOENABLE 0x01	/* Enable Legacy 16bit I/O address */

/* PCI Memory register offsets for YENTA devices */
#define CB_SOCKET_EVENT		0x00
#define CB_SOCKET_MASK		0x04
#define CB_SOCKET_STATE		0x08
#define CB_SOCKET_FORCE		0x0c
#define CB_SOCKET_POWER		0x10
#define CB_EXCA_OFFSET		0x800	/* Offset for ExCA registers */

#define CB_SE_CD		0x6	/* Socket Event Card detect */
#define CB_SE_POWER		0x8

#define CB_SM_CD		0x6	/* Socket MASK Card detect */
#define CB_SM_POWER		0x8

/* Socket State Register */
#define CB_SS_CARDSTS		0x00000001 /* Card Status Change */
#define CB_SS_CD1		0x00000002 /* Card Detect 1 */
#define CB_SS_CD2		0x00000004 /* Card Detect 2 */
#define CB_SS_CD		0x00000006 /* Card Detect all */
#define CB_SS_PWRCYCLE		0x00000008 /* Power Cycle */
#define CB_SS_16BIT		0x00000010 /* 16-bit Card */
#define CB_SS_CB		0x00000020 /* Cardbus Card */
#define CB_SS_IREQ		0x00000040 /* Ready */
#define CB_SS_NOTCARD		0x00000080 /* Unrecognized Card */
#define CB_SS_DATALOST		0x00000100 /* Data Lost */
#define CB_SS_BADVCC		0x00000200 /* Bad VccRequest */
#define CB_SS_5VCARD		0x00000400 /* 5 V Card */
#define CB_SS_3VCARD		0x00000800 /* 3.3 V Card */
#define CB_SS_XVCARD		0x00001000 /* X.X V Card */
#define CB_SS_YVCARD		0x00002000 /* Y.Y V Card */
#define CB_SS_CARD_MASK		0x00003c00 /* *VCARD signal */
#define CB_SS_5VSOCK		0x10000000 /* 5 V Socket */
#define CB_SS_3VSOCK		0x20000000 /* 3.3 V Socket */
#define CB_SS_XVSOCK		0x40000000 /* X.X V Socket */
#define CB_SS_YVSOCK		0x80000000 /* Y.Y V Socket */

/* Socket power register */
#define CB_SP_CLKSTOP		0x80	/* Cardbus clock stop protocol */
#define CB_SP_VCC_MASK		0x70
#define CB_SP_VCC_0V		0x00
					/* 0x10 is reserved 12V in VPP */
#define CB_SP_VCC_5V		0x20
#define CB_SP_VCC_3V		0x30
#define CB_SP_VCC_XV		0x40
#define CB_SP_VCC_YV		0x50
					/* 0x60 and 0x70 are reserved */
#define CB_SP_VPP_MASK		0x07
#define CB_SP_VPP_0V		0x00
#define CB_SP_VPP_12V		0x01
#define CB_SP_VPP_5V		0x02
#define CB_SP_VPP_3V		0x03
#define CB_SP_VPP_XV		0x04
#define CB_SP_VPP_YV		0x05

/* Socket force register */
#define CB_SF_INTCVS		(1 << 14)	/* Interregate CVS/CCD pins */
#define CB_SF_5VCARD		(1 << 11)
#define CB_SF_3VCARD		(1 << 10)
#define CB_SF_BADVCC		(1 << 9)
#define CB_SF_DATALOST		(1 << 8)
#define CB_SF_NOTACARD		(1 << 7)
#define CB_SF_CBCARD		(1 << 5)
#define CB_SF_16CARD		(1 << 4)
#define CB_SF_POWERCYCLE	(1 << 3)
#define CB_SF_CCD2		(1 << 2)
#define CB_SF_CCD1		(1 << 1)
#define CB_SF_CSTCHG		(1 << 0)
					/* 0x6 and 0x7 are reserved */
