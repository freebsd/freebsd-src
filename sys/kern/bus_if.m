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
# $FreeBSD$
#

#include <sys/bus.h>

INTERFACE bus;

#
# Default implementations of some methods.
#
CODE {
	static struct resource *
	null_alloc_resource(device_t dev, device_t child,
			    int type, int *rid,
			    u_long start, u_long end,
			    u_long count, u_int flags)
	{
	    return 0;
	}
};

#
# This is called from system code which prints out a description of a
# device.  It should describe the attachment that the child has with
# the parent.  For instance the TurboLaser bus prints which node the
# device is attached to.  See bus_generic_print_child.9 for more 
# information.
# This method returns the number of characters output.
#
METHOD int print_child {
	device_t dev;
	device_t child;
};

# 
# Called for each child device that 
# did not succeed in probing for a
# driver.
#    
METHOD void probe_nomatch {
        device_t dev;
        device_t child;
};

#
# These two methods manage a bus specific set of instance variables of
# a child device.  The intention is that each different type of bus
# defines a set of appropriate instance variables (such as ports and
# irqs for ISA bus etc.)
#
# This information could be given to the child device as a struct but
# that makes it hard for a bus to add or remove variables without
# forcing an edit and recompile for all drivers which may not be
# possible for vendor supplied binary drivers.

#
# Read an instance variable.  Return 0 on success.
#
METHOD int read_ivar {
	device_t dev;
	device_t child;
	int index;
	uintptr_t *result;
};

#
# Write an instance variable.  Return 0 on success.
#
METHOD int write_ivar {
	device_t dev;
	device_t child;
	int index;
	uintptr_t value;
};

#
# Called after the child's DEVICE_DETACH method to allow the parent
# to reclaim any resources allocated on behalf of the child.
#
METHOD void child_detached {
	device_t dev;
	device_t child;
};

#
# Called when a new driver is added to the devclass which owns this
# bus. The generic implementation of this method attempts to probe and
# attach any un-matched children of the bus.
#
METHOD void driver_added {
	device_t dev;
	driver_t *driver;
} DEFAULT bus_generic_driver_added;

#
# For busses which use use drivers supporting DEVICE_IDENTIFY to
# enumerate their devices, these methods are used to create new
# device instances. If place is non-NULL, the new device will be
# added after the last existing child with the same order.
#
METHOD device_t add_child {
	device_t dev;
	int order;
	const char *name;
	int unit;
};

#
# Allocate a system resource attached to `dev' on behalf of `child'.
# The types are defined in <machine/resource.h>; the meaning of the
# resource-ID field varies from bus to bus (but *rid == 0 is always
# valid if the resource type is).  start and end reflect the allowable
# range, and should be passed as `0UL' and `~0UL', respectively, if
# the client has no range restriction.  count is the number of consecutive
# indices in the resource required.  flags is a set of sharing flags
# as defined in <sys/rman.h>.
#
# Returns a resource or a null pointer on failure.  The caller is
# responsible for calling rman_activate_resource() when it actually
# uses the resource.
#
METHOD struct resource * alloc_resource {
	device_t	dev;
	device_t	child;
	int		type;
	int	       *rid;
	u_long		start;
	u_long		end;
	u_long		count;
	u_int		flags;
} DEFAULT null_alloc_resource;

METHOD int activate_resource {
	device_t	dev;
	device_t	child;
	int		type;
	int		rid;
	struct resource *r;
};

METHOD int deactivate_resource {
	device_t	dev;
	device_t	child;
	int		type;
	int		rid;
	struct resource *r;
};

#
# Free a resource allocated by the preceding method.  The `rid' value
# must be the same as the one returned by BUS_ALLOC_RESOURCE (which
# is not necessarily the same as the one the client passed).
#
METHOD int release_resource {
	device_t	dev;
	device_t	child;
	int		type;
	int		rid;
	struct resource *res;
};

METHOD int setup_intr {
	device_t	dev;
	device_t	child;
	struct resource *irq;
	int		flags;
	driver_intr_t	*intr;
	void		*arg;
	void		**cookiep;
};

METHOD int teardown_intr {
	device_t	dev;
	device_t	child;
	struct resource	*irq;
	void		*cookie;
};

#
# Set the range used for a particular resource. Return EINVAL if
# the type or rid are out of range.
#
METHOD int set_resource {
	device_t	dev;
	device_t	child;
	int		type;
	int		rid;
	u_long		start;
	u_long		count;
};

#
# Get the range for a resource. Return ENOENT if the type or rid are
# out of range or have not been set.
#
METHOD int get_resource {
	device_t	dev;
	device_t	child;
	int		type;
	int		rid;
	u_long		*startp;
	u_long		*countp;
};

#
# Delete a resource.
#
METHOD void delete_resource {
	device_t	dev;
	device_t	child;
	int		type;
	int		rid;
};

#
# Return a struct resource_list.
#
METHOD int get_resource_list {
	device_t	dev;
	device_t	child;
	struct resource_list *rl;
} DEFAULT bus_generic_get_resource_list;
