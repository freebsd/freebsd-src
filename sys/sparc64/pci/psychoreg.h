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

struct psychoreg {
	struct upareg {
		/* UPA port ID register */		/* 1fe.0000.0000 */
		u_int64_t	upa_portid;
		/* UPA config register */		/* 1fe.0000.0008 */
		u_int64_t	upa_config;
	} sys_upa;

	/* PSYCHO control/status register */		/* 1fe.0000.0010 */
	u_int64_t	psy_csr;
	/* 
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

	u_int64_t	pad0;
	/* ECC control register */			/* 1fe.0000.0020 */
	u_int64_t	psy_ecccr;
							/* 1fe.0000.0028 */
	u_int64_t	reserved;
	/* Uncorrectable Error AFSR */			/* 1fe.0000.0030 */
	u_int64_t	psy_ue_afsr;
	/* Uncorrectable Error AFAR */			/* 1fe.0000.0038 */
	u_int64_t	psy_ue_afar;
	/* Correctable Error AFSR */			/* 1fe.0000.0040 */
	u_int64_t	psy_ce_afsr;
	/* Correctable Error AFAR */			/* 1fe.0000.0048 */
	u_int64_t	psy_ce_afar;

	u_int64_t	pad1[22];

	struct perfmon {
		/* Performance monitor control reg */	/* 1fe.0000.0100 */		
		u_int64_t	pm_cr;
		/* Performance monitor counter reg */	/* 1fe.0000.0108 */
		u_int64_t	pm_count;
	} psy_pm;

	u_int64_t	pad2[30];

							/* 1fe.0000.0200,0210 */
	struct iommureg psy_iommu;

	u_int64_t	pad3[317];

	/* PCI bus a slot 0 irq map reg */		/* 1fe.0000.0c00 */
	u_int64_t	pcia0_int_map;
	/* PCI bus a slot 1 irq map reg */		/* 1fe.0000.0c08 */
	u_int64_t	pcia1_int_map;
	/* PCI bus a slot 2 irq map reg (IIi) */	/* 1fe.0000.0c10 */	
	u_int64_t	pcia2_int_map;
	/* PCI bus a slot 3 irq map reg (IIi) */	/* 1fe.0000.0c18 */
	u_int64_t	pcia3_int_map;
	/* PCI bus b slot 0 irq map reg */		/* 1fe.0000.0c20 */
	u_int64_t	pcib0_int_map;
	/* PCI bus b slot 1 irq map reg */		/* 1fe.0000.0c28 */
	u_int64_t	pcib1_int_map;
	/* PCI bus b slot 2 irq map reg */		/* 1fe.0000.0c30 */
	u_int64_t	pcib2_int_map;
	/* PCI bus b slot 3 irq map reg */		/* 1fe.0000.0c38 */
	u_int64_t	pcib3_int_map;

	u_int64_t	pad4[120];

	/* SCSI interrupt map reg */			/* 1fe.0000.1000 */
	u_int64_t	scsi_int_map;
	/* ethernet interrupt map reg */		/* 1fe.0000.1008 */
	u_int64_t	ether_int_map;
	/* parallel interrupt map reg */		/* 1fe.0000.1010 */
	u_int64_t	bpp_int_map;
	/* audio record interrupt map reg */		/* 1fe.0000.1018 */
	u_int64_t	audior_int_map;
	/* audio playback interrupt map reg */		/* 1fe.0000.1020 */
	u_int64_t	audiop_int_map;
	/* power fail interrupt map reg */		/* 1fe.0000.1028 */
	u_int64_t	power_int_map;
	/* serial/kbd/mouse interrupt map reg */	/* 1fe.0000.1030 */
	u_int64_t	ser_kbd_ms_int_map;
	/* floppy interrupt map reg */			/* 1fe.0000.1038 */
	u_int64_t	fd_int_map;
	/* spare interrupt map reg */			/* 1fe.0000.1040 */
	u_int64_t	spare_int_map;
	/* kbd [unused] interrupt map reg */		/* 1fe.0000.1048 */
	u_int64_t	kbd_int_map;
	/* mouse [unused] interrupt map reg */		/* 1fe.0000.1050 */
	u_int64_t	mouse_int_map;
	/* second serial interrupt map reg */		/* 1fe.0000.1058 */
	u_int64_t	serial_int_map;
	/* timer 0 interrupt map reg */			/* 1fe.0000.1060 */
	u_int64_t	timer0_int_map;
	/* timer 1 interrupt map reg */			/* 1fe.0000.1068 */
	u_int64_t	timer1_int_map;
	/* UE interrupt map reg */			/* 1fe.0000.1070 */
	u_int64_t	ue_int_map;
	/* CE interrupt map reg */			/* 1fe.0000.1078 */
	u_int64_t	ce_int_map;
	/* PCI bus a error interrupt map reg */		/* 1fe.0000.1080 */
	u_int64_t	pciaerr_int_map;
	/* PCI bus b error interrupt map reg */		/* 1fe.0000.1088 */
	u_int64_t	pciberr_int_map;	
	/* power mgmt wake interrupt map reg */		/* 1fe.0000.1090 */
	u_int64_t	pwrmgt_int_map;
	/* FFB0 graphics interrupt map reg */		/* 1fe.0000.1098 */
	u_int64_t	ffb0_int_map;
	/* FFB1 graphics interrupt map reg */		/* 1fe.0000.10a0 */
	u_int64_t	ffb1_int_map;
	
	u_int64_t	pad5[107];

	/* Note: clear interrupt 0 registers are not really used */

	/* PCI a slot 0 clear int regs 0..7 */		/* 1fe.0000.1400-1418 */
	u_int64_t	pcia0_int_clr[4];
	/* PCI a slot 1 clear int regs 0..7 */		/* 1fe.0000.1420-1438 */
	u_int64_t	pcia1_int_clr[4];
	/* PCI a slot 2 clear int regs 0..7 */		/* 1fe.0000.1440-1458 */
	u_int64_t	pcia2_int_clr[4];
	/* PCI a slot 3 clear int regs 0..7 */		/* 1fe.0000.1480-1478 */
	u_int64_t	pcia3_int_clr[4];
	/* PCI b slot 0 clear int regs 0..7 */		/* 1fe.0000.1480-1498 */
	u_int64_t	pcib0_int_clr[4];
	/* PCI b slot 1 clear int regs 0..7 */		/* 1fe.0000.14a0-14b8 */
	u_int64_t	pcib1_int_clr[4];
	/* PCI b slot 2 clear int regs 0..7 */		/* 1fe.0000.14c0-14d8 */
	u_int64_t	pcib2_int_clr[4];
	/* PCI b slot 3 clear int regs 0..7 */		/* 1fe.0000.14d0-14f8 */
	u_int64_t	pcib3_int_clr[4];

	u_int64_t	pad6[96];

	/* SCSI clear int reg */			/* 1fe.0000.1800 */
	u_int64_t	scsi_int_clr;
	/* ethernet clear int reg */			/* 1fe.0000.1808 */
	u_int64_t	ether_int_clr;
	/* parallel clear int reg */			/* 1fe.0000.1810 */
	u_int64_t	bpp_int_clr;
	/* audio record clear int reg */		/* 1fe.0000.1818 */
	u_int64_t	audior_int_clr;
	/* audio playback clear int reg */		/* 1fe.0000.1820 */
	u_int64_t	audiop_int_clr;
	/* power fail clear int reg */			/* 1fe.0000.1828 */
	u_int64_t	power_int_clr;
	/* serial/kbd/mouse clear int reg */		/* 1fe.0000.1830 */
	u_int64_t	ser_kb_ms_int_clr;
	/* floppy clear int reg */			/* 1fe.0000.1838 */
	u_int64_t	fd_int_clr;
	/* spare clear int reg */			/* 1fe.0000.1840 */
	u_int64_t	spare_int_clr;
	/* kbd [unused] clear int reg */		/* 1fe.0000.1848 */
	u_int64_t	kbd_int_clr;
	/* mouse [unused] clear int reg */		/* 1fe.0000.1850 */
	u_int64_t	mouse_int_clr;
	/* second serial clear int reg */		/* 1fe.0000.1858 */
	u_int64_t	serial_clr;
	/* timer 0 clear int reg */			/* 1fe.0000.1860 */
	u_int64_t	timer0_int_clr;
	/* timer 1 clear int reg */			/* 1fe.0000.1868 */
	u_int64_t	timer1_int_clr;
	/* UE clear int reg */				/* 1fe.0000.1870 */
	u_int64_t	ue_int_clr;
	/* CE clear int reg */				/* 1fe.0000.1878 */
	u_int64_t	ce_int_clr;
	/* PCI bus a error clear int reg */		/* 1fe.0000.1880 */
	u_int64_t	pciaerr_int_clr;
	/* PCI bus b error clear int reg */		/* 1fe.0000.1888 */
	u_int64_t	pciberr_int_clr;
	/* power mgmt wake clr interrupt reg */		/* 1fe.0000.1890 */
	u_int64_t	pwrmgt_int_clr;

	u_int64_t	pad7[45];

	/* interrupt retry timer */			/* 1fe.0000.1a00 */
	u_int64_t	intr_retry_timer;

	u_int64_t	pad8[63];

	struct timer_counter {
		/* timer/counter 0/1 count register */	/* 1fe.0000.1c00,1c10 */
		u_int64_t	tc_count;
		/* timer/counter 0/1 limit register */	/* 1fe.0000.1c08,1c18 */
		u_int64_t	tc_limit;
	} tc[2];

	/* PCI DMA write sync register (IIi) */		/* 1fe.0000.1c20 */
	u_int64_t	pci_dma_write_sync;

	u_int64_t	pad9[123];

	struct pci_ctl {
		/* PCI a/b control/status register */	/* 1fe.0000.2000,4000 */
		u_int64_t	pci_csr;
		u_int64_t	pad10;
		/* PCI a/b AFSR register */		/* 1fe.0000.2010,4010 */
		u_int64_t	pci_afsr;
		/* PCI a/b AFAR register */		/* 1fe.0000.2018,4018 */
		u_int64_t	pci_afar;
		/* PCI a/b diagnostic register */	/* 1fe.0000.2020,4020 */
		u_int64_t	pci_diag;
		/* PCI target address space reg (IIi)*/	/* 1fe.0000.2028,4028 */
		u_int64_t	pci_tasr;

		u_int64_t	pad11[250];

		/* This is really the IOMMU's, not the PCI bus's */
							/* 1fe.0000.2800-210 */
		struct iommu_strbuf pci_strbuf;
#define psy_iommu_strbuf psy_pcictl[0].pci_strbuf
		
		u_int64_t	pad12[765];
	} psy_pcictl[2];				/* For PCI a and b */

	/*
	 * NB: FFB0 and FFB1 intr map regs also appear at 1fe.0000.6000 and
	 * 1fe.0000.8000 respectively
	 */
	u_int64_t	pad13[2048];

	/* DMA scoreboard diag reg 0 */			/* 1fe.0000.a000 */
	u_int64_t	dma_scb_diag0;
	/* DMA scoreboard diag reg 1 */			/* 1fe.0000.a008 */
	u_int64_t	dma_scb_diag1;

	u_int64_t	pad14[126];

	/* IOMMU virtual addr diag reg */		/* 1fe.0000.a400 */
	u_int64_t	iommu_svadiag;
	/* IOMMU TLB tag compare diag reg */		/* 1fe.0000.a408 */
	u_int64_t	iommu_tlb_comp_diag;
	
	u_int64_t	pad15[30];

	/* IOMMU LRU queue diag */			/* 1fe.0000.a500-a578 */
	u_int64_t	iommu_queue_diag[16];
	/* TLB tag diag */				/* 1fe.0000.a580-a5f8 */
	u_int64_t	tlb_tag_diag[16];
	/* TLB data RAM diag */				/* 1fe.0000.a600-a678 */
	u_int64_t	tlb_data_diag[16];

	u_int64_t	pad16[48];

	/* PCI int state diag reg */			/* 1fe.0000.a800 */
	u_int64_t	pci_int_diag;
	/* OBIO and misc int state diag reg */		/* 1fe.0000.a808 */
	u_int64_t	obio_int_diag;

	u_int64_t	pad17[254];

	struct strbuf_diag {
		/* streaming buffer data RAM diag */	/* 1fe.0000.b000-b3f8 */
		u_int64_t	strbuf_data_diag[128];
		/* streaming buffer error status diag *//* 1fe.0000.b400-b7f8 */
		u_int64_t	strbuf_error_diag[128];
		/* streaming buffer page tag diag */	/* 1fe.0000.b800-b878 */
		u_int64_t	strbuf_pg_tag_diag[16];
		u_int64_t	pad18[16];
		/* streaming buffer line tag diag */	/* 1fe.0000.b900-b978 */
		u_int64_t	strbuf_ln_tag_diag[16];
		u_int64_t	pad19[208];
	} psy_strbufdiag[2];				/* For PCI a and b */

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
};

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
