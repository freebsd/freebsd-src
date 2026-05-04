#
# Copyright (c) 2026 Justin Hibbits
#
# SPDX-License-Identifier: BSD-2-Clause

#include <machine/bus.h>
#include <dev/dpaa/fman_port.h>

/**
 * @brief DPAA FMan Port interface
 *
 */
INTERFACE fman_port;

/**
 * @brief Configure the port for a specific purpose
 */
METHOD int config {
	device_t	dev;
	struct fman_port_params *params;
};

METHOD int init {
	device_t	dev;
};

METHOD int disable {
	device_t	dev;
};

METHOD int enable {
	device_t	dev;
};
