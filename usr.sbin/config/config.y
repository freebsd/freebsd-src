%union {
	char	*str;
	int	val;
	struct	file_list *file;
}

%token	ARCH
%token	COMMA
%token	CONFIG
%token	CPU
%token	DEVICE
%token	ENV
%token	EQUALS
%token	HINTS
%token	IDENT
%token	MAXUSERS
%token	PROFILE
%token	OPTIONS
%token	MAKEOPTIONS
%token	SEMICOLON
%token	INCLUDE

%token	<str>	ID
%token	<val>	NUMBER

%type	<str>	Save_id
%type	<str>	Opt_value
%type	<str>	Dev

%{

/*
 * Copyright (c) 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)config.y	8.1 (Berkeley) 6/6/93
 * $FreeBSD$
 */

#include <ctype.h>
#include <err.h>
#include <stdio.h>
#include <string.h>

#include "config.h"

struct	device_head dtab;
char	*ident;
char	*env;
int	envmode;
char	*hints;
int	hintmode;
int	yyline;
const	char *yyfile;
struct  file_list_head ftab;
char	errbuf[80];
int	maxusers;

#define ns(s)	strdup(s)
int include(const char *, int);
void yyerror(const char *s);

static char *
devopt(char *dev)
{
	char *ret = malloc(strlen(dev) + 5);
	
	sprintf(ret, "DEV_%s", dev);
	raisestr(ret);
	return ret;
}

%}
%%
Configuration:
	Many_specs
		;

Many_specs:
	Many_specs Spec
		|
	/* lambda */
		;

Spec:
	Device_spec SEMICOLON
		|
	Config_spec SEMICOLON
		|
	SEMICOLON
		|
	error SEMICOLON
		;

Config_spec:
	ARCH Save_id
	    = {
		machinename = $2;
	      } |
	CPU Save_id
	      = {
		struct cputype *cp =
		    (struct cputype *)malloc(sizeof (struct cputype));
		memset(cp, 0, sizeof(*cp));
		cp->cpu_name = $2;
		SLIST_INSERT_HEAD(&cputype, cp, cpu_next);
	      } |
	OPTIONS Opt_list
		|
	MAKEOPTIONS Mkopt_list
		|
	IDENT ID
	      = { ident = $2; } |
	System_spec
		|
	MAXUSERS NUMBER
	      = { maxusers = $2; } |
	PROFILE NUMBER
	      = { profiling = $2; } |
	ENV ID
	      = {
		      env = $2;
		      envmode = 1;
		} |
	HINTS ID
	      = {
		      hints = $2;
		      hintmode = 1;
	        } |
	INCLUDE ID
	      = { include($2, 0); };

System_spec:
	CONFIG System_id System_parameter_list
	  = { errx(1, "%s:%d: root/dump/swap specifications obsolete",
	      yyfile, yyline);}
	  |
	CONFIG System_id
	  ;

System_id:
	Save_id
	      = { newopt(&mkopt, ns("KERNEL"), $1); };

System_parameter_list:
	  System_parameter_list ID
	| ID
	;

Opt_list:
	Opt_list COMMA Option
		|
	Option
		;

Option:
	Save_id
	      = {
		char *s;

		newopt(&opt, $1, NULL);
		if ((s = strchr($1, '=')))
			errx(1, "%s:%d: The `=' in options should not be "
			    "quoted", yyfile, yyline);
	      } |
	Save_id EQUALS Opt_value
	      = {
		newopt(&opt, $1, $3);
	      } ;

Opt_value:
	ID
		= { $$ = $1; } |
	NUMBER
		= {
			char buf[80];

			(void) snprintf(buf, sizeof(buf), "%d", $1);
			$$ = ns(buf);
		} ;

Save_id:
	ID
	      = { $$ = $1; }
	;

Mkopt_list:
	Mkopt_list COMMA Mkoption
		|
	Mkoption
		;

Mkoption:
	Save_id EQUALS Opt_value
	      = { newopt(&mkopt, $1, $3); } ;

Dev:
	ID
	      = { $$ = $1; }
	;

Device_spec:
	DEVICE Dev
	      = {
		newopt(&opt, devopt($2), ns("1"));
		/* and the device part */
		newdev($2, UNKNOWN);
		} |
	DEVICE Dev NUMBER
	      = {
		newopt(&opt, devopt($2), ns("1"));
		/* and the device part */
		newdev($2, $3);
		if ($3 == 0)
			errx(1, "%s:%d: devices with zero units are not "
			    "likely to be correct", yyfile, yyline);
		} ;

%%

void
yyerror(const char *s)
{

	errx(1, "%s:%d: %s", yyfile, yyline + 1, s);
}

/*
 * add a device to the list of devices
 */
static void
newdev(char *name, int count)
{
	struct device *np;

	np = (struct device *) malloc(sizeof *np);
	memset(np, 0, sizeof(*np));
	np->d_name = name;
	np->d_count = count;
	STAILQ_INSERT_TAIL(&dtab, np, d_next);
}

static void
newopt(struct opt_head *list, char *name, char *value)
{
	struct opt *op;

	op = (struct opt *)malloc(sizeof (struct opt));
	memset(op, 0, sizeof(*op));
	op->op_name = name;
	op->op_ownfile = 0;
	op->op_value = value;
	SLIST_INSERT_HEAD(list, op, op_next);
}
