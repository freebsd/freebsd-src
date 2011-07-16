/*-
 * Copyright 2003-2011 Netlogic Microsystems (Netlogic). All rights
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY Netlogic Microsystems ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL NETLOGIC OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 * NETLOGIC_BSD */

#ifndef __NLM_IOMAP_H__
#define __NLM_IOMAP_H__

/**
* @file_name xlpiomap.h
* @author Netlogic Microsystems
* @brief Basic definitions Netlogic XLP IO BASEs
*/

/* ----------------------------------
 *   XLP RESET Physical Address Map
 * ----------------------------------
 * PCI ECFG : 0x18000000 - 0x1bffffff
 * PCI CFG  : 0x1c000000 - 0x1cffffff
 * FLASH    : 0x1fc00000 - 0x1fffffff
 * ----------------------------------
 */

#define XLP_DEFAULT_IO_BASE             0x18000000
#define XLP_DEFAULT_IO_BASE_KSEG1       0xb8000000
#define XLP_IO_SIZE 			(64 << 20)	/* Size of the ECFG Space      */
#define XLP_IO_PCI_HDRSZ		0x100
#define XLP_IO_DEV(node, dev)		((dev) + (node) * 8)
#define XLP_HDR_OFFSET(node, bus, dev, fn)	(((bus) << 20) | \
				((XLP_IO_DEV(node, dev)) << 15) | ((fn) << 12))

#define XLP_IO_BRIDGE_OFFSET(node)	XLP_HDR_OFFSET(node,0,0,0)
/* coherent inter chip */
#define XLP_IO_CIC0_OFFSET(node)	XLP_HDR_OFFSET(node,0,0,1)
#define XLP_IO_CIC1_OFFSET(node)	XLP_HDR_OFFSET(node,0,0,2)
#define XLP_IO_CIC2_OFFSET(node)	XLP_HDR_OFFSET(node,0,0,3)
#define	XLP_IO_PIC_OFFSET(node)		XLP_HDR_OFFSET(node,0,0,4)

#define	XLP_IO_PCIE_OFFSET(node,i)	XLP_HDR_OFFSET(node,0,1,i)
#define	XLP_IO_PCIE0_OFFSET(node)	XLP_HDR_OFFSET(node,0,1,0)
#define	XLP_IO_PCIE1_OFFSET(node)	XLP_HDR_OFFSET(node,0,1,1)
#define	XLP_IO_PCIE2_OFFSET(node)	XLP_HDR_OFFSET(node,0,1,2)
#define	XLP_IO_PCIE3_OFFSET(node)	XLP_HDR_OFFSET(node,0,1,3)

#define XLP_IO_USB_OFFSET(node, i)	XLP_HDR_OFFSET(node,0,2,i)
#define	XLP_IO_USB_EHCI0_OFFSET(node)	XLP_HDR_OFFSET(node,0,2,0)
#define	XLP_IO_USB_OHCI0_OFFSET(node)	XLP_HDR_OFFSET(node,0,2,1)
#define	XLP_IO_USB_OHCI1_OFFSET(node)	XLP_HDR_OFFSET(node,0,2,2)
#define	XLP_IO_USB_EHCI1_OFFSET(node)	XLP_HDR_OFFSET(node,0,2,3)
#define	XLP_IO_USB_OHCI2_OFFSET(node)	XLP_HDR_OFFSET(node,0,2,4)
#define	XLP_IO_USB_OHCI3_OFFSET(node)	XLP_HDR_OFFSET(node,0,2,5)

#define	XLP_IO_NAE_OFFSET(node)		XLP_HDR_OFFSET(node,0,3,0)
#define	XLP_IO_POE_OFFSET(node)		XLP_HDR_OFFSET(node,0,3,1)

#define	XLP_IO_CMS_OFFSET(node)		XLP_HDR_OFFSET(node,0,4,0)

#define	XLP_IO_DMA_OFFSET(node)		XLP_HDR_OFFSET(node,0,5,1)
#define	XLP_IO_SEC_OFFSET(node)		XLP_HDR_OFFSET(node,0,5,2)
#define	XLP_IO_CMP_OFFSET(node)		XLP_HDR_OFFSET(node,0,5,3)

#define XLP_IO_UART_OFFSET(node, i)	XLP_HDR_OFFSET(node,0,6,i)
#define XLP_IO_UART0_OFFSET(node)	XLP_HDR_OFFSET(node,0,6,0)
#define XLP_IO_UART1_OFFSET(node)	XLP_HDR_OFFSET(node,0,6,1)
#define XLP_IO_I2C_OFFSET(node, i)	XLP_HDR_OFFSET(node,0,6,2+i)
#define XLP_IO_I2C0_OFFSET(node)	XLP_HDR_OFFSET(node,0,6,2)
#define XLP_IO_I2C1_OFFSET(node)	XLP_HDR_OFFSET(node,0,6,3)
#define	XLP_IO_GPIO_OFFSET(node)	XLP_HDR_OFFSET(node,0,6,4)
/* system management */
#define	XLP_IO_SYS_OFFSET(node)		XLP_HDR_OFFSET(node,0,6,5)
#define	XLP_IO_JTAG_OFFSET(node)	XLP_HDR_OFFSET(node,0,6,6)

#define	XLP_IO_NOR_OFFSET(node)		XLP_HDR_OFFSET(node,0,7,0)
#define	XLP_IO_NAND_OFFSET(node)	XLP_HDR_OFFSET(node,0,7,1)
#define	XLP_IO_SPI_OFFSET(node)		XLP_HDR_OFFSET(node,0,7,2)
/* SD flash */
#define	XLP_IO_SD_OFFSET(node)		XLP_HDR_OFFSET(node,0,7,3)
#define XLP_IO_MMC_OFFSET(node, slot)	((XLP_IO_SD_OFFSET(node))+(slot*0x100)+XLP_IO_PCI_HDRSZ)
/* PCI config header register id's */
#define XLP_PCI_CFGREG0			0x00
#define XLP_PCI_CFGREG1			0x01
#define XLP_PCI_CFGREG2			0x02
#define XLP_PCI_CFGREG3			0x03
#define XLP_PCI_CFGREG4			0x04
#define XLP_PCI_CFGREG5			0x05
#define XLP_PCI_DEVINFO_REG0		0x30
#define XLP_PCI_DEVINFO_REG1		0x31
#define XLP_PCI_DEVINFO_REG2		0x32
#define XLP_PCI_DEVINFO_REG3		0x33
#define XLP_PCI_DEVINFO_REG4		0x34
#define XLP_PCI_DEVINFO_REG5		0x35
#define XLP_PCI_DEVINFO_REG6		0x36
#define XLP_PCI_DEVINFO_REG7		0x37
#define XLP_PCI_DEVSCRATCH_REG0		0x38
#define XLP_PCI_DEVSCRATCH_REG1		0x39
#define XLP_PCI_DEVSCRATCH_REG2		0x3a
#define XLP_PCI_DEVSCRATCH_REG3		0x3b
#define XLP_PCI_MSGSTN_REG		0x3c
#define XLP_PCI_IRTINFO_REG		0x3d
#define XLP_PCI_UCODEINFO_REG		0x3e
#define XLP_PCI_SBB_WT_REG		0x3f

#if !defined(LOCORE) && !defined(__ASSEMBLY__)

#ifndef __NLM_NLMIO_H__
#error iomap.h needs mmio.h to be included
#endif

static __inline__ uint32_t
nlm_read_reg_kseg(uint64_t base, uint32_t reg)
{
	volatile uint32_t *addr = (volatile uint32_t *)(intptr_t)base + reg;

	return (*addr);
}

static __inline__ void
nlm_write_reg_kseg(uint64_t base, uint32_t reg, uint32_t val)
{
	volatile uint32_t *addr = (volatile uint32_t *)(intptr_t)base + reg;

	*addr = val;
}

static __inline__ uint64_t
nlm_read_reg64_kseg(uint64_t base, uint32_t reg)
{
	volatile uint64_t *addr = (volatile uint64_t *)(intptr_t)base + (reg >> 1);

	return (nlm_load_dword(addr));
}

static __inline__ void
nlm_write_reg64_kseg(uint64_t base, uint32_t reg, uint64_t val)
{
	volatile uint64_t *addr = (volatile uint64_t *)(intptr_t)base + (reg >> 1);

	return (nlm_store_dword(addr, val));
}

/*
 * Routines to store 32/64 bit values to 64 bit addresses,
 * used when going thru XKPHYS to access registers
 */
static __inline__ uint32_t
nlm_read_reg_xkseg(uint64_t base, uint32_t reg)
{
	uint64_t addr = base + reg * sizeof(uint32_t);

	return (nlm_load_word_daddr(addr));
}

static __inline__ void
nlm_write_reg_xkseg(uint64_t base, uint32_t reg, uint32_t val)
{
	uint64_t addr = base + reg * sizeof(uint32_t);

	return (nlm_store_word_daddr(addr, val));
}

static __inline__ uint64_t
nlm_read_reg64_xkseg(uint64_t base, uint32_t reg)
{
	uint64_t addr = base + (reg >> 1) * sizeof(uint64_t);

	return (nlm_load_dword_daddr(addr));
}

static __inline__ void
nlm_write_reg64_xkseg(uint64_t base, uint32_t reg, uint64_t val)
{
	uint64_t addr = base + (reg >> 1) * sizeof(uint64_t);

	return (nlm_store_dword_daddr(addr, val));
}

/* Location where IO base is mapped */
extern uint64_t nlm_pcicfg_baseaddr;

static __inline__ uint64_t
nlm_pcicfg_base(uint32_t devoffset)
{
	return (nlm_pcicfg_baseaddr + devoffset);
}

static __inline__ uint64_t
nlm_pcibar0_base_xkphys(uint64_t pcibase)
{
	uint64_t paddr;

	paddr = nlm_read_reg_kseg(pcibase, XLP_PCI_CFGREG4) & ~0xfu;
	return (0x9000000000000000 | paddr);
}
#define	nlm_pci_rdreg(b, r)	nlm_read_reg_kseg(b, r)
#define	nlm_pci_wreg(b, r, v)	nlm_write_reg_kseg(b, r, v)

#endif /* !LOCORE && !__ASSEMBLY__*/


/* COMPAT stuff - TODO remove */
#define bit_set(p, m) ((p) |= (m))
#define bit_clear(p, m) ((p) &= ~(m))
#define bit_get(p,m) ((p) & (m))
#define BIT(x) (0x01 << (x))

#define XLP_MAX_NODES		4
#define	XLP_MAX_CORES		8
#define	XLP_MAX_THREADS		4
#define	XLP_CACHELINE_SIZE	64
#define XLP_NUM_NODES		1   /* we support only one now */

#endif
