/*
 * Copyright (c) 2026 Advanced Micro Devices, Inc.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * AMD/Intel MPERF/APERF MSR counters, and the AMD-only IRPERF counter,
 * exposed as an hwpmc(4) PMC class.
 */

#ifdef  _KERNEL

/*
 * A row per MSR: MPERF, APERF and IRPERF.
 */

#define PERF_NPMCS_TOTAL	3

extern	int	tsc_perf_stat;
extern	u_int	cpu_power_ecx;
extern	u_int	amd_extended_feature_extensions;

/*
 * Prototypes.
 */

int     pmc_perf_initialize(struct pmc_mdep *_md, int maxcpu, int classindex);
void    pmc_perf_finalize(struct pmc_mdep *_md);
#endif  /* _KERNEL */
