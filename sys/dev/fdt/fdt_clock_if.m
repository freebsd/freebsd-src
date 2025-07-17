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

#
# This is the interface that fdt_clock drivers provide to other drivers.
# In this context, clock refers to a clock signal provided to some other
# hardware component within the system.  They are most often found within
# embedded processors that have on-chip IO controllers.
#

INTERFACE fdt_clock;

HEADER {

	enum {
		FDT_CIFLAG_RUNNING =	0x01,
	};

	struct fdt_clock_info {
		device_t	provider;
		uint32_t	index;
		const char *	name;         /* May be "", will not be NULL. */
		uint32_t	flags;
		uint64_t	frequency;    /* In Hz. */
	};
}

#
# Enable the specified clock.
# Returns 0 on success or a standard errno value.
#
METHOD int enable {
	device_t	provider;
	int		index;
};

#
# Disable the specified clock.
# Returns 0 on success or a standard errno value.
#
METHOD int disable {
	device_t	provider;
	int		index;
};

#
# Returns information about the current operational state of specified clock.
#
METHOD int get_info {
	device_t	provider;
	int		index;
	struct fdt_clock_info *info;
};

