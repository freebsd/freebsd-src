/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
/* $FreeBSD$ */
/******************************************************************************
 * @file lac_symbols.c
 *
 * This file contains all the symbols that are exported by the Look Aside
 * kernel Module.
 *
 *****************************************************************************/
#include <linux/module.h>
#include "cpa.h"
#include "cpa_dc.h"
#include "cpa_dc_dp.h"
#include "cpa_dc_bp.h"
#include "icp_adf_init.h"
#include "icp_adf_transport.h"
#include "icp_adf_poll.h"
#include "icp_sal_poll.h"
#include "icp_sal_iommu.h"
#include "icp_sal_versions.h"
#include "lac_common.h"

/* Symbols for getting version information */
EXPORT_SYMBOL(icp_sal_getDevVersionInfo);

/* DC Compression */
EXPORT_SYMBOL(cpaDcGetNumIntermediateBuffers);
EXPORT_SYMBOL(cpaDcInitSession);
EXPORT_SYMBOL(cpaDcResetSession);
EXPORT_SYMBOL(cpaDcUpdateSession);
EXPORT_SYMBOL(cpaDcRemoveSession);
EXPORT_SYMBOL(cpaDcCompressData);
EXPORT_SYMBOL(cpaDcDecompressData);
EXPORT_SYMBOL(cpaDcGenerateHeader);
EXPORT_SYMBOL(cpaDcGenerateFooter);
EXPORT_SYMBOL(cpaDcGetStats);
EXPORT_SYMBOL(cpaDcGetInstances);
EXPORT_SYMBOL(cpaDcGetNumInstances);
EXPORT_SYMBOL(cpaDcGetSessionSize);
EXPORT_SYMBOL(cpaDcGetStatusText);
EXPORT_SYMBOL(cpaDcBufferListGetMetaSize);
EXPORT_SYMBOL(cpaDcBnpBufferListGetMetaSize);
EXPORT_SYMBOL(cpaDcDeflateCompressBound);
EXPORT_SYMBOL(cpaDcInstanceGetInfo2);
EXPORT_SYMBOL(cpaDcQueryCapabilities);
EXPORT_SYMBOL(cpaDcSetAddressTranslation);
EXPORT_SYMBOL(cpaDcStartInstance);
EXPORT_SYMBOL(cpaDcStopInstance);
EXPORT_SYMBOL(cpaDcBPCompressData);
EXPORT_SYMBOL(cpaDcCompressData2);
EXPORT_SYMBOL(cpaDcDecompressData2);

/* DcDp Compression */
EXPORT_SYMBOL(cpaDcDpGetSessionSize);
EXPORT_SYMBOL(cpaDcDpInitSession);
EXPORT_SYMBOL(cpaDcDpRemoveSession);
EXPORT_SYMBOL(cpaDcDpUpdateSession);
EXPORT_SYMBOL(cpaDcDpRegCbFunc);
EXPORT_SYMBOL(cpaDcDpEnqueueOp);
EXPORT_SYMBOL(cpaDcDpEnqueueOpBatch);
EXPORT_SYMBOL(cpaDcDpPerformOpNow);

EXPORT_SYMBOL(icp_sal_DcPollInstance);
EXPORT_SYMBOL(icp_sal_DcPollDpInstance);
EXPORT_SYMBOL(icp_sal_pollBank);
EXPORT_SYMBOL(icp_sal_pollAllBanks);

/* sal iommu symbols */
EXPORT_SYMBOL(icp_sal_iommu_get_remap_size);
EXPORT_SYMBOL(icp_sal_iommu_map);
EXPORT_SYMBOL(icp_sal_iommu_unmap);

EXPORT_SYMBOL(icp_sal_get_dc_error);
