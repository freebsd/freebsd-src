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
 *	$Id: pcause_q850.c,v 1.6 1999/12/13 21:25:26 hm Exp $
 *
 * $FreeBSD$
 *
 *	last edit-date: [Mon Dec 13 21:56:18 1999]
 *
 *---------------------------------------------------------------------------*/

#include "trace.h"
#include "pcause_q850.h"

char *
print_cause_q850(unsigned char code)
{
	static char error_message[120];
	char *e;

	switch(code)
	{
		case CAUSE_Q850_SHUTDN:
			e = "normal D-channel shutdown";
			break;

		case CAUSE_Q850_NUNALLC:
			e = "Unallocated (unassigned) number";
			break;

		case CAUSE_Q850_NRTTN:
			e = "No route to specified transit network";
			break;

		case CAUSE_Q850_NRTDST:
			e = "No route to destination";
			break;

		case CAUSE_Q850_SSINFTN:
			e = "Send special information tone";
			break;

		case CAUSE_Q850_MDIALTP:
			e = "Misdialled trunk prefix";
			break;

		case CAUSE_Q850_CHUNACC:
			e = "Channel unacceptable";
			break;

		case CAUSE_Q850_CALLAWD:
			e = "Call awarded and being delivered in an established channel";
			break;

		case CAUSE_Q850_PREEMPT:
			e = "Preemption";
			break;

		case CAUSE_Q850_PREECRR:
			e = "Preemption - circuit reserved for reuse";
			break;

		case CAUSE_Q850_NCCLR:
			e = "Normal call clearing";
			break;

		case CAUSE_Q850_USRBSY:
			e = "User busy";
			break;

		case CAUSE_Q850_NOUSRRSP:
			e = "No user responding";
			break;

		case CAUSE_Q850_NOANSWR:
			e = "No answer from user (user alerted)";
			break;

		case CAUSE_Q850_SUBSABS:
			e = "Subscriber absent";
			break;

		case CAUSE_Q850_CALLREJ:
			e = "Call rejected";
			break;

		case CAUSE_Q850_NUCHNG:
			e = "Number changed";
			break;

		case CAUSE_Q850_NONSELUC:
			e = "Non-selected user clearing";
			break;

		case CAUSE_Q850_DSTOOORDR:
			e = "Destination out of order";
			break;

		case CAUSE_Q850_INVNUFMT:
			e = "Invalid number format";
			break;

		case CAUSE_Q850_FACREJ:
			e = "Facility rejected";
			break;

		case CAUSE_Q850_STENQRSP:
			e = "Response to STATUS ENQUIRY";
			break;

		case CAUSE_Q850_NORMUNSP:
			e = "Normal, unspecified";
			break;

		case CAUSE_Q850_NOCAVAIL:
			e = "No circuit / channel available";
			break;

		case CAUSE_Q850_NETOOORDR:
			e = "Network out of order";
			break;

		case CAUSE_Q850_PFMCDOOSERV:
			e = "Permanent frame mode connection out of service";
			break;

		case CAUSE_Q850_PFMCOPER:
			e = "Permanent frame mode connection operational";
			break;

		case CAUSE_Q850_TMPFAIL:
			e = "Temporary failure";
			break;

		case CAUSE_Q850_SWEQCONG:
			e = "Switching equipment congestion";
			break;

		case CAUSE_Q850_ACCINFDIS:
			e = "Access information discarded";
			break;

		case CAUSE_Q850_REQCNOTAV:
			e = "Requested circuit/channel not available";
			break;

		case CAUSE_Q850_PRECALBLK:
			e = "Precedence call blocked";
			break;

		case CAUSE_Q850_RESUNAVAIL:
			e = "Resources unavailable, unspecified";
			break;

		case CAUSE_Q850_QOSUNAVAIL:
			e = "Quality of service unavailable";
			break;

		case CAUSE_Q850_REQSERVNS:
			e = "Requested facility not subscribed";
			break;

		case CAUSE_Q850_OCBARRCUG:
			e = "Outgoing calls barred within CUG";
			break;

		case CAUSE_Q850_ICBARRCUG:
			e = "Incoming calls barred within CUG";
			break;

		case CAUSE_Q850_BCAPNAUTH:
			e = "Bearer capability not authorized";
			break;

		case CAUSE_Q850_BCAPNAVAIL:
			e = "Bearer capability not presently available";
			break;

		case CAUSE_Q850_INCSTOACISC:
			e = "Inconsistenciy in designated outg. access info and subscriber class";
			break;

		case CAUSE_Q850_SOONOTAVAIL:
			e = "Service or option not available, unspecified";
			break;

		case CAUSE_Q850_BCAPNOTIMPL:
			e = "Bearer capability not implemented";
			break;

		case CAUSE_Q850_CHTYPNIMPL:
			e = "Channel type not implemented";
			break;

		case CAUSE_Q850_REQFACNIMPL:
			e = "Requested facility not implemented";
			break;

		case CAUSE_Q850_ORDINBCAVL:
			e = "Only restricted digital information bearer capability is available";
			break;

		case CAUSE_Q850_SOONOTIMPL:
			e = "Service or option not implemented, unspecified";
			break;

		case CAUSE_Q850_INVCLRFVAL:
			e = "Invalid call reference value";
			break;

		case CAUSE_Q850_IDCHDNOEX:
			e = "Identified channel does not exist";
			break;

		case CAUSE_Q850_SUSCAEXIN:
			e = "A suspended call exists, but this call identity does not";
			break;

		case CAUSE_Q850_CLIDINUSE:
			e = "Call identity in use";
			break;

		case CAUSE_Q850_NOCLSUSP:
			e = "No call suspended";
			break;

		case CAUSE_Q850_CLIDCLRD:
			e = "Call having the requested call identity has been cleared";
			break;

		case CAUSE_Q850_UNOTMEMCUG:
			e = "User not member of CUG";
			break;

		case CAUSE_Q850_INCDEST:
			e = "Incompatible destination";
			break;

		case CAUSE_Q850_NONEXCUG:
			e = "Non-existent CUG";
			break;

		case CAUSE_Q850_INVNTWSEL:
			e = "Invalid transit network selection";
			break;

		case CAUSE_Q850_INVMSG:
			e = "Invalid message, unspecified";
			break;

		case CAUSE_Q850_MIEMISS:
			e = "Mandatory information element is missing";
			break;

		case CAUSE_Q850_MSGTNI:
			e = "Message type non-existent or not implemented";
			break;

		case CAUSE_Q850_MSGNCMPT:
			e = "Msg incompatible with call state/message type non-existent/not implemented";
			break;

		case CAUSE_Q850_IENENI:
			e = "Information element/parameter non-existent or not implemented";
			break;

		case CAUSE_Q850_INVIEC:
			e = "Invalid information element contents";
			break;

		case CAUSE_Q850_MSGNCWCS:
			e = "Message not compatible with call state";
			break;

		case CAUSE_Q850_RECOTIMEXP:
			e = "Recovery on timer expiry";
			break;

		case CAUSE_Q850_PARMNENIPO:
			e = "Parameter non-existent or not implemented, passed on";
			break;

		case CAUSE_Q850_MSGUNRDPRM:
			e = "Message with unrecognized parameter, discarded";
			break;

		case CAUSE_Q850_PROTERR:
			e = "Protocol error, unspecified";
			break;

		case CAUSE_Q850_INTWRKU:
			e = "Interworking, unspecified";
			break;

		default:
			e = "ERROR, unknown cause value!";
			break;
	}

	sprintf(error_message, "%d: %s (Q.850)", code, e);
	return(error_message);
}

/* EOF */
