#
# Copyright (c) 2002, 2003 Sam Leffler, Errno Consulting
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer,
#    without modification.
# 2. Redistributions in binary form must reproduce at minimum a disclaimer
#    similar to the "NO WARRANTY" disclaimer below ("Disclaimer") and any
#    redistribution must be conditioned upon including a substantially
#    similar Disclaimer requirement for further binary redistribution.
# 3. Neither the names of the above-listed copyright holders nor the names
#    of any contributors may be used to endorse or promote products derived
#    from this software without specific prior written permission.
#
# Alternatively, this software may be distributed under the terms of the
# GNU General Public License ("GPL") version 2 as published by the Free
# Software Foundation.
#
# NO WARRANTY
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF NONINFRINGEMENT, MERCHANTIBILITY
# AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
# THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY,
# OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
# IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
# THE POSSIBILITY OF SUCH DAMAGES.
#
# $FreeBSD$
#

#
# Hardware Access Layer (HAL) for the Atheros Wireless NIC driver.
#
# This module contains the hardware-specific bits for the network
# interface driver.  It is built as a separate module to simplify
# maintenance and isolate the bits that are not (currently) distributed
# in source form.
#
HAL=	${.CURDIR}/../../contrib/dev/ath

.PATH: ${HAL}/freebsd

KMOD=	ath_hal
SRCS=	ah_osdep.c
SRCS+=	bus_if.h device_if.h pci_if.h ah_if.h opt_ah.h
OBJS=	hal.o
MFILES=	kern/bus_if.m kern/device_if.m dev/pci/pci_if.m \
	contrib/dev/ath/freebsd/ah_if.m

CFLAGS+= -I. -I${HAL}/freebsd -I${HAL}

.if defined(HAL_SRC)
.include "${HAL}/freebsd/Makefile.inc"
.else

hal.o:	${HAL}/freebsd/i386-elf.hal.o.uu
	uudecode -p < ${HAL}/freebsd/i386-elf.hal.o.uu > ${.TARGET}
.endif

.include <bsd.kmod.mk>
