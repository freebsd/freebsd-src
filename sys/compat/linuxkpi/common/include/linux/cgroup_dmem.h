/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The FreeBSD Foundation
 */

#ifndef _LINUXKPI_LINUX_CGROUP_DMEM_H_
#define	_LINUXKPI_LINUX_CGROUP_DMEM_H_

#include <linux/types.h>
#include <linux/llist.h>

struct dmem_cgroup_pool_state;
struct dmem_cgroup_region;

static inline __printf(2,3) struct dmem_cgroup_region *
dmem_cgroup_register_region(uint64_t size, const char *name_fmt, ...)
{
	return (NULL);
}

static inline void
dmem_cgroup_unregister_region(struct dmem_cgroup_region *region)
{
}

static inline int
dmem_cgroup_try_charge(struct dmem_cgroup_region *region, u64 size,
    struct dmem_cgroup_pool_state **ret_pool,
    struct dmem_cgroup_pool_state **ret_limit_pool)
{
	*ret_pool = NULL;

	if (ret_limit_pool)
		*ret_limit_pool = NULL;

	return (0);
}

static inline void
dmem_cgroup_uncharge(struct dmem_cgroup_pool_state *pool, uint64_t size)
{
}

static inline
bool dmem_cgroup_state_evict_valuable(struct dmem_cgroup_pool_state *limit_pool,
    struct dmem_cgroup_pool_state *test_pool,
    bool ignore_low, bool *ret_hit_low)
{
	return (true);
}

static inline void
dmem_cgroup_pool_state_put(struct dmem_cgroup_pool_state *pool)
{
}

#endif /* _LINUXKPI_LINUX_CGROUP_DMEM_H_ */
