/*
 * Copyright (c) 1997, Stefan Esser <se@freebsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
 *
 * $FreeBSD$
 *
 */

/*
 * PCIM_xxx: mask to locate subfield in register
 * PCIR_xxx: config register offset
 * PCIC_xxx: device class
 * PCIS_xxx: device subclass
 * PCIP_xxx: device programming interface
 * PCIV_xxx: PCI vendor ID (only required to fixup ancient devices)
 * PCID_xxx: device ID
 */

/* some PCI bus constants */

#define PCI_BUSMAX	255
#define PCI_SLOTMAX	31
#define PCI_FUNCMAX	7
#define PCI_REGMAX	255

/* PCI config header registers for all devices */

#define PCIR_DEVVENDOR	0x00
#define PCIR_VENDOR	0x00
#define PCIR_DEVICE	0x02
#define PCIR_COMMAND	0x04
#define PCIM_CMD_PORTEN		0x0001
#define PCIM_CMD_MEMEN		0x0002
#define PCIM_CMD_MWRICEN	0x0010
#define PCIM_CMD_BUSMASTEREN	0x0004
#define PCIM_CMD_PERRESPEN	0x0040
#define PCIR_STATUS	0x06
#define PCIR_REVID	0x08
#define PCIR_PROGIF	0x09
#define PCIR_SUBCLASS	0x0a
#define PCIR_CLASS	0x0b
#define PCIR_CACHELNSZ	0x0c
#define PCIR_LATTIMER	0x0d
#define PCIR_HEADERTYPE	0x0e
#define PCIM_MFDEV		0x80
#define PCIR_BIST	0x0f

/* config registers for header type 0 devices */

#define PCIR_MAPS	0x10
#define PCIR_CARDBUSCIS	0x28
#define PCIR_SUBVEND_0	0x2c
#define PCIR_SUBDEV_0	0x2e
#define PCIR_BIOS	0x30
#define PCIM_BIOS_ENABLE	0x01
#define PCIR_INTLINE	0x3c
#define PCIR_INTPIN	0x3d
#define PCIR_MINGNT	0x3e
#define PCIR_MAXLAT	0x3f

/* config registers for header type 1 devices */

#define PCIR_SECSTAT_1	0 /**/

#define PCIR_PRIBUS_1	0x18
#define PCIR_SECBUS_1	0x19
#define PCIR_SUBBUS_1	0x1a
#define PCIR_SECLAT_1	0x1b

#define PCIR_IOBASEL_1	0x1c
#define PCIR_IOLIMITL_1	0x1d
#define PCIR_IOBASEH_1	0 /**/
#define PCIR_IOLIMITH_1	0 /**/

#define PCIR_MEMBASE_1	0x20
#define PCIR_MEMLIMIT_1	0x22

#define PCIR_PMBASEL_1	0x24
#define PCIR_PMLIMITL_1	0x26
#define PCIR_PMBASEH_1	0 /**/
#define PCIR_PMLIMITH_1	0 /**/

#define PCIR_BRIDGECTL_1 0 /**/

#define PCIR_SUBVEND_1	0x34
#define PCIR_SUBDEV_1	0x36

/* config registers for header type 2 devices */

#define PCIR_SECSTAT_2	0x16

#define PCIR_PRIBUS_2	0x18
#define PCIR_SECBUS_2	0x19
#define PCIR_SUBBUS_2	0x1a
#define PCIR_SECLAT_2	0x1b

#define PCIR_MEMBASE0_2	0x1c
#define PCIR_MEMLIMIT0_2 0x20
#define PCIR_MEMBASE1_2	0x24
#define PCIR_MEMLIMIT1_2 0x28
#define PCIR_IOBASE0_2	0x2c
#define PCIR_IOLIMIT0_2	0x30
#define PCIR_IOBASE1_2	0x34
#define PCIR_IOLIMIT1_2	0x38

#define PCIR_BRIDGECTL_2 0x3e

#define PCIR_SUBVEND_2	0x40
#define PCIR_SUBDEV_2	0x42

#define PCIR_PCCARDIF_2	0x44

/* PCI device class, subclass and programming interface definitions */

#define PCIC_OLD	0x00
#define PCIS_OLD_NONVGA		0x00
#define PCIS_OLD_VGA		0x01

#define PCIC_STORAGE	0x01
#define PCIS_STORAGE_SCSI	0x00
#define PCIS_STORAGE_IDE	0x01
#define PCIP_STORAGE_IDE_MODEPRIM	0x01
#define PCIP_STORAGE_IDE_PROGINDPRIM	0x02
#define PCIP_STORAGE_IDE_MODESEC	0x04
#define PCIP_STORAGE_IDE_PROGINDSEC	0x08
#define PCIP_STORAGE_IDE_MASTERDEV	0x80
#define PCIS_STORAGE_FLOPPY	0x02
#define PCIS_STORAGE_IPI	0x03
#define PCIS_STORAGE_RAID	0x04
#define PCIS_STORAGE_OTHER	0x80

#define PCIC_NETWORK	0x02
#define PCIS_NETWORK_ETHERNET	0x00
#define PCIS_NETWORK_TOKENRING	0x01
#define PCIS_NETWORK_FDDI	0x02
#define PCIS_NETWORK_ATM	0x03
#define PCIS_NETWORK_OTHER	0x80

#define PCIC_DISPLAY	0x03
#define PCIS_DISPLAY_VGA	0x00
#define PCIS_DISPLAY_XGA	0x01
#define PCIS_DISPLAY_OTHER	0x80

#define PCIC_MULTIMEDIA	0x04
#define PCIS_MULTIMEDIA_VIDEO	0x00
#define PCIS_MULTIMEDIA_AUDIO	0x01
#define PCIS_MULTIMEDIA_OTHER	0x80

#define PCIC_MEMORY	0x05
#define PCIS_MEMORY_RAM		0x00
#define PCIS_MEMORY_FLASH	0x01
#define PCIS_MEMORY_OTHER	0x80

#define PCIC_BRIDGE	0x06
#define PCIS_BRIDGE_HOST	0x00
#define PCIS_BRIDGE_ISA		0x01
#define PCIS_BRIDGE_EISA	0x02
#define PCIS_BRIDGE_MCA		0x03
#define PCIS_BRIDGE_PCI		0x04
#define PCIS_BRIDGE_PCMCIA	0x05
#define PCIS_BRIDGE_NUBUS	0x06
#define PCIS_BRIDGE_CARDBUS	0x07
#define PCIS_BRIDGE_OTHER	0x80

#define PCIC_SIMPLECOMM	0x07
#define PCIS_SIMPLECOMM_UART	0x00
#define PCIP_SIMPLECOMM_UART_16550A	0x02
#define PCIS_SIMPLECOMM_PAR	0x01
#define PCIS_SIMPLECOMM_OTHER	0x80

#define PCIC_BASEPERIPH	0x08
#define PCIS_BASEPERIPH_PIC	0x00
#define PCIS_BASEPERIPH_DMA	0x01
#define PCIS_BASEPERIPH_TIMER	0x02
#define PCIS_BASEPERIPH_RTC	0x03
#define PCIS_BASEPERIPH_OTHER	0x80

#define PCIC_INPUTDEV	0x09
#define PCIS_INPUTDEV_KEYBOARD	0x00
#define PCIS_INPUTDEV_DIGITIZER	0x01
#define PCIS_INPUTDEV_MOUSE	0x02
#define PCIS_INPUTDEV_OTHER	0x80

#define PCIC_DOCKING	0x0a
#define PCIS_DOCKING_GENERIC	0x00
#define PCIS_DOCKING_OTHER	0x80

#define PCIC_PROCESSOR	0x0b
#define PCIS_PROCESSOR_386	0x00
#define PCIS_PROCESSOR_486	0x01
#define PCIS_PROCESSOR_PENTIUM	0x02
#define PCIS_PROCESSOR_ALPHA	0x10
#define PCIS_PROCESSOR_POWERPC	0x20
#define PCIS_PROCESSOR_COPROC	0x40

#define PCIC_SERIALBUS	0x0c
#define PCIS_SERIALBUS_FW	0x00
#define PCIS_SERIALBUS_ACCESS	0x01
#define PCIS_SERIALBUS_SSA	0x02
#define PCIS_SERIALBUS_USB	0x03
#define PCIS_SERIALBUS_FC	0x04
#define PCIS_SERIALBUS
#define PCIS_SERIALBUS

#define PCIC_OTHER	0xff

/* some PCI vendor definitions (only used to identify ancient devices !!! */

#define PCIV_INTEL	0x8086

#define PCID_INTEL_SATURN	0x0483
#define PCID_INTEL_ORION	0x84c4

/* for compatibility to FreeBSD-2.2 and 3.x versions of PCI code */

#if defined(_KERNEL) && !defined(KLD_MODULE)
#include "opt_compat_oldpci.h"
#endif

#ifdef COMPAT_OLDPCI

#define PCI_ID_REG		0x00
#define PCI_COMMAND_STATUS_REG	0x04
#define	PCI_COMMAND_IO_ENABLE		0x00000001
#define	PCI_COMMAND_MEM_ENABLE		0x00000002
#define PCI_CLASS_REG		0x08
#define PCI_CLASS_MASK			0xff000000
#define PCI_SUBCLASS_MASK		0x00ff0000
#define	PCI_REVISION_MASK		0x000000ff
#define PCI_CLASS_PREHISTORIC		0x00000000
#define PCI_SUBCLASS_PREHISTORIC_VGA		0x00010000
#define PCI_CLASS_MASS_STORAGE		0x01000000
#define PCI_CLASS_DISPLAY		0x03000000
#define PCI_SUBCLASS_DISPLAY_VGA		0x00000000
#define PCI_CLASS_BRIDGE		0x06000000
#define PCI_MAP_REG_START	0x10
#define	PCI_MAP_REG_END		0x28
#define	PCI_MAP_IO			0x00000001
#define	PCI_INTERRUPT_REG	0x3c

#endif /* COMPAT_OLDPCI */
