/*
 * Copyright (c) 2026 Abdelkader Boudih <seuros@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef _DEV_ASMC_ASMC_SYSCTL_H_
#define _DEV_ASMC_ASMC_SYSCTL_H_

/*
 * Sysctl subtree builders, one per dev.asmc.N node.  asmc_attach() calls these
 * in registration order; a node that fails to create just leaves that subtree
 * missing, so none of these are fatal to attach.
 */
void	asmc_sysctl_add_fan(device_t dev, struct sysctl_ctx_list *ctx);
void	asmc_sysctl_add_temp(device_t dev, struct sysctl_ctx_list *ctx);
void	asmc_sysctl_add_light(device_t dev, struct sysctl_ctx_list *ctx);
void	asmc_sysctl_add_sms(device_t dev, struct sysctl_ctx_list *ctx);
void	asmc_sysctl_add_system(device_t dev, struct sysctl_ctx_list *ctx);
void	asmc_sysctl_add_platform(device_t dev, struct sysctl_ctx_list *ctx);

/* Registered by the core's sensor range scan, one OID per detected key. */
int	asmc_sensor_sysctl(SYSCTL_HANDLER_ARGS);

#endif /* _DEV_ASMC_ASMC_SYSCTL_H_ */
