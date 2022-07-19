/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
/* $FreeBSD$ */
/**
 *****************************************************************************
 * @file sal_string_parse.c
 *
 * @ingroup SalStringParse
 *
 * @description
 *    This file contains string parsing functions for both user space and kernel
 *    space
 *
 *****************************************************************************/
#include "cpa.h"
#include "lac_mem.h"
#include "sal_string_parse.h"

CpaStatus
Sal_StringParsing(char *string1,
		  Cpa32U instanceNumber,
		  char *string2,
		  char *result)
{
	char instNumString[SAL_CFG_MAX_VAL_LEN_IN_BYTES] = { 0 };
	Cpa32U instNumStringLen = 0;

	snprintf(instNumString,
		 SAL_CFG_MAX_VAL_LEN_IN_BYTES,
		 "%d",
		 instanceNumber);
	instNumStringLen = strnlen(instNumString, SAL_CFG_MAX_VAL_LEN_IN_BYTES);
	if ((strnlen(string1, SAL_CFG_MAX_VAL_LEN_IN_BYTES) + instNumStringLen +
	     strnlen(string2, SAL_CFG_MAX_VAL_LEN_IN_BYTES)) >
	    SAL_CFG_MAX_VAL_LEN_IN_BYTES) {
		QAT_UTILS_LOG("Size of result too small.\n");
		return CPA_STATUS_FAIL;
	}

	LAC_OS_BZERO(result, SAL_CFG_MAX_VAL_LEN_IN_BYTES);
	snprintf(result,
		 SAL_CFG_MAX_VAL_LEN_IN_BYTES,
		 "%s%d%s",
		 string1,
		 instanceNumber,
		 string2);

	return CPA_STATUS_SUCCESS;
}

Cpa64U
Sal_Strtoul(const char *cp, char **endp, unsigned int cfgBase)
{
	Cpa64U ulResult = 0;

	ulResult = (Cpa64U)simple_strtoull(cp, endp, cfgBase);

	return ulResult;
}
