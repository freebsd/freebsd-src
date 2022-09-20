#-
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright © 2021-2022 Dmitry Salychev
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

#include <machine/bus.h>
#include <dev/dpaa2/dpaa2_mc.h>

/**
 * @brief Interface of the DPAA2 Management Complex (MC) bus driver.
 *
 * It helps to manipulate DPAA2-specific resources (DPIOs, DPBPs, etc.)
 */
INTERFACE dpaa2_mc;

#
# Default implementation of the commands.
#
CODE {
	static int
	bypass_manage_dev(device_t dev, device_t dpaa2_dev, uint32_t flags)
	{
		if (device_get_parent(dev) != NULL)
			return (DPAA2_MC_MANAGE_DEV(device_get_parent(dev),
				dpaa2_dev, flags));
		return (ENXIO);
	}

	static int
	bypass_get_free_dev(device_t dev, device_t *dpaa2_dev,
		enum dpaa2_dev_type devtype)
	{
		if (device_get_parent(dev) != NULL)
			return (DPAA2_MC_GET_FREE_DEV(device_get_parent(dev),
				dpaa2_dev, devtype));
		return (ENXIO);
	}

	static int
	bypass_get_dev(device_t dev, device_t *dpaa2_dev,
		enum dpaa2_dev_type devtype, uint32_t obj_id)
	{
		if (device_get_parent(dev) != NULL)
			return (DPAA2_MC_GET_DEV(device_get_parent(dev),
				dpaa2_dev, devtype, obj_id));
		return (ENXIO);
	}

	static int
	bypass_get_shared_dev(device_t dev, device_t *dpaa2_dev,
		enum dpaa2_dev_type devtype)
	{
		if (device_get_parent(dev) != NULL)
			return (DPAA2_MC_GET_SHARED_DEV(device_get_parent(dev),
				dpaa2_dev, devtype));
		return (ENXIO);
	}

	static int
	bypass_reserve_dev(device_t dev, device_t dpaa2_dev,
		enum dpaa2_dev_type devtype)
	{
		if (device_get_parent(dev) != NULL)
			return (DPAA2_MC_RESERVE_DEV(device_get_parent(dev),
				dpaa2_dev, devtype));
		return (ENXIO);
	}

	static int
	bypass_release_dev(device_t dev, device_t dpaa2_dev,
		enum dpaa2_dev_type devtype)
	{
		if (device_get_parent(dev) != NULL)
			return (DPAA2_MC_RELEASE_DEV(device_get_parent(dev),
				dpaa2_dev, devtype));
		return (ENXIO);
	}

	static int
	bypass_get_phy_dev(device_t dev, device_t *phy_dev, uint32_t id)
	{
		if (device_get_parent(dev) != NULL)
			return (DPAA2_MC_GET_PHY_DEV(device_get_parent(dev),
			    phy_dev, id));
		return (ENXIO);
	}
}

METHOD int manage_dev {
	device_t	 dev;
	device_t	 dpaa2_dev;
	uint32_t	 flags;
} DEFAULT bypass_manage_dev;

METHOD int get_free_dev {
	device_t	 dev;
	device_t	*dpaa2_dev;
	enum dpaa2_dev_type devtype;
} DEFAULT bypass_get_free_dev;

METHOD int get_dev {
	device_t	 dev;
	device_t	*dpaa2_dev;
	enum dpaa2_dev_type devtype;
	uint32_t	 obj_id;
} DEFAULT bypass_get_dev;

METHOD int get_shared_dev {
	device_t	 dev;
	device_t	*dpaa2_dev;
	enum dpaa2_dev_type devtype;
} DEFAULT bypass_get_shared_dev;

METHOD int reserve_dev {
	device_t	 dev;
	device_t	 dpaa2_dev;
	enum dpaa2_dev_type devtype;
} DEFAULT bypass_reserve_dev;

METHOD int release_dev {
	device_t	 dev;
	device_t	 dpaa2_dev;
	enum dpaa2_dev_type devtype;
} DEFAULT bypass_release_dev;

METHOD int get_phy_dev {
	device_t	 dev;
	device_t	 *phy_dev;
	uint32_t	 id;
} DEFAULT bypass_get_phy_dev;
