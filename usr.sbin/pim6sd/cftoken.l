%{
/*
 * Copyright (C) 1999 WIDE Project.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/usr.sbin/pim6sd/cftoken.l,v 1.1.2.1 2000/07/15 07:36:35 kris Exp $
 */

#include <sys/types.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#ifdef HAVE_STDARG_H
#include <stdarg.h>
#else
#include <varargs.h>
#endif

#include "var.h"
#include "vmbuf.h"
#include "debug.h"
#include "cfparse.h"
#include "y.tab.h"

static int yyerrorcount = 0;
int lineno = 1;
int yy_first_time = 1;

extern char configfilename[];

#if 0
static void cfdebug_print __P((char *, char *, int));
#endif

extern int yylex __P((void));
%}

/* common seciton */
nl		\n
ws		[ \t]+
comment		\#.*
semi		\;
string		[a-zA-Z0-9:\._][a-zA-Z0-9:\._]*
digit		[0-9]
integer 	{digit}+
number		{integer}|({digit}*\.{integer})
hexdigit	[0-9A-Fa-f]
hexpair		{hexdigit}{hexdigit}
hexstring	0[xX]{hexpair}+
ifname		[a-zA-Z]+[0-9]+
slash		\/

%s S_CNF
%s S_LOG
%s S_PHYINT S_IFCONF S_CANDRP S_CANDBSR S_PREFIX
%s S_THRES

%%
%{
	if (yy_first_time) {
		BEGIN S_CNF;
		yy_first_time = 0;
	}
%}

	/* logging */
<S_CNF>log { DP("begin logging"); BEGIN S_LOG; return(LOGGING); }
<S_LOG>(no)?mld_proto {
	YYD_ECHO;
	yylval.num = DEBUG_MLD_PROTO;
	if (strncmp("no", yytext, 2))
		return(LOGLEV);
	else
		return(NOLOGLEV);
}
<S_LOG>(no)?mld_timer {
	YYD_ECHO;
	yylval.num = DEBUG_MLD_TIMER;
	if (strncmp("no", yytext, 2))
		return(LOGLEV);
	else
		return(NOLOGLEV);
}
<S_LOG>(no)?mld_member {
	YYD_ECHO;
	yylval.num = DEBUG_MLD_MEMBER;
	if (strncmp("no", yytext, 2))
		return(LOGLEV);
	else
		return(NOLOGLEV);
}
<S_LOG>(no)?mld {
	YYD_ECHO;
	yylval.num = DEBUG_MLD;
	if (strncmp("no", yytext, 2))
		return(LOGLEV);
	else
		return(NOLOGLEV);
}
<S_LOG>(no)?switch {
	YYD_ECHO;
	yylval.num = DEBUG_SWITCH;
	if (strncmp("no", yytext, 2))
		return(LOGLEV);
	else
		return(NOLOGLEV);
}
<S_LOG>(no)?m?trace {
	YYD_ECHO;
	yylval.num = DEBUG_TRACE;
	if (strncmp("no", yytext, 2))
		return(LOGLEV);
	else
		return(NOLOGLEV);
}
<S_LOG>(no)?traceroute {
	YYD_ECHO;
	yylval.num = DEBUG_TRACE;
	if (strncmp("no", yytext, 2))
		return(LOGLEV);
	else
		return(NOLOGLEV);
}
<S_LOG>(no)?timeout {
	YYD_ECHO;
	yylval.num = DEBUG_TIMEOUT;
	if (strncmp("no", yytext, 2))
		return(LOGLEV);
	else
		return(NOLOGLEV);
}
<S_LOG>(no)?callout {
	YYD_ECHO;
	yylval.num = DEBUG_TIMEOUT;
	if (strncmp("no", yytext, 2))
		return(LOGLEV);
	else
		return(NOLOGLEV);
}
<S_LOG>(no)?pkt {
	YYD_ECHO;
	yylval.num = DEBUG_PKT;
	if (strncmp("no", yytext, 2))
		return(LOGLEV);
	else
		return(NOLOGLEV);
}
<S_LOG>(no)?packets {
	YYD_ECHO;
	yylval.num = DEBUG_PKT;
	if (strncmp("no", yytext, 2))
		return(LOGLEV);
	else
		return(NOLOGLEV);
}
<S_LOG>(no)?interfaces {
	YYD_ECHO;
	yylval.num = DEBUG_IF;
	if (strncmp("no", yytext, 2))
		return(LOGLEV);
	else
		return(NOLOGLEV);
}
<S_LOG>(no)?vif {
	YYD_ECHO;
	yylval.num = DEBUG_IF;
	if (strncmp("no", yytext, 2))
		return(LOGLEV);
	else
		return(NOLOGLEV);
}
<S_LOG>(no)?kernel {
	YYD_ECHO;
	yylval.num = DEBUG_KERN;
	if (strncmp("no", yytext, 2))
		return(LOGLEV);
	else
		return(NOLOGLEV);
}
<S_LOG>(no)?cache {
	YYD_ECHO;
	yylval.num = DEBUG_MFC;
	if (strncmp("no", yytext, 2))
		return(LOGLEV);
	else
		return(NOLOGLEV);
}
<S_LOG>(no)?mfc {
	YYD_ECHO;
	yylval.num = DEBUG_MFC;
	if (strncmp("no", yytext, 2))
		return(LOGLEV);
	else
		return(NOLOGLEV);
}
<S_LOG>(no)?k_cache {
	YYD_ECHO;
	yylval.num = DEBUG_MFC;
	if (strncmp("no", yytext, 2))
		return(LOGLEV);
	else
		return(NOLOGLEV);
}
<S_LOG>(no)?k_mfc {
	YYD_ECHO;
	yylval.num = DEBUG_MFC;
	if (strncmp("no", yytext, 2))
		return(LOGLEV);
	else
		return(NOLOGLEV);
}
<S_LOG>(no)?rsrr {
	YYD_ECHO;
	yylval.num = DEBUG_RSRR;
	if (strncmp("no", yytext, 2))
		return(LOGLEV);
	else
		return(NOLOGLEV);
}
<S_LOG>(no)?pim_detail {
	YYD_ECHO;
	yylval.num = DEBUG_PIM_DETAIL;
	if (strncmp("no", yytext, 2))
		return(LOGLEV);
	else
		return(NOLOGLEV);
}
<S_LOG>(no)?pim_hello {
	YYD_ECHO;
	yylval.num = DEBUG_PIM_HELLO;
	if (strncmp("no", yytext, 2))
		return(LOGLEV);
	else
		return(NOLOGLEV);
}
<S_LOG>(no)?pim_neighbors {
	YYD_ECHO;
	yylval.num = DEBUG_PIM_HELLO;
	if (strncmp("no", yytext, 2))
		return(LOGLEV);
	else
		return(NOLOGLEV);
}
<S_LOG>(no)?pim_register {
	YYD_ECHO;
	yylval.num = DEBUG_PIM_REGISTER;
	if (strncmp("no", yytext, 2))
		return(LOGLEV);
	else
		return(NOLOGLEV);
}
<S_LOG>(no)?registers {
	YYD_ECHO;
	yylval.num = DEBUG_PIM_REGISTER;
	if (strncmp("no", yytext, 2))
		return(LOGLEV);
	else
		return(NOLOGLEV);
}
<S_LOG>(no)?pim_join_prune {
	YYD_ECHO;
	yylval.num = DEBUG_PIM_JOIN_PRUNE;
	if (strncmp("no", yytext, 2))
		return(LOGLEV);
	else
		return(NOLOGLEV);
}
<S_LOG>(no)?pim_j_p {
	YYD_ECHO;
	yylval.num = DEBUG_PIM_JOIN_PRUNE;
	if (strncmp("no", yytext, 2))
		return(LOGLEV);
	else
		return(NOLOGLEV);
}
<S_LOG>(no)?pim_jp {
	YYD_ECHO;
	yylval.num = DEBUG_PIM_JOIN_PRUNE;
	if (strncmp("no", yytext, 2))
		return(LOGLEV);
	else
		return(NOLOGLEV);
}
<S_LOG>(no)?pim_bootstrap {
	YYD_ECHO;
	yylval.num = DEBUG_PIM_BOOTSTRAP;
	if (strncmp("no", yytext, 2))
		return(LOGLEV);
	else
		return(NOLOGLEV);
}
<S_LOG>(no)?pim_bsr {
	YYD_ECHO;
	yylval.num = DEBUG_PIM_BOOTSTRAP;
	if (strncmp("no", yytext, 2))
		return(LOGLEV);
	else
		return(NOLOGLEV);
}
<S_LOG>(no)?bsr {
	YYD_ECHO;
	yylval.num = DEBUG_PIM_BOOTSTRAP;
	if (strncmp("no", yytext, 2))
		return(LOGLEV);
	else
		return(NOLOGLEV);
}
<S_LOG>(no)?bootstrap {
	YYD_ECHO;
	yylval.num = DEBUG_PIM_BOOTSTRAP;
	if (strncmp("no", yytext, 2))
		return(LOGLEV);
	else
		return(NOLOGLEV);
}
<S_LOG>(no)?pim_asserts {
	YYD_ECHO;
	yylval.num = DEBUG_PIM_ASSERT;
	if (strncmp("no", yytext, 2))
		return(LOGLEV);
	else
		return(NOLOGLEV);
}
<S_LOG>(no)?pim_cand_rp {
	YYD_ECHO;
	yylval.num = DEBUG_PIM_CAND_RP;
	if (strncmp("no", yytext, 2))
		return(LOGLEV);
	else
		return(NOLOGLEV);
}
<S_LOG>(no)?pim_c_rp {
	YYD_ECHO;
	yylval.num = DEBUG_PIM_CAND_RP;
	if (strncmp("no", yytext, 2))
		return(LOGLEV);
	else
		return(NOLOGLEV);
}
<S_LOG>(no)?pim_rp {
	YYD_ECHO;
	yylval.num = DEBUG_PIM_CAND_RP;
	if (strncmp("no", yytext, 2))
		return(LOGLEV);
	else
		return(NOLOGLEV);
}
<S_LOG>(no)?rp {
	YYD_ECHO;
	yylval.num = DEBUG_PIM_CAND_RP;
	if (strncmp("no", yytext, 2))
		return(LOGLEV);
	else
		return(NOLOGLEV);
}
<S_LOG>(no)?pim_routes {
	YYD_ECHO;
	yylval.num = DEBUG_PIM_MRT;
	if (strncmp("no", yytext, 2))
		return(LOGLEV);
	else
		return(NOLOGLEV);
}
<S_LOG>(no)?pim_routing {
	YYD_ECHO;
	yylval.num = DEBUG_PIM_MRT;
	if (strncmp("no", yytext, 2))
		return(LOGLEV);
	else
		return(NOLOGLEV);
}
<S_LOG>(no)?pim_mrt {
	YYD_ECHO;
	yylval.num = DEBUG_PIM_MRT;
	if (strncmp("no", yytext, 2))
		return(LOGLEV);
	else
		return(NOLOGLEV);
}
<S_LOG>(no)?pim_timers {
	YYD_ECHO;
	yylval.num = DEBUG_PIM_TIMER;
	if (strncmp("no", yytext, 2))
		return(LOGLEV);
	else
		return(NOLOGLEV);
}
<S_LOG>(no)?pim_rpf {
	YYD_ECHO;
	yylval.num = DEBUG_PIM_RPF;
	if (strncmp("no", yytext, 2))
		return(LOGLEV);
	else
		return(NOLOGLEV);
}
<S_LOG>(no)?rpf {
	YYD_ECHO;
	yylval.num = DEBUG_RPF;
	if (strncmp("no", yytext, 2))
		return(LOGLEV);
	else
		return(NOLOGLEV);
}
<S_LOG>(no)?pim {
	YYD_ECHO;
	yylval.num = DEBUG_PIM;
	if (strncmp("no", yytext, 2))
		return(LOGLEV);
	else
		return(NOLOGLEV);
}
<S_LOG>(no)?routes {
	YYD_ECHO;
	yylval.num = DEBUG_MRT;
	if (strncmp("no", yytext, 2))
		return(LOGLEV);
	else
		return(NOLOGLEV);
}
<S_LOG>(no)?routing {
	YYD_ECHO;
	yylval.num = DEBUG_MRT;
	if (strncmp("no", yytext, 2))
		return(LOGLEV);
	else
		return(NOLOGLEV);
}
<S_LOG>(no)?mrt {
	YYD_ECHO;
	yylval.num = DEBUG_MRT;
	if (strncmp("no", yytext, 2))
		return(LOGLEV);
	else
		return(NOLOGLEV);
}
<S_LOG>(no)?routers {
	YYD_ECHO;
	yylval.num = DEBUG_NEIGHBORS;
	if (strncmp("no", yytext, 2))
		return(LOGLEV);
	else
		return(NOLOGLEV);
}
<S_LOG>(no)?mrouters {
	YYD_ECHO;
	yylval.num = DEBUG_NEIGHBORS;
	if (strncmp("no", yytext, 2))
		return(LOGLEV);
	else
		return(NOLOGLEV);
}
<S_LOG>(no)?neighbors {
	YYD_ECHO;
	yylval.num = DEBUG_NEIGHBORS;
	if (strncmp("no", yytext, 2))
		return(LOGLEV);
	else
		return(NOLOGLEV);
}
<S_LOG>(no)?timers {
	YYD_ECHO;
	yylval.num = DEBUG_TIMER;
	if (strncmp("no", yytext, 2))
		return(LOGLEV);
	else
		return(NOLOGLEV);
}
<S_LOG>(no)?asserts {
	YYD_ECHO;
	yylval.num = DEBUG_ASSERT;
	if (strncmp("no", yytext, 2))
		return(LOGLEV);
	else
		return(NOLOGLEV);
}
<S_LOG>all { YYD_ECHO; yylval.num = DEBUG_ALL; return(LOGLEV); }
<S_LOG>3 { YYD_ECHO; yylval.num = DEBUG_ALL; return(LOGLEV); }
<S_LOG>{semi} { DP("end logging"); BEGIN S_CNF; return(EOS); }
<S_LOG>{string} { yywarn("unknown log type: %s (ignored)", yytext); }

	/* yes-or-no */
[yY][eE][sS] { YYD_ECHO; return(YES); }
[nN][oO] { YYD_ECHO; return(NO); }

	/* reverselookup */
<S_CNF>reverselookup { YYD_ECHO; return(REVERSELOOKUP); }

	/* phyint */
<S_CNF>phyint { DP("begin phyint"); BEGIN S_PHYINT; return(PHYINT); }
<S_PHYINT>{string} {
	YYD_ECHO;
	BEGIN S_IFCONF;
	yylval.val.l = strlen(yytext);
	yylval.val.v = strdup(yytext);
	return(IFNAME);
}
<S_PHYINT>{semi} { yyerror("phyint was specified without interface."); return(EOS); }
<S_IFCONF>disable { YYD_ECHO; return(DISABLE); }
<S_IFCONF>preference { YYD_ECHO; return(PREFERENCE); }
<S_IFCONF>metric { YYD_ECHO; return(METRIC); }
<S_IFCONF>nolistener { YYD_ECHO; return(NOLISTENER); }
<S_IFCONF>{semi} { DP("end phyint"); BEGIN S_CNF; return(EOS); }

	/* cand_rp */
<S_CNF>cand_rp { DP("begin cand_rp"); BEGIN S_CANDRP; return(CANDRP); }
<S_CANDRP>priority { YYD_ECHO; return(PRIORITY); }
<S_CANDRP>time { YYD_ECHO; return(TIME); }
<S_CANDRP>{ifname} {
	YYD_ECHO;
	yylval.val.l = strlen(yytext);
	yylval.val.v = strdup(yytext);
	return(IFNAME);
}
<S_CANDRP>{semi} { DP("end cand_rp"); BEGIN S_CNF; return(EOS); }

	/* cand_bootstrap_router */
<S_CNF>cand_bootstrap_router { DP("begin cand_bsr"); BEGIN S_CANDBSR; return(CANDBSR); }
<S_CANDBSR>priority { YYD_ECHO; return(PRIORITY); }
<S_CANDBSR>masklen { YYD_ECHO; return(MASKLEN); }
<S_CANDBSR>time { YYD_ECHO; return(TIME); }
<S_CANDBSR>{ifname} {
	YYD_ECHO;
	yylval.val.l = strlen(yytext);
	yylval.val.v = strdup(yytext);
	return(IFNAME);
}
<S_CANDBSR>{semi} { DP("end cand_bsr"); BEGIN S_CNF; return(EOS); }

	/* group_prefix */
<S_CNF>group_prefix { YYD_ECHO; return(GRPPFX); }

	/* switch_register_threshold */
<S_CNF>switch_register_threshold {
	YYD_ECHO; BEGIN S_THRES; return(REGTHRES);
}
	/* switch_data_threshold */
<S_CNF>switch_data_threshold {
	YYD_ECHO; BEGIN S_THRES; return(DATATHRES);
}
<S_THRES>rate { YYD_ECHO; return(RATE); }
<S_THRES>interval { YYD_ECHO; return(INTERVAL); }
<S_THRES>{semi} { DP("end thres"); BEGIN S_CNF; return(EOS); }

	/* various parameters */
<S_CNF>default_source_metric { YYD_ECHO; return(SRCMETRIC); }
<S_CNF>default_source_preference { YYD_ECHO; return(SRCPREF); }
<S_CNF>hello_period { YYD_ECHO; return(HELLOPERIOD); }
<S_CNF>granularity { YYD_ECHO; return(GRANULARITY); }
<S_CNF>join_prune_period { YYD_ECHO; return(JPPERIOD); }
<S_CNF>data_timeout { YYD_ECHO; return(DATATIME); }
<S_CNF>register_suppression_timeout { YYD_ECHO; return(REGSUPTIME); }
<S_CNF>probe_time { YYD_ECHO; return(PROBETIME); }
<S_CNF>assert_timeout { YYD_ECHO; return(ASSERTTIME); }

	/* misc */
{ws}		{ ; }
{nl}		{ lineno++; }
{comment}	{ DP("comment"); }
{number} 	{ YYD_ECHO; yylval.fl = atof(yytext); return(NUMBER); }
{slash} { YYD_ECHO; return(SLASH); }
{semi} { DP("end cnf"); return(EOS); }

	/* last resort */
{string} {
			YYD_ECHO;
			yylval.val.l = strlen(yytext);
			yylval.val.v = strdup(yytext);
			return(STRING);
		}
%%

#if 0
static void
cfdebug_print(w, t, l)
	char *w, *t;
	int l;
{
	printf("<%d>%s [%s] (%d)\n", yy_start, w, t, l);
}
#endif

static void
yyerror0(char *s, va_list ap)
{
	fprintf(stderr, "%s %d: ", configfilename, lineno);
	vfprintf(stderr, s, ap);
	fprintf(stderr, "\n");
}

void
yyerror(char *s, ...)
{
	va_list ap;
#ifdef HAVE_STDARG_H
	va_start(ap, s);
#else
	va_start(ap);
#endif
	yyerror0(s, ap);
	va_end(ap);
	yyerrorcount++;
}

void
yywarn(char *s, ...)
{
	va_list ap;
#ifdef HAVE_STDARG_H
	va_start(ap, s);
#else
	va_start(ap);
#endif
	yyerror0(s, ap);
	va_end(ap);
}

int
cfparse(strict, debugonly)
	int strict, debugonly;
{
	if ((yyin = fopen(configfilename, "r")) == NULL) {
		fprintf(stderr, "cfparse: fopen(%s)\n", configfilename);
		return(-1);
	}

	cf_init(strict, debugonly);

	if ((yyparse() || yyerrorcount) && strict) {
		if (yyerrorcount) {
			yyerror("fatal parse failure: exiting (%d errors)",
				yyerrorcount);
		} else
			yyerror("fatal parse failure: exiting");
		return(-1);
	}

	YIPSDP(PLOG("parse successed.\n"));

	return cf_post_config();
}
