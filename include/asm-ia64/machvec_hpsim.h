#ifndef _ASM_IA64_MACHVEC_HPSIM_h
#define _ASM_IA64_MACHVEC_HPSIM_h

extern ia64_mv_setup_t hpsim_setup;
extern ia64_mv_irq_init_t hpsim_irq_init;
extern ia64_mv_map_nr_t map_nr_dense;

/*
 * This stuff has dual use!
 *
 * For a generic kernel, the macros are used to initialize the
 * platform's machvec structure.  When compiling a non-generic kernel,
 * the macros are used directly.
 */
#define platform_name		"hpsim"
#define platform_setup		hpsim_setup
#define platform_irq_init	hpsim_irq_init
#define platform_map_nr		map_nr_dense

#endif /* _ASM_IA64_MACHVEC_HPSIM_h */
