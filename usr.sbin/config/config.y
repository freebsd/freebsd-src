%union {
	char	*str;
	int	val;
	struct	file_list *file;
}

%token	ANY
%token	ARCH
%token	AT
%token	BUS
%token	COMMA
%token	CONFIG
%token	CONFLICTS
%token	CONTROLLER
%token	CPU
%token	DEVICE
%token	DISABLE
%token	DISK
%token	DRIVE
%token	DRQ
%token	EQUALS
%token	FLAGS
%token	IDENT
%token	IOMEM
%token	IOSIZ
%token	IRQ
%token	MAXUSERS
%token	MINUS
%token	NEXUS
%token	OPTIONS
%token	MAKEOPTIONS
%token	PORT
%token	PSEUDO_DEVICE
%token	SEMICOLON
%token	TAPE
%token	TARGET
%token	TTY
%token	UNIT
%token	VECTOR

%token	<str>	ID
%token	<val>	NUMBER
%token	<val>	FPNUMBER

%type	<str>	Save_id
%type	<str>	Opt_value
%type	<str>	Dev
%type	<str>	device_name

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
int	yyline;
struct  file_list *ftab;
char	errbuf[80];
int	maxusers;

#define ns(s)	strdup(s)

static int connect __P((char *, int));
static void yyerror __P((char *s));


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
	      = { maxusers = $2; };

System_spec:
	CONFIG System_id System_parameter_list
	  = { errx(1,"line %d: root/dump/swap specifications obsolete", yyline);}
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

device_name:
	  Save_id
		= { $$ = $1; }
	| Save_id NUMBER
		= {
			char buf[80];

			(void) snprintf(buf, sizeof(buf), "%s%d", $1, $2);
			$$ = ns(buf); free($1);
		}
	| Save_id NUMBER ID
		= {
			char buf[80];

			(void) snprintf(buf, sizeof(buf), "%s%d%s", $1, $2, $3);
			$$ = ns(buf); free($1);
		}
	| Save_id NUMBER ID NUMBER
		= {
			char buf[80];

			(void) snprintf(buf, sizeof(buf), "%s%d%s%d",
			     $1, $2, $3, $4);
			$$ = ns(buf); free($1);
		}
	| Save_id NUMBER ID NUMBER ID
		= {
			char buf[80];

			(void) snprintf(buf, sizeof(buf), "%s%d%s%d%s",
			     $1, $2, $3, $4, $5);
			$$ = ns(buf); free($1);
		}
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
		if ((s = strchr(op->op_name, '=')))
			errx(1, "line %d: The `=' in options should not be quoted", yyline);
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
	DEVICE Dev_spec
	      = { cur.d_type = DEVICE; } |
	DISK Dev_spec
	      = {
		errx(1, "line %d: Obsolete keyword 'disk' found - use 'device'", yyline);
		} |
	TAPE Dev_spec
	      = {
		errx(1, "line %d: Obsolete keyword 'tape' found - use 'device'", yyline);
		} |
	CONTROLLER Dev_spec
	      = {
		errx(1, "line %d: Obsolete keyword 'controller' found - use 'device'", yyline);
	        } |
	PSEUDO_DEVICE Init_dev Dev
	      = {
		cur.d_name = $3;
		cur.d_type = PSEUDO_DEVICE;
		} |
	PSEUDO_DEVICE Init_dev Dev NUMBER
	      = {
		cur.d_name = $3;
		cur.d_type = PSEUDO_DEVICE;
		cur.d_count = $4;
		} ;

Dev_spec:
	Init_dev Dev
	      = {
		cur.d_name = $2;
		cur.d_unit = UNKNOWN;
		} |
	Init_dev Dev NUMBER Dev_info
	      = {
		cur.d_name = $2;
		cur.d_unit = $3;
		};

Init_dev:
	/* lambda */
	      = { init_dev(&cur); };

Dev_info:
	Con_info Info_list
		|
	/* lambda */
		;

Con_info:
	AT Dev NUMBER
	      = {
		connect($2, $3);
		cur.d_conn = $2;
		cur.d_connunit = $3;
		} |
	AT NEXUS NUMBER
	      = {
	        cur.d_conn = "nexus";
	        cur.d_connunit = 0;
	        };
    
Info_list:
	Info_list Info
		|
	/* lambda */
		;

Info:
	BUS NUMBER	/* device scbus1 at ahc0 bus 1 - twin channel */
	      = { cur.d_bus = $2; } |
	TARGET NUMBER
	      = { cur.d_target = $2; } |
	UNIT NUMBER
	      = { cur.d_lun = $2; } |
	DRIVE NUMBER
	      = { cur.d_drive = $2; } |
	IRQ NUMBER
	      = { cur.d_irq = $2; } |
	DRQ NUMBER
	      = { cur.d_drq = $2; } |
	IOMEM NUMBER
	      = { cur.d_maddr = $2; } |
	IOSIZ NUMBER
	      = { cur.d_msize = $2; } |
	PORT device_name
	      = { cur.d_port = $2; } |
	PORT NUMBER
	      = { cur.d_portn = $2; } |
	FLAGS NUMBER
	      = { cur.d_flags = $2; } |
	DISABLE	
	      = { cur.d_disabled = 1; } |
	CONFLICTS
	      = {
		errx(1, "line %d: Obsolete keyword 'conflicts' found", yyline);
		};

%%

static void
yyerror(s)
	char *s;
{

	errx(1, "line %d: %s", yyline + 1, s);
}

/*
 * add a device to the list of devices
 */
static void
newdev(dp)
	register struct device *dp;
{
	register struct device *np, *xp;

	if (dp->d_unit >= 0) {
		for (xp = dtab; xp != 0; xp = xp->d_next) {
			if ((xp->d_unit == dp->d_unit) &&
			    eq(xp->d_name, dp->d_name)) {
				errx(1, "line %d: already seen device %s%d",
				    yyline, xp->d_name, xp->d_unit);
			}
		}
	}
	np = (struct device *) malloc(sizeof *np);
	memset(np, 0, sizeof(*np));
	*np = *dp;
	np->d_next = 0;
	if (curp == 0)
		dtab = np;
	else
		curp->d_next = np;
	curp = np;
}


/*
 * find the pointer to connect to the given device and number.
 * returns 0 if no such device and prints an error message
 */
static int
connect(dev, num)
	register char *dev;
	register int num;
{
	register struct device *dp;

	if (num == QUES) {
		for (dp = dtab; dp != 0; dp = dp->d_next)
			if (eq(dp->d_name, dev))
				break;
		if (dp == 0) {
			(void) snprintf(errbuf, sizeof(errbuf),
			    "no %s's to wildcard", dev);
			yyerror(errbuf);
			return (0);
		}
		return (1);
	}
	for (dp = dtab; dp != 0; dp = dp->d_next) {
		if ((num != dp->d_unit) || !eq(dev, dp->d_name))
			continue;
		if (dp->d_type != DEVICE) {
			(void) snprintf(errbuf, sizeof(errbuf), 
			    "%s connected to non-device", dev);
			yyerror(errbuf);
			return (0);
		}
		return (1);
	}
	(void) snprintf(errbuf, sizeof(errbuf), "%s %d not defined", dev, num);
	yyerror(errbuf);
	return (0);
}

void
init_dev(dp)
	register struct device *dp;
{

	dp->d_name = "OHNO!!!";
	dp->d_type = DEVICE;
	dp->d_conn = 0;
	dp->d_disabled = 0;
	dp->d_flags = 0;
	dp->d_bus = dp->d_lun = dp->d_target = dp->d_drive = dp->d_unit = \
		dp->d_count = UNKNOWN;
	dp->d_port = (char *)0;
	dp->d_portn = -1;
	dp->d_irq = -1;
	dp->d_drq = -1;
	dp->d_maddr = 0;
	dp->d_msize = 0;
}
