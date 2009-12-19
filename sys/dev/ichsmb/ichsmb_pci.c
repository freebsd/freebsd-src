/*-
 * ichsmb_pci.c
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 * Copyright (c) 2000 Whistle Communications, Inc.
 * All rights reserved.
 * Author: Archie Cobbs <archie@freebsd.org>
 * 
 * Subject to the following obligations and disclaimer of warranty, use and
 * redistribution of this software, in source or object code forms, with or
 * without modifications are expressly permitted by Whistle Communications;
 * provided, however, that:
 * 1. Any and all reproductions of the source or object code must include the
 *    copyright notice above and the following disclaimer of warranties; and
 * 2. No rights are granted, in any manner or form, to use Whistle
 *    Communications, Inc. trademarks, including the mark "WHISTLE
 *    COMMUNICATIONS" on advertising, endorsements, or otherwise except as
 *    such appears in the above copyright notice or in the software.
 * 
 * THIS SOFTWARE IS BEING PROVIDED BY WHISTLE COMMUNICATIONS "AS IS", AND
 * TO THE MAXIMUM EXTENT PERMITTED BY LAW, WHISTLE COMMUNICATIONS MAKES NO
 * REPRESENTATIONS OR WARRANTIES, EXPRESS OR IMPLIED, REGARDING THIS SOFTWARE,
 * INCLUDING WITHOUT LIMITATION, ANY AND ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, OR NON-INFRINGEMENT.
 * WHISTLE COMMUNICATIONS DOES NOT WARRANT, GUARANTEE, OR MAKE ANY
 * REPRESENTATIONS REGARDING THE USE OF, OR THE RESULTS OF THE USE OF THIS
 * SOFTWARE IN TERMS OF ITS CORRECTNESS, ACCURACY, RELIABILITY OR OTHERWISE.
 * IN NO EVENT SHALL WHISTLE COMMUNICATIONS BE LIABLE FOR ANY DAMAGES
 * RESULTING FROM OR ARISING OUT OF ANY USE OF THIS SOFTWARE, INCLUDING
 * WITHOUT LIMITATION, ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * PUNITIVE, OR CONSEQUENTIAL DAMAGES, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES, LOSS OF USE, DATA OR PROFITS, HOWEVER CAUSED AND UNDER ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF WHISTLE COMMUNICATIONS IS ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * Support for the SMBus controller logical device which is part of the
 * Intel 81801AA/AB/BA/CA/DC/EB (ICH/ICH[02345]) I/O controller hub chips.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/errno.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/syslog.h>
#include <sys/bus.h>

#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>

#include <dev/smbus/smbconf.h>

#include <dev/ichsmb/ichsmb_var.h>
#include <dev/ichsmb/ichsmb_reg.h>

/* PCI unique identifiers */
#define ID_82801AA			0x24138086
#define ID_82801AB			0x24238086
#define ID_82801BA			0x24438086
#define ID_82801CA			0x24838086
#define ID_82801DC			0x24C38086
#define ID_82801EB			0x24D38086
#define ID_82801FB			0x266A8086
#define ID_82801GB			0x27da8086
#define ID_82801H			0x283e8086
#define ID_82801I			0x29308086
#define ID_82801JI			0x3a308086
#define ID_PCH				0x3b308086
#define ID_6300ESB			0x25a48086
#define	ID_631xESB			0x269b8086

#define PCIS_SERIALBUS_SMBUS_PROGIF	0x00

/* Internal functions */
static int	ichsmb_pci_probe(device_t dev);
static int	ichsmb_pci_attach(device_t dev);
/*Use generic one for now*/
#if 0
static int	ichsmb_pci_detach(device_t dev);
#endif

/* Device methods */
static device_method_t ichsmb_pci_methods[] = {
	/* Device interface */
        DEVMETHOD(device_probe, ichsmb_pci_probe),
        DEVMETHOD(device_attach, ichsmb_pci_attach),
        DEVMETHOD(device_detach, ichsmb_detach),

	/* Bus methods */
        DEVMETHOD(bus_print_child, bus_generic_print_child),

	/* SMBus methods */
        DEVMETHOD(smbus_callback, ichsmb_callback),
        DEVMETHOD(smbus_quick, ichsmb_quick),
        DEVMETHOD(smbus_sendb, ichsmb_sendb),
        DEVMETHOD(smbus_recvb, ichsmb_recvb),
        DEVMETHOD(smbus_writeb, ichsmb_writeb),
        DEVMETHOD(smbus_writew, ichsmb_writew),
        DEVMETHOD(smbus_readb, ichsmb_readb),
        DEVMETHOD(smbus_readw, ichsmb_readw),
        DEVMETHOD(smbus_pcall, ichsmb_pcall),
        DEVMETHOD(smbus_bwrite, ichsmb_bwrite),
        DEVMETHOD(smbus_bread, ichsmb_bread),
	{ 0, 0 }
};

static driver_t ichsmb_pci_driver = {
	"ichsmb",
	ichsmb_pci_methods,
	sizeof(struct ichsmb_softc)
};

static devclass_t ichsmb_pci_devclass;

DRIVER_MODULE(ichsmb, pci, ichsmb_pci_driver, ichsmb_pci_devclass, 0, 0);

static int
ichsmb_pci_probe(device_t dev)
{
	/* Check PCI identifier */
	switch (pci_get_devid(dev)) {
	case ID_82801AA:
		device_set_desc(dev, "Intel 82801AA (ICH) SMBus controller");
		break;
	case ID_82801AB:
		device_set_desc(dev, "Intel 82801AB (ICH0) SMBus controller");
		break;
	case ID_82801BA:
		device_set_desc(dev, "Intel 82801BA (ICH2) SMBus controller");
		break;
	case ID_82801CA:
		device_set_desc(dev, "Intel 82801CA (ICH3) SMBus controller");
		break;
	case ID_82801DC:
		device_set_desc(dev, "Intel 82801DC (ICH4) SMBus controller");
		break;
	case ID_82801EB:
		device_set_desc(dev, "Intel 82801EB (ICH5) SMBus controller");
		break;
	case ID_82801FB:
		device_set_desc(dev, "Intel 82801FB (ICH6) SMBus controller");
		break;
	case ID_82801GB:
		device_set_desc(dev, "Intel 82801GB (ICH7) SMBus controller");
		break;
	case ID_82801H:
		device_set_desc(dev, "Intel 82801H (ICH8) SMBus controller");
		break;
	case ID_82801I:
		device_set_desc(dev, "Intel 82801I (ICH9) SMBus controller");
		break;
	case ID_82801JI:
		device_set_desc(dev, "Intel 82801JI (ICH10) SMBus controller");
		break;
	case ID_PCH:
		device_set_desc(dev, "Intel PCH SMBus controller");
		break;
	case ID_6300ESB:
		device_set_desc(dev, "Intel 6300ESB (ICH) SMBus controller");
		break;
	case ID_631xESB:
		device_set_desc(dev, "Intel 631xESB/6321ESB (ESB2) SMBus controller");
		break;
	default:
		if (pci_get_class(dev) == PCIC_SERIALBUS
		    && pci_get_subclass(dev) == PCIS_SERIALBUS_SMBUS
		    && pci_get_progif(dev) == PCIS_SERIALBUS_SMBUS_PROGIF) {
			device_set_desc(dev, "SMBus controller");
			return (BUS_PROBE_DEFAULT); /* XXX */
		}
		return (ENXIO);
	}

	/* Done */
	return (ichsmb_probe(dev));
}

static int
ichsmb_pci_attach(device_t dev)
{
	const sc_p sc = device_get_softc(dev);
	int error;

	/* Initialize private state */
	bzero(sc, sizeof(*sc));
	sc->ich_cmd = -1;
	sc->dev = dev;

	/* Allocate an I/O range */
	sc->io_rid = ICH_SMB_BASE;
	sc->io_res = bus_alloc_resource(dev, SYS_RES_IOPORT,
	    &sc->io_rid, 0, ~0, 16, RF_ACTIVE);
	if (sc->io_res == NULL)
		sc->io_res = bus_alloc_resource(dev, SYS_RES_IOPORT,
		    &sc->io_rid, 0, ~0, 32, RF_ACTIVE);
	if (sc->io_res == NULL) {
		device_printf(dev, "can't map I/O\n");
		error = ENXIO;
		goto fail;
	}
	sc->io_bst = rman_get_bustag(sc->io_res);
	sc->io_bsh = rman_get_bushandle(sc->io_res);

	/* Allocate interrupt */
	sc->irq_rid = 0;
	sc->irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ,
	    &sc->irq_rid, RF_ACTIVE | RF_SHAREABLE);
	if (sc->irq_res == NULL) {
		device_printf(dev, "can't get IRQ\n");
		error = ENXIO;
		goto fail;
	}

	/* Enable device */
	pci_write_config(dev, ICH_HOSTC, ICH_HOSTC_HST_EN, 1);

	/* Done */
	error = ichsmb_attach(dev);
	if (error)
		goto fail;
	return (0);

fail:
	/* Attach failed, release resources */
	ichsmb_release_resources(sc);
	return (error);
}


MODULE_DEPEND(ichsmb, pci, 1, 1, 1);
MODULE_DEPEND(ichsmb, smbus, SMBUS_MINVER, SMBUS_PREFVER, SMBUS_MAXVER);
MODULE_VERSION(ichsmb, 1);
