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

#include <sys/queue.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <libxo/xo.h>

#include "iscsictl.h"
#include <netinet/in.h>
#include <netinet/ip.h>

extern FILE *yyin;
extern char *yytext;
extern int lineno;

static struct conf *conf;
static struct target *target;

extern void	yyerror(const char *);
extern void	yyrestart(FILE *);

%}

%token AUTH_METHOD ENABLE HEADER_DIGEST DATA_DIGEST TARGET_NAME TARGET_ADDRESS
%token INITIATOR_NAME INITIATOR_ADDRESS INITIATOR_ALIAS USER SECRET
%token MUTUAL_USER MUTUAL_SECRET SEMICOLON SESSION_TYPE PROTOCOL OFFLOAD
%token IGNORED EQUALS OPENING_BRACKET CLOSING_BRACKET DSCP PINGTIMEOUT LOGINTIMEOUT
%token AF11 AF12 AF13 AF21 AF22 AF23 AF31 AF32 AF33 AF41 AF42 AF43
%token BE EF CS0 CS1 CS2 CS3 CS4 CS5 CS6 CS7

%union
{
	char *str;
}

%token <str> STR

%%

targets:
	|
	targets target
	;

target:		STR OPENING_BRACKET target_entries CLOSING_BRACKET
	{
		if (target_find(conf, $1) != NULL)
			xo_errx(1, "duplicated target %s", $1);
		target->t_nickname = $1;
		target = target_new(conf);
	}
	;

target_entries:
	|
	target_entries target_entry
	|
	target_entries target_entry SEMICOLON
	;

target_entry:
	target_name
	|
	target_address
	|
	initiator_name
	|
	initiator_address
	|
	initiator_alias
	|
	user
	|
	secret
	|
	mutual_user
	|
	mutual_secret
	|
	auth_method
	|
	header_digest
	|
	data_digest
	|
	session_type
	|
	enable
	|
	offload
	|
	protocol
	|
	ignored
	|
	dscp
	|
	pcp
	|
	ping_timeout
	|
	login_timeout
	;

target_name:	TARGET_NAME EQUALS STR
	{
		if (target->t_name != NULL)
			xo_errx(1, "duplicated TargetName at line %d", lineno);
		target->t_name = $3;
	}
	;

target_address:	TARGET_ADDRESS EQUALS STR
	{
		if (target->t_address != NULL)
			xo_errx(1, "duplicated TargetAddress at line %d", lineno);
		target->t_address = $3;
	}
	;

initiator_name:	INITIATOR_NAME EQUALS STR
	{
		if (target->t_initiator_name != NULL)
			xo_errx(1, "duplicated InitiatorName at line %d", lineno);
		target->t_initiator_name = $3;
	}
	;

initiator_address:	INITIATOR_ADDRESS EQUALS STR
	{
		if (target->t_initiator_address != NULL)
			xo_errx(1, "duplicated InitiatorAddress at line %d", lineno);
		target->t_initiator_address = $3;
	}
	;

initiator_alias:	INITIATOR_ALIAS EQUALS STR
	{
		if (target->t_initiator_alias != NULL)
			xo_errx(1, "duplicated InitiatorAlias at line %d", lineno);
		target->t_initiator_alias = $3;
	}
	;

user:		USER EQUALS STR
	{
		if (target->t_user != NULL)
			xo_errx(1, "duplicated chapIName at line %d", lineno);
		target->t_user = $3;
	}
	;

secret:		SECRET EQUALS STR
	{
		if (target->t_secret != NULL)
			xo_errx(1, "duplicated chapSecret at line %d", lineno);
		target->t_secret = $3;
	}
	;

mutual_user:	MUTUAL_USER EQUALS STR
	{
		if (target->t_mutual_user != NULL)
			xo_errx(1, "duplicated tgtChapName at line %d", lineno);
		target->t_mutual_user = $3;
	}
	;

mutual_secret:	MUTUAL_SECRET EQUALS STR
	{
		if (target->t_mutual_secret != NULL)
			xo_errx(1, "duplicated tgtChapSecret at line %d", lineno);
		target->t_mutual_secret = $3;
	}
	;

auth_method:	AUTH_METHOD EQUALS STR
	{
		if (target->t_auth_method != AUTH_METHOD_UNSPECIFIED)
			xo_errx(1, "duplicated AuthMethod at line %d", lineno);
		if (strcasecmp($3, "none") == 0)
			target->t_auth_method = AUTH_METHOD_NONE;
		else if (strcasecmp($3, "chap") == 0)
			target->t_auth_method = AUTH_METHOD_CHAP;
		else
			xo_errx(1, "invalid AuthMethod at line %d; "
			    "must be either \"none\" or \"CHAP\"", lineno);
	}
	;

header_digest:	HEADER_DIGEST EQUALS STR
	{
		if (target->t_header_digest != DIGEST_UNSPECIFIED)
			xo_errx(1, "duplicated HeaderDigest at line %d", lineno);
		if (strcasecmp($3, "none") == 0)
			target->t_header_digest = DIGEST_NONE;
		else if (strcasecmp($3, "CRC32C") == 0)
			target->t_header_digest = DIGEST_CRC32C;
		else
			xo_errx(1, "invalid HeaderDigest at line %d; "
			    "must be either \"none\" or \"CRC32C\"", lineno);
	}
	;

data_digest:	DATA_DIGEST EQUALS STR
	{
		if (target->t_data_digest != DIGEST_UNSPECIFIED)
			xo_errx(1, "duplicated DataDigest at line %d", lineno);
		if (strcasecmp($3, "none") == 0)
			target->t_data_digest = DIGEST_NONE;
		else if (strcasecmp($3, "CRC32C") == 0)
			target->t_data_digest = DIGEST_CRC32C;
		else
			xo_errx(1, "invalid DataDigest at line %d; "
			    "must be either \"none\" or \"CRC32C\"", lineno);
	}
	;

session_type:	SESSION_TYPE EQUALS STR
	{
		if (target->t_session_type != SESSION_TYPE_UNSPECIFIED)
			xo_errx(1, "duplicated SessionType at line %d", lineno);
		if (strcasecmp($3, "normal") == 0)
			target->t_session_type = SESSION_TYPE_NORMAL;
		else if (strcasecmp($3, "discovery") == 0)
			target->t_session_type = SESSION_TYPE_DISCOVERY;
		else
			xo_errx(1, "invalid SessionType at line %d; "
			    "must be either \"normal\" or \"discovery\"", lineno);
	}
	;

enable:		ENABLE EQUALS STR
	{
		if (target->t_enable != ENABLE_UNSPECIFIED)
			xo_errx(1, "duplicated enable at line %d", lineno);
		target->t_enable = parse_enable($3);
		if (target->t_enable == ENABLE_UNSPECIFIED)
			xo_errx(1, "invalid enable at line %d; "
			    "must be either \"on\" or \"off\"", lineno);
	}
	;

offload:	OFFLOAD EQUALS STR
	{
		if (target->t_offload != NULL)
			xo_errx(1, "duplicated offload at line %d", lineno);
		target->t_offload = $3;
	}
	;

protocol:	PROTOCOL EQUALS STR
	{
		if (target->t_protocol != PROTOCOL_UNSPECIFIED)
			xo_errx(1, "duplicated protocol at line %d", lineno);
		if (strcasecmp($3, "iscsi") == 0)
			target->t_protocol = PROTOCOL_ISCSI;
		else if (strcasecmp($3, "iser") == 0)
			target->t_protocol = PROTOCOL_ISER;
		else
			xo_errx(1, "invalid protocol at line %d; "
			    "must be either \"iscsi\" or \"iser\"", lineno);
	}
	;

ignored:	IGNORED EQUALS STR
	{
		xo_warnx("obsolete statement ignored at line %d", lineno);
	}
	;

dscp:		DSCP EQUALS STR
	{
		uint64_t tmp;

		if (target->t_dscp != -1)
			xo_errx(1, "duplicated dscp at line %d", lineno);
		if (strcmp($3, "0x") == 0) {
			tmp = strtol($3 + 2, NULL, 16);
		} else if (expand_number($3, &tmp) != 0) {
			yyerror("invalid numeric value");
			free($3);
			return(1);
		}
		if (tmp >= 0x40) {
			yyerror("invalid dscp value");
			return(1);
		}

		target->t_dscp = tmp;
	}
	| DSCP EQUALS BE	{ target->t_dscp = IPTOS_DSCP_CS0  >> 2 ; }
	| DSCP EQUALS EF	{ target->t_dscp = IPTOS_DSCP_EF   >> 2 ; }
	| DSCP EQUALS CS0	{ target->t_dscp = IPTOS_DSCP_CS0  >> 2 ; }
	| DSCP EQUALS CS1	{ target->t_dscp = IPTOS_DSCP_CS1  >> 2 ; }
	| DSCP EQUALS CS2	{ target->t_dscp = IPTOS_DSCP_CS2  >> 2 ; }
	| DSCP EQUALS CS3	{ target->t_dscp = IPTOS_DSCP_CS3  >> 2 ; }
	| DSCP EQUALS CS4	{ target->t_dscp = IPTOS_DSCP_CS4  >> 2 ; }
	| DSCP EQUALS CS5	{ target->t_dscp = IPTOS_DSCP_CS5  >> 2 ; }
	| DSCP EQUALS CS6	{ target->t_dscp = IPTOS_DSCP_CS6  >> 2 ; }
	| DSCP EQUALS CS7	{ target->t_dscp = IPTOS_DSCP_CS7  >> 2 ; }
	| DSCP EQUALS AF11	{ target->t_dscp = IPTOS_DSCP_AF11 >> 2 ; }
	| DSCP EQUALS AF12	{ target->t_dscp = IPTOS_DSCP_AF12 >> 2 ; }
	| DSCP EQUALS AF13	{ target->t_dscp = IPTOS_DSCP_AF13 >> 2 ; }
	| DSCP EQUALS AF21	{ target->t_dscp = IPTOS_DSCP_AF21 >> 2 ; }
	| DSCP EQUALS AF22	{ target->t_dscp = IPTOS_DSCP_AF22 >> 2 ; }
	| DSCP EQUALS AF23	{ target->t_dscp = IPTOS_DSCP_AF23 >> 2 ; }
	| DSCP EQUALS AF31	{ target->t_dscp = IPTOS_DSCP_AF31 >> 2 ; }
	| DSCP EQUALS AF32	{ target->t_dscp = IPTOS_DSCP_AF32 >> 2 ; }
	| DSCP EQUALS AF33	{ target->t_dscp = IPTOS_DSCP_AF33 >> 2 ; }
	| DSCP EQUALS AF41	{ target->t_dscp = IPTOS_DSCP_AF41 >> 2 ; }
	| DSCP EQUALS AF42	{ target->t_dscp = IPTOS_DSCP_AF42 >> 2 ; }
	| DSCP EQUALS AF43	{ target->t_dscp = IPTOS_DSCP_AF43 >> 2 ; }
	;

pcp:	PCP EQUALS STR
	{
		uint64_t tmp;

		if (target->t_pcp != -1)
			xo_errx(1, "duplicated pcp at line %d", lineno);

		if (expand_number($3, &tmp) != 0) {
			yyerror("invalid numeric value");
			free($3);
			return(1);
		}
		if (tmp > 7) {
			yyerror("invalid pcp value");
			return(1);
		}

		target->t_pcp = tmp;
	}
	;

ping_timeout:	PINGTIMEOUT EQUALS STR
	{
		uint64_t tmp;

		if (target->t_pingtimeout != -1)
			xo_errx(1, "duplicated PingTimeout at line %d", lineno);

		if (expand_number($3, &tmp) != 0) {
			yyerror("invalid numeric value");
			free($3);
			return(1);
		}
		target->t_pingtimeout = tmp;
	}
	;

login_timeout:	LOGINTIMEOUT EQUALS STR
	{
		uint64_t tmp;

		if (target->t_logintimeout != -1)
			xo_errx(1, "duplicated LoginTimeout at line %d", lineno);

		if (expand_number($3, &tmp) != 0) {
			yyerror("invalid numeric value");
			free($3);
			return(1);
		}
		target->t_logintimeout = tmp;
	}
	;

%%

void
yyerror(const char *str)
{

	xo_errx(1, "error in configuration file at line %d near '%s': %s",
	    lineno, yytext, str);
}

static void
check_perms(const char *path)
{
	struct stat sb;
	int error;

	error = stat(path, &sb);
	if (error != 0) {
		xo_warn("stat");
		return;
	}
	if (sb.st_mode & S_IWOTH) {
		xo_warnx("%s is world-writable", path);
	} else if (sb.st_mode & S_IROTH) {
		xo_warnx("%s is world-readable", path);
	} else if (sb.st_mode & S_IXOTH) {
		/*
		 * Ok, this one doesn't matter, but still do it,
		 * just for consistency.
		 */
		xo_warnx("%s is world-executable", path);
	}

	/*
	 * XXX: Should we also check for owner != 0?
	 */
}

struct conf *
conf_new_from_file(const char *path)
{
	int error;

	conf = conf_new();
	target = target_new(conf);

	yyin = fopen(path, "r");
	if (yyin == NULL)
		xo_err(1, "unable to open configuration file %s", path);
	check_perms(path);
	lineno = 1;
	yyrestart(yyin);
	error = yyparse();
	assert(error == 0);
	fclose(yyin);

	assert(target->t_nickname == NULL);
	target_delete(target);

	conf_verify(conf);

	return (conf);
}
