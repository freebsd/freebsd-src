# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2007-2022 Intel Corporation
.PATH: ${SRCTOP}/sys/dev/qat/qat

KMOD= qat
SRCS+= qat_ocf.c qat_ocf_mem_pool.c qat_ocf_utils.c
SRCS+= device_if.h bus_if.h vnode_if.h pci_if.h cryptodev_if.h opt_qat.h

CFLAGS+= ${LINUXKPI_INCLUDES}
CFLAGS+= -I${SRCTOP}/sys/dev/qat/include
CFLAGS+= -I${SRCTOP}/sys/dev/qat/include/common
CFLAGS+= -I${SRCTOP}/sys/dev/qat/qat_api/include
CFLAGS+= -I${SRCTOP}/sys/dev/qat/qat_api/common/include
CFLAGS+= -I${SRCTOP}/sys/dev/qat/qat_api/include/lac
CFLAGS+= -I${SRCTOP}/sys/dev/qat/qat_api/qat_utils/include
CFLAGS+= -I${SRCTOP}/sys/dev/qat/qat_api/qat_direct/include
CFLAGS+= -I${SRCTOP}/sys/dev/qat/qat_api/firmware/include
CFLAGS+= -I${SRCTOP}/sys/dev/qat/qat_api/common/crypto/sym/include

.if !defined(KERNBUILDDIR)
CFLAGS+=        -include opt_qat.h
MKDEP=          -include opt_qat.h

opt_qat.h:
	:> ${.TARGET}
.if defined(QAT_DISABLE_SAFE_DC_MODE) && ${QAT_DISABLE_SAFE_DC_MODE} == "1"
	@echo "#define QAT_DISABLE_SAFE_DC_MODE 1" >> ${.TARGET}
.endif
.endif

.include <bsd.kmod.mk>

.if ${COMPILER_TYPE} == "clang"
CWARNFLAGS.qat_ocf.c += -Wno-incompatible-pointer-types-discards-qualifiers
CWARNFLAGS.qat_ocf_utils.c += -Wno-incompatible-pointer-types-discards-qualifiers
.elif ${COMPILER_TYPE} == "gcc"
CWARNFLAGS.qat_ocf.c += -Wno-discarded-qualifiers
CWARNFLAGS.qat_ocf_utils.c += -Wno-discarded-qualifiers
.endif
