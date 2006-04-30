#-
# Copyright (c) 2000 Doug Rabson
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
#include <dev/pci/pcivar.h>

INTERFACE pcib;

CODE {
	static int
	null_route_interrupt(device_t pcib, device_t dev, int pin)
	{
		return (PCI_INVALID_IRQ);
	}
};

#
# Return the number of slots on the attached PCI bus.
#
METHOD int maxslots {
	device_t	dev;
};

#
# Read configuration space on the PCI bus. The bus, slot and func
# arguments determine the device which is being read and the reg
# argument is a byte offset into configuration space for that
# device. The width argument (which should be 1, 2 or 4) specifies how
# many byte of configuration space to read from that offset.
#
METHOD u_int32_t read_config {
	device_t	dev;
	u_int		bus;
	u_int		slot;
	u_int		func;
	u_int		reg;
	int		width;
};

#
# Write configuration space on the PCI bus. The bus, slot and func
# arguments determine the device which is being written and the reg
# argument is a byte offset into configuration space for that
# device. The value field is written to the configuration space, with
# the number of bytes written depending on the width argument.
#
METHOD void write_config {
	device_t	dev;
	u_int		bus;
	u_int		slot;
	u_int		func;
	u_int		reg;
	u_int32_t	value;
	int		width;
};

#
# Route an interrupt.  Returns a value suitable for stuffing into 
# a device's interrupt register.
#
METHOD int route_interrupt {
	device_t pcib;
	device_t dev;
	int pin;
} DEFAULT null_route_interrupt;
