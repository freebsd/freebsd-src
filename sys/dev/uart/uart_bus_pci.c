/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2006 Marcel Moolenaar All rights reserved.
 * Copyright (c) 2001 M. Warner Losh <imp@FreeBSD.org>
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>

#include <dev/uart/uart.h>
#include <dev/uart/uart_bus.h>
#include <dev/uart/uart_cpu.h>

#define	DEFAULT_RCLK	1843200

static int uart_pci_probe(device_t dev);
static int uart_pci_attach(device_t dev);
static int uart_pci_detach(device_t dev);

static device_method_t uart_pci_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		uart_pci_probe),
	DEVMETHOD(device_attach,	uart_pci_attach),
	DEVMETHOD(device_detach,	uart_pci_detach),
	DEVMETHOD(device_resume,	uart_bus_resume),
	DEVMETHOD_END
};

static driver_t uart_pci_driver = {
	uart_driver_name,
	uart_pci_methods,
	sizeof(struct uart_softc),
};

struct pci_id {
	uint16_t	vendor;
	uint16_t	device;
	uint16_t	subven;
	uint16_t	subdev;
	const char	*desc;
	int		rid;
	int		rclk;
	int		regshft;
};

struct pci_unique_id {
	uint16_t	vendor;
	uint16_t	device;
};

#define PCI_NO_MSI	0x40000000
#define PCI_RID_MASK	0x0000ffff

static const struct pci_id pci_ns8250_ids[] = {
{ 0x1028, 0x0008, 0xffff, 0, "Dell Remote Access Card III", 0x14,
	128 * DEFAULT_RCLK },
{ 0x1028, 0x0012, 0xffff, 0, "Dell RAC 4 Daughter Card Virtual UART", 0x14,
	128 * DEFAULT_RCLK },
{ 0x1033, 0x0074, 0x1033, 0x8014, "NEC RCV56ACF 56k Voice Modem", 0x10 },
{ 0x1033, 0x007d, 0x1033, 0x8012, "NEC RS232C", 0x10 },
{ 0x103c, 0x1048, 0x103c, 0x1227, "HP Diva Serial [GSP] UART - Powerbar SP2",
	0x10 },
{ 0x103c, 0x1048, 0x103c, 0x1301, "HP Diva RMP3", 0x14 },
{ 0x103c, 0x1290, 0xffff, 0, "HP Auxiliary Diva Serial Port", 0x18 },
{ 0x103c, 0x3301, 0xffff, 0, "HP iLO serial port", 0x10 },
{ 0x11c1, 0x0480, 0xffff, 0, "Agere Systems Venus Modem (V90, 56KFlex)", 0x14 },
{ 0x115d, 0x0103, 0xffff, 0, "Xircom Cardbus Ethernet + 56k Modem", 0x10 },
{ 0x125b, 0x9100, 0xa000, 0x1000,
	"ASIX AX99100 PCIe 1/2/3/4-port RS-232/422/485", 0x10 },
{ 0x1282, 0x6585, 0xffff, 0, "Davicom 56PDV PCI Modem", 0x10 },
{ 0x12b9, 0x1008, 0xffff, 0, "3Com 56K FaxModem Model 5610", 0x10 },
{ 0x131f, 0x1000, 0xffff, 0, "Siig CyberSerial (1-port) 16550", 0x18 },
{ 0x131f, 0x1001, 0xffff, 0, "Siig CyberSerial (1-port) 16650", 0x18 },
{ 0x131f, 0x1002, 0xffff, 0, "Siig CyberSerial (1-port) 16850", 0x18 },
{ 0x131f, 0x2000, 0xffff, 0, "Siig CyberSerial (1-port) 16550", 0x10 },
{ 0x131f, 0x2001, 0xffff, 0, "Siig CyberSerial (1-port) 16650", 0x10 },
{ 0x131f, 0x2002, 0xffff, 0, "Siig CyberSerial (1-port) 16850", 0x10 },
{ 0x135a, 0x0a61, 0xffff, 0, "Brainboxes UC-324", 0x18 },
{ 0x135a, 0x0aa1, 0xffff, 0, "Brainboxes UC-246", 0x18 },
{ 0x135a, 0x0aa2, 0xffff, 0, "Brainboxes UC-246", 0x18 },
{ 0x135a, 0x0d60, 0xffff, 0, "Intashield IS-100", 0x18 },
{ 0x135a, 0x0da0, 0xffff, 0, "Intashield IS-300", 0x18 },
{ 0x135a, 0x4000, 0xffff, 0, "Brainboxes PX-420", 0x10 },
{ 0x135a, 0x4001, 0xffff, 0, "Brainboxes PX-431", 0x10 },
{ 0x135a, 0x4002, 0xffff, 0, "Brainboxes PX-820", 0x10 },
{ 0x135a, 0x4003, 0xffff, 0, "Brainboxes PX-831", 0x10 },
{ 0x135a, 0x4004, 0xffff, 0, "Brainboxes PX-246", 0x10 },
{ 0x135a, 0x4005, 0xffff, 0, "Brainboxes PX-101", 0x10 },
{ 0x135a, 0x4006, 0xffff, 0, "Brainboxes PX-257", 0x10 },
{ 0x135a, 0x4008, 0xffff, 0, "Brainboxes PX-846", 0x10 },
{ 0x135a, 0x4009, 0xffff, 0, "Brainboxes PX-857", 0x10 },
{ 0x135c, 0x0190, 0xffff, 0, "Quatech SSCLP-100", 0x18 },
{ 0x135c, 0x01c0, 0xffff, 0, "Quatech SSCLP-200/300", 0x18 },
{ 0x135e, 0x7101, 0xffff, 0, "Sealevel Systems Single Port RS-232/422/485/530",
	0x18 },
{ 0x1407, 0x0110, 0xffff, 0, "Lava Computer mfg DSerial-PCI Port A", 0x10 },
{ 0x1407, 0x0111, 0xffff, 0, "Lava Computer mfg DSerial-PCI Port B", 0x10 },
{ 0x1407, 0x0510, 0xffff, 0, "Lava SP Serial 550 PCI", 0x10 },
{ 0x1409, 0x7168, 0x1409, 0x4025, "Timedia Technology Serial Port", 0x10,
	8 * DEFAULT_RCLK },
{ 0x1409, 0x7168, 0x1409, 0x4027, "Timedia Technology Serial Port", 0x10,
	8 * DEFAULT_RCLK },
{ 0x1409, 0x7168, 0x1409, 0x4028, "Timedia Technology Serial Port", 0x10,
	8 * DEFAULT_RCLK },
{ 0x1409, 0x7168, 0x1409, 0x5025, "Timedia Technology Serial Port", 0x10,
	8 * DEFAULT_RCLK },
{ 0x1409, 0x7168, 0x1409, 0x5027, "Timedia Technology Serial Port", 0x10,
	8 * DEFAULT_RCLK },
{ 0x1415, 0x950b, 0xffff, 0, "Oxford Semiconductor OXCB950 Cardbus 16950 UART",
	0x10, 16384000 },
{ 0x1415, 0xc120, 0xffff, 0, "Oxford Semiconductor OXPCIe952 PCIe 16950 UART",
	0x10 },
{ 0x14e4, 0x160a, 0xffff, 0, "Broadcom TruManage UART", 0x10,
	128 * DEFAULT_RCLK, 2},
{ 0x14e4, 0x4344, 0xffff, 0, "Sony Ericsson GC89 PC Card", 0x10},
{ 0x151f, 0x0000, 0xffff, 0, "TOPIC Semiconductor TP560 56k modem", 0x10 },
{ 0x1d0f, 0x8250, 0x0000, 0, "Amazon PCI serial device", 0x10 },
{ 0x1d0f, 0x8250, 0x1d0f, 0, "Amazon PCI serial device", 0x10 },
{ 0x1fd4, 0x1999, 0x1fd4, 0x0001, "Sunix SER5xxxx Serial Port", 0x10,
	8 * DEFAULT_RCLK },
{ 0x8086, 0x0c5f, 0xffff, 0, "Atom Processor S1200 UART",
	0x10 | PCI_NO_MSI },
{ 0x8086, 0x0f0a, 0xffff, 0, "Intel ValleyView LPIO1 HSUART#1", 0x10,
	24 * DEFAULT_RCLK, 2 },
{ 0x8086, 0x0f0c, 0xffff, 0, "Intel ValleyView LPIO1 HSUART#2", 0x10,
	24 * DEFAULT_RCLK, 2 },
{ 0x8086, 0x108f, 0xffff, 0, "Intel AMT - SOL", 0x10 },
{ 0x8086, 0x19d8, 0xffff, 0, "Intel Denverton UART", 0x10 },
{ 0x8086, 0x1c3d, 0xffff, 0, "Intel AMT - KT Controller", 0x10 },
{ 0x8086, 0x1d3d, 0xffff, 0, "Intel C600/X79 Series Chipset KT Controller",
	0x10 },
{ 0x8086, 0x1e3d, 0xffff, 0, "Intel Panther Point KT Controller", 0x10 },
{ 0x8086, 0x228a, 0xffff, 0, "Intel Cherryview SIO HSUART#1", 0x10,
	24 * DEFAULT_RCLK, 2 },
{ 0x8086, 0x228c, 0xffff, 0, "Intel Cherryview SIO HSUART#2", 0x10,
	24 * DEFAULT_RCLK, 2 },
{ 0x8086, 0x2a07, 0xffff, 0, "Intel AMT - PM965/GM965 KT Controller", 0x10 },
{ 0x8086, 0x2a47, 0xffff, 0, "Mobile 4 Series Chipset KT Controller", 0x10 },
{ 0x8086, 0x2e17, 0xffff, 0, "4 Series Chipset Serial KT Controller", 0x10 },
{ 0x8086, 0x31bc, 0xffff, 0, "Intel Gemini Lake SIO/LPSS UART 0", 0x10,
	24 * DEFAULT_RCLK, 2 },
{ 0x8086, 0x31be, 0xffff, 0, "Intel Gemini Lake SIO/LPSS UART 1", 0x10,
	24 * DEFAULT_RCLK, 2 },
{ 0x8086, 0x31c0, 0xffff, 0, "Intel Gemini Lake SIO/LPSS UART 2", 0x10,
	24 * DEFAULT_RCLK, 2 },
{ 0x8086, 0x31ee, 0xffff, 0, "Intel Gemini Lake SIO/LPSS UART 3", 0x10,
	24 * DEFAULT_RCLK, 2 },
{ 0x8086, 0x3b67, 0xffff, 0, "5 Series/3400 Series Chipset KT Controller",
	0x10 },
{ 0x8086, 0x5abc, 0xffff, 0, "Intel Apollo Lake SIO/LPSS UART 0", 0x10,
	24 * DEFAULT_RCLK, 2 },
{ 0x8086, 0x5abe, 0xffff, 0, "Intel Apollo Lake SIO/LPSS UART 1", 0x10,
	24 * DEFAULT_RCLK, 2 },
{ 0x8086, 0x5ac0, 0xffff, 0, "Intel Apollo Lake SIO/LPSS UART 2", 0x10,
	24 * DEFAULT_RCLK, 2 },
{ 0x8086, 0x5aee, 0xffff, 0, "Intel Apollo Lake SIO/LPSS UART 3", 0x10,
	24 * DEFAULT_RCLK, 2 },
{ 0x8086, 0x8811, 0xffff, 0, "Intel EG20T Serial Port 0", 0x10 },
{ 0x8086, 0x8812, 0xffff, 0, "Intel EG20T Serial Port 1", 0x10 },
{ 0x8086, 0x8813, 0xffff, 0, "Intel EG20T Serial Port 2", 0x10 },
{ 0x8086, 0x8814, 0xffff, 0, "Intel EG20T Serial Port 3", 0x10 },
{ 0x8086, 0x8c3d, 0xffff, 0, "Intel Lynx Point KT Controller", 0x10 },
{ 0x8086, 0x8cbd, 0xffff, 0, "Intel Wildcat Point KT Controller", 0x10 },
{ 0x8086, 0x8d3d, 0xffff, 0,
	"Intel Corporation C610/X99 series chipset KT Controller", 0x10 },
{ 0x8086, 0x9c3d, 0xffff, 0, "Intel Lynx Point-LP HECI KT", 0x10 },
{ 0x8086, 0xa13d, 0xffff, 0,
	"100 Series/C230 Series Chipset Family KT Redirection",
	0x10 | PCI_NO_MSI },
{ 0x9710, 0x9820, 0x1000, 1, "NetMos NM9820 Serial Port", 0x10 },
{ 0x9710, 0x9835, 0x1000, 1, "NetMos NM9835 Serial Port", 0x10 },
{ 0x9710, 0x9865, 0xa000, 0x1000, "NetMos NM9865 Serial Port", 0x10 },
{ 0x9710, 0x9900, 0xa000, 0x1000,
	"MosChip MCS9900 PCIe to Peripheral Controller", 0x10 },
{ 0x9710, 0x9901, 0xa000, 0x1000,
	"MosChip MCS9901 PCIe to Peripheral Controller", 0x10 },
{ 0x9710, 0x9904, 0xa000, 0x1000,
	"MosChip MCS9904 PCIe to Peripheral Controller", 0x10 },
{ 0x9710, 0x9922, 0xa000, 0x1000,
	"MosChip MCS9922 PCIe to Peripheral Controller", 0x10 },
{ 0xdeaf, 0x9051, 0xffff, 0, "Middle Digital PC Weasel Serial Port", 0x10 },
{ 0xffff, 0, 0xffff, 0, NULL, 0, 0}
};

const static struct pci_id *
uart_pci_match(device_t dev, const struct pci_id *id)
{
	uint16_t device, subdev, subven, vendor;

	vendor = pci_get_vendor(dev);
	device = pci_get_device(dev);
	while (id->vendor != 0xffff &&
	    (id->vendor != vendor || id->device != device))
		id++;
	if (id->vendor == 0xffff)
		return (NULL);
	if (id->subven == 0xffff)
		return (id);
	subven = pci_get_subvendor(dev);
	subdev = pci_get_subdevice(dev);
	while (id->vendor == vendor && id->device == device &&
	    (id->subven != subven || id->subdev != subdev))
		id++;
	return ((id->vendor == vendor && id->device == device) ? id : NULL);
}

extern SLIST_HEAD(uart_devinfo_list, uart_devinfo) uart_sysdevs;

/* PCI vendor/device pairs of devices guaranteed to be unique on a system. */
static const struct pci_unique_id pci_unique_devices[] = {
{ 0x1d0f, 0x8250 }	/* Amazon PCI serial device */
};

/* Match a UART to a console if it's a PCI device known to be unique. */
static void
uart_pci_unique_console_match(device_t dev)
{
	struct uart_softc *sc;
	struct uart_devinfo * sysdev;
	const struct pci_unique_id * id;
	uint16_t vendor, device;

	sc = device_get_softc(dev);
	vendor = pci_get_vendor(dev);
	device = pci_get_device(dev);

	/* Is this a device known to exist only once in a system? */
	for (id = pci_unique_devices; ; id++) {
		if (id == &pci_unique_devices[nitems(pci_unique_devices)])
			return;
		if (id->vendor == vendor && id->device == device)
			break;
	}

	/* If it matches a console, it must be the same device. */
	SLIST_FOREACH(sysdev, &uart_sysdevs, next) {
		if (sysdev->pci_info.vendor == vendor &&
		    sysdev->pci_info.device == device) {
			sc->sc_sysdev = sysdev;
			sysdev->bas.rclk = sc->sc_bas.rclk;
		}
	}
}

static int
uart_pci_probe(device_t dev)
{
	struct uart_softc *sc;
	const struct pci_id *id;
	struct pci_id cid = {
		.regshft = 0,
		.rclk = 0,
		.rid = 0x10 | PCI_NO_MSI,
		.desc = "Generic SimpleComm PCI device",
	};
	int result;

	sc = device_get_softc(dev);

	id = uart_pci_match(dev, pci_ns8250_ids);
	if (id != NULL) {
		sc->sc_class = &uart_ns8250_class;
		goto match;
	}
	if (pci_get_class(dev) == PCIC_SIMPLECOMM &&
	    pci_get_subclass(dev) == PCIS_SIMPLECOMM_UART &&
	    pci_get_progif(dev) < PCIP_SIMPLECOMM_UART_16550A) {
		/* XXX rclk what to do */
		id = &cid;
		sc->sc_class = &uart_ns8250_class;
		goto match;
	}
	/* Add checks for non-ns8250 IDs here. */
	return (ENXIO);

 match:
	result = uart_bus_probe(dev, id->regshft, 0, id->rclk,
	    id->rid & PCI_RID_MASK, 0, 0);
	/* Bail out on error. */
	if (result > 0)
		return (result);
	/*
	 * If we haven't already matched this to a console, check if it's a
	 * PCI device which is known to only exist once in any given system
	 * and we can match it that way.
	 */
	if (sc->sc_sysdev == NULL)
		uart_pci_unique_console_match(dev);
	/* Set/override the device description. */
	if (id->desc)
		device_set_desc(dev, id->desc);
	return (result);
}

static int
uart_pci_attach(device_t dev)
{
	struct uart_softc *sc;
	const struct pci_id *id;
	int count;

	sc = device_get_softc(dev);

	/*
	 * Use MSI in preference to legacy IRQ if available. However, experience
	 * suggests this is only reliable when one MSI vector is advertised.
	 */
	id = uart_pci_match(dev, pci_ns8250_ids);
	if ((id == NULL || (id->rid & PCI_NO_MSI) == 0) &&
	    pci_msi_count(dev) == 1) {
		count = 1;
		if (pci_alloc_msi(dev, &count) == 0) {
			sc->sc_irid = 1;
			device_printf(dev, "Using %d MSI message\n", count);
		}
	}

	return (uart_bus_attach(dev));
}

static int
uart_pci_detach(device_t dev)
{
	struct uart_softc *sc;

	sc = device_get_softc(dev);

	if (sc->sc_irid != 0)
		pci_release_msi(dev);

	return (uart_bus_detach(dev));
}

DRIVER_MODULE(uart, pci, uart_pci_driver, NULL, NULL);
