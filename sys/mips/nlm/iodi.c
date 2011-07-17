/*-
 * Copyright 2003-2011 Netlogic Microsystems (Netlogic). All rights
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY Netlogic Microsystems ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL NETLOGIC OR CONTRIBUTORS BE 
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 * NETLOGIC_BSD */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#define __RMAN_RESOURCE_VISIBLE
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/interrupt.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/reboot.h>
#include <sys/rman.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>

#include <machine/cpu.h>
#include <machine/bus.h>
#include <machine/intr_machdep.h>

#include <mips/nlm/hal/mmio.h>
#include <mips/nlm/hal/iomap.h>
#include <mips/nlm/hal/pic.h>
#include <mips/nlm/hal/uart.h>
#include <mips/nlm/hal/cop2.h>
#include <mips/nlm/hal/fmn.h>

#include <mips/nlm/msgring.h>
#include <mips/nlm/xlp.h>
#include <mips/nlm/board.h>

extern void iodi_activateirqs(void);

extern bus_space_tag_t uart_bus_space_mem;

static struct resource *iodi_alloc_resource(device_t, device_t, int, int *,
        u_long, u_long, u_long, u_int);

static int iodi_activate_resource(device_t, device_t, int, int,
        struct resource *);
struct iodi_softc *iodi_softc; /* There can be only one. */

static int
iodi_setup_intr(device_t dev, device_t child,
    struct resource *ires,  int flags, driver_filter_t *filt,
    driver_intr_t *intr, void *arg, void **cookiep)
{
	const char *name = device_get_name(child);
	int unit = device_get_unit(child);

	if (strcmp(name, "uart") == 0) {
		/* Note: in xlp, all pic interrupts are level triggered */
		nlm_pic_write_irt_id(xlp_pic_base, XLP_PIC_IRT_UART0_INDEX, 1, 0, 
		    xlp_irt_to_irq(XLP_PIC_IRT_UART0_INDEX), 0, 0, 0x1);

		cpu_establish_hardintr("uart", filt, intr, arg,
		    xlp_irt_to_irq(XLP_PIC_IRT_UART0_INDEX), flags, cookiep);
	} else if (strcmp(name, "ehci") == 0) {
		if (unit == 0) {	
			nlm_pic_write_irt_id(xlp_pic_base, XLP_PIC_IRT_EHCI0_INDEX, 1, 0,
				xlp_irt_to_irq(XLP_PIC_IRT_EHCI0_INDEX), 0, 0, 0x1);

			cpu_establish_hardintr("ehci0", filt, intr, arg,
				xlp_irt_to_irq(XLP_PIC_IRT_EHCI0_INDEX), flags, cookiep);
		} else if (unit == 1) { 
			nlm_pic_write_irt_id(xlp_pic_base, XLP_PIC_IRT_EHCI1_INDEX, 1, 0,
				xlp_irt_to_irq(XLP_PIC_IRT_EHCI1_INDEX), 0, 0, 0x1);

			cpu_establish_hardintr("ehci1", filt, intr, arg,
				xlp_irt_to_irq(XLP_PIC_IRT_EHCI1_INDEX), flags, cookiep);
		
		}
	} else if (strcmp(name, "xlp_sdhci") == 0) {
		nlm_pic_write_irt_id(xlp_pic_base, XLP_PIC_IRT_MMC_INDEX, 1, 0, 
			xlp_irt_to_irq(XLP_PIC_IRT_MMC_INDEX), 0, 0, 0x1);

		cpu_establish_hardintr("xlp_sdhci", filt, intr, arg,
			xlp_irt_to_irq(XLP_PIC_IRT_MMC_INDEX), flags, cookiep);
	
        }

	return (0);
}

static struct resource *
iodi_alloc_resource(device_t bus, device_t child, int type, int *rid,
		    u_long start, u_long end, u_long count, u_int flags)
{
	struct resource *res = malloc(sizeof(*res), M_DEVBUF, M_WAITOK);
	const char *name = device_get_name(child);
	int unit;

	switch (type) {
	case SYS_RES_IRQ:
		device_printf(bus, "IRQ resource - for %s %lx-%lx\n",
			 device_get_nameunit(child), start, end);
                break;

        case SYS_RES_IOPORT:
		device_printf(bus, "IOPORT resource - for %s %lx-%lx\n",
			 device_get_nameunit(child), start, end);
                break;

        case SYS_RES_MEMORY:
		device_printf(bus, "MEMORY resource - for %s %lx-%lx\n",
			 device_get_nameunit(child), start, end);
                break;
        }

	unit = device_get_unit(child);
	if (strcmp(name, "uart") == 0) {
		if (unit == 0) {
			res->r_bushandle = nlm_regbase_uart(0, 0) + XLP_IO_PCI_HDRSZ;
		} else if ( unit == 1) {
			res->r_bushandle = nlm_regbase_uart(0, 1) + XLP_IO_PCI_HDRSZ;
		} else 
			printf("%s: Unknown uart unit\n", __FUNCTION__);

		res->r_bustag = uart_bus_space_mem;
	} 

	return (res);
}

static int
iodi_activate_resource(device_t bus, device_t child, int type, int rid,
		       struct resource *r)
{
	return (0);
}
/* prototypes */
static int	iodi_probe(device_t);
static int	iodi_attach(device_t);
static void	iodi_identify(driver_t *, device_t);

int
iodi_probe(device_t dev)
{
	return 0;
}

void
iodi_identify(driver_t *driver, device_t parent)
{
	
	BUS_ADD_CHILD(parent, 0, "iodi", 0);
}


int
iodi_attach(device_t dev)
{
	device_t tmpd;
	char desc[32];
	int i;

	device_printf(dev, "IODI - Initialize message ring.\n");
	xlp_msgring_iodi_config();
	
	/*
	 *  Attach each devices
	 */
	device_add_child(dev, "uart", 0);
	device_add_child(dev, "xlp_i2c", 0);
	device_add_child(dev, "xlp_i2c", 1);
	device_add_child(dev, "ehci", 0);
	device_add_child(dev, "ehci", 1);
	device_add_child(dev, "xlp_sdhci", 0);

	for (i=0; i < XLP_NUM_NODES; i++) {
		tmpd = device_add_child(dev, "xlpnae", i);
		device_set_ivars(tmpd, &xlp_board_info.nodes[i].nae_ivars);
		snprintf(desc, sizeof(desc), "XLP NAE %d", i);
		device_set_desc_copy(tmpd, desc);
	}

	bus_generic_probe(dev);
	bus_generic_attach(dev);
	return 0;
}

static device_method_t iodi_methods[] = {
	DEVMETHOD(device_probe, iodi_probe),
	DEVMETHOD(device_attach, iodi_attach),
	DEVMETHOD(device_identify, iodi_identify),
	DEVMETHOD(bus_alloc_resource, iodi_alloc_resource),
	DEVMETHOD(bus_activate_resource, iodi_activate_resource),
	DEVMETHOD(bus_add_child, bus_generic_add_child),
	DEVMETHOD(bus_setup_intr, iodi_setup_intr),
	{0, 0},
};

static driver_t iodi_driver = {
	"iodi",
	iodi_methods,
	1      /* no softc */
};
static devclass_t iodi_devclass;

DRIVER_MODULE(iodi, nexus, iodi_driver, iodi_devclass, 0, 0);
