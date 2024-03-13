/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright © 2021-2022 Dmitry Salychev
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef	_DPAA2_MC_H
#define	_DPAA2_MC_H

#include <sys/rman.h>
#include <sys/bus.h>
#include <sys/queue.h>

#include <net/ethernet.h>

#include <dev/ofw/openfirm.h>

#include "pci_if.h"

#include "dpaa2_types.h"
#include "dpaa2_mcp.h"
#include "dpaa2_swp.h"
#include "dpaa2_ni.h"
#include "dpaa2_io.h"
#include "dpaa2_mac.h"
#include "dpaa2_con.h"
#include "dpaa2_bp.h"

/*
 * Maximum number of MSIs supported by the MC for its children without IOMMU.
 *
 * TODO: Should be much more with IOMMU translation.
 */
#define DPAA2_MC_MSI_COUNT	 32

/* Flags for DPAA2 devices as resources. */
#define DPAA2_MC_DEV_ALLOCATABLE 0x01u /* to be managed by DPAA2-specific rman */
#define DPAA2_MC_DEV_ASSOCIATED	 0x02u /* to obtain info about DPAA2 device  */
#define DPAA2_MC_DEV_SHAREABLE	 0x04u /* to be shared among DPAA2 devices */

struct dpaa2_mc_devinfo; /* about managed DPAA2 devices */

/**
 * @brief Software context for the DPAA2 Management Complex (MC) driver.
 *
 * dev:		Device associated with this software context.
 * rcdev:	Child device associated with the root resource container.
 * acpi_based:	Attached using ACPI (true) or FDT (false).
 * ofw_node:	FDT node of the Management Complex (acpi_based == false).
 *
 * res:		Unmapped MC command portal and control registers resources.
 * map:		Mapped MC command portal and control registers resources.
 *
 * dpio_rman:	I/O objects resource manager.
 * dpbp_rman:	Buffer Pools resource manager.
 * dpcon_rman:	Concentrators resource manager.
 * dpmcp_rman:	MC portals resource manager.
 */
struct dpaa2_mc_softc {
	device_t		 dev;
	device_t		 rcdev;
	bool			 acpi_based;
	phandle_t		 ofw_node;

	struct resource 	*res[2];
	struct resource_map	 map[2];

	/* For allocatable managed DPAA2 objects. */
	struct rman		 dpio_rman;
	struct rman		 dpbp_rman;
	struct rman		 dpcon_rman;
	struct rman		 dpmcp_rman;

	/* For managed DPAA2 objects. */
	struct mtx		 mdev_lock;
	STAILQ_HEAD(, dpaa2_mc_devinfo) mdev_list;

	/* NOTE: Workaround in case of no IOMMU available. */
#ifndef IOMMU
	device_t		 msi_owner;
	bool			 msi_allocated;
	struct mtx		 msi_lock;
	struct {
		device_t	 child;
		int		 irq;
	} msi[DPAA2_MC_MSI_COUNT];
#endif
};

/**
 * @brief Software context for the DPAA2 Resource Container (RC) driver.
 *
 * dev:		Device associated with this software context.
 * portal:	Helper object to send commands to the MC portal.
 * unit:	Helps to distinguish between root (0) and child DRPCs.
 * cont_id:	Container ID.
 */
struct dpaa2_rc_softc {
	device_t		 dev;
	int			 unit;
	uint32_t		 cont_id;
};

/**
 * @brief Information about MSI messages supported by the DPAA2 object.
 *
 * msi_msgnum:	 Number of MSI messages supported by the DPAA2 object.
 * msi_alloc:	 Number of MSI messages allocated for the DPAA2 object.
 * msi_handlers: Number of MSI message handlers configured.
 */
struct dpaa2_msinfo {
	uint8_t			 msi_msgnum;
	uint8_t			 msi_alloc;
	uint32_t		 msi_handlers;
};

/**
 * @brief Information about DPAA2 device.
 *
 * pdev:	Parent device.
 * dev:		Device this devinfo is associated with.
 *
 * id:		ID of a logical DPAA2 object resource.
 * portal_id:	ID of the MC portal which belongs to the object's container.
 * icid:	Isolation context ID of the DPAA2 object. It is shared
 *		between a resource container and all of its children.
 *
 * dtype:	Type of the DPAA2 object.
 * resources:	Resources available for this DPAA2 device.
 * msi:		Information about MSI messages supported by the DPAA2 object.
 */
struct dpaa2_devinfo {
	device_t		 pdev;
	device_t		 dev;

	uint32_t		 id;
	uint32_t		 portal_id;
	uint32_t		 icid;

	enum dpaa2_dev_type	 dtype;
	struct resource_list	 resources;
	struct dpaa2_msinfo	 msi;

	/*
	 * DPAA2 object might or might not have its own portal allocated to
	 * execute MC commands. If the portal has been allocated, it takes
	 * precedence over the portal owned by the resource container.
	 */
	struct dpaa2_mcp	*portal;
};

DECLARE_CLASS(dpaa2_mc_driver);

/* For device interface. */

int dpaa2_mc_attach(device_t dev);
int dpaa2_mc_detach(device_t dev);

/* For bus interface. */

struct rman *dpaa2_mc_rman(device_t mcdev, int type, u_int flags);
struct resource * dpaa2_mc_alloc_resource(device_t mcdev, device_t child,
    int type, int *rid, rman_res_t start, rman_res_t end, rman_res_t count,
    u_int flags);
int dpaa2_mc_adjust_resource(device_t mcdev, device_t child,
    struct resource *r, rman_res_t start, rman_res_t end);
int dpaa2_mc_release_resource(device_t mcdev, device_t child,
    struct resource *r);
int dpaa2_mc_activate_resource(device_t mcdev, device_t child,
    struct resource *r);
int dpaa2_mc_deactivate_resource(device_t mcdev, device_t child,
    struct resource *r);

/* For pseudo-pcib interface. */

int dpaa2_mc_alloc_msi(device_t mcdev, device_t child, int count, int maxcount,
    int *irqs);
int dpaa2_mc_release_msi(device_t mcdev, device_t child, int count, int *irqs);
int dpaa2_mc_map_msi(device_t mcdev, device_t child, int irq, uint64_t *addr,
    uint32_t *data);
int dpaa2_mc_get_id(device_t mcdev, device_t child, enum pci_id_type type,
    uintptr_t *id);

/* For DPAA2 MC bus interface. */

int dpaa2_mc_manage_dev(device_t mcdev, device_t dpaa2_dev, uint32_t flags);
int dpaa2_mc_get_free_dev(device_t mcdev, device_t *dpaa2_dev,
    enum dpaa2_dev_type devtype);
int dpaa2_mc_get_dev(device_t mcdev, device_t *dpaa2_dev,
    enum dpaa2_dev_type devtype, uint32_t obj_id);
int dpaa2_mc_get_shared_dev(device_t mcdev, device_t *dpaa2_dev,
    enum dpaa2_dev_type devtype);
int dpaa2_mc_reserve_dev(device_t mcdev, device_t dpaa2_dev,
    enum dpaa2_dev_type devtype);
int dpaa2_mc_release_dev(device_t mcdev, device_t dpaa2_dev,
    enum dpaa2_dev_type devtype);

#endif /* _DPAA2_MC_H */
