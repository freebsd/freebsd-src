/*
 * Copyright (c) 1998, 1999 Eduardo E. Horvath
 * Copyright (c) 1999 Matthew R. Green
 * All rights reserved.
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
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: NetBSD: psychoreg.h,v 1.8 2001/09/10 16:17:06 eeh Exp
 *
 * $FreeBSD$
 */

#ifndef _SPARC64_PCI_PSYCHOREG_H_
#define _SPARC64_PCI_PSYCHOREG_H_

/*
 * Sun4u PCI definitions.  Here's where we deal w/the machine
 * dependencies of psycho and the PCI controller on the UltraIIi.
 *
 * All PCI registers are bit-swapped, however they are not byte-swapped.
 * This means that they must be accessed using little-endian access modes,
 * either map the pages little-endian or use little-endian ASIs.
 *
 * PSYCHO implements two PCI buses, A and B.
 */

/*
 * psycho register offset.s
 *
 * NB: FFB0 and FFB1 intr map regs also appear at 0x6000 and 0x8000
 * respectively.
 */
#define	PSR_UPA_PORTID		0x0000	/* UPA port ID register */
#define	PSR_UPA_CONFIG		0x0008	/* UPA config register */
#define	PSR_CS			0x0010	/* PSYCHO control/status register */
#define	PSR_ECCC		0x0020	/* ECC control register */
#define	PSR_UE_AFS		0x0030	/* Uncorrectable Error AFSR */
#define	PSR_UE_AFA		0x0038	/* Uncorrectable Error AFAR */
#define	PSR_CE_AFS		0x0040	/* Correctable Error AFSR */
#define	PSR_CE_AFA		0x0048	/* Correctable Error AFAR */
#define	PSR_PM_CTL		0x0100	/* Performance monitor control reg */
#define	PSR_PM_COUNT		0x0108	/* Performance monitor counter reg */
#define	PSR_IOMMU		0x0200	/* IOMMU registers. */
#define	PSR_PCIA0_INT_MAP	0x0c00	/* PCI bus a slot 0 irq map reg */
#define	PSR_PCIA1_INT_MAP	0x0c08	/* PCI bus a slot 1 irq map reg */
#define	PSR_PCIA2_INT_MAP	0x0c10	/* PCI bus a slot 2 irq map reg (IIi) */
#define	PSR_PCIA3_INT_MAP	0x0c18	/* PCI bus a slot 3 irq map reg (IIi) */
#define	PSR_PCIB0_INT_MAP	0x0c20	/* PCI bus b slot 0 irq map reg */
#define	PSR_PCIB1_INT_MAP	0x0c28	/* PCI bus b slot 1 irq map reg */
#define	PSR_PCIB2_INT_MAP	0x0c30	/* PCI bus b slot 2 irq map reg */
#define	PSR_PCIB3_INT_MAP	0x0c38	/* PCI bus b slot 3 irq map reg */
#define	PSR_SCSI_INT_MAP	0x1000	/* SCSI interrupt map reg */
#define	PSR_ETHER_INT_MAP	0x1008	/* ethernet interrupt map reg */
#define	PSR_BPP_INT_MAP		0x1010	/* parallel interrupt map reg */
#define	PSR_AUDIOR_INT_MAP	0x1018	/* audio record interrupt map reg */
#define	PSR_AUDIOP_INT_MAP	0x1020	/* audio playback interrupt map reg */
#define	PSR_POWER_INT_MAP	0x1028	/* power fail interrupt map reg */
#define	PSR_SKBDMS_INT_MAP	0x1030	/* serial/kbd/mouse interrupt map reg */
#define	PSR_FD_INT_MAP		0x1038	/* floppy interrupt map reg */
#define	PSR_SPARE_INT_MAP	0x1040	/* spare interrupt map reg */
#define	PSR_KBD_INT_MAP		0x1048	/* kbd [unused] interrupt map reg */
#define	PSR_MOUSE_INT_MAP	0x1050	/* mouse [unused] interrupt map reg */
#define	PSR_SERIAL_INT_MAP	0x1058	/* second serial interrupt map reg */
#define	PSR_TIMER0_INT_MAP	0x1060	/* timer 0 interrupt map reg */
#define	PSR_TIMER1_INT_MAP	0x1068	/* timer 1 interrupt map reg */
#define	PSR_UE_INT_MAP		0x1070	/* UE interrupt map reg */
#define	PSR_CE_INT_MAP		0x1078	/* CE interrupt map reg */
#define	PSR_PCIAERR_INT_MAP	0x1080	/* PCI bus a error interrupt map reg */
#define	PSR_PCIBERR_INT_MAP	0x1088	/* PCI bus b error interrupt map reg */
#define	PSR_PWRMGT_INT_MAP	0x1090	/* power mgmt wake interrupt map reg */
#define	PSR_FFB0_INT_MAP	0x1098	/* FFB0 graphics interrupt map reg */
#define	PSR_FFB1_INT_MAP	0x10a0	/* FFB1 graphics interrupt map reg */
/* Note: clear interrupt 0 registers are not really used */
#define	PSR_PCIA0_INT_CLR	0x1400	/* PCI a slot 0 clear int regs 0..3 */
#define	PSR_PCIA1_INT_CLR	0x1420	/* PCI a slot 1 clear int regs 0..3 */
#define	PSR_PCIA2_INT_CLR	0x1440	/* PCI a slot 1 clear int regs 0..3 */
#define	PSR_PCIA3_INT_CLR	0x1460	/* PCI a slot 1 clear int regs 0..3 */
#define	PSR_PCIB0_INT_CLR	0x1480	/* PCI b slot 0 clear int regs 0..3 */
#define	PSR_PCIB1_INT_CLR	0x14a0	/* PCI b slot 1 clear int regs 0..3 */
#define	PSR_PCIB2_INT_CLR	0x14c0	/* PCI b slot 2 clear int regs 0..3 */
#define	PSR_PCIB3_INT_CLR	0x14d0	/* PCI b slot 3 clear int regs 0..3 */
#define	PSR_SCSI_INT_CLR	0x1800	/* SCSI clear int reg */
#define	PSR_ETHER_INT_CLR	0x1808	/* ethernet clear int reg */
#define	PSR_BPP_INT_CLR		0x1810	/* parallel clear int reg */
#define	PSR_AUDIOR_INT_CLR	0x1818	/* audio record clear int reg */
#define	PSR_AUDIOP_INT_CLR	0x1820	/* audio playback clear int reg */
#define	PSR_POWER_INT_CLR	0x1828	/* power fail clear int reg */
#define	PSR_SKBDMS_INT_CLR	0x1830	/* serial/kbd/mouse clear int reg */
#define	PSR_FD_INT_CLR		0x1838	/* floppy clear int reg */
#define	PSR_SPARE_INT_CLR	0x1840	/* spare clear int reg */
#define	PSR_KBD_INT_CLR		0x1848	/* kbd [unused] clear int reg */
#define	PSR_MOUSE_INT_CLR	0x1850	/* mouse [unused] clear int reg */
#define	PSR_SERIAL_INT_CLR	0x1858	/* second serial clear int reg */
#define	PSR_TIMER0_INT_CLR	0x1860	/* timer 0 clear int reg */
#define	PSR_TIMER1_INT_CLR	0x1868	/* timer 1 clear int reg */
#define	PSR_UE_INT_CLR		0x1870	/* UE clear int reg */
#define	PSR_CE_INT_CLR		0x1878	/* CE clear int reg */
#define	PSR_PCIAERR_INT_CLR	0x1880	/* PCI bus a error clear int reg */
#define	PSR_PCIBERR_INT_CLR	0x1888	/* PCI bus b error clear int reg */
#define	PSR_PWRMGT_INT_CLR	0x1890	/* power mgmt wake clr interrupt reg */
#define	PSR_INTR_RETRY_TIM	0x1a00	/* interrupt retry timer */
#define	PSR_TC0			0x1c00	/* timer/counter 0 */
#define	PSR_TC1			0x1c10	/* timer/counter 1 */
#define	PSR_DMA_WRITE_SYNC	0x1c20	/* PCI DMA write sync register (IIi) */
#define	PSR_PCICTL0		0x2000	/* PCICTL registers for 1st psycho. */
#define	PSR_PCICTL1		0x4000	/* PCICTL registers for 2nd psycho. */
#define	PSR_DMA_SCB_DIAG0	0xa000	/* DMA scoreboard diag reg 0 */
#define	PSR_DMA_SCB_DIAG1	0xa008	/* DMA scoreboard diag reg 1 */
#define	PSR_IOMMU_SVADIAG	0xa400	/* IOMMU virtual addr diag reg */
#define	PSR_IOMMU_TLB_CMP_DIAG	0xa408	/* IOMMU TLB tag compare diag reg */
#define	PSR_IOMMU_QUEUE_DIAG	0xa500	/* IOMMU LRU queue diag regs 0..15 */
#define	PSR_IOMMU_TLB_TAG_DIAG	0xa580	/* TLB tag diag regs 0..15 */
#define	PSR_IOMMU_TLB_DATA_DIAG	0xa600	/* TLB data RAM diag regs 0..15 */
#define	PSR_PCI_INT_DIAG	0xa800	/* PCI int state diag reg */
#define	PSR_OBIO_INT_DIAG	0xa808	/* OBIO and misc int state diag reg */
#define	PSR_STRBUF_DIAG		0xb000	/* Streaming buffer diag regs */
/*
 * Here is the rest of the map, which we're not specifying:
 *
 * 1fe.0100.0000 - 1fe.01ff.ffff	PCI configuration space
 * 1fe.0100.0000 - 1fe.0100.00ff	PCI B configuration header
 * 1fe.0101.0000 - 1fe.0101.00ff	PCI A configuration header
 * 1fe.0200.0000 - 1fe.0200.ffff	PCI A I/O space
 * 1fe.0201.0000 - 1fe.0201.ffff	PCI B I/O space
 * 1ff.0000.0000 - 1ff.7fff.ffff	PCI A memory space
 * 1ff.8000.0000 - 1ff.ffff.ffff	PCI B memory space
 *
 * NB: config and I/O space can use 1-4 byte accesses, not 8 byte
 * accesses.  Memory space can use any sized accesses.
 *
 * Note that the SUNW,sabre/SUNW,simba combinations found on the
 * Ultra5 and Ultra10 machines uses slightly differrent addresses
 * than the above.  This is mostly due to the fact that the APB is
 * a multi-function PCI device with two PCI bridges, and the U2P is
 * two separate PCI bridges.  It uses the same PCI configuration
 * space, though the configuration header for each PCI bus is
 * located differently due to the SUNW,simba PCI busses being
 * function 0 and function 1 of the APB, whereas the psycho's are
 * each their own PCI device.  The I/O and memory spaces are each
 * split into 8 equally sized areas (8x2MB blocks for I/O space,
 * and 8x512MB blocks for memory space).  These are allocated in to
 * either PCI A or PCI B, or neither in the APB's `I/O Address Map
 * Register A/B' (0xde) and `Memory Address Map Register A/B' (0xdf)
 * registers of each simba.  We must ensure that both of the
 * following are correct (the prom should do this for us):
 *
 *    (PCI A Memory Address Map) & (PCI B Memory Address Map) == 0
 *
 *    (PCI A I/O Address Map) & (PCI B I/O Address Map) == 0
 *
 * 1fe.0100.0000 - 1fe.01ff.ffff	PCI configuration space
 * 1fe.0100.0800 - 1fe.0100.08ff	PCI B configuration header
 * 1fe.0100.0900 - 1fe.0100.09ff	PCI A configuration header
 * 1fe.0200.0000 - 1fe.02ff.ffff	PCI I/O space (divided)
 * 1ff.0000.0000 - 1ff.ffff.ffff	PCI memory space (divided)
 */

/*
 * PSR_CS defines:
 *
 * 63     59     55     50     45     4        3       2     1      0
 * +------+------+------+------+--//---+--------+-------+-----+------+
 * | IMPL | VERS | MID  | IGN  |  xxx  | APCKEN | APERR | IAP | MODE |
 * +------+------+------+------+--//---+--------+-------+-----+------+
 *
 */
#define PSYCHO_GCSR_IMPL(csr)	((u_int)(((csr) >> 60) & 0xf))
#define PSYCHO_GCSR_VERS(csr)	((u_int)(((csr) >> 56) & 0xf))
#define PSYCHO_GCSR_MID(csr)	((u_int)(((csr) >> 51) & 0x1f))
#define PSYCHO_GCSR_IGN(csr)	((u_int)(((csr) >> 46) & 0x1f))
#define PSYCHO_CSR_APCKEN	8	/* UPA addr parity check enable */
#define PSYCHO_CSR_APERR	4	/* UPA addr parity error */
#define PSYCHO_CSR_IAP		2	/* invert UPA address parity */
#define PSYCHO_CSR_MODE		1	/* UPA/PCI handshake */

/* Offsets into the PSR_PCICTL* register block. */
#define	PCR_CS			0x0000	/* PCI control/status register */
#define	PCR_AFS			0x0010	/* PCI AFSR register */
#define	PCR_AFA			0x0018	/* PCI AFAR register */
#define	PCR_DIAG		0x0020	/* PCI diagnostic register */
#define	PCR_TAS			0x0028	/* PCI target address space reg (IIi) */
#define	PCR_STRBUF		0x0800	/* IOMMU streaming buffer registers. */

/* Device space defines. */
#define	PSYCHO_CONF_SIZE	0x1000000
#define	PSYCHO_CONF_BUS_SHIFT	16
#define	PSYCHO_CONF_DEV_SHIFT	11
#define	PSYCHO_CONF_FUNC_SHIFT	8
#define	PSYCHO_CONF_REG_SHIFT	0
#define	PSYCHO_IO_SIZE		0x1000000
#define	PSYCHO_MEM_SIZE		0x100000000

#define	PSYCHO_CONF_OFF(bus, slot, func, reg)				\
	(((bus) << PSYCHO_CONF_BUS_SHIFT) |				\
	((slot) << PSYCHO_CONF_DEV_SHIFT) |				\
	((func) << PSYCHO_CONF_FUNC_SHIFT) |				\
	((reg) << PSYCHO_CONF_REG_SHIFT))

/* what the bits mean! */

/* PCI [a|b] control/status register */
/* note that the sabre only has one set of PCI control/status registers */
#define	PCICTL_MRLM	0x0000001000000000	/* Memory Read Line/Multiple */
#define	PCICTL_SERR	0x0000000400000000	/* SERR asserted; W1C */
#define	PCICTL_ARB_PARK	0x0000000000200000	/* PCI arbitration parking */
#define	PCICTL_CPU_PRIO	0x0000000000100000	/* PCI arbitration parking */
#define	PCICTL_ARB_PRIO	0x00000000000f0000	/* PCI arbitration parking */
#define	PCICTL_ERRINTEN	0x0000000000000100	/* PCI error interrupt enable */
#define	PCICTL_RTRYWAIT 0x0000000000000080	/* PCI error interrupt enable */
#define	PCICTL_4ENABLE	0x000000000000000f	/* enable 4 PCI slots */
#define	PCICTL_6ENABLE	0x000000000000003f	/* enable 6 PCI slots */

/* Uncorrectable error asynchronous fault status registers */
#define	UEAFSR_BLK	(1UL << 22)	/* pri. error caused by read */
#define	UEAFSR_P_DTE	(1UL << 56)	/* pri. DMA translation error */
#define	UEAFSR_S_DTE	(1UL << 57)	/* sec. DMA translation error */
#define	UEAFSR_S_DWR	(1UL << 58)	/* sec. error during write */
#define	UEAFSR_S_DRD	(1UL << 59)	/* sec. error during read */
#define	UEAFSR_P_DWR	(1UL << 61)	/* pri. error during write */
#define	UEAFSR_P_DRD	(1UL << 62)	/* pri. error during read */

/*
 * these are the PROM structures we grovel
 */

/*
 * For the physical adddresses split into 3 32 bit values, we deocde
 * them like the following (IEEE1275 PCI Bus binding 2.0, 2.2.1.1
 * Numerical Representation):
 *
 * 	phys.hi cell:	npt000ss bbbbbbbb dddddfff rrrrrrrr
 * 	phys.mid cell:	hhhhhhhh hhhhhhhh hhhhhhhh hhhhhhhh
 * 	phys.lo cell:	llllllll llllllll llllllll llllllll
 *
 * where these bits affect the address' properties:
 *	n	not-relocatable
 *	p	prefetchable
 *	t	aliased (non-relocatable IO), below 1MB (memory) or
 *		below 64KB (reloc. IO)
 *	ss	address space code:
 *		00 - configuration space
 *		01 - I/O space
 *		10 - 32 bit memory space
 *		11 - 64 bit memory space
 *	bb..bb	8 bit bus number
 *	ddddd	5 bit device number
 *	fff	3 bit function number
 *	rr..rr	8 bit register number
 *	hh..hh	32 bit unsigned value
 *	ll..ll	32 bit unsigned value
 * the values of hh..hh and ll..ll are combined to form a larger number.
 *
 * For config space, we don't have to do much special.  For I/O space,
 * hh..hh must be zero, and if n == 0 ll..ll is the offset from the
 * start of I/O space, otherwise ll..ll is the I/O space.  For memory
 * space, hh..hh must be zero for the 32 bit space, and is the high 32
 * bits in 64 bit space, with ll..ll being the low 32 bits in both cases,
 * with offset handling being driver via `n == 0' as for I/O space.
 */

/* commonly used */
#define TAG2BUS(tag)	((tag) >> 16) & 0xff;
#define TAG2DEV(tag)	((tag) >> 11) & 0x1f;
#define TAG2FN(tag)	((tag) >> 8) & 0x7;

#define	INTPCI_MAXOBINO	0x16		/* maximum OBIO INO value for PCI */
#define	INTPCIOBINOX(x)	((x) & 0x1f)	/* OBIO ino index (for PCI machines) */
#define	INTPCIINOX(x)	(((x) & 0x1c) >> 2)	/* PCI ino index */

#endif /* _SPARC64_PCI_PSYCHOREG_H_ */
