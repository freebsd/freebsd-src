#
# Copyright (c) 1998 Doug Rabson
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
# $FreeBSD: src/sys/kern/device_if.m,v 1.7 1999/08/28 00:46:09 peter Exp $
#

INTERFACE device;

#
# Default implementations of some methods.
#
CODE {
	static int null_shutdown(device_t dev)
	{
	    return 0;
	}

	static int null_suspend(device_t dev)
	{
	    return 0;
	}

	static int null_resume(device_t dev)
	{
	    return 0;
	}
};

#
# Probe to see if the device is present.  Return 0 if the device exists,
# ENXIO if it cannot be found. If some other error happens during the
# probe (such as a memory allocation failure), an appropriate error code
# should be returned. For cases where more than one driver matches a
# device, a priority value can be returned.  In this case, success codes
# are values less than or equal to zero with the highest value representing
# the best match.  Failure codes are represented by positive values and
# the regular unix error codes should be used for the purpose.

# If a driver returns a success code which is less than zero, it must
# not assume that it will be the same driver which is attached to the
# device. In particular, it must not assume that any values stored in
# the softc structure will be available for its attach method and any
# resources allocated during probe must be released and re-allocated
# if the attach method is called.  If a success code of zero is
# returned, the driver can assume that it will be the one attached.
# 
# Devices which implement busses should use this method to probe for
# the existence of devices attached to the bus and add them as
# children.  If this is combined with the use of bus_generic_attach,
# the child devices will be automatically probed and attached.
#
METHOD int probe {
	device_t dev;
};

#
# Called by a parent bus to add new devices to the bus.
#
STATICMETHOD void identify {
	driver_t *driver;
	device_t parent;
};

#
# Attach a device to the system.  The probe method will have been
# called and will have indicated that the device exists.  This routine
# should initialise the hardware and allocate other system resources
# (such as devfs entries).  Returns 0 on success.
#
METHOD int attach {
	device_t dev;
};

#
# Detach a device.  This can be called if the user is replacing the
# driver software or if a device is about to be physically removed
# from the system (e.g. for pccard devices).  Returns 0 on success.
#
METHOD int detach {
	device_t dev;
};

#
# This is called during system shutdown to allow the driver to put the 
# hardware into a consistent state for rebooting the computer.
#
METHOD int shutdown {
	device_t dev;
} DEFAULT null_shutdown;

#
# This is called by the power-management subsystem when a suspend has been
# requested by the user or by some automatic mechanism.  This gives
# drivers a chance to veto the suspend or save their configuration before
# power is removed.
#
METHOD int suspend {
	device_t dev;
} DEFAULT null_suspend;

METHOD int resume {
	device_t dev;
} DEFAULT null_resume;
