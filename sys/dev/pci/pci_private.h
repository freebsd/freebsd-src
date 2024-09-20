/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
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
 */

#ifndef _PCI_PRIVATE_H_
#define	_PCI_PRIVATE_H_

/*
 * Export definitions of the pci bus so that we can more easily share
 * it with "subclass" buses.
 */
DECLARE_CLASS(pci_driver);

struct pci_softc {
	bus_dma_tag_t sc_dma_tag;
	struct resource *sc_bus;
};

extern int 	pci_do_power_resume;
extern int 	pci_do_power_suspend;


device_attach_t		pci_attach;
device_detach_t		pci_detach;
device_resume_t		pci_resume;

bus_print_child_t	pci_print_child;
bus_probe_nomatch_t	pci_probe_nomatch;
bus_read_ivar_t		pci_read_ivar;
bus_write_ivar_t	pci_write_ivar;
bus_driver_added_t	pci_driver_added;
bus_setup_intr_t	pci_setup_intr;
bus_teardown_intr_t	pci_teardown_intr;

bus_get_dma_tag_t	pci_get_dma_tag;
bus_get_resource_list_t	pci_get_resource_list;
bus_delete_resource_t	pci_delete_resource;
bus_alloc_resource_t	pci_alloc_resource;
bus_adjust_resource_t	pci_adjust_resource;
bus_release_resource_t	pci_release_resource;
bus_activate_resource_t	pci_activate_resource;
bus_deactivate_resource_t pci_deactivate_resource;
bus_map_resource_t	pci_map_resource;
bus_unmap_resource_t	pci_unmap_resource;
bus_child_deleted_t	pci_child_deleted;
bus_child_detached_t	pci_child_detached;
bus_child_pnpinfo_t	pci_child_pnpinfo_method;
bus_child_location_t	pci_child_location_method;
bus_get_device_path_t	pci_get_device_path_method;
bus_suspend_child_t	pci_suspend_child;
bus_resume_child_t	pci_resume_child;
bus_rescan_t		pci_rescan_method;

pci_read_config_t	pci_read_config_method;
pci_write_config_t	pci_write_config_method;
pci_enable_busmaster_t	pci_enable_busmaster_method;
pci_disable_busmaster_t	pci_disable_busmaster_method;
pci_enable_io_t		pci_enable_io_method;
pci_disable_io_t	pci_disable_io_method;
pci_get_vpd_ident_t	pci_get_vpd_ident_method;
pci_get_vpd_readonly_t	pci_get_vpd_readonly_method;
pci_get_powerstate_t	pci_get_powerstate_method;
pci_set_powerstate_t	pci_set_powerstate_method;
pci_assign_interrupt_t	pci_assign_interrupt_method;
pci_find_cap_t		pci_find_cap_method;
pci_find_next_cap_t	pci_find_next_cap_method;
pci_find_extcap_t	pci_find_extcap_method;
pci_find_next_extcap_t	pci_find_next_extcap_method;
pci_find_htcap_t	pci_find_htcap_method;
pci_find_next_htcap_t	pci_find_next_htcap_method;
pci_alloc_msi_t		pci_alloc_msi_method;
pci_alloc_msix_t	pci_alloc_msix_method;
pci_enable_msi_t	pci_enable_msi_method;
pci_enable_msix_t	pci_enable_msix_method;
pci_disable_msi_t	pci_disable_msi_method;
pci_remap_msix_t	pci_remap_msix_method;
pci_release_msi_t	pci_release_msi_method;
pci_msi_count_t		pci_msi_count_method;
pci_msix_count_t	pci_msix_count_method;
pci_msix_pba_bar_t	pci_msix_pba_bar_method;
pci_msix_table_bar_t	pci_msix_table_bar_method;
pci_alloc_devinfo_t	pci_alloc_devinfo_method;
pci_child_added_t	pci_child_added_method;
#ifdef PCI_IOV
pci_iov_attach_t	pci_iov_attach_method;
pci_iov_detach_t	pci_iov_detach_method;
pci_create_iov_child_t	pci_create_iov_child_method;
#endif

void		pci_add_children(device_t dev, int domain, int busno);
void		pci_add_child(device_t bus, struct pci_devinfo *dinfo);
device_t	pci_add_iov_child(device_t bus, device_t pf, uint16_t rid,
		    uint16_t vid, uint16_t did);
void		pci_add_resources(device_t bus, device_t dev, int force,
		    uint32_t prefetchmask);
void		pci_add_resources_ea(device_t bus, device_t dev, int alloc_iov);
int		pci_attach_common(device_t dev);
int		pci_ea_is_enabled(device_t dev, int rid);
struct pci_devinfo *pci_read_device(device_t pcib, device_t bus, int d, int b,
		    int s, int f);
void		pci_print_verbose(struct pci_devinfo *dinfo);
int		pci_freecfg(struct pci_devinfo *dinfo);

/** Restore the config register state.  The state must be previously
 * saved with pci_cfg_save.  However, the pci bus driver takes care of
 * that.  This function will also return the device to PCI_POWERSTATE_D0
 * if it is currently in a lower power mode.
 */
void		pci_cfg_restore(device_t, struct pci_devinfo *);

/** Save the config register state.  Optionally set the power state to D3
 * if the third argument is non-zero.
 */
void		pci_cfg_save(device_t, struct pci_devinfo *, int);

int		pci_mapsize(uint64_t testval);
void		pci_read_bar(device_t dev, int reg, pci_addr_t *mapp,
		    pci_addr_t *testvalp, int *bar64);
struct pci_map *pci_add_bar(device_t dev, int reg, pci_addr_t value,
		    pci_addr_t size);

struct resource *pci_reserve_map(device_t dev, device_t child, int type,
		    int *rid, rman_res_t start, rman_res_t end,
		    rman_res_t count, u_int num, u_int flags);

struct resource *pci_alloc_multi_resource(device_t dev, device_t child,
		    int type, int *rid, rman_res_t start, rman_res_t end,
		    rman_res_t count, u_long num, u_int flags);

struct resource *pci_vf_alloc_mem_resource(device_t dev, device_t child,
		    int *rid, rman_res_t start, rman_res_t end,
		    rman_res_t count, u_int flags);
int		pci_vf_release_mem_resource(device_t dev, device_t child,
		    struct resource *r);
int		pci_vf_activate_mem_resource(device_t dev, device_t child,
		    struct resource *r);
int		pci_vf_deactivate_mem_resource(device_t dev, device_t child,
		    struct resource *r);
int		pci_vf_adjust_mem_resource(device_t dev, device_t child,
		    struct resource *r, rman_res_t start, rman_res_t end);
int		pci_vf_map_mem_resource(device_t dev, device_t child,
		    struct resource *r, struct resource_map_request *argsp,
		    struct resource_map *map);
int		pci_vf_unmap_mem_resource(device_t dev, device_t child,
		    struct resource *r, struct resource_map *map);

#endif /* _PCI_PRIVATE_H_ */
