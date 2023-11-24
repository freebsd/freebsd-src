# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2007-2022 Intel Corporation
.PATH: ${SRCTOP}/sys/dev/qat/qat_hw

KMOD= qat_hw
SRCS+= qat_c62x/adf_c62x_hw_data.c qat_c62x/adf_drv.c
SRCS+= qat_200xx/adf_200xx_hw_data.c qat_200xx/adf_drv.c
SRCS+= qat_4xxx/adf_4xxx_hw_data.c qat_4xxx/adf_drv.c
SRCS+= qat_4xxxvf/adf_4xxxvf_hw_data.c qat_4xxxvf/adf_drv.c
SRCS+= qat_c3xxx/adf_c3xxx_hw_data.c qat_c3xxx/adf_drv.c
SRCS+= qat_dh895xcc/adf_dh895xcc_hw_data.c qat_dh895xcc/adf_drv.c
SRCS+= qat_c4xxx/adf_c4xxx_hw_data.c qat_c4xxx/adf_drv.c qat_c4xxx/adf_c4xxx_ae_config.c qat_c4xxx/adf_c4xxx_misc_error_stats.c
SRCS+= qat_c4xxx/adf_c4xxx_pke_replay_stats.c qat_c4xxx/adf_c4xxx_ras.c qat_c4xxx/adf_c4xxx_res_part.c
SRCS+= qat_c4xxx/adf_c4xxx_reset.c
SRCS+= device_if.h bus_if.h vnode_if.h pci_if.h cryptodev_if.h

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

.include <bsd.kmod.mk>
