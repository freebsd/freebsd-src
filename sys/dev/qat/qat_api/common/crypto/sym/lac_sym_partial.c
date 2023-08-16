/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */

/**
 ***************************************************************************
 * @file lac_sym_partial.c   common partial packet functions
 *
 * @ingroup LacSym
 *
 ***************************************************************************/

/*
*******************************************************************************
* Include public/global header files
*******************************************************************************
*/
#include "cpa.h"

#include "icp_accel_devices.h"
#include "icp_adf_debug.h"

#include "lac_log.h"
#include "lac_sym.h"
#include "cpa_cy_sym.h"
#include "lac_common.h"

#include "lac_sym_partial.h"

CpaStatus
LacSym_PartialPacketStateCheck(CpaCySymPacketType packetType,
			       CpaCySymPacketType partialState)
{
	CpaStatus status = CPA_STATUS_SUCCESS;

	/* ASSUMPTION - partial requests on a given session must be issued
	 * sequentially to guarantee ordering
	 * (i.e. issuing partials on concurrent threads for a particular session
	 * just wouldn't work)
	 */

	/* state is no partial - only a partial is allowed */
	if (((CPA_CY_SYM_PACKET_TYPE_FULL == partialState) &&
	     (CPA_CY_SYM_PACKET_TYPE_PARTIAL == packetType)) ||

	    /* state is partial - only a partial or final partial is allowed */
	    ((CPA_CY_SYM_PACKET_TYPE_PARTIAL == partialState) &&
	     ((CPA_CY_SYM_PACKET_TYPE_PARTIAL == packetType) ||
	      (CPA_CY_SYM_PACKET_TYPE_LAST_PARTIAL == packetType)))) {
		status = CPA_STATUS_SUCCESS;
	} else /* invalid sequence */
	{
		LAC_INVALID_PARAM_LOG("invalid partial packet sequence");
		status = CPA_STATUS_INVALID_PARAM;
	}

	return status;
}

void
LacSym_PartialPacketStateUpdate(CpaCySymPacketType packetType,
				CpaCySymPacketType *pPartialState)
{
	/* if previous packet was either a full or ended a partial stream,
	 * update
	 * state to partial to indicate a new partial stream was created */
	if (CPA_CY_SYM_PACKET_TYPE_FULL == *pPartialState) {
		*pPartialState = CPA_CY_SYM_PACKET_TYPE_PARTIAL;
	} else {
		/* if packet type is final - reset the partial state */
		if (CPA_CY_SYM_PACKET_TYPE_LAST_PARTIAL == packetType) {
			*pPartialState = CPA_CY_SYM_PACKET_TYPE_FULL;
		}
	}
}
