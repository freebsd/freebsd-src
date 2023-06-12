# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2007-2022 Intel Corporation
# $FreeBSD$
.PATH:	${SRCTOP}/sys/dev/qat/qat_common

KMOD= qat_common

SRCS+= adf_accel_engine.c adf_freebsd_admin.c adf_aer.c adf_cfg.c qat_common_module.c
SRCS+= adf_heartbeat.c adf_freebsd_heartbeat_dbg.c
SRCS+= adf_freebsd_dev_processes.c adf_freebsd_uio.c adf_freebsd_uio_cleanup.c
SRCS+= adf_ctl_drv.c adf_dev_mgr.c adf_hw_arbiter.c
SRCS+= adf_init.c adf_transport.c adf_isr.c adf_fw_counters.c adf_dev_err.c
SRCS+= adf_gen2_hw_data.c
SRCS+= adf_gen4_hw_data.c
SRCS+= qat_freebsd.c
SRCS+= adf_freebsd_cfg_dev_dbg.c adf_freebsd_ver_dbg.c
SRCS+= adf_cfg_device.c adf_cfg_section.c adf_cfg_instance.c adf_cfg_bundle.c adf_cfg_sysctl.c
SRCS+= qat_hal.c qat_uclo.c
SRCS+= adf_vf_isr.c
SRCS+= adf_gen4_pfvf.c
SRCS+= adf_gen4_timer.c
SRCS+= adf_pfvf_utils.c adf_pfvf_vf_msg.c adf_pfvf_vf_proto.c
SRCS+= adf_gen4vf_hw_csr_data.c
SRCS+= adf_freebsd_transport_debug.c adf_clock.c
SRCS+= adf_freebsd_cnvnr_ctrs_dbg.c
SRCS+= adf_freebsd_pfvf_ctrs_dbg.c
SRCS+= bus_if.h device_if.h pci_if.h vnode_if.h

CFLAGS+= -I${SRCTOP}/sys/dev/qat/include
CFLAGS+= -I${SRCTOP}/sys/dev/qat/include/common
CFLAGS+= ${LINUXKPI_INCLUDES}

.include <bsd.kmod.mk>
