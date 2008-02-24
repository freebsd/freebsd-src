/*-
 * This file is in the public domain.
 *
 * $FreeBSD: src/sys/sun4v/include/pmc_mdep.h,v 1.1 2006/10/05 06:14:26 kmacy Exp $
 */

#ifndef _MACHINE_PMC_MDEP_H_
#define	_MACHINE_PMC_MDEP_H_

union pmc_md_op_pmcallocate {
	uint64_t		__pad[4];
};

/* Logging */
#define	PMCLOG_READADDR		PMCLOG_READ64
#define	PMCLOG_EMITADDR		PMCLOG_EMIT64

#if	_KERNEL
union pmc_md_pmc {
};

#endif

#endif /* !_MACHINE_PMC_MDEP_H_ */
