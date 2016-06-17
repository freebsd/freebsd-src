#ifndef _ASM_IA64_MACHVEC_DIG_h
#define _ASM_IA64_MACHVEC_DIG_h

extern ia64_mv_setup_t dig_setup;
extern ia64_mv_irq_init_t dig_irq_init;
extern ia64_mv_pci_fixup_t iosapic_pci_fixup;
extern ia64_mv_map_nr_t map_nr_dense;

/*
 * This stuff has dual use!
 *
 * For a generic kernel, the macros are used to initialize the
 * platform's machvec structure.  When compiling a non-generic kernel,
 * the macros are used directly.
 */
#define platform_name		"dig"
#define platform_setup		dig_setup
#define platform_irq_init	dig_irq_init
#define platform_pci_fixup	iosapic_pci_fixup
#define platform_map_nr		map_nr_dense

#endif /* _ASM_IA64_MACHVEC_DIG_h */
