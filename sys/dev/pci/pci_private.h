/*
 * Copyright (c) 1997, Stefan Esser <se@freebsd.org>
 * Copyright (c) 2000, Michael Smith <msmith@freebsd.org>
 * Copyright (c) 2000, BSDi
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
 * $FreeBSD$
 *
 */

#ifndef _PCI_PRIVATE_H_
#define _PCI_PRIVATE_H_

/*
 * Export definitions of the pci bus so that we can more easily share
 * it with "subclass" busses.  A more generic subclassing mechanism would
 * be nice, but is not present in the tree at this time.
 */
int		pci_print_child(device_t dev, device_t child);
void		pci_probe_nomatch(device_t dev, device_t child);
int		pci_read_ivar(device_t dev, device_t child, int which,
		    uintptr_t *result);
int		pci_write_ivar(device_t dev, device_t child, int which,
		    uintptr_t value);
int		pci_set_powerstate_method(device_t dev, device_t child,
		    int state);
int		pci_get_powerstate_method(device_t dev, device_t child);
u_int32_t	pci_read_config_method(device_t dev, device_t child, 
		    int reg, int width);
void		pci_write_config_method(device_t dev, device_t child, 
		    int reg, u_int32_t val, int width);
void		pci_enable_busmaster_method(device_t dev, device_t child);
void		pci_disable_busmaster_method(device_t dev, device_t child);
void		pci_enable_io_method(device_t dev, device_t child, int space);
void		pci_disable_io_method(device_t dev, device_t child, int space);
struct resource	*pci_alloc_resource(device_t dev, device_t child, 
		    int type, int *rid, u_long start, u_long end, u_long count,
		    u_int flags);
void		pci_delete_resource(device_t dev, device_t child, 
		    int type, int rid);
struct resource_list *pci_get_resource_list (device_t dev, device_t child);
struct pci_devinfo *pci_read_device(device_t pcib, int b, int s, int f,
		    size_t size);
void		pci_print_verbose(struct pci_devinfo *dinfo);
int		pci_freecfg(struct pci_devinfo *dinfo);

#endif /* _PCI_PRIVATE_H_ */
