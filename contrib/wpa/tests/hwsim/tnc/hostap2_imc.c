/*
 * Example IMC for TNC testing
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
static TNC_TNCC_SendMessagePointer send_message = NULL;
static TNC_TNCC_ReportMessageTypesPointer report_message_types = NULL;
static TNC_TNCC_RequestHandshakeRetryPointer request_retry = NULL;

static TNC_MessageType message_types[] =
{
	(TNC_VENDORID_ANY << 8) | TNC_SUBTYPE_ANY
};


TNC_Result TNC_IMC_Initialize(
	/*in*/ TNC_IMCID imcID,
	/*in*/ TNC_Version minVersion,
	/*in*/ TNC_Version maxVersion,
	/*out*/ TNC_Version *pOutActualVersion)
{
	wpa_printf(MSG_INFO,
		   "IMC(hostap2) %s(imcID=%u, minVersion=%u, maxVersion=%u)",
		   __func__, (unsigned) imcID, (unsigned) minVersion,
		   (unsigned) maxVersion);

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
	char *msg = "hello";
	TNC_Result res;

	wpa_printf(MSG_INFO, "IMC(hostap2) %s(imcID=%u, connectionID=%u)",
		   __func__, (unsigned) imcID, (unsigned) connectionID);

	if (!initialized)
		return TNC_RESULT_NOT_INITIALIZED;

	if (imcID != my_id)
		return TNC_RESULT_INVALID_PARAMETER;

	if (!send_message)
		return TNC_RESULT_FATAL;

	res = send_message(imcID, connectionID, msg, os_strlen(msg), 1);
	if (res != TNC_RESULT_SUCCESS)
		return res;

	return TNC_RESULT_SUCCESS;
}


TNC_Result TNC_IMC_ProvideBindFunction(
	/*in*/ TNC_IMCID imcID,
	/*in*/ TNC_TNCC_BindFunctionPointer bindFunction)
{
	TNC_Result res;

	wpa_printf(MSG_INFO, "IMC(hostap2) %s(imcID=%u)",
		   __func__, (unsigned) imcID);

	if (!initialized)
		return TNC_RESULT_NOT_INITIALIZED;

	if (imcID != my_id || !bindFunction)
		return TNC_RESULT_INVALID_PARAMETER;

	if (bindFunction(imcID, "TNC_TNCC_SendMessage",
			 (void **) &send_message) != TNC_RESULT_SUCCESS ||
	    !send_message)
		return TNC_RESULT_FATAL;

	if (bindFunction(imcID, "TNC_TNCC_ReportMessageTypes",
			 (void **) &report_message_types) !=
	    TNC_RESULT_SUCCESS ||
	    !report_message_types)
		return TNC_RESULT_FATAL;

	if (bindFunction(imcID, "TNC_TNCC_RequestHandshakeRetry",
			 (void **) &request_retry) != TNC_RESULT_SUCCESS ||
	    !request_retry)
		return TNC_RESULT_FATAL;

	res = report_message_types(imcID, message_types,
				   ARRAY_SIZE(message_types));
	if (res != TNC_RESULT_SUCCESS)
		return res;

	return TNC_RESULT_SUCCESS;
}


TNC_Result TNC_IMC_NotifyConnectionChange(
	/*in*/ TNC_IMCID imcID,
	/*in*/ TNC_ConnectionID connectionID,
	/*in*/ TNC_ConnectionState newState)
{
	wpa_printf(MSG_INFO,
		   "IMC(hostap2) %s(imcID=%u, connectionID=%u, newState=%u)",
		   __func__, (unsigned) imcID, (unsigned) connectionID,
		   (unsigned) newState);

	return TNC_RESULT_SUCCESS;
}


TNC_Result TNC_IMC_ReceiveMessage(
	/*in*/ TNC_IMCID imcID,
	/*in*/ TNC_ConnectionID connectionID,
	/*in*/ TNC_BufferReference message,
	/*in*/ TNC_UInt32 messageLength,
	/*in*/ TNC_MessageType messageType)
{
	TNC_Result res;

	wpa_printf(MSG_INFO,
		   "IMC(hostap2) %s(imcID=%u, connectionID=%u, messageType=%u)",
		   __func__, (unsigned) imcID, (unsigned) connectionID,
		   (unsigned) messageType);
	wpa_hexdump_ascii(MSG_INFO, "IMC(hostap2) message",
			  message, messageLength);

	if (messageType == 1 && messageLength == 5 &&
	    os_memcmp(message, "hello", 5) == 0) {
		char *msg = "i'm fine";

		res = send_message(imcID, connectionID, msg, os_strlen(msg), 1);
		if (res != TNC_RESULT_SUCCESS)
			return res;
	}

	return TNC_RESULT_SUCCESS;
}


TNC_Result TNC_IMC_BatchEnding(
	/*in*/ TNC_IMCID imcID,
	/*in*/ TNC_ConnectionID connectionID)
{
	wpa_printf(MSG_INFO, "IMC(hostap2) %s(imcID=%u, connectionID=%u)",
		   __func__, (unsigned) imcID, (unsigned) connectionID);

	return TNC_RESULT_SUCCESS;
}


TNC_Result TNC_IMC_Terminate(
	/*in*/ TNC_IMCID imcID)
{
	wpa_printf(MSG_INFO, "IMC(hostap2) %s(imcID=%u)",
		   __func__, (unsigned) imcID);

	return TNC_RESULT_SUCCESS;
}
