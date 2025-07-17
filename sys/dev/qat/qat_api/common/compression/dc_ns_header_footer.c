/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */

/**
 *****************************************************************************
 * @file dc_ns_header_footer.c
 *
 * @ingroup Dc_DataCompression
 *
 * @description
 *      Implementation of the Data Compression header and footer operations.
 *
 *****************************************************************************/

/*
 *******************************************************************************
 * Include public/global header files
 *******************************************************************************
 */
#include "cpa.h"
#include "cpa_dc.h"

/*
 *******************************************************************************
 * Include private header files
 *******************************************************************************
 */
#include "dc_session.h"

CpaStatus
cpaDcNsGenerateHeader(CpaDcNsSetupData *pSetupData,
		      CpaFlatBuffer *pDestBuff,
		      Cpa32U *count)
{
	return CPA_STATUS_UNSUPPORTED;
}

CpaStatus
cpaDcNsGenerateFooter(CpaDcNsSetupData *pSetupData,
		      Cpa64U totalLength,
		      CpaFlatBuffer *pDestBuff,
		      CpaDcRqResults *pResults)
{
	return CPA_STATUS_UNSUPPORTED;
}
