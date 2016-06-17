/*
 * Machine vector for IA-64.
 *
 * Copyright (C) 1999 Silicon Graphics, Inc.
 * Copyright (C) Srinivasa Thirumalachar <sprasad@engr.sgi.com>
 * Copyright (C) Vijay Chander <vijay@engr.sgi.com>
 * Copyright (C) 1999-2001 Hewlett-Packard Co.
 *	David Mosberger-Tang <davidm@hpl.hp.com>
 */
#ifndef _ASM_IA64_MACHVEC_H
#define _ASM_IA64_MACHVEC_H

#include <linux/config.h>
#include <linux/types.h>

/* forward declarations: */
struct pci_dev;
struct pt_regs;
struct scatterlist;
struct irq_desc;
struct page;

typedef void ia64_mv_setup_t (char **);
typedef void ia64_mv_cpu_init_t(void);
typedef void ia64_mv_irq_init_t (void);
typedef void ia64_mv_pci_fixup_t (int);
typedef unsigned long ia64_mv_map_nr_t (void *);
typedef void ia64_mv_mca_init_t (void);
typedef void ia64_mv_mca_handler_t (void);
typedef void ia64_mv_cmci_handler_t (int, void *, struct pt_regs *);
typedef void ia64_mv_log_print_t (void);
typedef void ia64_mv_send_ipi_t (int, int, int, int);
typedef void ia64_mv_global_tlb_purge_t (unsigned long, unsigned long, unsigned long);
typedef struct irq_desc *ia64_mv_irq_desc (unsigned int);
typedef u8 ia64_mv_irq_to_vector (u8);
typedef unsigned int ia64_mv_local_vector_to_irq (u8 vector);

/* PCI-DMA interface: */
typedef void ia64_mv_pci_dma_init (void);
typedef void *ia64_mv_pci_alloc_consistent (struct pci_dev *, size_t, dma_addr_t *);
typedef void ia64_mv_pci_free_consistent (struct pci_dev *, size_t, void *, dma_addr_t);
typedef dma_addr_t ia64_mv_pci_map_single (struct pci_dev *, void *, size_t, int);
typedef void ia64_mv_pci_unmap_single (struct pci_dev *, dma_addr_t, size_t, int);
typedef int ia64_mv_pci_map_sg (struct pci_dev *, struct scatterlist *, int, int);
typedef void ia64_mv_pci_unmap_sg (struct pci_dev *, struct scatterlist *, int, int);
typedef void ia64_mv_pci_dma_sync_single (struct pci_dev *, dma_addr_t, size_t, int);
typedef void ia64_mv_pci_dma_sync_sg (struct pci_dev *, struct scatterlist *, int, int);
typedef int ia64_mv_pci_dma_supported (struct pci_dev *, u64);

/*
 * WARNING: The legacy I/O space is _architected_.  Platforms are
 * expected to follow this architected model (see Section 10.7 in the
 * IA-64 Architecture Software Developer's Manual).  Unfortunately,
 * some broken machines do not follow that model, which is why we have
 * to make the inX/outX operations part of the machine vector.
 * Platform designers should follow the architected model whenever
 * possible.
 */
typedef unsigned int ia64_mv_inb_t (unsigned long);
typedef unsigned int ia64_mv_inw_t (unsigned long);
typedef unsigned int ia64_mv_inl_t (unsigned long);
typedef void ia64_mv_outb_t (unsigned char, unsigned long);
typedef void ia64_mv_outw_t (unsigned short, unsigned long);
typedef void ia64_mv_outl_t (unsigned int, unsigned long);

extern void machvec_noop (void);

# if defined (CONFIG_IA64_HP_SIM)
#  include <asm/machvec_hpsim.h>
# elif defined (CONFIG_IA64_DIG)
#  include <asm/machvec_dig.h>
# elif defined (CONFIG_IA64_HP_ZX1)
#  include <asm/machvec_hpzx1.h>
# elif defined (CONFIG_IA64_SGI_SN1)
#  include <asm/machvec_sn1.h>
# elif defined (CONFIG_IA64_SGI_SN2)
#  include <asm/machvec_sn2.h>
# elif defined (CONFIG_IA64_GENERIC)

# ifdef MACHVEC_PLATFORM_HEADER
#  include MACHVEC_PLATFORM_HEADER
# else
#  define platform_name		ia64_mv.name
#  define platform_setup	ia64_mv.setup
#  define platform_cpu_init	ia64_mv.cpu_init
#  define platform_irq_init	ia64_mv.irq_init
#  define platform_map_nr	ia64_mv.map_nr
#  define platform_mca_init	ia64_mv.mca_init
#  define platform_mca_handler	ia64_mv.mca_handler
#  define platform_cmci_handler	ia64_mv.cmci_handler
#  define platform_log_print	ia64_mv.log_print
#  define platform_pci_fixup	ia64_mv.pci_fixup
#  define platform_send_ipi		ia64_mv.send_ipi
#  define platform_global_tlb_purge	ia64_mv.global_tlb_purge
#  define platform_pci_dma_init		ia64_mv.dma_init
#  define platform_pci_alloc_consistent	ia64_mv.alloc_consistent
#  define platform_pci_free_consistent	ia64_mv.free_consistent
#  define platform_pci_map_single	ia64_mv.map_single
#  define platform_pci_unmap_single	ia64_mv.unmap_single
#  define platform_pci_map_sg		ia64_mv.map_sg
#  define platform_pci_unmap_sg		ia64_mv.unmap_sg
#  define platform_pci_dma_sync_single	ia64_mv.sync_single
#  define platform_pci_dma_sync_sg	ia64_mv.sync_sg
#  define platform_pci_dma_supported	ia64_mv.dma_supported
#  define platform_irq_desc		ia64_mv.irq_desc
#  define platform_irq_to_vector	ia64_mv.irq_to_vector
#  define platform_local_vector_to_irq	ia64_mv.local_vector_to_irq
#  define platform_inb		ia64_mv.inb
#  define platform_inw		ia64_mv.inw
#  define platform_inl		ia64_mv.inl
#  define platform_outb		ia64_mv.outb
#  define platform_outw		ia64_mv.outw
#  define platform_outl		ia64_mv.outl
# endif

struct ia64_machine_vector {
	const char *name;
	ia64_mv_setup_t *setup;
	ia64_mv_cpu_init_t *cpu_init;
	ia64_mv_irq_init_t *irq_init;
	ia64_mv_pci_fixup_t *pci_fixup;
	ia64_mv_map_nr_t *map_nr;
	ia64_mv_mca_init_t *mca_init;
	ia64_mv_mca_handler_t *mca_handler;
	ia64_mv_cmci_handler_t *cmci_handler;
	ia64_mv_log_print_t *log_print;
	ia64_mv_send_ipi_t *send_ipi;
	ia64_mv_global_tlb_purge_t *global_tlb_purge;
	ia64_mv_pci_dma_init *dma_init;
	ia64_mv_pci_alloc_consistent *alloc_consistent;
	ia64_mv_pci_free_consistent *free_consistent;
	ia64_mv_pci_map_single *map_single;
	ia64_mv_pci_unmap_single *unmap_single;
	ia64_mv_pci_map_sg *map_sg;
	ia64_mv_pci_unmap_sg *unmap_sg;
	ia64_mv_pci_dma_sync_single *sync_single;
	ia64_mv_pci_dma_sync_sg *sync_sg;
	ia64_mv_pci_dma_supported *dma_supported;
	ia64_mv_irq_desc *irq_desc;
	ia64_mv_irq_to_vector *irq_to_vector;
	ia64_mv_local_vector_to_irq *local_vector_to_irq;
	ia64_mv_inb_t *inb;
	ia64_mv_inw_t *inw;
	ia64_mv_inl_t *inl;
	ia64_mv_outb_t *outb;
	ia64_mv_outw_t *outw;
	ia64_mv_outl_t *outl;
};

#define MACHVEC_INIT(name)			\
{						\
	#name,					\
	platform_setup,				\
	platform_cpu_init,			\
	platform_irq_init,			\
	platform_pci_fixup,			\
	platform_map_nr,			\
	platform_mca_init,			\
	platform_mca_handler,			\
	platform_cmci_handler,			\
	platform_log_print,			\
	platform_send_ipi,			\
	platform_global_tlb_purge,		\
	platform_pci_dma_init,			\
	platform_pci_alloc_consistent,		\
	platform_pci_free_consistent,		\
	platform_pci_map_single,		\
	platform_pci_unmap_single,		\
	platform_pci_map_sg,			\
	platform_pci_unmap_sg,			\
	platform_pci_dma_sync_single,		\
	platform_pci_dma_sync_sg,		\
	platform_pci_dma_supported,		\
	platform_irq_desc,			\
	platform_irq_to_vector,			\
	platform_local_vector_to_irq,		\
	platform_inb,				\
	platform_inw,				\
	platform_inl,				\
	platform_outb,				\
	platform_outw,				\
	platform_outl				\
}

extern struct ia64_machine_vector ia64_mv;
extern void machvec_init (const char *name);

# else
#  error Unknown configuration.  Update asm-ia64/machvec.h.
# endif /* CONFIG_IA64_GENERIC */

/*
 * Declare default routines which aren't declared anywhere else:
 */
extern ia64_mv_pci_dma_init swiotlb_init;
extern ia64_mv_pci_alloc_consistent swiotlb_alloc_consistent;
extern ia64_mv_pci_free_consistent swiotlb_free_consistent;
extern ia64_mv_pci_map_single swiotlb_map_single;
extern ia64_mv_pci_unmap_single swiotlb_unmap_single;
extern ia64_mv_pci_map_sg swiotlb_map_sg;
extern ia64_mv_pci_unmap_sg swiotlb_unmap_sg;
extern ia64_mv_pci_dma_sync_single swiotlb_sync_single;
extern ia64_mv_pci_dma_sync_sg swiotlb_sync_sg;
extern ia64_mv_pci_dma_supported swiotlb_pci_dma_supported;

/*
 * Define default versions so we can extend machvec for new platforms without having
 * to update the machvec files for all existing platforms.
 */
#ifndef platform_setup
# define platform_setup		((ia64_mv_setup_t *) machvec_noop)
#endif
#ifndef platform_cpu_init
# define platform_cpu_init	((ia64_mv_cpu_init_t *) machvec_noop)
#endif
#ifndef platform_irq_init
# define platform_irq_init	((ia64_mv_irq_init_t *) machvec_noop)
#endif
#ifndef platform_mca_init
# define platform_mca_init	((ia64_mv_mca_init_t *) machvec_noop)
#endif
#ifndef platform_mca_handler
# define platform_mca_handler	((ia64_mv_mca_handler_t *) machvec_noop)
#endif
#ifndef platform_cmci_handler
# define platform_cmci_handler	((ia64_mv_cmci_handler_t *) machvec_noop)
#endif
#ifndef platform_log_print
# define platform_log_print	((ia64_mv_log_print_t *) machvec_noop)
#endif
#ifndef platform_pci_fixup
# define platform_pci_fixup	((ia64_mv_pci_fixup_t *) machvec_noop)
#endif
#ifndef platform_send_ipi
# define platform_send_ipi	ia64_send_ipi	/* default to architected version */
#endif
#ifndef platform_global_tlb_purge
# define platform_global_tlb_purge	ia64_global_tlb_purge /* default to architected version */
#endif
#ifndef platform_pci_dma_init
# define platform_pci_dma_init		swiotlb_init
#endif
#ifndef platform_pci_alloc_consistent
# define platform_pci_alloc_consistent	swiotlb_alloc_consistent
#endif
#ifndef platform_pci_free_consistent
# define platform_pci_free_consistent	swiotlb_free_consistent
#endif
#ifndef platform_pci_map_single
# define platform_pci_map_single	swiotlb_map_single
#endif
#ifndef platform_pci_unmap_single
# define platform_pci_unmap_single	swiotlb_unmap_single
#endif
#ifndef platform_pci_map_sg
# define platform_pci_map_sg		swiotlb_map_sg
#endif
#ifndef platform_pci_unmap_sg
# define platform_pci_unmap_sg		swiotlb_unmap_sg
#endif
#ifndef platform_pci_dma_sync_single
# define platform_pci_dma_sync_single	swiotlb_sync_single
#endif
#ifndef platform_pci_dma_sync_sg
# define platform_pci_dma_sync_sg	swiotlb_sync_sg
#endif
#ifndef platform_pci_dma_supported
# define  platform_pci_dma_supported	swiotlb_pci_dma_supported
#endif
#ifndef platform_irq_desc
# define platform_irq_desc		__ia64_irq_desc
#endif
#ifndef platform_irq_to_vector
# define platform_irq_to_vector		__ia64_irq_to_vector
#endif
#ifndef platform_local_vector_to_irq
# define platform_local_vector_to_irq	__ia64_local_vector_to_irq
#endif
#ifndef platform_inb
# define platform_inb		__ia64_inb
#endif
#ifndef platform_inw
# define platform_inw		__ia64_inw
#endif
#ifndef platform_inl
# define platform_inl		__ia64_inl
#endif
#ifndef platform_outb
# define platform_outb		__ia64_outb
#endif
#ifndef platform_outw
# define platform_outw		__ia64_outw
#endif
#ifndef platform_outl
# define platform_outl		__ia64_outl
#endif

#endif /* _ASM_IA64_MACHVEC_H */
