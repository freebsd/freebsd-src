/*
 * Minimal example IMV for TNC testing
 * Copyright (c) 2014, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "includes.h"

#include "common.h"
#include "common/tnc.h"

static int initialized = 0;
static TNC_IMVID my_id = -1;

TNC_Result TNC_IMV_Initialize(
	/*in*/ TNC_IMVID imvID,
	/*in*/ TNC_Version minVersion,
	/*in*/ TNC_Version maxVersion,
	/*out*/ TNC_Version *pOutActualVersion)
{
	if (initialized)
		return TNC_RESULT_ALREADY_INITIALIZED;

	if (minVersion < TNC_IFIMV_VERSION_1 ||
	    maxVersion > TNC_IFIMV_VERSION_1)
		return TNC_RESULT_NO_COMMON_VERSION;

	if (!pOutActualVersion)
		return TNC_RESULT_INVALID_PARAMETER;
	*pOutActualVersion = TNC_IFIMV_VERSION_1;

	initialized = 1;
	my_id = imvID;

	return TNC_RESULT_SUCCESS;
}


TNC_Result TNC_IMV_SolicitRecommendation(
	/*in*/ TNC_IMVID imvID,
	/*in*/ TNC_ConnectionID connectionID)
{
	if (!initialized)
		return TNC_RESULT_NOT_INITIALIZED;

	if (imvID != my_id)
		return TNC_RESULT_INVALID_PARAMETER;

	return TNC_RESULT_SUCCESS;
}


TNC_Result TNC_IMV_ProvideBindFunction(
	/*in*/ TNC_IMVID imvID,
	/*in*/ TNC_TNCS_BindFunctionPointer bindFunction)
{
	if (!initialized)
		return TNC_RESULT_NOT_INITIALIZED;

	if (imvID != my_id)
		return TNC_RESULT_INVALID_PARAMETER;

	return TNC_RESULT_SUCCESS;
}
