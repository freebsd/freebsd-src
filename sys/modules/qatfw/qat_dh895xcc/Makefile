# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2007-2022 Intel Corporation
# $FreeBSD$
.PATH: ${SRCTOP}/sys/contrib/dev/qat

KMOD= qat_dh895xcc_fw

FIRMWS= ${SRCTOP}/sys/contrib/dev/qat/qat_895xcc.bin:qat_dh895xcc_fw:111 ${SRCTOP}/sys/contrib/dev/qat/qat_895xcc_mmp.bin:qat_dh895xcc_mmp_fw:111

.include <bsd.kmod.mk>
