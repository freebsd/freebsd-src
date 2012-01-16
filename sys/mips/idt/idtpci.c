/* $NetBSD: idtpci.c,v 1.1 2007/03/20 08:52:02 dyoung Exp $ */

/*-
 * Copyright (c) 2007 David Young.
 * Copyright (c) 2007 Oleskandr Tymoshenko.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or
 * without modification, are permitted provided that the following
 * conditions are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 */
/*-
 * Copyright (c) 2006 Itronix Inc.
 * All rights reserved.
 *
 * Written by Garrett D'Amore for Itronix Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of Itronix Inc. may not be used to endorse
 *    or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY ITRONIX INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL ITRONIX INC. BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */ 

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>

#include <sys/bus.h>
#include <sys/interrupt.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/rman.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_extern.h>

#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/pmap.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>

#include <dev/pci/pcib_private.h>
#include "pcib_if.h"

#include <mips/idt/idtreg.h>

#ifdef IDTPCI_DEBUG
int idtpci_debug = 1;
#define	IDTPCI_DPRINTF(__fmt, ...)		\
do {						\
	if (idtpci_debug)			\
		printf((__fmt), __VA_ARGS__);	\
} while (/*CONSTCOND*/0)
#else /* !IDTPCI_DEBUG */
#define	IDTPCI_DPRINTF(__fmt, ...)	do { } while (/*CONSTCOND*/0)
#endif /* IDTPCI_DEBUG */

#define	IDTPCI_TAG_BUS_MASK		0x007f0000
#define	IDTPCI_TAG_DEVICE_MASK		0x00007800
#define	IDTPCI_TAG_FUNCTION_MASK	0x00000300
#define	IDTPCI_TAG_REGISTER_MASK	0x0000007c

#define	IDTPCI_MAX_DEVICE

#define REG_READ(o) *((volatile uint32_t *)MIPS_PHYS_TO_KSEG1(IDT_BASE_PCI + (o)))
#define REG_WRITE(o,v) (REG_READ(o)) = (v)

unsigned int korina_fixup[24] = { 
	0x00000157, 0x00000000, 0x00003c04, 0x00000008, 0x18800001, 0x18000001,
	0x48000008, 0x00000000, 0x00000000, 0x00000000, 0x011d0214, 0x00000000,
	0x00000000, 0x00000000, 0x38080101, 0x00008080, 0x00000d6e, 0x00000000,
	0x00000051, 0x00000000, 0x00000055, 0x18000000, 0x00000000, 0x00000000 
};

struct idtpci_softc {
	device_t		sc_dev;

	int			sc_busno;
	struct rman		sc_mem_rman[2];
	struct rman		sc_io_rman[2];
	struct rman		sc_irq_rman;
};

static uint32_t
idtpci_make_addr(int bus, int slot, int func, int reg)
{

	return 0x80000000 | (bus << 16) | (slot << 11) | (func << 8) | reg;
}

static int
idtpci_probe(device_t dev)
{

	return (0);
}

static int
idtpci_attach(device_t dev)
{
	int busno = 0;
	struct idtpci_softc *sc = device_get_softc(dev);
	unsigned int pci_data, force_endianess = 0;
	int		i;
	bus_addr_t	addr;

	sc->sc_dev = dev;
	sc->sc_busno = busno;

	/* TODO: Check for host mode */

	/* Enabled PCI, IG mode, EAP mode */
	REG_WRITE(IDT_PCI_CNTL, IDT_PCI_CNTL_IGM | IDT_PCI_CNTL_EAP |
	    IDT_PCI_CNTL_EN);
	/* Wait while "Reset in progress bit" set */
	while(1) {
		pci_data = REG_READ(IDT_PCI_STATUS);
		if((pci_data & IDT_PCI_STATUS_RIP) == 0)
			break;
	}

	/* Reset status register */
	REG_WRITE(IDT_PCI_STATUS, 0);
	/* Mask interrupts related to status register */
	REG_WRITE(IDT_PCI_STATUS_MASK, 0xffffffff);

	/* Disable PCI decoupled access */
	REG_WRITE(IDT_PCI_DAC, 0);
	/* Zero status and mask DA interrupts */
	REG_WRITE(IDT_PCI_DAS, 0);
	REG_WRITE(IDT_PCI_DASM, 0x7f);

	/* Init PCI messaging unit */
	/* Disable messaging interrupts */
	REG_WRITE(IDT_PCI_IIC, 0);
	REG_WRITE(IDT_PCI_IIM, 0xffffffff);
	REG_WRITE(IDT_PCI_OIC, 0);
	REG_WRITE(IDT_PCI_OIM, 0);

#ifdef	__MIPSEB__
	force_endianess = IDT_PCI_LBA_FE;
#endif

	/* LBA0 -- memory window */
	REG_WRITE(IDT_PCI_LBA0, IDT_PCIMEM0_BASE);
	REG_WRITE(IDT_PCI_LBA0_MAP, IDT_PCIMEM0_BASE);
	REG_WRITE(IDT_PCI_LBA0_CNTL, IDT_PCI_LBA_SIZE_16MB | force_endianess);
	pci_data = REG_READ(IDT_PCI_LBA0_CNTL); 

	/* LBA1 -- memory window */
	REG_WRITE(IDT_PCI_LBA1, IDT_PCIMEM1_BASE);
	REG_WRITE(IDT_PCI_LBA1_MAP, IDT_PCIMEM1_BASE);
	REG_WRITE(IDT_PCI_LBA1_CNTL, IDT_PCI_LBA_SIZE_256MB | force_endianess);
	pci_data = REG_READ(IDT_PCI_LBA1_CNTL); 

	/* LBA2 -- IO window */
	REG_WRITE(IDT_PCI_LBA2, IDT_PCIMEM2_BASE);
	REG_WRITE(IDT_PCI_LBA2_MAP, IDT_PCIMEM2_BASE);
	REG_WRITE(IDT_PCI_LBA2_CNTL, IDT_PCI_LBA_SIZE_4MB | IDT_PCI_LBA_MSI |
	    force_endianess);
	pci_data = REG_READ(IDT_PCI_LBA2_CNTL); 

	/* LBA3 -- IO window */
	REG_WRITE(IDT_PCI_LBA3, IDT_PCIMEM3_BASE);
	REG_WRITE(IDT_PCI_LBA3_MAP, IDT_PCIMEM3_BASE);
	REG_WRITE(IDT_PCI_LBA3_CNTL, IDT_PCI_LBA_SIZE_1MB | IDT_PCI_LBA_MSI |
	    force_endianess);
	pci_data = REG_READ(IDT_PCI_LBA3_CNTL); 


	pci_data = REG_READ(IDT_PCI_CNTL) & ~IDT_PCI_CNTL_TNR; 
	REG_WRITE(IDT_PCI_CNTL, pci_data);
	pci_data = REG_READ(IDT_PCI_CNTL);

	/* Rewrite Target Control register with default values */
	REG_WRITE(IDT_PCI_TC, (IDT_PCI_TC_DTIMER << 8) | IDT_PCI_TC_RTIMER);

	/* Perform Korina fixup */
	addr = idtpci_make_addr(0, 0, 0, 4);
	for (i = 0; i < 24; i++) {

		REG_WRITE(IDT_PCI_CFG_ADDR, addr);
		REG_WRITE(IDT_PCI_CFG_DATA, korina_fixup[i]);
		__asm__ volatile ("sync");

		REG_WRITE(IDT_PCI_CFG_ADDR, 0);
		REG_WRITE(IDT_PCI_CFG_DATA, 0);
		addr += 4;
	}

	/* Use KSEG1 to access IO ports for it is uncached */
	sc->sc_io_rman[0].rm_type = RMAN_ARRAY;
	sc->sc_io_rman[0].rm_descr = "IDTPCI I/O Ports window 1";
	if (rman_init(&sc->sc_io_rman[0]) != 0 ||
	  rman_manage_region(&sc->sc_io_rman[0], 
	      IDT_PCIMEM2_BASE, IDT_PCIMEM2_BASE + IDT_PCIMEM2_SIZE - 1) != 0) {
		panic("idtpci_attach: failed to set up I/O rman");
	}

	sc->sc_io_rman[1].rm_type = RMAN_ARRAY;
	sc->sc_io_rman[1].rm_descr = "IDTPCI I/O Ports window 2";
	if (rman_init(&sc->sc_io_rman[1]) != 0 ||
	  rman_manage_region(&sc->sc_io_rman[1], 
	      IDT_PCIMEM3_BASE, IDT_PCIMEM3_BASE + IDT_PCIMEM3_SIZE - 1) != 0) {
		panic("idtpci_attach: failed to set up I/O rman");
	}

	/* Use KSEG1 to access PCI memory for it is uncached */
	sc->sc_mem_rman[0].rm_type = RMAN_ARRAY;
	sc->sc_mem_rman[0].rm_descr = "IDTPCI PCI Memory window 1";
	if (rman_init(&sc->sc_mem_rman[0]) != 0 ||
	    rman_manage_region(&sc->sc_mem_rman[0], 
	    IDT_PCIMEM0_BASE, IDT_PCIMEM0_BASE + IDT_PCIMEM0_SIZE) != 0) {
		panic("idtpci_attach: failed to set up memory rman");
	}

	sc->sc_mem_rman[1].rm_type = RMAN_ARRAY;
	sc->sc_mem_rman[1].rm_descr = "IDTPCI PCI Memory window 2";
	if (rman_init(&sc->sc_mem_rman[1]) != 0 ||
	    rman_manage_region(&sc->sc_mem_rman[1], 
	    IDT_PCIMEM1_BASE, IDT_PCIMEM1_BASE + IDT_PCIMEM1_SIZE) != 0) {
		panic("idtpci_attach: failed to set up memory rman");
	}

	sc->sc_irq_rman.rm_type = RMAN_ARRAY;
	sc->sc_irq_rman.rm_descr = "IDTPCI PCI IRQs";
	if (rman_init(&sc->sc_irq_rman) != 0 ||
	    rman_manage_region(&sc->sc_irq_rman, PCI_IRQ_BASE, 
	        PCI_IRQ_END) != 0)
		panic("idtpci_attach: failed to set up IRQ rman");

	device_add_child(dev, "pci", busno);
	return (bus_generic_attach(dev));
}

static int
idtpci_maxslots(device_t dev)
{

	return (PCI_SLOTMAX);
}

static uint32_t
idtpci_read_config(device_t dev, u_int bus, u_int slot, u_int func, u_int reg,
    int bytes)
{
	uint32_t data;
	uint32_t shift, mask;
	bus_addr_t addr;

	IDTPCI_DPRINTF("%s: tag (%x, %x, %x) reg %d(%d)\n", __func__, 
			bus, slot, func, reg, bytes);

	addr = idtpci_make_addr(bus, slot, func, reg);

	REG_WRITE(IDT_PCI_CFG_ADDR, addr);
	data = REG_READ(IDT_PCI_CFG_DATA);

	switch (reg % 4) {
	case 3:
		shift = 24;
		break;
	case 2:
		shift = 16;
		break;
	case 1:
		shift = 8;
		break;
	default:
		shift = 0;
		break;
	}	

	switch (bytes) {
	case 1:
		mask = 0xff;
		data = (data >> shift) & mask;
		break;
	case 2:
		mask = 0xffff;
		if (reg % 4 == 0)
			data = data & mask;
		else
			data = (data >> 16) & mask;
		break;
	case 4:
		break;
	default:
		panic("%s: wrong bytes count", __func__);
		break;
	}

	__asm__ volatile ("sync");
 	IDTPCI_DPRINTF("%s: read 0x%x\n", __func__, data);

	return (data);
}

static void
idtpci_write_config(device_t dev, u_int bus, u_int slot, u_int func, u_int reg,
    uint32_t data, int bytes)
{
	bus_addr_t addr;
	uint32_t reg_data;
	uint32_t shift, mask;

	IDTPCI_DPRINTF("%s: tag (%x, %x, %x) reg %d(%d) data %08x\n", __func__, 
			bus, slot, func, reg, bytes, data);

	if (bytes != 4) {
		reg_data = idtpci_read_config(dev, bus, slot, func, reg, 4);

		switch (reg % 4) {
		case 3:
			shift = 24;
			break;
		case 2:
			shift = 16;
			break;
		case 1:
			shift = 8;
			break;
		default:
			shift = 0;
			break;
		}	

		switch (bytes) {
		case 1:
			mask = 0xff;
			data = (reg_data & ~ (mask << shift)) | (data << shift);
			break;
		case 2:
			mask = 0xffff;
			if (reg % 4 == 0)
				data = (reg_data & ~mask) | data;
			else
				data = (reg_data & ~ (mask << shift)) | 
				    (data << shift);
			break;
		case 4:
			break;
		default:
			panic("%s: wrong bytes count", __func__);
			break;
		}
	}

	addr = idtpci_make_addr(bus, slot, func, reg);


	REG_WRITE(IDT_PCI_CFG_ADDR, addr);
	REG_WRITE(IDT_PCI_CFG_DATA, data);
	__asm__ volatile ("sync");

	REG_WRITE(IDT_PCI_CFG_ADDR, 0);
	REG_WRITE(IDT_PCI_CFG_DATA, 0);
}

static int
idtpci_route_interrupt(device_t pcib, device_t device, int pin)
{
	static int idt_pci_table[2][12] =
	{
		{ 0, 0, 2, 3, 2, 3, 0, 0, 0, 0, 0, 1 },
		{ 0, 0, 1, 3, 0, 2, 1, 3, 0, 2, 1, 3 }
	};
	int dev, bus, irq;
	
	dev = pci_get_slot(device);
	bus = pci_get_bus(device);
	if (bootverbose)
		device_printf(pcib, "routing pin %d for %s\n", pin,
		    device_get_nameunit(device));
	if (bus >= 0 && bus <= 1 &&
	    dev >= 0 && dev <= 11) {
		irq = IP_IRQ(6, idt_pci_table[bus][dev] + 4);
		if (bootverbose)
			printf("idtpci: %d/%d/%d -> IRQ%d\n",
			    pci_get_bus(device), dev, pci_get_function(device),
			    irq);
		return (irq);
	} else
		printf("idtpci: no mapping for %d/%d/%d\n",
			pci_get_bus(device), dev, pci_get_function(device));

	return (-1);
}

static int
idtpci_read_ivar(device_t dev, device_t child, int which, uintptr_t *result)
{
	struct idtpci_softc *sc = device_get_softc(dev);

	switch (which) {
	case PCIB_IVAR_DOMAIN:
		*result = 0;
		return (0);
	case PCIB_IVAR_BUS:
		*result = sc->sc_busno;
		return (0);
	}

	return (ENOENT);
}

static int
idtpci_write_ivar(device_t dev, device_t child, int which, uintptr_t result)
{
	struct idtpci_softc * sc = device_get_softc(dev);

	switch (which) {
	case PCIB_IVAR_BUS:
		sc->sc_busno = result;
		return (0);
	}
	return (ENOENT);
}

static struct resource *
idtpci_alloc_resource(device_t bus, device_t child, int type, int *rid,
    u_long start, u_long end, u_long count, u_int flags)
{

	struct idtpci_softc *sc = device_get_softc(bus);	
	struct resource *rv = NULL;
	struct rman *rm1, *rm2;

	switch (type) {
	case SYS_RES_IRQ:
		rm1 = &sc->sc_irq_rman;
		rm2 = NULL;
		break;
	case SYS_RES_MEMORY:
		rm1 = &sc->sc_mem_rman[0];
		rm2 = &sc->sc_mem_rman[1];
		break;
	case SYS_RES_IOPORT:
		rm1 = &sc->sc_io_rman[0];
		rm2 = &sc->sc_io_rman[1];
		break;
	default:
		return (NULL);
	}

	rv = rman_reserve_resource(rm1, start, end, count, flags, child);

	/* Try second window if it exists */
	if ((rv == NULL) && (rm2 != NULL))
		rv = rman_reserve_resource(rm2, start, end, count, flags, 
		    child);

	if (rv == NULL)
		return (NULL);

	rman_set_rid(rv, *rid);

	if (flags & RF_ACTIVE) {
		if (bus_activate_resource(child, type, *rid, rv)) {
			rman_release_resource(rv);
			return (NULL);
		}
	} 

	return (rv);
}

static int
idtpci_teardown_intr(device_t dev, device_t child, struct resource *res,
    void *cookie)
{

	return (intr_event_remove_handler(cookie));
}

static device_method_t idtpci_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		idtpci_probe),
	DEVMETHOD(device_attach,	idtpci_attach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	DEVMETHOD(device_suspend,	bus_generic_suspend),
	DEVMETHOD(device_resume,	bus_generic_resume),

	/* Bus interface */
	DEVMETHOD(bus_read_ivar,	idtpci_read_ivar),
	DEVMETHOD(bus_write_ivar,	idtpci_write_ivar),
	DEVMETHOD(bus_alloc_resource,	idtpci_alloc_resource),
	DEVMETHOD(bus_release_resource,	bus_generic_release_resource),
	DEVMETHOD(bus_activate_resource, bus_generic_activate_resource),
	DEVMETHOD(bus_deactivate_resource, bus_generic_deactivate_resource),
	DEVMETHOD(bus_setup_intr,	bus_generic_setup_intr),
	DEVMETHOD(bus_teardown_intr,	idtpci_teardown_intr),

	/* pcib interface */
	DEVMETHOD(pcib_maxslots,	idtpci_maxslots),
	DEVMETHOD(pcib_read_config,	idtpci_read_config),
	DEVMETHOD(pcib_write_config,	idtpci_write_config),
	DEVMETHOD(pcib_route_interrupt,	idtpci_route_interrupt),

	DEVMETHOD_END
};

static driver_t idtpci_driver = {
	"pcib",
	idtpci_methods,
	sizeof(struct idtpci_softc),
};

static devclass_t idtpci_devclass;

DRIVER_MODULE(idtpci, obio, idtpci_driver, idtpci_devclass, 0, 0);
