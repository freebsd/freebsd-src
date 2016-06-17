#ifndef _ASM_IA64_MACHVEC_HPZX1_h
#define _ASM_IA64_MACHVEC_HPZX1_h

extern ia64_mv_setup_t dig_setup;
extern ia64_mv_pci_fixup_t hpzx1_pci_fixup;
extern ia64_mv_map_nr_t map_nr_dense;
extern ia64_mv_pci_alloc_consistent sba_alloc_coherent;
extern ia64_mv_pci_free_consistent sba_free_coherent;
extern ia64_mv_pci_map_single sba_map_single;
extern ia64_mv_pci_unmap_single sba_unmap_single;
extern ia64_mv_pci_map_sg sba_map_sg;
extern ia64_mv_pci_unmap_sg sba_unmap_sg;
extern ia64_mv_pci_dma_supported sba_dma_supported;

/*
 * This stuff has dual use!
 *
 * For a generic kernel, the macros are used to initialize the
 * platform's machvec structure.  When compiling a non-generic kernel,
 * the macros are used directly.
 */
#define platform_name			"hp"
#define platform_setup			dig_setup
#define platform_pci_fixup		hpzx1_pci_fixup
#define platform_map_nr			map_nr_dense
#define platform_pci_dma_init		((ia64_mv_pci_dma_init *) machvec_noop)
#define platform_pci_alloc_consistent	sba_alloc_coherent
#define platform_pci_free_consistent	sba_free_coherent
#define platform_pci_map_single		sba_map_single
#define platform_pci_unmap_single	sba_unmap_single
#define platform_pci_map_sg		sba_map_sg
#define platform_pci_unmap_sg		sba_unmap_sg
#define platform_pci_dma_sync_single	((ia64_mv_pci_dma_sync_single *) machvec_noop)
#define platform_pci_dma_sync_sg	((ia64_mv_pci_dma_sync_sg *) machvec_noop)
#define platform_pci_dma_supported	sba_dma_supported

#endif /* _ASM_IA64_MACHVEC_HPZX1_h */
