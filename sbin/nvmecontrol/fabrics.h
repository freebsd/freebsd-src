/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023-2024 Chelsio Communications, Inc.
 * Written by: John Baldwin <jhb@FreeBSD.org>
 */

#ifndef __FABRICS_H__
#define	__FABRICS_H__

/*
 * Splits 'in_address' into separate 'address' and 'port' strings.  If
 * a separate buffer for the address was allocated, 'tofree' is set to
 * the allocated buffer, otherwise 'tofree' is set to NULL.
 */
void	nvmf_parse_address(const char *in_address, const char **address,
    const char **port, char **tofree);

uint16_t nvmf_parse_cntlid(const char *cntlid);

/* Returns true if able to open a connection. */
bool	tcp_qpair_params(struct nvmf_qpair_params *params, int adrfam,
    const char *address, const char *port);

/* Connect to a discovery controller and return the Admin qpair. */
struct nvmf_qpair *connect_discovery_adminq(enum nvmf_trtype trtype,
    const char *address, const char *port, const char *hostnqn);

/*
 * Connect to an NVM controller establishing an Admin qpair and one or
 * more I/O qpairs.  The controller's controller data is returned in
 * *cdata on success.  Returns a non-zero value from <sysexits.h> on
 * failure.
 */
int	connect_nvm_queues(const struct nvmf_association_params *aparams,
    enum nvmf_trtype trtype, int adrfam, const char *address,
    const char *port, uint16_t cntlid, const char *subnqn, const char *hostnqn,
    uint32_t kato, struct nvmf_qpair **admin, struct nvmf_qpair **io,
    u_int num_io_queues, u_int queue_size, struct nvme_controller_data *cdata);

#endif /* !__FABRICS_H__ */
