/*
 * Copyright (c) 2000,2001 Jonathan Chen.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

/*
 * Copyright (c) 1998, 1999 and 2000
 *      HAYAKAWA Koichi.  All rights reserved.
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
 *	This product includes software developed by HAYAKAWA Koichi.
 * 4. The name of the author may not be used to endorse or promote products
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
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Register definitions for PCI to Cardbus Bridge chips
 */


/* PCI header registers */
#define	CBBR_SOCKBASE				0x10	/* len=4 */

#define	CBBR_MEMBASE0				0x1c	/* len=4 */
#define	CBBR_MEMLIMIT0				0x20	/* len=4 */
#define	CBBR_MEMBASE1				0x24	/* len=4 */
#define	CBBR_MEMLIMIT1				0x28	/* len=4 */
#define	CBBR_IOBASE0				0x2c	/* len=4 */
#define	CBBR_IOLIMIT0				0x30	/* len=4 */
#define	CBBR_IOBASE1				0x34	/* len=4 */
#define	CBBR_IOLIMIT1				0x38	/* len=4 */
#define	CBB_MEMALIGN				4096
#define	CBB_IOALIGN				4

#define	CBBR_INTRLINE				0x3c	/* len=1 */
#define	CBBR_INTRPIN				0x3d	/* len=1 */
#define	CBBR_BRIDGECTRL				0x3e	/* len=2 */
# define	CBBM_BRIDGECTRL_MASTER_ABORT		0x0020
# define	CBBM_BRIDGECTRL_RESET			0x0040
# define	CBBM_BRIDGECTRL_INTR_IREQ_EN		0x0080
# define	CBBM_BRIDGECTRL_PREFETCH_0		0x0100
# define	CBBM_BRIDGECTRL_PREFETCH_1		0x0200
# define	CBBM_BRIDGECTRL_WRITE_POST_EN		0x0400
  /* additional bit for RF5C46[567] */
# define	CBBM_BRIDGECTRL_RL_3E0_EN		0x0800
# define	CBBM_BRIDGECTRL_RL_3E2_EN		0x1000

#define	CBBR_LEGACY				0x44	/* len=4 */

/* TI * */
#define CBBR_SYSCTRL				0x80	/* len=4 */
# define	CBBM_SYSCTRL_INTRTIE			0x20000000u

/* TI [14][245]xx */
#define CBBR_MMCTRL				0x84	/* len=4 */

/* TI 12xx/14xx/15xx (except 1250/1251/1251B/1450) */
#define CBBR_MFUNC				0x8c	/* len=4 */
# define	CBBM_MFUNC_PIN0				0x0000000f
# define		CBBM_MFUNC_PIN0_INTA			0x02
# define	CBBM_MFUNC_PIN1				0x000000f0
# define		CBBM_MFUNC_PIN1_INTB			0x20
# define	CBBM_MFUNC_PIN2				0x00000f00
# define	CBBM_MFUNC_PIN3				0x0000f000
# define	CBBM_MFUNC_PIN4				0x000f0000
# define	CBBM_MFUNC_PIN5				0x00f00000
# define	CBBM_MFUNC_PIN6				0x0f000000

#define	CBBR_CBCTRL				0x91	/* len=1 */
  /* bits for TI 113X */
# define	CBBM_CBCTRL_113X_RI_EN		0x80
# define	CBBM_CBCTRL_113X_ZV_EN		0x40
# define	CBBM_CBCTRL_113X_PCI_IRQ_EN		0x20
# define	CBBM_CBCTRL_113X_PCI_INTR		0x10
# define	CBBM_CBCTRL_113X_PCI_CSC		0x08
# define	CBBM_CBCTRL_113X_PCI_CSC_D		0x04
# define	CBBM_CBCTRL_113X_SPEAKER_EN		0x02
# define	CBBM_CBCTRL_113X_INTR_DET		0x01
  /* TI [14][245]xx */
# define	CBBM_CBCTRL_12XX_RI_EN		0x80
# define	CBBM_CBCTRL_12XX_ZV_EN		0x40
# define	CBBM_CBCTRL_12XX_AUD2MUX		0x04
# define	CBBM_CBCTRL_12XX_SPEAKER_EN		0x02
# define	CBBM_CBCTRL_12XX_INTR_DET		0x01
#define	CBBR_DEVCTRL				0x92	/* len=1 */
# define	CBBM_DEVCTRL_INT_SERIAL		0x04
# define	CBBM_DEVCTRL_INT_PCI			0x02

/* ToPIC 95 ONLY */
#define	CBBR_TOPIC_SOCKETCTRL			0x90
# define	CBBM_TOPIC_SOCKETCTRL_SCR_IRQSEL	0x00000001 /* PCI intr */
/* ToPIC 97, 100 */
#define CBBR_TOPIC_ZV_CONTROL			0x9c	/* 1 byte */
# define	CBBM_TOPIC_ZVC_ENABLE			0x1

/* TOPIC 95+ */
#define	CBBR_TOPIC_SLOTCTRL			0xa0	/* 1 byte */
# define	CBBM_TOPIC_SLOTCTRL_SLOTON		0x80
# define	CBBM_TOPIC_SLOTCTRL_SLOTEN		0x40
# define	CBBM_TOPIC_SLOTCTRL_ID_LOCK		0x20
# define	CBBM_TOPIC_SLOTCTRL_ID_WP		0x10
# define	CBBM_TOPIC_SLOTCTRL_PORT_MASK		0x0c
# define	CBBM_TOPIC_SLOTCTRL_PORT_SHIFT		2
# define	CBBM_TOPIC_SLOTCTRL_OSF_MASK		0x03
# define	CBBM_TOPIC_SLOTCTRL_OSF_SHIFT		0

/* TOPIC 95+ */
#define CBBR_TOPIC_INTCTRL			0xa1	/* 1 byte */
# define	CBBM_TOPIC_INTCTRL_INTB			0x20
# define	CBBM_TOPIC_INTCTRL_INTA			0x10
# define	CBBM_TOPIC_INTCTRL_INT_MASK		0x30
/* The following bits may be for ToPIC 95 only */
# define	CBBM_TOPIC_INTCTRL_CLOCK_MASK		0x0c
# define	CBBM_TOPIC_INTCTRL_CLOCK_2		0x08 /* PCI Clk/2 */
# define	CBBM_TOPIC_INTCTRL_CLOCK_1		0x04 /* PCI Clk */
# define	CBBM_TOPIC_INTCTRL_CLOCK_0		0x00 /* no clock */
/* ToPIC97, 100 defines the following bits */
# define	CBBM_TOPIC_INTCTRL_STSIRQNP		0x04
# define	CBBM_TOPIC_INTCTRL_IRQNP		0x02
# define	CBBM_TOPIC_INTCTRL_INTIRQSEL		0x01

/* TOPIC 95+ */
#define CBBR_TOPIC_CDC			0xa3	/* 1 byte */
# define	CBBM_TOPIC_CDC_CARDBUS			0x80
# define	CBBM_TOPIC_CDC_VS1			0x04
# define	CBBM_TOPIC_CDC_VS2			0x02
# define	CBBM_TOPIC_CDC_SWDETECT			0x01

/* Socket definitions */
#define	CBB_SOCKET_EVENT_CSTS		0x01	/* Card Status Change */
#define	CBB_SOCKET_EVENT_CD1		0x02	/* Card Detect 1 */
#define	CBB_SOCKET_EVENT_CD2		0x04	/* Card Detect 2 */
#define	CBB_SOCKET_EVENT_CD		0x06	/* Card Detect all */
#define	CBB_SOCKET_EVENT_POWER		0x08	/* Power Cycle */

#define	CBB_SOCKET_MASK_CSTS		0x01	/* Card Status Change */
#define	CBB_SOCKET_MASK_CD		0x06	/* Card Detect */
#define	CBB_SOCKET_MASK_POWER		0x08	/* Power Cycle */
#define	CBB_SOCKET_MASK_ALL		0x0F	/* all of the above */

#define	CBB_SOCKET_STAT_CARDSTS		0x00000001	/* Card Status Change */
#define	CBB_SOCKET_STAT_CD1		0x00000002	/* Card Detect 1 */
#define	CBB_SOCKET_STAT_CD2		0x00000004	/* Card Detect 2 */
#define	CBB_SOCKET_STAT_CD		0x00000006	/* Card Detect all */
#define	CBB_SOCKET_STAT_PWRCYCLE	0x00000008	/* Power Cycle */
#define	CBB_SOCKET_STAT_16BIT		0x00000010	/* 16-bit Card */
#define	CBB_SOCKET_STAT_CB		0x00000020	/* Cardbus Card */
#define	CBB_SOCKET_STAT_IREQ		0x00000040	/* Ready */
#define	CBB_SOCKET_STAT_NOTCARD		0x00000080	/* Unrecognized Card */
#define	CBB_SOCKET_STAT_DATALOST	0x00000100	/* Data Lost */
#define	CBB_SOCKET_STAT_BADVCC		0x00000200	/* Bad VccRequest */
#define	CBB_SOCKET_STAT_5VCARD		0x00000400	/* 5 V Card */
#define	CBB_SOCKET_STAT_3VCARD		0x00000800	/* 3.3 V Card */
#define	CBB_SOCKET_STAT_XVCARD		0x00001000	/* X.X V Card */
#define	CBB_SOCKET_STAT_YVCARD		0x00002000	/* Y.Y V Card */
#define	CBB_SOCKET_STAT_5VSOCK		0x10000000	/* 5 V Socket */
#define	CBB_SOCKET_STAT_3VSOCK		0x20000000	/* 3.3 V Socket */
#define	CBB_SOCKET_STAT_XVSOCK		0x40000000	/* X.X V Socket */
#define	CBB_SOCKET_STAT_YVSOCK		0x80000000	/* Y.Y V Socket */

#define	CBB_SOCKET_CTRL_VPPMASK		0x07
#define	CBB_SOCKET_CTRL_VPP_OFF		0x00
#define	CBB_SOCKET_CTRL_VPP_12V		0x01
#define	CBB_SOCKET_CTRL_VPP_5V		0x02
#define	CBB_SOCKET_CTRL_VPP_3V		0x03
#define	CBB_SOCKET_CTRL_VPP_XV		0x04
#define	CBB_SOCKET_CTRL_VPP_YV		0x05

#define	CBB_SOCKET_CTRL_VCCMASK		0x70
#define	CBB_SOCKET_CTRL_VCC_OFF		0x00
#define	CBB_SOCKET_CTRL_VCC_5V		0x20
#define	CBB_SOCKET_CTRL_VCC_3V		0x30
#define	CBB_SOCKET_CTRL_VCC_XV		0x40
#define	CBB_SOCKET_CTRL_VCC_YV		0x50

#define	CBB_SOCKET_CTRL_STOPCLK		0x80

#include <dev/pccbb/pccbbdevid.h>

#define CBB_SOCKET_EVENT		0x00
#define CBB_SOCKET_MASK			0x04
#define CBB_SOCKET_STATE		0x08
#define CBB_SOCKET_FORCE		0x0c
#define CBB_SOCKET_CONTROL		0x10
#define CBB_SOCKET_POWER		0x14

#define CBB_EXCA_OFFSET			0x800	/* offset for exca regs */
