#
# Copyright (c) 2004 Nate Lawson
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
#include "acpi.h"

INTERFACE acpi;

#
# Default implementation for the probe method.
#
CODE {
	static char *
	acpi_generic_id_probe(device_t bus, device_t dev, char **ids)
	{
		return (NULL);
	}
};

#
# Probe
#
METHOD char * id_probe {
	device_t	bus;
	device_t	dev;
	char		**ids;
} DEFAULT acpi_generic_id_probe;

#
# AcpiEvaluateObject
#
METHOD ACPI_STATUS evaluate_object {
	device_t	bus;
	device_t	dev;
	ACPI_STRING 	pathname;
	ACPI_OBJECT_LIST *parameters;
	ACPI_BUFFER	*ret;
};

#
# AcpiWalkNamespace
#
METHOD ACPI_STATUS walk_namespace {
	device_t	bus;
	device_t	dev;
	ACPI_OBJECT_TYPE type;
	UINT32		max_depth;
	ACPI_WALK_CALLBACK user_fn;
	void		*context;
	void		**ret;
};
