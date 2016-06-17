/*
 * arch/ppc/platforms/ibm_ocp.h
 *
 *	Definitions for the on-chip peripherals on the IBM
 *	PPC405GP embedded processor.
 *
 * Copyright 2001 MontaVista Softare Inc.
 *
 * This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 *  THIS  SOFTWARE  IS PROVIDED   ``AS  IS'' AND   ANY  EXPRESS OR   IMPLIED
 *  WARRANTIES,   INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 *  NO  EVENT  SHALL   THE AUTHOR  BE    LIABLE FOR ANY   DIRECT,  INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 *  NOT LIMITED   TO, PROCUREMENT OF  SUBSTITUTE GOODS  OR SERVICES; LOSS OF
 *  USE, DATA,  OR PROFITS; OR  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 *  ANY THEORY OF LIABILITY, WHETHER IN  CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 *  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  You should have received a copy of the  GNU General Public License along
 *  with this program; if not, write  to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#ifdef __KERNEL__
#ifndef __ASM_IBM_OCP_H__
#define __ASM_IBM_OCP_H__

#ifndef __ASSEMBLY__
#include <linux/types.h>

//
// TODO: DEPRECATE THIS FILE !
//

 /* PCI 32 */

struct pmm_regs {
	u32 la;
	u32 ma;
	u32 pcila;
	u32 pciha;
};

typedef struct pcil0_regs {
	struct pmm_regs pmm[3];
	u32 ptm1ms;
	u32 ptm1la;
	u32 ptm2ms;
	u32 ptm2la;
} pci0_t;

/* Serial Ports */

#define thr rbr
#define iir fcr
#define dll rbr
#define dlm ier

typedef struct NS16550 {
	u8 rbr;			/* 0 */
	u8 ier;			/* 1 */
	u8 fcr;			/* 2 */
	u8 lcr;			/* 3 */
	u8 mcr;			/* 4 */
	u8 lsr;			/* 5 */
	u8 msr;			/* 6 */
	u8 scr;			/* 7 */
} uart_t;

/* I2c */
typedef struct iic_regs {
	u16 mdbuf;
	u16 sbbuf;
	u8 lmadr;
	u8 hmadr;
	u8 cntl;
	u8 mdcntl;
	u8 sts;
	u8 extsts;
	u8 lsadr;
	u8 hsadr;
	u8 clkdiv;
	u8 intmsk;
	u8 xfrcnt;
	u8 xtcntlss;
	u8 directcntl;
} iic_t;

/* OPB arbiter */
typedef struct opb {
	u8 pr;
	u8 cr;
} opb_t;

/* General purpose i/o */

typedef struct gpio_regs {
	u32 or;
	u32 tcr;
	u32 pad[4];
	u32 odr;
	u32 ir;
} gpio_t;

/* Structure of the memory mapped IDE control.
*/
typedef struct ide_regs {
	unsigned int si_stat;	/* IDE status */
	unsigned int si_intenable;	/* IDE interrupt enable */
	unsigned int si_control;	/* IDE control */
	unsigned int pad0[0x3d];
	unsigned int si_c0rt;	/* Chan 0 Register transfer timing */
	unsigned int si_c0fpt;	/* Chan 0 Fast PIO transfer timing */
	unsigned int si_c0timo;	/* Chan 0 timeout */
	unsigned int pad1[2];
	unsigned int si_c0d0u;	/* Chan 0 UDMA transfer timing */
#define si_c0d0m si_c0d0u	/* Chan 0 Multiword DMA timing */
	unsigned int pad2;
	unsigned int si_c0d1u;	/* Chan 0 dev 1 UDMA timing */
#define si_c0d1m si_c0d1u	/* Chan 0 dev 1 Multiword DMA timing */
	unsigned int si_c0c;	/* Chan 0 Control */
	unsigned int si_c0s0;	/* Chan 0 Status 0 */
	unsigned int si_c0ie;	/* Chan 0 Interrupt Enable */
	unsigned int si_c0s1;	/* Chan 0 Status 0 */
	unsigned int pad4[4];
	unsigned int si_c0dcm;	/* Chan 0 DMA Command */
	unsigned int si_c0tb;	/* Chan 0 PRD Table base address */
	unsigned int si_c0dct;	/* Chan 0 DMA Count */
	unsigned int si_c0da;	/* Chan 0 DMA Address */
	unsigned int si_c0sr;	/* Chan 0 Slew Rate Output Control */
	unsigned char pad5[0xa2];
	unsigned short si_c0adc;	/* Chan 0 Alt status/control */
	unsigned char si_c0d;	/* Chan 0 data */
	unsigned char si_c0ef;	/* Chan 0 error/features */
	unsigned char si_c0sct;	/* Chan 0 sector count */
	unsigned char si_c0sn;	/* Chan 0 sector number */
	unsigned char si_c0cl;	/* Chan 0 cylinder low */
	unsigned char si_c0ch;	/* Chan 0 cylinder high */
	unsigned char si_c0dh;	/* Chan 0 device/head */
	unsigned char si_c0scm;	/* Chan 0 status/command */
} ide_t;

#endif				/* __ASSEMBLY__ */
#endif				/* __ASM_IBM_OCP_H__ */
#endif				/* __KERNEL__ */
