#-
# Copyright (c) 2014 Ian Lepore
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
#

#include <sys/types.h>
#include <sys/bus.h>
#include <dev/ofw/openfirm.h>

#
# This is the interface that fdt_pinctrl drivers provide to other drivers.
#

INTERFACE fdt_pinctrl;

CODE {
	static int
	fdt_pinctrl_default_is_gpio(device_t pinctrl, device_t gpio, bool *is_gpio)
	{

		return (EOPNOTSUPP);
	}

	static int
	fdt_pinctrl_default_set_flags(device_t pinctrl, device_t gpio, uint32_t pin,
	    uint32_t flags)
	{

		return (EOPNOTSUPP);
	}

	static int
	fdt_pinctrl_default_get_flags(device_t pinctrl, device_t gpio, uint32_t pin,
	    uint32_t *flags)
	{

		return (EOPNOTSUPP);
	}
};

# Needed for timestamping device probe/attach calls
HEADER {
	#include <sys/tslog.h>
}

#
# Set pins to the specified configuration.  The cfgxref arg is an xref phandle
# to a descendent node (child, grandchild, ...) of the pinctrl device node.
# Returns 0 on success or a standard errno value.
#
PROLOG {
	TSENTER2(device_get_name(pinctrl));
}
EPILOG {
	TSEXIT2(device_get_name(pinctrl));
}
METHOD int configure {
	device_t	pinctrl;
	phandle_t	cfgxref;
};


#
# Test if the pin is in gpio mode
# Called from a gpio device
#
METHOD int is_gpio {
	device_t pinctrl;
	device_t gpio;
	uint32_t pin;
	bool *is_gpio;
} DEFAULT fdt_pinctrl_default_is_gpio;

#
# Set the flags of a pin
# Called from a gpio device
#
METHOD int set_flags {
	device_t pinctrl;
	device_t gpio;
	uint32_t pin;
	uint32_t flags;
} DEFAULT fdt_pinctrl_default_set_flags;

#
# Get the flags of a pin
# Called from a gpio device
#
METHOD int get_flags {
	device_t pinctrl;
	device_t gpio;
	uint32_t pin;
	uint32_t *flags;
} DEFAULT fdt_pinctrl_default_get_flags;
