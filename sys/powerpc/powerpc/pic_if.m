#-
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
# from: src/sys/kern/bus_if.m,v 1.21 2002/04/21 11:16:10 markm Exp
# $FreeBSD$
#

#include <sys/bus.h>

INTERFACE pic;

METHOD struct resource * allocate_intr {
	device_t	dev;
	device_t	child;
	int		*rid;
	u_long		intr;
	u_int		flags;
};

METHOD int setup_intr {
	device_t	dev;
	device_t	child;
	struct		resource *res;
	int		flags;
	driver_filter_t	*filter;
	driver_intr_t	*intr;
	void		*arg;
	void		**cookiep;
};

METHOD int teardown_intr {
	device_t	dev;
	device_t	child;
	struct		resource *res;
	void		*ih;
};

METHOD int release_intr {
	device_t	dev;
	device_t	child;
	int		rid;
	struct		resource *res;
};
