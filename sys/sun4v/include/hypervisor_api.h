/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 * $FreeBSD$
 */

/*
 * Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#ifndef _MACHINE_HYPERVISOR_API_H
#define	_MACHINE_HYPERVISOR_API_H

/*
 * sun4v Hypervisor API
 *
 * Reference: api.pdf Revision 0.12 dated May 12, 2004.
 *	      io-api.txt version 1.11 dated 10/19/2004
 */

#include <machine/hypervisorvar.h>

#ifndef _ASM

typedef uint64_t devhandle_t;
typedef uint64_t pci_device_t;
typedef uint32_t pci_config_offset_t;
typedef uint8_t pci_config_size_t;
typedef union pci_cfg_data {
	uint8_t b;
	uint16_t w;
	uint32_t dw;
	uint64_t qw;
} pci_cfg_data_t;
typedef uint64_t tsbid_t;
typedef uint32_t pages_t;
typedef enum io_attributes {
	PCI_MAP_ATTR_READ	= (uint32_t)0x01,
	PCI_MAP_ATTR_WRITE	= (uint32_t)0x02,
} io_attributes_t;
typedef enum io_sync_direction {
	IO_SYNC_DEVICE		= (uint32_t)0x01,
	IO_SYNC_CPU		= (uint32_t)0x02,
} io_sync_direction_t;
typedef uint64_t io_page_list_t;
typedef uint64_t r_addr_t;
typedef uint64_t io_addr_t;

typedef struct trap_trace_entry {
	uint8_t		tte_type;	/* Hypervisor or guest entry. */
	uint8_t		tte_hpstat;	/* Hyper-privileged state. */
	uint8_t		tte_tl;		/* Trap level. */
	uint8_t		tte_gl;		/* Global register level. */
	uint16_t	tte_tt;		/* Trap type.*/
	uint16_t	tte_tag;	/* Extended trap identifier. */
	uint64_t	tte_tstate;	/* Trap state. */
	uint64_t	tte_tick;	/* Tick. */
	uint64_t	tte_tpc;	/* Trap PC. */
	uint64_t	tte_f1;		/* Entry specific. */
	uint64_t	tte_f2;		/* Entry specific. */
	uint64_t	tte_f3;		/* Entry specific. */
	uint64_t	tte_f4;		/* Entry specific. */
} trap_trace_entry_t;

extern uint64_t hv_mmu_map_perm_addr(void *, int, uint64_t, int);
extern uint64_t	hv_mmu_unmap_perm_addr(void *, int, int);
extern uint64_t	hv_set_ctx0(uint64_t, uint64_t);
extern uint64_t	hv_set_ctxnon0(uint64_t, uint64_t);
#ifdef SET_MMU_STATS
extern uint64_t hv_mmu_set_stat_area(uint64_t, uint64_t);
#endif /* SET_MMU_STATS */

extern uint64_t hv_cpu_qconf(int queue, uint64_t paddr, int size);
extern uint64_t hv_cpu_mondo_send(int n, vm_paddr_t cpu_list_ra);
extern uint64_t hv_cpu_yield(void);

extern uint64_t hv_cpu_state(uint64_t cpuid, uint64_t *cpu_state);
extern uint64_t hv_mem_scrub(uint64_t real_addr, uint64_t length,
    uint64_t *scrubbed_len);
extern uint64_t hv_mem_sync(uint64_t real_addr, uint64_t length,
    uint64_t *flushed_len);

extern uint64_t hv_service_recv(uint64_t s_id, uint64_t buf_pa,
    uint64_t size, uint64_t *recv_bytes);
extern uint64_t hv_service_send(uint64_t s_id, uint64_t buf_pa,
    uint64_t size, uint64_t *send_bytes);
extern uint64_t hv_service_getstatus(uint64_t s_id, uint64_t *vreg);
extern uint64_t hv_service_setstatus(uint64_t s_id, uint64_t bits);
extern uint64_t hv_service_clrstatus(uint64_t s_id, uint64_t bits);

extern uint64_t hv_mach_desc(uint64_t buffer_ra, uint64_t *buffer_sizep);

extern uint64_t hv_ttrace_buf_info(uint64_t *, uint64_t *);
extern uint64_t hv_ttrace_buf_conf(uint64_t, uint64_t, uint64_t *);
extern uint64_t hv_ttrace_enable(uint64_t, uint64_t *);
extern uint64_t hv_ttrace_freeze(uint64_t, uint64_t *);
extern uint64_t hv_ttrace_addentry(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
extern uint64_t hv_dump_buf_update(uint64_t, uint64_t, uint64_t *);

extern int64_t hv_cnputchar(uint8_t);
extern int64_t hv_cngetchar(uint8_t *);

extern uint64_t hv_tod_get(uint64_t *seconds);
extern uint64_t hv_tod_set(uint64_t);

extern uint64_t hvio_intr_devino_to_sysino(uint64_t dev_hdl, uint32_t devino,
    uint64_t *sysino);
extern uint64_t hvio_intr_getvalid(uint64_t sysino,
    int *intr_valid_state);
extern uint64_t hvio_intr_setvalid(uint64_t sysino,
    int intr_valid_state);
extern uint64_t hvio_intr_getstate(uint64_t sysino,
    int *intr_state);
extern uint64_t hvio_intr_setstate(uint64_t sysino, int intr_state);
extern uint64_t hvio_intr_gettarget(uint64_t sysino, uint32_t *cpuid);
extern uint64_t hvio_intr_settarget(uint64_t sysino, uint32_t cpuid);
extern uint64_t hvio_peek(devhandle_t dev_hdl, uint64_t r_addr, uint64_t size,
			  uint32_t *err_flag, uint64_t *data);
extern uint64_t hvio_poke(devhandle_t dev_hdl, uint64_t r_addr, uint64_t size,
			  uint64_t data, uint64_t pcidev, uint32_t *err_flag);

extern uint64_t hvio_config_get(devhandle_t dev_hdl, pci_device_t pci_device, 
				pci_config_offset_t off, pci_config_size_t size, pci_cfg_data_t *data);
extern uint64_t hvio_config_put(devhandle_t dev_hdl, pci_device_t pci_device, 
				pci_config_offset_t off, pci_config_size_t size,
				pci_cfg_data_t data);
extern uint64_t hvio_iommu_map(devhandle_t dev_hdl, tsbid_t tsbid,
				pages_t pages, io_attributes_t io_attributes,
				io_page_list_t *io_page_list_p,
				pages_t *pages_mapped);
extern uint64_t hvio_iommu_demap(devhandle_t dev_hdl, tsbid_t tsbid,
				pages_t pages, pages_t *pages_demapped);
extern uint64_t hvio_iommu_getmap(devhandle_t dev_hdl, tsbid_t tsbid,
				 io_attributes_t *attributes_p, r_addr_t *r_addr_p);
extern uint64_t hvio_iommu_getbypass(devhandle_t dev_hdl, r_addr_t ra,
				io_attributes_t io_attributes,
				io_addr_t *io_addr_p);
extern uint64_t hvio_dma_sync(devhandle_t dev_hdl, r_addr_t ra,
				size_t num_bytes, uint64_t io_sync_direction,
				size_t *bytes_synched);

extern void hv_magic_trap_on(void);
extern void hv_magic_trap_off(void);
extern int hv_sim_read(uint64_t offset, vm_paddr_t buffer_ra, uint64_t size);
extern int hv_sim_write(uint64_t offset, vm_paddr_t buffer_ra, uint64_t size);

#endif

#endif /* _MACHINE_HYPERVISOR_API_H */
