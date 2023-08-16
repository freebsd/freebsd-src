#-
# SPDX-License-Identifier: BSD-2-Clause
#:
# Copyright (c) 2020 Ruslan Bukin <br@bsdpad.com>
#
# This software was developed by SRI International and the University of
# Cambridge Computer Laboratory (Department of Computer Science and
# Technology) under DARPA contract HR0011-18-C-0016 ("ECATS"), as part of the
# DARPA SSITH research programme.
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
#

#include "opt_platform.h"

#include <sys/types.h>
#include <sys/taskqueue.h>
#include <sys/bus.h>
#include <sys/sysctl.h>
#include <sys/tree.h>
#include <vm/vm.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/iommu/iommu.h>

#ifdef FDT
#include <dev/fdt/fdt_common.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#endif

INTERFACE iommu;

#
# Check if the iommu controller dev is responsible to serve traffic
# for a given child.
#
METHOD int find {
	device_t		dev;
	device_t		child;
};

#
# Map a virtual address VA to a physical address PA.
#
METHOD int map {
	device_t		dev;
	struct iommu_domain	*iodom;
	vm_offset_t		va;
	vm_page_t		*ma;
	bus_size_t		size;
	vm_prot_t		prot;
};

#
# Unmap a virtual address VA.
#
METHOD int unmap {
	device_t		dev;
	struct iommu_domain	*iodom;
	vm_offset_t		va;
	bus_size_t		size;
};

#
# Allocate an IOMMU domain.
#
METHOD struct iommu_domain * domain_alloc {
	device_t		dev;
	struct iommu_unit	*iommu;
};

#
# Release all the resources held by IOMMU domain.
#
METHOD void domain_free {
	device_t		dev;
	struct iommu_domain	*iodom;
};

#
# Find a domain allocated for a dev.
#
METHOD struct iommu_domain * domain_lookup {
	device_t		dev;
};

#
# Find an allocated context for a device.
#
METHOD struct iommu_ctx * ctx_lookup {
	device_t		dev;
	device_t		child;
};

#
# Allocate a new iommu context.
#
METHOD struct iommu_ctx * ctx_alloc {
	device_t		dev;
	struct iommu_domain	*iodom;
	device_t		child;
	bool			disabled;
};

#
# Initialize the new iommu context.
#
METHOD int ctx_init {
	device_t		dev;
	struct iommu_ctx	*ioctx;
};

#
# Free the iommu context.
#
METHOD void ctx_free {
	device_t		dev;
	struct iommu_ctx	*ioctx;
};

#ifdef FDT
#
# Notify controller we have machine-dependent data.
#
METHOD int ofw_md_data {
	device_t dev;
	struct iommu_ctx *ioctx;
	pcell_t *cells;
	int ncells;
};
#endif
