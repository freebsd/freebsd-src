/*
 * Copyright (c) 2000 Doug Rabson & Andrew Gallatin 
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/alpha/pci/t2reg.h,v 1.1.2.1 2000/07/04 01:42:22 mjacob Exp $
 */



/*
 * Registers in the T2 CBUS-to-PCI bridge as used in the SABLE
 * systems.
 */

#define REGVAL(r)	(*(volatile int32_t *)				\
				ALPHA_PHYS_TO_K0SEG(r + t2_csr_base)) 
#define REGVAL64(r)	(*(volatile int64_t *)				\
				ALPHA_PHYS_TO_K0SEG(r + t2_csr_base))

#define SABLE_BASE	0x0UL		/* offset of SABLE CSRs */
#define LYNX_BASE	0x8000000000UL	/* offset of LYNX CSRs */

#define CBUS_BASE	0x380000000	/* CBUS CSRs */
#define T2_PCI_SIO	0x3a0000000	/* PCI sparse I/O space */
#define T2_PCI_CONF	0x390000000	/* PCI configuration space */
#define T2_PCI_SPARSE	0x200000000	/* PCI sparse memory space */
#define T2_PCI_DENSE	0x3c0000000	/* PCI dense memory space */

#define T2_IOCSR	(CBUS_BASE + 0xe000000)
					/* Low word */
#define	 T2_IOCSRL_EL		0x00000002UL	/* loopback enable */
#define	 T2_IOCSRL_ESMV		0x00000004UL	/* enable state machine visibility */
#define	 T2_IOCSRL_PDBP		0x00000008UL	/* PCI drive bad parity */
#define	 T2_IOCSRL_SLOT0	0x00000030UL	/* PCI slot 0 present bits */
#define	 T2_IOCSRL_PINT		0x00000040UL	/* PCI interrupt */
#define	 T2_IOCSRL_ENTLBEC	0x00000080UL	/* enable TLB error check */
#define	 T2_IOCSRL_ENCCDMA	0x00000100UL	/* enable CXACK for DMA */
#define	 T2_IOCSRL_ENXXCHG	0x00000400UL	/* enable exclusive exchange for EV5 */
#define	 T2_IOCSRL_CAWWP0	0x00001000UL	/* CBUS command/address write wrong parity 0 */
#define	 T2_IOCSRL_CAWWP2	0x00002000UL	/* CBUS command/address write wrong parity 2 */
#define	 T2_IOCSRL_CDWWPE	0x00004000UL	/* CBUS data write wrong parity even */
#define	 T2_IOCSRL_SLOT2	0x00008000UL	/* PCI slot 2 present bit */
#define	 T2_IOCSRL_PSERR	0x00010000UL	/* power supply error */
#define	 T2_IOCSRL_MBA7		0x00020000UL	/* MBA7 asserted */
#define	 T2_IOCSRL_SLOT1	0x000c0000UL	/* PCI slot 1 present bits */
#define	 T2_IOCSRL_PDWWP1	0x00100000UL	/* PCI DMA write wrong parity HW1 */
#define	 T2_IOCSRL_PDWWP0	0x00200000UL	/* PCI DMA write wrong parity HW0 */
#define	 T2_IOCSRL_PBR		0x00400000UL	/* PCI bus reset */
#define	 T2_IOCSRL_PIR		0x00800000UL	/* PCI interface reset */
#define	 T2_IOCSRL_ENCOI	0x01000000UL	/* enable NOACK, CUCERR and out-of-sync int */
#define	 T2_IOCSRL_EPMS		0x02000000UL	/* enable PCI memory space */
#define	 T2_IOCSRL_ETLB		0x04000000UL	/* enable TLB */
#define	 T2_IOCSRL_EACC		0x08000000UL	/* enable atomic CBUS cycles */
#define	 T2_IOCSRL_ITLB		0x10000000UL	/* flush TLB */
#define	 T2_IOCSRL_ECPC		0x20000000UL	/* enable CBUS parity check */
#define	 T2_IOCSRL_CIR		0x40000000UL	/* CBUS interface reset */
#define	 T2_IOCSRL_EPL		0x80000000UL	/* enable PCI lock */
					/* High word */
#define	 T2_IOCSRH_CBBCE	0x00000001UL	/* CBUS back-to-back cycle enable */
#define	 T2_IOCSRH_TM		0x0000000eUL	/* T2 revision number */
#define	 T2_IOCSRH_SMVL		0x00000070UL	/* state machine visibility select */
#define	 T2_IOCSRH_SLOT2	0x00000080UL	/* PCI slot 2 present bit */
#define	 T2_IOCSRH_EPR		0x00000100UL	/* enable passive release */
#define	 T2_IOCSRH_CAWWP1	0x00001000UL	/* cbus command/address write wrong parity 1 */
#define	 T2_IOCSRH_CAWWP3	0x00002000UL	/* cbus command/address write wrong parity 3 */
#define	 T2_IOCSRH_DWWPO	0x00004000UL	/* CBUS data write wrong parity odd */
#define	 T2_IOCSRH_PRM		0x00100000UL	/* PCI read multiple */
#define	 T2_IOCSRH_PWM		0x00200000UL	/* PCI write multiple */
#define	 T2_IOCSRH_FPRDPED	0x00400000UL	/* force PCI RDPE detect */
#define	 T2_IOCSRH_PFAPED	0x00800000UL	/* force PCI APE detect */
#define	 T2_IOCSRH_FPWDPED	0x01000000UL	/* force PCI WDPE detect */
#define	 T2_IOCSRH_EPNMI	0x02000000UL	/* enable PCI NMI */
#define	 T2_IOCSRH_EPDTI	0x04000000UL	/* enable PCI DTI */
#define	 T2_IOCSRH_EPSEI	0x08000000UL	/* enable PCI SERR interrupt */
#define	 T2_IOCSRH_EPPEI	0x10000000UL	/* enable PCI PERR interrupt */
#define	 T2_IOCSRH_ERDPC	0x20000000UL	/* enable PCI RDP interrupt */
#define	 T2_IOCSRH_EADPC	0x40000000UL	/* enable PCI AP interrupt */
#define	 T2_IOCSRH_EWDPC	0x80000000UL	/* enable PCI WDP interrupt */

#define T2_CERR1	(CBUS_BASE + 0xe000020)
#define T2_CERR2	(CBUS_BASE + 0xe000040)
#define T2_CERR3	(CBUS_BASE + 0xe000060)
#define T2_PERR1	(CBUS_BASE + 0xe000080)
#define	 T2_PERR1_PWDPE		0x00000001	/* PCI write data parity error */
#define	 T2_PERR1_PAPE		0x00000002	/* PCI address parity error */
#define	 T2_PERR1_PRDPE		0x00000004	/* PCI read data parity error */
#define	 T2_PERR1_PPE		0x00000008	/* PCI parity error */
#define	 T2_PERR1_PSE		0x00000010	/* PCI system error */
#define	 T2_PERR1_PDTE		0x00000020	/* PCI device timeout error */
#define	 T2_PERR1_NMI		0x00000040	/* PCI NMI */

#define T2_PERR2	(CBUS_BASE + 0xe0000a0)
#define T2_PSCR		(CBUS_BASE + 0xe0000c0)
#define T2_HAE0_1	(CBUS_BASE + 0xe0000e0)
#define T2_HAE0_2	(CBUS_BASE + 0xe000100)
#define T2_HBASE	(CBUS_BASE + 0xe000120)
#define T2_WBASE1	(CBUS_BASE + 0xe000140)
#define T2_WMASK1	(CBUS_BASE + 0xe000160)
#define T2_TBASE1	(CBUS_BASE + 0xe000180)
#define T2_WBASE2	(CBUS_BASE + 0xe0001a0)
#define T2_WMASK2	(CBUS_BASE + 0xe0001c0)
#define T2_TBASE2	(CBUS_BASE + 0xe0001e0)
#define T2_TLBBR	(CBUS_BASE + 0xe000200)
#define T2_HAE0_3	(CBUS_BASE + 0xe000240)
#define T2_HAE0_4	(CBUS_BASE + 0xe000280)

/*
 * DMA window constants, section 5.2.1.1.1 of the 
 * Sable I/O Specification
 */
 
#define T2_WINDOW_ENABLE	0x00080000
#define T2_WINDOW_DISABLE	0x00000000
#define T2_WINDOW_SG		0x00040000
#define T2_WINDOW_DIRECT	0x00000000

#define T2_WMASK_2G		0x7ff00000
#define T2_WMASK_1G		0x3ff00000
#define T2_WMASK_512M		0x1ff00000
#define T2_WMASK_256M		0x0ff00000
#define T2_WMASK_128M		0x07f00000
#define T2_WMASK_64M		0x03f00000
#define T2_WMASK_32M		0x01f00000
#define T2_WMASK_16M		0x00f00000
#define T2_WMASK_8M		0x00700000
#define T2_WMASK_4M		0x00300000
#define T2_WMASK_2M		0x00100000
#define T2_WMASK_1M		0x00000000


#define T2_WSIZE_2G		0x80000000
#define T2_WSIZE_1G		0x40000000
#define T2_WSIZE_512M		0x20000000
#define T2_WSIZE_256M		0x10000000
#define T2_WSIZE_128M		0x08000000
#define T2_WSIZE_64M		0x04000000
#define T2_WSIZE_32M		0x02000000
#define T2_WSIZE_16M		0x01000000
#define T2_WSIZE_8M		0x00800000
#define T2_WSIZE_4M		0x00400000
#define T2_WSIZE_2M		0x00200000
#define T2_WSIZE_1M		0x00100000
#define T2_WSIZE_0M		0x00000000

#define T2_TBASE_SHIFT		1

#define	MASTER_ICU	0x535
#define	SLAVE0_ICU	0x537
#define	SLAVE1_ICU	0x53b
#define	SLAVE2_ICU	0x53d
#define	SLAVE3_ICU	0x53f
