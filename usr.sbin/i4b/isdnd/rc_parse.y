/*
 *   Copyright (c) 1997 Joerg Wunsch. All rights reserved.
 *
 *   Copyright (c) 1997, 2001 Hellmuth Michaelis. All rights reserved.
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
 *	i4b daemon - runtime configuration parser
 *	-----------------------------------------
 *
 * $FreeBSD$
 *
 *      last edit-date: [Mon May 21 11:22:21 2001]
 *
 *---------------------------------------------------------------------------*/

%{

/* #define YYDEBUG 1 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include "monitor.h"	/* monitor access rights bit definitions */
#include "isdnd.h"

#ifndef FALSE
# define FALSE 0
#endif

#ifndef TRUE
# define TRUE 1
#endif

extern void 	cfg_setval(int keyword);
extern void	cfg_set_controller_default();
extern void	reset_scanner(FILE *infile);
extern void 	yyerror(const char *msg);
extern int	yylex();

extern int	lineno;
extern char	*yytext;
extern int	nentries;

int		saw_system = 0;
int		entrycount = -1;
int		controllercount = -1;

%}

%token		ACCTALL
%token		ACCTFILE
%token		ALERT
%token		ALIASFNAME
%token		ALIASING
%token		ANSWERPROG
%token		B1PROTOCOL
%token		BEEPCONNECT
%token		BUDGETCALLOUTPERIOD
%token		BUDGETCALLOUTNCALLS
%token		BUDGETCALLOUTSFILE
%token		BUDGETCALLOUTSFILEROTATE
%token		BUDGETCALLBACKPERIOD
%token		BUDGETCALLBACKNCALLS
%token		BUDGETCALLBACKSFILE
%token		BUDGETCALLBACKSFILEROTATE
%token		CALLBACKWAIT
%token		CALLEDBACKWAIT
%token		CALLIN
%token		CALLOUT
%token		CHANNELSTATE
%token		CLONE
%token		CONNECTPROG
%token		CONTROLLER
%token		DIALOUTTYPE
%token		DIALRANDINCR
%token		DIALRETRIES
%token		DIRECTION
%token		DISCONNECTPROG
%token		DOWNTIME
%token		DOWNTRIES
%token		EARLYHANGUP
%token		ENTRY
%token		EXTCALLATTR
%token		FIRMWARE
%token		FULLCMD
%token		HOLIDAYFILE
%token		IDLETIME_IN
%token		IDLETIME_OUT
%token		IDLE_ALG_OUT
%token		ISDNCHANNEL
%token		ISDNCONTROLLER
%token		ISDNTIME
%token		ISDNTXDELIN
%token		ISDNTXDELOUT
%token		LOCAL_PHONE_DIALOUT
%token		LOCAL_PHONE_INCOMING
%token		LOGEVENTS
%token		MAILER
%token		MAILTO
%token		MONITOR
%token		MONITORACCESS
%token		MONITORPORT
%token		MONITORSW
%token		NAME
%token		NO
%token		OFF
%token		ON
%token		PPP_AUTH_RECHALLENGE
%token		PPP_AUTH_PARANOID
%token		PPP_EXPECT_AUTH
%token		PPP_EXPECT_NAME
%token		PPP_EXPECT_PASSWORD
%token		PPP_SEND_AUTH
%token		PPP_SEND_NAME
%token		PPP_SEND_PASSWORD
%token		PROTOCOL
%token		RATESFILE
%token		RATETYPE
%token		REACTION
%token		RECOVERYTIME
%token		REGEXPR
%token		REGPROG
%token		REMOTE_NUMBERS_HANDLING
%token		REMOTE_PHONE_DIALOUT
%token		REMOTE_PHONE_INCOMING
%token		RESTRICTEDCMD
%token		ROTATESUFFIX
%token		RTPRIO
%token		SYSTEM
%token		TINAINITPROG
%token		UNITLENGTH
%token		UNITLENGTHSRC
%token		USEACCTFILE
%token		USEDOWN
%token		USRDEVICENAME
%token		USRDEVICEUNIT
%token		VALID
%token		YES


%token	<str>	NUMBERSTR

%token	<str>	STRING

%type	<booln>	boolean 

%type	<num>	sysfilekeyword sysnumkeyword sysstrkeyword sysboolkeyword
%type	<num>	filekeyword numkeyword strkeyword boolkeyword monrights monright
%type	<num>	cstrkeyword cfilekeyword
%type	<str>	filename

%union {
	int 	booln;
	int	num;
	char 	*str;
}

%%

config:		sections
		;

sections:	possible_nullentries
		syssect
		optcontrollersects
		entrysects
		;

possible_nullentries:
		/* lambda */
		| possible_nullentries error '\n'
		| possible_nullentries nullentry
		;

nullentry:	'\n'
		;

entrysects:	entrysect
		| entrysects entrysect
		;

optcontrollersects:
		controllersects
		|
			{
				cfg_set_controller_default();
			}
		;

controllersects:  controllersect
		| controllersects controllersect
		;

/* ============== */
/* system section */
/* ============== */

syssect:	SYSTEM sysentries
		;

sysentries:	sysentry
			{ 
				saw_system = 1; 
				monitor_clear_rights();
			}
		| sysentries sysentry
		;

sysentry:	sysfileentry
		| sysboolentry
		| sysnumentry
		| sysstrentry
		| sysmonitorstart
		| sysmonitorrights
		| nullentry
		| error '\n'
		;

  
sysmonitorstart:
		MONITOR '=' STRING '\n'
			{
			    char *err = NULL;
			    switch (monitor_start_rights($3)) {
			    	case I4BMAR_OK:
			    		break;
			    	case I4BMAR_LENGTH:
			    		err = "local socket name too long: %s";
			    		break;
			    	case I4BMAR_DUP:
			    		err = "duplicate entry: %s";
			    		break;
			    	case I4BMAR_CIDR:
			    		err = "invalid CIDR specification: %s";
			    		break;
			    	case I4BMAR_NOIP:
			    		err = "could not resolve host or net specification: %s";
			    		break;
			    }
			    if (err) {
			    	char msg[1024];
		    		snprintf(msg, sizeof msg, err, $3);
		    		yyerror(msg);
		    	    }
			}
		;

sysmonitorrights:
		MONITORACCESS '=' monrights '\n'
			{ monitor_add_rights($3); }	
		;

monrights:	monrights ',' monright	{ $$ = $1 | $3; }
		| monright		{ $$ = $1; }
		;

monright:	FULLCMD			{ $$ = I4B_CA_COMMAND_FULL; }
		| RESTRICTEDCMD		{ $$ = I4B_CA_COMMAND_RESTRICTED; }
		| CHANNELSTATE		{ $$ = I4B_CA_EVNT_CHANSTATE; }
		| CALLIN		{ $$ = I4B_CA_EVNT_CALLIN; }
		| CALLOUT		{ $$ = I4B_CA_EVNT_CALLOUT; }
		| LOGEVENTS		{ $$ = I4B_CA_EVNT_I4B; }
		;

sysfileentry:	sysfilekeyword '=' filename '\n'
			{
			cfg_setval($1);
			}
		;

sysboolentry:	sysboolkeyword '=' boolean '\n'
			{
			yylval.booln = $3;
			cfg_setval($1);
			}
		;

sysnumentry:	sysnumkeyword '=' NUMBERSTR '\n'
			{ 
			yylval.num = atoi($3);
			cfg_setval($1);
			}
		;

sysstrentry:	  sysstrkeyword '=' STRING '\n'
			{ 
			cfg_setval($1);
			}
		| sysstrkeyword '=' NUMBERSTR '\n'
			{ 
			cfg_setval($1);
			}
		;

filename:	STRING		{
					if ($1[0] != '/') 
					{
						yyerror("filename doesn't start with a slash");
						YYERROR;
					}
					$$ = $1;
				}
		;

boolean:	  NO			{ $$ = FALSE; }
		| OFF			{ $$ = FALSE; }
		| ON			{ $$ = TRUE; }
		| YES			{ $$ = TRUE; }
		;

sysfilekeyword:	  RATESFILE		{ $$ = RATESFILE; }
		| ACCTFILE		{ $$ = ACCTFILE; }
		| ALIASFNAME		{ $$ = ALIASFNAME; }
		| HOLIDAYFILE		{ $$ = HOLIDAYFILE; }
		| TINAINITPROG		{ $$ = TINAINITPROG; }
		;

sysboolkeyword:	  USEACCTFILE		{ $$ = USEACCTFILE; }
		| ALIASING		{ $$ = ALIASING; }
		| ACCTALL		{ $$ = ACCTALL; }
		| BEEPCONNECT		{ $$ = BEEPCONNECT; }
		| EXTCALLATTR		{ $$ = EXTCALLATTR; }
		| ISDNTIME		{ $$ = ISDNTIME; }
		| MONITORSW		{ $$ = MONITORSW; }
		;

sysnumkeyword:	  MONITORPORT		{ $$ = MONITORPORT; }
		| RTPRIO		{ $$ = RTPRIO; }
		;

sysstrkeyword:	  MAILER		{ $$ = MAILER; }
		| MAILTO		{ $$ = MAILTO; }
		| ROTATESUFFIX		{ $$ = ROTATESUFFIX; }
		| REGEXPR		{ $$ = REGEXPR; }
		| REGPROG		{ $$ = REGPROG; }
		;

/* ============= */
/* entry section */
/* ============= */

entrysect:	ENTRY
			{ 
				entrycount++;
				nentries++;
			}
		entries
		;

entries:	entry
		| entries entry
		;

entry:		fileentry
		| strentry
		| numentry
		| boolentry
		| nullentry
		| error '\n'
		;

fileentry:	filekeyword '=' filename '\n'
			{
			cfg_setval($1);
			}
		;


strentry:	strkeyword '=' STRING '\n'
			{ 
			cfg_setval($1);
			}
		| strkeyword '=' NUMBERSTR '\n'
			{ 
			cfg_setval($1);
			}
		;

boolentry:	boolkeyword '=' boolean '\n'
			{
			yylval.booln = $3;
			cfg_setval($1);
			}
		;

numentry:	numkeyword '=' NUMBERSTR '\n'
			{ 
			yylval.num = atoi($3);
			cfg_setval($1);
			}
		;

filekeyword:	  BUDGETCALLBACKSFILE	{ $$ = BUDGETCALLBACKSFILE; }
		| BUDGETCALLOUTSFILE	{ $$ = BUDGETCALLOUTSFILE; }
		;

strkeyword:	  ANSWERPROG		{ $$ = ANSWERPROG; }
		| B1PROTOCOL		{ $$ = B1PROTOCOL; }
		| CONNECTPROG		{ $$ = CONNECTPROG; }
		| DIALOUTTYPE		{ $$ = DIALOUTTYPE; }
		| DIRECTION		{ $$ = DIRECTION; }
		| DISCONNECTPROG	{ $$ = DISCONNECTPROG; }
		| IDLE_ALG_OUT		{ $$ = IDLE_ALG_OUT; }
		| LOCAL_PHONE_INCOMING	{ $$ = LOCAL_PHONE_INCOMING; }
		| LOCAL_PHONE_DIALOUT	{ $$ = LOCAL_PHONE_DIALOUT; }
		| NAME			{ $$ = NAME; }		
		| PPP_EXPECT_AUTH	{ $$ = PPP_EXPECT_AUTH; }
		| PPP_EXPECT_NAME	{ $$ = PPP_EXPECT_NAME; }
		| PPP_EXPECT_PASSWORD	{ $$ = PPP_EXPECT_PASSWORD; }
		| PPP_SEND_AUTH		{ $$ = PPP_SEND_AUTH; }
		| PPP_SEND_NAME		{ $$ = PPP_SEND_NAME; }
		| PPP_SEND_PASSWORD	{ $$ = PPP_SEND_PASSWORD; }
		| REACTION		{ $$ = REACTION; }
		| REMOTE_NUMBERS_HANDLING { $$ = REMOTE_NUMBERS_HANDLING; }
		| REMOTE_PHONE_INCOMING	{ $$ = REMOTE_PHONE_INCOMING; }
		| REMOTE_PHONE_DIALOUT	{ $$ = REMOTE_PHONE_DIALOUT; }
		| UNITLENGTHSRC		{ $$ = UNITLENGTHSRC; }		
		| USRDEVICENAME		{ $$ = USRDEVICENAME; }
		| VALID			{ $$ = VALID; }
		| CLONE			{ $$ = CLONE; }
		;

numkeyword:	  ALERT			{ $$ = ALERT; }
		| BUDGETCALLBACKPERIOD	{ $$ = BUDGETCALLBACKPERIOD; }
		| BUDGETCALLBACKNCALLS	{ $$ = BUDGETCALLBACKNCALLS; }
		| BUDGETCALLOUTPERIOD	{ $$ = BUDGETCALLOUTPERIOD; }
		| BUDGETCALLOUTNCALLS	{ $$ = BUDGETCALLOUTNCALLS; }
		| CALLBACKWAIT		{ $$ = CALLBACKWAIT; }
		| CALLEDBACKWAIT	{ $$ = CALLEDBACKWAIT; }
		| DIALRETRIES		{ $$ = DIALRETRIES; }
		| EARLYHANGUP		{ $$ = EARLYHANGUP; }
		| IDLETIME_IN		{ $$ = IDLETIME_IN; }
		| IDLETIME_OUT		{ $$ = IDLETIME_OUT; }
		| ISDNCONTROLLER	{ $$ = ISDNCONTROLLER; }
		| ISDNCHANNEL		{ $$ = ISDNCHANNEL; }
		| ISDNTXDELIN		{ $$ = ISDNTXDELIN; }
		| ISDNTXDELOUT		{ $$ = ISDNTXDELOUT; }
		| RATETYPE		{ $$ = RATETYPE; }
		| RECOVERYTIME		{ $$ = RECOVERYTIME; }
		| UNITLENGTH		{ $$ = UNITLENGTH; }		
		| USRDEVICEUNIT		{ $$ = USRDEVICEUNIT; }
		| DOWNTIME		{ $$ = DOWNTIME; }
		| DOWNTRIES		{ $$ = DOWNTRIES; }
		;

boolkeyword:	  BUDGETCALLBACKSFILEROTATE { $$ = BUDGETCALLBACKSFILEROTATE; }
                | BUDGETCALLOUTSFILEROTATE  { $$ = BUDGETCALLOUTSFILEROTATE; }
		| DIALRANDINCR		{ $$ = DIALRANDINCR; }
		| PPP_AUTH_RECHALLENGE	{ $$ = PPP_AUTH_RECHALLENGE; }
		| PPP_AUTH_PARANOID	{ $$ = PPP_AUTH_PARANOID; }
		| USEDOWN		{ $$ = USEDOWN; }
		;

/* ================== */
/* controller section */
/* ================== */

controllersect:	CONTROLLER
		{ 
			controllercount++;
		}
		controllers
		;

controllers:	controller
		| controllers controller
		;

controller:	strcontroller
		| nullentry
		| error '\n'
		;

strcontroller:	cstrkeyword '=' STRING '\n'
			{ 
			cfg_setval($1);
			}
		| cstrkeyword '=' NUMBERSTR '\n'
			{ 
			cfg_setval($1);
			}
		| cfilekeyword '=' filename '\n'
			{ 
			cfg_setval($1);
			}
		;

cstrkeyword:	  PROTOCOL		{ $$ = PROTOCOL; }
		;

cfilekeyword:	  FIRMWARE		{ $$ = FIRMWARE; }
		;


%%
