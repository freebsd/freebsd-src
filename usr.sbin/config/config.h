/*
 * Copyright (c) 1980, 1993
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
 *	@(#)config.h	8.1 (Berkeley) 6/6/93
 * $FreeBSD$
 */

/*
 * Config.
 */
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>

struct file_list {
	struct	file_list *f_next;
	char	*f_fn;			/* the name */
	int     f_type;                 /* type or count */
	u_char	f_flags;		/* see below */
	char	*f_compilewith;		/* special make rule if present */
	char	*f_depends;		/* additional dependancies */
	char	*f_clean;		/* File list to add to clean rule */
	char	*f_needs;
	char	*f_warn;		/* warning message */
};

/*
 * Types.
 */
#define NORMAL		1
#define	INVISIBLE	2
#define	PROFILING	3
#define NODEPEND	4
#define LOCAL		5
#define DEVDONE		0x80000000
#define TYPEMASK	0x7fffffff

/*
 * Attributes (flags).
 */
#define NO_IMPLCT_RULE	1
#define NO_OBJ		2
#define BEFORE_DEPEND	4
#define NEED_COUNT	8
#define ISDUP		16

struct device {
	int	d_done;			/* processed */
	char	*d_name;		/* name of device (e.g. rk11) */
	int	d_count;		/* device count */
#define	UNKNOWN -2	/* -2 means not set yet */
	struct	device *d_next;		/* Next one in list */
};

struct config {
	char	*s_sysname;
};

/*
 * Config has a global notion of which machine type is
 * being used.  It uses the name of the machine in choosing
 * files and directories.  Thus if the name of the machine is ``i386'',
 * it will build from ``Makefile.i386'' and use ``../i386/inline''
 * in the makerules, etc.
 */
char	*machinename;

/*
 * For each machine, a set of CPU's may be specified as supported.
 * These and the options (below) are put in the C flags in the makefile.
 */
struct cputype {
	char	*cpu_name;
	struct	cputype *cpu_next;
} *cputype;

/*
 * A set of options may also be specified which are like CPU types,
 * but which may also specify values for the options.
 * A separate set of options may be defined for make-style options.
 */
struct opt {
	char	*op_name;
	char	*op_value;
	int	op_ownfile;	/* true = own file, false = makefile */
	struct	opt *op_next;
} *opt, *mkopt;

struct opt_list {
	char *o_name;
	char *o_file;
	struct opt_list *o_next;
} *otab;

extern char	*ident;
extern char	*hints;
extern int	do_trace;
extern int	hintmode;

char	*get_word(FILE *);
char	*get_quoted_word(FILE *);
char	*path(const char *);
char	*raisestr(char *);
void	remember(const char *);
void	moveifchanged(const char *, const char *);
int	yyparse(void);
int	yylex(void);
void	options(void);
void	makefile(void);
void	headers(void);

extern struct	device *dtab;

extern char	errbuf[80];
extern int	yyline;
extern const	char *yyfile;

extern struct	file_list *ftab;

extern int	profiling;
extern int	debugging;

extern int	maxusers;

extern char *PREFIX;		/* Config file name - for error messages */
extern char srcdir[];		/* root of the kernel source tree */

#define eq(a,b)	(!strcmp(a,b))
#define ns(s)	strdup(s)
