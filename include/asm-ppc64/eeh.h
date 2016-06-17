/* 
 * eeh.h
 * Copyright (C) 2001  Dave Engebretsen & Todd Inglett IBM Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */

/* Start Change Log
 * 2001/10/27 : engebret : Created.
 * End Change Log 
 */

#ifndef _EEH_H
#define _EEH_H

struct pci_dev;

/* I/O addresses are converted to EEH "tokens" such that a driver will cause
 * a bad page fault if the address is used directly (i.e. these addresses are
 * never actually mapped.  Translation between IO <-> EEH region is 1 to 1.
 */
#define IO_TOKEN_TO_ADDR(token) (((unsigned long)(token) & ~(0xfUL << REGION_SHIFT)) | \
				(IO_REGION_ID << REGION_SHIFT))
#define IO_ADDR_TO_TOKEN(addr) (((unsigned long)(addr) & ~(0xfUL << REGION_SHIFT)) | \
				(EEH_REGION_ID << REGION_SHIFT))

/* Values for eeh_mode bits in device_node */
#define EEH_MODE_SUPPORTED	(1<<0)
#define EEH_MODE_NOCHECK	(1<<1)

/* This is for profiling only */
extern unsigned long eeh_total_mmio_ffs;

void eeh_init(void);
int eeh_get_state(unsigned long ea);
unsigned long eeh_check_failure(void *token, unsigned long val);
void *eeh_ioremap(unsigned long addr, void *vaddr);

#define EEH_DISABLE		0
#define EEH_ENABLE		1
#define EEH_RELEASE_LOADSTORE	2
#define EEH_RELEASE_DMA		3
int eeh_set_option(struct pci_dev *dev, int options);

/* Given a PCI device check if eeh should be configured or not.
 * This may look at firmware properties and/or kernel cmdline options.
 */
int is_eeh_configured(struct pci_dev *dev);

/* Translate a (possible) eeh token to a physical addr.
 * If "token" is not an eeh token it is simply returned under
 * the assumption that it is already a physical addr.
 */
unsigned long eeh_token_to_phys(unsigned long token);

extern void *memcpy(void *, const void *, unsigned long);
extern void *memset(void *,int, unsigned long);

/* EEH_POSSIBLE_ERROR() -- test for possible MMIO failure.
 *
 * Order this macro for performance.
 * If EEH is off for a device and it is a memory BAR, ioremap will
 * map it to the IOREGION.  In this case addr == vaddr and since these
 * should be in registers we compare them first.  Next we check for
 * ff's which indicates a (very) possible failure.
 *
 * If this macro yields TRUE, the caller relays to eeh_check_failure()
 * which does further tests out of line.
 */
/* #define EEH_POSSIBLE_IO_ERROR(val) (~(val) == 0) */
/* #define EEH_POSSIBLE_ERROR(addr, vaddr, val) ((vaddr) != (addr) && EEH_POSSIBLE_IO_ERROR(val) */
/* This version is rearranged to collect some profiling data */
#define EEH_POSSIBLE_IO_ERROR(val, type)				\
		((val) == (type)~0 && ++eeh_total_mmio_ffs)
#define EEH_POSSIBLE_ERROR(addr, vaddr, val, type)			\
		(EEH_POSSIBLE_IO_ERROR(val, type) && (vaddr) != (addr))

/* 
 * MMIO read/write operations with EEH support.
 *
 * addr: 64b token of the form 0xA0PPBBDDyyyyyyyy
 *       0xA0     : Unmapped MMIO region
 *       PP       : PHB index (starting at zero)
 *	 BB	  : PCI Bus number under given PHB
 *	 DD	  : PCI devfn under given bus
 *       yyyyyyyy : Virtual address offset
 * 
 * An actual virtual address is produced from this token
 * by masking into the form:
 *   0xE0000000yyyyyyyy
 */
static inline u8 eeh_readb(void *addr) {
	volatile u8 *vaddr = (volatile u8 *)IO_TOKEN_TO_ADDR(addr);
	u8 val = in_8(vaddr);
	if (EEH_POSSIBLE_ERROR(addr, vaddr, val, u8))
		return eeh_check_failure(addr, val);
	return val;
}
static inline void eeh_writeb(u8 val, void *addr) {
	volatile u8 *vaddr = (volatile u8 *)IO_TOKEN_TO_ADDR(addr);
	out_8(vaddr, val);
}
static inline u16 eeh_readw(void *addr) {
	volatile u16 *vaddr = (volatile u16 *)IO_TOKEN_TO_ADDR(addr);
	u16 val = in_le16(vaddr);
	if (EEH_POSSIBLE_ERROR(addr, vaddr, val, u16))
		return eeh_check_failure(addr, val);
	return val;
}
static inline void eeh_writew(u16 val, void *addr) {
	volatile u16 *vaddr = (volatile u16 *)IO_TOKEN_TO_ADDR(addr);
	out_le16(vaddr, val);
}
static inline u16 eeh_raw_readw(void *addr) {
       volatile u16 *vaddr = (volatile u16 *)IO_TOKEN_TO_ADDR(addr);
       u16 val = in_be16(vaddr);
       if (EEH_POSSIBLE_ERROR(addr, vaddr, val, u16))
               return eeh_check_failure(addr, val);
       return val;
}
static inline void eeh_raw_writew(u16 val, void *addr) {
       volatile u16 *vaddr = (volatile u16 *)IO_TOKEN_TO_ADDR(addr);
       out_be16(vaddr, val);
}
static inline u32 eeh_readl(void *addr) {
	volatile u32 *vaddr = (volatile u32 *)IO_TOKEN_TO_ADDR(addr);
	u32 val = in_le32(vaddr);
	if (EEH_POSSIBLE_ERROR(addr, vaddr, val, u32))
		return eeh_check_failure(addr, val);
	return val;
}
static inline void eeh_writel(u32 val, void *addr) {
	volatile u32 *vaddr = (volatile u32 *)IO_TOKEN_TO_ADDR(addr);
	out_le32(vaddr, val);
}
static inline u32 eeh_raw_readl(void *addr) {
       volatile u32 *vaddr = (volatile u32 *)IO_TOKEN_TO_ADDR(addr);
       u32 val = in_be32(vaddr);
       if (EEH_POSSIBLE_ERROR(addr, vaddr, val, u32))
               return eeh_check_failure(addr, val);
       return val;
}
static inline void eeh_raw_writel(u32 val, void *addr) {
       volatile u32 *vaddr = (volatile u32 *)IO_TOKEN_TO_ADDR(addr);
       out_be32(vaddr, val);
}

static inline void eeh_memset_io(void *addr, int c, unsigned long n) {
	void *vaddr = (void *)IO_TOKEN_TO_ADDR(addr);
	memset(vaddr, c, n);
}
static inline void eeh_memcpy_fromio(void *dest, void *src, unsigned long n) {
	void *vsrc = (void *)IO_TOKEN_TO_ADDR(src);
	memcpy(dest, vsrc, n);
	/* look for ffff's here at dest[n] */
}
static inline void eeh_memcpy_toio(void *dest, void *src, unsigned long n) {
	void *vdest = (void *)IO_TOKEN_TO_ADDR(dest);
	memcpy(vdest, src, n);
}

/* The I/O macros must handle ISA ports as well as PCI I/O bars.
 * ISA does not implement EEH and ISA may not exist in the system.
 * For PCI we check for EEH failures.
 */
#define _IO_IS_ISA(port) ((port) < 0x10000)
#define _IO_HAS_ISA_BUS	(isa_io_base != 0)

static inline u8 eeh_inb(unsigned long port) {
	u8 val;
	if (_IO_IS_ISA(port) && !_IO_HAS_ISA_BUS)
		return ~0;
	val = in_8((u8 *)(port+pci_io_base));
	if (!_IO_IS_ISA(port) && EEH_POSSIBLE_IO_ERROR(val, u8))
		return eeh_check_failure((void*)(port+pci_io_base), val);
	return val;
}

static inline void eeh_outb(u8 val, unsigned long port) {
	if (!_IO_IS_ISA(port) || _IO_HAS_ISA_BUS)
		return out_8((u8 *)(port+pci_io_base), val);
}

static inline u16 eeh_inw(unsigned long port) {
	u16 val;
	if (_IO_IS_ISA(port) && !_IO_HAS_ISA_BUS)
		return ~0;
	val = in_le16((u16 *)(port+pci_io_base));
	if (!_IO_IS_ISA(port) && EEH_POSSIBLE_IO_ERROR(val, u16))
		return eeh_check_failure((void*)(port+pci_io_base), val);
	return val;
}

static inline void eeh_outw(u16 val, unsigned long port) {
	if (!_IO_IS_ISA(port) || _IO_HAS_ISA_BUS)
		return out_le16((u16 *)(port+pci_io_base), val);
}

static inline u32 eeh_inl(unsigned long port) {
	u32 val;
	if (_IO_IS_ISA(port) && !_IO_HAS_ISA_BUS)
		return ~0;
	val = in_le32((u32 *)(port+pci_io_base));
	if (!_IO_IS_ISA(port) && EEH_POSSIBLE_IO_ERROR(val, u32))
		return eeh_check_failure((void*)(port+pci_io_base), val);
	return val;
}

static inline void eeh_outl(u32 val, unsigned long port) {
	if (!_IO_IS_ISA(port) || _IO_HAS_ISA_BUS)
		return out_le32((u32 *)(port+pci_io_base), val);
}

#endif /* _EEH_H */
