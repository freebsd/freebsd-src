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

INTERFACE bus

#
# This is called from system code which prints out a description of a
# device.  It should describe the attachment that the child has with
# the parent.  For instance the TurboLaser bus prints which node the
# device is attached to.
#
METHOD void print_child {
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
	u_long *result;
};

#
# Write an instance variable.  Return 0 on success.
#
METHOD int write_ivar {
	device_t dev;
	device_t child;
	int index;
	u_long value;
};

#
# Register an interrupt handler for the child device.  The handler
# will be called with the value 'arg' as its only argument.
#
METHOD int map_intr {
	device_t dev;
	device_t child;
	driver_intr_t *intr;
	void *arg;
};
