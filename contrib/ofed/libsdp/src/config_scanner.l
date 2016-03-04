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
 * $Id: ibnl_scanner.ll,v 1.4 2005/02/23 21:08:37 eitan Exp $
 */

%{

/* #define DEBUG 1 */

#define yyparse libsdp_yyparse
#define yylex   libsdp_yylex
#define yyerror libsdp_yyerror
#define yylval  libsdp_yylval
#define yychar  libsdp_yychar
#define yydebug libsdp_yydebug
#define yynerrs libsdp_yynerrs

#define yywrap  libsdp_yywrap

#include <string.h>
#include <stdio.h>
#include "config_parser.h"
extern long __sdp_config_line_num;
%}
%s CANNAME
%%

^[ \t]*#.* {}

::|[0-9a-f:\.]*[0-9a-f](\/[0-9]+)?/:[0-9*] { 
    yylval.ival = SUBNET;
#ifdef DEBUG
    printf("SUBNET: %s\n", yytext);
#endif
    yylval.sval = (char *)malloc(strlen(yytext) + 1);
    strcpy(yylval.sval, yytext);
    return SUBNET;
}

([1-9][0-9]*|0) { 
  yylval.ival = atoi(yytext);
#ifdef DEBUG
  printf("INT:%d\n",yylval.ival);
#endif
  return INT;
}

log {
  yylval.ival = LOG;
#ifdef DEBUG
  printf("LOG\n");
#endif
  return LOG;
}

destination {
  yylval.ival = DEST;
#ifdef DEBUG
  printf("DEST\n");
#endif
  return DEST;
}

min-level {
  yylval.ival = LEVEL;
#ifdef DEBUG
  printf("LEVEL\n");
#endif
  return LEVEL;
}

stderr {
  yylval.ival = STDERR;
#ifdef DEBUG
  printf("STDERR\n");
#endif
  return STDERR;
}

syslog {
  yylval.ival = SYSLOG;
#ifdef DEBUG
  printf("SYSLOG\n");
#endif
  return SYSLOG;
}

file {
  yylval.ival = FILENAME;
#ifdef DEBUG
  printf("FILENAME\n");
#endif
  BEGIN(CANNAME);
  return FILENAME;
}

use {
  yylval.ival = USE;
#ifdef DEBUG
  printf("USE\n");
#endif
  return USE;
}

tcp {
  yylval.ival = TCP;
#ifdef DEBUG
  printf("TCP\n");
#endif
  return TCP;
}

sdp {
  yylval.ival = SDP;
#ifdef DEBUG
  printf("SDP\n");
#endif
  return SDP;
}

both {
  yylval.ival = BOTH;
#ifdef DEBUG
  printf("BOTH\n");
#endif
  return BOTH;
}

client|connect {
  yylval.ival = CLIENT;
#ifdef DEBUG
  printf("CLIENT\n");
#endif
  BEGIN(CANNAME);
  return CLIENT;
}

server|listen {
  yylval.ival = SERVER;
#ifdef DEBUG
  printf("SERVER\n");
#endif
  BEGIN(CANNAME);
  return SERVER;
}

<CANNAME>[^ \t\n]+ {
	 yylval.sval = (char *)malloc(strlen(yytext) + 1);
    strcpy(yylval.sval, yytext);
#ifdef DEBUG
	 printf("NAME:%s\n",yylval.sval);
#endif
    BEGIN(0);
	 return (NAME);
}

\n {
  __sdp_config_line_num++;
#ifdef DEBUG
  printf("LINE\n");
#endif
  yylval.ival = LINE;
  return(LINE);
}

[#][^\n]* {
  __sdp_config_line_num++;
}

[ \t]+ {}

. {
#ifdef DEBUG
  printf("CHAR:%c\n",yytext[0]);
#endif
  return(yytext[0]);
}

%%

int yywrap ()
{
  return (1);
}

