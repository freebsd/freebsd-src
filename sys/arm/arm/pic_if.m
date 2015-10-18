#-
# Copyright (c) 2012 Jakub Wojciech Klama <jceel@FreeBSD.org>
# Copyright (c) 2015 Svatopluk Kraus
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
# $FreeBSD$
#

#include <sys/bus.h>
#include <sys/cpuset.h>
#include <machine/frame.h>
#include <machine/intr.h>

INTERFACE pic;

CODE {
	static int null_pic_bind(device_t dev, struct arm_irqsrc *isrc)
	{
		return (EOPNOTSUPP);
	}

	static void null_pic_disable_intr(device_t dev, struct arm_irqsrc *isrc)
	{
		return;
	}

	static void null_pic_enable_intr(device_t dev, struct arm_irqsrc *isrc)
	{
		return;
	}

	static void null_pic_init_secondary(device_t dev)
	{
		return;
	}

	static void null_pic_ipi_send(device_t dev, cpuset_t cpus, u_int ipi)
	{
		return;
	}
};

METHOD int register {
	device_t		dev;
	struct arm_irqsrc	*isrc;
	boolean_t		*is_percpu;
};

METHOD int unregister {
	device_t		dev;
	struct arm_irqsrc	*isrc;
};

METHOD void disable_intr {
	device_t		dev;
	struct arm_irqsrc	*isrc;
} DEFAULT null_pic_disable_intr;

METHOD void disable_source {
	device_t		dev;
	struct arm_irqsrc	*isrc;
};

METHOD void enable_source {
	device_t		dev;
	struct arm_irqsrc	*isrc;
};

METHOD void enable_intr {
	device_t		dev;
	struct arm_irqsrc	*isrc;
} DEFAULT null_pic_enable_intr;

METHOD void pre_ithread {
	device_t		dev;
	struct arm_irqsrc	*isrc;
};

METHOD void post_ithread {
	device_t		dev;
	struct arm_irqsrc	*isrc;
};

METHOD void post_filter {
	device_t		dev;
	struct arm_irqsrc	*isrc;
};

METHOD int bind {
	device_t		dev;
	struct arm_irqsrc	*isrc;
} DEFAULT null_pic_bind;

METHOD void init_secondary {
	device_t	dev;
} DEFAULT null_pic_init_secondary;

METHOD void ipi_send {
	device_t		dev;
	struct arm_irqsrc	*isrc;
	cpuset_t		cpus;
} DEFAULT null_pic_ipi_send;
