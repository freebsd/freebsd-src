/*
 *   Copyright (c) 1997 Joerg Wunsch. All rights reserved.
 *
 *   Copyright (c) 1997, 2002 Hellmuth Michaelis. All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *   
 *   THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 *   ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *   ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 *   FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 *   DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 *   OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 *   HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *   LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 *   OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 *   SUCH DAMAGE.
 *
 *---------------------------------------------------------------------------
 *
 *	i4b daemon - runtime configuration lexical analyzer
 *	---------------------------------------------------
 *
 * $FreeBSD$
 *
 *      last edit-date: [Tue Mar 26 14:37:59 2002]
 *
 *---------------------------------------------------------------------------*/

%{

#include <err.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <sysexits.h>

#include "y.tab.h"

int lineno;

%}

%option	noyywrap
%option nounput

%%

#.*$				{ /*
				   * Drop comment.  NB: this prevents a hash
				   * sign from appearing inside a quoted string.
				   */
				}

["][^"]*["]			{
				char *str;
				if ((str = malloc(yyleng - 1)) == 0)
					errx(EX_OSERR, "Out of virtual memory");
				memcpy(str, yytext + 1, yyleng - 2);
				str[yyleng - 2] = 0;
				yylval.str = str;
				return STRING;
				}

(-*[0-9]+)|\*			{
				char *str;
				char *p = yytext;
				int i = 0;
				if ((str = malloc(128)) == 0)
					errx(EX_OSERR, "Out of virtual memory");
				while(*p == '-' || isdigit(*p) || *p == '*')
					str[i++] = *p++;
				str[i] = '\0';
				yylval.str = str;
				return NUMBERSTR;
				}

acctall				{ return ACCTALL; }
acctfile			{ return ACCTFILE; }
alert				{ return ALERT; }
aliasing			{ return ALIASING; }
aliasfile			{ return ALIASFNAME; }
answerprog			{ return ANSWERPROG; }
b1protocol			{ return B1PROTOCOL; }
beepconnect			{ return BEEPCONNECT; }
budget-callbackperiod		{ return BUDGETCALLBACKPERIOD; }
budget-callbackncalls		{ return BUDGETCALLBACKNCALLS; }
budget-callbacksfile		{ return BUDGETCALLBACKSFILE; }
budget-callbacksfile-rotate	{ return BUDGETCALLBACKSFILEROTATE; }
budget-calloutperiod		{ return BUDGETCALLOUTPERIOD; }
budget-calloutncalls		{ return BUDGETCALLOUTNCALLS; }
budget-calloutsfile		{ return BUDGETCALLOUTSFILE; }
budget-calloutsfile-rotate	{ return BUDGETCALLOUTSFILEROTATE; }
callbackwait			{ return CALLBACKWAIT; }
calledbackwait			{ return CALLEDBACKWAIT; }
clone				{ return CLONE; }
connectprog			{ return CONNECTPROG; }
controller			{ return CONTROLLER; }
dialin-reaction			{ return REACTION; }
dialout-type			{ return DIALOUTTYPE; }
dialrandincr			{ return DIALRANDINCR; }
dialretries			{ return DIALRETRIES; }
direction			{ return DIRECTION; }
disconnectprog			{ return DISCONNECTPROG; }
downtries			{ return DOWNTRIES; }
downtime			{ return DOWNTIME; }
earlyhangup			{ return EARLYHANGUP; }
entry				{ return ENTRY; }
extcallattr			{ return EXTCALLATTR; }
firmware			{ return FIRMWARE; }
holidayfile			{ return HOLIDAYFILE; }
idletime-incoming		{ return IDLETIME_IN; }
idletime-outgoing		{ return IDLETIME_OUT; }
idle-algorithm-outgoing		{ return IDLE_ALG_OUT; }
isdncontroller			{ return ISDNCONTROLLER; }
isdnchannel			{ return ISDNCHANNEL; }
isdntime			{ return ISDNTIME; }
isdntxdel-incoming		{ return ISDNTXDELIN; }
isdntxdel-outgoing		{ return ISDNTXDELOUT; }
local-phone-dialout		{ return LOCAL_PHONE_DIALOUT; }
local-subaddr-dialout		{ return LOCAL_SUBADDR_DIALOUT; }
local-phone-incoming		{ return LOCAL_PHONE_INCOMING; }
local-subaddr-incoming		{ return LOCAL_SUBADDR_INCOMING; }
mailer				{ return MAILER; }
mailto				{ return MAILTO; }
maxconnecttime			{ return MAXCONNECTTIME; }
monitor-allowed			{ return MONITORSW; }
monitor-port			{ return MONITORPORT; }
monitor				{ return MONITOR; }
monitor-access			{ return MONITORACCESS; }
fullcmd				{ return FULLCMD; }
restrictedcmd			{ return RESTRICTEDCMD; }
channelstate			{ return CHANNELSTATE; }
callin				{ return CALLIN; }
callout				{ return CALLOUT; }
logevents			{ return LOGEVENTS; }
name				{ return NAME; }
no				{ return NO; }
off				{ return OFF; }
on				{ return ON; }
ppp-auth-rechallenge		{ return PPP_AUTH_RECHALLENGE; }
ppp-auth-paranoid		{ return PPP_AUTH_PARANOID; }
ppp-expect-auth			{ return PPP_EXPECT_AUTH; }
ppp-expect-name			{ return PPP_EXPECT_NAME; }
ppp-expect-password		{ return PPP_EXPECT_PASSWORD; }
ppp-send-auth			{ return PPP_SEND_AUTH; }
ppp-send-name			{ return PPP_SEND_NAME; }
ppp-send-password		{ return PPP_SEND_PASSWORD; }
protocol			{ return PROTOCOL; }
ratesfile			{ return RATESFILE; }
ratetype			{ return RATETYPE; }
recoverytime			{ return RECOVERYTIME; }
regexpr				{ return REGEXPR; }
regprog				{ return REGPROG; }
remdial-handling		{ return REMOTE_NUMBERS_HANDLING; }
remote-phone-dialout		{ return REMOTE_PHONE_DIALOUT; }
remote-subaddr-dialout		{ return REMOTE_SUBADDR_DIALOUT; }
remote-phone-incoming		{ return REMOTE_PHONE_INCOMING; }
remote-subaddr-incoming		{ return REMOTE_SUBADDR_INCOMING; }
rotatesuffix			{ return ROTATESUFFIX; }
rtprio				{ return RTPRIO; }
system				{ return SYSTEM; }
tinainitprog			{ return TINAINITPROG; }
unitlength			{ return UNITLENGTH; }
unitlengthsrc			{ return UNITLENGTHSRC; }
useacctfile			{ return USEACCTFILE; }
usesubaddr			{ return USESUBADDR; }
usrdevicename			{ return USRDEVICENAME; }
usrdeviceunit			{ return USRDEVICEUNIT; }
usedown				{ return USEDOWN; }
valid				{ return VALID; }
yes				{ return YES; }

\n				{ lineno++; return '\n'; }

[A-Za-z/.][-A-Za-z0-9_/.]*	{
				char *str;
				if ((str = strdup(yytext)) == 0)
					err(EX_OSERR, "Out of virtual memory");
				yylval.str = str;
				return STRING;
				}

[ \t]				{ /* drop white space */ }

.				{ return yytext[0]; }

%%

void
reset_scanner(FILE *infile)
{
        yyrestart(infile);
        lineno = 1;
}
