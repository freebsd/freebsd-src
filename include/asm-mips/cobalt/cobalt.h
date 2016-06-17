/*
 * Lowlevel hardware stuff for the MIPS based Cobalt microservers.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1997 Cobalt Microserver
 * Copyright (C) 1997 Ralf Baechle
 * Copyright (C) 2001, 2002, 2003 Liam Davies (ldavies@agile.tv)
 *
 */
#ifndef __ASM_MIPS_COBALT_H
#define __ASM_MIPS_COBALT_H

/*
 * COBALT interrupt enable bits
 */
#define COBALT_IE_PCI          (1 << 0)
#define COBALT_IE_FLOPPY       (1 << 1)
#define COBALT_IE_KEYBOARD     (1 << 2)
#define COBALT_IE_SERIAL1      (1 << 3)
#define COBALT_IE_SERIAL2      (1 << 4)
#define COBALT_IE_PARALLEL     (1 << 5)
#define COBALT_IE_GPIO         (1 << 6)
#define COBALT_IE_RTC          (1 << 7)

/*
 * PCI defines
 */
#define COBALT_IE_ETHERNET     (1 << 7)
#define COBALT_IE_SCSI         (1 << 7)

/*
 * COBALT Interrupt Level definitions.
 * These should match the request IRQ id's.
 */
#define COBALT_TIMER_IRQ       0
#define COBALT_KEYBOARD_IRQ    1
#define COBALT_QUBE_SLOT_IRQ   9
#define COBALT_ETH0_IRQ        4
#define COBALT_ETH1_IRQ        13
#define COBALT_SCC_IRQ         4
#define COBALT_SERIAL2_IRQ     4
#define COBALT_PARALLEL_IRQ    5
#define COBALT_FLOPPY_IRQ      6 /* needs to be consistent with floppy driver! */
#define COBALT_SCSI_IRQ        7
#define COBALT_SERIAL_IRQ      7
#define COBALT_RAQ_SCSI_IRQ    4

/*
 * PCI configuration space manifest constants.  These are wired into
 * the board layout according to the PCI spec to enable the software
 * to probe the hardware configuration space in a well defined manner.
 *
 * The PCI_DEVSHFT() macro transforms these values into numbers
 * suitable for passing as the dev parameter to the various
 * pcibios_read/write_config routines.
 */
#define COBALT_PCICONF_CPU      0x06
#define COBALT_PCICONF_ETH0     0x07
#define COBALT_PCICONF_RAQSCSI  0x08
#define COBALT_PCICONF_VIA      0x09
#define COBALT_PCICONF_PCISLOT  0x0A
#define COBALT_PCICONF_ETH1     0x0C


/*
 * The Cobalt board id information.  The boards have an ID number wired
 * into the VIA that is available in the high nibble of register 94.
 * This register is available in the VIA configuration space through the
 * interface routines qube_pcibios_read/write_config. See cobalt/pci.c
 */
#define VIA_COBALT_BRD_ID_REG  0x94
#define VIA_COBALT_BRD_REG_to_ID(reg)  ((unsigned char) (reg) >> 4)
#define COBALT_BRD_ID_QUBE1    0x3
#define COBALT_BRD_ID_RAQ1     0x4
#define COBALT_BRD_ID_QUBE2    0x5
#define COBALT_BRD_ID_RAQ2     0x6


/*
 * Galileo chipset access macros for the Cobalt. The base address for
 * the GT64111 chip is 0x14000000
 */
#define GT64111_BASE		0x04000000
#define GALILEO_REG(ofs)	(GT64111_BASE + (ofs))

#define GALILEO_INL(port)	(inl(GALILEO_REG(port)))
#define GALILEO_OUTL(val, port)	outl(val, GALILEO_REG(port))

#define GALILEO_T0EXP		0x0100
#define GALILEO_ENTC0		0x01
#define GALILEO_SELTC0		0x02

#endif /* __ASM_MIPS_COBALT_H */
