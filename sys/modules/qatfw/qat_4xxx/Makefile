# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2007-2022 Intel Corporation
.PATH: ${SRCTOP}/sys/contrib/dev/qat

KMOD= qat_4xxx_fw

FIRMWS= qat_4xxx.bin:qat_4xxx_fw:111 \
        qat_4xxx_mmp.bin:qat_4xxx_mmp_fw:111 \
        qat_402xx.bin:qat_402xx_fw:111 \
        qat_402xx_mmp.bin:qat_402xx_mmp_fw:111

.include <bsd.kmod.mk>
