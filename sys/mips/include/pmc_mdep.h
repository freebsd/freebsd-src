/*-
 * This file is in the public domain.
 *
 *	from: src/sys/alpha/include/pmc_mdep.h,v 1.2 2005/06/09 19:45:06 jkoshy
 * $FreeBSD$
 */

#ifndef _MACHINE_PMC_MDEP_H_
#define	_MACHINE_PMC_MDEP_H_

#define	PMC_MDEP_CLASS_INDEX_MIPS24K	0
#include <dev/hwpmc/hwpmc_mips24k.h>

union pmc_md_op_pmcallocate {
	uint64_t	__pad[4];
};

/* Logging */
#define	PMCLOG_READADDR		PMCLOG_READ32
#define	PMCLOG_EMITADDR		PMCLOG_EMIT32

#if	_KERNEL
union pmc_md_pmc {
	struct pmc_md_mips24k_pmc	pm_mips24k;
};

#define	PMC_TRAPFRAME_TO_PC(TF)	((TF)->pc)
#define	PMC_TRAPFRAME_TO_FP(TF)	((TF)->tf_usr_lr)
#define	PMC_TRAPFRAME_TO_SP(TF)	((TF)->tf_usr_sp)

/*
 * Prototypes
 */
struct pmc_mdep *pmc_mips24k_initialize(void);
void		pmc_mips24k_finalize(struct pmc_mdep *_md);
#endif /* _KERNEL */

#endif /* !_MACHINE_PMC_MDEP_H_ */
