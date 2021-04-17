/*
 * Example IMV for TNC testing
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
static TNC_TNCS_ReportMessageTypesPointer report_message_types = NULL;
static TNC_TNCS_SendMessagePointer send_message = NULL;
static TNC_TNCS_RequestHandshakeRetryPointer request_retry = NULL;
TNC_TNCS_ProvideRecommendationPointer provide_recomm = NULL;

static TNC_MessageType message_types[] =
{
	(TNC_VENDORID_ANY << 8) | TNC_SUBTYPE_ANY
};


TNC_Result TNC_IMV_Initialize(
	/*in*/ TNC_IMVID imvID,
	/*in*/ TNC_Version minVersion,
	/*in*/ TNC_Version maxVersion,
	/*out*/ TNC_Version *pOutActualVersion)
{
	wpa_printf(MSG_INFO,
		   "IMV(hostap2) %s(imvID=%u, minVersion=%u, maxVersion=%u)",
		   __func__, (unsigned) imvID, (unsigned) minVersion,
		   (unsigned) maxVersion);

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


TNC_Result TNC_IMV_NotifyConnectionChange(
	/*in*/ TNC_IMVID imvID,
	/*in*/ TNC_ConnectionID connectionID,
	/*in*/ TNC_ConnectionState newState)
{
	wpa_printf(MSG_INFO,
		   "IMV(hostap2) %s(imvID=%u, connectionID=%u, newState=%u)",
		   __func__, (unsigned) imvID, (unsigned) connectionID,
		   (unsigned) newState);

	if (!initialized)
		return TNC_RESULT_NOT_INITIALIZED;

	if (imvID != my_id)
		return TNC_RESULT_INVALID_PARAMETER;

	/* TODO: call TNC_TNCS_ProvideRecommendation */

	return TNC_RESULT_SUCCESS;
}


TNC_Result TNC_IMV_ReceiveMessage(
	/*in*/ TNC_IMVID imvID,
	/*in*/ TNC_ConnectionID connectionID,
	/*in*/ TNC_BufferReference message,
	/*in*/ TNC_UInt32 messageLength,
	/*in*/ TNC_MessageType messageType)
{
	TNC_Result res;

	wpa_printf(MSG_INFO,
		   "IMV(hostap2) %s(imvID=%u, connectionID=%u, messageType=%u)",
		   __func__, (unsigned) imvID, (unsigned) connectionID,
		   (unsigned) messageType);
	wpa_hexdump_ascii(MSG_INFO, "IMV(hostap2) message",
			  message, messageLength);

	if (!send_message)
		return TNC_RESULT_FATAL;

	if (messageType == 1 && messageLength == 5 &&
	    os_memcmp(message, "hello", 5) == 0) {
		char *msg = "hello";

		res = send_message(imvID, connectionID, msg, os_strlen(msg), 1);
		if (res != TNC_RESULT_SUCCESS)
			return res;
	}

	if (messageType == 1 && messageLength == 8 &&
	    os_memcmp(message, "i'm fine", 8) == 0) {
		if (!provide_recomm)
			return TNC_RESULT_FATAL;
		res = provide_recomm(imvID, connectionID,
				     TNC_IMV_ACTION_RECOMMENDATION_ALLOW,
				     TNC_IMV_EVALUATION_RESULT_COMPLIANT);
		if (res != TNC_RESULT_SUCCESS)
			return res;
	}

	return TNC_RESULT_SUCCESS;
}


TNC_Result TNC_IMV_SolicitRecommendation(
	/*in*/ TNC_IMVID imvID,
	/*in*/ TNC_ConnectionID connectionID)
{
	wpa_printf(MSG_INFO, "IMV(hostap2) %s(imvID=%u, connectionID=%u)",
		   __func__, (unsigned) imvID, (unsigned) connectionID);

	if (!initialized)
		return TNC_RESULT_NOT_INITIALIZED;

	if (imvID != my_id)
		return TNC_RESULT_INVALID_PARAMETER;

	/* TODO: call TNC_TNCS_ProvideRecommendation */

	return TNC_RESULT_SUCCESS;
}


TNC_Result TNC_IMV_BatchEnding(
	/*in*/ TNC_IMVID imvID,
	/*in*/ TNC_ConnectionID connectionID)
{
	wpa_printf(MSG_INFO, "IMV(hostap2) %s(imvID=%u, connectionID=%u)",
		   __func__, (unsigned) imvID, (unsigned) connectionID);

	return TNC_RESULT_SUCCESS;
}


TNC_Result TNC_IMV_Terminate(
	/*in*/ TNC_IMVID imvID)
{
	wpa_printf(MSG_INFO, "IMV(hostap2) %s(imvID=%u)",
		   __func__, (unsigned) imvID);

	return TNC_RESULT_SUCCESS;
}


TNC_Result TNC_IMV_ProvideBindFunction(
	/*in*/ TNC_IMVID imvID,
	/*in*/ TNC_TNCS_BindFunctionPointer bindFunction)
{
	TNC_Result res;

	wpa_printf(MSG_INFO, "IMV(hostap2) %s(imvID=%u)",
		   __func__, (unsigned) imvID);

	if (!initialized)
		return TNC_RESULT_NOT_INITIALIZED;

	if (imvID != my_id || !bindFunction)
		return TNC_RESULT_INVALID_PARAMETER;

	if (bindFunction(imvID, "TNC_TNCS_ReportMessageTypes",
			 (void **) &report_message_types) !=
	    TNC_RESULT_SUCCESS ||
	    !report_message_types)
		return TNC_RESULT_FATAL;

	if (bindFunction(imvID, "TNC_TNCS_SendMessage",
			 (void **) &send_message) != TNC_RESULT_SUCCESS ||
	    !send_message)
		return TNC_RESULT_FATAL;

	if (bindFunction(imvID, "TNC_TNCS_RequestHandshakeRetry",
			 (void **) &request_retry) != TNC_RESULT_SUCCESS ||
	    !request_retry)
		return TNC_RESULT_FATAL;

	if (bindFunction(imvID, "TNC_TNCS_ProvideRecommendation",
			 (void **) &provide_recomm) != TNC_RESULT_SUCCESS ||
	    !provide_recomm)
		return TNC_RESULT_FATAL;

	res = report_message_types(imvID, message_types,
				   ARRAY_SIZE(message_types));
	if (res != TNC_RESULT_SUCCESS)
		return res;

	return TNC_RESULT_SUCCESS;
}
