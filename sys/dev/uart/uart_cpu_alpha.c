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
#include <sys/bus.h>
#include <sys/reboot.h>

#include <machine/bus.h>
#include <machine/md_var.h>
#include <machine/rpb.h>

#include <dev/uart/uart.h>
#include <dev/uart/uart_cpu.h>

bus_addr_t
uart_cpu_busaddr(struct uart_bas *bas)
{

	return (bas->bsh);
}

int
uart_cpu_eqres(struct uart_bas *b1, struct uart_bas *b2)
{

	return ((b1->bsh == b2->bsh && b1->bst == b2->bst) ? 1 : 0);
}

int
uart_cpu_getdev(int devtype, struct uart_devinfo *di)
{
	struct ctb *ctb;
	unsigned int i, ivar;

	if (devtype == UART_DEV_CONSOLE) {
		ctb = (struct ctb *)(((caddr_t)hwrpb) + hwrpb->rpb_ctb_off);
		if (ctb->ctb_term_type != CTB_PRINTERPORT)
			return (ENXIO);
		boothowto |= RB_SERIAL;
		di->ops = uart_ns8250_ops;
		di->bas.bst = busspace_isa_io;
		di->bas.bsh = 0x3f8;
		di->bas.regshft = 0;
		di->bas.rclk = 0;
		di->baudrate = 9600;
		di->databits = 8;
		di->stopbits = 1;
		di->parity = UART_PARITY_NONE;
		return (0);
	}

	/*
	 * Scan the hints. We only try units 0 to 3 (inclusive). This
	 * covers the ISA legacy where 4 UARTs had their resources
	 * predefined.
	 */
	for (i = 0; i < 4; i++) {
		if (resource_int_value("uart", i, "flags", &ivar))
			continue;
		if (devtype == UART_DEV_DBGPORT && !UART_FLAGS_DBGPORT(ivar))
			continue;
		/*
		 * We have a possible device. Make sure it's enabled and
		 * that we have an I/O port.
		 */
		if (resource_int_value("uart", i, "disabled", &ivar) == 0 &&
		    ivar != 0)
			continue;
		if (resource_int_value("uart", i, "port", &ivar) != 0 ||
		    ivar == 0)
			continue;
		/*
		 * Got it. Fill in the instance and return it. We assume we
		 * only have ns8250 and successors on alpha.
		 */
		di->ops = uart_ns8250_ops;
		di->bas.bst = busspace_isa_io;
		di->bas.bsh = ivar;
		di->bas.regshft = 0;
		di->bas.rclk = 0;
		if (resource_int_value("uart", i, "baud", &ivar) != 0)
			ivar = 0;
		di->baudrate = ivar;
		di->databits = 8;
		di->stopbits = 1;
		di->parity = UART_PARITY_NONE;
		return (0);
	}

	return (ENXIO);
}
