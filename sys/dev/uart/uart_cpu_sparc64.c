/*
 * Copyright (c) 2003 Marcel Moolenaar
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

#include <sys/param.h>
#include <sys/systm.h>

#include <machine/bus.h>
#include <machine/bus_private.h>

#include <dev/ofw/openfirm.h>
#include <machine/ofw_machdep.h>

#include <dev/uart/uart.h>
#include <dev/uart/uart_cpu.h>

static phandle_t uart_cpu_getdev_keyboard(phandle_t root);

static struct bus_space_tag bst_store[3];

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

static phandle_t
uart_cpu_getdev_keyboard(phandle_t root)
{
	phandle_t child;
	phandle_t node;
	char buf[32];

	for (child = OF_child(root); child != 0 && child != -1;
	    child = OF_peer(child)) {
		if (OF_getprop(child, "device_type", buf, sizeof(buf)) != -1 &&
		    !strcmp(buf, "serial") &&
		    OF_getprop(child, "keyboard", buf, sizeof(buf)) != -1)
			return (child);
		if ((node = uart_cpu_getdev_keyboard(child)) != -1)
			return (node);
	}
	return (-1);
}

int
uart_cpu_getdev(int devtype, struct uart_devinfo *di)
{
	char buf[32], dev[32], compat[32];
	phandle_t input, options, output;
	bus_addr_t addr;
	int baud, bits, error, space, stop;
	char flag, par;

	/*
	 * Get the address of the UART that is selected as the console, if
	 * the console is an UART of course. Note that we enforce that both
	 * stdin and stdout are selected. For weird configurations, use
	 * ofw_console(4).
	 * Note that the currently active console (ie /chosen/stdout and
	 * /chosen/stdin) may not be the same as the device selected in the
	 * environment (ie /options/output-device and /options/input-device)
	 * because the user may have changed the environment. In that case
	 * I would assume that the user expects that FreeBSD uses the new
	 * console setting. There's choice choice, really.
	 */
	 if ((options = OF_finddevice("/options")) == -1)
		 return (ENXIO);
	if (devtype == UART_DEV_CONSOLE) {
		if (OF_getprop(options, "input-device", dev, sizeof(dev)) == -1)
			return (ENXIO);
		if ((input = OF_finddevice(dev)) == -1)
			return (ENXIO);
		if (OF_getprop(input, "device_type", buf, sizeof(buf)) == -1)
			return (ENXIO);
		if (strcmp(buf, "serial"))
			return (ENODEV);
		if (OF_getprop(options, "output-device", buf, sizeof(buf))
		    == -1)
			return (ENXIO);
		if ((output = OF_finddevice(buf)) == -1)
			return (ENXIO);
		if (input != output)
			return (ENXIO);
	} else if (devtype == UART_DEV_KEYBOARD) {
		if ((input = uart_cpu_getdev_keyboard(OF_peer(0))) == -1)
			return (ENXIO);
	} else
		return (ENODEV);

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
		di->bas.chan = uart_cpu_channel(dev);
		addr += 64 * (di->bas.chan - 1);
	} else if (!strcmp(buf, "zs")) {
		di->ops = uart_z8530_ops;
		di->bas.chan = uart_cpu_channel(dev);
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
