# Copyright (c) 2001 by Thomas Moestl <tmm@FreeBSD.org>.
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
#include <machine/bus.h>

#include <dev/ofw/openfirm.h>

INTERFACE sparcbus;

HEADER {
	/* Bus tag types for the get_bustag method */
	enum sbbt_id {
		SBBT_IO,
		SBBT_MEM,
	};
};

CODE {
	static int
	sparcbus_default_intr_pending(device_t dev, int intr)
	{

		return (SPARCBUS_INTR_PENDING(device_get_parent(dev), intr));
	}

	static u_int32_t
	sparcbus_default_guess_ino(device_t dev, phandle_t node, u_int slot,
	    u_int pin)
	{

		return (SPARCBUS_GUESS_INO(device_get_parent(dev), node, slot,
		    pin));
	}

	static bus_space_handle_t
	sparcbus_default_get_bus_handle(device_t dev, enum sbbt_id id,
	    bus_space_handle_t childhdl, bus_space_tag_t *tag)
	{

		return (SPARCBUS_GET_BUS_HANDLE(device_get_parent(dev), id,
		    childhdl, tag));
	}
};

# Return whether an interrupt request is pending for the INO intr.
METHOD int intr_pending {
	device_t dev;
	int intr;
} DEFAULT sparcbus_default_intr_pending;

# Let the bus driver guess the INO of the device at the given slot and intpin
# on the bus described by the node if it could not be determined from the
# firmware properties. Returns 255 if no INO could be found (mapping will
# continue at the parent), or the desired INO.
METHOD u_int32_t guess_ino {
	device_t dev;
	phandle_t node;
	u_int slot;
	u_int pin;
} DEFAULT sparcbus_default_guess_ino;

# Get the bustag for the root bus. This is needed for ISA old-stlye
# in[bwl]()/out[bwl]() support, where no tag retrieved from a resource is
# passed. The returned tag is used to construct a tag for the whole ISA bus.
METHOD bus_space_handle_t get_bus_handle {
	device_t dev;
	enum sbbt_id id;
	bus_space_handle_t childhdl;
	bus_space_tag_t *tag;
} DEFAULT sparcbus_default_get_bus_handle;
