/*
 * Copyright (c) 2026 Abdelkader Boudih <seuros@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef _DEV_ASMC_ASMC_DEBUG_H_
#define _DEV_ASMC_ASMC_DEBUG_H_

/*
 * Raw SMC key access and the boot-time key dump, built only with the
 * ASMC_DEBUG option.  Without it the calls compile away, so the core needs
 * no conditionals of its own.
 */
#ifdef ASMC_DEBUG
void	asmc_debug_sysctl_add(device_t dev, struct sysctl_ctx_list *ctx);
void	asmc_dumpall(device_t dev);
#else
static inline void
asmc_debug_sysctl_add(device_t dev __unused,
    struct sysctl_ctx_list *ctx __unused)
{
}

static inline void
asmc_dumpall(device_t dev __unused)
{
}
#endif

#endif /* _DEV_ASMC_ASMC_DEBUG_H_ */
