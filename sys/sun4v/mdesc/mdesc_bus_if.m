#-
# Copyright (c) 2001, 2003 by Thomas Moestl <tmm@FreeBSD.org>
# Copyright (c) 2004, 2005 by Marius Strobl <marius@FreeBSD.org>
# Copyright (c) 2006 by Kip Macy <kmacy@FreeBSD.org>
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
# $FreeBSD: src/sys/sun4v/mdesc/mdesc_bus_if.m,v 1.1 2006/10/05 06:14:27 kmacy Exp $

# Interface for retrieving the package handle and a subset, namely
# 'compatible', 'device_type', 'model' and 'name', of the standard
# properties of a device on an Open Firmware assisted bus for use
# in device drivers. The rest of the standard properties, 'address',
# 'interrupts', 'reg' and 'status', are not covered by this interface
# as they are expected to be only of interest in the respective bus
# driver.

#include <sys/bus.h>

#include <machine/cddl/mdesc.h>

INTERFACE mdesc_bus;

HEADER {
	struct mdesc_bus_devinfo {
		char		*mbd_compat;
		char		*mbd_name;
		char		*mbd_type;
		uint64_t         mbd_handle;
	};
};

CODE {
	static mdesc_bus_get_devinfo_t mdesc_bus_default_get_devinfo;
	static mdesc_bus_get_compat_t mdesc_bus_default_get_compat;
	static mdesc_bus_get_name_t mdesc_bus_default_get_name;
	static mdesc_bus_get_type_t mdesc_bus_default_get_type;

	static const struct mdesc_bus_devinfo *
	mdesc_bus_default_get_devinfo(device_t bus, device_t dev)
	{

		return (NULL);
	}

	static const char *
	mdesc_bus_default_get_compat(device_t bus, device_t dev)
	{

		return (NULL);
	}

	static const char *
	mdesc_bus_default_get_name(device_t bus, device_t dev)
	{

		return (NULL);
	}

	static const char *
	mdesc_bus_default_get_type(device_t bus, device_t dev)
	{

		return (NULL);
	}

	static uint64_t
	mdesc_bus_default_get_handle(device_t bus, device_t dev)
	{

		return (0);
	}
};

# Get the mdesc_bus_devinfo struct for the device dev on the bus. Used for bus
# drivers which use the generic methods in mdesc_bus_subr.c to implement the
# reset of this interface. The default method will return NULL, which means
# there is no such struct associated with the device.
METHOD const struct mdesc_bus_devinfo * get_devinfo {
	device_t bus;
	device_t dev;
} DEFAULT mdesc_bus_default_get_devinfo;

# Get the alternate firmware name for the device dev on the bus. The default
# method will return NULL, which means the device doesn't have such a property.
METHOD const char * get_compat {
	device_t bus;
	device_t dev;
} DEFAULT mdesc_bus_default_get_compat;

# Get the firmware name for the device dev on the bus. The default method will
# return NULL, which means the device doesn't have such a property.
METHOD const char * get_name {
	device_t bus;
	device_t dev;
} DEFAULT mdesc_bus_default_get_name;

# Get the firmware device type for the device dev on the bus. The default
# method will return NULL, which means the device doesn't have such a property.
METHOD const char * get_type {
	device_t bus;
	device_t dev;
} DEFAULT mdesc_bus_default_get_type;

METHOD uint64_t get_handle {
	device_t bus;
	device_t dev;
} DEFAULT mdesc_bus_default_get_handle;
