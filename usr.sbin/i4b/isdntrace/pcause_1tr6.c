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
 *	$Id: pcause_1tr6.c,v 1.6 1999/12/13 21:25:26 hm Exp $
 *
 * $FreeBSD: src/usr.sbin/i4b/isdntrace/pcause_1tr6.c,v 1.6 1999/12/14 21:07:49 hm Exp $
 *
 *	last edit-date: [Mon Dec 13 21:56:03 1999]
 *
 *---------------------------------------------------------------------------*/

#include "trace.h"
#include "pcause_1tr6.h"

char *
print_cause_1tr6(unsigned char code)
{
	static char error_message[120];
	char *e;

	switch(code)
	{
		case CAUSE_1TR6_SHUTDN:
			e = "normal D-channel shutdown";
			break;

		case CAUSE_1TR6_ICRV:
			e = "invalid call reference value";
			break;

		case CAUSE_1TR6_BSNI:
			e = "bearer service not implemented";
			break;

		case CAUSE_1TR6_CIDNE:
			e = "call identity does not exist";
			break;

		case CAUSE_1TR6_CIIU:
			e = "call identity in use";
			break;

		case CAUSE_1TR6_NCA:
			e = "no channel available";
			break;

		case CAUSE_1TR6_RFNI:
			e = "requested facility not implemented";
			break;

		case CAUSE_1TR6_RFNS:
			e = "requested facility not subscribed";
			break;

		case CAUSE_1TR6_OCB:
			e = "outgoing calls barred";
			break;

		case CAUSE_1TR6_UAB:
			e = "user access busy";
			break;

		case CAUSE_1TR6_NECUG:
			e = "non existent CUG";
			break;

		case CAUSE_1TR6_NECUG1:
			e = "non existent CUG";
			break;

		case CAUSE_1TR6_SPV:
			e = "kommunikationsbeziehung als SPV nicht erlaubt";
			break;

		case CAUSE_1TR6_DNO:
			e = "destination not obtainable";
			break;

		case CAUSE_1TR6_NC:
			e = "number changed";
			break;

		case CAUSE_1TR6_OOO:
			e = "out of order";
			break;

		case CAUSE_1TR6_NUR:
			e = "no user responding";
			break;

		case CAUSE_1TR6_UB:
			e = "user busy";
			break;

		case CAUSE_1TR6_ICB:
			e = "incoming calls barred";
			break;

		case CAUSE_1TR6_CR:
			e = "call rejected";
			break;

		case CAUSE_1TR6_NCO:
			e = "network congestion";
			break;

		case CAUSE_1TR6_RUI:
			e = "remote user initiated";
			break;

		case CAUSE_1TR6_LPE:
			e = "local procedure error";
			break;

		case CAUSE_1TR6_RPE:
			e = "remote procedure error";
			break;

		case CAUSE_1TR6_RUS:
			e = "remote user suspended";
			break;

		case CAUSE_1TR6_RUR:
			e = "remote user resumed";
			break;

		case CAUSE_1TR6_UIDL:
			e = "user info discharded locally";
			break;

		default:
			e = "UNKNOWN error occured";
			break;
	}

	sprintf(error_message, "0x%02x: %s", code & 0x7f, e);	
	return(error_message);
}

/* EOF */
