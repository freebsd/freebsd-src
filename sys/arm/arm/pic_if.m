#-
# Copyright (c) 2012 Jakub Wojciech Klama <jceel@FreeBSD.org>
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
#include <dev/ofw/openfirm.h>

INTERFACE pic;

CODE {
	static int null_pic_translate(device_t dev, pcell_t *cells, int *irq,
	    enum intr_trigger *trig, enum intr_polarity *pol)
	{
		*irq = cells[0];
		*trig = INTR_TRIGGER_CONFORM;
		*pol = INTR_POLARITY_CONFORM;
		return (0);
	}

	static void null_pic_bind(device_t dev, int irq, cpuset_t cpumask)
	{
		return;
	}

	static void null_pic_ipi_send(device_t dev, cpuset_t cpus, int ipi)
	{
		return;
	}

	static void null_pic_ipi_clear(device_t dev, int ipi)
	{
		return;
	}

	static int null_pic_ipi_read(device_t dev, int ipi)
	{
		return (ipi);
	}

	static void null_pic_init_secondary(device_t dev)
	{
		return;
	}
};

METHOD int config {
	device_t	dev;
	int		irq;
	enum intr_trigger trig;
	enum intr_polarity pol;
};

METHOD int translate {
	device_t	dev;
	pcell_t		*cells;
	int		*irq;
	enum intr_trigger *trig;
	enum intr_polarity *pol;
} DEFAULT null_pic_translate;

METHOD void bind {
	device_t	dev;
	int		irq;
	cpuset_t	cpumask;
} DEFAULT null_pic_bind;

METHOD void eoi {
	device_t	dev;
	int		irq;
};

METHOD void mask {
	device_t	dev;
	int		irq;
};

METHOD void unmask {
	device_t	dev;
	int		irq;
};

METHOD void init_secondary {
	device_t	dev;
} DEFAULT null_pic_init_secondary;

METHOD void ipi_send {
	device_t	dev;
	cpuset_t	cpus;
	int		ipi;
} DEFAULT null_pic_ipi_send;

METHOD void ipi_clear {
	device_t	dev;
	int		ipi;
} DEFAULT null_pic_ipi_clear;

METHOD int ipi_read {
	device_t	dev;
	int		ipi;
} DEFAULT null_pic_ipi_read;
