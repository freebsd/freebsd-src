/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023 Chelsio Communications, Inc.
 */

#ifndef __CTL_NVME_ALL_H__
#define	__CTL_NVME_ALL_H__

__BEGIN_DECLS

void	ctl_nvme_command_string(struct ctl_nvmeio *ctnio, struct sbuf *sb);
void	ctl_nvme_status_string(struct ctl_nvmeio *ctnio, struct sbuf *sb);

__END_DECLS

#endif /* !__CTL_NVME_ALL_H__ */
