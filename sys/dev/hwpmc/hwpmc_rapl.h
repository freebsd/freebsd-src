/*
 * Copyright (c) 2026 Advanced Micro Devices, Inc.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * AMD/Intel RAPL energy counters exposed as an hwpmc(4) PMC class.
 */

#ifndef _DEV_HWPMC_RAPL_H_
#define	_DEV_HWPMC_RAPL_H_ 1

#ifdef	_KERNEL

struct pmc_mdep;

/*
 * Max RAPL rows per CPU: package, "cores" and (Intel only) DRAM. The actual
 * per-vendor count is determined at initialize time (pcd_num).
 */
#define	RAPL_MAX_NPMCS	3

int	pmc_rapl_initialize(struct pmc_mdep *_md, int _maxcpu, int _classindex);
void	pmc_rapl_finalize(struct pmc_mdep *_md);

#endif	/* _KERNEL */
#endif	/* _DEV_HWPMC_RAPL_H_ */
