%{
/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2012 The FreeBSD Foundation
 *
 * This software was developed by Edward Tomasz Napierala under sponsorship
 * from the FreeBSD Foundation.
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
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "y.tab.h"

int lineno;

#define	YY_DECL int yylex(void)
extern int	yylex(void);

%}

%option noyywrap
%option noinput
%option nounput

%%
alias			{ return ALIAS; }
auth-group		{ return AUTH_GROUP; }
auth-type		{ return AUTH_TYPE; }
backend			{ return BACKEND; }
blocksize		{ return BLOCKSIZE; }
chap			{ return CHAP; }
chap-mutual		{ return CHAP_MUTUAL; }
ctl-lun			{ return CTL_LUN; }
debug			{ return DEBUG; }
device-id		{ return DEVICE_ID; }
device-type		{ return DEVICE_TYPE; }
discovery-auth-group	{ return DISCOVERY_AUTH_GROUP; }
discovery-filter	{ return DISCOVERY_FILTER; }
dscp			{ return DSCP; }
pcp			{ return PCP; }
foreign			{ return FOREIGN; }
initiator-name		{ return INITIATOR_NAME; }
initiator-portal	{ return INITIATOR_PORTAL; }
listen			{ return LISTEN; }
listen-iser		{ return LISTEN_ISER; }
lun			{ return LUN; }
maxproc			{ return MAXPROC; }
offload			{ return OFFLOAD; }
option			{ return OPTION; }
path			{ return PATH; }
pidfile			{ return PIDFILE; }
isns-server		{ return ISNS_SERVER; }
isns-period		{ return ISNS_PERIOD; }
isns-timeout		{ return ISNS_TIMEOUT; }
port			{ return PORT; }
portal-group		{ return PORTAL_GROUP; }
redirect		{ return REDIRECT; }
serial			{ return SERIAL; }
size			{ return SIZE; }
tag			{ return TAG; }
target			{ return TARGET; }
timeout			{ return TIMEOUT; }
af11			{ return AF11; }
af12			{ return AF12; }
af13			{ return AF13; }
af21			{ return AF21; }
af22			{ return AF22; }
af23			{ return AF23; }
af31			{ return AF31; }
af32			{ return AF32; }
af33			{ return AF33; }
af41			{ return AF41; }
af42			{ return AF42; }
af43			{ return AF43; }
be			{ return CS0;  }
ef			{ return EF;   }
cs0			{ return CS0;  }
cs1			{ return CS1;  }
cs2			{ return CS2;  }
cs3			{ return CS3;  }
cs4			{ return CS4;  }
cs5			{ return CS5;  }
cs6			{ return CS6;  }
cs7			{ return CS7;  }
\"[^"]+\"		{ yylval.str = strndup(yytext + 1,
			    strlen(yytext) - 2); return STR; }
[a-zA-Z0-9\.\-@_/\:\[\]]+ { yylval.str = strdup(yytext); return STR; }
\{			{ return OPENING_BRACKET; }
\}			{ return CLOSING_BRACKET; }
#.*$			/* ignore comments */;
\r\n			{ lineno++; }
\n			{ lineno++; }
;			{ return SEMICOLON; }
[ \t]+			/* ignore whitespace */;
.			{ yylval.str = strdup(yytext); return STR; }
%%
