/*-
 * Copyright (c) 2012 Juli Mallett <jmallett@FreeBSD.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/cons.h>
#include <sys/kernel.h>
#include <sys/reboot.h>

#include <contrib/octeon-sdk/cvmx.h>
#include <contrib/octeon-sdk/cvmx-bootmem.h>
#include <contrib/octeon-sdk/octeon-pci-console.h>

static cn_probe_t opcic_cnprobe;
static cn_init_t opcic_cninit;
static cn_term_t opcic_cnterm;
static cn_getc_t opcic_cngetc;
static cn_putc_t opcic_cnputc;
static cn_grab_t opcic_cngrab;
static cn_ungrab_t opcic_cnungrab;

CONSOLE_DRIVER(opcic);

static void
opcic_cnprobe(struct consdev *cp)
{
	const struct cvmx_bootmem_named_block_desc *pci_console_block;

	cp->cn_pri = CN_DEAD;

	pci_console_block = cvmx_bootmem_find_named_block(OCTEON_PCI_CONSOLE_BLOCK_NAME);
	if (pci_console_block == NULL)
		return;

	cp->cn_arg = (void *)(uintptr_t)pci_console_block->base_addr;
	snprintf(cp->cn_name, sizeof cp->cn_name, "opcic@%p", cp->cn_arg);
	cp->cn_pri = (boothowto & RB_SERIAL) ? CN_REMOTE : CN_NORMAL;
}

static void
opcic_cninit(struct consdev *cp)
{
	(void)cp;
}

static void
opcic_cnterm(struct consdev *cp)
{
	(void)cp;
}

static int
opcic_cngetc(struct consdev *cp)
{
	uint64_t console_desc_addr;
	char ch;
	int rv;

	console_desc_addr = (uintptr_t)cp->cn_arg;

	rv = octeon_pci_console_read(console_desc_addr, 0, &ch, 1, OCT_PCI_CON_FLAG_NONBLOCK);
	if (rv != 1)
		return (-1);
	return (ch);
}

static void
opcic_cnputc(struct consdev *cp, int c)
{
	uint64_t console_desc_addr;
	char ch;
	int rv;

	console_desc_addr = (uintptr_t)cp->cn_arg;
	ch = c;

	rv = octeon_pci_console_write(console_desc_addr, 0, &ch, 1, 0);
	if (rv == -1)
		panic("%s: octeon_pci_console_write failed.", __func__);
}

static void
opcic_cngrab(struct consdev *cp)
{
	(void)cp;
}

static void
opcic_cnungrab(struct consdev *cp)
{
	(void)cp;
}
