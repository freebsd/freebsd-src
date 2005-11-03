/*-
 * This file is in the public domain.
 *
 * $FreeBSD: src/sys/arm/include/pmc_mdep.h,v 1.2 2005/06/09 19:45:06 jkoshy Exp $
 */

#ifndef _MACHINE_PMC_MDEP_H_
#define	_MACHINE_PMC_MDEP_H_

union pmc_md_op_pmcallocate {
	uint64_t	__pad[4];
};

/* Logging */
#define	PMCLOG_READADDR		PMCLOG_READ32
#define	PMCLOG_EMITADDR		PMCLOG_EMIT32

#if	_KERNEL
union pmc_md_pmc {
};

#endif

#endif /* !_MACHINE_PMC_MDEP_H_ */
