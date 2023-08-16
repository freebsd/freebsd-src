/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */

/**
 ***************************************************************************
 * @file lac_log.h
 *
 * @defgroup LacLog     Log
 *
 * @ingroup LacCommon
 *
 * Logging Macros. These macros also log the function name they are called in.
 *
 ***************************************************************************/

/***************************************************************************/

#ifndef LAC_LOG_H
#define LAC_LOG_H

/***************************************************************************
 * Include public/global header files
 ***************************************************************************/
#include "cpa.h"
#include "lac_common.h"
#include "icp_accel_devices.h"

#define LAC_INVALID_PARAM_LOG_(log, args...)                                   \
	QAT_UTILS_LOG("[error] %s() - : Invalid API Param - " log "\n",        \
		      __func__,                                                \
		      ##args)

#define LAC_INVALID_PARAM_LOG(log) LAC_INVALID_PARAM_LOG_(log)

#define LAC_INVALID_PARAM_LOG1(log, param1) LAC_INVALID_PARAM_LOG_(log, param1)

#define LAC_INVALID_PARAM_LOG2(log, param1, param2)                            \
	LAC_INVALID_PARAM_LOG_(log, param1, param2)

#define LAC_UNSUPPORTED_PARAM_LOG(log)                                         \
	QAT_UTILS_LOG("%s() - : UnSupported API Param - " log "\n", __func__)

#define LAC_LOG_ERROR(log) QAT_UTILS_LOG("%s() - : " log "\n", __func__)

#endif /* LAC_LOG_H */
