/*
 * Minimal example IMC for TNC testing
 * Copyright (c) 2014, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "includes.h"

#include "common.h"
#include "common/tnc.h"

static int initialized = 0;
static TNC_IMCID my_id = -1;

TNC_Result TNC_IMC_Initialize(
	/*in*/ TNC_IMCID imcID,
	/*in*/ TNC_Version minVersion,
	/*in*/ TNC_Version maxVersion,
	/*out*/ TNC_Version *pOutActualVersion)
{
	wpa_printf(MSG_INFO, "IMC(hostap) %s", __func__);

	if (initialized)
		return TNC_RESULT_ALREADY_INITIALIZED;

	if (minVersion < TNC_IFIMC_VERSION_1 ||
	    maxVersion > TNC_IFIMC_VERSION_1)
		return TNC_RESULT_NO_COMMON_VERSION;

	if (!pOutActualVersion)
		return TNC_RESULT_INVALID_PARAMETER;
	*pOutActualVersion = TNC_IFIMC_VERSION_1;
	my_id = imcID;

	initialized = 1;

	return TNC_RESULT_SUCCESS;
}


TNC_Result TNC_IMC_BeginHandshake(
	/*in*/ TNC_IMCID imcID,
	/*in*/ TNC_ConnectionID connectionID)
{
	wpa_printf(MSG_INFO, "IMC(hostap) %s", __func__);

	if (!initialized)
		return TNC_RESULT_NOT_INITIALIZED;

	if (imcID != my_id)
		return TNC_RESULT_INVALID_PARAMETER;

	return TNC_RESULT_SUCCESS;
}


TNC_Result TNC_IMC_ProvideBindFunction(
	/*in*/ TNC_IMCID imcID,
	/*in*/ TNC_TNCC_BindFunctionPointer bindFunction)
{
	wpa_printf(MSG_INFO, "IMC(hostap) %s", __func__);

	if (!initialized)
		return TNC_RESULT_NOT_INITIALIZED;

	if (imcID != my_id)
		return TNC_RESULT_INVALID_PARAMETER;

	return TNC_RESULT_SUCCESS;
}
