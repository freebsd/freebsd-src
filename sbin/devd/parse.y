%{
/*-
 * DEVD (Device action daemon)
 *
 * Copyright (c) 2002 M. Warner Losh <imp@freebsd.org>.
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
 * $FreeBSD$
 */

#include "devd.h"
#include <stdio.h>

%}

%union {
	char *str;
	int i;
}

%token SEMICOLON BEGINBLOCK ENDBLOCK COMMA
%token <i> NUMBER
%token <str> STRING
%token <str> ID
%token OPTIONS SET DIRECTORY PID_FILE DEVICE_NAME ACTION MATCH
%token ATTACH DETACH NOMATCH

%type <str> id
%type <i> number
%type <str> string

%%

config_file
	: config_list
	;

config_list
	: config
	| config_list config
	;

config
	: option_block
	| attach_block
	| detach_block
	| nomatch_block
	;

option_block
	: OPTIONS BEGINBLOCK options ENDBLOCK SEMICOLON
	;

options
	: option
	| options option

option
	: directory_option
	| pid_file_option
	| set_option
	;

directory_option
	: DIRECTORY string SEMICOLON { add_directory($2); }
	;

pid_file_option
	: PID_FILE string SEMICOLON
	;

set_option
	: SET id string SEMICOLON
	;

attach_block
	: ATTACH number BEGINBLOCK match_or_action_list ENDBLOCK SEMICOLON
	;

detach_block
	: DETACH number BEGINBLOCK match_or_action_list ENDBLOCK SEMICOLON
	;

nomatch_block
	: NOMATCH number BEGINBLOCK match_or_action_list ENDBLOCK SEMICOLON
	;

match_or_action_list
	: match_or_action
	| match_or_action_list match_or_action
	;

match_or_action
	: match
	| action
	;

match
	: MATCH string string SEMICOLON
	| DEVICE_NAME string SEMICOLON
	;

action
	: ACTION string SEMICOLON
	;

number
	: NUMBER	{ $$ = $1; }

string
	: STRING	{ $$ = $1; }

id
	: ID		{ $$ = $1; }

%%
