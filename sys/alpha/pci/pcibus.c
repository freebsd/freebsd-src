/*
 * Copyright (c) 1997, Stefan Esser <se@freebsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
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
 *
 * $Id: pcibus.c,v 1.3 1998/07/22 08:33:30 dfr Exp $
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/interrupt.h>

#include <pci/pcivar.h>
#include <machine/chipset.h>
#include <machine/cpuconf.h>

static int cfgmech;
static int devmax;

#ifdef notyet

/* return max number of devices on the bus */
int
pci_maxdevs(pcicfgregs *cfg)
{
	return chipset.maxdevs(cfg->bus);
}

#endif

/* read configuration space register */

int
pci_cfgread(pcicfgregs *cfg, int reg, int bytes)
{
	switch (bytes) {
	case 1:
		return chipset.cfgreadb(cfg->bus, cfg->slot, cfg->func, reg);
	case 2:
		return chipset.cfgreadw(cfg->bus, cfg->slot, cfg->func, reg);
	case 4:
		return chipset.cfgreadl(cfg->bus, cfg->slot, cfg->func, reg);
	}
	return ~0;
}		


/* write configuration space register */

void
pci_cfgwrite(pcicfgregs *cfg, int reg, int data, int bytes)
{
	switch (bytes) {
	case 1:
		return chipset.cfgwriteb(cfg->bus, cfg->slot, cfg->func, reg, data);
	case 2:
		return chipset.cfgwritew(cfg->bus, cfg->slot, cfg->func, reg, data);
	case 4:
		return chipset.cfgwritel(cfg->bus, cfg->slot, cfg->func, reg, data);
	}
}

int
pci_cfgopen(void)
{
	return 1;
}

/*
 * These can disappear when I update the pci code to use the new
 * device framework.
 */
struct intrec *
intr_create(void *dev_instance, int irq, inthand2_t handler, void *arg,
	    intrmask_t *maskptr, int flags)
{
	device_t pcib = chipset.intrdev;
	if (pcib)
		return BUS_CREATE_INTR(pcib, pcib, irq,
				       (driver_intr_t*) handler, arg);
	else
		return 0;
}

int
intr_connect(struct intrec *idesc)
{
	device_t pcib = chipset.intrdev;
	if (pcib)
		return BUS_CONNECT_INTR(pcib, idesc);
	else
		return EINVAL;
}

void
alpha_platform_assign_pciintr(pcicfgregs *cfg)
{
	if(platform.pci_intr_map)
		platform.pci_intr_map((void *)cfg);
}

void
memcpy_fromio(void *d, u_int32_t s, size_t size)
{
    char *cp = d;

    while (size--)
	*cp++ = readb(s++);
}

void
memcpy_toio(u_int32_t d, void *s, size_t size)
{
    char *cp = s;

    while (size--)
	writeb(d++, *cp++);
}

void
memset_io(u_int32_t d, int val, size_t size)
{
    while (size--)
	writeb(d++, val);
}

#include "opt_ddb.h"
#ifdef DDB
#include <ddb/ddb.h>

DB_COMMAND(in, db_in)
{
    int c;
    int size;
    u_int32_t val;

    if (!have_addr)
	return;

    size = -1;
    while (c = *modif++) {
	switch (c) {
	case 'b':
	    size = 1;
	    break;
	case 'w':
	    size = 2;
	    break;
	case 'l':
	    size = 4;
	    break;
	}
    }

    if (size < 0) {
	db_printf("bad size\n");
	return;
    }

    if (count <= 0) count = 1;
    while (--count >= 0) {
	db_printf("%08x:\t", addr);
	switch (size) {
	case 1:
	    db_printf("%02x\n", inb(addr));
	    break;
	case 2:
	    db_printf("%04x\n", inw(addr));
	    break;
	case 4:
	    db_printf("%08x\n", inl(addr));
	    break;
	}
    }
}

#endif
