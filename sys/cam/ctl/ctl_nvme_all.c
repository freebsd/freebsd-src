/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (C) 2012-2014 Intel Corporation
 * All rights reserved.
 *
 * Copyright (c) 2023 Chelsio Communications, Inc.
 */

#include <sys/types.h>
#include <sys/sbuf.h>
#ifndef _KERNEL
#include <sys/time.h>
#include <stdio.h>
#endif

#include <cam/cam.h>
#include <cam/nvme/nvme_all.h>

#include <cam/ctl/ctl_io.h>
#include <cam/ctl/ctl_nvme_all.h>

void
ctl_nvme_command_string(struct ctl_nvmeio *ctnio, struct sbuf *sb)
{
	nvme_opcode_sbuf(ctnio->io_hdr.io_type == CTL_IO_NVME_ADMIN,
	    ctnio->cmd.opc, sb);
}

void
ctl_nvme_status_string(struct ctl_nvmeio *ctnio, struct sbuf *sb)
{
	nvme_cpl_sbuf(&ctnio->cpl, sb);
}
