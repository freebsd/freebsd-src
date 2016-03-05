/*
 * Copyright (c) 2006 Mellanox Technologies Ltd. All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * $Id: config_parser.y 1.5 2005/06/29 11:39:27 eitan Exp $
 */


/*

*/
%{

/* header section */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "libsdp.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define YYERROR_VERBOSE 1

extern int yyerror(char *msg);
extern int yylex(void);
static int parse_err = 0;

struct use_family_rule *__sdp_clients_family_rules_head = NULL;
struct use_family_rule *__sdp_clients_family_rules_tail = NULL;
struct use_family_rule *__sdp_servers_family_rules_head = NULL;
struct use_family_rule *__sdp_servers_family_rules_tail = NULL;

/* some globals to store intermidiate parser state */
static struct use_family_rule __sdp_rule;
static int current_role = 0;

int __sdp_config_empty(
                       void
                       )
{
  return ( (__sdp_clients_family_rules_head == NULL) &&
           (__sdp_servers_family_rules_head == NULL) );
}

static void __sdp_set_ip_addr(char *addr)
{
	int rc;
	char *addrlen;
	struct sockaddr_in *addr4 = (struct sockaddr_in *)(&__sdp_rule.ip);
	struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *)(&__sdp_rule.ip);
    	int prefixlen = 0;

	addrlen = strrchr(addr, '/');
	if (addrlen) {
    		prefixlen = atoi(addrlen + 1);
		*addrlen = '\0';
	}

	rc = inet_pton(AF_INET, addr, &addr4->sin_addr);
	if (rc > 0) {
		addr4->sin_family = AF_INET;
		__sdp_rule.prefixlen = prefixlen ?: 32;
		return;
	}

	rc = inet_pton(AF_INET6, addr, &addr6->sin6_addr);
	if (rc > 0) {
		addr6->sin6_family = AF_INET6;
		__sdp_rule.prefixlen = prefixlen ?: 128;
		return;
	}

    	parse_err = 1;
    	yyerror("provided address is not legal");
}

static const char *addr2str(struct sockaddr_storage *src)
{
	static char dst[INET6_ADDRSTRLEN];
	int af = src->ss_family;
	const struct sockaddr_in *addr4 = (const struct sockaddr_in *)src;
	const struct sockaddr_in6 *addr6 = (const struct sockaddr_in6 *)src;

	if (af == AF_INET6)
		return inet_ntop(af, &addr6->sin6_addr, dst, INET6_ADDRSTRLEN);

	return inet_ntop(af, &addr4->sin_addr, dst, INET6_ADDRSTRLEN);
}

static void __sdp_set_prog_name_expr(char *prog_name_expr)
{
  __sdp_rule.prog_name_expr = strdup(prog_name_expr);
  if (!__sdp_rule.prog_name_expr) {
    yyerror("fail to allocate program name expression");    
  }
}

static char *__sdp_get_role_str(int role)
{
  if (role == 1) return("server");
  if (role == 2) return("client");
  return("unknown role");
}

extern int __sdp_min_level;

/* dump the current state in readable format */
static void  __sdp_dump_config_state() {
  char buf[1024];
  sprintf(buf, "CONFIG: use %s %s %s", 
          __sdp_get_family_str(__sdp_rule.target_family), 
          __sdp_get_role_str( current_role ),
          __sdp_rule.prog_name_expr);
  if (__sdp_rule.match_by_addr) {
      sprintf(buf+strlen(buf), " %s/%d", 
              addr2str(&__sdp_rule.ip), __sdp_rule.prefixlen);
  } else {
    sprintf(buf+strlen(buf), " *");
  }
  if (__sdp_rule.match_by_port) {
    sprintf(buf+strlen(buf), ":%d",__sdp_rule.sport);
    if (__sdp_rule.eport > __sdp_rule.sport) 
      sprintf(buf+strlen(buf), "-%d",__sdp_rule.eport);
  }
  else
    sprintf(buf+strlen(buf), ":*");
  sprintf(buf+strlen(buf), "\n");
  __sdp_log(1, buf);
}

/* use the above state for making a new rule */
static void __sdp_add_rule() {
  struct use_family_rule **p_tail, **p_head, *rule;

  if (__sdp_min_level <= 1) __sdp_dump_config_state();
  if ( current_role == 1 ) {
    p_tail = &__sdp_servers_family_rules_tail;
    p_head = &__sdp_servers_family_rules_head;
  } else if ( current_role == 2 ) {
    p_tail = &__sdp_clients_family_rules_tail;
    p_head = &__sdp_clients_family_rules_head;
  } else {
    yyerror("ignoring unknown role");
    parse_err = 1;
    return;
  }

  rule = (struct use_family_rule *)malloc(sizeof(*rule));
  if (!rule) {
    yyerror("fail to allocate new rule");
    parse_err = 1;
    return;
  }

  memset(rule, 0, sizeof(*rule));
  *rule = __sdp_rule;
  rule->prev = *p_tail;
  if (!(*p_head)) {
    *p_head = rule;
  } else {
    (*p_tail)->next = rule;
  } /* if */
  *p_tail = rule;
}

%}


%union {
  int        ival;
  char      *sval;
}             
%token USE "use"
%token CLIENT "client or connect"
%token SERVER "server or listen"
%token TCP "tcp"
%token SDP "sdp"
%token BOTH "both"
%token INT "integer value"
%token LOG "log statement"
%token DEST "destination"
%token STDERR "stderr"
%token SYSLOG "syslog"
%token FILENAME "file"
%token NAME "a name"
%token LEVEL "min-level"
%token LINE "new line"
%token SUBNET "ip address"
%type <sval> NAME SUBNET
%type <ival> INT LOG DEST STDERR SYSLOG FILENAME USE TCP SDP BOTH CLIENT SERVER LEVEL LINE 
%debug
%error-verbose
%start config

%{
  long __sdp_config_line_num;
%}
%%

NL:
    LINE
  | NL LINE;
    
ONL:
  | NL;
    
config: 
  ONL statements
  ;

statements:
  | statements statement
  ;

statement: 
    log_statement
  | socket_statement
  ;

log_statement: 
    LOG log_opts NL
  ;

log_opts:
  | log_opts log_dest
  | log_opts verbosity
  ;

log_dest: 
    DEST STDERR        { __sdp_log_set_log_stderr(); }
  | DEST SYSLOG        { __sdp_log_set_log_syslog(); }
  | DEST FILENAME NAME { __sdp_log_set_log_file($3); }
  ;
    
verbosity: 
    LEVEL INT { __sdp_log_set_min_level($2); }
  ;

socket_statement: 
    USE family role program address ':' ports NL { __sdp_add_rule(); }
  ;

family:
    TCP  { __sdp_rule.target_family = USE_TCP; }
  | SDP  { __sdp_rule.target_family = USE_SDP; }
  | BOTH { __sdp_rule.target_family = USE_BOTH; }
  ;

role:
    SERVER { current_role = 1; }
  | CLIENT { current_role = 2; }
  ;

program:
    NAME { __sdp_set_prog_name_expr($1); }
  | '*'  { __sdp_set_prog_name_expr("*"); }
  ;

address:
    SUBNET       { __sdp_rule.match_by_addr = 1; __sdp_set_ip_addr($1); }
  | '*'          { __sdp_rule.match_by_addr = 0; __sdp_rule.prefixlen = 32; }
  ;

ports:
    INT         { __sdp_rule.match_by_port = 1; __sdp_rule.sport= $1; __sdp_rule.eport= $1; }
  | INT '-' INT { __sdp_rule.match_by_port = 1; __sdp_rule.sport= $1; __sdp_rule.eport= $3; }
  | '*'         { __sdp_rule.match_by_port = 0; __sdp_rule.sport= 0 ; __sdp_rule.eport= 0; }
  ;

%%

int yyerror(char *msg)
{
	/* replace the $undefined and $end if exists */
	char *orig_msg = (char*)malloc(strlen(msg)+25);
	char *final_msg = (char*)malloc(strlen(msg)+25);

	strcpy(orig_msg, msg);
	
	char *word = strtok(orig_msg, " ");
	final_msg[0] = '\0';
	while (word != NULL) {
		if (!strncmp(word, "$undefined", 10)) {
			strcat(final_msg, "unrecognized-token ");
		} else if (!strncmp(word, "$end",4)) {
			strcat(final_msg, "end-of-file ");
		} else {
			strcat(final_msg, word);
			strcat(final_msg, " ");
		}
		word = strtok(NULL, " ");
	}
	
	__sdp_log(9, "Error (line:%ld) : %s\n", __sdp_config_line_num, final_msg);
	parse_err = 1;
	
	free(orig_msg);
	free(final_msg);
	return 1;
}

#include <unistd.h>
#include <errno.h>

/* parse apollo route dump file */
int __sdp_parse_config (const char *fileName) {
  extern FILE * libsdp_yyin;
   
  /* open the file */
  if (access(fileName, R_OK)) {
	 printf("libsdp Error: No access to open File:%s %s\n", 
           fileName, strerror(errno));
	 return(1);
  }

  libsdp_yyin = fopen(fileName,"r");
  if (!libsdp_yyin) {
	 printf("libsdp Error: Fail to open File:%s\n", fileName);
	 return(1);
  }
  parse_err = 0;
  __sdp_config_line_num = 1;

  /* parse it */
  yyparse();

  fclose(libsdp_yyin);
  return(parse_err);
}


