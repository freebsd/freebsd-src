/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2012 NetApp, Inc.
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY NETAPP, INC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL NETAPP, INC OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/types.h>

#include <stdio.h>

#include "bhyverun.h"
#include "config.h"
#include "debug.h"
#include "pci_emul.h"
#include "uart_emul.h"

/*
 * Pick a PCI vid/did of a chip with a single uart at
 * BAR0, that most versions of FreeBSD can understand:
 * Siig CyberSerial 1-port.
 */
#define COM_VENDOR	0x131f
#define COM_DEV		0x2000

static void
pci_uart_intr_assert(void *arg)
{
	struct pci_devinst *pi = arg;

	pci_lintr_assert(pi);
}

static void
pci_uart_intr_deassert(void *arg)
{
	struct pci_devinst *pi = arg;

	pci_lintr_deassert(pi);
}

static void
pci_uart_write(struct pci_devinst *pi, int baridx, uint64_t offset, int size,
    uint64_t value)
{
	assert(baridx == 0);
	assert(size == 1);

	uart_ns16550_write(pi->pi_arg, offset, value);
}

static uint64_t
pci_uart_read(struct pci_devinst *pi, int baridx, uint64_t offset, int size)
{
	uint8_t val;

	assert(baridx == 0);
	assert(size == 1);

	val = uart_ns16550_read(pi->pi_arg, offset);
	return (val);
}

static int
pci_uart_legacy_config(nvlist_t *nvl, const char *opts)
{

	if (opts != NULL)
		set_config_value_node(nvl, "path", opts);
	return (0);
}

static int
pci_uart_init(struct pci_devinst *pi, nvlist_t *nvl)
{
	struct uart_ns16550_softc *sc;
	const char *device;

	pci_emul_alloc_bar(pi, 0, PCIBAR_IO, UART_NS16550_IO_BAR_SIZE);
	pci_lintr_request(pi);

	/* initialize config space */
	pci_set_cfgdata16(pi, PCIR_DEVICE, COM_DEV);
	pci_set_cfgdata16(pi, PCIR_VENDOR, COM_VENDOR);
	pci_set_cfgdata8(pi, PCIR_CLASS, PCIC_SIMPLECOMM);

	sc = uart_ns16550_init(pci_uart_intr_assert, pci_uart_intr_deassert,
	    pi);
	pi->pi_arg = sc;

	device = get_config_value_node(nvl, "path");
	if (device != NULL && uart_ns16550_tty_open(sc, device) != 0) {
		EPRINTLN("Unable to initialize backend '%s' for "
		    "pci uart at %d:%d", device, pi->pi_slot, pi->pi_func);
		return (-1);
	}

	return (0);
}

static const struct pci_devemu pci_de_com = {
	.pe_emu =	"uart",
	.pe_init =	pci_uart_init,
	.pe_legacy_config = pci_uart_legacy_config,
	.pe_barwrite =	pci_uart_write,
	.pe_barread =	pci_uart_read
};
PCI_EMUL_SET(pci_de_com);
