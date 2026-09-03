/*
 * Copyright (c) 2026 Advanced Micro Devices, Inc.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * AMD/Intel MPERF and APERF MSRs counters exposed as an hwpmc(4) PMC class.
 */

#ifdef  _KERNEL

/*
 * A row per MSR: MPERF and APERF.
 */

#define PERF_NPMCS 2
#define PERF_MPERF 0
#define PERF_APERF 1

extern int      tsc_perf_stat;
extern  u_int   cpu_power_ecx;

/*
 * Prototypes.
 */

int     pmc_perf_initialize(struct pmc_mdep *_md, int maxcpu, int classindex);
void    pmc_perf_finalize(struct pmc_mdep *_md);
#endif  /* _KERNEL */
