#-
# Copyright (c) 2001, 2003 by Thomas Moestl <tmm@FreeBSD.org>
# Copyright (c) 2004 by Marius Strobl <marius@FreeBSD.org>
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

INTERFACE ofw_bus;

CODE {
	static ofw_bus_get_compat_t ofw_bus_default_get_compat;
	static ofw_bus_get_model_t ofw_bus_default_get_model;
	static ofw_bus_get_name_t ofw_bus_default_get_name;
	static ofw_bus_get_node_t ofw_bus_default_get_node;
	static ofw_bus_get_type_t ofw_bus_default_get_type;

	static const char *
	ofw_bus_default_get_compat(device_t bus, device_t dev)
	{

		return (NULL);
	}

	static const char *
	ofw_bus_default_get_model(device_t bus, device_t dev)
	{

		return (NULL);
	}

	static const char *
	ofw_bus_default_get_name(device_t bus, device_t dev)
	{

		return (NULL);
	}

	static phandle_t
	ofw_bus_default_get_node(device_t bus, device_t dev)
	{

		return (0);
	}

	static const char *
	ofw_bus_default_get_type(device_t bus, device_t dev)
	{

		return (NULL);
	}
};

# Get the alternate firmware name for the device dev on the bus. The default
# method will return NULL, which means the device doesn't have such a property.
METHOD const char * get_compat {
	device_t bus;
	device_t dev;
} DEFAULT ofw_bus_default_get_compat;

# Get the firmware model name for the device dev on the bus. The default method
# will return NULL, which means the device doesn't have such a property.
METHOD const char * get_model {
	device_t bus;
	device_t dev;
} DEFAULT ofw_bus_default_get_model;

# Get the firmware name for the device dev on the bus. The default method will
# return NULL, which means the device doesn't have such a property.
METHOD const char * get_name {
	device_t bus;
	device_t dev;
} DEFAULT ofw_bus_default_get_name;

# Get the firmware node for the device dev on the bus. The default method will
# return 0, which signals that there is no such node.
METHOD phandle_t get_node {
	device_t bus;
	device_t dev;
} DEFAULT ofw_bus_default_get_node;

# Get the firmware device type for the device dev on the bus. The default
# method will return NULL, which means the device doesn't have such a property.
METHOD const char * get_type {
	device_t bus;
	device_t dev;
} DEFAULT ofw_bus_default_get_type;
