/*-
 * This code is derived from software copyrighted by the Free Software
 * Foundation.
 *
 * Modified 1991 by Donn Seeley at UUNET Technologies, Inc.
 *
 * Modified 1993 by Paul Kranenburg, Erasmus University
 */

#ifndef lint
static char sccsid[] = "@(#)ld.c	6.10 (Berkeley) 5/22/91";
#endif /* not lint */

/* Linker `ld' for GNU
   Copyright (C) 1988 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 1, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

/* Written by Richard Stallman with some help from Eric Albert.
   Set, indirect, and warning symbol features added by Randy Smith. */

/*
 *	$Id: ld.c,v 1.43 1997/04/11 17:08:56 bde Exp $
 */

/* Define how to initialize system-dependent header fields.  */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <err.h>
#include <fcntl.h>
#include <ar.h>
#include <ranlib.h>
#include <a.out.h>
#include <stab.h>
#include <string.h>

#include "ld.h"
#include "dynamic.h"

/* Vector of entries for input files specified by arguments.
   These are all the input files except for members of specified libraries. */
struct file_entry	*file_table;
int			number_of_files;

/* 1 => write relocation into output file so can re-input it later. */
int	relocatable_output;

/* 1 => building a shared object, set by `-Bshareable'. */
int	building_shared_object;

/* 1 => create the output executable. */
int	make_executable;

/* Force the executable to be output, even if there are non-fatal errors */
int	force_executable;

/* 1 => assign space to common symbols even if `relocatable_output'.  */
int	force_common_definition;

/* 1 => assign jmp slots to text symbols in shared objects even if non-PIC */
int	force_alias_definition;

/* 1 => some files contain PIC code, affects relocation bits
	if `relocatable_output'. */
int	pic_code_seen;

/* 1 => segments must be page aligned (ZMAGIC, QMAGIC) */
int	page_align_segments;

/* 1 => data segment must be page aligned, even if `-n' or `-N' */
int	page_align_data;

/* 1 => do not use standard library search path */
int	nostdlib;

/* Version number to put in __DYNAMIC (set by -V) */
int	soversion;

int	text_size;		/* total size of text. */
int	text_start;		/* start of text */
int	text_pad;		/* clear space between text and data */
int	data_size;		/* total size of data. */
int	data_start;		/* start of data */
int	data_pad;		/* part of bss segment as part of data */

int	bss_size;		/* total size of bss. */
int	bss_start;		/* start of bss */

int	text_reloc_size;	/* total size of text relocation. */
int	data_reloc_size;	/* total size of data relocation. */

int	rrs_section_type;	/* What's in the RRS section */
int	rrs_text_size;		/* Size of RRS text additions */
int	rrs_text_start;		/* Location of above */
int	rrs_data_size;		/* Size of RRS data additions */
int	rrs_data_start;		/* Location of above */

/* Specifications of start and length of the area reserved at the end
   of the data segment for the set vectors.  Computed in 'digest_symbols' */
int	set_sect_start;		/* start of set element vectors */
int	set_sect_size;		/* size of above */

int	link_mode;		/* Current link mode */
int	pic_type;		/* PIC type */

/*
 * When loading the text and data, we can avoid doing a close
 * and another open between members of the same library.
 *
 * These two variables remember the file that is currently open.
 * Both are zero if no file is open.
 *
 * See `each_file' and `file_close'.
 */
struct file_entry	*input_file;
int			input_desc;

/* The name of the file to write; "a.out" by default. */
char		*output_filename;	/* Output file name. */
char		*real_output_filename;	/* Output file name. */
FILE		*outstream;		/* Output file descriptor. */
struct exec	outheader;		/* Output file header. */
int		magic;			/* Output file magic. */
int		oldmagic;
int		relocatable_output;	/* `-r'-ed output */

symbol		*entry_symbol;		/* specified by `-e' */
int		entry_offset;		/* program entry if no `-e' given */

int		page_size;		/* Size of a page (machine dependent) */

/*
 * Keep a list of any symbols referenced from the command line (so
 * that error messages for these guys can be generated). This list is
 * zero terminated.
 */
symbol		**cmdline_references;
int		cl_refs_allocated;

/*
 * Which symbols should be stripped (omitted from the output): none, all, or
 * debugger symbols.
 */
enum {
	STRIP_NONE, STRIP_ALL, STRIP_DEBUGGER
} strip_symbols;

/*
 * Which local symbols should be omitted: none, all, or those starting with L.
 * This is irrelevant if STRIP_NONE.
 */
enum {
	DISCARD_NONE, DISCARD_ALL, DISCARD_L
} discard_locals;

int	global_sym_count;	/* # of nlist entries for global symbols */
int	size_sym_count;		/* # of N_SIZE nlist entries for output
				  (relocatable_output only) */
int	local_sym_count;	/* # of nlist entries for local symbols. */
int	non_L_local_sym_count;	/* # of nlist entries for non-L symbols */
int	debugger_sym_count;	/* # of nlist entries for debugger info. */
int	undefined_global_sym_count;	/* # of global symbols referenced and
					   not defined. */
int	undefined_shobj_sym_count;	/* # of undefined symbols referenced
					   by shared objects */
int	multiple_def_count;		/* # of multiply defined symbols. */
int	defined_global_sym_count;	/* # of defined global symbols. */
int	common_defined_global_count;	/* # of common symbols. */
int	undefined_weak_sym_count;	/* # of weak symbols referenced and
					   not defined. */

#if notused
int	special_sym_count;	/* # of linker defined symbols. */
	/* XXX - Currently, only __DYNAMIC and _G_O_T_ go here if required,
	 *  perhaps _etext, _edata and _end should go here too.
	 */
#endif
int	global_alias_count;	/* # of aliased symbols */
int	set_symbol_count;	/* # of N_SET* symbols. */
int	set_vector_count;	/* # of set vectors in output. */
int	warn_sym_count;		/* # of warning symbols encountered. */
int	flag_list_files;	/* 1 => print pathnames of files, don't link */
int	list_warning_symbols;	/* 1 => warning symbols referenced */

struct string_list_element	*set_element_prefixes;

int	trace_files;	/* print names of input files as processed (`-t'). */
int	write_map;	/* write a load map (`-M') */

/*
 * `text-start' address is normally this much plus a page boundary.
 * This is not a user option; it is fixed for each system.
 */
int	text_start_alignment;

/*
 * Nonzero if -T was specified in the command line.
 * This prevents text_start from being set later to default values.
 */
int	T_flag_specified;

/*
 * Nonzero if -Tdata was specified in the command line.
 * This prevents data_start from being set later to default values.
 */
int	Tdata_flag_specified;

/*
 * Size to pad data section up to.
 * We simply increase the size of the data section, padding with zeros,
 * and reduce the size of the bss section to match.
 */
int	specified_data_size;

long	*set_vectors;
int	setv_fill_count;

static void	decode_option __P((char *, char *));
static void	decode_command __P((int, char **));
static int	classify_arg __P((char *));
static void	load_symbols __P((void));
static void	enter_global_ref __P((struct localsymbol *,
						char *, struct file_entry *));
static void	digest_symbols __P((void));
static void	digest_pass1 __P((void)), digest_pass2 __P((void));
static void	consider_file_section_lengths __P((struct file_entry *));
static void	relocate_file_addresses __P((struct file_entry *));
static void	consider_relocation __P((struct file_entry *, int));
static void	consider_local_symbols __P((struct file_entry *));
static void	perform_relocation __P((char *, int,
						struct relocation_info *, int,
						struct file_entry *, int));
static void	copy_text __P((struct file_entry *));
static void	copy_data __P((struct file_entry *));
static void	coptxtrel __P((struct file_entry *));
static void	copdatrel __P((struct file_entry *));
static void	write_output __P((void));
static void	write_header __P((void));
static void	write_text __P((void));
static void	write_data __P((void));
static void	write_rel __P((void));
static void	write_syms __P((void));
static void	assign_symbolnums __P((struct file_entry *, int *));
static void	cleanup __P((void));
static int	parse __P((char *, char *, char *));
static void	list_files __P((void));


int
main(argc, argv)
	int	argc;
	char	*argv[];
{

	/* Added this to stop ld core-dumping on very large .o files.    */
#ifdef RLIMIT_STACK
	/* Get rid of any avoidable limit on stack size.  */
	{
		struct rlimit   rlim;

		/* Set the stack limit huge so that alloca does not fail. */
		if (getrlimit(RLIMIT_STACK, &rlim) != 0)
			warn("getrlimit");
		else {
			rlim.rlim_cur = rlim.rlim_max;
			if (setrlimit(RLIMIT_STACK, &rlim) != 0)
				warn("setrlimit");
		}
	}
#endif	/* RLIMIT_STACK */

	page_size = PAGSIZ;

	/* Clear the cumulative info on the output file.  */

	text_size = 0;
	data_size = 0;
	bss_size = 0;
	text_reloc_size = 0;
	data_reloc_size = 0;

	data_pad = 0;
	text_pad = 0;
	page_align_segments = 0;
	page_align_data = 0;

	/* Initialize the data about options.  */

	specified_data_size = 0;
	strip_symbols = STRIP_NONE;
	trace_files = 0;
	discard_locals = DISCARD_NONE;
	entry_symbol = 0;
	write_map = 0;
	relocatable_output = 0;
	force_common_definition = 0;
	T_flag_specified = 0;
	Tdata_flag_specified = 0;
	magic = DEFAULT_MAGIC;
	make_executable = 1;
	force_executable = 0;
	link_mode = DYNAMIC;
#ifdef SUNOS4
	link_mode |= SILLYARCHIVE;
#endif
	soversion = DEFAULT_SOVERSION;

	/* Initialize the cumulative counts of symbols.  */

	local_sym_count = 0;
	non_L_local_sym_count = 0;
	debugger_sym_count = 0;
	undefined_global_sym_count = 0;
	warn_sym_count = 0;
	list_warning_symbols = 0;
	multiple_def_count = 0;
	common_defined_global_count = 0;

	/* Keep a list of symbols referenced from the command line */
	cl_refs_allocated = 10;
	cmdline_references = (symbol **)
		xmalloc(cl_refs_allocated * sizeof(symbol *));
	*cmdline_references = 0;

	/* Completely decode ARGV.  */
	decode_command(argc, argv);

	if (flag_list_files)
		list_files();

	building_shared_object =
		(!relocatable_output && (link_mode & SHAREABLE));

	if (building_shared_object && entry_symbol) {
		errx(1,"`-Bshareable' and `-e' options are mutually exclusive");
	}

	/* Create the symbols `etext', `edata' and `end'.  */
	symtab_init(relocatable_output);

	/*
	 * Determine whether to count the header as part of the text size,
	 * and initialize the text size accordingly. This depends on the kind
	 * of system and on the output format selected.
	 */

	if (magic == ZMAGIC || magic == QMAGIC)
		page_align_segments = 1;

	md_init_header(&outheader, magic, 0);

	text_size = sizeof(struct exec);
	text_size -= N_TXTOFF(outheader);

	if (text_size < 0)
		text_size = 0;
	entry_offset = text_size;

	if (!T_flag_specified && !relocatable_output)
		text_start = TEXT_START(outheader);

	/* The text-start address is normally this far past a page boundary.  */
	text_start_alignment = text_start % page_size;

	/*
	 * Load symbols of all input files. Also search all libraries and
	 * decide which library members to load.
	 */
	load_symbols();

	/* Compute where each file's sections go, and relocate symbols.  */
	digest_symbols();

	/*
	 * Print error messages for any missing symbols, for any warning
	 * symbols, and possibly multiple definitions
	 */
	make_executable &= do_warnings(stderr);

	/* Print a map, if requested.  */
	if (write_map)
		print_symbols(stdout);

	/* Write the output file.  */
	if (make_executable || force_executable)
		write_output();

	exit(!make_executable);
}

/*
 * Analyze a command line argument. Return 0 if the argument is a filename.
 * Return 1 if the argument is a option complete in itself. Return 2 if the
 * argument is a option which uses an argument.
 *
 * Thus, the value is the number of consecutive arguments that are part of
 * options.
 */

static int
classify_arg(arg)
	register char  *arg;
{
	if (*arg != '-')
		return 0;
	switch (arg[1]) {
	case 'a':
		if (!strcmp(&arg[2], "ssert"))
			return 2;
	case 'A':
	case 'D':
	case 'e':
	case 'L':
	case 'l':
	case 'O':
	case 'o':
	case 'R':
	case 'u':
	case 'V':
	case 'y':
		if (arg[2])
			return 1;
		return 2;

	case 'B':
		if (!strcmp(&arg[2], "static"))
			return 1;
		if (!strcmp(&arg[2], "dynamic"))
			return 1;

	case 'T':
		if (arg[2] == 0)
			return 2;
		if (!strcmp(&arg[2], "text"))
			return 2;
		if (!strcmp(&arg[2], "data"))
			return 2;
		return 1;
	}

	return 1;
}

/*
 * Process the command arguments, setting up file_table with an entry for
 * each input file, and setting variables according to the options.
 */

static void
decode_command(argc, argv)
	int             argc;
	char          **argv;
{
	register int    i;
	register struct file_entry *p;

	number_of_files = 0;
	output_filename = "a.out";

	/*
	 * First compute number_of_files so we know how long to make
	 * file_table.
	 * Also process most options completely.
	 */

	for (i = 1; i < argc; i++) {
		register int    code = classify_arg(argv[i]);
		if (code) {
			if (i + code > argc)
				errx(1, "no argument following %s", argv[i]);

			decode_option(argv[i], argv[i + 1]);

			if (argv[i][1] == 'l' || argv[i][1] == 'A')
				number_of_files++;

			i += code - 1;
		} else
			number_of_files++;
	}

	if (!number_of_files) {
		if (flag_list_files)
			exit(0);
		errx(1, "No input files specified");
	}

	p = file_table = (struct file_entry *)
		xmalloc(number_of_files * sizeof(struct file_entry));
	bzero(p, number_of_files * sizeof(struct file_entry));

	/* Now scan again and fill in file_table.  */
	/* All options except -A and -l are ignored here.  */

	for (i = 1; i < argc; i++) {
		char           *string;
		register int    code = classify_arg(argv[i]);

		if (code == 0) {
			p->filename = argv[i];
			p->local_sym_name = argv[i];
			p++;
			continue;
		}
		if (code == 2)
			string = argv[i + 1];
		else
			string = &argv[i][2];

		if (argv[i][1] == 'B') {
			if (strcmp(string, "static") == 0)
				link_mode &= ~DYNAMIC;
			else if (strcmp(string, "dynamic") == 0)
				link_mode |= DYNAMIC;
			else if (strcmp(string, "symbolic") == 0)
				link_mode |= SYMBOLIC;
			else if (strcmp(string, "forcearchive") == 0)
				link_mode |= FORCEARCHIVE;
			else if (strcmp(string, "shareable") == 0)
				link_mode |= SHAREABLE;
#ifdef SUN_COMPAT
			else if (strcmp(string, "silly") == 0)
				link_mode |= SILLYARCHIVE;
			else if (strcmp(string, "~silly") == 0)
				link_mode &= ~SILLYARCHIVE;
#endif
		}
		if (argv[i][1] == 'A') {
			if (p != file_table)
				errx(1, "-A specified before an input file other than the first");
			p->filename = string;
			p->local_sym_name = string;
			p->flags |= E_JUST_SYMS;
			link_mode &= ~DYNAMIC;
			p++;
		}
		if (argv[i][1] == 'l') {
			p->filename = string;
			p->local_sym_name = concat("-l", string, "");
			p->flags |= E_SEARCH_DIRS;
			if (link_mode & DYNAMIC && !relocatable_output)
				p->flags |= E_SEARCH_DYNAMIC;
			p++;
		}
		i += code - 1;
	}

	/* Now check some option settings for consistency.  */

	if (page_align_segments &&
	    (text_start - text_start_alignment) & (page_size - 1))
		errx(1, "incorrect alignment of text start address");

	/* Append the standard search directories to the user-specified ones. */
	add_search_path(getenv("LD_LIBRARY_PATH"));
	if (!nostdlib)
		std_search_path();
}

void
add_cmdline_ref(sp)
	symbol *sp;
{
	symbol **ptr;

	for (ptr = cmdline_references;
	     ptr < cmdline_references + cl_refs_allocated && *ptr;
	     ptr++);

	if (ptr >= cmdline_references + cl_refs_allocated - 1) {
		int diff = ptr - cmdline_references;

		cl_refs_allocated *= 2;
		cmdline_references = (symbol **)
			xrealloc(cmdline_references,
			       cl_refs_allocated * sizeof(symbol *));
		ptr = cmdline_references + diff;
	}
	*ptr++ = sp;
	*ptr = (symbol *)0;
}

int
set_element_prefixed_p(name)
	char           *name;
{
	struct string_list_element *p;
	int             i;

	for (p = set_element_prefixes; p; p = p->next) {

		for (i = 0; p->str[i] != '\0' && (p->str[i] == name[i]); i++);
		if (p->str[i] == '\0')
			return 1;
	}
	return 0;
}

/*
 * Record an option and arrange to act on it later. ARG should be the
 * following command argument, which may or may not be used by this option.
 *
 * The `l' and `A' options are ignored here since they actually specify input
 * files.
 */

static void
decode_option(swt, arg)
	register char  *swt, *arg;
{
	if (!strcmp(swt + 1, "Bstatic"))
		return;
	if (!strcmp(swt + 1, "Bdynamic"))
		return;
	if (!strcmp(swt + 1, "Bsymbolic"))
		return;
	if (!strcmp(swt + 1, "Bforcearchive"))
		return;
	if (!strcmp(swt + 1, "Bshareable"))
		return;
	if (!strcmp(swt + 1, "assert"))
		return;
#ifdef SUN_COMPAT
	if (!strcmp(swt + 1, "Bsilly"))
		return;
#endif
	if (!strcmp(swt + 1, "Ttext")) {
		text_start = parse(arg, "%x", "invalid argument to -Ttext");
		T_flag_specified = 1;
		return;
	}
	if (!strcmp(swt + 1, "Tdata")) {
		rrs_data_start = parse(arg, "%x", "invalid argument to -Tdata");
		Tdata_flag_specified = 1;
		return;
	}
	if (!strcmp(swt + 1, "noinhibit-exec")) {
		force_executable = 1;
		return;
	}
	if (!strcmp(swt + 1, "nostdlib")) {
		nostdlib = 1;
		return;
	}
	if (swt[2] != 0)
		arg = &swt[2];

	switch (swt[1]) {
	case 'A':
		return;

	case 'D':
		specified_data_size = parse(arg, "%x", "invalid argument to -D");
		return;

	case 'd':
		if (swt[2] == 0 || *arg == 'c')
			force_common_definition = 1;
		else if (*arg == 'p')
			force_alias_definition = 1;
		else
			errx(1, "-d option takes 'c' or 'p' argument");
		return;

	case 'e':
		entry_symbol = getsym(arg);
		if (!entry_symbol->defined &&
				!(entry_symbol->flags & GS_REFERENCED))
			undefined_global_sym_count++;
		entry_symbol->flags |= GS_REFERENCED;
		add_cmdline_ref(entry_symbol);
		return;

	case 'f':
		flag_list_files = 1;
		return;

	case 'l':
		return;

	case 'L':
		add_search_dir(arg);
		return;

	case 'M':
		write_map = 1;
		return;

	case 'N':
		magic = OMAGIC;
		return;

	case 'n':
		magic = NMAGIC;
		return;

	case 'O':
		output_filename = xmalloc(strlen(arg)+5);
		strcpy(output_filename, arg);
		strcat(output_filename, ".tmp");
		real_output_filename = arg;
		return;

	case 'o':
		output_filename = arg;
		return;

	case 'p':
		page_align_data = 1;
		return;

#ifdef QMAGIC
	case 'Q':
		magic = QMAGIC;
		return;
#endif

	case 'r':
		relocatable_output = 1;
		magic = OMAGIC;
		text_start = 0;
		return;

	case 'R':
		rrs_search_paths = (rrs_search_paths == NULL)
			? strdup(arg)
			: concat(rrs_search_paths, ":", arg);
		return;

	case 'S':
		strip_symbols = STRIP_DEBUGGER;
		return;

	case 's':
		strip_symbols = STRIP_ALL;
		return;

	case 'T':
		text_start = parse(arg, "%x", "invalid argument to -T");
		T_flag_specified = 1;
		return;

	case 't':
		trace_files = 1;
		return;

	case 'u':
		{
			register symbol *sp = getsym(arg);

			if (!sp->defined && !(sp->flags & GS_REFERENCED))
				undefined_global_sym_count++;
			sp->flags |= GS_REFERENCED;
			add_cmdline_ref(sp);
		}
		return;

#if 1
	case 'V':
		soversion = parse(arg, "%d", "invalid argument to -V");
		return;
#endif

	case 'X':
		discard_locals = DISCARD_L;
		return;

	case 'x':
		discard_locals = DISCARD_ALL;
		return;

	case 'y':
		{
			register symbol *sp = getsym(&swt[2]);
			sp->flags |= GS_TRACE;
		}
		return;

	case 'z':
		magic = ZMAGIC;
		oldmagic = 0;
#ifdef __FreeBSD__
		netzmagic = 1;
#endif
		return;

	case 'Z':
		magic = oldmagic = ZMAGIC;
#ifdef __FreeBSD__
		netzmagic = 0;
#endif
		return;

	default:
		errx(1, "invalid command option `%s'", swt);
	}
}

/* Convenient functions for operating on one or all files being loaded. */

/*
 * Call FUNCTION on each input file entry. Do not call for entries for
 * libraries; instead, call once for each library member that is being
 * loaded.
 *
 * FUNCTION receives two arguments: the entry, and ARG.
 */

void
each_file(function, arg)
	register void	(*function)();
	register void	*arg;
{
	register int    i;

	for (i = 0; i < number_of_files; i++) {
		register struct file_entry *entry = &file_table[i];
		register struct file_entry *subentry;

		if (entry->flags & E_SCRAPPED)
			continue;

		if (!(entry->flags & E_IS_LIBRARY))
			(*function)(entry, arg);

		subentry = entry->subfiles;
		for (; subentry; subentry = subentry->chain) {
			if (subentry->flags & E_SCRAPPED)
				continue;
			(*function)(subentry, arg);
		}

#ifdef SUN_COMPAT
		if (entry->silly_archive) {

			if (!(entry->flags & E_DYNAMIC))
				warnx("Silly");

			if (!(entry->silly_archive->flags & E_IS_LIBRARY))
				warnx("Sillier");

			subentry = entry->silly_archive->subfiles;
			for (; subentry; subentry = subentry->chain) {
				if (subentry->flags & E_SCRAPPED)
					continue;
				(*function)(subentry, arg);
			}
		}
#endif
	}
}

/*
 * Call FUNCTION on each input file entry until it returns a non-zero value.
 * Return this value. Do not call for entries for libraries; instead, call
 * once for each library member that is being loaded.
 *
 * FUNCTION receives two arguments: the entry, and ARG.  It must be a function
 * returning unsigned long (though this can probably be fudged).
 */

unsigned long
check_each_file(function, arg)
	register unsigned long	(*function)();
	register void		*arg;
{
	register int    i;
	register unsigned long return_val;

	for (i = 0; i < number_of_files; i++) {
		register struct file_entry *entry = &file_table[i];
		if (entry->flags & E_SCRAPPED)
			continue;
		if (entry->flags & E_IS_LIBRARY) {
			register struct file_entry *subentry = entry->subfiles;
			for (; subentry; subentry = subentry->chain) {
				if (subentry->flags & E_SCRAPPED)
					continue;
				if ( (return_val = (*function)(subentry, arg)) )
					return return_val;
			}
		} else if ( (return_val = (*function)(entry, arg)) )
			return return_val;
	}
	return 0;
}

/* Like `each_file' but ignore files that were just for symbol definitions.  */

void
each_full_file(function, arg)
	register void	(*function)();
	register void	*arg;
{
	register int    i;

	for (i = 0; i < number_of_files; i++) {
		register struct file_entry *entry = &file_table[i];
		register struct file_entry *subentry;

		if (entry->flags & (E_SCRAPPED | E_JUST_SYMS))
			continue;

#ifdef SUN_COMPAT
		if (entry->silly_archive) {

			if (!(entry->flags & E_DYNAMIC))
				warnx("Silly");

			if (!(entry->silly_archive->flags & E_IS_LIBRARY))
				warnx("Sillier");

			subentry = entry->silly_archive->subfiles;
			for (; subentry; subentry = subentry->chain) {
				if (subentry->flags & E_SCRAPPED)
					continue;
				(*function)(subentry, arg);
			}
		}
#endif
		if (entry->flags & E_DYNAMIC)
			continue;

		if (!(entry->flags & E_IS_LIBRARY))
			(*function)(entry, arg);

		subentry = entry->subfiles;
		for (; subentry; subentry = subentry->chain) {
			if (subentry->flags & E_SCRAPPED)
				continue;
			(*function)(subentry, arg);
		}

	}
}

/* Close the input file that is now open.  */

void
file_close()
{
	close(input_desc);
	input_desc = 0;
	input_file = 0;
}

/*
 * Open the input file specified by 'entry', and return a descriptor. The
 * open file is remembered; if the same file is opened twice in a row, a new
 * open is not actually done.
 */
int
file_open(entry)
	register struct file_entry *entry;
{
	register int	fd;

	if (entry->superfile && (entry->superfile->flags & E_IS_LIBRARY))
		return file_open(entry->superfile);

	if (entry == input_file)
		return input_desc;

	if (input_file)
		file_close();

	if (entry->flags & E_SEARCH_DIRS) {
		fd = findlib(entry);
	} else
		fd = open(entry->filename, O_RDONLY, 0);

	if (fd >= 0) {
		input_file = entry;
		input_desc = fd;
		return fd;
	}

	if (entry->flags & E_SEARCH_DIRS)
		errx(1, "%s: no match", entry->local_sym_name);
	else
		err(1, "%s", entry->filename);
	return fd;
}

int
text_offset(entry)
     struct file_entry *entry;
{
	return entry->starting_offset + N_TXTOFF (entry->header);
}

/*---------------------------------------------------------------------------*/

/*
 * Read a file's header into the proper place in the file_entry. FD is the
 * descriptor on which the file is open. ENTRY is the file's entry.
 */
void
read_header(fd, entry)
	int	fd;
	struct file_entry *entry;
{
	register int len;

	if (lseek(fd, entry->starting_offset, L_SET) !=
	    entry->starting_offset)
		err(1, "%s: read_header: lseek", get_file_name(entry));

	len = read(fd, &entry->header, sizeof(struct exec));
	if (len != sizeof (struct exec))
		err(1, "%s: read_header: read", get_file_name(entry));

	md_swapin_exec_hdr(&entry->header);

	if (N_BADMAG (entry->header))
		errx(1, "%s: bad magic", get_file_name(entry));

	if (N_BADMID(entry->header))
		errx(1, "%s: non-native input file", get_file_name(entry));

	entry->flags |= E_HEADER_VALID;
}

/*
 * Read the symbols of file ENTRY into core. Assume it is already open, on
 * descriptor FD. Also read the length of the string table, which follows
 * the symbol table, but don't read the contents of the string table.
 */

void
read_entry_symbols(fd, entry)
	struct file_entry *entry;
	int fd;
{
	int		str_size;
	struct nlist	*np;
	int		i;

	if (!(entry->flags & E_HEADER_VALID))
		read_header(fd, entry);

	np = (struct nlist *)alloca(entry->header.a_syms);
	entry->nsymbols = entry->header.a_syms / sizeof(struct nlist);
	if (entry->nsymbols == 0)
		return;

	entry->symbols = (struct localsymbol *)
		xmalloc(entry->nsymbols * sizeof(struct localsymbol));

	if (lseek(fd, N_SYMOFF(entry->header) + entry->starting_offset, L_SET)
	    != N_SYMOFF(entry->header) + entry->starting_offset)
		err(1, "%s: read_symbols: lseek(syms)", get_file_name(entry));

	if (entry->header.a_syms != read(fd, np, entry->header.a_syms))
		errx(1, "%s: read_symbols: premature end of file in symbols",
			get_file_name(entry));

	md_swapin_symbols(np, entry->header.a_syms / sizeof(struct nlist));

	for (i = 0; i < entry->nsymbols; i++) {
		entry->symbols[i].nzlist.nlist = *np++;
		entry->symbols[i].nzlist.nz_size = 0;
		entry->symbols[i].symbol = NULL;
		entry->symbols[i].next = NULL;
		entry->symbols[i].entry = entry;
		entry->symbols[i].gotslot_offset = -1;
		entry->symbols[i].flags = 0;
	}

	entry->strings_offset = N_STROFF(entry->header) +
				entry->starting_offset;
	if (lseek(fd, entry->strings_offset, 0) == (off_t)-1)
		err(1, "%s: read_symbols: lseek(strings)",
			get_file_name(entry));
	if (sizeof str_size != read(fd, &str_size, sizeof str_size))
		errx(1, "%s: read_symbols: cannot read string table size",
			get_file_name(entry));

	entry->string_size = md_swap_long(str_size);
}

/*
 * Read the string table of file ENTRY open on descriptor FD, into core.
 */
void
read_entry_strings(fd, entry)
	struct file_entry *entry;
	int fd;
{

	if (entry->string_size == 0)
		return;

	if (!(entry->flags & E_HEADER_VALID) || !entry->strings_offset)
		errx(1, "%s: read_strings: string table unavailable",
			get_file_name(entry));

	if (lseek(fd, entry->strings_offset, L_SET) !=
	    entry->strings_offset)
		err(1, "%s: read_strings: lseek",
			get_file_name(entry));

	if (read(fd, entry->strings, entry->string_size) !=
	    entry->string_size)
		errx(1, "%s: read_strings: premature end of file in strings",
			get_file_name(entry));

	return;
}

/* Read in the relocation sections of ENTRY if necessary */

void
read_entry_relocation(fd, entry)
	int			fd;
	struct file_entry	*entry;
{
	register struct relocation_info *reloc;
	off_t	pos;

	if (!entry->textrel) {

		reloc = (struct relocation_info *)
			xmalloc(entry->header.a_trsize);

		pos = text_offset(entry) +
			entry->header.a_text + entry->header.a_data;

		if (lseek(fd, pos, L_SET) != pos)
			err(1, "%s: read_reloc(text): lseek",
				get_file_name(entry));

		if (read(fd, reloc, entry->header.a_trsize) !=
		    entry->header.a_trsize)
			errx(1, "%s: read_reloc(text): premature EOF",
			     get_file_name(entry));

		md_swapin_reloc(reloc, entry->header.a_trsize / sizeof(*reloc));
		entry->textrel = reloc;
		entry->ntextrel = entry->header.a_trsize / sizeof(*reloc);

	}

	if (!entry->datarel) {

		reloc = (struct relocation_info *)
			xmalloc(entry->header.a_drsize);

		pos = text_offset(entry) + entry->header.a_text +
		      entry->header.a_data + entry->header.a_trsize;

		if (lseek(fd, pos, L_SET) != pos)
			err(1, "%s: read_reloc(data): lseek",
				get_file_name(entry));

		if (read(fd, reloc, entry->header.a_drsize) !=
		    entry->header.a_drsize)
			errx(1, "%s: read_reloc(data): premature EOF",
			     get_file_name(entry));

		md_swapin_reloc(reloc, entry->header.a_drsize / sizeof(*reloc));
		entry->datarel = reloc;
		entry->ndatarel = entry->header.a_drsize / sizeof(*reloc);

	}
}

/*---------------------------------------------------------------------------*/

/*
 * Read in the symbols of all input files.
 */
static void
load_symbols()
{
	register int i;

	if (trace_files)
		fprintf(stderr, "Loading symbols:\n\n");

	for (i = 0; i < number_of_files; i++)
		read_file_symbols(&file_table[i]);

	if (trace_files)
		fprintf(stderr, "\n");
}

/*
 * If ENTRY is a rel file, read its symbol and string sections into core. If
 * it is a library, search it and load the appropriate members (which means
 * calling this function recursively on those members).
 */

void
read_file_symbols(entry)
	register struct file_entry *entry;
{
	register int	fd;
	register int	len;
	struct exec	hdr;

	fd = file_open(entry);

	len = read(fd, &hdr, sizeof hdr);
	if (len != sizeof hdr)
		errx(1, "%s: read_file_symbols(header): premature EOF",
			get_file_name(entry));

	md_swapin_exec_hdr(&hdr);

	if (!N_BADMAG (hdr)) {
		if (N_IS_DYNAMIC(hdr) && !(entry->flags & E_JUST_SYMS)) {
			if (relocatable_output) {
				errx(1,
			"%s: -r and shared objects currently not supported",
					get_file_name(entry));
				return;
			}
#if notyet /* Compatibility */
			if (!(N_GETFLAG(hdr) & EX_PIC))
				warnx("%s: EX_PIC not set",
				      get_file_name(entry));
#endif
			entry->flags |= E_DYNAMIC;
			if (entry->superfile || rrs_add_shobj(entry))
				read_shared_object(fd, entry);
			else
				entry->flags |= E_SCRAPPED;
		} else {
			if (N_GETFLAG(hdr) & EX_PIC)
				pic_code_seen = 1;
			read_entry_symbols(fd, entry);
			entry->strings = (char *)alloca(entry->string_size);
			read_entry_strings(fd, entry);
			read_entry_relocation(fd, entry);
			enter_file_symbols(entry);
			entry->strings = 0;
		}
	} else {
		char armag[SARMAG];

		lseek (fd, 0, 0);
		if (SARMAG != read(fd, armag, SARMAG) ||
		    strncmp (armag, ARMAG, SARMAG))
			errx(1,
			     "%s: malformed input file (not rel or archive)",
			     get_file_name(entry));
		entry->flags |= E_IS_LIBRARY;
		search_library(fd, entry);
	}

	file_close();
}


/*
 * Enter the external symbol defs and refs of ENTRY in the hash table.
 */

void
enter_file_symbols(entry)
     struct file_entry *entry;
{
	struct localsymbol	*lsp, *lspend;

	if (trace_files)
		prline_file_name(entry, stderr);

	lspend = entry->symbols + entry->nsymbols;

	for (lsp = entry->symbols; lsp < lspend; lsp++) {
		register struct nlist *p = &lsp->nzlist.nlist;

		if (p->n_type == (N_SETV | N_EXT))
			continue;

		/*
		 * Turn magically prefixed symbols into set symbols of
		 * a corresponding type.
		 */
		if (set_element_prefixes &&
		    set_element_prefixed_p(entry->strings+lsp->nzlist.nz_strx))
			lsp->nzlist.nz_type += (N_SETA - N_ABS);

		if (SET_ELEMENT_P(p->n_type)) {
			set_symbol_count++;
			if (!relocatable_output)
				enter_global_ref(lsp,
					p->n_un.n_strx + entry->strings, entry);
		} else if (p->n_type == N_WARNING) {
			char *msg = p->n_un.n_strx + entry->strings;

			/* Grab the next entry.  */
			lsp++;
			p = &lsp->nzlist.nlist;
			if (p->n_type != (N_UNDF | N_EXT)) {
				warnx(
		"%s: Warning symbol without external reference following.",
					get_file_name(entry));
				make_executable = 0;
				lsp--;		/* Process normally.  */
			} else {
				symbol *sp;
				char *name = p->n_un.n_strx + entry->strings;
				/* Deal with the warning symbol.  */
				lsp->flags |= LS_WARNING;
				enter_global_ref(lsp, name, entry);
				sp = getsym(name);
				if (sp->warning == NULL) {
					sp->warning = (char *)
						xmalloc(strlen(msg)+1);
					strcpy(sp->warning, msg);
					warn_sym_count++;
				} else if (strcmp(sp->warning, msg))
					warnx(
			"%s: multiple definitions for warning symbol `%s'",
					get_file_name(entry), demangle(sp->name));
			}
		} else if (p->n_type & N_EXT) {
			enter_global_ref(lsp,
				p->n_un.n_strx + entry->strings, entry);
		} else if (p->n_un.n_strx &&
				(p->n_un.n_strx + entry->strings)[0] == LPREFIX)
			lsp->flags |= LS_L_SYMBOL;
	}

}

/*
 * Enter one global symbol in the hash table. LSP points to the `struct
 * localsymbol' from the file that describes the global symbol.  NAME is the
 * symbol's name. ENTRY is the file entry for the file the symbol comes from.
 *
 * LSP is put on the chain of all such structs that refer to the same symbol.
 * This chain starts in the `refs' for symbols from relocatable objects. A
 * backpointer to the global symbol is kept in LSP.
 *
 * Symbols from shared objects are linked through `soref'. For such symbols
 * that's all we do at this stage, with the exception of the case where the
 * symbol is a common. The `referenced' bit is only set for references from
 * relocatable objects.
 *
 */

static void
enter_global_ref(lsp, name, entry)
     struct localsymbol *lsp;
     char *name;
     struct file_entry *entry;
{
	register struct nzlist *nzp = &lsp->nzlist;
	register symbol *sp = getsym(name);
	register int type = nzp->nz_type;
	int oldref = (sp->flags & GS_REFERENCED);
	int olddef = sp->defined;
	int com = sp->defined && sp->common_size;

	if (type == (N_INDR | N_EXT) && !olddef) {
		sp->alias = getsym(entry->strings + (lsp + 1)->nzlist.nz_strx);
		if (sp == sp->alias) {
			warnx("%s: %s is alias for itself",
				get_file_name(entry), name);
			/* Rewrite symbol as global text symbol with value 0 */
			lsp->nzlist.nz_type = N_TEXT|N_EXT;
			lsp->nzlist.nz_value = 0;
			make_executable = 0;
		}
#if 0
		if (sp->flags & GS_REFERENCED)
			sp->alias->flags |= GS_REFERENCED;
#endif
	}

	if (entry->flags & E_DYNAMIC) {
		lsp->next = sp->sorefs;
		sp->sorefs = lsp;
		lsp->symbol = sp;

		/*
		 * Handle commons from shared objects:
		 *   1) If symbol hitherto undefined, turn it into a common.
		 *   2) If symbol already common, update size if necessary.
		 */
/*XXX - look at case where commons are only in shared objects */
		if (type == (N_UNDF | N_EXT) && nzp->nz_value) {
			if (!olddef) {
				if (oldref)
					undefined_global_sym_count--;
				common_defined_global_count++;
				sp->common_size = nzp->nz_value;
				sp->defined = N_UNDF | N_EXT;
			} else if (com && sp->common_size < nzp->nz_value) {
				sp->common_size = nzp->nz_value;
			}
		} else if (type != (N_UNDF | N_EXT) && !oldref) {
			/*
			 * This is an ex common...
			 */
			if (com)
				common_defined_global_count--;
			sp->common_size = 0;
			sp->defined = 0;
		}

		/*
		 * Handle size information in shared objects.
		 */
		if (nzp->nz_size > sp->size)
			sp->size = nzp->nz_size;

		if ((lsp->flags & LS_WARNING) && (sp->flags & GS_REFERENCED))
			/*
			 * Prevent warning symbols from getting
			 * gratuitously referenced.
			 */
			list_warning_symbols = 1;
		return;
	}

	lsp->next = sp->refs;
	sp->refs = lsp;
	lsp->symbol = sp;

	if (lsp->flags & LS_WARNING) {
		/*
		 * Prevent warning symbols from getting
		 * gratuitously referenced.
		 */
		if (sp->flags & GS_REFERENCED)
			list_warning_symbols = 1;
		return;
	}

	if (sp->warning)
		list_warning_symbols = 1;

	sp->flags |= GS_REFERENCED;

	if (sp == dynamic_symbol || sp == got_symbol) {
		if (type != (N_UNDF | N_EXT) && !(entry->flags & E_JUST_SYMS))
			errx(1,"Linker reserved symbol %s defined as type %x ",
				name, type);
		return;
	}

	if (olddef && N_ISWEAK(&nzp->nlist) && !(sp->flags & GS_WEAK)) {
#ifdef DEBUG
		printf("%s: not overridden by weak symbol from %s\n",
			demangle(sp->name), get_file_name(entry));
#endif
		return;
	}

	if (type == (N_SIZE | N_EXT)) {

		if (relocatable_output && nzp->nz_value != 0 && sp->size == 0)
			size_sym_count++;
		if (sp->size < nzp->nz_value)
			sp->size = nzp->nz_value;

	} else if (type != (N_UNDF | N_EXT) || nzp->nz_value) {

		/*
		 * Set `->defined' here, so commons and undefined globals
		 * can be counted correctly.
		 */
		if (!sp->defined || sp->defined == (N_UNDF | N_EXT)) {
			sp->defined = type;
		}

		if ((sp->flags & GS_WEAK) && !N_ISWEAK(&nzp->nlist)) {
			/*
			 * Upgrade an existing weak definition.
			 * We fake it by pretending the symbol is undefined;
			 * must undo any common fiddling, however.
			 */
			if (!oldref)
				errx(1, "internal error: enter_glob_ref: "
					"weak symbol not referenced");
			if (!olddef && !com)
				undefined_weak_sym_count--;
			undefined_global_sym_count++;
			sp->defined = type;
			sp->flags &= ~GS_WEAK;
			olddef = 0;
			if (com)
				common_defined_global_count--;
			com = 0;
			sp->common_size = 0;
		}
		if (oldref && !olddef) {
			/*
			 * It used to be undefined and we're defining it.
			 */
			undefined_global_sym_count--;
			if (sp->flags & GS_WEAK)
				/* Used to be a weak reference */
				undefined_weak_sym_count--;
			if (undefined_global_sym_count < 0 ||
			    undefined_weak_sym_count < 0)
				errx(1, "internal error: enter_glob_ref: "
					"undefined_global_sym_count = %d, "
					"undefined_weak_sym_count = %d",
					undefined_global_sym_count,
					undefined_weak_sym_count);

		}

		if (N_ISWEAK(&nzp->nlist))
			/* The definition is weak */
			sp->flags |= GS_WEAK;

		if (!olddef && type == (N_UNDF | N_EXT) && nzp->nz_value) {
			/*
			 * First definition and it's common.
			 */
			common_defined_global_count++;
			sp->common_size = nzp->nz_value;
		} else if (com && type != (N_UNDF | N_EXT)) {
			/*
			 * It used to be common and we're defining
			 * it as something else.
			 */
			common_defined_global_count--;
			sp->common_size = 0;
		} else if (com && type == (N_UNDF | N_EXT) &&
			   sp->common_size < nzp->nz_value)
			/*
			 * It used to be common and this is a new common entry
			 * to which we need to pay attention.
			 */
			sp->common_size = nzp->nz_value;

		if (SET_ELEMENT_P(type) && (!olddef || com))
			set_vector_count++;

	} else if (!oldref && !com) {
		/*
		 * An unreferenced symbol can already be defined
		 * as common by shared objects.
		 */
		undefined_global_sym_count++;
		if (N_ISWEAK(&nzp->nlist)) {
			/* The reference is weak */
			sp->flags |= GS_WEAK;
			undefined_weak_sym_count++;
		}
	}

	if (sp == end_symbol && (entry->flags & E_JUST_SYMS) &&
	    !T_flag_specified)
		text_start = nzp->nz_value;

	if (sp->flags & GS_TRACE) {
		register char *reftype;
		switch (type & N_TYPE) {
		case N_UNDF:
			reftype = nzp->nz_value
				  ? "defined as common" : "referenced";
			break;

		case N_ABS:
			reftype = "defined as absolute";
			break;

		case N_TEXT:
			reftype = "defined in text section";
			break;

		case N_DATA:
			reftype = "defined in data section";
			break;

		case N_BSS:
			reftype = "defined in BSS section";
			break;

		case N_INDR:
			reftype = "alias";
			break;

		case N_SIZE:
			reftype = "size spec";
			break;

		default:
			reftype = "I don't know this type";
			break;
		}

		fprintf(stderr, "symbol %s %s%s in ", demangle(sp->name),
			(N_ISWEAK(&nzp->nlist))?"weakly ":"", reftype);
		print_file_name (entry, stderr);
		fprintf(stderr, "\n");
	}
}

/*
 * This returns 0 if the given file entry's symbol table does *not* contain
 * the nlist point entry, and it returns the files entry pointer (cast to
 * unsigned long) if it does.
 */

unsigned long
contains_symbol(entry, np)
     struct file_entry *entry;
     register struct nlist *np;
{
	if (np >= &entry->symbols->nzlist.nlist &&
		np < &(entry->symbols + entry->nsymbols)->nzlist.nlist)
		return (unsigned long) entry;
	return 0;
}


/*
 * Having entered all the global symbols and found the sizes of sections of
 * all files to be linked, make all appropriate deductions from this data.
 *
 * We propagate global symbol values from definitions to references. We compute
 * the layout of the output file and where each input file's contents fit
 * into it.
 *
 * This is now done in several stages.
 *
 * 1) All global symbols are examined for definitions in relocatable (.o)
 *    files. The symbols' type is set according to the definition found,
 *    but its value can not yet be determined. In stead, we keep a pointer
 *    to the file entry's localsymbol that bequeathed the global symbol with
 *    its definition. Also, multiple (incompatible) definitions are checked
 *    for in this pass. If no definition comes forward, the set of local
 *    symbols originating from shared objects is searched for a definition.
 *
 * 2) Then the relocation information of each relocatable file is examined
 *    for possible contributions to the RRS section.
 *
 * 3) When this is done, the sizes and start addresses are set of all segments
 *    that will appear in the output file (including the RRS segment).
 *
 * 4) Finally, all symbols are relocated according according to the start
 *    of the entry they are part of. Then global symbols are assigned their
 *    final values. Also, space for commons and imported data are allocated
 *    during this pass, if the link mode in effect so demands.
 *
 */

static void
digest_symbols()
{

	if (trace_files)
		fprintf(stderr, "Digesting symbol information:\n\n");

	if (!relocatable_output) {
		/*
		 * The set sector size is the number of set elements + a word
		 * for each symbol for the length word at the beginning of
		 * the vector, plus a word for each symbol for a zero at the
		 * end of the vector (for incremental linking).
		 */
		set_sect_size = (set_symbol_count + 2 * set_vector_count) *
							sizeof (unsigned long);
		set_vectors = (long *)xmalloc (set_sect_size);
		setv_fill_count = 0;
	}

	/* Pass 1: check and define symbols */
	defined_global_sym_count = 0;
	digest_pass1();

	each_full_file(consider_relocation, (void *)0);	/* Text */
	each_full_file(consider_relocation, (void *)1); /* Data */

	each_file(consider_local_symbols, (void *)0);

	/*
	 * Compute total size of sections.
	 * RRS data is the first output data section, RRS text is the last
	 * text section. Thus, DATA_START is calculated from RRS_DATA_START
	 * and RRS_DATA_SIZE, while RRS_TEXT_START is derived from TEXT_START
	 * and TEXT_SIZE.
	 */
	consider_rrs_section_lengths();
	each_full_file(consider_file_section_lengths, 0);
	rrs_text_start = text_start + text_size;
	text_size += rrs_text_size;
	data_size += rrs_data_size;

	/*
	 * If necessary, pad text section to full page in the file. Include
	 * the padding in the text segment size.
	 */

	if (page_align_segments || page_align_data) {
		int  text_end = text_size + N_TXTOFF(outheader);
		text_pad = PALIGN(text_end, page_size) - text_end;
		text_size += text_pad;
	}
	outheader.a_text = text_size;

	/*
	 * Make the data segment address start in memory on a suitable
	 * boundary.
	 */

	if (!Tdata_flag_specified)
		rrs_data_start = text_start +
			DATA_START(outheader) - TEXT_START(outheader);

	data_start = rrs_data_start + rrs_data_size;
	if (!relocatable_output) {
		set_sect_start = rrs_data_start + data_size;
		data_size += MALIGN(set_sect_size);
	}
	bss_start = rrs_data_start + data_size;

#ifdef DEBUG
printf("textstart = %#x, textsize = %#x, rrs_text_start = %#x, rrs_text_size %#x\n",
	text_start, text_size, rrs_text_start, rrs_text_size);
printf("datastart = %#x, datasize = %#x, rrs_data_start %#x, rrs_data_size %#x\n",
	data_start, data_size, rrs_data_start, rrs_data_size);
printf("bssstart = %#x, bsssize = %#x\n",
	bss_start, bss_size);
printf("set_sect_start = %#x, set_sect_size = %#x\n",
	set_sect_start, set_sect_size);
#endif

	/* Compute start addresses of each file's sections and symbols.  */

	each_full_file(relocate_file_addresses, 0);
	relocate_rrs_addresses();

	/* Pass 2: assign values to symbols */
	digest_pass2();

	if (end_symbol) {	/* These are null if -r.  */
		etext_symbol->value = text_start + text_size - text_pad;
		edata_symbol->value = rrs_data_start + data_size;
		end_symbol->value = rrs_data_start + data_size + bss_size;
	}
	/*
	 * Figure the data_pad now, so that it overlaps with the bss
	 * addresses.
	 */

	if (specified_data_size && specified_data_size > data_size)
		data_pad = specified_data_size - data_size;

	if (page_align_segments)
		data_pad = PALIGN(data_pad + data_size, page_size) - data_size;

	bss_size -= data_pad;
	if (bss_size < 0)
		bss_size = 0;

	data_size += data_pad;

	/*
	 * Calculate total number of symbols that will go into
	 * the output symbol table (barring DISCARD_* settings).
	 */
	global_sym_count = defined_global_sym_count +
			   undefined_global_sym_count;

	if (dynamic_symbol->flags & GS_REFERENCED)
		global_sym_count++;

	if (got_symbol->flags & GS_REFERENCED)
		global_sym_count++;

	if (relocatable_output || building_shared_object) {
		/* For each alias we write out two struct nlists */
		global_sym_count += global_alias_count;
		/* Propagate warning symbols; costs two extra struct nlists */
		global_sym_count += 2 * warn_sym_count;
	}

	if (relocatable_output)
		/* We write out the original N_SIZE symbols */
		global_sym_count += size_sym_count;

#ifdef DEBUG
printf(
"global symbols %d "
"(defined %d, undefined %d, weak %d, aliases %d, warnings 2 * %d, "
"size symbols %d)\ncommons %d, locals: %d, debug symbols: %d, set_symbols %d\n",
	global_sym_count,
	defined_global_sym_count, undefined_global_sym_count,
	undefined_weak_sym_count,
	global_alias_count, warn_sym_count, size_sym_count,
	common_defined_global_count, local_sym_count,
	debugger_sym_count, set_symbol_count);
#endif
}

/*
 * Determine the definition of each global symbol.
 */
static void
digest_pass1()
{

	/*
	 * For each symbol, verify that it is defined globally at most
	 * once within relocatable files (except when building a shared lib).
	 * and set the `defined' field if there is a definition.
	 *
	 * Then check the shared object symbol chain for any remaining
	 * undefined symbols. Set the `so_defined' field for any
	 * definition find this way.
	 */
	FOR_EACH_SYMBOL(i, sp) {
		symbol *spsave;
		struct localsymbol *lsp;
		int             defs = 0;

		if (!(sp->flags & GS_REFERENCED)) {
#if 0
			/* Check for undefined symbols in shared objects */
			int type;
			for (lsp = sp->sorefs; lsp; lsp = lsp->next) {
				type = lsp->nzlist.nlist.n_type;
				if ((type & N_EXT) && type != (N_UNDF | N_EXT))
					break;
			}
			if ((type & N_EXT) && type == (N_UNDF | N_EXT))
				undefined_shobj_sym_count++;
#endif

			/* Superfluous symbol from shared object */
			continue;
		}
		if (sp->so_defined)
			/* Already examined; must have been an alias */
			continue;

		if (sp == got_symbol || sp == dynamic_symbol)
			continue;

		for (lsp = sp->refs; lsp; lsp = lsp->next) {
			register struct nlist *p = &lsp->nzlist.nlist;
			register int    type = p->n_type;

			if (SET_ELEMENT_P(type)) {
				if (relocatable_output)
					errx(1,
				"internal error: global ref to set el %s with -r",
						demangle(sp->name));
				if (!defs++) {
					sp->defined = N_SETV | N_EXT;
					sp->value =
						setv_fill_count++ * sizeof(long);
				} else if ((sp->defined & N_TYPE) != N_SETV) {
					sp->mult_defs = 1;
					multiple_def_count++;
				}
				/* Keep count and remember symbol */
				sp->setv_count++;
				set_vectors[setv_fill_count++] = (long)p;
				if (building_shared_object) {
					struct relocation_info reloc;

					/*
					 * Make sure to relocate the contents
					 * of this set vector.
					 */
					bzero(&reloc, sizeof(reloc));
					RELOC_INIT_SEGMENT_RELOC(&reloc);
					RELOC_ADDRESS(&reloc) =
						setv_fill_count * sizeof(long);
					alloc_rrs_segment_reloc(NULL, &reloc);
				}

			} else if ((type & N_EXT) && type != (N_UNDF | N_EXT)
						&& (type & N_TYPE) != N_FN
						&& (type & N_TYPE) != N_SIZE) {
				/* non-common definition */
				if (!N_ISWEAK(p))
					++defs;
				if (defs > 1) {
					sp->mult_defs = 1;
					multiple_def_count++;
				} else if (!N_ISWEAK(p) ||
					   (!sp->def_lsp && !sp->common_size)) {
					sp->def_lsp = lsp;
					lsp->entry->flags |= E_SYMBOLS_USED;
					sp->defined = type;
					sp->aux = N_AUX(p);
				}
			}
		}

		/*
		 * If this symbol has acquired final definition, we're done.
		 * Commons must be allowed to bind to shared object data
		 * definitions.
		 */
		if (sp->defined &&
		    (sp->common_size == 0 ||
		     relocatable_output || building_shared_object)) {
			if ((sp->defined & N_TYPE) == N_SETV)
				/* Allocate zero entry in set vector */
				setv_fill_count++;
			/*
			 * At this stage, we do not know whether an alias
			 * is going to be defined for real here, or whether
			 * it refers to a shared object symbol. The decision
			 * is deferred until digest_pass2().
			 */
			if (!sp->alias)
				defined_global_sym_count++;
			continue;
		}

		if (relocatable_output)
			/* We're done */
			continue;

		/*
		 * Still undefined, search the shared object symbols for a
		 * definition. This symbol must go into the RRS.
		 */
		if (building_shared_object) {
			/* Just punt for now */
			undefined_global_sym_count--;
			if (undefined_global_sym_count < 0)
				errx(1,
	"internal error: digest_pass1,1: %s: undefined_global_sym_count = %d",
					demangle(sp->name), undefined_global_sym_count);
			continue;
		}

		spsave=sp; /*XXX*/
	again:
		for (lsp = sp->sorefs; lsp; lsp = lsp->next) {
			register struct nlist *p = &lsp->nzlist.nlist;
			register int    type = p->n_type;

			if ((type & N_EXT) && type != (N_UNDF | N_EXT) &&
			    (type & N_TYPE) != N_FN) {
				/* non-common definition */
				if (sp->common_size) {
					/*
					 * This common has an so defn; switch
					 * to it iff defn is: data, first-class
					 * and not weak.
					 */
					if (N_AUX(p) != AUX_OBJECT ||
					    N_ISWEAK(p) ||
					    (lsp->entry->flags & E_SECONDCLASS))
						continue;

					/*
					 * Change common to so ref. First,
					 * downgrade common to undefined.
					 */
					sp->common_size = 0;
					sp->defined = 0;
					common_defined_global_count--;
					undefined_global_sym_count++;
				}
				sp->def_lsp = lsp;
				sp->so_defined = type;
				sp->aux = N_AUX(p);
				if (lsp->entry->flags & E_SECONDCLASS)
					/* Keep looking for something better */
					continue;
				if (N_ISWEAK(p))
					/* Keep looking for something better */
					continue;
				break;
			}
		}
		if (sp->def_lsp) {
#ifdef DEBUG
printf("pass1: SO definition for %s, type %x in %s at %#x\n",
	demangle(sp->name), sp->so_defined, get_file_name(sp->def_lsp->entry),
	sp->def_lsp->nzlist.nz_value);
#endif
			sp->def_lsp->entry->flags |= E_SYMBOLS_USED;
			if (sp->flags & GS_REFERENCED) {
				undefined_global_sym_count--;
			} else
				sp->flags |= GS_REFERENCED;
			if (undefined_global_sym_count < 0)
				errx(1, "internal error: digest_pass1,2: "
					"%s: undefined_global_sym_count = %d",
					demangle(sp->name), undefined_global_sym_count);
			if (sp->alias &&
			    !(sp->alias->flags & GS_REFERENCED)) {
				sp = sp->alias;
				goto again;
			}
		} else if (sp->defined) {
			if (sp->common_size == 0)
				errx(1, "internal error: digest_pass1,3: "
					"%s: not a common: %x",
					demangle(sp->name), sp->defined);
			/*
			 * Common not bound to shared object data; treat
			 * it now like other defined symbols were above.
			 */
			if (!sp->alias)
				defined_global_sym_count++;
		}
		sp=spsave; /*XXX*/
	} END_EACH_SYMBOL;

	if (setv_fill_count != set_sect_size/sizeof(long))
		errx(1, "internal error: allocated set symbol space (%d) "
			"doesn't match actual (%d)",
			set_sect_size/sizeof(long), setv_fill_count);
}


/*
 * Scan relocation info in ENTRY for contributions to the RRS section
 * of the output file.
 */
static void
consider_relocation(entry, dataseg)
	struct file_entry	*entry;
	int			dataseg;
{
	struct relocation_info	*reloc, *end;
	struct localsymbol	*lsp;
	symbol			*sp;

	if (dataseg == 0) {
		/* Text relocations */
		reloc = entry->textrel;
		end = entry->textrel + entry->ntextrel;
	} else {
		/* Data relocations */
		reloc = entry->datarel;
		end = entry->datarel + entry->ndatarel;
	}

	for (; reloc < end; reloc++) {

		if (relocatable_output) {
			lsp = &entry->symbols[reloc->r_symbolnum];
			if (RELOC_BASEREL_P(reloc)) {
				pic_code_seen = 1; /* Compatibility */
				if (!RELOC_EXTERN_P(reloc))
					lsp->flags |= LS_RENAME;
			}
			continue;
		}

		/*
		 * First, do the PIC specific relocs.
		 * r_relative and r_copy should not occur at this point
		 * (we do output them). The others break down to these
		 * combinations:
		 *
		 * jmptab:	extern:		needs jmp slot
		 *		!extern:	"intersegment" jump/call,
		 *				should get resolved in output
		 *
		 * baserel:	extern:		need GOT entry
		 *		!extern:	may need GOT entry,
		 *				machine dependent
		 *
		 * baserel's always refer to symbol through `r_symbolnum'
		 * whether extern or not. Internal baserels refer to statics
		 * that must be accessed either *through* the GOT table like
		 * global data, or by means of an offset from the GOT table.
		 * The macro RELOC_STATICS_THROUGH_GOT_P() determines which
		 * applies, since this is a machine (compiler?) dependent
		 * addressing mode.
		 */

		if (RELOC_JMPTAB_P(reloc)) {

			if (!RELOC_EXTERN_P(reloc))
				continue;

			lsp = &entry->symbols[reloc->r_symbolnum];
			sp = lsp->symbol;
			if (sp->alias)
				sp = sp->alias;
			if (sp->flags & GS_TRACE) {
				fprintf(stderr, "symbol %s has jmpslot in %s\n",
						demangle(sp->name), get_file_name(entry));
			}
			alloc_rrs_jmpslot(entry, sp);

		} else if (RELOC_BASEREL_P(reloc)) {

			lsp = &entry->symbols[reloc->r_symbolnum];
			alloc_rrs_gotslot(entry, reloc, lsp);
			if (pic_type != PIC_TYPE_NONE &&
			    RELOC_PIC_TYPE(reloc) != pic_type)
				errx(1, "%s: illegal reloc type mix",
					get_file_name(entry));
			pic_type = RELOC_PIC_TYPE(reloc);

		} else if (RELOC_EXTERN_P(reloc)) {

			/*
			 * Non-PIC relocations.
			 * If the definition comes from a shared object
			 * we need a relocation entry in RRS.
			 *
			 * If the .so definition is N_TEXT a jmpslot is
			 * allocated.
			 *
			 * If it is N_DATA we allocate an address in BSS (?)
			 * and arrange for the data to be copied at run-time.
			 * The symbol is temporarily marked with N_SIZE in
			 * the `defined' field, so we know what to do in
			 * pass2() and during actual relocation. We convert
			 * the type back to something real again when writing
			 * out the symbols.
			 *
			 */
			lsp = &entry->symbols[reloc->r_symbolnum];
			sp = lsp->symbol;
			if (sp == NULL)
				errx(1, "%s: bogus relocation record",
					get_file_name(entry));

			if (sp->alias)
				sp = sp->alias;

			/*
			 * Skip refs to _GLOBAL_OFFSET_TABLE_ and __DYNAMIC
			 */
			if (sp == got_symbol) {
				if (!CHECK_GOT_RELOC(reloc))
					errx(1,
				"%s: Unexpected relocation type for GOT symbol",
					get_file_name(entry));
				continue;
			}

			/*
			 * This symbol gives rise to a RRS entry
			 */

			if (building_shared_object) {
				if (sp->flags & GS_TRACE) {
					fprintf(stderr,
					    "symbol %s RRS entry in %s\n",
					    demangle(sp->name), get_file_name(entry));
				}
				alloc_rrs_reloc(entry, sp);
				continue;
			}

			if (force_alias_definition && sp->so_defined &&
			    sp->aux == AUX_FUNC) {

				/* Call to shared library procedure */
				alloc_rrs_jmpslot(entry, sp);

			} else if (sp->size && sp->so_defined &&
				   sp->aux == AUX_OBJECT) {

				/* Reference to shared library data */
				alloc_rrs_cpy_reloc(entry, sp);
				sp->defined = N_SIZE;

			} else if (!sp->defined && sp->common_size == 0 &&
				   sp->so_defined)
				alloc_rrs_reloc(entry, sp);

		} else {
			/*
			 * Segment relocation.
			 * Prepare an RRS relocation as these are load
			 * address dependent.
			 */
			if (building_shared_object && !RELOC_PCREL_P(reloc)) {
				alloc_rrs_segment_reloc(entry, reloc);
			}
		}
	}
}

/*
 * Determine the disposition of each local symbol.
 */
static void
consider_local_symbols(entry)
     register struct file_entry *entry;
{
	register struct localsymbol	*lsp, *lspend;

	if (entry->flags & E_DYNAMIC)
		return;

	lspend = entry->symbols + entry->nsymbols;

	/*
	 * For each symbol determine whether it should go
	 * in the output symbol table.
	 */

	for (lsp = entry->symbols; lsp < lspend; lsp++) {
		register struct nlist *p = &lsp->nzlist.nlist;
		register int type = p->n_type;

		if (type == N_WARNING)
			continue;

		if (SET_ELEMENT_P (type)) {
			/*
			 * This occurs even if global. These types of
			 * symbols are never written globally, though
			 * they are stored globally.
			 */
			if (relocatable_output)
				lsp->flags |= LS_WRITE;

		} else if (!(type & (N_STAB | N_EXT))) {

			/*
			 * Ordinary local symbol
			 */
			if ((lsp->flags & LS_RENAME) || (
				discard_locals != DISCARD_ALL &&
					!(discard_locals == DISCARD_L &&
					(lsp->flags & LS_L_SYMBOL))) ) {

				lsp->flags |= LS_WRITE;
				local_sym_count++;
			}

		} else if (!(type & N_EXT)) {

			/*
			 * Debugger symbol
			 */
			if (strip_symbols == STRIP_NONE) {
				lsp->flags |= LS_WRITE;
				debugger_sym_count++;
			}
		}
	}

	/*
	 * Count one for the local symbol that we generate,
	 * whose name is the file's name (usually) and whose address
	 * is the start of the file's text.
	 */
	if (discard_locals != DISCARD_ALL)
		local_sym_count++;
}

/*
 * Accumulate the section sizes of input file ENTRY into the section sizes of
 * the output file.
 */
static void
consider_file_section_lengths(entry)
     register struct file_entry *entry;
{

	entry->text_start_address = text_size;
	/* If there were any vectors, we need to chop them off */
	text_size += entry->header.a_text;
	entry->data_start_address = data_size;
	data_size += entry->header.a_data;
	entry->bss_start_address = bss_size;
	bss_size += MALIGN(entry->header.a_bss);

	text_reloc_size += entry->header.a_trsize;
	data_reloc_size += entry->header.a_drsize;
}

/*
 * Determine where the sections of ENTRY go into the output file,
 * whose total section sizes are already known.
 * Also relocate the addresses of the file's local and debugger symbols.
 */
static void
relocate_file_addresses(entry)
     register struct file_entry *entry;
{
	register struct localsymbol	*lsp, *lspend;

	entry->text_start_address += text_start;
	/*
	 * Note that `data_start' and `data_size' have not yet been
	 * adjusted for `data_pad'.  If they had been, we would get the wrong
	 * results here.
	 */
	entry->data_start_address += data_start;
	entry->bss_start_address += bss_start;
#ifdef DEBUG
printf("%s: datastart: %#x, bss %#x\n", get_file_name(entry),
		entry->data_start_address, entry->bss_start_address);
#endif

	lspend = entry->symbols + entry->nsymbols;

	for (lsp = entry->symbols; lsp < lspend; lsp++) {
		register struct nlist *p = &lsp->nzlist.nlist;
		register int type = p->n_type;

		/*
		 * If this belongs to a section, update it
		 * by the section's start address
		 */

		switch (type & N_TYPE) {
		case N_TEXT:
		case N_SETT:
			p->n_value += entry->text_start_address;
			break;
		case N_DATA:
		case N_SETD:
		case N_SETV:
			/*
			 * A symbol whose value is in the data section is
			 * present in the input file as if the data section
			 * started at an address equal to the length of the
			 * file's text.
			 */
			p->n_value += entry->data_start_address -
				      entry->header.a_text;
			break;
		case N_BSS:
		case N_SETB:
			/* likewise for symbols with value in BSS.  */
			p->n_value += entry->bss_start_address -
				      (entry->header.a_text +
				      entry->header.a_data);
		break;
		}

	}

}

/*
 * Assign a value to each global symbol.
 */
static void
digest_pass2()
{
	FOR_EACH_SYMBOL(i, sp) {
		int		size;
		int             align = sizeof(int);

		if (!(sp->flags & GS_REFERENCED))
			continue;

		if (sp->alias &&
		    (relocatable_output || building_shared_object ||
		     (sp->alias->defined && !sp->alias->so_defined))) {
			/*
			 * The alias points at a defined symbol, so it
			 * must itself be counted as one too, in order to
			 * compute the correct number of symbol table entries.
			 */
			if (!sp->defined) {
				/*
				 * Change aliased symbol's definition too.
				 * These things happen if shared object commons
				 * or data is going into our symbol table.
				 */
				if (sp->so_defined != (N_INDR+N_EXT))
					warnx( "pass2: %s: alias isn't",
						demangle(sp->name));
				sp->defined = sp->so_defined;
				sp->so_defined = 0;
			}
			defined_global_sym_count++;
		}

		/*
		 * Count the aliases that will appear in the output.
		 */
		if (sp->alias && !sp->so_defined && !sp->alias->so_defined &&
		    (sp->defined || relocatable_output ||
		     !building_shared_object))
			global_alias_count++;

		if ((sp->defined & N_TYPE) == N_SETV) {
			/*
			 * Set length word at front of vector and zero byte
			 * at end. Reverse the vector itself to put it in
			 * file order.
			 */
			unsigned long	i, *p, *q;
			unsigned long	length_word_index =
						sp->value / sizeof(long);

			/* Relocate symbol value */
			sp->value += set_sect_start;

			set_vectors[length_word_index] = sp->setv_count;

			/*
			 * Relocate vector to final address.
			 */
			for (i = 0; i < sp->setv_count; i++) {
				struct nlist	*p = (struct nlist *)
					set_vectors[1+i+length_word_index];

				set_vectors[1+i+length_word_index] = p->n_value;
				if (building_shared_object) {
					struct relocation_info reloc;

					bzero(&reloc, sizeof(reloc));
					RELOC_INIT_SEGMENT_RELOC(&reloc);
					RELOC_ADDRESS(&reloc) =
						(1 + i + length_word_index) *
								sizeof(long)
						+ set_sect_start;
					RELOC_TYPE(&reloc) =
					(p->n_type - (N_SETA - N_ABS)) & N_TYPE;
					claim_rrs_segment_reloc(NULL, &reloc);
				}
			}

			/*
			 * Reverse the vector.
			 */
			p = &set_vectors[length_word_index + 1];
			q = &set_vectors[length_word_index + sp->setv_count];
			while (p < q) {
				unsigned long tmp = *p;
				*p++ = *q;
				*q-- = tmp;
			}

			/* Clear terminating entry */
			set_vectors[length_word_index + sp->setv_count + 1] = 0;
			continue;
		}

		if (sp->def_lsp) {
			if (sp->defined && (sp->defined & ~N_EXT) != N_SETV)
				sp->value = sp->def_lsp->nzlist.nz_value;
			if (sp->so_defined &&
			    (sp->def_lsp->entry->flags & E_SECONDCLASS))
				/* Flag second-hand definitions */
				undefined_global_sym_count++;
			if (sp->flags & GS_TRACE)
				printf("symbol %s assigned to location %#lx\n",
					demangle(sp->name), sp->value);
		}

		/*
		 * If not -r'ing, allocate common symbols in the BSS section.
		 */
		if (building_shared_object && !(link_mode & SYMBOLIC))
			/* No common allocation in shared objects */
			continue;

		if ((size = sp->common_size) != 0) {
			/*
			 * It's a common.
			 */
			if (sp->defined != (N_UNDF + N_EXT))
				errx(1, "%s: common isn't", demangle(sp->name));

		} else if ((size = sp->size) != 0 && sp->defined == N_SIZE) {
			/*
			 * It's data from shared object with size info.
			 */
			if (!sp->so_defined)
				errx(1, "%s: Bogus N_SIZE item", demangle(sp->name));

		} else
			/*
			 * It's neither
			 */
			continue;


		if (relocatable_output && !force_common_definition) {
			sp->defined = 0;
			undefined_global_sym_count++;
			defined_global_sym_count--;
			continue;
		}

		/*
		 * Round up to nearest sizeof (int). I don't know whether
		 * this is necessary or not (given that alignment is taken
		 * care of later), but it's traditional, so I'll leave it in.
		 * Note that if this size alignment is ever removed, ALIGN
		 * above will have to be initialized to 1 instead of sizeof
		 * (int).
		 */

		size = PALIGN(size, sizeof(int));

		while (align < MAX_ALIGNMENT && !(size & align))
			align <<= 1;

		bss_size = PALIGN(bss_size + data_size + rrs_data_start, align)
				- (data_size + rrs_data_start);

		sp->value = rrs_data_start + data_size + bss_size;
		if (sp->defined == (N_UNDF | N_EXT))
			sp->defined = N_BSS | N_EXT;
		else {
			sp->so_defined = 0;
			defined_global_sym_count++;
		}
		bss_size += size;
		if (write_map)
			printf("Allocating %s %s: %x at %lx\n",
				sp->defined==(N_BSS|N_EXT)?"common":"data",
				demangle(sp->name), size, sp->value);

	} END_EACH_SYMBOL;
}


/* -------------------------------------------------------------------*/

/* Write the output file */
void
write_output()
{
	struct stat	statbuf;
	int		filemode;
	mode_t		u_mask;

	if (lstat(output_filename, &statbuf) == 0) {
		if (S_ISREG(statbuf.st_mode))
			(void)unlink(output_filename);
	}

	u_mask = umask(0);
	(void)umask(u_mask);

	outstream = fopen(output_filename, "w");
	if (outstream == NULL)
		err(1, "fopen: %s", output_filename);

	if (atexit(cleanup))
		err(1, "atexit");

	if (fstat(fileno(outstream), &statbuf) < 0)
		err(1, "fstat: %s", output_filename);

	filemode = statbuf.st_mode;

	if (S_ISREG(statbuf.st_mode) &&
	    chmod(output_filename, filemode & ~0111) == -1)
		err(1, "chmod: %s", output_filename);

	/* Output the a.out header.  */
	write_header();

	/* Output the text and data segments, relocating as we go.  */
	write_text();
	write_data();

	/* Output the merged relocation info, if requested with `-r'.  */
	if (relocatable_output)
		write_rel();

	/* Output the symbol table (both globals and locals).  */
	write_syms();

	/* Output the RSS section */
	write_rrs();

	if (chmod (output_filename, filemode | (0111 & ~u_mask)) == -1)
		err(1, "chmod: %s", output_filename);

	fflush(outstream);
	/* Report I/O error such as disk full.  */
	if (ferror(outstream) || fclose(outstream) != 0)
		err(1, "write_output: %s", output_filename);
	outstream = 0;
	if (real_output_filename)
		if (rename(output_filename, real_output_filename))
			err(1, "rename output: %s to %s",
				output_filename, real_output_filename);
}

/* Total number of symbols to be written in the output file. */
static int	nsyms;

void
write_header()
{
	int	flags;

	if (link_mode & SHAREABLE)
		/* Output is shared object */
		flags = EX_DYNAMIC | EX_PIC;
	else if (relocatable_output && pic_code_seen)
		/* Output is relocatable and contains PIC code */
		flags = EX_PIC;
	else if (rrs_section_type == RRS_FULL)
		/* Output is a dynamic executable */
		flags = EX_DYNAMIC;
	else
		/*
		 * Output is a static executable
		 * or a non-PIC relocatable object
		 */
		flags = 0;

	if (oldmagic && (flags & EX_DPMASK))
		warnx("Cannot set flag in old magic headers\n");

	N_SET_FLAG (outheader, flags);

	outheader.a_text = text_size;
	outheader.a_data = data_size;
	outheader.a_bss = bss_size;
	outheader.a_entry = (entry_symbol ? entry_symbol->value
					  : text_start + entry_offset);

	if (strip_symbols == STRIP_ALL)
		nsyms = 0;
	else
		nsyms = global_sym_count + local_sym_count + debugger_sym_count;

	if (relocatable_output)
		nsyms += set_symbol_count;

	outheader.a_syms = nsyms * sizeof (struct nlist);

	if (relocatable_output) {
		outheader.a_trsize = text_reloc_size;
		outheader.a_drsize = data_reloc_size;
	} else {
		outheader.a_trsize = 0;
		outheader.a_drsize = 0;
	}

	md_swapout_exec_hdr(&outheader);
	mywrite(&outheader, 1, sizeof (struct exec), outstream);
	md_swapin_exec_hdr(&outheader);

	/*
	 * Output whatever padding is required in the executable file
	 * between the header and the start of the text.
	 */

#ifndef COFF_ENCAPSULATE
	padfile(N_TXTOFF(outheader) - sizeof outheader, outstream);
#endif
}

/*
 * Relocate the text segment of each input file
 * and write to the output file.
 */
void
write_text()
{

	if (trace_files)
		fprintf(stderr, "Copying and relocating text:\n\n");

	each_full_file(copy_text, 0);
	file_close();

	if (trace_files)
		fprintf(stderr, "\n");

	padfile(text_pad, outstream);
}

/*
 * Read the text segment contents of ENTRY, relocate them, and write the
 * result to the output file.  If `-r', save the text relocation for later
 * reuse.
 */
void
copy_text(entry)
	struct file_entry *entry;
{
	register char	*bytes;
	register int	fd;

	if (trace_files)
		prline_file_name(entry, stderr);

	fd = file_open(entry);

	/* Allocate space for the file's text section */
	bytes = (char *)alloca(entry->header.a_text);

	/* Deal with relocation information however is appropriate */
	if (entry->textrel == NULL)
		errx(1, "%s: no text relocation", get_file_name(entry));

	/* Read the text section into core.  */
	if (lseek(fd, text_offset(entry), L_SET) == (off_t)-1)
		err(1, "%s: copy_text: lseek", get_file_name(entry));
	if (entry->header.a_text != read(fd, bytes, entry->header.a_text))
		errx(1, "%s: copy_text: premature EOF", get_file_name(entry));

	/* Relocate the text according to the text relocation.  */
	perform_relocation (bytes, entry->header.a_text,
			    entry->textrel, entry->ntextrel, entry, 0);

	/* Write the relocated text to the output file.  */
	mywrite(bytes, entry->header.a_text, 1, outstream);
}

/*
 * Relocate the data segment of each input file
 * and write to the output file.
 */

void
write_data()
{
	off_t	pos;

	if (trace_files)
		fprintf(stderr, "Copying and relocating data:\n\n");

	pos = N_DATOFF(outheader) + data_start - rrs_data_start;
	if (fseek(outstream, pos, SEEK_SET) != 0)
		errx(1, "write_data: fseek");

	each_full_file(copy_data, 0);
	file_close();

	/*
	 * Write out the set element vectors.  See digest symbols for
	 * description of length of the set vector section.
	 */

	if (set_vector_count) {
		swap_longs(set_vectors, set_symbol_count + 2*set_vector_count);
		mywrite(set_vectors, set_symbol_count + 2*set_vector_count,
				sizeof (unsigned long), outstream);
	}

	if (trace_files)
		fprintf(stderr, "\n");

	padfile(data_pad, outstream);
}

/*
 * Read the data segment contents of ENTRY, relocate them, and write the
 * result to the output file. If `-r', save the data relocation for later
 * reuse. See comments in `copy_text'.
 */
void
copy_data(entry)
	struct file_entry *entry;
{
	register char	*bytes;
	register int	fd;

	if (trace_files)
		prline_file_name (entry, stderr);

	fd = file_open(entry);

	bytes = (char *)alloca(entry->header.a_data);

	if (entry->datarel == NULL)
		errx(1, "%s: no data relocation", get_file_name(entry));

	if (lseek(fd, text_offset(entry) + entry->header.a_text, L_SET) ==
	    (off_t)-1)
		err(1, "%s: copy_data: lseek", get_file_name(entry));
	if (entry->header.a_data != read(fd, bytes, entry->header.a_data))
		errx(1, "%s: copy_data: premature EOF", get_file_name(entry));

	perform_relocation(bytes, entry->header.a_data,
			   entry->datarel, entry->ndatarel, entry, 1);

	mywrite(bytes, entry->header.a_data, 1, outstream);
}

/*
 * Relocate ENTRY's text or data section contents. DATA is the address of the
 * contents, in core. DATA_SIZE is the length of the contents. PC_RELOCATION
 * is the difference between the address of the contents in the output file
 * and its address in the input file. RELOC is the address of the
 * relocation info, in core. NRELOC says how many there are.
 */

int	pc_relocation;

void
perform_relocation(data, data_size, reloc, nreloc, entry, dataseg)
	char			*data;
	int			data_size;
	struct relocation_info	*reloc;
	int			nreloc;
	struct file_entry	*entry;
	int			dataseg;
{

	register struct relocation_info	*r = reloc;
	struct relocation_info		*end = reloc + nreloc;

	int text_relocation = entry->text_start_address;
	int data_relocation = entry->data_start_address - entry->header.a_text;
	int bss_relocation = entry->bss_start_address -
				entry->header.a_text - entry->header.a_data;
	pc_relocation = dataseg
			? entry->data_start_address - entry->header.a_text
			: entry->text_start_address;

	for (; r < end; r++) {
		int	addr = RELOC_ADDRESS(r);
		long	addend = md_get_addend(r, data+addr);
		long	relocation;

		/*
		 * Loop over the relocations again as we did in
		 * consider_relocation(), claiming the reserved RRS
		 * relocations.
		 */

		if (addr >= data_size)
			errx(1, "%s: relocation address out of range",
				get_file_name(entry));

		if (RELOC_JMPTAB_P(r)) {

			int		   symindex = RELOC_SYMBOL(r);
			struct localsymbol *lsp = &entry->symbols[symindex];
			symbol		   *sp;

			if (symindex >= entry->nsymbols)
				errx(1, "%s: relocation symbolnum out of range",
					get_file_name(entry));

			sp = lsp->symbol;
			if (sp == NULL)
				errx(1, "%s: bogus relocation record",
					get_file_name(entry));
			if (sp->alias)
				sp = sp->alias;

			if (relocatable_output)
				relocation = addend;
			else if (!RELOC_EXTERN_P(r)) {
				relocation = addend +
					data_relocation - text_relocation;
			} else
				relocation = addend +
					claim_rrs_jmpslot(entry, r, sp, addend);

		} else if (RELOC_BASEREL_P(r)) {

			int		   symindex = RELOC_SYMBOL(r);
			struct localsymbol *lsp = &entry->symbols[symindex];

			if (symindex >= entry->nsymbols)
				errx(1, "%s: relocation symbolnum out of range",
					get_file_name(entry));

			if (relocatable_output)
				relocation = addend;
			else if (!RELOC_EXTERN_P(r))
				relocation = claim_rrs_internal_gotslot(
						entry, r, lsp, addend);
			else
				relocation = claim_rrs_gotslot(
						entry, r, lsp, addend);

		} else if (RELOC_EXTERN_P(r)) {

			int     symindex = RELOC_SYMBOL(r);
			symbol  *sp;

			if (symindex >= entry->nsymbols)
				errx(1, "%s: relocation symbolnum out of range",
					get_file_name(entry));

			sp = entry->symbols[symindex].symbol;
			if (sp == NULL)
				errx(1, "%s: bogus relocation record",
					get_file_name(entry));
			if (sp->alias)
				sp = sp->alias;

			if (relocatable_output) {
				relocation = addend;
				/*
				 * In PIC code, we keep the reference to the
				 * external symbol, even if defined now.
				 */
				if (!pic_code_seen)
					relocation += sp->value;
			} else if (sp->defined) {
				if (sp->flags & GS_TRACE) {
					fprintf(stderr,
					    "symbol %s defined as %x in %s\n",
					    demangle(sp->name), sp->defined,
					    get_file_name(entry) );
				}
				if (sp == got_symbol) {
					/* Handle _GOT_ refs */
					relocation = addend + sp->value
						     + md_got_reloc(r);
				} else if (building_shared_object) {
					/*
					 * Normal (non-PIC) relocation needs
					 * to be converted into an RRS reloc
					 * when building a shared object.
					 */
					r->r_address += dataseg?
						entry->data_start_address:
						entry->text_start_address;
					relocation = addend;
					if (claim_rrs_reloc(
						entry, r, sp, &relocation))
						continue;
				} else if (sp->defined == N_SIZE) {
					/*
					 * If size is known, arrange a
					 * run-time copy.
					 */
					if (!sp->size)
						errx(1, "Copy item isn't: %s",
							demangle(sp->name));

					relocation = addend + sp->value;
					r->r_address = sp->value;
					claim_rrs_cpy_reloc(entry, r, sp);
				} else
					/* Plain old relocation */
					relocation = addend + sp->value;
			} else {
				/*
				 * If the symbol is undefined, we relocate it
				 * in a way similar to -r case. We use an
				 * RRS relocation to resolve the symbol at
				 * run-time. The r_address field is updated
				 * to reflect the changed position in the
				 * output file.
				 */
				if (sp->flags & GS_TRACE) {
					fprintf(stderr,
					    "symbol %s claims RRS in %s%s\n",
					    demangle(sp->name), get_file_name(entry),
					    (sp->so_defined == (N_TEXT+N_EXT) &&
					    sp->flags & GS_HASJMPSLOT)?
						" (JMPSLOT)":"");
				}
				if (sp->so_defined == (N_TEXT+N_EXT) &&
				    sp->flags & GS_HASJMPSLOT) {
					/*
					 * Claim a jmpslot if one was allocated.
					 *
					 * At this point, a jmpslot can only
					 * result from a shared object reference
					 * while `force_alias' is in effect.
					 */
					relocation = addend +
						     claim_rrs_jmpslot(
							entry, r, sp, addend);
				} else {
					r->r_address += dataseg?
						entry->data_start_address:
						entry->text_start_address;
					relocation = addend;
					if ((building_shared_object ||
					     sp->so_defined) &&
					    claim_rrs_reloc(entry, r, sp,
							    &relocation))
						continue;
				}
			}

		} else {

			switch (RELOC_TYPE(r)) {
			case N_TEXT:
			case N_TEXT | N_EXT:
				relocation = addend + text_relocation;
				break;

			case N_DATA:
			case N_DATA | N_EXT:
				/*
				 * A word that points to beginning of the the
				 * data section initially contains not 0 but
				 * rather the "address" of that section in
				 * the input file, which is the length of the
				 * file's text.
				 */
				relocation = addend + data_relocation;
				break;

			case N_BSS:
			case N_BSS | N_EXT:
				/*
				 * Similarly, an input word pointing to the
				 * beginning of the bss initially contains
				 * the length of text plus data of the file.
				 */
				relocation = addend + bss_relocation;
				break;

			case N_ABS:
			case N_ABS | N_EXT:
				/*
				 * Don't know why this code would occur, but
				 * apparently it does.
				 */
				break;

			default:
				errx(1, "%s: nonexternal relocation invalid",
					get_file_name(entry));
			}

			/*
			 * When building a shared object, these segment
			 * relocations need a "load address relative"
			 * RRS fixup.
			 */
			if (building_shared_object && !RELOC_PCREL_P(r)) {
				r->r_address += dataseg?
					entry->data_start_address:
					entry->text_start_address;
				claim_rrs_segment_reloc(entry, r);
			}
		}

		if (RELOC_PCREL_P(r))
			relocation -= pc_relocation;

		md_relocate(r, relocation, data+addr, relocatable_output);

	}
}


/*
 * For relocatable_output only: write out the relocation,
 * relocating the addresses-to-be-relocated.
 */
void
write_rel()
{
	int count = 0;

	if (trace_files)
		fprintf(stderr, "Writing text relocation:\n\n");

	/*
	 * Assign each global symbol a sequence number, giving the order
	 * in which `write_syms' will write it.
	 * This is so we can store the proper symbolnum fields
	 * in relocation entries we write.
	 */

	/* BLECH - Assign number 0 to __DYNAMIC (!! Sun compatibility) */

	if (dynamic_symbol->flags & GS_REFERENCED)
		dynamic_symbol->symbolnum = count++;
	FOR_EACH_SYMBOL(i, sp) {
		if (sp == dynamic_symbol)
			continue;
		if (sp->warning)
			count += 2;
		if (!(sp->flags & GS_REFERENCED))
			continue;
		sp->symbolnum = count++;
		if (sp->size)
			count++;
		if (sp->alias)
			count++;
	} END_EACH_SYMBOL;

	if (count != global_sym_count)
		errx(1, "internal error: write_rel: count = %d", count);

	each_full_file(assign_symbolnums, &count);

	/* Write out the relocations of all files, remembered from copy_text. */
	each_full_file(coptxtrel, 0);

	if (trace_files)
		fprintf(stderr, "\nWriting data relocation:\n\n");

	each_full_file(copdatrel, 0);

	if (trace_files)
		fprintf(stderr, "\n");
}


/*
 * Assign symbol ordinal numbers to local symbols in each entry.
 */
static void
assign_symbolnums(entry, countp)
	struct file_entry	*entry;
	int			*countp;
{
	struct localsymbol	*lsp, *lspend;
	int			n = *countp;

	lspend = entry->symbols + entry->nsymbols;

	if (discard_locals != DISCARD_ALL)
		/* Count the N_FN symbol for this entry */
		n++;

	for (lsp = entry->symbols; lsp < lspend; lsp++) {
		if (lsp->flags & LS_WRITE)
			lsp->symbolnum = n++;
	}
	*countp = n;
}

static void
coptxtrel(entry)
	struct file_entry *entry;
{
	register struct relocation_info *r, *end;
	register int    reloc = entry->text_start_address;

	r = entry->textrel;
	end = r + entry->ntextrel;

	for (; r < end; r++) {
		register int  		symindex;
		struct localsymbol	*lsp;
		symbol			*sp;

		RELOC_ADDRESS(r) += reloc;

		symindex = RELOC_SYMBOL(r);
		lsp = &entry->symbols[symindex];

		if (!RELOC_EXTERN_P(r)) {
			if (!pic_code_seen)
				continue;
			if (RELOC_BASEREL_P(r))
				RELOC_SYMBOL(r) = lsp->symbolnum;
			continue;
		}

		if (symindex >= entry->nsymbols)
			errx(1, "%s: relocation symbolnum out of range",
				get_file_name(entry));

		sp = lsp->symbol;

#ifdef N_INDR
		/* Resolve indirection.  */
		if ((sp->defined & ~N_EXT) == N_INDR) {
			if (sp->alias == NULL)
				errx(1, "internal error: alias in hyperspace");
			sp = sp->alias;
		}
#endif

		/*
		 * If the symbol is now defined, change the external
		 * relocation to an internal one.
		 */

		if (sp->defined) {
			if (!pic_code_seen) {
				RELOC_EXTERN_P(r) = 0;
				RELOC_SYMBOL(r) = (sp->defined & N_TYPE);
			} else
				RELOC_SYMBOL(r) = sp->symbolnum;
		} else
			/*
			 * Global symbols come first.
			 */
			RELOC_SYMBOL(r) = sp->symbolnum;
	}
	md_swapout_reloc(entry->textrel, entry->ntextrel);
	mywrite(entry->textrel, entry->ntextrel,
		sizeof(struct relocation_info), outstream);
}

static void
copdatrel(entry)
	struct file_entry *entry;
{
	register struct relocation_info *r, *end;
	/*
	 * Relocate the address of the relocation. Old address is relative to
	 * start of the input file's data section. New address is relative to
	 * start of the output file's data section.
	 */
	register int    reloc = entry->data_start_address - text_size;

	r = entry->datarel;
	end = r + entry->ndatarel;

	for (; r < end; r++) {
		register int  symindex;
		symbol       *sp;
		int           symtype;

		RELOC_ADDRESS(r) += reloc;

		if (!RELOC_EXTERN_P(r)) {
			if (RELOC_BASEREL_P(r))
				errx(1, "%s: Unsupported relocation type",
					get_file_name(entry));
			continue;
		}

		symindex = RELOC_SYMBOL(r);
		sp = entry->symbols[symindex].symbol;

		if (symindex >= entry->header.a_syms)
			errx(1, "%s: relocation symbolnum out of range",
				get_file_name(entry));

#ifdef N_INDR
		/* Resolve indirection.  */
		if ((sp->defined & ~N_EXT) == N_INDR) {
			if (sp->alias == NULL)
				errx(1, "internal error: alias in hyperspace");
			sp = sp->alias;
		}
#endif

		symtype = sp->defined & N_TYPE;

		if (!pic_code_seen && ( symtype == N_BSS ||
					symtype == N_DATA ||
					symtype == N_TEXT ||
					symtype == N_ABS)) {
			RELOC_EXTERN_P(r) = 0;
			RELOC_SYMBOL(r) = symtype;
		} else
			/*
			 * Global symbols come first.
			 */
			RELOC_SYMBOL(r) =
				entry->symbols[symindex].symbol->symbolnum;
	}
	md_swapout_reloc(entry->datarel, entry->ndatarel);
	mywrite(entry->datarel, entry->ndatarel,
		sizeof(struct relocation_info), outstream);
}

void write_file_syms __P((struct file_entry *, int *));
void write_string_table __P((void));

/* Offsets and current lengths of symbol and string tables in output file. */

static int	symtab_offset;
static int	symtab_len;

/* Address in output file where string table starts. */
static int	strtab_offset;

/* Offset within string table
   where the strings in `strtab_vector' should be written. */
static int	strtab_len;

/* Total size of string table strings allocated so far,
   including strings in `strtab_vector'. */
static int	strtab_size;

/* Vector whose elements are strings to be added to the string table. */
static char	**strtab_vector;

/* Vector whose elements are the lengths of those strings. */
static int	*strtab_lens;

/* Index in `strtab_vector' at which the next string will be stored. */
static int	strtab_index;

/*
 * Add the string NAME to the output file string table. Record it in
 * `strtab_vector' to be output later. Return the index within the string
 * table that this string will have.
 */

static int
assign_string_table_index(name)
	char           *name;
{
	register int    index = strtab_size;
	register int    len = strlen(name) + 1;

	strtab_size += len;
	strtab_vector[strtab_index] = name;
	strtab_lens[strtab_index++] = len;

	return index;
}

/*
 * Write the contents of `strtab_vector' into the string table. This is done
 * once for each file's local&debugger symbols and once for the global
 * symbols.
 */
void
write_string_table()
{
	register int i;

	if (fseek(outstream, strtab_offset + strtab_len, SEEK_SET) != 0)
		err(1, "write_string_table: %s: fseek", output_filename);

	for (i = 0; i < strtab_index; i++) {
		mywrite(strtab_vector[i], strtab_lens[i], 1, outstream);
		strtab_len += strtab_lens[i];
	}
}

/* Write the symbol table and string table of the output file. */

void
write_syms()
{
	/* Number of symbols written so far.  */
	int		syms_written = 0;
	struct nlist	nl;

	/*
	 * Buffer big enough for all the global symbols.  One extra struct
	 * for each indirect symbol to hold the extra reference following.
	 */
	struct nlist   *buf = (struct nlist *)
				alloca(global_sym_count * sizeof(struct nlist));
	/* Pointer for storing into BUF.  */
	register struct nlist *bufp = buf;

	/* Size of string table includes the bytes that store the size.  */
	strtab_size = sizeof strtab_size;

	symtab_offset = N_SYMOFF(outheader);
	symtab_len = 0;
	strtab_offset = N_STROFF(outheader);
	strtab_len = strtab_size;

	if (strip_symbols == STRIP_ALL)
		return;

	/* First, write out the global symbols.  */

	/*
	 * Allocate two vectors that record the data to generate the string
	 * table from the global symbols written so far.  This must include
	 * extra space for the references following indirect outputs.
	 */

	strtab_vector = (char **)alloca((global_sym_count) * sizeof(char *));
	strtab_lens = (int *)alloca((global_sym_count) * sizeof(int));
	strtab_index = 0;

	/*
	 * __DYNAMIC symbol *must* be first for Sun compatibility, as Sun's
	 * ld.so reads the shared object's first symbol. This means that
	 * (Sun's) shared libraries cannot be stripped! (We only assume
	 * that __DYNAMIC is the first item in the data segment)
	 *
	 * If defined (ie. not relocatable_output), make it look
	 * like an internal symbol.
	 */
	if (dynamic_symbol->flags & GS_REFERENCED) {
		nl.n_other = 0;
		nl.n_desc = 0;
		nl.n_type = dynamic_symbol->defined;
		if (nl.n_type == N_UNDF)
			nl.n_type |= N_EXT;
		else
			nl.n_type &= ~N_EXT;
		nl.n_value = dynamic_symbol->value;
		nl.n_un.n_strx = assign_string_table_index(dynamic_symbol->name);
		*bufp++ = nl;
		syms_written++;
	}

	/* Scan the symbol hash table, bucket by bucket.  */

	FOR_EACH_SYMBOL(i, sp) {

		if (sp == dynamic_symbol)
			/* Already dealt with above */
			continue;

		/*
		 * Propagate N_WARNING symbols.
		 */
		if ((relocatable_output || building_shared_object)
		     && sp->warning) {
			nl.n_type = N_WARNING;
			nl.n_un.n_strx = assign_string_table_index(sp->warning);
			nl.n_value = 0;
			nl.n_other = 0;
			nl.n_desc = 0;
			*bufp++ = nl;
			syms_written++;

			nl.n_type = N_UNDF + N_EXT;
			nl.n_un.n_strx = assign_string_table_index(sp->name);
			nl.n_value = 0;
			nl.n_other = 0;
			nl.n_desc = 0;
			*bufp++ = nl;
			syms_written++;
		}

		if (!(sp->flags & GS_REFERENCED))
			/* Came from shared object but was not used */
			continue;

		if (sp->so_defined || (sp->alias && sp->alias->so_defined))
			/*
			 * Definition came from shared object,
			 * don't mention it here
			 */
			continue;

		if (!sp->defined && !relocatable_output) {
			/*
			 * We're building a shared object and there
			 * are still undefined symbols. Don't output
			 * these, symbol was discounted in digest_pass1()
			 * (they are in the RRS symbol table).
			 */
			if (building_shared_object)
				continue;
			if (!(sp->flags & GS_WEAK))
				warnx("symbol %s remains undefined", demangle(sp->name));
		}

		if (syms_written >= global_sym_count)
			errx(1,
			"internal error: number of symbols exceeds alloc'd %d",
				global_sym_count);

		/*
		 * Construct a `struct nlist' for the symbol.
		 */
		nl.n_other = 0;
		nl.n_desc = 0;

		if (sp->defined > 1) {
			/*
			 * defined with known type
			 */
			if (!relocatable_output && !building_shared_object &&
					sp->alias && sp->alias->defined > 1) {
				/*
				 * If the target of an indirect symbol has
				 * been defined and we are outputting an
				 * executable, resolve the indirection; it's
				 * no longer needed.
				 */
				nl.n_type = sp->alias->defined;
				nl.n_value = sp->alias->value;
				nl.n_other = N_OTHER(0, sp->alias->aux);
			} else {
				int bind = 0;

				if (sp->defined == N_SIZE)
					nl.n_type = N_DATA | N_EXT;
				else
					nl.n_type = sp->defined;
				if (nl.n_type == (N_INDR|N_EXT) &&
							sp->value != 0)
					errx(1, "%s: N_INDR has value %#x",
							demangle(sp->name), sp->value);
				nl.n_value = sp->value;
				if (sp->def_lsp)
				    bind = N_BIND(&sp->def_lsp->nzlist.nlist);
				nl.n_other = N_OTHER(bind, sp->aux);
			}

		} else if (sp->common_size) {
			/*
			 * defined as common but not allocated,
			 * happens only with -r and not -d, write out
			 * a common definition.
			 *
			 * common condition needs to be before undefined
			 * condition because unallocated commons are set
			 * undefined in digest_symbols.
			 */
			nl.n_type = N_UNDF | N_EXT;
			nl.n_value = sp->common_size;
		} else if (!sp->defined) {
			/* undefined -- legit only if -r */
			nl.n_type = N_UNDF | N_EXT;
			nl.n_value = 0;
		} else
			errx(1,
			      "internal error: %s defined in mysterious way",
			      demangle(sp->name));

		/*
		 * Allocate string table space for the symbol name.
		 */

		nl.n_un.n_strx = assign_string_table_index(sp->name);

		/* Output to the buffer and count it.  */

		*bufp++ = nl;
		syms_written++;

		/*
		 * Write second symbol of an alias pair.
		 */
		if (nl.n_type == N_INDR + N_EXT) {
			if (sp->alias == NULL)
				errx(1, "internal error: alias in hyperspace");
			nl.n_type = N_UNDF + N_EXT;
			nl.n_un.n_strx =
				assign_string_table_index(sp->alias->name);
			nl.n_value = 0;
			nl.n_other = 0;
			nl.n_desc = 0;
			*bufp++ = nl;
			syms_written++;
		}

		/*
		 * Write N_SIZE symbol for a symbol with a known size.
		 */
		if (relocatable_output && sp->size) {
			nl.n_type = N_SIZE + N_EXT;
			nl.n_un.n_strx = assign_string_table_index(sp->name);
			nl.n_value = sp->size;
			nl.n_other = 0;
			nl.n_desc = 0;
			*bufp++ = nl;
			syms_written++;
		}

#ifdef DEBUG
printf("writesym(#%d): %s, type %x\n", syms_written, demangle(sp->name), sp->defined);
#endif
	} END_EACH_SYMBOL;

	if (syms_written != strtab_index || strtab_index != global_sym_count)
		errx(1, "internal error: wrong number (%d) of global symbols "
			"written into output file, should be %d",
			syms_written, global_sym_count);

	/* Output the buffer full of `struct nlist's.  */

	if (fseek(outstream, symtab_offset + symtab_len, SEEK_SET) != 0)
		err(1, "write_syms: fseek");
	md_swapout_symbols(buf, bufp - buf);
	mywrite(buf, bufp - buf, sizeof(struct nlist), outstream);
	symtab_len += sizeof(struct nlist) * (bufp - buf);

	/* Write the strings for the global symbols.  */
	write_string_table();

	/* Write the local symbols defined by the various files.  */
	each_file(write_file_syms, (void *)&syms_written);
	file_close();

	if (syms_written != nsyms)
		errx(1, "internal error: wrong number of symbols (%d) "
			"written into output file, should be %d",
			syms_written, nsyms);

	if (symtab_offset + symtab_len != strtab_offset)
		errx(1,
		"internal error: inconsistent symbol table length: %d vs %s",
		symtab_offset + symtab_len, strtab_offset);

	if (fseek(outstream, strtab_offset, SEEK_SET) != 0)
		err(1, "write_syms: fseek");
	strtab_size = md_swap_long(strtab_size);
	mywrite(&strtab_size, sizeof(int), 1, outstream);
}


/*
 * Write the local and debugger symbols of file ENTRY. Increment
 * *SYMS_WRITTEN_ADDR for each symbol that is written.
 */

/*
 * Note that we do not combine identical names of local symbols. dbx or gdb
 * would be confused if we did that.
 */
void
write_file_syms(entry, syms_written_addr)
	struct file_entry	*entry;
	int			*syms_written_addr;
{
	struct localsymbol	*lsp, *lspend;

	/* Upper bound on number of syms to be written here.  */
	int	max_syms = entry->nsymbols + 1;

	/*
	 * Buffer to accumulate all the syms before writing them. It has one
	 * extra slot for the local symbol we generate here.
	 */
	struct nlist *buf = (struct nlist *)
			alloca(max_syms * sizeof(struct nlist));

	register struct nlist *bufp = buf;

	if (entry->flags & E_DYNAMIC)
		return;

	/*
	 * Make tables that record, for each symbol, its name and its name's
	 * length. The elements are filled in by `assign_string_table_index'.
	 */

	strtab_vector = (char **)alloca(max_syms * sizeof(char *));
	strtab_lens = (int *)alloca(max_syms * sizeof(int));
	strtab_index = 0;

	/* Generate a local symbol for the start of this file's text.  */

	if (discard_locals != DISCARD_ALL) {
		struct nlist    nl;

		nl.n_type = N_FN | N_EXT;
		nl.n_un.n_strx =
			assign_string_table_index(entry->local_sym_name);
		nl.n_value = entry->text_start_address;
		nl.n_desc = 0;
		nl.n_other = 0;
		*bufp++ = nl;
		(*syms_written_addr)++;
	}
	/* Read the file's string table.  */

	entry->strings = (char *)alloca(entry->string_size);
	read_entry_strings(file_open(entry), entry);

	lspend = entry->symbols + entry->nsymbols;

	for (lsp = entry->symbols; lsp < lspend; lsp++) {
		register struct nlist *p = &lsp->nzlist.nlist;
		char		*name;

		if (!(lsp->flags & LS_WRITE))
			continue;

		if (discard_locals == DISCARD_ALL ||
		    (discard_locals == DISCARD_L &&
		     (lsp->flags & LS_L_SYMBOL))) {
			/*
			 * The user wants to discard this symbol, but it
			 * is referenced by a relocation.  We can still
			 * save some file space by suppressing the unique
			 * renaming of the symbol.
			 */
			lsp->flags &= ~LS_RENAME;
		}

		if (p->n_un.n_strx == 0)
			name = NULL;
		else if (!(lsp->flags & LS_RENAME))
			name = p->n_un.n_strx + entry->strings;
		else {
			char *cp = p->n_un.n_strx + entry->strings;
			name = (char *)alloca(
					strlen(entry->local_sym_name) +
					strlen(cp) + 2 );
			(void)sprintf(name, "%s.%s", entry->local_sym_name, cp);
		}

		/*
		 * If this symbol has a name, allocate space for it
		 * in the output string table.
		 */

		if (name)
			p->n_un.n_strx = assign_string_table_index(name);

		/* Output this symbol to the buffer and count it.  */

		*bufp++ = *p;
		(*syms_written_addr)++;
	}

	/* All the symbols are now in BUF; write them.  */

	if (fseek(outstream, symtab_offset + symtab_len, SEEK_SET) != 0)
		err(1, "write local symbols: fseek");
	md_swapout_symbols(buf, bufp - buf);
	mywrite(buf, bufp - buf, sizeof(struct nlist), outstream);
	symtab_len += sizeof(struct nlist) * (bufp - buf);

	/*
	 * Write the string-table data for the symbols just written, using
	 * the data in vectors `strtab_vector' and `strtab_lens'.
	 */

	write_string_table();
	entry->strings = 0;	/* Since it will disappear anyway.  */
}

/*
 * Parse the string ARG using scanf format FORMAT, and return the result.
 * If it does not parse, report fatal error
 * generating the error message using format string ERROR and ARG as arg.
 */

static int
parse(arg, format, error)
	char *arg, *format, *error;
{
	int x;

	if (1 != sscanf(arg, format, &x))
		errx(1, error, arg);
	return x;
}

/*
 * Output COUNT*ELTSIZE bytes of data at BUF to the descriptor FD.
 */
void
mywrite(buf, count, eltsize, fd)
	void *buf;
	int count;
	int eltsize;
	FILE *fd;
{

	if (fwrite(buf, eltsize, count, fd) != count)
		err(1, "write");
}

static void
cleanup()
{
	struct stat	statbuf;

	if (outstream == 0)
		return;

	if (fstat(fileno(outstream), &statbuf) == 0) {
		if (S_ISREG(statbuf.st_mode))
			(void)unlink(output_filename);
	}
}

/*
 * Output PADDING zero-bytes to descriptor FD.
 * PADDING may be negative; in that case, do nothing.
 */
void
padfile(padding, fd)
	int	padding;
	FILE	*fd;
{
	register char *buf;
	if (padding <= 0)
		return;

	buf = (char *)alloca(padding);
	bzero(buf, padding);
	mywrite(buf, padding, 1, fd);
}

static void
list_files()
{
	int    error, i;

	error = 0;
	for (i = 0; i < number_of_files; i++) {
		register struct file_entry *entry = &file_table[i];
		int	fd;

		if (entry->flags & E_SEARCH_DIRS)
			fd = findlib(entry);
		else
			fd = open(entry->filename, O_RDONLY, 0);
		if (fd < 0)
			error = 1;
		else
			close(fd);

		/*
		 * Print the name even if the file doesn't exist except in
		 * the -lfoo case.  This allows `ld -f' to work as well as
		 * possible when it is used to generate dependencies before
		 * the libraries exist.
		 */
		if (fd >= 0 || !(entry->flags & E_SEARCH_DIRS))
			printf("%s\n", entry->filename);
	}
	exit(error);
}
