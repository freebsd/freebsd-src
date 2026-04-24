#-
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2026 Justin Hibbits

#include <machine/bus.h>
#include <dev/dpaa/fman.h>

/**
 * @brief DPAA FMan interface
 *
 */
INTERFACE fman;

METHOD void get_revision {
	device_t	dev;
	int		*major;
	int		*minor;
};

METHOD size_t get_bmi_max_fifo_size {
	device_t	dev;
};

METHOD int get_qman_channel_id {
	device_t	dev;
	int		port_id;
};

METHOD int reset_mac {
	device_t	dev;
	int		mac_id;
};

METHOD int set_port_params {
	device_t			dev;
	struct fman_port_init_params	*params;
};
