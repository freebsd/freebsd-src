#-
# Copyright (c) 2017, Bryan Venteicher <bryanv@FreeBSD.org>
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

#include <sys/bus.h>
#include <machine/bus.h>

INTERFACE virtio_pci;

HEADER {
struct virtqueue;
struct vtpci_interrupt;
};

METHOD uint8_t read_isr {
	device_t	dev;
};

METHOD uint16_t get_vq_size {
	device_t	dev;
	int		idx;
};

METHOD bus_size_t get_vq_notify_off {
	device_t	dev;
	int		idx;
};

METHOD void set_vq {
	device_t		dev;
	struct virtqueue	*vq;
};

METHOD void disable_vq {
	device_t		 dev;
	int			 idx;
};

METHOD int register_cfg_msix {
	device_t	dev;
	struct vtpci_interrupt *intr;
};

METHOD int register_vq_msix {
	device_t		dev;
	int			idx;
	struct vtpci_interrupt	*intr;
};
