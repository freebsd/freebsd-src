/*
 * Copyright (c) 2002-2003 Silicon Graphics, Inc.  All Rights Reserved.
 * 
 * This program is free software; you can redistribute it and/or modify it 
 * under the terms of version 2 of the GNU General Public License 
 * as published by the Free Software Foundation.
 * 
 * This program is distributed in the hope that it would be useful, but 
 * WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 * 
 * Further, this software is distributed without any warranty that it is 
 * free of the rightful claim of any third person regarding infringement 
 * or the like.  Any license provided herein, whether implied or 
 * otherwise, applies only to this software file.  Patent licenses, if 
 * any, provided herein do not apply to combinations of this program with 
 * other software, or any other product whatsoever.
 * 
 * You should have received a copy of the GNU General Public 
 * License along with this program; if not, write the Free Software 
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston MA 02111-1307, USA.
 * 
 * Contact information:  Silicon Graphics, Inc., 1600 Amphitheatre Pkwy, 
 * Mountain View, CA  94043, or:
 * 
 * http://www.sgi.com 
 * 
 * For further information regarding this notice, see: 
 * 
 * http://oss.sgi.com/projects/GenInfo/NoticeExplan
 */

#ifndef _ASM_IA64_MACHVEC_SN2_H
#define _ASM_IA64_MACHVEC_SN2_H

#include <asm/sn/sn2/io.h>

extern ia64_mv_setup_t			sn_setup;
extern ia64_mv_cpu_init_t		sn_cpu_init;
extern ia64_mv_irq_init_t		sn_irq_init;
extern ia64_mv_send_ipi_t		sn2_send_IPI;
extern ia64_mv_global_tlb_purge_t	sn2_global_tlb_purge;
extern ia64_mv_irq_desc			sn_irq_desc;
extern ia64_mv_irq_to_vector		sn_irq_to_vector;
extern ia64_mv_local_vector_to_irq	sn_local_vector_to_irq;
extern ia64_mv_pci_fixup_t		sn_pci_fixup;
extern ia64_mv_pci_alloc_consistent	sn_pci_alloc_consistent;
extern ia64_mv_pci_free_consistent	sn_pci_free_consistent;
extern ia64_mv_pci_map_single		sn_pci_map_single;
extern ia64_mv_pci_unmap_single		sn_pci_unmap_single;
extern ia64_mv_pci_map_sg		sn_pci_map_sg;
extern ia64_mv_pci_unmap_sg		sn_pci_unmap_sg;
extern ia64_mv_pci_dma_sync_single	sn_pci_dma_sync_single;
extern ia64_mv_pci_dma_sync_sg		sn_pci_dma_sync_sg;
extern ia64_mv_pci_dma_supported	sn_pci_dma_supported;

/*
 * This stuff has dual use!
 *
 * For a generic kernel, the macros are used to initialize the
 * platform's machvec structure.  When compiling a non-generic kernel,
 * the macros are used directly.
 */
#define platform_name			"sn2"
#define platform_setup			sn_setup
#define platform_cpu_init		sn_cpu_init
#define platform_irq_init		sn_irq_init
#define platform_send_ipi		sn2_send_IPI
#define platform_global_tlb_purge       sn2_global_tlb_purge
#define platform_pci_fixup		sn_pci_fixup
#define platform_inb			__sn_inb
#define platform_inw			__sn_inw
#define platform_inl			__sn_inl
#define platform_outb			__sn_outb
#define platform_outw			__sn_outw
#define platform_outl			__sn_outl
#define platform_readb			__sn_readb
#define platform_readw			__sn_readw
#define platform_readl			__sn_readl
#define platform_readq			__sn_readq
#define platform_irq_desc		sn_irq_desc
#define platform_irq_to_vector		sn_irq_to_vector
#define platform_local_vector_to_irq	sn_local_vector_to_irq
#define platform_pci_dma_init		machvec_noop
#define platform_pci_alloc_consistent	sn_pci_alloc_consistent
#define platform_pci_free_consistent	sn_pci_free_consistent
#define platform_pci_map_single		sn_pci_map_single
#define platform_pci_unmap_single	sn_pci_unmap_single
#define platform_pci_map_sg		sn_pci_map_sg
#define platform_pci_unmap_sg		sn_pci_unmap_sg
#define platform_pci_dma_sync_single	sn_pci_dma_sync_single
#define platform_pci_dma_sync_sg	sn_pci_dma_sync_sg
#define platform_pci_dma_supported	sn_pci_dma_supported

#endif /* _ASM_IA64_MACHVEC_SN2_H */
