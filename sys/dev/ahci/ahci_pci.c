/*-
 * Copyright (c) 2009-2012 Alexander Motin <mav@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/module.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/endian.h>
#include <sys/malloc.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <machine/stdarg.h>
#include <machine/resource.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include "ahci.h"

static int force_ahci = 1;
TUNABLE_INT("hw.ahci.force", &force_ahci);

static const struct {
	uint32_t	id;
	uint8_t		rev;
	const char	*name;
	int		quirks;
} ahci_ids[] = {
	{0x43801002, 0x00, "AMD SB600",
	    AHCI_Q_NOMSI | AHCI_Q_ATI_PMP_BUG | AHCI_Q_MAXIO_64K},
	{0x43901002, 0x00, "AMD SB7x0/SB8x0/SB9x0",
	    AHCI_Q_ATI_PMP_BUG | AHCI_Q_1MSI},
	{0x43911002, 0x00, "AMD SB7x0/SB8x0/SB9x0",
	    AHCI_Q_ATI_PMP_BUG | AHCI_Q_1MSI},
	{0x43921002, 0x00, "AMD SB7x0/SB8x0/SB9x0",
	    AHCI_Q_ATI_PMP_BUG | AHCI_Q_1MSI},
	{0x43931002, 0x00, "AMD SB7x0/SB8x0/SB9x0",
	    AHCI_Q_ATI_PMP_BUG | AHCI_Q_1MSI},
	{0x43941002, 0x00, "AMD SB7x0/SB8x0/SB9x0",
	    AHCI_Q_ATI_PMP_BUG | AHCI_Q_1MSI},
	/* Not sure SB8x0/SB9x0 needs this quirk. Be conservative though */
	{0x43951002, 0x00, "AMD SB8x0/SB9x0",	AHCI_Q_ATI_PMP_BUG},
	{0x78001022, 0x00, "AMD Hudson-2",	0},
	{0x78011022, 0x00, "AMD Hudson-2",	0},
	{0x78021022, 0x00, "AMD Hudson-2",	0},
	{0x78031022, 0x00, "AMD Hudson-2",	0},
	{0x78041022, 0x00, "AMD Hudson-2",	0},
	{0x06111b21, 0x00, "ASMedia ASM2106",	0},
	{0x06121b21, 0x00, "ASMedia ASM1061",	0},
	{0x26528086, 0x00, "Intel ICH6",	AHCI_Q_NOFORCE},
	{0x26538086, 0x00, "Intel ICH6M",	AHCI_Q_NOFORCE},
	{0x26818086, 0x00, "Intel ESB2",	0},
	{0x26828086, 0x00, "Intel ESB2",	0},
	{0x26838086, 0x00, "Intel ESB2",	0},
	{0x27c18086, 0x00, "Intel ICH7",	0},
	{0x27c38086, 0x00, "Intel ICH7",	0},
	{0x27c58086, 0x00, "Intel ICH7M",	0},
	{0x27c68086, 0x00, "Intel ICH7M",	0},
	{0x28218086, 0x00, "Intel ICH8",	0},
	{0x28228086, 0x00, "Intel ICH8",	0},
	{0x28248086, 0x00, "Intel ICH8",	0},
	{0x28298086, 0x00, "Intel ICH8M",	0},
	{0x282a8086, 0x00, "Intel ICH8M",	0},
	{0x29228086, 0x00, "Intel ICH9",	0},
	{0x29238086, 0x00, "Intel ICH9",	0},
	{0x29248086, 0x00, "Intel ICH9",	0},
	{0x29258086, 0x00, "Intel ICH9",	0},
	{0x29278086, 0x00, "Intel ICH9",	0},
	{0x29298086, 0x00, "Intel ICH9M",	0},
	{0x292a8086, 0x00, "Intel ICH9M",	0},
	{0x292b8086, 0x00, "Intel ICH9M",	0},
	{0x292c8086, 0x00, "Intel ICH9M",	0},
	{0x292f8086, 0x00, "Intel ICH9M",	0},
	{0x294d8086, 0x00, "Intel ICH9",	0},
	{0x294e8086, 0x00, "Intel ICH9M",	0},
	{0x3a058086, 0x00, "Intel ICH10",	0},
	{0x3a228086, 0x00, "Intel ICH10",	0},
	{0x3a258086, 0x00, "Intel ICH10",	0},
	{0x3b228086, 0x00, "Intel 5 Series/3400 Series",	0},
	{0x3b238086, 0x00, "Intel 5 Series/3400 Series",	0},
	{0x3b258086, 0x00, "Intel 5 Series/3400 Series",	0},
	{0x3b298086, 0x00, "Intel 5 Series/3400 Series",	0},
	{0x3b2c8086, 0x00, "Intel 5 Series/3400 Series",	0},
	{0x3b2f8086, 0x00, "Intel 5 Series/3400 Series",	0},
	{0x1c028086, 0x00, "Intel Cougar Point",	0},
	{0x1c038086, 0x00, "Intel Cougar Point",	0},
	{0x1c048086, 0x00, "Intel Cougar Point",	0},
	{0x1c058086, 0x00, "Intel Cougar Point",	0},
	{0x1d028086, 0x00, "Intel Patsburg",	0},
	{0x1d048086, 0x00, "Intel Patsburg",	0},
	{0x1d068086, 0x00, "Intel Patsburg",	0},
	{0x28268086, 0x00, "Intel Patsburg (RAID)",	0},
	{0x1e028086, 0x00, "Intel Panther Point",	0},
	{0x1e038086, 0x00, "Intel Panther Point",	0},
	{0x1e048086, 0x00, "Intel Panther Point (RAID)",	0},
	{0x1e058086, 0x00, "Intel Panther Point (RAID)",	0},
	{0x1e068086, 0x00, "Intel Panther Point (RAID)",	0},
	{0x1e078086, 0x00, "Intel Panther Point (RAID)",	0},
	{0x1e0e8086, 0x00, "Intel Panther Point (RAID)",	0},
	{0x1e0f8086, 0x00, "Intel Panther Point (RAID)",	0},
	{0x1f228086, 0x00, "Intel Avoton",	0},
	{0x1f238086, 0x00, "Intel Avoton",	0},
	{0x1f248086, 0x00, "Intel Avoton (RAID)",	0},
	{0x1f258086, 0x00, "Intel Avoton (RAID)",	0},
	{0x1f268086, 0x00, "Intel Avoton (RAID)",	0},
	{0x1f278086, 0x00, "Intel Avoton (RAID)",	0},
	{0x1f2e8086, 0x00, "Intel Avoton (RAID)",	0},
	{0x1f2f8086, 0x00, "Intel Avoton (RAID)",	0},
	{0x1f328086, 0x00, "Intel Avoton",	0},
	{0x1f338086, 0x00, "Intel Avoton",	0},
	{0x1f348086, 0x00, "Intel Avoton (RAID)",	0},
	{0x1f358086, 0x00, "Intel Avoton (RAID)",	0},
	{0x1f368086, 0x00, "Intel Avoton (RAID)",	0},
	{0x1f378086, 0x00, "Intel Avoton (RAID)",	0},
	{0x1f3e8086, 0x00, "Intel Avoton (RAID)",	0},
	{0x1f3f8086, 0x00, "Intel Avoton (RAID)",	0},
	{0x23a38086, 0x00, "Intel Coleto Creek",	0},
	{0x28238086, 0x00, "Intel Wellsburg (RAID)",	0},
	{0x28278086, 0x00, "Intel Wellsburg (RAID)",	0},
	{0x8c028086, 0x00, "Intel Lynx Point",	0},
	{0x8c038086, 0x00, "Intel Lynx Point",	0},
	{0x8c048086, 0x00, "Intel Lynx Point (RAID)",	0},
	{0x8c058086, 0x00, "Intel Lynx Point (RAID)",	0},
	{0x8c068086, 0x00, "Intel Lynx Point (RAID)",	0},
	{0x8c078086, 0x00, "Intel Lynx Point (RAID)",	0},
	{0x8c0e8086, 0x00, "Intel Lynx Point (RAID)",	0},
	{0x8c0f8086, 0x00, "Intel Lynx Point (RAID)",	0},
	{0x8c828086, 0x00, "Intel Wildcat Point",	0},
	{0x8c838086, 0x00, "Intel Wildcat Point",	0},
	{0x8c848086, 0x00, "Intel Wildcat Point (RAID)",	0},
	{0x8c858086, 0x00, "Intel Wildcat Point (RAID)",	0},
	{0x8c868086, 0x00, "Intel Wildcat Point (RAID)",	0},
	{0x8c878086, 0x00, "Intel Wildcat Point (RAID)",	0},
	{0x8c8e8086, 0x00, "Intel Wildcat Point (RAID)",	0},
	{0x8c8f8086, 0x00, "Intel Wildcat Point (RAID)",	0},
	{0x8d028086, 0x00, "Intel Wellsburg",	0},
	{0x8d048086, 0x00, "Intel Wellsburg (RAID)",	0},
	{0x8d068086, 0x00, "Intel Wellsburg (RAID)",	0},
	{0x8d628086, 0x00, "Intel Wellsburg",	0},
	{0x8d648086, 0x00, "Intel Wellsburg (RAID)",	0},
	{0x8d668086, 0x00, "Intel Wellsburg (RAID)",	0},
	{0x8d6e8086, 0x00, "Intel Wellsburg (RAID)",	0},
	{0x9c028086, 0x00, "Intel Lynx Point-LP",	0},
	{0x9c038086, 0x00, "Intel Lynx Point-LP",	0},
	{0x9c048086, 0x00, "Intel Lynx Point-LP (RAID)",	0},
	{0x9c058086, 0x00, "Intel Lynx Point-LP (RAID)",	0},
	{0x9c068086, 0x00, "Intel Lynx Point-LP (RAID)",	0},
	{0x9c078086, 0x00, "Intel Lynx Point-LP (RAID)",	0},
	{0x9c0e8086, 0x00, "Intel Lynx Point-LP (RAID)",	0},
	{0x9c0f8086, 0x00, "Intel Lynx Point-LP (RAID)",	0},
	{0x23238086, 0x00, "Intel DH89xxCC",	0},
	{0x2360197b, 0x00, "JMicron JMB360",	0},
	{0x2361197b, 0x00, "JMicron JMB361",	AHCI_Q_NOFORCE},
	{0x2362197b, 0x00, "JMicron JMB362",	0},
	{0x2363197b, 0x00, "JMicron JMB363",	AHCI_Q_NOFORCE},
	{0x2365197b, 0x00, "JMicron JMB365",	AHCI_Q_NOFORCE},
	{0x2366197b, 0x00, "JMicron JMB366",	AHCI_Q_NOFORCE},
	{0x2368197b, 0x00, "JMicron JMB368",	AHCI_Q_NOFORCE},
	{0x611111ab, 0x00, "Marvell 88SE6111",	AHCI_Q_NOFORCE | AHCI_Q_NOPMP |
	    AHCI_Q_1CH | AHCI_Q_EDGEIS},
	{0x612111ab, 0x00, "Marvell 88SE6121",	AHCI_Q_NOFORCE | AHCI_Q_NOPMP |
	    AHCI_Q_2CH | AHCI_Q_EDGEIS | AHCI_Q_NONCQ | AHCI_Q_NOCOUNT},
	{0x614111ab, 0x00, "Marvell 88SE6141",	AHCI_Q_NOFORCE | AHCI_Q_NOPMP |
	    AHCI_Q_4CH | AHCI_Q_EDGEIS | AHCI_Q_NONCQ | AHCI_Q_NOCOUNT},
	{0x614511ab, 0x00, "Marvell 88SE6145",	AHCI_Q_NOFORCE | AHCI_Q_NOPMP |
	    AHCI_Q_4CH | AHCI_Q_EDGEIS | AHCI_Q_NONCQ | AHCI_Q_NOCOUNT},
	{0x91201b4b, 0x00, "Marvell 88SE912x",	AHCI_Q_EDGEIS},
	{0x91231b4b, 0x11, "Marvell 88SE912x",	AHCI_Q_ALTSIG},
	{0x91231b4b, 0x00, "Marvell 88SE912x",	AHCI_Q_EDGEIS|AHCI_Q_SATA2},
	{0x91251b4b, 0x00, "Marvell 88SE9125",	0},
	{0x91281b4b, 0x00, "Marvell 88SE9128",	AHCI_Q_ALTSIG},
	{0x91301b4b, 0x00, "Marvell 88SE9130",  AHCI_Q_ALTSIG},
	{0x91721b4b, 0x00, "Marvell 88SE9172",	0},
	{0x91821b4b, 0x00, "Marvell 88SE9182",	0},
	{0x91831b4b, 0x00, "Marvell 88SS9183",	0},
	{0x91a01b4b, 0x00, "Marvell 88SE91Ax",	0},
	{0x92151b4b, 0x00, "Marvell 88SE9215",  0},
	{0x92201b4b, 0x00, "Marvell 88SE9220",  AHCI_Q_ALTSIG},
	{0x92301b4b, 0x00, "Marvell 88SE9230",  AHCI_Q_ALTSIG},
	{0x92351b4b, 0x00, "Marvell 88SE9235",  0},
	{0x06201103, 0x00, "HighPoint RocketRAID 620",	0},
	{0x06201b4b, 0x00, "HighPoint RocketRAID 620",	0},
	{0x06221103, 0x00, "HighPoint RocketRAID 622",	0},
	{0x06221b4b, 0x00, "HighPoint RocketRAID 622",	0},
	{0x06401103, 0x00, "HighPoint RocketRAID 640",	0},
	{0x06401b4b, 0x00, "HighPoint RocketRAID 640",	0},
	{0x06441103, 0x00, "HighPoint RocketRAID 644",	0},
	{0x06441b4b, 0x00, "HighPoint RocketRAID 644",	0},
	{0x06411103, 0x00, "HighPoint RocketRAID 640L",	0},
	{0x06421103, 0x00, "HighPoint RocketRAID 642L",	0},
	{0x06451103, 0x00, "HighPoint RocketRAID 644L",	0},
	{0x044c10de, 0x00, "NVIDIA MCP65",	AHCI_Q_NOAA},
	{0x044d10de, 0x00, "NVIDIA MCP65",	AHCI_Q_NOAA},
	{0x044e10de, 0x00, "NVIDIA MCP65",	AHCI_Q_NOAA},
	{0x044f10de, 0x00, "NVIDIA MCP65",	AHCI_Q_NOAA},
	{0x045c10de, 0x00, "NVIDIA MCP65",	AHCI_Q_NOAA},
	{0x045d10de, 0x00, "NVIDIA MCP65",	AHCI_Q_NOAA},
	{0x045e10de, 0x00, "NVIDIA MCP65",	AHCI_Q_NOAA},
	{0x045f10de, 0x00, "NVIDIA MCP65",	AHCI_Q_NOAA},
	{0x055010de, 0x00, "NVIDIA MCP67",	AHCI_Q_NOAA},
	{0x055110de, 0x00, "NVIDIA MCP67",	AHCI_Q_NOAA},
	{0x055210de, 0x00, "NVIDIA MCP67",	AHCI_Q_NOAA},
	{0x055310de, 0x00, "NVIDIA MCP67",	AHCI_Q_NOAA},
	{0x055410de, 0x00, "NVIDIA MCP67",	AHCI_Q_NOAA},
	{0x055510de, 0x00, "NVIDIA MCP67",	AHCI_Q_NOAA},
	{0x055610de, 0x00, "NVIDIA MCP67",	AHCI_Q_NOAA},
	{0x055710de, 0x00, "NVIDIA MCP67",	AHCI_Q_NOAA},
	{0x055810de, 0x00, "NVIDIA MCP67",	AHCI_Q_NOAA},
	{0x055910de, 0x00, "NVIDIA MCP67",	AHCI_Q_NOAA},
	{0x055A10de, 0x00, "NVIDIA MCP67",	AHCI_Q_NOAA},
	{0x055B10de, 0x00, "NVIDIA MCP67",	AHCI_Q_NOAA},
	{0x058410de, 0x00, "NVIDIA MCP67",	AHCI_Q_NOAA},
	{0x07f010de, 0x00, "NVIDIA MCP73",	AHCI_Q_NOAA},
	{0x07f110de, 0x00, "NVIDIA MCP73",	AHCI_Q_NOAA},
	{0x07f210de, 0x00, "NVIDIA MCP73",	AHCI_Q_NOAA},
	{0x07f310de, 0x00, "NVIDIA MCP73",	AHCI_Q_NOAA},
	{0x07f410de, 0x00, "NVIDIA MCP73",	AHCI_Q_NOAA},
	{0x07f510de, 0x00, "NVIDIA MCP73",	AHCI_Q_NOAA},
	{0x07f610de, 0x00, "NVIDIA MCP73",	AHCI_Q_NOAA},
	{0x07f710de, 0x00, "NVIDIA MCP73",	AHCI_Q_NOAA},
	{0x07f810de, 0x00, "NVIDIA MCP73",	AHCI_Q_NOAA},
	{0x07f910de, 0x00, "NVIDIA MCP73",	AHCI_Q_NOAA},
	{0x07fa10de, 0x00, "NVIDIA MCP73",	AHCI_Q_NOAA},
	{0x07fb10de, 0x00, "NVIDIA MCP73",	AHCI_Q_NOAA},
	{0x0ad010de, 0x00, "NVIDIA MCP77",	AHCI_Q_NOAA},
	{0x0ad110de, 0x00, "NVIDIA MCP77",	AHCI_Q_NOAA},
	{0x0ad210de, 0x00, "NVIDIA MCP77",	AHCI_Q_NOAA},
	{0x0ad310de, 0x00, "NVIDIA MCP77",	AHCI_Q_NOAA},
	{0x0ad410de, 0x00, "NVIDIA MCP77",	AHCI_Q_NOAA},
	{0x0ad510de, 0x00, "NVIDIA MCP77",	AHCI_Q_NOAA},
	{0x0ad610de, 0x00, "NVIDIA MCP77",	AHCI_Q_NOAA},
	{0x0ad710de, 0x00, "NVIDIA MCP77",	AHCI_Q_NOAA},
	{0x0ad810de, 0x00, "NVIDIA MCP77",	AHCI_Q_NOAA},
	{0x0ad910de, 0x00, "NVIDIA MCP77",	AHCI_Q_NOAA},
	{0x0ada10de, 0x00, "NVIDIA MCP77",	AHCI_Q_NOAA},
	{0x0adb10de, 0x00, "NVIDIA MCP77",	AHCI_Q_NOAA},
	{0x0ab410de, 0x00, "NVIDIA MCP79",	AHCI_Q_NOAA},
	{0x0ab510de, 0x00, "NVIDIA MCP79",	AHCI_Q_NOAA},
	{0x0ab610de, 0x00, "NVIDIA MCP79",	AHCI_Q_NOAA},
	{0x0ab710de, 0x00, "NVIDIA MCP79",	AHCI_Q_NOAA},
	{0x0ab810de, 0x00, "NVIDIA MCP79",	AHCI_Q_NOAA},
	{0x0ab910de, 0x00, "NVIDIA MCP79",	AHCI_Q_NOAA},
	{0x0aba10de, 0x00, "NVIDIA MCP79",	AHCI_Q_NOAA},
	{0x0abb10de, 0x00, "NVIDIA MCP79",	AHCI_Q_NOAA},
	{0x0abc10de, 0x00, "NVIDIA MCP79",	AHCI_Q_NOAA},
	{0x0abd10de, 0x00, "NVIDIA MCP79",	AHCI_Q_NOAA},
	{0x0abe10de, 0x00, "NVIDIA MCP79",	AHCI_Q_NOAA},
	{0x0abf10de, 0x00, "NVIDIA MCP79",	AHCI_Q_NOAA},
	{0x0d8410de, 0x00, "NVIDIA MCP89",	AHCI_Q_NOAA},
	{0x0d8510de, 0x00, "NVIDIA MCP89",	AHCI_Q_NOFORCE|AHCI_Q_NOAA},
	{0x0d8610de, 0x00, "NVIDIA MCP89",	AHCI_Q_NOAA},
	{0x0d8710de, 0x00, "NVIDIA MCP89",	AHCI_Q_NOAA},
	{0x0d8810de, 0x00, "NVIDIA MCP89",	AHCI_Q_NOAA},
	{0x0d8910de, 0x00, "NVIDIA MCP89",	AHCI_Q_NOAA},
	{0x0d8a10de, 0x00, "NVIDIA MCP89",	AHCI_Q_NOAA},
	{0x0d8b10de, 0x00, "NVIDIA MCP89",	AHCI_Q_NOAA},
	{0x0d8c10de, 0x00, "NVIDIA MCP89",	AHCI_Q_NOAA},
	{0x0d8d10de, 0x00, "NVIDIA MCP89",	AHCI_Q_NOAA},
	{0x0d8e10de, 0x00, "NVIDIA MCP89",	AHCI_Q_NOAA},
	{0x0d8f10de, 0x00, "NVIDIA MCP89",	AHCI_Q_NOAA},
	{0x3781105a, 0x00, "Promise TX8660",	0},
	{0x33491106, 0x00, "VIA VT8251",	AHCI_Q_NOPMP|AHCI_Q_NONCQ},
	{0x62871106, 0x00, "VIA VT8251",	AHCI_Q_NOPMP|AHCI_Q_NONCQ},
	{0x11841039, 0x00, "SiS 966",		0},
	{0x11851039, 0x00, "SiS 968",		0},
	{0x01861039, 0x00, "SiS 968",		0},
	{0xa01c177d, 0x00, "ThunderX",		AHCI_Q_ABAR0|AHCI_Q_1MSI},
	{0x00000000, 0x00, NULL,		0}
};

static int
ahci_pci_ctlr_reset(device_t dev)
{

	if (pci_read_config(dev, PCIR_DEVVENDOR, 4) == 0x28298086 &&
	    (pci_read_config(dev, 0x92, 1) & 0xfe) == 0x04)
		pci_write_config(dev, 0x92, 0x01, 1);
	return ahci_ctlr_reset(dev);
}

static int
ahci_probe(device_t dev)
{
	char buf[64];
	int i, valid = 0;
	uint32_t devid = pci_get_devid(dev);
	uint8_t revid = pci_get_revid(dev);

	/*
	 * Ensure it is not a PCI bridge (some vendors use
	 * the same PID and VID in PCI bridge and AHCI cards).
	 */
	if (pci_get_class(dev) == PCIC_BRIDGE)
		return (ENXIO);

	/* Is this a possible AHCI candidate? */
	if (pci_get_class(dev) == PCIC_STORAGE &&
	    pci_get_subclass(dev) == PCIS_STORAGE_SATA &&
	    pci_get_progif(dev) == PCIP_STORAGE_SATA_AHCI_1_0)
		valid = 1;
	else if (pci_get_class(dev) == PCIC_STORAGE &&
	    pci_get_subclass(dev) == PCIS_STORAGE_RAID)
		valid = 2;
	/* Is this a known AHCI chip? */
	for (i = 0; ahci_ids[i].id != 0; i++) {
		if (ahci_ids[i].id == devid &&
		    ahci_ids[i].rev <= revid &&
		    (valid || (force_ahci == 1 &&
		     !(ahci_ids[i].quirks & AHCI_Q_NOFORCE)))) {
			/* Do not attach JMicrons with single PCI function. */
			if (pci_get_vendor(dev) == 0x197b &&
			    (pci_read_config(dev, 0xdf, 1) & 0x40) == 0)
				return (ENXIO);
			snprintf(buf, sizeof(buf), "%s AHCI SATA controller",
			    ahci_ids[i].name);
			device_set_desc_copy(dev, buf);
			return (BUS_PROBE_DEFAULT);
		}
	}
	if (valid != 1)
		return (ENXIO);
	device_set_desc_copy(dev, "AHCI SATA controller");
	return (BUS_PROBE_DEFAULT);
}

static int
ahci_ata_probe(device_t dev)
{
	char buf[64];
	int i;
	uint32_t devid = pci_get_devid(dev);
	uint8_t revid = pci_get_revid(dev);

	if ((intptr_t)device_get_ivars(dev) >= 0)
		return (ENXIO);
	/* Is this a known AHCI chip? */
	for (i = 0; ahci_ids[i].id != 0; i++) {
		if (ahci_ids[i].id == devid &&
		    ahci_ids[i].rev <= revid) {
			snprintf(buf, sizeof(buf), "%s AHCI SATA controller",
			    ahci_ids[i].name);
			device_set_desc_copy(dev, buf);
			return (BUS_PROBE_DEFAULT);
		}
	}
	device_set_desc_copy(dev, "AHCI SATA controller");
	return (BUS_PROBE_DEFAULT);
}

static int
ahci_pci_attach(device_t dev)
{
	struct ahci_controller *ctlr = device_get_softc(dev);
	int	error, i;
	uint32_t devid = pci_get_devid(dev);
	uint8_t revid = pci_get_revid(dev);

	i = 0;
	while (ahci_ids[i].id != 0 &&
	    (ahci_ids[i].id != devid ||
	     ahci_ids[i].rev > revid))
		i++;
	ctlr->quirks = ahci_ids[i].quirks;
	/* Limit speed for my onboard JMicron external port.
	 * It is not eSATA really, limit to SATA 1 */
	if (pci_get_devid(dev) == 0x2363197b &&
	    pci_get_subvendor(dev) == 0x1043 &&
	    pci_get_subdevice(dev) == 0x81e4)
		ctlr->quirks |= AHCI_Q_SATA1_UNIT0;
	ctlr->vendorid = pci_get_vendor(dev);
	ctlr->deviceid = pci_get_device(dev);
	ctlr->subvendorid = pci_get_subvendor(dev);
	ctlr->subdeviceid = pci_get_subdevice(dev);

	/* Default AHCI Base Address is BAR(5), Cavium uses BAR(0) */
	if (ctlr->quirks & AHCI_Q_ABAR0)
		ctlr->r_rid = PCIR_BAR(0);
	else
		ctlr->r_rid = PCIR_BAR(5);
	if (!(ctlr->r_mem = bus_alloc_resource_any(dev, SYS_RES_MEMORY,
	    &ctlr->r_rid, RF_ACTIVE)))
		return ENXIO;
	pci_enable_busmaster(dev);
	/* Reset controller */
	if ((error = ahci_pci_ctlr_reset(dev)) != 0) {
		bus_release_resource(dev, SYS_RES_MEMORY, ctlr->r_rid, ctlr->r_mem);
		return (error);
	};

	/* Setup interrupts. */

	/* Setup MSI register parameters */
	/* Process hints. */
	if (ctlr->quirks & AHCI_Q_NOMSI)
		ctlr->msi = 0;
	else if (ctlr->quirks & AHCI_Q_1MSI)
		ctlr->msi = 1;
	else
		ctlr->msi = 2;
	resource_int_value(device_get_name(dev),
	    device_get_unit(dev), "msi", &ctlr->msi);
	ctlr->numirqs = 1;
	if (ctlr->msi < 0)
		ctlr->msi = 0;
	else if (ctlr->msi == 1)
		ctlr->msi = min(1, pci_msi_count(dev));
	else if (ctlr->msi > 1) {
		ctlr->msi = 2;
		ctlr->numirqs = pci_msi_count(dev);
	}
	/* Allocate MSI if needed/present. */
	if (ctlr->msi && pci_alloc_msi(dev, &ctlr->numirqs) != 0) {
		ctlr->msi = 0;
		ctlr->numirqs = 1;
	}

	error = ahci_attach(dev);
	if (error != 0)
		if (ctlr->msi)
			pci_release_msi(dev);
	return error;
}

static int
ahci_pci_detach(device_t dev)
{

	ahci_detach(dev);
	pci_release_msi(dev);
	return (0);
}

static int
ahci_pci_suspend(device_t dev)
{
	struct ahci_controller *ctlr = device_get_softc(dev);

	bus_generic_suspend(dev);
	/* Disable interupts, so the state change(s) doesn't trigger */
	ATA_OUTL(ctlr->r_mem, AHCI_GHC,
	     ATA_INL(ctlr->r_mem, AHCI_GHC) & (~AHCI_GHC_IE));
	return 0;
}

static int
ahci_pci_resume(device_t dev)
{
	int res;

	if ((res = ahci_pci_ctlr_reset(dev)) != 0)
		return (res);
	ahci_ctlr_setup(dev);
	return (bus_generic_resume(dev));
}

devclass_t ahci_devclass;
static device_method_t ahci_methods[] = {
	DEVMETHOD(device_probe,     ahci_probe),
	DEVMETHOD(device_attach,    ahci_pci_attach),
	DEVMETHOD(device_detach,    ahci_pci_detach),
	DEVMETHOD(device_suspend,   ahci_pci_suspend),
	DEVMETHOD(device_resume,    ahci_pci_resume),
	DEVMETHOD(bus_print_child,  ahci_print_child),
	DEVMETHOD(bus_alloc_resource,       ahci_alloc_resource),
	DEVMETHOD(bus_release_resource,     ahci_release_resource),
	DEVMETHOD(bus_setup_intr,   ahci_setup_intr),
	DEVMETHOD(bus_teardown_intr,ahci_teardown_intr),
	DEVMETHOD(bus_child_location_str, ahci_child_location_str),
	DEVMETHOD(bus_get_dma_tag,  ahci_get_dma_tag),
	DEVMETHOD_END
};
static driver_t ahci_driver = {
        "ahci",
        ahci_methods,
        sizeof(struct ahci_controller)
};
DRIVER_MODULE(ahci, pci, ahci_driver, ahci_devclass, NULL, NULL);
static device_method_t ahci_ata_methods[] = {
	DEVMETHOD(device_probe,     ahci_ata_probe),
	DEVMETHOD(device_attach,    ahci_pci_attach),
	DEVMETHOD(device_detach,    ahci_pci_detach),
	DEVMETHOD(device_suspend,   ahci_pci_suspend),
	DEVMETHOD(device_resume,    ahci_pci_resume),
	DEVMETHOD(bus_print_child,  ahci_print_child),
	DEVMETHOD(bus_alloc_resource,       ahci_alloc_resource),
	DEVMETHOD(bus_release_resource,     ahci_release_resource),
	DEVMETHOD(bus_setup_intr,   ahci_setup_intr),
	DEVMETHOD(bus_teardown_intr,ahci_teardown_intr),
	DEVMETHOD(bus_child_location_str, ahci_child_location_str),
	DEVMETHOD_END
};
static driver_t ahci_ata_driver = {
        "ahci",
        ahci_ata_methods,
        sizeof(struct ahci_controller)
};
DRIVER_MODULE(ahci, atapci, ahci_ata_driver, ahci_devclass, NULL, NULL);
