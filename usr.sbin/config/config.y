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
%token	EQUALS
%token	HINTS
%token	IDENT
%token	MAXUSERS
%token	OPTIONS
%token	MAKEOPTIONS
%token	SEMICOLON

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

static struct	device cur;
static struct	device *curp = 0;

struct  device *dtab;
char	*ident;
char	*hints;
int	hintmode;
int	yyline;
struct  file_list *ftab;
char	errbuf[80];
int	maxusers;

#define ns(s)	strdup(s)

static void yyerror(char *s);


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
	      = { newdev(&cur); } |
	Config_spec SEMICOLON
		|
	SEMICOLON
		|
	error SEMICOLON
		;

Config_spec:
	ARCH Save_id
	    = {
		if (!strcmp($2, "i386")) {
			machine = MACHINE_I386;
			machinename = "i386";
		} else if (!strcmp($2, "pc98")) {
			machine = MACHINE_PC98;
			machinename = "pc98";
		} else if (!strcmp($2, "alpha")) {
			machine = MACHINE_ALPHA;
			machinename = "alpha";
		} else if (!strcmp($2, "ia64")) {
			machine = MACHINE_IA64;
			machinename = "ia64";
		} else
			yyerror("Unknown machine type");
	      } |
	CPU Save_id
	      = {
		struct cputype *cp =
		    (struct cputype *)malloc(sizeof (struct cputype));
		memset(cp, 0, sizeof(*cp));
		cp->cpu_name = $2;
		cp->cpu_next = cputype;
		cputype = cp;
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
	HINTS ID
	      = {
		      hints = $2;
		      hintmode = 1;
		};

System_spec:
	CONFIG System_id System_parameter_list
	  = { warnx("line %d: root/dump/swap specifications obsolete", yyline);}
	  |
	CONFIG System_id
	  ;

System_id:
	Save_id
	      = {
		struct opt *op = (struct opt *)malloc(sizeof (struct opt));
		memset(op, 0, sizeof(*op));
		op->op_name = ns("KERNEL");
		op->op_ownfile = 0;
		op->op_next = mkopt;
		op->op_value = $1;
		op->op_line = yyline + 1;
		mkopt = op;
	      };

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
		struct opt *op = (struct opt *)malloc(sizeof (struct opt));
		char *s;
		memset(op, 0, sizeof(*op));
		op->op_name = $1;
		op->op_next = opt;
		op->op_value = 0;
		/*
		 * op->op_line is 1-based; yyline is 0-based but is now 1
		 * larger than when `Save_id' was lexed.
		 */
		op->op_line = yyline;
		opt = op;
		if ((s = strchr(op->op_name, '='))) {
			warnx("line %d: The `=' in options should not be quoted", yyline);
			*s = '\0';
			op->op_value = ns(s + 1);
		}
	      } |
	Save_id EQUALS Opt_value
	      = {
		struct opt *op = (struct opt *)malloc(sizeof (struct opt));
		memset(op, 0, sizeof(*op));
		op->op_name = $1;
		op->op_next = opt;
		op->op_value = $3;
		op->op_line = yyline + 1;
		opt = op;
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
	      = {
		struct opt *op = (struct opt *)malloc(sizeof (struct opt));
		memset(op, 0, sizeof(*op));
		op->op_name = $1;
		op->op_ownfile = 0;	/* for now */
		op->op_next = mkopt;
		op->op_value = $3;
		op->op_line = yyline + 1;
		mkopt = op;
	      } ;

Dev:
	ID
	      = { $$ = $1; }
	;

Device_spec:
	DEVICE Dev
	      = {
		cur.d_type = DEVICE;
		cur.d_name = $2;
		cur.d_count = UNKNOWN;
		} |
	DEVICE Dev NUMBER
	      = {
		cur.d_type = DEVICE;
		cur.d_name = $2;
		cur.d_count = $3;
		if (cur.d_count == 0)
			warnx("line %d: devices with zero units are not likely to be correct", yyline);
		} ;

%%

static void
yyerror(char *s)
{

	warnx("line %d: %s", yyline + 1, s);
}

/*
 * add a device to the list of devices
 */
static void
newdev(struct device *dp)
{
	struct device *np;

	np = (struct device *) malloc(sizeof *np);
	memset(np, 0, sizeof(*np));
	*np = *dp;
	np->d_name = dp->d_name;
	np->d_type = dp->d_type;
	np->d_count = dp->d_count;
	np->d_next = 0;
	if (curp == 0)
		dtab = np;
	else
		curp->d_next = np;
	curp = np;
}
