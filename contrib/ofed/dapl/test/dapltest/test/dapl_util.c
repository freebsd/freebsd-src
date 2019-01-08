/*
 * Copyright (c) 2002-2005, Network Appliance, Inc. All rights reserved.
 *
 * This Software is licensed under one of the following licenses:
 *
 * 1) under the terms of the "Common Public License 1.0" a copy of which is
 *    in the file LICENSE.txt in the root directory. The license is also
 *    available from the Open Source Initiative, see
 *    http://www.opensource.org/licenses/cpl.php.
 *
 * 2) under the terms of the "The BSD License" a copy of which is in the file
 *    LICENSE2.txt in the root directory. The license is also available from
 *    the Open Source Initiative, see
 *    http://www.opensource.org/licenses/bsd-license.php.
 *
 * 3) under the terms of the "GNU General Public License (GPL) Version 2" a 
 *    copy of which is in the file LICENSE3.txt in the root directory. The 
 *    license is also available from the Open Source Initiative, see
 *    http://www.opensource.org/licenses/gpl-license.php.
 *
 * Licensee has the right to choose one of the above licenses.
 *
 * Redistributions of source code must retain the above copyright
 * notice and one of the license notices.
 *
 * Redistributions in binary form must reproduce both the above copyright
 * notice, one of the license notices in the documentation
 * and/or other materials provided with the distribution.
 */

#include "dapl_proto.h"

/*
 * Map DAT_RETURN values to readable strings,
 * but don't assume the values are zero-based or contiguous.
 */
const char *DT_RetToString(DAT_RETURN ret_value)
{
	const char *major_msg, *minor_msg;
	int sz;
	char *errmsg;

	dat_strerror(ret_value, &major_msg, &minor_msg);

	sz = strlen(major_msg) + strlen(minor_msg) + 2;
/*
 * FIXME: The callers of this function are not freeing
 * the errmsg string. Hence there is a memory leak 
 * (this function is likely only used on error paths, 
 *  so the consequences may not be that dire).
 */
	errmsg = DT_Mdep_Malloc(sz);
	strcpy(errmsg, major_msg);
	strcat(errmsg, " ");
	strcat(errmsg, minor_msg);

	return errmsg;
}

/*
 * Map DAT_RETURN values to readable strings,
 * but don't assume the values are zero-based or contiguous.
 */
const char *DT_TransferTypeToString(DT_Transfer_Type type)
{
	static char *DT_Type[] = {
		"RR",
		"RW",
		"SR"
	};

	if ((0 <= type) && (type <= 2)) {
		return DT_Type[type];
	} else {
		return "Error: Unkown Transfer Type";
	}
}

/*
 * Map DAT_ASYNC_ERROR_CODE values to readable strings
 */
const char *DT_AsyncErr2Str(DAT_EVENT_NUMBER error_code)
{
	unsigned int i;
	static struct {
		const char *name;
		DAT_RETURN value;
	} dat_errors[] = {
#   define DATxx(x) { # x, x }
		DATxx(DAT_DTO_COMPLETION_EVENT),
		    DATxx(DAT_RMR_BIND_COMPLETION_EVENT),
		    DATxx(DAT_CONNECTION_REQUEST_EVENT),
		    DATxx(DAT_CONNECTION_EVENT_ESTABLISHED),
		    DATxx(DAT_CONNECTION_EVENT_PEER_REJECTED),
		    DATxx(DAT_CONNECTION_EVENT_NON_PEER_REJECTED),
		    DATxx(DAT_CONNECTION_EVENT_ACCEPT_COMPLETION_ERROR),
		    DATxx(DAT_CONNECTION_EVENT_DISCONNECTED),
		    DATxx(DAT_CONNECTION_EVENT_BROKEN),
		    DATxx(DAT_CONNECTION_EVENT_TIMED_OUT),
		    DATxx(DAT_ASYNC_ERROR_EVD_OVERFLOW),
		    DATxx(DAT_ASYNC_ERROR_IA_CATASTROPHIC),
		    DATxx(DAT_ASYNC_ERROR_EP_BROKEN),
		    DATxx(DAT_ASYNC_ERROR_TIMED_OUT),
		    DATxx(DAT_ASYNC_ERROR_PROVIDER_INTERNAL_ERROR),
		    DATxx(DAT_SOFTWARE_EVENT)
#   undef DATxx
	};
#   define NUM_ERRORS (sizeof (dat_errors)/sizeof (dat_errors[0]))

	for (i = 0; i < NUM_ERRORS; i++) {
		if (dat_errors[i].value == error_code) {
			return (dat_errors[i].name);
		}
	}

	return ("Invalid_DAT_EVENT_NUMBER");
}

/*
 * Map DAT_EVENT_CODE values to readable strings
 */
const char *DT_EventToSTr(DAT_EVENT_NUMBER event_code)
{
	unsigned int i;
	static struct {
		const char *name;
		DAT_RETURN value;
	} dat_events[] = {
#   define DATxx(x) { # x, x }
		DATxx(DAT_DTO_COMPLETION_EVENT),
		    DATxx(DAT_RMR_BIND_COMPLETION_EVENT),
		    DATxx(DAT_CONNECTION_REQUEST_EVENT),
		    DATxx(DAT_CONNECTION_EVENT_ESTABLISHED),
		    DATxx(DAT_CONNECTION_EVENT_PEER_REJECTED),
		    DATxx(DAT_CONNECTION_EVENT_NON_PEER_REJECTED),
		    DATxx(DAT_CONNECTION_EVENT_ACCEPT_COMPLETION_ERROR),
		    DATxx(DAT_CONNECTION_EVENT_DISCONNECTED),
		    DATxx(DAT_CONNECTION_EVENT_BROKEN),
		    DATxx(DAT_CONNECTION_EVENT_TIMED_OUT),
		    DATxx(DAT_CONNECTION_EVENT_UNREACHABLE),
		    DATxx(DAT_ASYNC_ERROR_EVD_OVERFLOW),
		    DATxx(DAT_ASYNC_ERROR_IA_CATASTROPHIC),
		    DATxx(DAT_ASYNC_ERROR_EP_BROKEN),
		    DATxx(DAT_ASYNC_ERROR_TIMED_OUT),
		    DATxx(DAT_ASYNC_ERROR_PROVIDER_INTERNAL_ERROR),
		    DATxx(DAT_SOFTWARE_EVENT)
#   undef DATxx
	};
#   define NUM_EVENTS (sizeof (dat_events)/sizeof (dat_events[0]))

	for (i = 0; i < NUM_EVENTS; i++) {
		if (dat_events[i].value == event_code) {
			return (dat_events[i].name);
		}
	}

	return ("Invalid_DAT_EVENT_NUMBER");
}

/*
 * Map DAT_EP_STATE_CODE values to readable strings
 */
const char *DT_State2Str(DAT_EP_STATE state_code)
{
	unsigned int i;
	static struct {
		const char *name;
		DAT_RETURN value;
	} dat_state[] = {
#   define DATxx(x) { # x, x }
		DATxx(DAT_EP_STATE_UNCONNECTED),
		    DATxx(DAT_EP_STATE_RESERVED),
		    DATxx(DAT_EP_STATE_PASSIVE_CONNECTION_PENDING),
		    DATxx(DAT_EP_STATE_ACTIVE_CONNECTION_PENDING),
		    DATxx(DAT_EP_STATE_TENTATIVE_CONNECTION_PENDING),
		    DATxx(DAT_EP_STATE_CONNECTED),
		    DATxx(DAT_EP_STATE_DISCONNECT_PENDING),
		    DATxx(DAT_EP_STATE_ERROR)
#   undef DATxx
	};
#   define NUM_STATES (sizeof (dat_state)/sizeof (dat_state[0]))

	for (i = 0; i < NUM_STATES; i++) {
		if (dat_state[i].value == state_code) {
			return (dat_state[i].name);
		}
	}

	return ("Invalid_DAT_STATE_NUMBER");
}

/*
 * A couple of round-up routines (for pointers and counters)
 * which both assume a power-of-two 'align' factor,
 * and do the correct thing if align == 0.
 */
unsigned char *DT_AlignPtr(void *val, DAT_COUNT align)
{
	if (align) {
#if defined(_WIN64)
		return ((unsigned char *)
			(((uint64_t) val + ((uint64_t) align) -
			  1) & ~(((uint64_t) align) - 1)));
#else
		return ((unsigned char *)
			(((unsigned long)val + ((unsigned long)align) -
			  1) & ~(((unsigned long)align) - 1)));
#endif
	}
	return (val);
}

DAT_COUNT DT_RoundSize(DAT_COUNT val, DAT_COUNT align)
{
	if (align) {
		return (((val + align - 1) & ~(align - 1)));
	}
	return (val);
}
