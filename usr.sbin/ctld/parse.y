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

#include <sys/types.h>
#include <sys/nv.h>
#include <sys/queue.h>
#include <sys/stat.h>
#include <assert.h>
#include <libiscsiutil.h>
#include <libutil.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <netinet/ip.h>

#include "conf.h"

extern FILE *yyin;
extern char *yytext;
extern int lineno;

extern void	yyerror(const char *);
extern void	yyrestart(FILE *);

%}

%token ALIAS AUTH_GROUP AUTH_TYPE BACKEND BLOCKSIZE CHAP CHAP_MUTUAL
%token CLOSING_BRACKET CONTROLLER CTL_LUN DEBUG DEVICE_ID DEVICE_TYPE
%token DISCOVERY_AUTH_GROUP DISCOVERY_FILTER DISCOVERY_TCP DSCP FOREIGN
%token HOST_ADDRESS HOST_NQN
%token INITIATOR_NAME INITIATOR_PORTAL ISNS_SERVER ISNS_PERIOD ISNS_TIMEOUT
%token LISTEN LISTEN_ISER LUN MAXPROC NAMESPACE
%token OFFLOAD OPENING_BRACKET OPTION
%token PATH PCP PIDFILE PORT PORTAL_GROUP REDIRECT SEMICOLON SERIAL
%token SIZE STR TAG TARGET TCP TIMEOUT TRANSPORT_GROUP
%token AF11 AF12 AF13 AF21 AF22 AF23 AF31 AF32 AF33 AF41 AF42 AF43
%token BE EF CS0 CS1 CS2 CS3 CS4 CS5 CS6 CS7

%union
{
	char *str;
}

%token <str> STR

%%

statements:
	|
	statements statement
	|
	statements statement SEMICOLON
	;

statement:
	debug
	|
	timeout
	|
	maxproc
	|
	pidfile
	|
	isns_server
	|
	isns_period
	|
	isns_timeout
	|
	auth_group
	|
	portal_group
	|
	transport_group
	|
	lun
	|
	target
	|
	controller
	;

debug:		DEBUG STR
	{
		int64_t tmp;

		if (expand_number($2, &tmp) != 0) {
			yyerror("invalid numeric value");
			free($2);
			return (1);
		}
		free($2);

		conf_set_debug(tmp);
	}
	;

timeout:	TIMEOUT STR
	{
		int64_t tmp;

		if (expand_number($2, &tmp) != 0) {
			yyerror("invalid numeric value");
			free($2);
			return (1);
		}
		free($2);

		conf_set_timeout(tmp);
	}
	;

maxproc:	MAXPROC STR
	{
		int64_t tmp;

		if (expand_number($2, &tmp) != 0) {
			yyerror("invalid numeric value");
			free($2);
			return (1);
		}
		free($2);

		conf_set_maxproc(tmp);
	}
	;

pidfile:	PIDFILE STR
	{
		bool ok;

		ok = conf_set_pidfile_path($2);
		free($2);
		if (!ok)
			return (1);
	}
	;

isns_server:	ISNS_SERVER STR
	{
		bool ok;

		ok = isns_add_server($2);
		free($2);
		if (!ok)
			return (1);
	}
	;

isns_period:	ISNS_PERIOD STR
	{
		int64_t tmp;

		if (expand_number($2, &tmp) != 0) {
			yyerror("invalid numeric value");
			free($2);
			return (1);
		}
		free($2);

		conf_set_isns_period(tmp);
	}
	;

isns_timeout:	ISNS_TIMEOUT STR
	{
		int64_t tmp;

		if (expand_number($2, &tmp) != 0) {
			yyerror("invalid numeric value");
			free($2);
			return (1);
		}
		free($2);

		conf_set_isns_timeout(tmp);
	}
	;

auth_group:	AUTH_GROUP auth_group_name
    OPENING_BRACKET auth_group_entries CLOSING_BRACKET
	{
		auth_group_finish();
	}
	;

auth_group_name:	STR
	{
		bool ok;

		ok = auth_group_start($1);
		free($1);
		if (!ok)
			return (1);
	}
	;

auth_group_entries:
	|
	auth_group_entries auth_group_entry
	|
	auth_group_entries auth_group_entry SEMICOLON
	;

auth_group_entry:
	auth_group_auth_type
	|
	auth_group_chap
	|
	auth_group_chap_mutual
	|
	auth_group_host_address
	|
	auth_group_host_nqn
	|
	auth_group_initiator_name
	|
	auth_group_initiator_portal
	;

auth_group_auth_type:	AUTH_TYPE STR
	{
		bool ok;

		ok = auth_group_set_type($2);
		free($2);
		if (!ok)
			return (1);
	}
	;

auth_group_chap:	CHAP STR STR
	{
		bool ok;

		ok = auth_group_add_chap($2, $3);
		free($2);
		free($3);
		if (!ok)
			return (1);
	}
	;

auth_group_chap_mutual:	CHAP_MUTUAL STR STR STR STR
	{
		bool ok;

		ok = auth_group_add_chap_mutual($2, $3, $4, $5);
		free($2);
		free($3);
		free($4);
		free($5);
		if (!ok)
			return (1);
	}
	;

auth_group_host_address:	HOST_ADDRESS STR
	{
		bool ok;

		ok = auth_group_add_host_address($2);
		free($2);
		if (!ok)
			return (1);
	}
	;

auth_group_host_nqn:	HOST_NQN STR
	{
		bool ok;

		ok = auth_group_add_host_nqn($2);
		free($2);
		if (!ok)
			return (1);
	}
	;

auth_group_initiator_name:	INITIATOR_NAME STR
	{
		bool ok;

		ok = auth_group_add_initiator_name($2);
		free($2);
		if (!ok)
			return (1);
	}
	;

auth_group_initiator_portal:	INITIATOR_PORTAL STR
	{
		bool ok;

		ok = auth_group_add_initiator_portal($2);
		free($2);
		if (!ok)
			return (1);
	}
	;

portal_group:	PORTAL_GROUP portal_group_name
    OPENING_BRACKET portal_group_entries CLOSING_BRACKET
	{
		portal_group_finish();
	}
	;

portal_group_name:	STR
	{
		bool ok;

		ok = portal_group_start($1);
		free($1);
		if (!ok)
			return (1);
	}
	;

portal_group_entries:
	|
	portal_group_entries portal_group_entry
	|
	portal_group_entries portal_group_entry SEMICOLON
	;

portal_group_entry:
	portal_group_discovery_auth_group
	|
	portal_group_discovery_filter
	|
	portal_group_foreign
	|
	portal_group_listen
	|
	portal_group_listen_iser
	|
	portal_group_offload
	|
	portal_group_option
	|
	portal_group_redirect
	|
	portal_group_tag
	|
	portal_group_dscp
	|
	portal_group_pcp
	;

portal_group_discovery_auth_group:	DISCOVERY_AUTH_GROUP STR
	{
		bool ok;

		ok = portal_group_set_discovery_auth_group($2);
		free($2);
		if (!ok)
			return (1);
	}
	;

portal_group_discovery_filter:	DISCOVERY_FILTER STR
	{
		bool ok;

		ok = portal_group_set_filter($2);
		free($2);
		if (!ok)
			return (1);
	}
	;

portal_group_foreign:	FOREIGN
	{

		portal_group_set_foreign();
	}
	;

portal_group_listen:	LISTEN STR
	{
		bool ok;

		ok = portal_group_add_listen($2, false);
		free($2);
		if (!ok)
			return (1);
	}
	;

portal_group_listen_iser:	LISTEN_ISER STR
	{
		bool ok;

		ok = portal_group_add_listen($2, true);
		free($2);
		if (!ok)
			return (1);
	}
	;

portal_group_offload:	OFFLOAD STR
	{
		bool ok;

		ok = portal_group_set_offload($2);
		free($2);
		if (!ok)
			return (1);
	}
	;

portal_group_option:	OPTION STR STR
	{
		bool ok;

		ok = portal_group_add_option($2, $3);
		free($2);
		free($3);
		if (!ok)
			return (1);
	}
	;

portal_group_redirect:	REDIRECT STR
	{
		bool ok;

		ok = portal_group_set_redirection($2);
		free($2);
		if (!ok)
			return (1);
	}
	;

portal_group_tag:	TAG STR
	{
		int64_t tmp;

		if (expand_number($2, &tmp) != 0) {
			yyerror("invalid numeric value");
			free($2);
			return (1);
		}
		free($2);

		portal_group_set_tag(tmp);
	}
	;

portal_group_dscp
: DSCP STR
	{
		int64_t tmp;

		if (strcmp($2, "0x") == 0) {
			tmp = strtol($2 + 2, NULL, 16);
		} else if (expand_number($2, &tmp) != 0) {
			yyerror("invalid numeric value");
			free($2);
			return (1);
		}
		free($2);

		if (!portal_group_set_dscp(tmp))
			return (1);
	}
| DSCP BE	{ portal_group_set_dscp(IPTOS_DSCP_CS0  >> 2); }
| DSCP EF	{ portal_group_set_dscp(IPTOS_DSCP_EF   >> 2); }
| DSCP CS0	{ portal_group_set_dscp(IPTOS_DSCP_CS0  >> 2); }
| DSCP CS1	{ portal_group_set_dscp(IPTOS_DSCP_CS1  >> 2); }
| DSCP CS2	{ portal_group_set_dscp(IPTOS_DSCP_CS2  >> 2); }
| DSCP CS3	{ portal_group_set_dscp(IPTOS_DSCP_CS3  >> 2); }
| DSCP CS4	{ portal_group_set_dscp(IPTOS_DSCP_CS4  >> 2); }
| DSCP CS5	{ portal_group_set_dscp(IPTOS_DSCP_CS5  >> 2); }
| DSCP CS6	{ portal_group_set_dscp(IPTOS_DSCP_CS6  >> 2); }
| DSCP CS7	{ portal_group_set_dscp(IPTOS_DSCP_CS7  >> 2); }
| DSCP AF11	{ portal_group_set_dscp(IPTOS_DSCP_AF11 >> 2); }
| DSCP AF12	{ portal_group_set_dscp(IPTOS_DSCP_AF12 >> 2); }
| DSCP AF13	{ portal_group_set_dscp(IPTOS_DSCP_AF13 >> 2); }
| DSCP AF21	{ portal_group_set_dscp(IPTOS_DSCP_AF21 >> 2); }
| DSCP AF22	{ portal_group_set_dscp(IPTOS_DSCP_AF22 >> 2); }
| DSCP AF23	{ portal_group_set_dscp(IPTOS_DSCP_AF23 >> 2); }
| DSCP AF31	{ portal_group_set_dscp(IPTOS_DSCP_AF31 >> 2); }
| DSCP AF32	{ portal_group_set_dscp(IPTOS_DSCP_AF32 >> 2); }
| DSCP AF33	{ portal_group_set_dscp(IPTOS_DSCP_AF33 >> 2); }
| DSCP AF41	{ portal_group_set_dscp(IPTOS_DSCP_AF41 >> 2); }
| DSCP AF42	{ portal_group_set_dscp(IPTOS_DSCP_AF42 >> 2); }
| DSCP AF43	{ portal_group_set_dscp(IPTOS_DSCP_AF43 >> 2); }
	;

portal_group_pcp:	PCP STR
	{
		int64_t tmp;

		if (expand_number($2, &tmp) != 0) {
			yyerror("invalid numeric value");
			free($2);
			return (1);
		}
		free($2);

		if (!portal_group_set_pcp(tmp))
			return (1);
	}
	;

transport_group:	TRANSPORT_GROUP transport_group_name
    OPENING_BRACKET transport_group_entries CLOSING_BRACKET
	{
		portal_group_finish();
	}
	;

transport_group_name:	STR
	{
		bool ok;

		ok = transport_group_start($1);
		free($1);
		if (!ok)
			return (1);
	}
	;

transport_group_entries:
	|
	transport_group_entries transport_group_entry
	|
	transport_group_entries transport_group_entry SEMICOLON
	;

transport_group_entry:
	portal_group_discovery_auth_group
	|
	portal_group_discovery_filter
	|
	transport_group_listen_discovery_tcp
	|
	transport_group_listen_tcp
	|
	portal_group_option
	|
	portal_group_tag
	|
	portal_group_dscp
	|
	portal_group_pcp
	;

transport_group_listen_discovery_tcp:	LISTEN DISCOVERY_TCP STR
	{
		bool ok;

		ok = transport_group_add_listen_discovery_tcp($3);
		free($3);
		if (!ok)
			return (1);
	}
	;

transport_group_listen_tcp:	LISTEN TCP STR
	{
		bool ok;

		ok = transport_group_add_listen_tcp($3);
		free($3);
		if (!ok)
			return (1);
	}
	;

lun:	LUN lun_name
    OPENING_BRACKET lun_entries CLOSING_BRACKET
	{
		lun_finish();
	}
	;

lun_name:	STR
	{
		bool ok;

		ok = lun_start($1);
		free($1);
		if (!ok)
			return (1);
	}
	;

target:	TARGET target_name
    OPENING_BRACKET target_entries CLOSING_BRACKET
	{
		target_finish();
	}
	;

target_name:	STR
	{
		bool ok;

		ok = target_start($1);
		free($1);
		if (!ok)
			return (1);
	}
	;

target_entries:
	|
	target_entries target_entry
	|
	target_entries target_entry SEMICOLON
	;

target_entry:
	target_alias
	|
	target_auth_group
	|
	target_auth_type
	|
	target_chap
	|
	target_chap_mutual
	|
	target_initiator_name
	|
	target_initiator_portal
	|
	target_portal_group
	|
	target_port
	|
	target_redirect
	|
	target_lun
	|
	target_lun_ref
	;

target_alias:	ALIAS STR
	{
		bool ok;

		ok = target_set_alias($2);
		free($2);
		if (!ok)
			return (1);
	}
	;

target_auth_group:	AUTH_GROUP STR
	{
		bool ok;

		ok = target_set_auth_group($2);
		free($2);
		if (!ok)
			return (1);
	}
	;

target_auth_type:	AUTH_TYPE STR
	{
		bool ok;

		ok = target_set_auth_type($2);
		free($2);
		if (!ok)
			return (1);
	}
	;

target_chap:	CHAP STR STR
	{
		bool ok;

		ok = target_add_chap($2, $3);
		free($2);
		free($3);
		if (!ok)
			return (1);
	}
	;

target_chap_mutual:	CHAP_MUTUAL STR STR STR STR
	{
		bool ok;

		ok = target_add_chap_mutual($2, $3, $4, $5);
		free($2);
		free($3);
		free($4);
		free($5);
		if (!ok)
			return (1);
	}
	;

target_initiator_name:	INITIATOR_NAME STR
	{
		bool ok;

		ok = target_add_initiator_name($2);
		free($2);
		if (!ok)
			return (1);
	}
	;

target_initiator_portal:	INITIATOR_PORTAL STR
	{
		bool ok;

		ok = target_add_initiator_portal($2);
		free($2);
		if (!ok)
			return (1);
	}
	;

target_portal_group:	PORTAL_GROUP STR STR
	{
		bool ok;

		ok = target_add_portal_group($2, $3);
		free($2);
		free($3);
		if (!ok)
			return (1);
	}
	|		PORTAL_GROUP STR
	{
		bool ok;

		ok = target_add_portal_group($2, NULL);
		free($2);
		if (!ok)
			return (1);
	}
	;

target_port:	PORT STR
	{
		bool ok;

		ok = target_set_physical_port($2);
		free($2);
		if (!ok)
			return (1);
	}
	;

target_redirect:	REDIRECT STR
	{
		bool ok;

		ok = target_set_redirection($2);
		free($2);
		if (!ok)
			return (1);
	}
	;

target_lun:	LUN lun_number
    OPENING_BRACKET lun_entries CLOSING_BRACKET
	{
		lun_finish();
	}
	;

lun_number:	STR
	{
		int64_t tmp;

		if (expand_number($1, &tmp) != 0) {
			yyerror("invalid numeric value");
			free($1);
			return (1);
		}
		free($1);

		if (!target_start_lun(tmp))
			return (1);
	}
	;

target_lun_ref:	LUN STR STR
	{
		int64_t tmp;
		bool ok;

		if (expand_number($2, &tmp) != 0) {
			yyerror("invalid numeric value");
			free($2);
			free($3);
			return (1);
		}
		free($2);

		ok = target_add_lun(tmp, $3);
		free($3);
		if (!ok)
			return (1);
	}
	;

controller:	CONTROLLER controller_name
    OPENING_BRACKET controller_entries CLOSING_BRACKET
	{
		target_finish();
	}
	;

controller_name:	STR
	{
		bool ok;

		ok = controller_start($1);
		free($1);
		if (!ok)
			return (1);
	}
	;

controller_entries:
	|
	controller_entries controller_entry
	|
	controller_entries controller_entry SEMICOLON
	;

controller_entry:
	target_auth_group
	|
	target_auth_type
	|
	controller_host_address
	|
	controller_host_nqn
	|
	controller_transport_group
	|
	controller_namespace
	|
	controller_namespace_ref
	;

controller_host_address:	HOST_ADDRESS STR
	{
		bool ok;

		ok = controller_add_host_address($2);
		free($2);
		if (!ok)
			return (1);
	}
	;

controller_host_nqn:	HOST_NQN STR
	{
		bool ok;

		ok = controller_add_host_nqn($2);
		free($2);
		if (!ok)
			return (1);
	}
	;

controller_transport_group:	TRANSPORT_GROUP STR STR
	{
		bool ok;

		ok = target_add_portal_group($2, $3);
		free($2);
		free($3);
		if (!ok)
			return (1);
	}
	|		TRANSPORT_GROUP STR
	{
		bool ok;

		ok = target_add_portal_group($2, NULL);
		free($2);
		if (!ok)
			return (1);
	}
	;

controller_namespace:	NAMESPACE ns_number
    OPENING_BRACKET lun_entries CLOSING_BRACKET
	{
		lun_finish();
	}
	;

ns_number:	STR
	{
		uint64_t tmp;

		if (expand_number($1, &tmp) != 0) {
			yyerror("invalid numeric value");
			free($1);
			return (1);
		}
		free($1);

		if (!controller_start_namespace(tmp))
			return (1);
	}
	;

controller_namespace_ref:	NAMESPACE STR STR
	{
		uint64_t tmp;
		bool ok;

		if (expand_number($2, &tmp) != 0) {
			yyerror("invalid numeric value");
			free($2);
			free($3);
			return (1);
		}
		free($2);

		ok = controller_add_namespace(tmp, $3);
		free($3);
		if (!ok)
			return (1);
	}
	;

lun_entries:
	|
	lun_entries lun_entry
	|
	lun_entries lun_entry SEMICOLON
	;

lun_entry:
	lun_backend
	|
	lun_blocksize
	|
	lun_device_id
	|
	lun_device_type
	|
	lun_ctl_lun
	|
	lun_option
	|
	lun_path
	|
	lun_serial
	|
	lun_size
	;

lun_backend:	BACKEND STR
	{
		bool ok;

		ok = lun_set_backend($2);
		free($2);
		if (!ok)
			return (1);
	}
	;

lun_blocksize:	BLOCKSIZE STR
	{
		int64_t tmp;

		if (expand_number($2, &tmp) != 0) {
			yyerror("invalid numeric value");
			free($2);
			return (1);
		}
		free($2);

		if (!lun_set_blocksize(tmp))
			return (1);
	}
	;

lun_device_id:	DEVICE_ID STR
	{
		bool ok;

		ok = lun_set_device_id($2);
		free($2);
		if (!ok)
			return (1);
	}
	;

lun_device_type:	DEVICE_TYPE STR
	{
		bool ok;

		ok = lun_set_device_type($2);
		free($2);
		if (!ok)
			return (1);
	}
	;

lun_ctl_lun:	CTL_LUN STR
	{
		int64_t tmp;

		if (expand_number($2, &tmp) != 0) {
			yyerror("invalid numeric value");
			free($2);
			return (1);
		}
		free($2);

		if (!lun_set_ctl_lun(tmp))
			return (1);
	}
	;

lun_option:	OPTION STR STR
	{
		bool ok;

		ok = lun_add_option($2, $3);
		free($2);
		free($3);
		if (!ok)
			return (1);
	}
	;

lun_path:	PATH STR
	{
		bool ok;

		ok = lun_set_path($2);
		free($2);
		if (!ok)
			return (1);
	}
	;

lun_serial:	SERIAL STR
	{
		bool ok;

		ok = lun_set_serial($2);
		free($2);
		if (!ok)
			return (1);
	}
	;

lun_size:	SIZE STR
	{
		int64_t tmp;

		if (expand_number($2, &tmp) != 0) {
			yyerror("invalid numeric value");
			free($2);
			return (1);
		}
		free($2);

		if (!lun_set_size(tmp))
			return (1);
	}
	;
%%

void
yyerror(const char *str)
{

	log_warnx("error in configuration file at line %d near '%s': %s",
	    lineno, yytext, str);
}

bool
yyparse_conf(FILE *fp)
{
	int error;

	yyin = fp;
	lineno = 1;
	yyrestart(yyin);
	error = yyparse();

	return (error == 0);
}
