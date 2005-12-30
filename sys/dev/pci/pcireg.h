/*-
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
 * PCIY_xxx: capability identification number
 */

/* some PCI bus constants */

#define PCI_BUSMAX	255
#define PCI_SLOTMAX	31
#define PCI_FUNCMAX	7
#define PCI_REGMAX	255
#define PCI_MAXHDRTYPE	2

/* PCI config header registers for all devices */

#define PCIR_DEVVENDOR	0x00
#define PCIR_VENDOR	0x00
#define PCIR_DEVICE	0x02
#define PCIR_COMMAND	0x04
#define PCIM_CMD_PORTEN		0x0001
#define PCIM_CMD_MEMEN		0x0002
#define PCIM_CMD_BUSMASTEREN	0x0004
#define PCIM_CMD_SPECIALEN	0x0008
#define PCIM_CMD_MWRICEN	0x0010
#define PCIM_CMD_PERRESPEN	0x0040
#define PCIM_CMD_SERRESPEN	0x0100
#define PCIM_CMD_BACKTOBACK	0x0200
#define PCIR_STATUS	0x06
#define PCIM_STATUS_CAPPRESENT	0x0010
#define PCIM_STATUS_66CAPABLE	0x0020
#define PCIM_STATUS_BACKTOBACK	0x0080
#define PCIM_STATUS_PERRREPORT	0x0100
#define PCIM_STATUS_SEL_FAST	0x0000
#define PCIM_STATUS_SEL_MEDIMUM	0x0200
#define PCIM_STATUS_SEL_SLOW	0x0400
#define PCIM_STATUS_SEL_MASK	0x0600
#define PCIM_STATUS_STABORT	0x0800
#define PCIM_STATUS_RTABORT	0x1000
#define PCIM_STATUS_RMABORT	0x2000
#define PCIM_STATUS_SERR	0x4000
#define PCIM_STATUS_PERR	0x8000
#define PCIR_REVID	0x08
#define PCIR_PROGIF	0x09
#define PCIR_SUBCLASS	0x0a
#define PCIR_CLASS	0x0b
#define PCIR_CACHELNSZ	0x0c
#define PCIR_LATTIMER	0x0d
#define PCIR_HDRTYPE	0x0e
#define PCIM_HDRTYPE		0x7f
#define PCIM_HDRTYPE_NORMAL	0x00
#define PCIM_HDRTYPE_BRIDGE	0x01
#define PCIM_HDRTYPE_CARDBUS	0x02
#define PCIM_MFDEV		0x80
#define PCIR_BIST	0x0f

/* Capability Register Offsets */

#define	PCICAP_ID	0x0
#define	PCICAP_NEXTPTR	0x1

/* Capability Identification Numbers */

#define PCIY_PMG	0x01	/* PCI Power Management */
#define PCIY_AGP	0x02	/* AGP */
#define PCIY_VPD	0x03	/* Vital Product Data */
#define PCIY_SLOTID	0x04	/* Slot Identification */
#define PCIY_MSI	0x05	/* Message Signaled Interrupts */
#define PCIY_CHSWP	0x06	/* CompactPCI Hot Swap */
#define PCIY_PCIX	0x07	/* PCI-X */
#define PCIY_HT		0x08	/* HyperTransport */
#define PCIY_VENDOR	0x09	/* Vendor Unique */
#define PCIY_DEBUG	0x0a	/* Debug port */
#define PCIY_CRES	0x0b	/* CompactPCI central resource control */
#define PCIY_HOTPLUG	0x0c	/* PCI Hot-Plug */
#define PCIY_AGP8X	0x0e	/* AGP 8x */
#define PCIY_SECDEV	0x0f	/* Secure Device */
#define PCIY_EXPRESS	0x10	/* PCI Express */
#define PCIY_MSIX	0x11	/* MSI-X */

/* config registers for header type 0 devices */

#define PCIR_BARS	0x10
#define	PCIR_BAR(x)	(PCIR_BARS + (x) * 4)
#define PCI_RID2BAR(rid) (((rid)-PCIR_BARS)/4)
#define PCIR_CIS	0x28
#define PCIM_CIS_ASI_MASK	0x7
#define PCIM_CIS_ASI_TUPLE	0
#define PCIM_CIS_ASI_BAR0	1
#define PCIM_CIS_ASI_BAR1	2
#define PCIM_CIS_ASI_BAR2	3
#define PCIM_CIS_ASI_BAR3	4
#define PCIM_CIS_ASI_BAR4	5
#define PCIM_CIS_ASI_BAR5	6
#define PCIM_CIS_ASI_ROM	7
#define PCIM_CIS_ADDR_MASK	0x0ffffff8
#define PCIM_CIS_ROM_MASK	0xf0000000
#define PCIR_SUBVEND_0	0x2c
#define PCIR_SUBDEV_0	0x2e
#define PCIR_BIOS	0x30
#define PCIM_BIOS_ENABLE	0x01
#define PCIM_BIOS_ADDR_MASK	0xfffff800
#define	PCIR_CAP_PTR	0x34
#define PCIR_INTLINE	0x3c
#define PCIR_INTPIN	0x3d
#define PCIR_MINGNT	0x3e
#define PCIR_MAXLAT	0x3f

/* config registers for header type 1 (PCI-to-PCI bridge) devices */

#define PCIR_SECSTAT_1	0x1e

#define PCIR_PRIBUS_1	0x18
#define PCIR_SECBUS_1	0x19
#define PCIR_SUBBUS_1	0x1a
#define PCIR_SECLAT_1	0x1b

#define PCIR_IOBASEL_1	0x1c
#define PCIR_IOLIMITL_1	0x1d
#define PCIR_IOBASEH_1	0x30
#define PCIR_IOLIMITH_1	0x32
#define PCIM_BRIO_16		0x0
#define PCIM_BRIO_32		0x1
#define PCIM_BRIO_MASK		0xf

#define PCIR_MEMBASE_1	0x20
#define PCIR_MEMLIMIT_1	0x22

#define PCIR_PMBASEL_1	0x24
#define PCIR_PMLIMITL_1	0x26
#define PCIR_PMBASEH_1	0x28
#define PCIR_PMLIMITH_1	0x2c

#define PCIR_BRIDGECTL_1 0x3e

#define PCIR_SUBVEND_1	0x34
#define PCIR_SUBDEV_1	0x36

/* config registers for header type 2 (CardBus) devices */

#define	PCIR_CAP_PTR_2	0x14
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
#define PCIS_NETWORK_ISDN	0x04
#define PCIS_NETWORK_OTHER	0x80

#define PCIC_DISPLAY	0x03
#define PCIS_DISPLAY_VGA	0x00
#define PCIS_DISPLAY_XGA	0x01
#define PCIS_DISPLAY_3D		0x02
#define PCIS_DISPLAY_OTHER	0x80

#define PCIC_MULTIMEDIA	0x04
#define PCIS_MULTIMEDIA_VIDEO	0x00
#define PCIS_MULTIMEDIA_AUDIO	0x01
#define PCIS_MULTIMEDIA_TELE	0x02
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
#define PCIS_BRIDGE_RACEWAY	0x08
#define PCIS_BRIDGE_OTHER	0x80

#define PCIC_SIMPLECOMM	0x07
#define PCIS_SIMPLECOMM_UART	0x00
#define PCIP_SIMPLECOMM_UART_16550A	0x02
#define PCIS_SIMPLECOMM_PAR	0x01
#define PCIS_SIMPLECOMM_MULSER	0x02
#define PCIS_SIMPLECOMM_MODEM	0x03
#define PCIS_SIMPLECOMM_OTHER	0x80

#define PCIC_BASEPERIPH	0x08
#define PCIS_BASEPERIPH_PIC	0x00
#define PCIS_BASEPERIPH_DMA	0x01
#define PCIS_BASEPERIPH_TIMER	0x02
#define PCIS_BASEPERIPH_RTC	0x03
#define PCIS_BASEPERIPH_PCIHOT	0x04
#define PCIS_BASEPERIPH_OTHER	0x80

#define PCIC_INPUTDEV	0x09
#define PCIS_INPUTDEV_KEYBOARD	0x00
#define PCIS_INPUTDEV_DIGITIZER	0x01
#define PCIS_INPUTDEV_MOUSE	0x02
#define PCIS_INPUTDEV_SCANNER	0x03
#define PCIS_INPUTDEV_GAMEPORT	0x04
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
#define PCIS_PROCESSOR_MIPS	0x30
#define PCIS_PROCESSOR_COPROC	0x40

#define PCIC_SERIALBUS	0x0c
#define PCIS_SERIALBUS_FW	0x00
#define PCIS_SERIALBUS_ACCESS	0x01
#define PCIS_SERIALBUS_SSA	0x02
#define PCIS_SERIALBUS_USB	0x03
#define PCIP_SERIALBUS_USB_UHCI	0x00
#define PCIP_SERIALBUS_USB_OHCI	0x10
#define PCIP_SERIALBUS_USB_EHCI	0x20
#define PCIS_SERIALBUS_FC	0x04
#define PCIS_SERIALBUS_SMBUS	0x05

#define PCIC_WIRELESS	0x0d
#define PCIS_WIRELESS_IRDA	0x00
#define PCIS_WIRELESS_IR	0x01
#define PCIS_WIRELESS_RF	0x10
#define PCIS_WIRELESS_OTHER	0x80

#define PCIC_INTELLIIO	0x0e
#define PCIS_INTELLIIO_I2O	0x00

#define PCIC_SATCOM	0x0f
#define PCIS_SATCOM_TV		0x01
#define PCIS_SATCOM_AUDIO	0x02
#define PCIS_SATCOM_VOICE	0x03
#define PCIS_SATCOM_DATA	0x04

#define PCIC_CRYPTO	0x10
#define PCIS_CRYPTO_NETCOMP	0x00
#define PCIS_CRYPTO_ENTERTAIN	0x10
#define PCIS_CRYPTO_OTHER	0x80

#define PCIC_DASP	0x11
#define PCIS_DASP_DPIO	0x00
#define PCIS_DASP_OTHER	0x80

#define PCIC_OTHER	0xff

/* Bridge Control Values. */
#define	PCIB_BCR_PERR_ENABLE		0x0001
#define	PCIB_BCR_SERR_ENABLE		0x0002
#define	PCIB_BCR_ISA_ENABLE		0x0004
#define	PCIB_BCR_VGA_ENABLE		0x0008
#define	PCIB_BCR_MASTER_ABORT_MODE	0x0020
#define	PCIB_BCR_SECBUS_RESET		0x0040
#define	PCIB_BCR_SECBUS_BACKTOBACK	0x0080
#define	PCIB_BCR_PRI_DISCARD_TIMEOUT	0x0100
#define	PCIB_BCR_SEC_DISCARD_TIMEOUT	0x0200
#define	PCIB_BCR_DISCARD_TIMER_STATUS	0x0400
#define	PCIB_BCR_DISCARD_TIMER_SERREN	0x0800

/* PCI power manangement */

#define PCIR_POWER_CAP		0x2
#define PCIM_PCAP_SPEC			0x0007
#define PCIM_PCAP_PMEREQCLK		0x0008
#define PCIM_PCAP_PMEREQPWR		0x0010
#define PCIM_PCAP_DEVSPECINIT		0x0020
#define PCIM_PCAP_DYNCLOCK		0x0040
#define PCIM_PCAP_SECCLOCK		0x00c0
#define PCIM_PCAP_CLOCKMASK		0x00c0
#define PCIM_PCAP_REQFULLCLOCK		0x0100
#define PCIM_PCAP_D1SUPP		0x0200
#define PCIM_PCAP_D2SUPP		0x0400
#define PCIM_PCAP_D0PME			0x1000
#define PCIM_PCAP_D1PME			0x2000
#define PCIM_PCAP_D2PME			0x4000

#define PCIR_POWER_STATUS	0x4
#define PCIM_PSTAT_D0			0x0000
#define PCIM_PSTAT_D1			0x0001
#define PCIM_PSTAT_D2			0x0002
#define PCIM_PSTAT_D3			0x0003
#define PCIM_PSTAT_DMASK		0x0003
#define PCIM_PSTAT_REPENABLE		0x0010
#define PCIM_PSTAT_PMEENABLE		0x0100
#define PCIM_PSTAT_D0POWER		0x0000
#define PCIM_PSTAT_D1POWER		0x0200
#define PCIM_PSTAT_D2POWER		0x0400
#define PCIM_PSTAT_D3POWER		0x0600
#define PCIM_PSTAT_D0HEAT		0x0800
#define PCIM_PSTAT_D1HEAT		0x1000
#define PCIM_PSTAT_D2HEAT		0x1200
#define PCIM_PSTAT_D3HEAT		0x1400
#define PCIM_PSTAT_DATAUNKN		0x0000
#define PCIM_PSTAT_DATADIV10		0x2000
#define PCIM_PSTAT_DATADIV100		0x4000
#define PCIM_PSTAT_DATADIV1000		0x6000
#define PCIM_PSTAT_DATADIVMASK		0x6000
#define PCIM_PSTAT_PME			0x8000

#define PCIR_POWER_PMCSR	0x6
#define PCIM_PMCSR_DCLOCK		0x10
#define PCIM_PMCSR_B2SUPP		0x20
#define PCIM_BMCSR_B3SUPP		0x40
#define PCIM_BMCSR_BPCE			0x80

#define PCIR_POWER_DATA		0x7

/* PCI Message Signalled Interrupts (MSI) */
#define PCIR_MSI_CTRL		0x2
#define PCIM_MSICTRL_VECTOR		0x0100
#define PCIM_MSICTRL_64BIT		0x0080
#define PCIM_MSICTRL_MME_MASK		0x0070
#define PCIM_MSICTRL_MME_1		0x0000
#define PCIM_MSICTRL_MME_2		0x0010
#define PCIM_MSICTRL_MME_4		0x0020
#define PCIM_MSICTRL_MME_8		0x0030
#define PCIM_MSICTRL_MME_16		0x0040
#define PCIM_MSICTRL_MME_32		0x0050
#define PCIM_MSICTRL_MMC_MASK		0x000E
#define PCIM_MSICTRL_MMC_1		0x0000
#define PCIM_MSICTRL_MMC_2		0x0002
#define PCIM_MSICTRL_MMC_4		0x0004
#define PCIM_MSICTRL_MMC_8		0x0006
#define PCIM_MSICTRL_MMC_16		0x0008
#define PCIM_MSICTRL_MMC_32		0x000A
#define PCIM_MSICTRL_MSI_ENABLE		0x0001
#define PCIR_MSI_ADDR		0x4
#define PCIR_MSI_ADDR_HIGH	0x8
#define PCIR_MSI_DATA		0x8
#define PCIR_MSI_DATA_64BIT	0xc
#define PCIR_MSI_MASK		0x10
#define PCIR_MSI_PENDING	0x14

/* PCI-X definitions */
#define PCIXR_COMMAND	0x96
#define PCIXR_DEVADDR	0x98
#define PCIXM_DEVADDR_FNUM	0x0003	/* Function Number */
#define PCIXM_DEVADDR_DNUM	0x00F8	/* Device Number */
#define PCIXM_DEVADDR_BNUM	0xFF00	/* Bus Number */
#define PCIXR_STATUS	0x9A
#define PCIXM_STATUS_64BIT	0x0001	/* Active 64bit connection to device. */
#define PCIXM_STATUS_133CAP	0x0002	/* Device is 133MHz capable */
#define PCIXM_STATUS_SCDISC	0x0004	/* Split Completion Discarded */
#define PCIXM_STATUS_UNEXPSC	0x0008	/* Unexpected Split Completion */
#define PCIXM_STATUS_CMPLEXDEV	0x0010	/* Device Complexity (set == bridge) */
#define PCIXM_STATUS_MAXMRDBC	0x0060	/* Maximum Burst Read Count */
#define PCIXM_STATUS_MAXSPLITS	0x0380	/* Maximum Split Transactions */
#define PCIXM_STATUS_MAXCRDS	0x1C00	/* Maximum Cumulative Read Size */
#define PCIXM_STATUS_RCVDSCEM	0x2000	/* Received a Split Comp w/Error msg */
