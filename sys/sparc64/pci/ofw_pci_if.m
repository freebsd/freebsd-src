#-
# Copyright (c) 2001, 2003 by Thomas Moestl <tmm@FreeBSD.org>
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
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
# INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
# USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# $FreeBSD$

#include <sys/bus.h>

#include <dev/ofw/openfirm.h>

#include <sparc64/pci/ofw_pci.h>

INTERFACE ofw_pci;

CODE {
	static ofw_pci_intr_pending_t ofw_pci_default_intr_pending;
	static ofw_pci_alloc_busno_t ofw_pci_default_alloc_busno;
	static ofw_pci_adjust_busrange_t ofw_pci_default_adjust_busrange;

	static int
	ofw_pci_default_intr_pending(device_t dev, ofw_pci_intr_t intr)
	{

		if (device_get_parent(dev) != NULL)
			return (OFW_PCI_INTR_PENDING(device_get_parent(dev),
			    intr));
		return (0);
	}

	static int
	ofw_pci_default_alloc_busno(device_t dev)
	{

		if (device_get_parent(dev) != NULL)
			return (OFW_PCI_ALLOC_BUSNO(device_get_parent(dev)));
		return (-1);
	}

	static void
	ofw_pci_default_adjust_busrange(device_t dev, u_int busno)
	{

		if (device_get_parent(dev) != NULL)
			return (OFW_PCI_ADJUST_BUSRANGE(device_get_parent(dev),
			    busno));
	}
};

# Return whether an interrupt request is pending for the INO intr.
METHOD int intr_pending {
	device_t dev;
	ofw_pci_intr_t intr;
} DEFAULT ofw_pci_default_intr_pending;

# Allocate a bus number for reenumerating a PCI bus. A return value of -1
# means that reenumeration is generally not supported, otherwise all PCI
# busses must be reenumerated using bus numbers obtained via this method.
METHOD int alloc_busno {
	device_t dev;
} DEFAULT ofw_pci_default_alloc_busno;

# Make sure that all PCI bridges up in the hierarchy contain this bus in
# their subordinate bus range. This is required when reenumerating the PCI
# buses.
METHOD void adjust_busrange {
	device_t dev;
	u_int subbus;
} DEFAULT ofw_pci_default_adjust_busrange;
