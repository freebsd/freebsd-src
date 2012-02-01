#-
# Copyright (c) 2012 Oleksandr Tymoshenko <gonzo@freebsd.org>
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

#include <machine/bus.h>

#include <sys/bus.h>
#include <sys/bus_dma.h>

INTERFACE omap4;

#
# Read Generic Interrupt Controller register
#
METHOD uint32_t gic_dist_read {
	device_t dev;
	bus_size_t off;
};

#
# Write GIC register
#
METHOD void gic_dist_write {
	device_t dev;
	bus_size_t off;
	uint32_t val;
};


#
# Read GIC CPU register
#
METHOD uint32_t gic_cpu_read {
	device_t dev;
	bus_size_t off;
};

#
# Write GIC CPU register
#
METHOD void gic_cpu_write {
	device_t dev;
	bus_size_t off;
	uint32_t val;
};


#
# Read global timer register
#
METHOD uint32_t gbl_timer_read {
	device_t dev;
	bus_size_t off;
};

#
# Write global timer register
#
METHOD void gbl_timer_write {
	device_t dev;
	bus_size_t off;
	uint32_t val;
};

#
# Read private timer register
#
METHOD uint32_t prv_timer_read {
	device_t dev;
	bus_size_t off;
};

#
# Write private timer register
#
METHOD void prv_timer_write {
	device_t dev;
	bus_size_t off;
	uint32_t val;
};

