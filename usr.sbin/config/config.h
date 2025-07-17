/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
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
 * 3. Neither the name of the University nor the names of its contributors
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
 */

/*
 * Config.
 */
#include <sys/types.h>
#include <sys/queue.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
#include <string>

class configword {
private:
	std::string	cw_word;
	bool		cw_eof;
	bool		cw_eol;
public:
	configword() : cw_word(""), cw_eof(false), cw_eol(false) {}
	configword(std::string &&word) : cw_word(word), cw_eof(false), cw_eol(false) {}

	bool eof() const {
		return (cw_eof);
	}

	bool eol() const {
		return (cw_eol);
	}

	configword &eof(bool eof) {
		cw_eof = eof;
		return (*this);
	}

	configword &eol(bool eol) {
		cw_eol = eol;
		return (*this);
	}

	char operator[](int idx) {
		return (cw_word[idx]);
	}

	operator const char*() const {
		return (cw_word.c_str());
	}

	const std::string &operator*() const {
		return (cw_word);
	}

	const std::string *operator->() const {
		return (&cw_word);
	}
};

/*
 * Is it ugly to limit these to C++ files? Yes.
 */
configword get_word(FILE *);
configword get_quoted_word(FILE *);
#endif

__BEGIN_DECLS

struct cfgfile {
	STAILQ_ENTRY(cfgfile)	cfg_next;
	char	*cfg_path;
};
extern STAILQ_HEAD(cfgfile_head, cfgfile) cfgfiles;

struct file_list {
	STAILQ_ENTRY(file_list) f_next;
	char	*f_fn;			/* the name */
	int     f_type;                 /* type */
	u_char	f_flags;		/* see below */
	char	*f_compilewith;		/* special make rule if present */
	char	*f_depends;		/* additional dependencies */
	char	*f_clean;		/* File list to add to clean rule */
	char	*f_warn;		/* warning message */
	const char *f_objprefix;	/* prefix string for object name */
	const char *f_srcprefix;	/* source prefix such as $S/ */
};

struct files_name {
	char *f_name;
	STAILQ_ENTRY(files_name) f_next;
};

/*
 * Types.
 */
#define NORMAL		1
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
#define NOWERROR	16
#define NO_CTFCONVERT	32

struct device {
	int	d_done;			/* processed */
	char	*d_name;		/* name of device (e.g. rk11) */
	char	*yyfile;		/* name of the file that first include the device */
#define	UNKNOWN -2	/* -2 means not set yet */
	STAILQ_ENTRY(device) d_next;	/* Next one in list */
};

struct config {
	char	*s_sysname;
};

/*
 * Config has a global notion of which machine type is
 * being used.  It uses the name of the machine in choosing
 * files and directories.  Thus if the name of the machine is ``i386'',
 * it will build from ``Makefile.i386'' and use ``../i386/inline''
 * in the makerules, etc.  machinearch is the global notion of the
 * MACHINE_ARCH for this MACHINE.
 */
extern char	*machinename;
extern char	*machinearch;

/*
 * For each machine, a set of CPU's may be specified as supported.
 * These and the options (below) are put in the C flags in the makefile.
 */
struct cputype {
	char	*cpu_name;
	SLIST_ENTRY(cputype) cpu_next;
};

extern SLIST_HEAD(cputype_head, cputype) cputype;

/*
 * A set of options may also be specified which are like CPU types,
 * but which may also specify values for the options.
 * A separate set of options may be defined for make-style options.
 */
struct opt {
	char	*op_name;
	char	*op_value;
	int	op_ownfile;	/* true = own file, false = makefile */
	char	*yyfile;	/* name of the file that first include the option */
	SLIST_ENTRY(opt) op_next;
	SLIST_ENTRY(opt) op_append;
};

extern SLIST_HEAD(opt_head, opt) opt, mkopt, rmopts;

struct opt_list {
	char *o_name;
	char *o_file;
	int o_flags;
#define OL_ALIAS	1
	SLIST_ENTRY(opt_list) o_next;
};

extern SLIST_HEAD(opt_list_head, opt_list) otab;

struct envvar {
	char	*env_str;
	bool	env_is_file;
	STAILQ_ENTRY(envvar) envvar_next;
};

extern STAILQ_HEAD(envvar_head, envvar) envvars;

struct hint {
	char	*hint_name;
	STAILQ_ENTRY(hint) hint_next;
};

extern STAILQ_HEAD(hint_head, hint) hints;

struct includepath {
	char	*path;
	SLIST_ENTRY(includepath) path_next;
};

extern SLIST_HEAD(includepath_head, includepath) includepath;

/*
 * Tag present in the kernconf.tmpl template file. It's mandatory for those
 * two strings to be the same. Otherwise you'll get into trouble.
 */
#define	KERNCONFTAG	"%%KERNCONFFILE%%"

/*
 * Faked option to note, that the configuration file has been taken from the
 * kernel file and inclusion of DEFAULTS etc.. isn't nessesery, because we
 * already have a list of all required devices.
 */
#define OPT_AUTOGEN	"CONFIG_AUTOGENERATED"

extern char	*ident;
extern char	kernconfstr[];
extern int	do_trace;
extern int	incignore;

char	*path(const char *);
char	*raisestr(char *);
void	remember(const char *);
void	moveifchanged(const char *, const char *);
int	yylex(void);
int	yyparse(void);
void	options(void);
void	makefile(void);
void	makeenv(void);
void	makehints(void);
void	headers(void);
void	cfgfile_add(const char *);
void	cfgfile_removeall(void);
FILE	*open_makefile_template(void);

extern STAILQ_HEAD(device_head, device) dtab;

extern char	errbuf[80];
extern int	yyline;
extern const	char *yyfile;

extern STAILQ_HEAD(file_list_head, file_list) ftab;

extern STAILQ_HEAD(files_name_head, files_name) fntab;
extern STAILQ_HEAD(options_files_name_head, files_name) optfntab;

extern int	debugging;
extern int	found_defaults;
extern int	verbose;

extern int	maxusers;
extern int	versreq;

extern char *PREFIX;		/* Config file name - for error messages */
extern char srcdir[];		/* root of the kernel source tree */

__END_DECLS;

#define eq(a,b)	(!strcmp(a,b))
#define ns(s)	strdup(s)
