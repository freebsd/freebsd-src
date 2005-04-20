/*-
 * Copyright (c) 2003, 2004 Marcel Moolenaar
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_isa.h"

#include <sys/param.h>
#include <sys/systm.h>

#include <machine/bus.h>
#include <machine/bus_private.h>
#include <machine/resource.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/openfirm.h>
#include <machine/ofw_machdep.h>

#include <isa/isavar.h>

#include <dev/uart/uart.h>
#include <dev/uart/uart_bus.h>
#include <dev/uart/uart_cpu.h>

#include <sparc64/pci/ofw_pci.h>
#include <sparc64/isa/ofw_isa.h>

bus_space_tag_t uart_bus_space_io;
bus_space_tag_t uart_bus_space_mem;

static struct bus_space_tag bst_store[3];

/*
 * Determine which channel of a SCC a device referenced by an alias is.
 * The information present in the OF device tree only allows to do this
 * for "ttyX" aliases. If a device is a channel of a SCC its property
 * in the /aliases node looks like one of these:
 * ttya:  '/central/fhc/zs@0,902000:a'
 * ttyc:  '/pci@1f,0/pci@1,1/ebus@1/se@14,400000:a'
 */
static int
uart_cpu_channel(char *dev)
{
	char alias[64];
	phandle_t aliases;
	int len;

	strcpy(alias, dev);
	if ((aliases = OF_finddevice("/aliases")) != -1)
		OF_getprop(aliases, dev, alias, sizeof(alias));
	len = strlen(alias);
	if (len < 2 || alias[len - 2] != ':' || alias[len - 1] < 'a' ||
	    alias[len - 1] > 'b')
		return (0);
	return (alias[len - 1] - 'a' + 1);
}

int
uart_cpu_eqres(struct uart_bas *b1, struct uart_bas *b2)
{

	return ((b1->bsh == b2->bsh) ? 1 : 0);
}

/*
 * Get the package handle of the UART that is selected as the console, if
 * the console is an UART of course. Note that we enforce that both stdin
 * and stdout are selected.
 * Note that the currently active console (i.e. /chosen/stdout and
 * /chosen/stdin) may not be the same as the device selected in the
 * environment (ie /options/output-device and /options/input-device) because
 * keyboard and screen were selected but the keyboard was unplugged or the
 * user has changed the environment. In the latter case I would assume that
 * the user expects that FreeBSD uses the new console setting.
 * For weirder configurations, use ofw_console(4).
 */
static phandle_t
uart_cpu_getdev_console(phandle_t options, char *dev, size_t devsz)
{
	char buf[32];
	ihandle_t stdin, stdout;
	phandle_t chosen, input;

	if (OF_getprop(options, "input-device", dev, devsz) == -1)
		return (-1);
	if (OF_getprop(options, "output-device", buf, sizeof(buf)) == -1)
		return (-1);
	if (!strcmp(dev, "keyboard") && !strcmp(buf, "screen")) {
		if ((chosen = OF_finddevice("/chosen")) == -1)
			return (-1);
		if (OF_getprop(chosen, "stdin", &stdin, sizeof(stdin)) == -1)
			return (-1);
		if ((input = OF_instance_to_package(stdin)) == -1)
			return (-1);
		if (OF_getprop(chosen, "stdout", &stdout, sizeof(stdout)) == -1)
			return (-1);
		if (OF_instance_to_package(stdout) != input)
			return (-1);
		snprintf(dev, devsz, "ttya");
	} else {
		if ((input = OF_finddevice(dev)) == -1)
			return (-1);
		if (OF_finddevice(buf) != input)
			return (-1);
	}
	if (OF_getprop(input, "device_type", buf, sizeof(buf)) == -1)
		return (-1);
	if (strcmp(buf, "serial") != 0)
		return (-1);
	return (input);
}

/*
 * Get the package handle of the UART that's selected as the debug port.
 * Since there's no place for this in the OF, we use the kernel environment
 * variable "hw.uart.dbgport". Note however that the variable is not a
 * list of attributes. It's single device name or alias, as known by
 * the OF.
 */
static phandle_t
uart_cpu_getdev_dbgport(phandle_t options, char *dev, size_t devsz)
{
	char buf[32];
	phandle_t input;

	if (!getenv_string("hw.uart.dbgport", dev, devsz))
		return (-1);
	if ((input = OF_finddevice(dev)) == -1)
		return (-1);
	if (OF_getprop(input, "device_type", buf, sizeof(buf)) == -1)
		return (-1);
	if (strcmp(buf, "serial") != 0)
		return (-1);
	return (input);
}

/*
 * Get the package handle of the device that is selected as the keyboard
 * port.
 * XXX this also matches PS/2 keyboard controllers and most likely also
 * USB keyboards.
 */
static phandle_t
uart_cpu_getdev_keyboard(phandle_t root, char *dev, size_t devsz)
{
	char buf[32];
	phandle_t child, node;

	child = OF_child(root);
	while (child != 0 && child != -1) {
		if (OF_getprop(child, "device_type", buf, sizeof(buf)) != -1 &&
		    !strcmp(buf, "serial") &&
		    OF_getprop(child, "keyboard", buf, sizeof(buf)) != -1) {
			OF_getprop(child, "name", dev, devsz);
			return (child);
		}
		if ((node = uart_cpu_getdev_keyboard(child, dev, devsz)) != -1)
			return (node);
		child = OF_peer(child);
	}
	return (-1);
}

int
uart_cpu_getdev(int devtype, struct uart_devinfo *di)
{
	char buf[32], dev[32], compat[32];
	phandle_t input, options;
	bus_addr_t addr;
	int baud, bits, error, space, stop;
	char flag, par;

	if ((options = OF_finddevice("/options")) == -1)
		return (ENXIO);
	switch (devtype) {
	case UART_DEV_CONSOLE:
		input = uart_cpu_getdev_console(options, dev, sizeof(dev));
		break;
	case UART_DEV_DBGPORT:
		input = uart_cpu_getdev_dbgport(options, dev, sizeof(dev));
		break;
	case UART_DEV_KEYBOARD:
		input = uart_cpu_getdev_keyboard(OF_peer(0), dev, sizeof(dev));
		break;
	default:
		input = -1;
		break;
	}
	if (input == -1)
		return (ENXIO);
	error = OF_decode_addr(input, &space, &addr);
	if (error)
		return (error);

	/* Get the device class. */
	if (OF_getprop(input, "name", buf, sizeof(buf)) == -1)
		return (ENXIO);
	if (OF_getprop(input, "compatible", compat, sizeof(compat)) == -1)
		compat[0] = '\0';
	di->bas.regshft = 0;
	di->bas.rclk = 0;
	if (!strcmp(buf, "se")) {
		di->ops = uart_sab82532_ops;
		/* SAB82532 are only known to be used for TTYs. */
		if ((di->bas.chan = uart_cpu_channel(dev)) == 0)
			return (ENXIO);
		addr += 64 * (di->bas.chan - 1);
	} else if (!strcmp(buf, "zs")) {
		di->ops = uart_z8530_ops;
		if ((di->bas.chan = uart_cpu_channel(dev)) == 0) {
			/*
			 * There's no way to determine from OF which
			 * channel has the keyboard. Should always be
			 * on channel 1 however.
			 */
			if (devtype == UART_DEV_KEYBOARD)
				di->bas.chan = 1;
			else
				return (ENXIO);
		}
		di->bas.regshft = 1;
		addr += 4 - 4 * (di->bas.chan - 1);
	} else if (!strcmp(buf, "su") || !strcmp(buf, "su_pnp") ||
	    !strcmp(compat, "su") || !strcmp(compat, "su16550")) {
		di->ops = uart_ns8250_ops;
		di->bas.chan = 0;
	} else
		return (ENXIO);

	/* Fill in the device info. */
	di->bas.bst = &bst_store[devtype];
	di->bas.bsh = sparc64_fake_bustag(space, addr, di->bas.bst);

	/* Get the line settings. */
	if (devtype == UART_DEV_KEYBOARD)
		di->baudrate = 1200;
	else
		di->baudrate = 9600;
	di->databits = 8;
	di->stopbits = 1;
	di->parity = UART_PARITY_NONE;
	snprintf(buf, sizeof(buf), "%s-mode", dev);
	if (OF_getprop(options, buf, buf, sizeof(buf)) == -1)
		return (0);
	if (sscanf(buf, "%d,%d,%c,%d,%c", &baud, &bits, &par, &stop, &flag)
	    != 5)
		return (0);
	di->baudrate = baud;
	di->databits = bits;
	di->stopbits = stop;
	di->parity = (par == 'n') ? UART_PARITY_NONE :
	    (par == 'o') ? UART_PARITY_ODD : UART_PARITY_EVEN;
	return (0);
}

void
uart_cpu_identify(driver_t *driver, device_t parent)
{
#ifdef DEV_ISA
	char buf[32];
	struct isa_regs reg;
	device_t child;
	phandle_t node;
	ofw_isa_intr_t intr;
#endif

#ifdef DEV_ISA
	if (strcmp(device_get_name(parent), "isa") == 0) {
		if ((node = ofw_bus_get_node(device_get_parent(parent))) == 0)
			return;
		for (node = OF_child(node); node != 0; node = OF_peer(node)) {
			if (OF_getprop(node, "name", buf, sizeof(buf)) == -1)
				continue;
			if (strcmp(buf, "serial") != 0)
				continue;
			if ((OF_getprop(node, "reg", &reg,
			    sizeof(reg)) == -1) ||
		    	    (OF_getprop(node, "interrupts", &intr,
			    sizeof(intr)) == -1))
				continue;
			if ((child = BUS_ADD_CHILD(parent, ISA_ORDER_SENSITIVE,
			    uart_driver_name, -1)) == NULL)
				return;
			bus_set_resource(child, SYS_RES_IOPORT, 0,
			    ISA_REG_PHYS(&reg), reg.size);
			bus_set_resource(child, SYS_RES_IRQ, 0, intr, 1);
		}
	}
#endif
}
