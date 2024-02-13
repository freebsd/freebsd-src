/* SPDX-License-Identifier: BSD-3-Clause */
/*  Copyright (c) 2024, Intel Corporation
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *
 *   1. Redistributions of source code must retain the above copyright notice,
 *      this list of conditions and the following disclaimer.
 *
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *
 *   3. Neither the name of the Intel Corporation nor the names of its
 *      contributors may be used to endorse or promote products derived from
 *      this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file ice_strings.c
 * @brief functions to convert enumerated values to human readable strings
 *
 * Contains various functions which convert enumerated values into human
 * readable strings. Primarily this is used for error values, such as the
 * ice_status enum, the ice_aq_err values, or standard sys/errno.h values.
 *
 * Additionally, various other driver enumerations which are displayed via
 * sysctl have converter functions.
 *
 * Some of the functions return struct ice_str_buf, instead of a character
 * string pointer. This is a trick to allow the function to create a struct
 * with space to convert unknown numeric values into a string, and return the
 * contents via copying the struct memory back. The functions then have an
 * associated macro to access the string value immediately. This allows the
 * functions to return static strings for known values, and convert unknown
 * values into a numeric representation. It also does not require
 * pre-allocating storage at each callsite, or using a local static value
 * which wouldn't be re-entrant, and could collide if multiple threads call
 * the function. The extra copies are somewhat annoying, but generally the
 * error functions aren't expected to be in a hot path so this is an
 * acceptable trade off.
 */

#include "ice_lib.h"

/**
 * ice_aq_str - Convert an AdminQ error into a string
 * @aq_err: the AQ error code to convert
 *
 * Convert the AdminQ status into its string name, if known. Otherwise, format
 * the error as an integer.
 */
struct ice_str_buf
_ice_aq_str(enum ice_aq_err aq_err)
{
	struct ice_str_buf buf = { .str = "" };
	const char *str = NULL;

	switch (aq_err) {
	case ICE_AQ_RC_OK:
		str = "OK";
		break;
	case ICE_AQ_RC_EPERM:
		str = "AQ_RC_EPERM";
		break;
	case ICE_AQ_RC_ENOENT:
		str = "AQ_RC_ENOENT";
		break;
	case ICE_AQ_RC_ESRCH:
		str = "AQ_RC_ESRCH";
		break;
	case ICE_AQ_RC_EINTR:
		str = "AQ_RC_EINTR";
		break;
	case ICE_AQ_RC_EIO:
		str = "AQ_RC_EIO";
		break;
	case ICE_AQ_RC_ENXIO:
		str = "AQ_RC_ENXIO";
		break;
	case ICE_AQ_RC_E2BIG:
		str = "AQ_RC_E2BIG";
		break;
	case ICE_AQ_RC_EAGAIN:
		str = "AQ_RC_EAGAIN";
		break;
	case ICE_AQ_RC_ENOMEM:
		str = "AQ_RC_ENOMEM";
		break;
	case ICE_AQ_RC_EACCES:
		str = "AQ_RC_EACCES";
		break;
	case ICE_AQ_RC_EFAULT:
		str = "AQ_RC_EFAULT";
		break;
	case ICE_AQ_RC_EBUSY:
		str = "AQ_RC_EBUSY";
		break;
	case ICE_AQ_RC_EEXIST:
		str = "AQ_RC_EEXIST";
		break;
	case ICE_AQ_RC_EINVAL:
		str = "AQ_RC_EINVAL";
		break;
	case ICE_AQ_RC_ENOTTY:
		str = "AQ_RC_ENOTTY";
		break;
	case ICE_AQ_RC_ENOSPC:
		str = "AQ_RC_ENOSPC";
		break;
	case ICE_AQ_RC_ENOSYS:
		str = "AQ_RC_ENOSYS";
		break;
	case ICE_AQ_RC_ERANGE:
		str = "AQ_RC_ERANGE";
		break;
	case ICE_AQ_RC_EFLUSHED:
		str = "AQ_RC_EFLUSHED";
		break;
	case ICE_AQ_RC_BAD_ADDR:
		str = "AQ_RC_BAD_ADDR";
		break;
	case ICE_AQ_RC_EMODE:
		str = "AQ_RC_EMODE";
		break;
	case ICE_AQ_RC_EFBIG:
		str = "AQ_RC_EFBIG";
		break;
	case ICE_AQ_RC_ESBCOMP:
		str = "AQ_RC_ESBCOMP";
		break;
	case ICE_AQ_RC_ENOSEC:
		str = "AQ_RC_ENOSEC";
		break;
	case ICE_AQ_RC_EBADSIG:
		str = "AQ_RC_EBADSIG";
		break;
	case ICE_AQ_RC_ESVN:
		str = "AQ_RC_ESVN";
		break;
	case ICE_AQ_RC_EBADMAN:
		str = "AQ_RC_EBADMAN";
		break;
	case ICE_AQ_RC_EBADBUF:
		str = "AQ_RC_EBADBUF";
		break;
	case ICE_AQ_RC_EACCES_BMCU:
		str = "AQ_RC_EACCES_BMCU";
		break;
	}

	if (str)
		snprintf(buf.str, ICE_STR_BUF_LEN, "%s", str);
	else
		snprintf(buf.str, ICE_STR_BUF_LEN, "%d", aq_err);

	return buf;
}

/**
 * ice_status_str - convert status err code to a string
 * @status: the status error code to convert
 *
 * Convert the status code into its string name if known.
 *
 * Otherwise, use the scratch space to format the status code into a number.
 */
struct ice_str_buf
_ice_status_str(enum ice_status status)
{
	struct ice_str_buf buf = { .str = "" };
	const char *str = NULL;

	switch (status) {
	case ICE_SUCCESS:
		str = "OK";
		break;
	case ICE_ERR_PARAM:
		str = "ICE_ERR_PARAM";
		break;
	case ICE_ERR_NOT_IMPL:
		str = "ICE_ERR_NOT_IMPL";
		break;
	case ICE_ERR_NOT_READY:
		str = "ICE_ERR_NOT_READY";
		break;
	case ICE_ERR_NOT_SUPPORTED:
		str = "ICE_ERR_NOT_SUPPORTED";
		break;
	case ICE_ERR_BAD_PTR:
		str = "ICE_ERR_BAD_PTR";
		break;
	case ICE_ERR_INVAL_SIZE:
		str = "ICE_ERR_INVAL_SIZE";
		break;
	case ICE_ERR_DEVICE_NOT_SUPPORTED:
		str = "ICE_ERR_DEVICE_NOT_SUPPORTED";
		break;
	case ICE_ERR_RESET_FAILED:
		str = "ICE_ERR_RESET_FAILED";
		break;
	case ICE_ERR_FW_API_VER:
		str = "ICE_ERR_FW_API_VER";
		break;
	case ICE_ERR_NO_MEMORY:
		str = "ICE_ERR_NO_MEMORY";
		break;
	case ICE_ERR_CFG:
		str = "ICE_ERR_CFG";
		break;
	case ICE_ERR_OUT_OF_RANGE:
		str = "ICE_ERR_OUT_OF_RANGE";
		break;
	case ICE_ERR_ALREADY_EXISTS:
		str = "ICE_ERR_ALREADY_EXISTS";
		break;
	case ICE_ERR_NVM:
		str = "ICE_ERR_NVM";
		break;
	case ICE_ERR_NVM_CHECKSUM:
		str = "ICE_ERR_NVM_CHECKSUM";
		break;
	case ICE_ERR_BUF_TOO_SHORT:
		str = "ICE_ERR_BUF_TOO_SHORT";
		break;
	case ICE_ERR_NVM_BLANK_MODE:
		str = "ICE_ERR_NVM_BLANK_MODE";
		break;
	case ICE_ERR_IN_USE:
		str = "ICE_ERR_IN_USE";
		break;
	case ICE_ERR_MAX_LIMIT:
		str = "ICE_ERR_MAX_LIMIT";
		break;
	case ICE_ERR_RESET_ONGOING:
		str = "ICE_ERR_RESET_ONGOING";
		break;
	case ICE_ERR_HW_TABLE:
		str = "ICE_ERR_HW_TABLE";
		break;
	case ICE_ERR_FW_DDP_MISMATCH:
		str = "ICE_ERR_FW_DDP_MISMATCH";
		break;
	case ICE_ERR_DOES_NOT_EXIST:
		str = "ICE_ERR_DOES_NOT_EXIST";
		break;
	case ICE_ERR_AQ_ERROR:
		str = "ICE_ERR_AQ_ERROR";
		break;
	case ICE_ERR_AQ_TIMEOUT:
		str = "ICE_ERR_AQ_TIMEOUT";
		break;
	case ICE_ERR_AQ_FULL:
		str = "ICE_ERR_AQ_FULL";
		break;
	case ICE_ERR_AQ_NO_WORK:
		str = "ICE_ERR_AQ_NO_WORK";
		break;
	case ICE_ERR_AQ_EMPTY:
		str = "ICE_ERR_AQ_EMPTY";
		break;
	case ICE_ERR_AQ_FW_CRITICAL:
		str = "ICE_ERR_AQ_FW_CRITICAL";
		break;
	}

	if (str)
		snprintf(buf.str, ICE_STR_BUF_LEN, "%s", str);
	else
		snprintf(buf.str, ICE_STR_BUF_LEN, "%d", status);

	return buf;
}

/**
 * ice_err_str - convert error code to a string
 * @err: the error code to convert
 *
 * Convert an error code into its string/macro name if known. Note, it doesn't
 * handle negated errors.
 *
 * Otherwise, use the scratch space to format the error into a number.
 */
struct ice_str_buf
_ice_err_str(int err)
{
	struct ice_str_buf buf = { .str = "" };
	const char *str = NULL;

	switch (err) {
	case 0:
		str = "OK";
		break;
	case EPERM:
		str = "EPERM";
		break;
	case ENOENT:
		str = "ENOENT";
		break;
	case ESRCH:
		str = "ESRCH";
		break;
	case EINTR:
		str = "EINTR";
		break;
	case EIO:
		str = "EIO";
		break;
	case ENXIO:
		str = "ENXIO";
		break;
	case E2BIG:
		str = "E2BIG";
		break;
	case ENOEXEC:
		str = "ENOEXEC";
		break;
	case EBADF:
		str = "EBADF";
		break;
	case ECHILD:
		str = "ECHILD";
		break;
	case EDEADLK:
		str = "EDEADLK";
		break;
	case ENOMEM:
		str = "ENOMEM";
		break;
	case EACCES:
		str = "EACCES";
		break;
	case EFAULT:
		str = "EFAULT";
		break;
	case ENOTBLK:
		str = "ENOTBLK";
		break;
	case EBUSY:
		str = "EBUSY";
		break;
	case EEXIST:
		str = "EEXIST";
		break;
	case EXDEV:
		str = "EXDEV";
		break;
	case ENODEV:
		str = "ENODEV";
		break;
	case ENOTDIR:
		str = "ENOTDIR";
		break;
	case EISDIR:
		str = "EISDIR";
		break;
	case EINVAL:
		str = "EINVAL";
		break;
	case ENFILE:
		str = "ENFILE";
		break;
	case EMFILE:
		str = "EMFILE";
		break;
	case ENOTTY:
		str = "ENOTTY";
		break;
	case ETXTBSY:
		str = "ETXTBSY";
		break;
	case EFBIG:
		str = "EFBIG";
		break;
	case ENOSPC:
		str = "ENOSPC";
		break;
	case ESPIPE:
		str = "ESPIPE";
		break;
	case EROFS:
		str = "EROFS";
		break;
	case EMLINK:
		str = "EMLINK";
		break;
	case EPIPE:
		str = "EPIPE";
		break;
	case EDOM:
		str = "EDOM";
		break;
	case ERANGE:
		str = "ERANGE";
		break;
	case EAGAIN:
		/* EWOULDBLOCK */
		str = "EAGAIN";
		break;
	case EINPROGRESS:
		str = "EINPROGRESS";
		break;
	case EALREADY:
		str = "EALREADY";
		break;
	case ENOTSOCK:
		str = "ENOTSOCK";
		break;
	case EDESTADDRREQ:
		str = "EDESTADDRREQ";
		break;
	case EMSGSIZE:
		str = "EMSGSIZE";
		break;
	case EPROTOTYPE:
		str = "EPROTOTYPE";
		break;
	case ENOPROTOOPT:
		str = "ENOPROTOOPT";
		break;
	case EPROTONOSUPPORT:
		str = "EPROTONOSUPPORT";
		break;
	case ESOCKTNOSUPPORT:
		str = "ESOCKTNOSUPPORT";
		break;
	case EOPNOTSUPP:
		str = "EOPNOTSUPP";
		break;
	case EPFNOSUPPORT:
		/* ENOTSUP */
		str = "EPFNOSUPPORT";
		break;
	case EAFNOSUPPORT:
		str = "EAFNOSUPPORT";
		break;
	case EADDRINUSE:
		str = "EADDRINUSE";
		break;
	case EADDRNOTAVAIL:
		str = "EADDRNOTAVAIL";
		break;
	case ENETDOWN:
		str = "ENETDOWN";
		break;
	case ENETUNREACH:
		str = "ENETUNREACH";
		break;
	case ENETRESET:
		str = "ENETRESET";
		break;
	case ECONNABORTED:
		str = "ECONNABORTED";
		break;
	case ECONNRESET:
		str = "ECONNRESET";
		break;
	case ENOBUFS:
		str = "ENOBUFS";
		break;
	case EISCONN:
		str = "EISCONN";
		break;
	case ENOTCONN:
		str = "ENOTCONN";
		break;
	case ESHUTDOWN:
		str = "ESHUTDOWN";
		break;
	case ETOOMANYREFS:
		str = "ETOOMANYREFS";
		break;
	case ETIMEDOUT:
		str = "ETIMEDOUT";
		break;
	case ECONNREFUSED:
		str = "ECONNREFUSED";
		break;
	case ELOOP:
		str = "ELOOP";
		break;
	case ENAMETOOLONG:
		str = "ENAMETOOLONG";
		break;
	case EHOSTDOWN:
		str = "EHOSTDOWN";
		break;
	case EHOSTUNREACH:
		str = "EHOSTUNREACH";
		break;
	case ENOTEMPTY:
		str = "ENOTEMPTY";
		break;
	case EPROCLIM:
		str = "EPROCLIM";
		break;
	case EUSERS:
		str = "EUSERS";
		break;
	case EDQUOT:
		str = "EDQUOT";
		break;
	case ESTALE:
		str = "ESTALE";
		break;
	case EREMOTE:
		str = "EREMOTE";
		break;
	case EBADRPC:
		str = "EBADRPC";
		break;
	case ERPCMISMATCH:
		str = "ERPCMISMATCH";
		break;
	case EPROGUNAVAIL:
		str = "EPROGUNAVAIL";
		break;
	case EPROGMISMATCH:
		str = "EPROGMISMATCH";
		break;
	case EPROCUNAVAIL:
		str = "EPROCUNAVAIL";
		break;
	case ENOLCK:
		str = "ENOLCK";
		break;
	case ENOSYS:
		str = "ENOSYS";
		break;
	case EFTYPE:
		str = "EFTYPE";
		break;
	case EAUTH:
		str = "EAUTH";
		break;
	case ENEEDAUTH:
		str = "ENEEDAUTH";
		break;
	case EIDRM:
		str = "EIDRM";
		break;
	case ENOMSG:
		str = "ENOMSG";
		break;
	case EOVERFLOW:
		str = "EOVERFLOW";
		break;
	case ECANCELED:
		str = "ECANCELED";
		break;
	case EILSEQ:
		str = "EILSEQ";
		break;
	case ENOATTR:
		str = "ENOATTR";
		break;
	case EDOOFUS:
		str = "EDOOFUS";
		break;
	case EBADMSG:
		str = "EBADMSG";
		break;
	case EMULTIHOP:
		str = "EMULTIHOP";
		break;
	case ENOLINK:
		str = "ENOLINK";
		break;
	case EPROTO:
		str = "EPROTO";
		break;
	case ENOTCAPABLE:
		str = "ENOTCAPABLE";
		break;
	case ECAPMODE:
		str = "ECAPMODE";
		break;
	case ENOTRECOVERABLE:
		str = "ENOTRECOVERABLE";
		break;
	case EOWNERDEAD:
		str = "EOWNERDEAD";
		break;
	}

	if (str)
		snprintf(buf.str, ICE_STR_BUF_LEN, "%s", str);
	else
		snprintf(buf.str, ICE_STR_BUF_LEN, "%d", err);

	return buf;
}

/**
 * ice_fec_str - convert fec mode enum to a string
 * @mode: the enum value to convert
 *
 * Convert an FEC mode enum to a string for display in a sysctl or log message.
 * Returns "Unknown" if the mode is not one of currently known FEC modes.
 */
const char *
ice_fec_str(enum ice_fec_mode mode)
{
	switch (mode) {
	case ICE_FEC_AUTO:
		return ICE_FEC_STRING_AUTO;
	case ICE_FEC_RS:
		return ICE_FEC_STRING_RS;
	case ICE_FEC_BASER:
		return ICE_FEC_STRING_BASER;
	case ICE_FEC_NONE:
		return ICE_FEC_STRING_NONE;
	case ICE_FEC_DIS_AUTO:
		return ICE_FEC_STRING_DIS_AUTO;
	}

	/* The compiler generates errors on unhandled enum values if we omit
	 * the default case.
	 */
	return "Unknown";
}

/**
 * ice_fc_str - convert flow control mode enum to a string
 * @mode: the enum value to convert
 *
 * Convert a flow control mode enum to a string for display in a sysctl or log
 * message. Returns "Unknown" if the mode is not one of currently supported or
 * known flow control modes.
 */
const char *
ice_fc_str(enum ice_fc_mode mode)
{
	switch (mode) {
	case ICE_FC_FULL:
		return ICE_FC_STRING_FULL;
	case ICE_FC_TX_PAUSE:
		return ICE_FC_STRING_TX;
	case ICE_FC_RX_PAUSE:
		return ICE_FC_STRING_RX;
	case ICE_FC_NONE:
		return ICE_FC_STRING_NONE;
	case ICE_FC_AUTO:
	case ICE_FC_PFC:
	case ICE_FC_DFLT:
		break;
	}

	/* The compiler generates errors on unhandled enum values if we omit
	 * the default case.
	 */
	return "Unknown";
}

/**
 * ice_fltr_flag_str - Convert filter flags to a string
 * @flag: the filter flags to convert
 *
 * Convert the u16 flag value of a filter into a readable string for
 * outputting in a sysctl.
 */
struct ice_str_buf
_ice_fltr_flag_str(u16 flag)
{
	struct ice_str_buf buf = { .str = "" };
	const char *str = NULL;

	switch (flag) {
	case ICE_FLTR_RX:
		str = "RX";
		break;
	case ICE_FLTR_TX:
		str = "TX";
		break;
	case ICE_FLTR_TX_RX:
		str = "TX_RX";
		break;
	default:
		break;
	}

	if (str)
		snprintf(buf.str, ICE_STR_BUF_LEN, "%s", str);
	else
		snprintf(buf.str, ICE_STR_BUF_LEN, "%u", flag);

	return buf;
}

/**
 * ice_log_sev_str - Convert log level to a string
 * @log_level: the log level to convert
 *
 * Convert the u8 log level of a FW logging module into a readable
 * string for outputting in a sysctl.
 */
struct ice_str_buf
_ice_log_sev_str(u8 log_level)
{
	struct ice_str_buf buf = { .str = "" };
	const char *str = NULL;

	switch (log_level) {
	case ICE_FWLOG_LEVEL_NONE:
		str = "none";
		break;
	case ICE_FWLOG_LEVEL_ERROR:
		str = "error";
		break;
	case ICE_FWLOG_LEVEL_WARNING:
		str = "warning";
		break;
	case ICE_FWLOG_LEVEL_NORMAL:
		str = "normal";
		break;
	case ICE_FWLOG_LEVEL_VERBOSE:
		str = "verbose";
		break;
	default:
		break;
	}

	if (str)
		snprintf(buf.str, ICE_STR_BUF_LEN, "%s", str);
	else
		snprintf(buf.str, ICE_STR_BUF_LEN, "%u", log_level);

	return buf;
}

/**
 * ice_fwd_act_str - convert filter action enum to a string
 * @action: the filter action to convert
 *
 * Convert an enum value of type enum ice_sw_fwd_act_type into a string, for
 * display in a sysctl filter list. Returns "UNKNOWN" for actions outside the
 * enumeration type.
 */
const char *
ice_fwd_act_str(enum ice_sw_fwd_act_type action)
{
	switch (action) {
	case ICE_FWD_TO_VSI:
		return "FWD_TO_VSI";
	case ICE_FWD_TO_VSI_LIST:
		return "FWD_TO_VSI_LIST";
	case ICE_FWD_TO_Q:
		return "FWD_TO_Q";
	case ICE_FWD_TO_QGRP:
		return "FWD_TO_QGRP";
	case ICE_DROP_PACKET:
		return "DROP_PACKET";
	case ICE_LG_ACTION:
		return "LG_ACTION";
	case ICE_INVAL_ACT:
		return "INVAL_ACT";
	}

	/* The compiler generates errors on unhandled enum values if we omit
	 * the default case.
	 */
	return "Unknown";
}

/**
 * ice_mdd_tx_tclan_str - Convert MDD Tx TCLAN event to a string
 * @event: the MDD event number to convert
 *
 * Convert the Tx TCLAN event value from the GL_MDET_TX_TCLAN register into
 * a human readable string for logging of MDD events.
 */
struct ice_str_buf
_ice_mdd_tx_tclan_str(u8 event)
{
	struct ice_str_buf buf = { .str = "" };
	const char *str = NULL;

	switch (event) {
	case 0:
		str = "Wrong descriptor format/order";
		break;
	case 1:
		str = "Descriptor fetch failed";
		break;
	case 2:
		str = "Tail descriptor not EOP/NOP";
		break;
	case 3:
		str = "False scheduling error";
		break;
	case 4:
		str = "Tail value larger than ring len";
		break;
	case 5:
		str = "Too many data commands";
		break;
	case 6:
		str = "Zero packets sent in quanta";
		break;
	case 7:
		str = "Packet too small or too big";
		break;
	case 8:
		str = "TSO length doesn't match sum";
		break;
	case 9:
		str = "TSO tail reached before TLEN";
		break;
	case 10:
		str = "TSO max 3 descs for headers";
		break;
	case 11:
		str = "EOP on header descriptor";
		break;
	case 12:
		str = "MSS is 0 or TLEN is 0";
		break;
	case 13:
		str = "CTX desc invalid IPSec fields";
		break;
	case 14:
		str = "Quanta invalid # of SSO packets";
		break;
	case 15:
		str = "Quanta bytes exceeds pkt_len*64";
		break;
	case 16:
		str = "Quanta exceeds max_cmds_in_sq";
		break;
	case 17:
		str = "incoherent last_lso_quanta";
		break;
	case 18:
		str = "incoherent TSO TLEN";
		break;
	case 19:
		str = "Quanta: too many descriptors";
		break;
	case 20:
		str = "Quanta: # of packets mismatch";
		break;
	default:
		break;
	}

	if (str)
		snprintf(buf.str, ICE_STR_BUF_LEN, "%s", str);
	else
		snprintf(buf.str, ICE_STR_BUF_LEN, "Unknown Tx TCLAN event %u", event);

	return buf;
}

/**
 * ice_mdd_tx_pqm_str - Convert MDD Tx PQM event to a string
 * @event: the MDD event number to convert
 *
 * Convert the Tx PQM event value from the GL_MDET_TX_PQM register into
 * a human readable string for logging of MDD events.
 */
struct ice_str_buf
_ice_mdd_tx_pqm_str(u8 event)
{
	struct ice_str_buf buf = { .str = "" };
	const char *str = NULL;

	switch (event) {
	case 0:
		str = "PCI_DUMMY_COMP";
		break;
	case 1:
		str = "PCI_UR_COMP";
		break;
	/* Index 2 is unused */
	case 3:
		str = "RCV_SH_BE_LSO";
		break;
	case 4:
		str = "Q_FL_MNG_EPY_CH";
		break;
	case 5:
		str = "Q_EPY_MNG_FL_CH";
		break;
	case 6:
		str = "LSO_NUMDESCS_ZERO";
		break;
	case 7:
		str = "LSO_LENGTH_ZERO";
		break;
	case 8:
		str = "LSO_MSS_BELOW_MIN";
		break;
	case 9:
		str = "LSO_MSS_ABOVE_MAX";
		break;
	case 10:
		str = "LSO_HDR_SIZE_ZERO";
		break;
	case 11:
		str = "RCV_CNT_BE_LSO";
		break;
	case 12:
		str = "SKIP_ONE_QT_ONLY";
		break;
	case 13:
		str = "LSO_PKTCNT_ZERO";
		break;
	case 14:
		str = "SSO_LENGTH_ZERO";
		break;
	case 15:
		str = "SSO_LENGTH_EXCEED";
		break;
	case 16:
		str = "SSO_PKTCNT_ZERO";
		break;
	case 17:
		str = "SSO_PKTCNT_EXCEED";
		break;
	case 18:
		str = "SSO_NUMDESCS_ZERO";
		break;
	case 19:
		str = "SSO_NUMDESCS_EXCEED";
		break;
	case 20:
		str = "TAIL_GT_RING_LENGTH";
		break;
	case 21:
		str = "RESERVED_DBL_TYPE";
		break;
	case 22:
		str = "ILLEGAL_HEAD_DROP_DBL";
		break;
	case 23:
		str = "LSO_OVER_COMMS_Q";
		break;
	case 24:
		str = "ILLEGAL_VF_QNUM";
		break;
	case 25:
		str = "QTAIL_GT_RING_LENGTH";
		break;
	default:
		break;
	}

	if (str)
		snprintf(buf.str, ICE_STR_BUF_LEN, "%s", str);
	else
		snprintf(buf.str, ICE_STR_BUF_LEN, "Unknown Tx PQM event %u", event);

	return buf;
}

/**
 * ice_mdd_rx_str - Convert MDD Rx queue event to a string
 * @event: the MDD event number to convert
 *
 * Convert the Rx queue event value from the GL_MDET_RX register into a human
 * readable string for logging of MDD events.
 */
struct ice_str_buf
_ice_mdd_rx_str(u8 event)
{
	struct ice_str_buf buf = { .str = "" };
	const char *str = NULL;

	switch (event) {
	case 1:
		str = "Descriptor fetch failed";
		break;
	default:
		break;
	}

	if (str)
		snprintf(buf.str, ICE_STR_BUF_LEN, "%s", str);
	else
		snprintf(buf.str, ICE_STR_BUF_LEN, "Unknown Rx event %u", event);

	return buf;
}

/**
 * ice_state_to_str - Convert the state enum to a string value
 * @state: the state bit to convert
 *
 * Converts a given state bit to its human readable string name. If the enum
 * value is unknown, returns NULL;
 */
const char *
ice_state_to_str(enum ice_state state)
{
	switch (state) {
	case ICE_STATE_CONTROLQ_EVENT_PENDING:
		return "CONTROLQ_EVENT_PENDING";
	case ICE_STATE_VFLR_PENDING:
		return "VFLR_PENDING";
	case ICE_STATE_MDD_PENDING:
		return "MDD_PENDING";
	case ICE_STATE_RESET_OICR_RECV:
		return "RESET_OICR_RECV";
	case ICE_STATE_RESET_PFR_REQ:
		return "RESET_PFR_REQ";
	case ICE_STATE_PREPARED_FOR_RESET:
		return "PREPARED_FOR_RESET";
	case ICE_STATE_RESET_FAILED:
		return "RESET_FAILED";
	case ICE_STATE_DRIVER_INITIALIZED:
		return "DRIVER_INITIALIZED";
	case ICE_STATE_NO_MEDIA:
		return "NO_MEDIA";
	case ICE_STATE_RECOVERY_MODE:
		return "RECOVERY_MODE";
	case ICE_STATE_ROLLBACK_MODE:
		return "ROLLBACK_MODE";
	case ICE_STATE_LINK_STATUS_REPORTED:
		return "LINK_STATUS_REPORTED";
	case ICE_STATE_ATTACHING:
		return "ATTACHING";
	case ICE_STATE_DETACHING:
		return "DETACHING";
	case ICE_STATE_LINK_DEFAULT_OVERRIDE_PENDING:
		return "LINK_DEFAULT_OVERRIDE_PENDING";
	case ICE_STATE_LLDP_RX_FLTR_FROM_DRIVER:
		return "LLDP_RX_FLTR_FROM_DRIVER";
	case ICE_STATE_MULTIPLE_TCS:
		return "MULTIPLE_TCS";
	case ICE_STATE_DO_FW_DEBUG_DUMP:
		return "DO_FW_DEBUG_DUMP";
	case ICE_STATE_LINK_ACTIVE_ON_DOWN:
		return "LINK_ACTIVE_ON_DOWN";
	case ICE_STATE_FIRST_INIT_LINK:
		return "FIRST_INIT_LINK";
	case ICE_STATE_LAST:
		return NULL;
	}

	return NULL;
}

/**
 * ice_fw_module_str - Convert a FW logging module to a string name
 * @module: the module to convert
 *
 * Given a FW logging module id, convert it to a shorthand human readable
 * name, for generating sysctl tunables.
 */
const char *
ice_fw_module_str(enum ice_aqc_fw_logging_mod module)
{
	switch (module) {
	case ICE_AQC_FW_LOG_ID_GENERAL:
		return "general";
	case ICE_AQC_FW_LOG_ID_CTRL:
		return "ctrl";
	case ICE_AQC_FW_LOG_ID_LINK:
		return "link";
	case ICE_AQC_FW_LOG_ID_LINK_TOPO:
		return "link_topo";
	case ICE_AQC_FW_LOG_ID_DNL:
		return "dnl";
	case ICE_AQC_FW_LOG_ID_I2C:
		return "i2c";
	case ICE_AQC_FW_LOG_ID_SDP:
		return "sdp";
	case ICE_AQC_FW_LOG_ID_MDIO:
		return "mdio";
	case ICE_AQC_FW_LOG_ID_ADMINQ:
		return "adminq";
	case ICE_AQC_FW_LOG_ID_HDMA:
		return "hdma";
	case ICE_AQC_FW_LOG_ID_LLDP:
		return "lldp";
	case ICE_AQC_FW_LOG_ID_DCBX:
		return "dcbx";
	case ICE_AQC_FW_LOG_ID_DCB:
		return "dcb";
	case ICE_AQC_FW_LOG_ID_XLR:
		return "xlr";
	case ICE_AQC_FW_LOG_ID_NVM:
		return "nvm";
	case ICE_AQC_FW_LOG_ID_AUTH:
		return "auth";
	case ICE_AQC_FW_LOG_ID_VPD:
		return "vpd";
	case ICE_AQC_FW_LOG_ID_IOSF:
		return "iosf";
	case ICE_AQC_FW_LOG_ID_PARSER:
		return "parser";
	case ICE_AQC_FW_LOG_ID_SW:
		return "sw";
	case ICE_AQC_FW_LOG_ID_SCHEDULER:
		return "scheduler";
	case ICE_AQC_FW_LOG_ID_TXQ:
		return "txq";
	case ICE_AQC_FW_LOG_ID_RSVD:
		return "acl";
	case ICE_AQC_FW_LOG_ID_POST:
		return "post";
	case ICE_AQC_FW_LOG_ID_WATCHDOG:
		return "watchdog";
	case ICE_AQC_FW_LOG_ID_TASK_DISPATCH:
		return "task_dispatch";
	case ICE_AQC_FW_LOG_ID_MNG:
		return "mng";
	case ICE_AQC_FW_LOG_ID_SYNCE:
		return "synce";
	case ICE_AQC_FW_LOG_ID_HEALTH:
		return "health";
	case ICE_AQC_FW_LOG_ID_TSDRV:
		return "tsdrv";
	case ICE_AQC_FW_LOG_ID_PFREG:
		return "pfreg";
	case ICE_AQC_FW_LOG_ID_MDLVER:
		return "mdlver";
	case ICE_AQC_FW_LOG_ID_MAX:
		return "unknown";
	}

	/* The compiler generates errors on unhandled enum values if we omit
	 * the default case.
	 */
	return "unknown";
}

/**
 * ice_fw_lldp_status - Convert FW LLDP status to a string
 * @lldp_status: firmware LLDP status value to convert
 *
 * Given the FW LLDP status, convert it to a human readable string.
 */
struct ice_str_buf
_ice_fw_lldp_status(u32 lldp_status)
{
	struct ice_str_buf buf = { .str = "" };
	const char *str = NULL;

	switch (lldp_status)
	{
	case ICE_LLDP_ADMINSTATUS_DIS:
		str = "DISABLED";
		break;
	case ICE_LLDP_ADMINSTATUS_ENA_RX:
		str = "ENA_RX";
		break;
	case ICE_LLDP_ADMINSTATUS_ENA_TX:
		str = "ENA_TX";
		break;
	case ICE_LLDP_ADMINSTATUS_ENA_RXTX:
		str = "ENA_RXTX";
		break;
	case 0xF:
		str = "NVM_DEFAULT";
		break;
	}

	if (str)
		snprintf(buf.str, ICE_STR_BUF_LEN, "%s", str);
	else
		snprintf(buf.str, ICE_STR_BUF_LEN, "Unknown LLDP status %u", lldp_status);

	return buf;
}
