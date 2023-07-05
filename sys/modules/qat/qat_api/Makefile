# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2007-2022 Intel Corporation
# $FreeBSD$
.PATH: ${SRCTOP}/sys/dev/qat/qat_api

KMOD= qat_api

SRCS+= freebsd_module.c
SRCS+= common/compression/dc_datapath.c
SRCS+= common/compression/dc_header_footer.c
SRCS+= common/compression/dc_session.c
SRCS+= common/compression/dc_stats.c
SRCS+= common/compression/dc_buffers.c
SRCS+= common/compression/dc_dp.c
SRCS+= common/compression/icp_sal_dc_err.c
SRCS+= common/compression/dc_chain.c
SRCS+= common/compression/dc_ns_datapath.c
SRCS+= common/compression/dc_ns_header_footer.c
SRCS+= common/utils/lac_buffer_desc.c
SRCS+= common/utils/lac_mem.c
SRCS+= common/utils/lac_mem_pools.c
SRCS+= common/utils/lac_sync.c
SRCS+= common/utils/sal_service_state.c
SRCS+= common/utils/sal_statistics.c
SRCS+= common/utils/sal_string_parse.c
SRCS+= common/utils/sal_versions.c
SRCS+= common/utils/sal_user_process.c
SRCS+= common/ctrl/sal_list.c
SRCS+= common/ctrl/sal_compression.c
SRCS+= common/ctrl/sal_ctrl_services.c
SRCS+= common/ctrl/sal_create_services.c
SRCS+= common/ctrl/sal_crypto.c
SRCS+= common/ctrl/sal_get_instances.c
SRCS+= common/qat_comms/sal_qat_cmn_msg.c
SRCS+= common/crypto/sym/lac_sym_api.c
SRCS+= common/crypto/sym/lac_sym_cb.c
SRCS+= common/crypto/sym/lac_sym_queue.c
SRCS+= common/crypto/sym/lac_sym_cipher.c
SRCS+= common/crypto/sym/lac_sym_alg_chain.c
SRCS+= common/crypto/sym/lac_sym_auth_enc.c
SRCS+= common/crypto/sym/lac_sym_hash.c
SRCS+= common/crypto/sym/lac_sym_hash_sw_precomputes.c
SRCS+= common/crypto/sym/lac_sym_stats.c
SRCS+= common/crypto/sym/lac_sym_compile_check.c
SRCS+= common/crypto/sym/lac_sym_partial.c
SRCS+= common/crypto/sym/lac_sym_dp.c
SRCS+= common/crypto/sym/qat/lac_sym_qat.c
SRCS+= common/crypto/sym/qat/lac_sym_qat_hash.c
SRCS+= common/crypto/sym/qat/lac_sym_qat_hash_defs_lookup.c
SRCS+= common/crypto/sym/qat/lac_sym_qat_cipher.c
SRCS+= common/crypto/sym/qat/lac_sym_qat_constants_table.c
SRCS+= common/crypto/sym/qat/lac_sym_qat_key.c
SRCS+= common/crypto/sym/key/lac_sym_key.c
SRCS+= common/stubs/lac_stubs.c
SRCS+= device/dev_info.c
SRCS+= qat_kernel/src/lac_adf_interface_freebsd.c
SRCS+= qat_kernel/src/qat_transport.c
SRCS+= qat_kernel/src/lac_symbols.c
SRCS+= qat_utils/src/QatUtilsServices.c
SRCS+= qat_utils/src/QatUtilsSemaphore.c
SRCS+= qat_utils/src/QatUtilsSpinLock.c
SRCS+= qat_utils/src/QatUtilsAtomic.c
SRCS+= qat_utils/src/QatUtilsCrypto.c
SRCS+= bus_if.h cryptodev_if.h device_if.h pci_if.h vnode_if.h

CFLAGS+= -I${SRCTOP}/sys/dev/qat/qat_api/include
CFLAGS+= -I${SRCTOP}/sys/dev/qat/qat_api/include/lac
CFLAGS+= -I${SRCTOP}/sys/dev/qat/qat_api/include/dc
CFLAGS+= -I${SRCTOP}/sys/dev/qat/qat_api/qat_direct/include
CFLAGS+= -I${SRCTOP}/sys/dev/qat/qat_api/qat_utils/include
CFLAGS+= -I${SRCTOP}/sys/dev/qat/qat_api/common/include
CFLAGS+= -I${SRCTOP}/sys/dev/qat/qat_api/common/compression/include
CFLAGS+= -I${SRCTOP}/sys/dev/qat/qat_api/common/crypto/sym/include
CFLAGS+= -I${SRCTOP}/sys/dev/qat/qat_api/firmware/include
CFLAGS+= -I${SRCTOP}/sys/dev/qat/include/common
CFLAGS+= ${LINUXKPI_INCLUDES}

.include <bsd.kmod.mk>

CWARNFLAGS+= -Wno-cast-qual
