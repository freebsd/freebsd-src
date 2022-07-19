# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2007-2022 Intel Corporation
# $FreeBSD$
.PATH: ${SRCTOP}/sys/contrib/dev/qat

KMOD= qat_c4xxx_fw

FIRMWS= ${SRCTOP}/sys/contrib/dev/qat/qat_c4xxx.bin:qat_c4xxx_fw:111 ${SRCTOP}/sys/contrib/dev/qat/qat_c4xxx_mmp.bin:qat_c4xxx_mmp_fw:111

.include <bsd.kmod.mk>
