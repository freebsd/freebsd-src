/**************************************************************************
**
**  $Id: pcireg.h,v 1.7 1996/01/25 18:31:59 se Exp $
**
**  Names for PCI configuration space registers.
**
** Copyright (c) 1994 Wolfgang Stanglmeier.  All rights reserved.
**
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**
***************************************************************************
*/

#ifndef __PCI_REG_H__
#define __PCI_REG_H__ "pl2 95/03/21"

/*
** Device identification register; contains a vendor ID and a device ID.
** We have little need to distinguish the two parts.
*/
#define	PCI_ID_REG			0x00

/*
** Command and status register.
*/
#define	PCI_COMMAND_STATUS_REG		0x04

#define	PCI_COMMAND_IO_ENABLE		0x00000001
#define	PCI_COMMAND_MEM_ENABLE		0x00000002
#define	PCI_COMMAND_MASTER_ENABLE	0x00000004
#define	PCI_COMMAND_SPECIAL_ENABLE	0x00000008
#define	PCI_COMMAND_INVALIDATE_ENABLE	0x00000010
#define	PCI_COMMAND_PALETTE_ENABLE	0x00000020
#define	PCI_COMMAND_PARITY_ENABLE	0x00000040
#define	PCI_COMMAND_STEPPING_ENABLE	0x00000080
#define	PCI_COMMAND_SERR_ENABLE		0x00000100
#define	PCI_COMMAND_BACKTOBACK_ENABLE	0x00000200

#define	PCI_STATUS_BACKTOBACK_OKAY	0x00800000
#define	PCI_STATUS_PARITY_ERROR		0x01000000
#define	PCI_STATUS_DEVSEL_FAST		0x00000000
#define	PCI_STATUS_DEVSEL_MEDIUM	0x02000000
#define	PCI_STATUS_DEVSEL_SLOW		0x04000000
#define	PCI_STATUS_DEVSEL_MASK		0x06000000
#define	PCI_STATUS_TARGET_TARGET_ABORT	0x08000000
#define	PCI_STATUS_MASTER_TARGET_ABORT	0x10000000
#define	PCI_STATUS_MASTER_ABORT		0x20000000
#define	PCI_STATUS_SPECIAL_ERROR	0x40000000
#define	PCI_STATUS_PARITY_DETECT	0x80000000

/*
** Class register; defines basic type of device.
*/
#define	PCI_CLASS_REG			0x08

#define	PCI_CLASS_MASK			0xff000000
#define	PCI_SUBCLASS_MASK		0x00ff0000

/* base classes */
#define	PCI_CLASS_PREHISTORIC		0x00000000
#define	PCI_CLASS_MASS_STORAGE		0x01000000
#define	PCI_CLASS_NETWORK		0x02000000
#define	PCI_CLASS_DISPLAY		0x03000000
#define	PCI_CLASS_MULTIMEDIA		0x04000000
#define	PCI_CLASS_MEMORY		0x05000000
#define	PCI_CLASS_BRIDGE		0x06000000
#define	PCI_CLASS_UNDEFINED		0xff000000

/* 0x00 prehistoric subclasses */
#define	PCI_SUBCLASS_PREHISTORIC_MISC	0x00000000
#define	PCI_SUBCLASS_PREHISTORIC_VGA	0x00010000

/* 0x01 mass storage subclasses */
#define	PCI_SUBCLASS_MASS_STORAGE_SCSI	0x00000000
#define	PCI_SUBCLASS_MASS_STORAGE_IDE	0x00010000
#define	PCI_SUBCLASS_MASS_STORAGE_FLOPPY	0x00020000
#define	PCI_SUBCLASS_MASS_STORAGE_IPI	0x00030000
#define	PCI_SUBCLASS_MASS_STORAGE_MISC	0x00800000

/* 0x02 network subclasses */
#define	PCI_SUBCLASS_NETWORK_ETHERNET	0x00000000
#define	PCI_SUBCLASS_NETWORK_TOKENRING	0x00010000
#define	PCI_SUBCLASS_NETWORK_FDDI	0x00020000
#define	PCI_SUBCLASS_NETWORK_MISC	0x00800000

/* 0x03 display subclasses */
#define	PCI_SUBCLASS_DISPLAY_VGA	0x00000000
#define	PCI_SUBCLASS_DISPLAY_XGA	0x00010000
#define	PCI_SUBCLASS_DISPLAY_MISC	0x00800000

/* 0x04 multimedia subclasses */
#define	PCI_SUBCLASS_MULTIMEDIA_VIDEO	0x00000000
#define	PCI_SUBCLASS_MULTIMEDIA_AUDIO	0x00010000
#define	PCI_SUBCLASS_MULTIMEDIA_MISC	0x00800000

/* 0x05 memory subclasses */
#define	PCI_SUBCLASS_MEMORY_RAM		0x00000000
#define	PCI_SUBCLASS_MEMORY_FLASH	0x00010000
#define	PCI_SUBCLASS_MEMORY_MISC	0x00800000

/* 0x06 bridge subclasses */
#define	PCI_SUBCLASS_BRIDGE_HOST	0x00000000
#define	PCI_SUBCLASS_BRIDGE_ISA		0x00010000
#define	PCI_SUBCLASS_BRIDGE_EISA	0x00020000
#define	PCI_SUBCLASS_BRIDGE_MC		0x00030000
#define	PCI_SUBCLASS_BRIDGE_PCI		0x00040000
#define	PCI_SUBCLASS_BRIDGE_PCMCIA	0x00050000
#define	PCI_SUBCLASS_BRIDGE_MISC	0x00800000

/*
** Header registers
*/
#define PCI_HEADER_MISC			0x0c

#define PCI_HEADER_MULTIFUNCTION	0x00800000

/*
** Mapping registers
*/
#define	PCI_MAP_REG_START		0x10
#define	PCI_MAP_REG_END			0x28

#define	PCI_MAP_MEMORY			0x00000000
#define	PCI_MAP_IO			0x00000001

#define	PCI_MAP_MEMORY_TYPE_32BIT	0x00000000
#define	PCI_MAP_MEMORY_TYPE_32BIT_1M	0x00000002
#define	PCI_MAP_MEMORY_TYPE_64BIT	0x00000004
#define	PCI_MAP_MEMORY_TYPE_MASK	0x00000006
#define	PCI_MAP_MEMORY_CACHABLE		0x00000008
#define	PCI_MAP_MEMORY_ADDRESS_MASK	0xfffffff0

#define	PCI_MAP_IO_ADDRESS_MASK         0xfffffffc
/*
** PCI-PCI bridge mapping registers
*/
#define PCI_PCI_BRIDGE_BUS_REG		0x18
#define PCI_PCI_BRIDGE_IO_REG		0x1c
#define PCI_PCI_BRIDGE_MEM_REG		0x20
#define PCI_PCI_BRIDGE_PMEM_REG		0x24

#define PCI_SUBID_REG			0x2c

#define PCI_SUBORDINATE_BUS_MASK	0x00ff0000
#define PCI_SECONDARY_BUS_MASK		0x0000ff00
#define PCI_PRIMARY_BUS_MASK		0x000000ff

#define PCI_SUBORDINATE_BUS_EXTRACT(x)	(((x) >> 16) & 0xff)
#define PCI_SECONDARY_BUS_EXTRACT(x)	(((x) >>  8) & 0xff)
#define PCI_PRIMARY_BUS_EXTRACT(x)	(((x)      ) & 0xff)

#define	PCI_PRIMARY_BUS_INSERT(x, y)	(((x) & ~PCI_PRIMARY_BUS_MASK) | ((y) <<  0))
#define	PCI_SECONDARY_BUS_INSERT(x, y)	(((x) & ~PCI_SECONDARY_BUS_MASK) | ((y) <<  8))
#define	PCI_SUBORDINATE_BUS_INSERT(x, y) (((x) & ~PCI_SUBORDINATE_BUS_MASK) | ((y) << 16))

#define	PCI_PPB_IOBASE_EXTRACT(x)	(((x) << 8) & 0xFF00)
#define	PCI_PPB_IOLIMIT_EXTRACT(x)	(((x) << 0) & 0xFF00)

#define	PCI_PPB_MEMBASE_EXTRACT(x)	(((x) << 16) & 0xFFFF0000)
#define	PCI_PPB_MEMLIMIT_EXTRACT(x)	(((x) <<  0) & 0xFFFF0000)

/*
** Interrupt configuration register
*/
#define	PCI_INTERRUPT_REG		0x3c

#define	PCI_INTERRUPT_PIN_MASK		0x0000ff00
#define	PCI_INTERRUPT_PIN_EXTRACT(x)	((((x) & PCI_INTERRUPT_PIN_MASK) >> 8) & 0xff)
#define	PCI_INTERRUPT_PIN_NONE		0x00
#define	PCI_INTERRUPT_PIN_A		0x01
#define	PCI_INTERRUPT_PIN_B		0x02
#define	PCI_INTERRUPT_PIN_C		0x03
#define	PCI_INTERRUPT_PIN_D		0x04

#define	PCI_INTERRUPT_LINE_MASK		0x000000ff
#define	PCI_INTERRUPT_LINE_EXTRACT(x)	((((x) & PCI_INTERRUPT_LINE_MASK) >> 0) & 0xff)
#define	PCI_INTERRUPT_LINE_INSERT(x,v)	(((x) & ~PCI_INTERRUPT_LINE_MASK) | ((v) << 0))

#endif /* __PCI_REG_H__ */
