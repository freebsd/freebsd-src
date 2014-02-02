/*-
 * Copyright (c) 2013 Neel Natu <neel@freebsd.org>
 * Copyright (c) 2013 Tycho Nightingale <tycho.nightingale@pluribusnetworks.com>
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
 *
 * $FreeBSD$
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <machine/vmm.h>
#include <machine/vmm_dev.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vmmapi.h>

#include "acpi.h"
#include "inout.h"
#include "pci_emul.h"
#include "pci_lpc.h"
#include "uart_emul.h"

SET_DECLARE(lpc_dsdt_set, struct lpc_dsdt);
SET_DECLARE(lpc_sysres_set, struct lpc_sysres);

static struct pci_devinst *lpc_bridge;

#define	LPC_UART_NUM	2
static struct lpc_uart_softc {
	struct uart_softc *uart_softc;
	const char *opts;
	int	iobase;
	int	irq;
	int	enabled;
} lpc_uart_softc[LPC_UART_NUM];

static const char *lpc_uart_names[LPC_UART_NUM] = { "COM1", "COM2" };

/*
 * LPC device configuration is in the following form:
 * <lpc_device_name>[,<options>]
 * For e.g. "com1,stdio"
 */
int
lpc_device_parse(const char *opts)
{
	int unit, error;
	char *str, *cpy, *lpcdev;

	error = -1;
	str = cpy = strdup(opts);
	lpcdev = strsep(&str, ",");
	if (lpcdev != NULL) {
		for (unit = 0; unit < LPC_UART_NUM; unit++) {
			if (strcasecmp(lpcdev, lpc_uart_names[unit]) == 0) {
				lpc_uart_softc[unit].opts = str;
				error = 0;
				goto done;
			}
		}
	}

done:
	if (error)
		free(cpy);

	return (error);
}

static void
lpc_uart_intr_assert(void *arg)
{
	struct lpc_uart_softc *sc = arg;

	assert(sc->irq >= 0);

	vm_ioapic_pulse_irq(lpc_bridge->pi_vmctx, sc->irq);
}

static void
lpc_uart_intr_deassert(void *arg)
{
	/* 
	 * The COM devices on the LPC bus generate edge triggered interrupts,
	 * so nothing more to do here.
	 */
}

static int
lpc_uart_io_handler(struct vmctx *ctx, int vcpu, int in, int port, int bytes,
		    uint32_t *eax, void *arg)
{
	int offset;
	struct lpc_uart_softc *sc = arg;

	if (bytes != 1)
		return (-1);

	offset = port - sc->iobase;

	if (in)
		*eax = uart_read(sc->uart_softc, offset); 
	else
		uart_write(sc->uart_softc, offset, *eax);

	return (0);
}

static int
lpc_init(void)
{
	struct lpc_uart_softc *sc;
	struct inout_port iop;
	const char *name;
	int unit, error;

	/* COM1 and COM2 */
	for (unit = 0; unit < LPC_UART_NUM; unit++) {
		sc = &lpc_uart_softc[unit];
		name = lpc_uart_names[unit];

		if (uart_legacy_alloc(unit, &sc->iobase, &sc->irq) != 0) {
			fprintf(stderr, "Unable to allocate resources for "
			    "LPC device %s\n", name);
			return (-1);
		}

		sc->uart_softc = uart_init(lpc_uart_intr_assert,
				    lpc_uart_intr_deassert, sc);

		if (uart_set_backend(sc->uart_softc, sc->opts) != 0) {
			fprintf(stderr, "Unable to initialize backend '%s' "
			    "for LPC device %s\n", sc->opts, name);
			return (-1);
		}

		bzero(&iop, sizeof(struct inout_port));
		iop.name = name;
		iop.port = sc->iobase;
		iop.size = UART_IO_BAR_SIZE;
		iop.flags = IOPORT_F_INOUT;
		iop.handler = lpc_uart_io_handler;
		iop.arg = sc;

		error = register_inout(&iop);
		assert(error == 0);
		sc->enabled = 1;
	}

	return (0);
}

static void
pci_lpc_write_dsdt(struct pci_devinst *pi)
{
	struct lpc_dsdt **ldpp, *ldp;

	dsdt_line("");
	dsdt_line("Device (ISA)");
	dsdt_line("{");
	dsdt_line("  Name (_ADR, 0x%04X%04X)", pi->pi_slot, pi->pi_func);
	dsdt_line("  OperationRegion (P40C, PCI_Config, 0x60, 0x04)");

	dsdt_indent(1);
	SET_FOREACH(ldpp, lpc_dsdt_set) {
		ldp = *ldpp;
		ldp->handler();
	}
	dsdt_unindent(1);

	dsdt_line("}");
}

static void
pci_lpc_sysres_dsdt(void)
{
	struct lpc_sysres **lspp, *lsp;

	dsdt_line("");
	dsdt_line("Device (SIO)");
	dsdt_line("{");
	dsdt_line("  Name (_HID, EisaId (\"PNP0C02\"))");
	dsdt_line("  Name (_CRS, ResourceTemplate ()");
	dsdt_line("  {");

	dsdt_indent(2);
	SET_FOREACH(lspp, lpc_sysres_set) {
		lsp = *lspp;
		switch (lsp->type) {
		case LPC_SYSRES_IO:
			dsdt_fixed_ioport(lsp->base, lsp->length);
			break;
		case LPC_SYSRES_MEM:
			dsdt_fixed_mem32(lsp->base, lsp->length);
			break;
		}
	}
	dsdt_unindent(2);

	dsdt_line("  })");
	dsdt_line("}");
}
LPC_DSDT(pci_lpc_sysres_dsdt);

static void
pci_lpc_uart_dsdt(void)
{
	struct lpc_uart_softc *sc;
	int unit;

	for (unit = 0; unit < LPC_UART_NUM; unit++) {
		sc = &lpc_uart_softc[unit];
		if (!sc->enabled)
			continue;
		dsdt_line("");
		dsdt_line("Device (%s)", lpc_uart_names[unit]);
		dsdt_line("{");
		dsdt_line("  Name (_HID, EisaId (\"PNP0501\"))");
		dsdt_line("  Name (_UID, %d)", unit + 1);
		dsdt_line("  Name (_CRS, ResourceTemplate ()");
		dsdt_line("  {");
		dsdt_indent(2);
		dsdt_fixed_ioport(sc->iobase, UART_IO_BAR_SIZE);
		dsdt_fixed_irq(sc->irq);
		dsdt_unindent(2);
		dsdt_line("  })");
		dsdt_line("}");
	}
}
LPC_DSDT(pci_lpc_uart_dsdt);

static void
pci_lpc_write(struct vmctx *ctx, int vcpu, struct pci_devinst *pi,
	       int baridx, uint64_t offset, int size, uint64_t value)
{
}

uint64_t
pci_lpc_read(struct vmctx *ctx, int vcpu, struct pci_devinst *pi,
	      int baridx, uint64_t offset, int size)
{
	return (0);
}

#define	LPC_DEV		0x7000
#define	LPC_VENDOR	0x8086

static int
pci_lpc_init(struct vmctx *ctx, struct pci_devinst *pi, char *opts)
{
	/*
	 * Do not allow more than one LPC bridge to be configured.
	 */
	if (lpc_bridge != NULL)
		return (-1);

	if (lpc_init() != 0)
		return (-1);

	/* initialize config space */
	pci_set_cfgdata16(pi, PCIR_DEVICE, LPC_DEV);
	pci_set_cfgdata16(pi, PCIR_VENDOR, LPC_VENDOR);
	pci_set_cfgdata8(pi, PCIR_CLASS, PCIC_BRIDGE);
	pci_set_cfgdata8(pi, PCIR_SUBCLASS, PCIS_BRIDGE_ISA);

	lpc_bridge = pi;

	return (0);
}

struct pci_devemu pci_de_lpc = {
	.pe_emu =	"lpc",
	.pe_init =	pci_lpc_init,
	.pe_write_dsdt = pci_lpc_write_dsdt,
	.pe_barwrite =	pci_lpc_write,
	.pe_barread =	pci_lpc_read
};
PCI_EMUL_SET(pci_de_lpc);
