/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
/**
 *****************************************************************************
 * @file icp_sal_dc_err.c
 *
 * @defgroup SalCommon
 *
 * @ingroup SalCommon
 *
 *****************************************************************************/

/*
******************************************************************************
* Include public/global header files
******************************************************************************
*/
#include "cpa.h"
#include "icp_sal.h"

/*
*******************************************************************************
* Include private header files
*******************************************************************************
*/
#include "dc_error_counter.h"

Cpa64U
icp_sal_get_dc_error(Cpa8S dcError)
{
	return getDcErrorCounter(dcError);
}
