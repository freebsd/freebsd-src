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
 * $Id: pcibus.c,v 1.41 1997/12/20 09:04:25 se Exp $
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus_private.h>

#include <pci/pcivar.h>
#include <machine/chipset.h>

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
