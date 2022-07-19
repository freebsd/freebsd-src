# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2007-2022 Intel Corporation
# $FreeBSD$
.PATH: ${SRCTOP}/sys/dev/qat/qat

KMOD= qat
SRCS+= qat_ocf.c qat_ocf_mem_pool.c qat_ocf_utils.c
SRCS+= device_if.h bus_if.h vnode_if.h pci_if.h cryptodev_if.h

CFLAGS+= -I${SYSDIR}/compat/linuxkpi/common/include
CFLAGS+= -I${SRCTOP}/sys/dev/qat/include
CFLAGS+= -I${SRCTOP}/sys/dev/qat/include/common
CFLAGS+= -I${SRCTOP}/sys/dev/qat/qat_api/include
CFLAGS+= -I${SRCTOP}/sys/dev/qat/qat_api/common/include
CFLAGS+= -I${SRCTOP}/sys/dev/qat/qat_api/include/lac
CFLAGS+= -I${SRCTOP}/sys/dev/qat/qat_api/qat_utils/include
CFLAGS+= -I${SRCTOP}/sys/dev/qat/qat_api/qat_direct/include
CFLAGS+= -I${SRCTOP}/sys/dev/qat/qat_api/firmware/include
CFLAGS+= -I${SRCTOP}/sys/dev/qat/qat_api/common/crypto/sym/include

CWARNFLAGS.qat_ocf.c += -Wno-incompatible-pointer-types-discards-qualifiers
CWARNFLAGS.qat_ocf_utils.c += -Wno-incompatible-pointer-types-discards-qualifiers

.include <bsd.kmod.mk>
