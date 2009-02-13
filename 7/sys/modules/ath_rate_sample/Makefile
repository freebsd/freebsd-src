#
# Copyright (c) 2004 Sam Leffler, Errno Consulting
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

.PATH: ${.CURDIR}/../../dev/ath/ath_rate/sample

KMOD=	ath_rate
SRCS=	sample.c
SRCS+=	device_if.h bus_if.h pci_if.h opt_inet.h opt_ah.h

HAL=	${.CURDIR}/../../contrib/dev/ath
CFLAGS+=  -I. -I${.CURDIR}/../../dev/ath -I${HAL}

# patch for hal naming difference
.if ${MACHINE_ARCH} == "amd64"
ATH_MODULE_ARCH=x86_64
.elif ${MACHINE_ARCH} == "sparc64"
ATH_MODULE_ARCH=sparc64-be
.elif ${MACHINE_ARCH} == "powerpc"
ATH_MODULE_ARCH=powerpc-be
.else
ATH_MODULE_ARCH=${MACHINE_ARCH}
.endif

opt_ah.h: ${HAL}/public/${ATH_MODULE_ARCH}-elf.opt_ah.h
	cp ${HAL}/public/${ATH_MODULE_ARCH}-elf.opt_ah.h ${.TARGET}

.include <bsd.kmod.mk>
