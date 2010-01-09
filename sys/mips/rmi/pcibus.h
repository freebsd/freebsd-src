/*-
 * Copyright (c) 1998 Doug Rabson
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
 * $FreeBSD: src/sys/alpha/pci/pcibus.h,v 1.5 2002/02/28 18:18:41 gallatin Exp $
 */
#define DEFAULT_PCI_CONFIG_BASE         0x18000000

#define MSI_MIPS_ADDR_BASE             0xfee00000


#define PCIE_LINK0_MSI_STATUS        0x90
#define PCIE_LINK1_MSI_STATUS        0x94
#define PCIE_LINK2_MSI_STATUS        0x190
#define PCIE_LINK3_MSI_STATUS        0x194

void pci_init_resources(void);
struct resource *
xlr_pci_alloc_resource(device_t bus, device_t child,
    int type, int *rid,
    u_long start, u_long end, u_long count,
    u_int flags);
int 
pci_activate_resource(device_t bus, device_t child, int type, int rid,
    struct resource *r);
int 
pci_deactivate_resource(device_t bus, device_t child, int type, int rid,
    struct resource *r);
int 
pci_release_resource(device_t bus, device_t child, int type, int rid,
    struct resource *r);
struct rman *pci_get_rman(device_t dev, int type);

int
mips_platform_pci_setup_intr(device_t dev, device_t child,
    struct resource *irq, int flags,
    driver_filter_t * filt,
    driver_intr_t * intr, void *arg,
    void **cookiep);
int
    mips_pci_route_interrupt(device_t bus, device_t dev, int pin);
