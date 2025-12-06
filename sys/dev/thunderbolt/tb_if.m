#-
# SPDX-License-Identifier: BSD-2-Clause-FreeBSD
#
# Copyright (c) 2022 Scott Long
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
# $FreeBSD$
#

#include <sys/bus.h>
#include <sys/types.h>
#include <dev/thunderbolt/tb_reg.h>

INTERFACE tb;

CODE {
	struct nhi_softc;

	int
	tb_generic_find_ufp(device_t dev, device_t *ufp)
	{
		device_t parent;

		parent = device_get_parent(dev);
		if (parent == NULL)
			return (EOPNOTSUPP);

		return (TB_FIND_UFP(parent, ufp));
	}

	int
	tb_generic_get_debug(device_t dev, u_int *debug)
	{
		device_t parent;

		parent = device_get_parent(dev);
		if (parent == NULL)
			return (EOPNOTSUPP);

		return (TB_GET_DEBUG(parent, debug));
	}

}

HEADER {
	struct nhi_softc;

	struct tb_lcmbox_cmd {
		uint32_t cmd;
		uint32_t cmd_resp;
		uint32_t data_in;
		uint32_t data_out;
	};

	int tb_generic_find_ufp(device_t, device_t *);
	int tb_generic_get_debug(device_t, u_int *);
}

#
# Read the LC Mailbox
#
METHOD int lc_mailbox {
	device_t	dev;
	struct tb_lcmbox_cmd	*cmd;
};

#
# Read from the PCIE2CIO port
#
METHOD int pcie2cio_read {
	device_t	dev;
	u_int		space;
	u_int		port;
	u_int		index;
	uint32_t	*val;
}

#
# Write to the PCIE2CIO port
#
METHOD int pcie2cio_write {
	device_t	dev;
	u_int		space;
	u_int		port;
	u_int		index;
	uint32_t	val;
}

#
# Return the device that's the upstream facing port
#
METHOD int find_ufp {
	device_t dev;
	device_t *ufp;
} DEFAULT tb_generic_find_ufp;

METHOD int get_debug {
	device_t dev;
	u_int *debug;
} DEFAULT tb_generic_get_debug;
