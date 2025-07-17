#-
# Copyright (c) 2016 The FreeBSD Foundation
#
# This software was developed by Andrew Turner under
# sponsorship from the FreeBSD Foundation.
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
# ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
#

INTERFACE msi;

HEADER {
	#include <machine/bus.h>
	#include <dev/iommu/iommu_msi.h>

	struct intr_irqsrc;
};

#
# Default implementations of some methods.
#
CODE {
	static int
	iommu_init(device_t dev, device_t child, struct iommu_domain **domain)
	{
		*domain = NULL;
		return (0);
	}
	static void
	iommu_deinit(device_t dev, device_t child)
	{
	}
};

METHOD int alloc_msi {
	device_t	dev;
	device_t	child;
	int		count;
	int		maxcount;
	device_t	*pic;
	struct intr_irqsrc **srcs;
};

METHOD int release_msi {
	device_t	dev;
	device_t	child;
	int		count;
	struct intr_irqsrc **srcs;
};

METHOD int alloc_msix {
	device_t	dev;
	device_t	child;
	device_t	*pic;
	struct intr_irqsrc **src;
};

METHOD int release_msix {
	device_t	dev;
	device_t	child;
	struct intr_irqsrc *src;
};

METHOD int map_msi {
	device_t	dev;
	device_t	child;
	struct intr_irqsrc *src;
	uint64_t	*addr;
	uint32_t	*data;
};

METHOD int iommu_init {
	device_t	dev;
	device_t	child;
	struct iommu_domain **domain;
} DEFAULT iommu_init;

METHOD void iommu_deinit {
	device_t	dev;
	device_t	child;
} DEFAULT iommu_deinit;
