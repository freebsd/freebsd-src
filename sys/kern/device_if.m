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
#	$Id$
#

INTERFACE device

#
# Probe to see if the device is present.  Return 0 if the device exists,
# ENXIO if it cannot be found.
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
};
