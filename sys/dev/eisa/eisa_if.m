#-
# Copyright (c) 2004 M. Warner Losh
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
# $FreeBSD: src/sys/dev/eisa/eisa_if.m,v 1.2.18.1 2008/11/25 02:59:29 kensmith Exp $
#

#include <sys/bus.h>
#include <sys/types.h>

INTERFACE eisa;

#
# Add interrupt information for an EISA device.  irq is the irq number
# and trigger is either EISA_TRIGGER_EDGE or EISA_TRIGGER_LEVEL
#
METHOD int add_intr {
	device_t	dev;
	device_t	child;
	int		irq;
	int		trigger;
};

#
# Adds an I/O space to the reservation lis
#
METHOD int add_iospace {
	device_t	dev;
	device_t	child;
	u_long		iobase;
	u_long		iosize;
	int		flags;
};

#
# Adds a memory range to the reservation lis
#
METHOD int add_mspace {
	device_t	dev;
	device_t	child;
	u_long		mbase;
	u_long		msize;
	int		flags;
};
