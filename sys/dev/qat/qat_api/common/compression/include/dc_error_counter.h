/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
/* $FreeBSD$ */
/**
 *****************************************************************************
 * @file dc_error_counter.h
 *
 * @ingroup Dc_DataCompression
 *
 * @description
 *      Definition of the Data Compression Error Counter parameters.
 *
 *****************************************************************************/
#ifndef DC_ERROR_COUNTER_H
#define DC_ERROR_COUNTER_H

#include "cpa_types.h"
#include "cpa_dc.h"

#define MAX_DC_ERROR_TYPE 20

void dcErrorLog(CpaDcReqStatus dcError);
Cpa64U getDcErrorCounter(CpaDcReqStatus dcError);

#endif /* DC_ERROR_COUNTER_H */
