#
# Copyright (c) 1999 M. Warner Losh.
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

INTERFACE card;

#
# Companion interface for pccard.  We need to set attributes for memory
# and i/o port mappings (as well as other types of attributes) that have
# a well defined meaning inside the pccard/cardbus system.  The bus
# methods are inadequate for this because this must be done at the time the
# resources are set for the device, which predates their activation.  Also,
# the driver activating the resources doesn't necessarily know or need to know
# these attributes.
#
METHOD int set_res_flags {
	device_t dev;
	device_t child;
	int	 restype;
	int	 rid;
	u_long	 value;
};

METHOD int get_res_flags {
	device_t dev;
	device_t child;
	int	 restype;
	int	 rid;
	u_long	 *value;
};

#
# Sets the memory offset of the pccard bridge's window into attribute
# or common memory space.
#
METHOD int set_memory_offset {
	device_t  dev;
	device_t  child;
        int	  rid;
        u_int32_t offset;
}

METHOD int get_memory_offset {
	device_t  dev;
	device_t  child;
        int	  rid;
        u_int32_t *offset;
}

#
# pccard bridges call this method to initate the attachment of a card
#
METHOD int attach_card {
	device_t  dev;
}

#
# pccard bridges call this to detach a card.
#
METHOD int detach_card {
	device_t  dev;
	int	  flags;
}

HEADER {
	#define DETACH_FORCE 0x01
}

#
# Returns the type of card this is.  Maybe we don't need this.
#
METHOD int get_type {
	device_t  dev;
	int	  *type;
}

#
# Returns the function number for this device.
#
METHOD int get_function {
	device_t  dev;
	device_t  child;
	int	  *func;
}

#
# Activates (and powers up if necessary) the card's nth function
# since each function gets its own device, there is no need to
# to specify a function number
#
METHOD int activate_function {
	device_t  dev;
	device_t  child;
}

METHOD int deactivate_function {
	device_t  dev;
	device_t  child;
}

#
# Compatibility methods for OLDCARD drivers.  We use these routines to make
# it possible to call the OLDCARD driver's probe routine in the context that
# it expects.  For OLDCARD these are implemented as pass throughs to the
# device_{probe,attach} routines.  For NEWCARD they are implemented such
# such that probe becomes strictly a matching routine and attach does both
# the old probe and old attach.
#
# compat devices should use the following:
#
#	/* Device interface */
#	DEVMETHOD(device_probe),	pccard_compat_probe),
#	DEVMETHOD(device_attach),	pccard_compat_attach),
#	/* Card interface */
#	DEVMETHOD(card_compat_match,	foo_match),	/* newly written */
#	DEVMETHOD(card_compat_probe,	foo_probe),	/* old probe */
#	DEVMETHOD(card_compat_attach,	foo_attach),	/* old attach */
#
# This will allow a single driver binary image to be used for both
# OLDCARD and NEWCARD.
#
# Drivers wishing to not retain OLDCARD compatibility needn't do this.
#
METHOD int compat_probe {
	device_t dev;
}

METHOD int compat_attach {
	device_t dev;
}

#
# Helper method for the above.  When a compatibility driver is converted,
# one must write a match routine.  This routine is unused on OLDCARD but
# is used as a discriminator for NEWCARD.
#
METHOD int compat_match {
	device_t dev;
}
