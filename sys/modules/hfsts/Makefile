# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2026 Abdelkader Boudih <seuros@FreeBSD.org>

.PATH: ${SRCTOP}/sys/dev/intel

KMOD=	hfsts
SRCS=	hfsts.c hfsts_isa.c
SRCS+=	bus_if.h device_if.h pci_if.h isa_if.h

.include <bsd.kmod.mk>
