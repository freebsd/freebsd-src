/*-
 * Copyright (c) 2009-2010 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by Semihalf under sponsorship from
 * the FreeBSD Foundation.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
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

#ifndef _FDT_COMMON_H_
#define _FDT_COMMON_H_

#include <sys/slicer.h>
#include <contrib/libfdt/libfdt_env.h>
#include <dev/ofw/ofw_bus.h>
#include <machine/fdt.h>

#define FDT_MEM_REGIONS	8

#define DI_MAX_INTR_NUM	32

struct fdt_pci_range {
	u_long	base_pci;
	u_long	base_parent;
	u_long	len;
};

struct fdt_sense_level {
	enum intr_trigger	trig;
	enum intr_polarity	pol;
};

typedef int (*fdt_pic_decode_t)(phandle_t, pcell_t *, int *, int *, int *);
extern fdt_pic_decode_t fdt_pic_table[];

typedef void (*fdt_fixup_t)(phandle_t);
struct fdt_fixup_entry {
	char		*model;
	fdt_fixup_t	handler;
};
extern struct fdt_fixup_entry fdt_fixup_table[];

extern SLIST_HEAD(fdt_ic_list, fdt_ic) fdt_ic_list_head;
struct fdt_ic {
	SLIST_ENTRY(fdt_ic)	fdt_ics;
	ihandle_t		iph;
	device_t		dev;
};

extern vm_paddr_t fdt_immr_pa;
extern vm_offset_t fdt_immr_va;
extern vm_offset_t fdt_immr_size;

struct fdt_pm_mask_entry {
	char		*compat;
	uint32_t	mask;
};
extern struct fdt_pm_mask_entry fdt_pm_mask_table[];

#if defined(FDT_DTB_STATIC)
extern u_char fdt_static_dtb;
#endif

int fdt_addrsize_cells(phandle_t, int *, int *);
u_long fdt_data_get(void *, int);
int fdt_data_to_res(pcell_t *, int, int, u_long *, u_long *);
int fdt_data_verify(void *, int);
phandle_t fdt_find_compatible(phandle_t, const char *, int);
int fdt_get_mem_regions(struct mem_region *, int *, uint32_t *);
int fdt_get_reserved_regions(struct mem_region *, int *);
int fdt_get_phyaddr(phandle_t, device_t, int *, void **);
int fdt_get_range(phandle_t, int, u_long *, u_long *);
int fdt_immr_addr(vm_offset_t);
int fdt_regsize(phandle_t, u_long *, u_long *);
int fdt_intr_to_rl(device_t, phandle_t, struct resource_list *, struct fdt_sense_level *);
int fdt_is_compatible(phandle_t, const char *);
int fdt_is_compatible_strict(phandle_t, const char *);
int fdt_is_enabled(phandle_t);
int fdt_pm_is_enabled(phandle_t);
int fdt_is_type(phandle_t, const char *);
int fdt_parent_addr_cells(phandle_t);
int fdt_pci_ranges(phandle_t, struct fdt_pci_range *, struct fdt_pci_range *);
int fdt_pci_ranges_decode(phandle_t, struct fdt_pci_range *,
    struct fdt_pci_range *);
int fdt_ranges_verify(pcell_t *, int, int, int, int);
int fdt_reg_to_rl(phandle_t, struct resource_list *);
int fdt_pm(phandle_t);
int fdt_get_unit(device_t);

#endif /* _FDT_COMMON_H_ */
