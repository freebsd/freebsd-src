/*-
 * Copyright (C) 2007-2008 MARVELL INTERNATIONAL LTD.
 * All rights reserved.
 *
 * Developed by Semihalf.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of MARVELL nor the names of contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _MVWIN_H_
#define _MVWIN_H_

/*
 * Physical addresses of integrated SoC peripherals
 */
#define MV_PHYS_BASE		0xF1000000
#define MV_SIZE			0x100000

/*
 * Decode windows addresses (physical)
 */
#define MV_PCIE_IO_PHYS_BASE	(MV_PHYS_BASE + MV_SIZE)
#define MV_PCIE_IO_BASE		MV_PCIE_IO_PHYS_BASE
#define MV_PCIE_IO_SIZE		(1024 * 1024)
#define MV_PCI_IO_PHYS_BASE	(MV_PCIE_IO_PHYS_BASE + MV_PCIE_IO_SIZE) 
#define MV_PCI_IO_BASE		MV_PCI_IO_PHYS_BASE
#define MV_PCI_IO_SIZE		(1024 * 1024)

#define MV_PCIE_MEM_PHYS_BASE	(MV_PCI_IO_PHYS_BASE + MV_PCI_IO_SIZE)
#define MV_PCIE_MEM_BASE	MV_PCIE_MEM_PHYS_BASE
#define MV_PCIE_MEM_SIZE	(64 * 1024 * 1024)
#define MV_PCI_MEM_PHYS_BASE	(MV_PCIE_MEM_PHYS_BASE + MV_PCIE_MEM_SIZE)
#define MV_PCI_MEM_BASE		MV_PCI_MEM_PHYS_BASE
#define MV_PCI_MEM_SIZE		(64 * 1024 * 1024)

/* XXX DEV_BOOT, CSx are board specific, should be defined per platform */

/* 512KB NOR FLASH */
#define MV_DEV_BOOT_PHYS_BASE	(MV_PCI_MEM_PHYS_BASE + MV_PCI_MEM_SIZE)
#define MV_DEV_BOOT_SIZE		(512 * 1024)
/* CS0: 7-seg LED */
#define MV_DEV_CS0_PHYS_BASE	0xFA000000
#define MV_DEV_CS0_SIZE	(1024 * 1024) /* XXX u-boot has 2MB */
/* CS1: 32MB NOR FLASH */
#define MV_DEV_CS1_PHYS_BASE	(MV_DEV_CS0_PHYS_BASE + MV_DEV_CS0_SIZE)
#define MV_DEV_CS1_SIZE	(32 * 1024 * 1024)
/* CS2: 32MB NAND FLASH */
#define MV_DEV_CS2_PHYS_BASE	(MV_DEV_CS1_PHYS_BASE + MV_DEV_CS1_SIZE)
#define MV_DEV_CS2_SIZE	1024	/* XXX u-boot has 1MB */

#define MV_CESA_SRAM_PHYS_BASE	0xFD000000
#define MV_CESA_SRAM_BASE	MV_CESA_SRAM_PHYS_BASE /* VA == PA mapping */
#define MV_CESA_SRAM_SIZE	(1024 * 1024)

/* XXX this is probably not robust against wraparounds... */
#if ((MV_CESA_SRAM_PHYS_BASE + MV_CESA_SRAM_SIZE) > 0xFFFEFFFF)
#error Devices memory layout overlaps reset vectors range!
#endif

/*
 * Integrated SoC peripherals addresses
 */
#define MV_BASE			MV_PHYS_BASE	/* VA == PA mapping */
#define MV_DDR_CADR_BASE	(MV_BASE + 0x1500)
#define MV_MPP_BASE		(MV_BASE + 0x10000)

#define MV_MBUS_BRIDGE_BASE	(MV_BASE + 0x20000)
#define MV_INTREGS_BASE		(MV_MBUS_BRIDGE_BASE + 0x80)
#define MV_CPU_CONTROL_BASE	(MV_MBUS_BRIDGE_BASE + 0x100)

#define MV_PCI_BASE		(MV_BASE + 0x30000)
#define MV_PCI_SIZE		0x2000

#define MV_PCIE_BASE		(MV_BASE + 0x40000)
#define MV_PCIE_SIZE		0x2000

#define MV_PCIE00_BASE		(MV_PCIE_BASE + 0x00000)
#define MV_PCIE01_BASE		(MV_PCIE_BASE + 0x04000)
#define MV_PCIE02_BASE		(MV_PCIE_BASE + 0x08000)
#define MV_PCIE03_BASE		(MV_PCIE_BASE + 0x0C000)
#define MV_PCIE10_BASE		(MV_PCIE_BASE + 0x40000)
#define MV_PCIE11_BASE		(MV_PCIE_BASE + 0x44000)
#define MV_PCIE12_BASE		(MV_PCIE_BASE + 0x48000)
#define MV_PCIE13_BASE		(MV_PCIE_BASE + 0x4C000)

#define MV_DEV_CS0_BASE		MV_DEV_CS0_PHYS_BASE

/*
 * Decode windows definitions and macros
 */
#define MV_WIN_CPU_CTRL(n)		(0x10 * (n) + (((n) < 8) ? 0x000 : 0x880))
#define MV_WIN_CPU_BASE(n)		(0x10 * (n) + (((n) < 8) ? 0x004 : 0x884))
#define MV_WIN_CPU_REMAP_LO(n)		(0x10 * (n) + (((n) < 8) ? 0x008 : 0x888))
#define MV_WIN_CPU_REMAP_HI(n)		(0x10 * (n) + (((n) < 8) ? 0x00C : 0x88C))
#if defined(SOC_MV_DISCOVERY)
#define MV_WIN_CPU_MAX			14
#else
#define MV_WIN_CPU_MAX			8
#endif

#define MV_WIN_DDR_BASE(n)		(0x8 * (n) + 0x0)
#define MV_WIN_DDR_SIZE(n)		(0x8 * (n) + 0x4)
#define MV_WIN_DDR_MAX			4

#define MV_WIN_CESA_CTRL(n)		(0x8 * (n) + 0xa04)
#define MV_WIN_CESA_BASE(n)		(0x8 * (n) + 0xa00)
#define MV_WIN_CESA_MAX			4

#if defined(SOC_MV_DISCOVERY)
#define MV_WIN_CESA_TARGET		9
#define MV_WIN_CESA_ATTR		1
#else
#define MV_WIN_CESA_TARGET		3
#define MV_WIN_CESA_ATTR		0
#endif

#define MV_WIN_USB_CTRL(n)		(0x10 * (n) + 0x0)
#define MV_WIN_USB_BASE(n)		(0x10 * (n) + 0x4)
#define MV_WIN_USB_MAX			4

#define MV_WIN_ETH_BASE(n)		(0x8 * (n) + 0x200)
#define MV_WIN_ETH_SIZE(n)		(0x8 * (n) + 0x204)
#define MV_WIN_ETH_REMAP(n)		(0x4 * (n) + 0x280)
#define MV_WIN_ETH_MAX			6

#define MV_WIN_IDMA_BASE(n)		(0x8 * (n) + 0xa00)
#define MV_WIN_IDMA_SIZE(n)		(0x8 * (n) + 0xa04)
#define MV_WIN_IDMA_REMAP(n)		(0x4 * (n) + 0xa60)
#define MV_WIN_IDMA_CAP(n)		(0x4 * (n) + 0xa70)
#define MV_WIN_IDMA_MAX			8
#define MV_IDMA_CHAN_MAX		4

#define MV_WIN_XOR_BASE(n, m)		(0x4 * (n) + 0xa50 + (m) * 0x100)
#define MV_WIN_XOR_SIZE(n, m)		(0x4 * (n) + 0xa70 + (m) * 0x100)
#define MV_WIN_XOR_REMAP(n, m)		(0x4 * (n) + 0xa90 + (m) * 0x100)
#define MV_WIN_XOR_CTRL(n, m)		(0x4 * (n) + 0xa40 + (m) * 0x100)
#define MV_WIN_XOR_OVERR(n, m)		(0x4 * (n) + 0xaa0 + (m) * 0x100)
#define MV_WIN_XOR_MAX			8
#define MV_XOR_CHAN_MAX			2
#define MV_XOR_NON_REMAP		4

#if defined(SOC_MV_DISCOVERY)
#define MV_WIN_PCIE_MEM_TARGET		4
#define MV_WIN_PCIE_MEM_ATTR		0xE8
#define MV_WIN_PCIE_IO_TARGET		4
#define MV_WIN_PCIE_IO_ATTR		0xE0
#elif defined(SOC_MV_KIRKWOOD)
#define MV_WIN_PCIE_MEM_TARGET		4
#define MV_WIN_PCIE_MEM_ATTR		0xE8
#define MV_WIN_PCIE_IO_TARGET		4
#define MV_WIN_PCIE_IO_ATTR		0xE0
#elif defined(SOC_MV_ORION)
#define MV_WIN_PCIE_MEM_TARGET		4
#define MV_WIN_PCIE_MEM_ATTR		0x59
#define MV_WIN_PCIE_IO_TARGET		4
#define MV_WIN_PCIE_IO_ATTR		0x51
#define MV_WIN_PCI_MEM_TARGET		3
#define MV_WIN_PCI_MEM_ATTR		0x59
#define MV_WIN_PCI_IO_TARGET		3
#define MV_WIN_PCI_IO_ATTR		0x51
#endif

#define MV_WIN_PCIE_CTRL(n)		(0x10 * (((n) < 5) ? (n) : \
					    (n) + 1) + 0x1820)
#define MV_WIN_PCIE_BASE(n)		(0x10 * (((n) < 5) ? (n) : \
					    (n) + 1) + 0x1824)
#define MV_WIN_PCIE_REMAP(n)		(0x10 * (((n) < 5) ? (n) : \
					    (n) + 1) + 0x182C)
#define MV_WIN_PCIE_MAX			6

#define MV_PCIE_BAR(n)			(0x04 * (n) + 0x1804)
#define MV_PCIE_BAR_MAX			3

#define	MV_WIN_SATA_CTRL(n)		(0x10 * (n) + 0x30)
#define	MV_WIN_SATA_BASE(n)		(0x10 * (n) + 0x34)
#define	MV_WIN_SATA_MAX			4

#define WIN_REG_IDX_RD(pre,reg,off,base)					\
	static __inline uint32_t						\
	pre ## _ ## reg ## _read(int i)						\
	{									\
		return (bus_space_read_4(fdtbus_bs_tag, base, off(i)));		\
	}

#define WIN_REG_IDX_RD2(pre,reg,off,base)					\
	static  __inline uint32_t						\
	pre ## _ ## reg ## _read(int i, int j)					\
	{									\
		return (bus_space_read_4(fdtbus_bs_tag, base, off(i, j)));		\
	}									\

#define WIN_REG_BASE_IDX_RD(pre,reg,off)					\
	static __inline uint32_t						\
	pre ## _ ## reg ## _read(uint32_t base, int i)				\
	{									\
		return (bus_space_read_4(fdtbus_bs_tag, base, off(i)));		\
	}

#define WIN_REG_BASE_IDX_RD2(pre,reg,off)					\
	static __inline uint32_t						\
	pre ## _ ## reg ## _read(uint32_t base, int i, int j)				\
	{									\
		return (bus_space_read_4(fdtbus_bs_tag, base, off(i, j)));		\
	}

#define WIN_REG_IDX_WR(pre,reg,off,base)					\
	static __inline void							\
	pre ## _ ## reg ## _write(int i, uint32_t val)				\
	{									\
		bus_space_write_4(fdtbus_bs_tag, base, off(i), val);			\
	}

#define WIN_REG_IDX_WR2(pre,reg,off,base)					\
	static __inline void							\
	pre ## _ ## reg ## _write(int i, int j, uint32_t val)			\
	{									\
		bus_space_write_4(fdtbus_bs_tag, base, off(i, j), val);		\
	}

#define WIN_REG_BASE_IDX_WR(pre,reg,off)					\
	static __inline void							\
	pre ## _ ## reg ## _write(uint32_t base, int i, uint32_t val)		\
	{									\
		bus_space_write_4(fdtbus_bs_tag, base, off(i), val);			\
	}

#define WIN_REG_BASE_IDX_WR2(pre,reg,off)					\
	static __inline void							\
	pre ## _ ## reg ## _write(uint32_t base, int i, int j, uint32_t val)		\
	{									\
		bus_space_write_4(fdtbus_bs_tag, base, off(i, j), val);			\
	}

#define WIN_REG_RD(pre,reg,off,base)						\
	static __inline uint32_t						\
	pre ## _ ## reg ## _read(void)						\
	{									\
		return (bus_space_read_4(fdtbus_bs_tag, base, off));			\
	}

#define WIN_REG_BASE_RD(pre,reg,off)						\
	static __inline uint32_t						\
	pre ## _ ## reg ## _read(uint32_t base)					\
	{									\
		return (bus_space_read_4(fdtbus_bs_tag, base, off));			\
	}

#define WIN_REG_WR(pre,reg,off,base)						\
	static __inline void							\
	pre ## _ ## reg ## _write(uint32_t val)					\
	{									\
		bus_space_write_4(fdtbus_bs_tag, base, off, val);			\
	}

#define WIN_REG_BASE_WR(pre,reg,off)						\
	static __inline void							\
	pre ## _ ## reg ## _write(uint32_t base, uint32_t val)			\
	{									\
		bus_space_write_4(fdtbus_bs_tag, base, off, val);			\
	}

#endif /* _MVWIN_H_ */
