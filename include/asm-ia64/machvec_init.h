#include <asm/machvec.h>

extern ia64_mv_send_ipi_t ia64_send_ipi;
extern ia64_mv_global_tlb_purge_t ia64_global_tlb_purge;
extern ia64_mv_irq_desc __ia64_irq_desc;
extern ia64_mv_irq_to_vector __ia64_irq_to_vector;
extern ia64_mv_local_vector_to_irq __ia64_local_vector_to_irq;

extern ia64_mv_inb_t __ia64_inb;
extern ia64_mv_inw_t __ia64_inw;
extern ia64_mv_inl_t __ia64_inl;
extern ia64_mv_outb_t __ia64_outb;
extern ia64_mv_outw_t __ia64_outw;
extern ia64_mv_outl_t __ia64_outl;

#define MACHVEC_HELPER(name)									\
 struct ia64_machine_vector machvec_##name __attribute__ ((unused, __section__ (".machvec")))	\
	= MACHVEC_INIT(name);

#define MACHVEC_DEFINE(name)	MACHVEC_HELPER(name)

MACHVEC_DEFINE(MACHVEC_PLATFORM_NAME)
