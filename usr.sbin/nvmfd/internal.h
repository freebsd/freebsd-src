/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023-2024 Chelsio Communications, Inc.
 * Written by: John Baldwin <jhb@FreeBSD.org>
 */

#ifndef __INTERNAL_H__
#define	__INTERNAL_H__

#include <stdbool.h>

struct controller;
struct nvme_command;
struct nvme_controller_data;
struct nvme_ns_list;
struct nvmf_capsule;
struct nvmf_qpair;

typedef bool handle_command(const struct nvmf_capsule *,
    const struct nvme_command *, void *);

extern bool data_digests;
extern bool header_digests;
extern bool flow_control_disable;
extern bool kernel_io;

/* controller.c */
void	controller_handle_admin_commands(struct controller *c,
    handle_command *cb, void *cb_arg);
struct controller *init_controller(struct nvmf_qpair *qp,
    const struct nvme_controller_data *cdata);
void	free_controller(struct controller *c);

/* discovery.c */
void	init_discovery(void);
void	handle_discovery_socket(int s);
void	discovery_add_io_controller(int s, const char *subnqn);

/* io.c */
void	init_io(const char *subnqn);
void	handle_io_socket(int s);
void	shutdown_io(void);

/* devices.c */
void	register_devices(int ac, char **av);
u_int	device_count(void);
void	device_active_nslist(uint32_t nsid, struct nvme_ns_list *nslist);
bool	device_identification_descriptor(uint32_t nsid, void *buf);
bool	device_namespace_data(uint32_t nsid, struct nvme_namespace_data *nsdata);
void	device_read(uint32_t nsid, uint64_t lba, u_int nlb,
    const struct nvmf_capsule *nc);
void	device_write(uint32_t nsid, uint64_t lba, u_int nlb,
    const struct nvmf_capsule *nc);
void	device_flush(uint32_t nsid, const struct nvmf_capsule *nc);

/* ctl.c */
void	init_ctl_port(const char *subnqn,
    const struct nvmf_association_params *params);
void	ctl_handoff_qpair(struct nvmf_qpair *qp,
    const struct nvmf_fabric_connect_cmd *cmd,
    const struct nvmf_fabric_connect_data *data);
void	shutdown_ctl_port(const char *subnqn);

#endif /* !__INTERNAL_H__ */
