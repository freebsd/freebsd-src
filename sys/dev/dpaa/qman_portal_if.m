#-
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2026 Justin Hibbits

#include <sys/pcpu.h>
#include <machine/bus.h>
#include <dev/dpaa/portals.h>
#include <dev/dpaa/qman.h>
#include <dev/dpaa/qman_var.h>

/**
 * @brief DPAA QMan portal interface
 *
 */
INTERFACE qman_portal;

METHOD int enqueue {
	device_t	dev;
	struct qman_fq	*fq;
	struct dpaa_fd	*fd;
};

METHOD union qman_mc_result * mc_send_raw {
	device_t		dev;
	union qman_mc_command	*cmd;
};

METHOD void static_dequeue_channel {
	device_t	dev;
	int		channel;
}

METHOD void static_dequeue_rm_channel {
	device_t	dev;
	int		channel;
}
