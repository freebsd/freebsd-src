#-
# Copyright (c) 2015 Michal Meloun
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

#include <machine/bus.h>

INTERFACE syscon;

HEADER {
	struct syscon;
	int syscon_get_handle_default(device_t dev, struct syscon **syscon);
}

CODE {
	#include <sys/systm.h>
	#include <sys/bus.h>

	int
	syscon_get_handle_default(device_t dev, struct syscon **syscon)
	{
		device_t parent;

		parent = device_get_parent(dev);
		if (parent == NULL)
			return (ENODEV);
		return (SYSCON_GET_HANDLE(parent, syscon));
	}

	static void
	syscon_device_lock_default(device_t dev)
	{

		panic("syscon_device_lock is not implemented");
	};

	static void
	syscon_device_unlock_default(device_t dev)
	{

		panic("syscon_device_unlock is not implemented");
	};
}

METHOD int init {
	struct syscon	*syscon;
};

METHOD int uninit {
	struct syscon	*syscon;
};

/**
 * Accessor functions for syscon register space
 */
METHOD uint32_t read_4 {
	struct syscon	*syscon;
	bus_size_t	offset;
};

METHOD int write_4 {
	struct syscon	*syscon;
	bus_size_t	offset;
	uint32_t	val;
};

METHOD int modify_4 {
	struct syscon	*syscon;
	bus_size_t	offset;
	uint32_t	clear_bits;
	uint32_t	set_bits;
};

/**
 * Unlocked verion of access function
 */
METHOD uint32_t unlocked_read_4 {
	struct syscon	*syscon;
	bus_size_t	offset;
};

METHOD int unlocked_write_4 {
	struct syscon	*syscon;
	bus_size_t	offset;
	uint32_t	val;
};

METHOD int unlocked_modify_4 {
	struct syscon	*syscon;
	bus_size_t	offset;
	uint32_t	clear_bits;
	uint32_t	set_bits;
};

/**
* Locking for exclusive access to underlying device
*/
METHOD void device_lock {
	device_t	dev;
} DEFAULT syscon_device_lock_default;

METHOD void device_unlock {
	device_t	dev;
} DEFAULT syscon_device_unlock_default;

/**
 * Get syscon handle from parent driver
 */
METHOD int get_handle {
	device_t	dev;
	struct syscon	**syscon;
} DEFAULT syscon_get_handle_default;
