/*
 * Copyright (c) 1997, 1999 Hellmuth Michaelis. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *---------------------------------------------------------------------------
 *
 *	printing cause values
 *	---------------------
 *
 *	$Id: pcause.c,v 1.13 2000/10/09 12:53:29 hm Exp $
 *
 * $FreeBSD$
 *
 *	last edit-date: [Mon Dec 13 21:48:07 1999]
 *
 *---------------------------------------------------------------------------*/

#include "isdnd.h"

static char *cause_i4b_tab[CAUSE_I4B_MAX+1];
static char *cause_q850_tab[CAUSE_Q850_MAX];

char *
print_i4b_cause(cause_t code)
{
	static char error_message[128];

	snprintf(error_message, sizeof(error_message), "%d: ", GET_CAUSE_VAL(code));

	switch(GET_CAUSE_TYPE(code))
	{
		case CAUSET_Q850:
			strcat(error_message, cause_q850_tab[GET_CAUSE_VAL(code)]);
			strcat(error_message, " (Q.850)");
			break;

		case CAUSET_I4B:
			if((GET_CAUSE_VAL(code) < CAUSE_I4B_NORMAL) ||
			   (GET_CAUSE_VAL(code) >= CAUSE_I4B_MAX))
			{
				SET_CAUSE_VAL(code, CAUSE_I4B_MAX);
			}
			strcat(error_message, cause_i4b_tab[GET_CAUSE_VAL(code)]);
			strcat(error_message, " (I4B)");
			break;

		default:
			strcat(error_message, "ERROR: unknown cause type!");
			break;
	}
	return(error_message);
}

static char *cause_i4b_tab[CAUSE_I4B_MAX+1] = {
	"normal call clearing",
	"user busy",
	"channel not available",
	"incompatible source or destination",
	"call rejected",
	"destination out of order",
	"temporary failure",
	"layer 1 error / persistent deactivation",
	"dialing impossible on leased line",	
	"ERROR, invalid I4B cause value!"
};

static char *cause_q850_tab[CAUSE_Q850_MAX] = {
	"Normal D-channel shutdown",
	"Unallocated (unassigned) number",
	"No route to specified transit network (national use)",
	"No route to destination",
	"Send special information tone",
	"Misdialled trunk prefix (national use)",
	"Channel unacceptable",
	"Call awarded and being delivered in an established channel",
	"Preemption",
	"Preemption - circuit reserved for reuse",

/*10*/	"cause code 10: error, unassigned in Q.850 (03/93)",
	"cause code 11: error, unassigned in Q.850 (03/93)",
	"cause code 12: error, unassigned in Q.850 (03/93)",
	"cause code 13: error, unassigned in Q.850 (03/93)",
	"cause code 14: error, unassigned in Q.850 (03/93)",
	"cause code 15: error, unassigned in Q.850 (03/93)",
	"Normal call clearing",
	"User busy",
	"No user responding",
	"No answer from user (user alerted)",

/*20*/	"Subscriber absent",
	"Call rejected",
	"Number changed",
	"cause code 23: error, unassigned in Q.850 (03/93)",
	"cause code 24: error, unassigned in Q.850 (03/93)",
	"cause code 25: error, unassigned in Q.850 (03/93)",
	"Non-selected user clearing",
	"Destination out of order",
	"Invalid number format",
	"Facility rejected",

/*30*/	"Response to STATUS ENQUIRY",
	"Normal, unspecified",
	"cause code 32: error, unassigned in Q.850 (03/93)",
	"cause code 33: error, unassigned in Q.850 (03/93)",	
	"No circuit / channel available",
	"cause code 35: error, unassigned in Q.850 (03/93)",
	"cause code 36: error, unassigned in Q.850 (03/93)",
	"cause code 37: error, unassigned in Q.850 (03/93)",	
	"Network out of order",
	"Permanent frame mode connection out of service",

/*40*/	"Permanent frame mode connection operational",
	"Temporary failure",
	"Switching equipment congestion",
	"Access information discarded",
	"Requested circuit/channel not available",
	"cause code 45: error, unassigned in Q.850 (03/93)",
	"Precedence call blocked",
	"Resources unavailable, unspecified",
	"cause code 48: error, unassigned in Q.850 (03/93)",
	"Quality of service unavailable",

/*50*/	"Requested facility not subscribed",
	"cause code 51: error, unassigned in Q.850 (03/93)",
	"cause code 52: error, unassigned in Q.850 (03/93)",
	"Outgoing calls barred within CUG",
	"cause code 54: error, unassigned in Q.850 (03/93)",
	"Incoming calls barred within CUG",
	"cause code 56: error, unassigned in Q.850 (03/93)",
	"Bearer capability not authorized",
	"Bearer capability not presently available",
	"cause code 59: error, unassigned in Q.850 (03/93)",
	
/*60*/	"cause code 60: error, unassigned in Q.850 (03/93)",
	"cause code 61: error, unassigned in Q.850 (03/93)",	
	"Inconsistenciy in designated outg. access info and subscriber class",
	"Service or option not available, unspecified",
	"cause code 64: error, unassigned in Q.850 (03/93)",	
	"Bearer capability not implemented",
	"Channel type not implemented",
	"cause code 67: error, unassigned in Q.850 (03/93)",
	"cause code 68: error, unassigned in Q.850 (03/93)",
	"Requested facility not implemented",

/*70*/	"Only restricted digital information bearer capability is available",
	"cause code 71: error, unassigned in Q.850 (03/93)",
	"cause code 72: error, unassigned in Q.850 (03/93)",
	"cause code 73: error, unassigned in Q.850 (03/93)",
	"cause code 74: error, unassigned in Q.850 (03/93)",
	"cause code 75: error, unassigned in Q.850 (03/93)",
	"cause code 76: error, unassigned in Q.850 (03/93)",
	"cause code 77: error, unassigned in Q.850 (03/93)",
	"cause code 78: error, unassigned in Q.850 (03/93)",	
	"Service or option not implemented, unspecified",

/*80*/	"cause code 80: error, unassigned in Q.850 (03/93)",
	"Invalid call reference value",
	"Identified channel does not exist",
	"A suspended call exists, but this call identity does not",
	"Call identity in use",
	"No call suspended",
	"Call having the requested call identity has been cleared",
	"User not member of CUG",
	"Incompatible destination",
	"cause code 89: error, unassigned in Q.850 (03/93)",
	
/*90*/	"Non-existent CUG",
	"Invalid transit network selection",
	"cause code 92: error, unassigned in Q.850 (03/93)",
	"cause code 93: error, unassigned in Q.850 (03/93)",
	"cause code 94: error, unassigned in Q.850 (03/93)",
	"Invalid message, unspecified",
	"Mandatory information element is missing",
	"Message type non-existent or not implemented",
	"Message not compatible with call state or message type non-existent or not implemented",
	"Information element/parameter non-existent or not implemented",

/*100*/	"Invalid information element contents",
	"Message not compatible with call state",
	"Recovery on timer expiry",
	"Parameter non-existent or not implemented, passed on",
	"cause code 104: error, unassigned in Q.850 (03/93)",
	"cause code 105: error, unassigned in Q.850 (03/93)",
	"cause code 106: error, unassigned in Q.850 (03/93)",
	"cause code 107: error, unassigned in Q.850 (03/93)",
	"cause code 108: error, unassigned in Q.850 (03/93)",
	"cause code 109: error, unassigned in Q.850 (03/93)",	

/*110*/	"Message with unrecognized parameter, discarded",
	"Protocol error, unspecified",
	"cause code 112: error, unassigned in Q.850 (03/93)",
	"cause code 113: error, unassigned in Q.850 (03/93)",
	"cause code 114: error, unassigned in Q.850 (03/93)",
	"cause code 115: error, unassigned in Q.850 (03/93)",
	"cause code 116: error, unassigned in Q.850 (03/93)",
	"cause code 117: error, unassigned in Q.850 (03/93)",
	"cause code 118: error, unassigned in Q.850 (03/93)",
	"cause code 119: error, unassigned in Q.850 (03/93)",

/*120*/	"cause code 120: error, unassigned in Q.850 (03/93)",
	"cause code 121: error, unassigned in Q.850 (03/93)",
	"cause code 122: error, unassigned in Q.850 (03/93)",
	"cause code 123: error, unassigned in Q.850 (03/93)",
	"cause code 124: error, unassigned in Q.850 (03/93)",
	"cause code 125: error, unassigned in Q.850 (03/93)",
	"cause code 126: error, unassigned in Q.850 (03/93)",
	"Interworking, unspecified"
};

/* EOF */
