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
 *	$Id: ld.c,v 1.5 1993/11/09 04:18:56 paul Exp $
 */
   
/* Define how to initialize system-dependent header fields.  */

#include <sys/param.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <fcntl.h>
#include <ar.h>
#include <ranlib.h>
#include <a.out.h>
#include <stab.h>
#include <string.h>
#include <strings.h>

#include "ld.h"

int	building_shared_object;

/* 1 => write relocation into output file so can re-input it later.  */
int	relocatable_output;

/* Non zero means to create the output executable. */
/* Cleared by nonfatal errors.  */
int	make_executable;

/* Force the executable to be output, even if there are non-fatal errors */
int	force_executable;

/* 1 => assign space to common symbols even if `relocatable_output'.  */
int	force_common_definition;
 
/* 1 => assign jmp slots to text symbols in shared objects even if non-PIC */
int	force_alias_definition;

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

/* Nonzero means print names of input files as processed.  */
int	trace_files;

/* Magic number to use for the output file, set by switch.  */
int	magic;

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

int
main(argc, argv)
	char          **argv;
	int             argc;
{

	if ((progname = strrchr(argv[0], '/')) == NULL)
		progname = argv[0];
	else
		progname++;

	/* Added this to stop ld core-dumping on very large .o files.    */
#ifdef RLIMIT_STACK
	/* Get rid of any avoidable limit on stack size.  */
	{
		struct rlimit   rlim;

		/* Set the stack limit huge so that alloca does not fail. */
		getrlimit(RLIMIT_STACK, &rlim);
		rlim.rlim_cur = rlim.rlim_max;
		setrlimit(RLIMIT_STACK, &rlim);
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
	soversion = LD_VERSION_BSD;

	/* Initialize the cumulative counts of symbols.  */

	local_sym_count = 0;
	non_L_local_sym_count = 0;
	debugger_sym_count = 0;
	undefined_global_sym_count = 0;
	warning_count = 0;
	multiple_def_count = 0;
	common_defined_global_count = 0;

	/* Keep a list of symbols referenced from the command line */
	cl_refs_allocated = 10;
	cmdline_references
		= (struct glosym **) xmalloc(cl_refs_allocated
					     * sizeof(struct glosym *));
	*cmdline_references = 0;

	/* Completely decode ARGV.  */
	decode_command(argc, argv);

	building_shared_object =
		(!relocatable_output && (link_mode & SHAREABLE));

	/* Create the symbols `etext', `edata' and `end'.  */
	symtab_init(relocatable_output);

	/* Prepare for the run-time linking support. */
	init_rrs();

	/*
	 * Determine whether to count the header as part of the text size,
	 * and initialize the text size accordingly. This depends on the kind
	 * of system and on the output format selected.
	 */

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
	make_executable = do_warnings(stderr);

	/* Print a map, if requested.  */
	if (write_map)
		print_symbols(stdout);

	/* Write the output file.  */
	if (make_executable || force_executable)
		write_output();

	exit(!make_executable);
}

void            decode_option();

/*
 * Analyze a command line argument. Return 0 if the argument is a filename.
 * Return 1 if the argument is a option complete in itself. Return 2 if the
 * argument is a option which uses an argument.
 * 
 * Thus, the value is the number of consecutive arguments that are part of
 * options.
 */

int
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
	case 'o':
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

void
decode_command(argc, argv)
	char          **argv;
	int             argc;
{
	register int    i;
	register struct file_entry *p;
	char           *cp;

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
				fatal("no argument following %s\n", argv[i]);

			decode_option(argv[i], argv[i + 1]);

			if (argv[i][1] == 'l' || argv[i][1] == 'A')
				number_of_files++;

			i += code - 1;
		} else
			number_of_files++;
	}

	if (!number_of_files)
		fatal("no input files");

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
		}
		if (argv[i][1] == 'A') {
			if (p != file_table)
				fatal("-A specified before an input file other than the first");
			p->filename = string;
			p->local_sym_name = string;
			p->just_syms_flag = 1;
			p++;
		}
		if (argv[i][1] == 'l') {
			p->filename = string;
			p->local_sym_name = concat("-l", string, "");
			p->search_dirs_flag = 1;
			if (link_mode & DYNAMIC && !relocatable_output)
				p->search_dynamic_flag = 1;
			p++;
		}
		i += code - 1;
	}

	/* Now check some option settings for consistency.  */

	if ((magic != OMAGIC)
	    && (text_start - text_start_alignment) & (page_size - 1))
		fatal("-T argument not multiple of page size, with sharable output");

	/* Append the standard search directories to the user-specified ones. */
	std_search_dirs(getenv("LD_LIBRARY_PATH"));
}

void
add_cmdline_ref(sp)
	struct glosym  *sp;
{
	struct glosym **ptr;

	for (ptr = cmdline_references;
	     ptr < cmdline_references + cl_refs_allocated && *ptr;
	     ptr++);

	if (ptr >= cmdline_references + cl_refs_allocated - 1) {
		int diff = ptr - cmdline_references;

		cl_refs_allocated *= 2;
		cmdline_references = (struct glosym **)
			xrealloc(cmdline_references,
			       cl_refs_allocated * sizeof(struct glosym *));
		ptr = cmdline_references + diff;
	}
	*ptr++ = sp;
	*ptr = (struct glosym *) 0;
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

void
decode_option(swt, arg)
	register char  *swt, *arg;
{
	/* We get Bstatic from gcc on suns.  */
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
	if (swt[2] != 0)
		arg = &swt[2];

	switch (swt[1]) {
	case 'A':
		return;

	case 'D':
		specified_data_size = parse(arg, "%x", "invalid argument to -D");
		return;

	case 'd':
		if (*arg == 'c')
			force_common_definition = 1;
		else if (*arg == 'p')
			force_alias_definition = 1;
		else
			fatal("-d option takes 'c' or 'p' argument");
		return;

	case 'e':
		entry_symbol = getsym(arg);
		if (!entry_symbol->defined && !entry_symbol->referenced)
			undefined_global_sym_count++;
		entry_symbol->referenced = 1;
		add_cmdline_ref(entry_symbol);
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

#ifdef NMAGIC
	case 'n':
		magic = NMAGIC;
		return;
#endif

#ifdef QMAGIC
	case 'Q':
		magic = oldmagic = QMAGIC;
		return;
	case 'Z':
		magic = oldmagic = ZMAGIC;
		return;
#endif

	case 'o':
		output_filename = arg;
		return;

	case 'r':
		relocatable_output = 1;
		magic = OMAGIC;
		text_start = 0;
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

			if (!sp->defined && !sp->referenced)
				undefined_global_sym_count++;
			sp->referenced = 1;
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
			sp->trace = 1;
		}
		return;

	case 'z':
		magic = ZMAGIC;
		return;

	default:
		fatal("invalid command option `%s'", swt);
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
	register void   (*function) ();
	register int    arg;
{
	register int    i;

	for (i = 0; i < number_of_files; i++) {
		register struct file_entry *entry = &file_table[i];
		if (entry->library_flag) {
			register struct file_entry *subentry = entry->subfiles;
			for (; subentry; subentry = subentry->chain)
				(*function) (subentry, arg);
		} else
			(*function) (entry, arg);
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
	register unsigned long (*function) ();
	register int    arg;
{
	register int    i;
	register unsigned long return_val;

	for (i = 0; i < number_of_files; i++) {
		register struct file_entry *entry = &file_table[i];
		if (entry->library_flag) {
			register struct file_entry *subentry = entry->subfiles;
			for (; subentry; subentry = subentry->chain)
				if (return_val = (*function) (subentry, arg))
					return return_val;
		} else if (return_val = (*function) (entry, arg))
			return return_val;
	}
	return 0;
}

/* Like `each_file' but ignore files that were just for symbol definitions.  */

void
each_full_file(function, arg)
	register void   (*function) ();
	register int    arg;
{
	register int    i;

	for (i = 0; i < number_of_files; i++) {
		register struct file_entry *entry = &file_table[i];
		if (entry->just_syms_flag || entry->is_dynamic)
			continue;
		if (entry->library_flag) {
			register struct file_entry *subentry = entry->subfiles;
			for (; subentry; subentry = subentry->chain)
				(*function) (subentry, arg);
		} else
			(*function) (entry, arg);
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
file_open (entry)
     register struct file_entry *entry;
{
	register int desc;

	if (entry->superfile)
		return file_open (entry->superfile);

	if (entry == input_file)
		return input_desc;

	if (input_file) file_close ();

	if (entry->search_dirs_flag) {
		desc = findlib(entry);
	} else
		desc = open (entry->filename, O_RDONLY, 0);

	if (desc > 0) {
		input_file = entry;
		input_desc = desc;
		return desc;
	}

	perror_file (entry);
	/* NOTREACHED */
}

int
text_offset (entry)
     struct file_entry *entry;
{
	return entry->starting_offset + N_TXTOFF (entry->header);
}

/* Medium-level input routines for rel files.  */

/*
 * Read a file's header into the proper place in the file_entry. DESC is the
 * descriptor on which the file is open. ENTRY is the file's entry.
 */
void
read_header (desc, entry)
     int desc;
     register struct file_entry *entry;
{
	register int len;
	struct exec *loc = (struct exec *) &entry->header;

	if (lseek (desc, entry->starting_offset, L_SET) !=
						entry->starting_offset)
		fatal_with_file("read_header: lseek failure ", entry);

	len = read (desc, &entry->header, sizeof (struct exec));
	if (len != sizeof (struct exec))
		fatal_with_file ("failure reading header of ", entry);

	md_swapin_exec_hdr(&entry->header);

	if (N_BADMAG (*loc))
		fatal_with_file ("bad magic number in ", entry);

	entry->header_read_flag = 1;
}

/*
 * Read the symbols of file ENTRY into core. Assume it is already open, on
 * descriptor DESC. Also read the length of the string table, which follows
 * the symbol table, but don't read the contents of the string table.
 */
void
read_entry_symbols (desc, entry)
     struct file_entry *entry;
     int desc;
{
	int		str_size;
	struct nlist	*np;
	int		i;

	if (!entry->header_read_flag)
		read_header (desc, entry);

	np = (struct nlist *) alloca (entry->header.a_syms);
	entry->nsymbols = entry->header.a_syms / sizeof(struct nlist);
	entry->symbols = (struct localsymbol *)
		xmalloc(entry->nsymbols * sizeof(struct localsymbol));

	if (lseek(desc, N_SYMOFF(entry->header) + entry->starting_offset, L_SET)
			!= N_SYMOFF(entry->header) + entry->starting_offset)
		fatal_with_file ("read_symbols(h): lseek failure ", entry);

	if (entry->header.a_syms != read (desc, np, entry->header.a_syms))
		fatal_with_file ("premature end of file in symbols of ", entry);

	md_swapin_symbols(np, entry->header.a_syms / sizeof(struct nlist));

	for (i = 0; i < entry->nsymbols; i++) {
		entry->symbols[i].nzlist.nlist = *np++;
		entry->symbols[i].nzlist.nz_size = 0;
		entry->symbols[i].symbol = NULL;
		entry->symbols[i].next = NULL;
		entry->symbols[i].gotslot_offset = -1;
		entry->symbols[i].gotslot_claimed = 0;
	}

	entry->strings_offset = N_STROFF(entry->header) +
					entry->starting_offset;
	if (lseek(desc, entry->strings_offset, 0) == (off_t)-1)
		fatal_with_file ("read_symbols(s): lseek failure ", entry);
	if (sizeof str_size != read (desc, &str_size, sizeof str_size))
		fatal_with_file ("bad string table size in ", entry);

	entry->string_size = md_swap_long(str_size);
}

/*
 * Read the string table of file ENTRY into core. Assume it is already open,
 * on descriptor DESC.
 */
void
read_entry_strings (desc, entry)
     struct file_entry *entry;
     int desc;
{
	int buffer;

	if (!entry->header_read_flag || !entry->strings_offset)
		fatal("internal error: %s", "cannot read string table");

	if (lseek (desc, entry->strings_offset, L_SET) != entry->strings_offset)
		fatal_with_file ("read_strings: lseek failure ", entry);

	if (entry->string_size !=
			read (desc, entry->strings, entry->string_size))
		fatal_with_file ("premature end of file in strings of ", entry);

	return;
}

/* DEAD - Read in all of the relocation information */

void
read_relocation ()
{
	each_full_file (read_entry_relocation, 0);
}

/* Read in the relocation sections of ENTRY if necessary */

void
read_entry_relocation (desc, entry)
	int			desc;
	struct file_entry	*entry;
{
	register struct relocation_info *reloc;
	off_t	pos;

	if (!entry->textrel) {

		reloc = (struct relocation_info *)
				xmalloc(entry->header.a_trsize);

		pos = text_offset(entry) +
				entry->header.a_text + entry->header.a_data;

		if (lseek(desc, pos, L_SET) != pos)
			fatal_with_file("read_reloc(t): lseek failure ", entry);

		if (entry->header.a_trsize !=
				read(desc, reloc, entry->header.a_trsize)) {
			fatal_with_file (
				"premature eof in text relocation of ", entry);
		}
		md_swapin_reloc(reloc, entry->header.a_trsize / sizeof(*reloc));
		entry->textrel = reloc;
		entry->ntextrel = entry->header.a_trsize / sizeof(*reloc);

	}

	if (!entry->datarel) {

		reloc = (struct relocation_info *)
				xmalloc(entry->header.a_drsize);

		pos = text_offset(entry) + entry->header.a_text +
			entry->header.a_data + entry->header.a_trsize;

		if (lseek(desc, pos, L_SET) != pos)
			fatal_with_file("read_reloc(d): lseek failure ", entry);

		if (entry->header.a_drsize !=
				read (desc, reloc, entry->header.a_drsize)) {
			fatal_with_file (
				"premature eof in data relocation of ", entry);
		}
		md_swapin_reloc(reloc, entry->header.a_drsize / sizeof(*reloc));
		entry->datarel = reloc;
		entry->ndatarel = entry->header.a_drsize / sizeof(*reloc);

	}
}


/* Read in the symbols of all input files.  */

void read_file_symbols (), read_entry_symbols (), read_entry_strings ();
void enter_file_symbols (), enter_global_ref ();

void
load_symbols ()
{
	register int i;

	if (trace_files) fprintf (stderr, "Loading symbols:\n\n");

	for (i = 0; i < number_of_files; i++) {
		register struct file_entry *entry = &file_table[i];
		read_file_symbols (entry);
	}

	if (trace_files) fprintf (stderr, "\n");
}

/*
 * If ENTRY is a rel file, read its symbol and string sections into core. If
 * it is a library, search it and load the appropriate members (which means
 * calling this function recursively on those members).
 */

void
read_file_symbols (entry)
     register struct file_entry *entry;
{
	register int desc;
	register int len;
	struct exec hdr;

	desc = file_open (entry);

	len = read (desc, &hdr, sizeof hdr);
	if (len != sizeof hdr)
		fatal_with_file ("failure reading header of ", entry);

	md_swapin_exec_hdr(&hdr);

	if (!N_BADMAG (hdr)) {
		if (N_IS_DYNAMIC(hdr)) {
			if (relocatable_output) {
				fatal_with_file(
			"-r and shared objects currently not supported ",
					entry);
				return;
			}
			entry->is_dynamic = 1;
			read_shared_object(desc, entry);
			rrs_add_shobj(entry);
		} else {
			read_entry_symbols (desc, entry);
			entry->strings = (char *) alloca (entry->string_size);
			read_entry_strings (desc, entry);
			read_entry_relocation(desc, entry);
			enter_file_symbols (entry);
			entry->strings = 0;
		}
	} else {
		char armag[SARMAG];

		lseek (desc, 0, 0);
		if (SARMAG != read (desc, armag, SARMAG) ||
					strncmp (armag, ARMAG, SARMAG))
			fatal_with_file(
			"malformed input file (not rel or archive) ", entry);
		entry->library_flag = 1;
		search_library (desc, entry);
	}

	file_close ();
}

/* Enter the external symbol defs and refs of ENTRY in the hash table.  */

void
enter_file_symbols (entry)
     struct file_entry *entry;
{
	struct localsymbol	*lsp, *lspend;

	if (trace_files) prline_file_name (entry, stderr);

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
			char *name = p->n_un.n_strx + entry->strings;

			/* Grab the next entry.  */
			p++;
			if (p->n_type != (N_UNDF | N_EXT)) {
				error("Warning symbol found in %s without external reference following.",
					get_file_name(entry));
				make_executable = 0;
				p--;		/* Process normally.  */
			} else {
				symbol *sp;
				char *sname = p->n_un.n_strx + entry->strings;
				/* Deal with the warning symbol.  */
				enter_global_ref(lsp,
					p->n_un.n_strx + entry->strings, entry);
				sp = getsym (sname);
				sp->warning = (char *)xmalloc(strlen(name)+1);
				strcpy (sp->warning, name);
				warning_count++;
			}
		} else if (p->n_type & N_EXT) {
			enter_global_ref(lsp,
				p->n_un.n_strx + entry->strings, entry);
		} else if (p->n_un.n_strx && !(p->n_type & (N_STAB | N_EXT))
							&& !entry->is_dynamic) {
			if ((p->n_un.n_strx + entry->strings)[0] != LPREFIX)
				non_L_local_sym_count++;
			local_sym_count++;
		} else if (!entry->is_dynamic)
			debugger_sym_count++;
	}

	/*
	 * Count one for the local symbol that we generate,
	 * whose name is the file's name (usually) and whose address
	 * is the start of the file's text.
	 */

	if (!entry->is_dynamic) {
		local_sym_count++;
		non_L_local_sym_count++;
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
 * Symbols from shared objects are linked through `dynref'. For such symbols
 * that's all we do at this stage, with the exception of the case where the
 * symbol is a common. The `referenced' bit is only set for references from
 * relocatable objects.
 *
 */

void
enter_global_ref (lsp, name, entry)
     struct localsymbol *lsp;
     char *name;
     struct file_entry *entry;
{
	register struct nzlist *nzp = &lsp->nzlist;
	register symbol *sp = getsym (name);
	register int type = nzp->nz_type;
	int oldref = sp->referenced;
	int olddef = sp->defined;
	int com = sp->defined && sp->max_common_size;

	if (type == (N_INDR | N_EXT)) {
		sp->alias = getsym(entry->strings + (lsp + 1)->nzlist.nz_strx);
		if (sp == sp->alias) {
			error("%s: %s is alias for itself",
					get_file_name(entry), name);
			/* Rewrite symbol as global text symbol with value 0 */
			lsp->nzlist.nz_type = N_TEXT|N_EXT;
			lsp->nzlist.nz_value = 0;
			make_executable = 0;
		} else
			global_alias_count++;
	}

	if (entry->is_dynamic) {
		lsp->next = sp->sorefs;
		sp->sorefs = lsp;

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
				sp->max_common_size = nzp->nz_value;
				sp->defined = N_UNDF | N_EXT;
			} else if (com && sp->max_common_size < nzp->nz_value) {
				sp->max_common_size = nzp->nz_value;
			}
		}

		/*
		 * Handle size information in shared objects.
		 */
		if (nzp->nz_size > sp->size)
			sp->size = nzp->nz_size;

		lsp->symbol = sp;
		return;
	}

	lsp->next = sp->refs;
	sp->refs = lsp;
	lsp->symbol = sp;

	sp->referenced = 1;

	if (sp == dynamic_symbol || sp == got_symbol) {
		if (type != (N_UNDF | N_EXT) && !entry->just_syms_flag)
			fatal("Linker reserved symbol %s defined as type %x ",	
						name, type);
		return;
	}

#ifdef N_SIZE
	if (type == (N_SIZE | N_EXT)) {
		if (sp->size < nzp->nz_value)
			sp->size = nzp->nz_value;
	} else
#endif
	if (type != (N_UNDF | N_EXT) || nzp->nz_value) {

		/*
		 * Set `->defined' here, so commons and undefined globals
		 * can be counted correctly.
		 */
		if (!sp->defined || sp->defined == (N_UNDF | N_EXT))
			sp->defined = type;

		if (oldref && !olddef)
			/*
			 * It used to be undefined and we're defining it.
			 */
			undefined_global_sym_count--;

		if (!olddef && type == (N_UNDF | N_EXT) && nzp->nz_value) {
			/*
			 * First definition and it's common.
			 */
			common_defined_global_count++;
			sp->max_common_size = nzp->nz_value;
		} else if (com && type != (N_UNDF | N_EXT)) {
			/*
			 * It used to be common and we're defining
			 * it as something else.
			 */
			common_defined_global_count--;
			sp->max_common_size = 0;
		} else if (com && type == (N_UNDF | N_EXT)
				  && sp->max_common_size < nzp->nz_value)
			/*
			 * It used to be common and this is a new common entry
			 * to which we need to pay attention.
			 */
			sp->max_common_size = nzp->nz_value;

		if (SET_ELEMENT_P(type) && (!olddef || com))
			set_vector_count++;

	} else if (!oldref)
		undefined_global_sym_count++;


	if (sp == end_symbol && entry->just_syms_flag && !T_flag_specified)
		text_start = nzp->nz_value;

	if (sp->trace) {
		register char *reftype;
		switch (type & N_TYPE) {
		case N_UNDF:
			reftype = nzp->nz_value?
					"defined as common":"referenced";
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

		default:
			reftype = "I don't know this type";
			break;
		}

		fprintf (stderr, "symbol %s %s in ", sp->name, reftype);
		print_file_name (entry, stderr);
		fprintf (stderr, "\n");
	}
}

/*
 * This return 0 if the given file entry's symbol table does *not* contain
 * the nlist point entry, and it returns the files entry pointer (cast to
 * unsigned long) if it does.
 */

unsigned long
contains_symbol (entry, np)
     struct file_entry *entry;
     register struct nlist *np;
{
	if (np >= &entry->symbols->nzlist.nlist &&
		np < &(entry->symbols + entry->nsymbols)->nzlist.nlist)
		return (unsigned long) entry;
	return 0;
}


void consider_file_section_lengths (), relocate_file_addresses ();
void consider_relocation();

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
 *    for for possible contributions to the RRS section.
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

void digest_pass1(), digest_pass2();

void
digest_symbols ()
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
		set_sect_size = (2 * set_symbol_count + set_vector_count) *
							sizeof (unsigned long);
		set_vectors = (unsigned long *) xmalloc (set_sect_size);
		setv_fill_count = 0;
	}

	/* Pass 1: check and define symbols */
	defined_global_sym_count = 0;
	digest_pass1();

	if (!relocatable_output) {
		each_full_file(consider_relocation, 0);	/* Text */
		each_full_file(consider_relocation, 1); /* Data */
	}

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

	if (magic == ZMAGIC) {
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
		data_size += set_sect_size;
	}
	bss_start = rrs_data_start + data_size;

#ifdef DEBUG
printf("textstart = %#x, textsize = %#x, rrs_text_start = %#x, rrs_text_size %#x\n",
	text_start, text_size, rrs_text_start, rrs_text_size);
printf("datastart = %#x, datasize = %#x, rrs_data_start %#x, rrs_data_size %#x\n",
	data_start, data_size, rrs_data_start, rrs_data_size);
printf("bssstart = %#x, bsssize = %#x\n",
	bss_start, bss_size);
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

	if (magic == ZMAGIC)
		data_pad = PALIGN(data_pad + data_size, page_size) - data_size;

	bss_size -= data_pad;
	if (bss_size < 0)
		bss_size = 0;

	data_size += data_pad;
}

void
digest_pass1()
{

	/*
	 * Now, for each symbol, verify that it is defined globally at most
	 * once within relocatable files (except when building a shared lib).
	 * and set the `defined' field if there is a definition.
	 *
	 * Then check the shared object symbol chain for any remaining
	 * undefined symbols. Set the `so_defined' field for any
	 * definition find this way.
	 */
	FOR_EACH_SYMBOL(i, sp) {
		struct localsymbol *lsp;
		int             defs = 0;

		if (!sp->referenced) {
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

		if (sp == got_symbol || sp == dynamic_symbol)
			continue;

		for (lsp = sp->refs; lsp; lsp = lsp->next) {
			register struct nlist *p = &lsp->nzlist.nlist;
			register int    type = p->n_type;

			if (SET_ELEMENT_P(type)) {
				if (relocatable_output)
					fatal(
				"internal error: global ref to set el %s with -r",
						sp->name);
				if (!defs++) {
					sp->defined = N_SETV | N_EXT;
					sp->value =
						setv_fill_count++ * sizeof(long);
				} else if ((sp->defined & N_TYPE) != N_SETV) {
					sp->multiply_defined = 1;
					multiple_def_count++;
				}
				/* Keep count and remember symbol */
				sp->setv_count++;
				set_vectors[setv_fill_count++] = (long)p;

			} else if ((type & N_EXT) && type != (N_UNDF | N_EXT)
						&& (type & N_TYPE) != N_FN
						&& (type & N_TYPE) != N_SIZE) {
				/* non-common definition */
				if (defs++ && sp->value != p->n_value
				    && entry_symbol) {
					sp->multiply_defined = 1;
					multiple_def_count++;
				}
				sp->def_nlist = p;
				sp->defined = type;
			}
		}

		if (sp->defined) {
			if ((sp->defined & N_TYPE) == N_SETV)
				/* Allocate zero entry in set vector */
				setv_fill_count++;
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
			continue;
		}

		for (lsp = sp->sorefs; lsp; lsp = lsp->next) {
			register struct nlist *p = &lsp->nzlist.nlist;
			register int    type = p->n_type;

			if ((type & N_EXT) && type != (N_UNDF | N_EXT)
			    && (type & N_TYPE) != N_FN) {
				/* non-common definition */
				sp->def_nlist = p;
				sp->so_defined = type;
				undefined_global_sym_count--;
#ifdef DEBUG
printf("shr: %s gets defined to %x with value %x\n", sp->name, type, sp->value);
#endif
				break;
			}
		}
	} END_EACH_SYMBOL;
}

void
digest_pass2()
{
	/*
	 * Assign each symbol its final value.
	 * If not -r'ing, allocate common symbols in the BSS section.
	 */

	FOR_EACH_SYMBOL(i, sp) {
		int		size;
		int             align = sizeof(int);

		if (!sp->referenced)
			continue;

		if ((sp->defined & N_TYPE) == N_SETV) {
			/*
			 * Set length word at front of vector and zero byte
			 * at end. Reverse the vector itself to put it in
			 * file order.
			 */
			unsigned long	i, tmp;
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
			}

			/*
			 * Reverse the vector.
			 */
			for (i = 1; i < (sp->setv_count - 1)/2 + 1; i++) {

				tmp = set_vectors[length_word_index + i];
				set_vectors[length_word_index + i] =
					set_vectors[length_word_index + sp->setv_count + 1 - i];
				set_vectors[length_word_index + sp->setv_count + 1 - i] = tmp;
			}

			/* Clear terminating entry */
			set_vectors[length_word_index + sp->setv_count + 1] = 0;
			continue;
		}


		if (sp->defined && sp->def_nlist &&
				((sp->defined & ~N_EXT) != N_SETV))
			sp->value = sp->def_nlist->n_value;

		if (building_shared_object && !(link_mode & SYMBOLIC))
			/* No common allocation in shared objects */
			continue;

		if ((size = sp->max_common_size) != 0) {
			/*
			 * It's a common.
			 */
			if (sp->defined != (N_UNDF + N_EXT))
				fatal("%s: common isn't", sp->name);

		} else if ((size = sp->size) != 0 && sp->defined == N_SIZE) {
			/*
			 * It's data from shared object with size info.
			 */
			if (!sp->so_defined)
				fatal("%s: Bogus N_SIZE item", sp->name);

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

		while (!(size & align))
			align <<= 1;

		align = align > MAX_ALIGNMENT ?
			MAX_ALIGNMENT : align;

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
			printf("Allocating %s %s: %x at %x\n",
				sp->defined==(N_BSS|N_EXT)?"common":"data",
				sp->name, size, sp->value);

	} END_EACH_SYMBOL;
}

/*
 * Scan relocation info in ENTRY for contributions to the dynamic section of
 * the output file.
 */
void
consider_relocation (entry, dataseg)
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
			alloc_rrs_jmpslot(sp);

		} else if (RELOC_BASEREL_P(reloc)) {

			lsp = &entry->symbols[reloc->r_symbolnum];
			alloc_rrs_gotslot(reloc, lsp);

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
				fatal_with_file(
					"internal error, sp==NULL", entry);

			if (sp->alias)
				sp = sp->alias;

			/*
			 * Skip refs to _GLOBAL_OFFSET_TABLE_ and __DYNAMIC
			 */
			if (sp == got_symbol) {
				if (!CHECK_GOT_RELOC(reloc))
					fatal_with_file(
					"Unexpected relocation type ", entry);
				continue;
			}

			/*
			 * This symbol gives rise to a RRS entry
			 */

			if (building_shared_object) {
				alloc_rrs_reloc(sp);
				continue;
			}

			/*
			 * Only allocate an alias for function calls. Use
			 * sp->size here as a heuristic to discriminate
			 * between function definitions and data residing
			 * in the text segment.
			 * NOTE THAT THE COMPILER MUST NOT GENERATE ".size"
			 * DIRECTIVES FOR FUNCTIONS.
			 * In the future we might go for ".type" directives.
			 */
			if (force_alias_definition && sp->size == 0 &&
					sp->so_defined == N_TEXT + N_EXT) {

				/* Call to shared library procedure */
				alloc_rrs_jmpslot(sp);

			} else if (sp->size &&
					(sp->so_defined == N_DATA + N_EXT ||
					sp->so_defined == N_TEXT + N_EXT)) {

				/* Reference to shared library data */
				alloc_rrs_cpy_reloc(sp);
				sp->defined = N_SIZE;

			} else if (!sp->defined && sp->max_common_size == 0)
				alloc_rrs_reloc(sp);

		} else {
			/*
			 * Segment relocation.
			 * Prepare an RRS relocation as these are load
			 * address dependent.
			 */
			if (building_shared_object) {
				alloc_rrs_segment_reloc(reloc);
			}
		}
	}
}

/*
 * Accumulate the section sizes of input file ENTRY into the section sizes of
 * the output file.
 */
void
consider_file_section_lengths (entry)
     register struct file_entry *entry;
{
	if (entry->just_syms_flag)
		return;

	entry->text_start_address = text_size;
	/* If there were any vectors, we need to chop them off */
	text_size += entry->header.a_text;
	entry->data_start_address = data_size;
	data_size += entry->header.a_data;
	entry->bss_start_address = bss_size;
	bss_size += entry->header.a_bss;

	text_reloc_size += entry->header.a_trsize;
	data_reloc_size += entry->header.a_drsize;
}

/*
 * Determine where the sections of ENTRY go into the output file,
 * whose total section sizes are already known.
 * Also relocate the addresses of the file's local and debugger symbols.
 */
void
relocate_file_addresses (entry)
     register struct file_entry *entry;
{
	register struct localsymbol *lsp, *lspend;

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
		/*
		 * If this belongs to a section, update it
		 * by the section's start address
		 */
		register int type = p->n_type & N_TYPE;

		switch (type) {
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
			p->n_value += entry->bss_start_address
				- entry->header.a_text - entry->header.a_data;
		break;
		}
	}
}

/* Write the output file */

void
write_output ()
{
	struct stat	statbuf;
	int		filemode;

	if (lstat(output_filename, &statbuf) != -1) {
		if (!S_ISDIR(statbuf.st_mode))
			(void)unlink(output_filename);
	}

	outdesc = open (output_filename, O_WRONLY | O_CREAT | O_TRUNC, 0666);
	if (outdesc < 0)
		perror_name (output_filename);

	if (fstat (outdesc, &statbuf) < 0)
		perror_name (output_filename);

	filemode = statbuf.st_mode;

	chmod (output_filename, filemode & ~0111);

	/* Output the a.out header.  */
	write_header ();

	/* Output the text and data segments, relocating as we go.  */
	write_text ();
	write_data ();

	/* Output the merged relocation info, if requested with `-r'.  */
	if (relocatable_output)
		write_rel ();

	/* Output the symbol table (both globals and locals).  */
	write_syms ();

	/* Output the RSS section */
	write_rrs ();

	close (outdesc);

	if (chmod (output_filename, filemode | 0111) == -1)
		perror_name (output_filename);
}

void modify_location (), perform_relocation (), copy_text (), copy_data ();

/* Total number of symbols to be written in the output file. */
int	nsyms;

void
write_header ()
{
	int flags = (rrs_section_type == RRS_FULL) ? EX_DYNAMIC : 0;

	if (!oldmagic)
		N_SET_FLAG (outheader, flags);
	outheader.a_text = text_size;
	outheader.a_data = data_size;
	outheader.a_bss = bss_size;
	outheader.a_entry = (entry_symbol ? entry_symbol->value
					: text_start + entry_offset);

	if (strip_symbols == STRIP_ALL)
		nsyms = 0;
	else {
		nsyms = (defined_global_sym_count + undefined_global_sym_count);
		if (discard_locals == DISCARD_L)
			nsyms += non_L_local_sym_count;
		else if (discard_locals == DISCARD_NONE)
			nsyms += local_sym_count;

		if (relocatable_output)
			/* For each alias we write out two struct nlists */
			nsyms += set_symbol_count + global_alias_count;

		if (dynamic_symbol->referenced)
			nsyms++, special_sym_count++;

		if (got_symbol->referenced)
			nsyms++, special_sym_count++;
	}

	if (strip_symbols == STRIP_NONE)
		nsyms += debugger_sym_count;

#ifdef DEBUG
printf("defined globals: %d, undefined globals %d, locals: %d (non_L: %d), \
debug symbols: %d, special: %d --> nsyms %d\n",
	defined_global_sym_count, undefined_global_sym_count,
	local_sym_count, non_L_local_sym_count, debugger_sym_count,
	special_sym_count, nsyms);
#endif

	outheader.a_syms = nsyms * sizeof (struct nlist);

	if (relocatable_output) {
		outheader.a_trsize = text_reloc_size;
		outheader.a_drsize = data_reloc_size;
	} else {
		outheader.a_trsize = 0;
		outheader.a_drsize = 0;
	}

	md_swapout_exec_hdr(&outheader);
	mywrite (&outheader, sizeof (struct exec), 1, outdesc);
	md_swapin_exec_hdr(&outheader);

	/*
	 * Output whatever padding is required in the executable file
	 * between the header and the start of the text.
	 */

#ifndef COFF_ENCAPSULATE
	padfile (N_TXTOFF(outheader) - sizeof outheader, outdesc);
#endif
}

/* Relocate the text segment of each input file
   and write to the output file.  */

void
write_text ()
{

	if (trace_files)
		fprintf (stderr, "Copying and relocating text:\n\n");

	each_full_file (copy_text, 0);
	file_close ();

	if (trace_files)
		fprintf (stderr, "\n");

	padfile (text_pad, outdesc);
}

/*
 * Read the text segment contents of ENTRY, relocate them, and write the
 * result to the output file.  If `-r', save the text relocation for later
 * reuse.
 */
void
copy_text (entry)
     struct file_entry *entry;
{
	register char *bytes;
	register int desc;

	if (trace_files)
		prline_file_name (entry, stderr);

	desc = file_open (entry);

	/* Allocate space for the file's text section */
	bytes = (char *) alloca (entry->header.a_text);

	/* Deal with relocation information however is appropriate */
	if (entry->textrel == NULL)
		fatal_with_file("no text relocation of ", entry);

	/* Read the text section into core.  */
	lseek (desc, text_offset (entry), 0);
	if (entry->header.a_text != read (desc, bytes, entry->header.a_text))
		fatal_with_file ("premature eof in text section of ", entry);


	/* Relocate the text according to the text relocation.  */
	perform_relocation (bytes, entry->header.a_text,
			entry->textrel, entry->ntextrel, entry, 0);

	/* Write the relocated text to the output file.  */
	mywrite (bytes, 1, entry->header.a_text, outdesc);
}

/* Relocate the data segment of each input file
   and write to the output file.  */

void
write_data ()
{
	long	pos;

	if (trace_files)
		fprintf (stderr, "Copying and relocating data:\n\n");

	pos = N_DATOFF(outheader) + data_start - rrs_data_start;
	if (lseek(outdesc, pos, L_SET) != pos)
		fatal("write_data: lseek: cant position data offset");

	each_full_file (copy_data, 0);
	file_close ();

	/*
	 * Write out the set element vectors.  See digest symbols for
	 * description of length of the set vector section.
	 */

	if (set_vector_count) {
		swap_longs(set_vectors, 2 * set_symbol_count + set_vector_count);
		mywrite (set_vectors, 2 * set_symbol_count + set_vector_count,
				sizeof (unsigned long), outdesc);
	}

	if (trace_files)
		fprintf (stderr, "\n");

	padfile (data_pad, outdesc);
}

/*
 * Read the data segment contents of ENTRY, relocate them, and write the
 * result to the output file. If `-r', save the data relocation for later
 * reuse. See comments in `copy_text'.
 */
void
copy_data (entry)
     struct file_entry *entry;
{
	register char *bytes;
	register int desc;

	if (trace_files)
		prline_file_name (entry, stderr);

	desc = file_open (entry);

	bytes = (char *)alloca(entry->header.a_data);

	if (entry->datarel == NULL)
		fatal_with_file("no data relocation of ", entry);

	lseek (desc, text_offset (entry) + entry->header.a_text, 0);
	if (entry->header.a_data != read(desc, bytes, entry->header.a_data))
		fatal_with_file ("premature eof in data section of ", entry);

	perform_relocation (bytes, entry->header.a_data,
			entry->datarel, entry->ndatarel, entry, 1);

	mywrite (bytes, 1, entry->header.a_data, outdesc);
}

/*
 * Relocate ENTRY's text or data section contents. DATA is the address of the
 * contents, in core. DATA_SIZE is the length of the contents. PC_RELOCATION
 * is the difference between the address of the contents in the output file
 * and its address in the input file. RELOC is the address of the
 * relocation info, in core. NRELOC says how many there are.
 */
void
perform_relocation(data, data_size, reloc, nreloc, entry, dataseg)
	char			*data;
	int			data_size;
	struct relocation_info	*reloc;
	int			nreloc;
	struct file_entry	*entry;
	int			dataseg;
{
	register struct relocation_info *r = reloc;
	struct relocation_info *end = reloc + nreloc;

	text_relocation = entry->text_start_address;
	data_relocation = entry->data_start_address - entry->header.a_text;
	bss_relocation = entry->bss_start_address -
				entry->header.a_text - entry->header.a_data;
	pc_relocation = dataseg?
			entry->data_start_address - entry->header.a_text:
			entry->text_start_address;

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
			fatal_with_file(
				"relocation address out of range in ", entry);

		if (RELOC_JMPTAB_P(r)) {

			int		   symindex = RELOC_SYMBOL(r);
			struct localsymbol *lsp = &entry->symbols[symindex];
			symbol		   *sp;

			if (symindex >= entry->nsymbols)
				fatal_with_file(
				"relocation symbolnum out of range in ", entry);

			sp = lsp->symbol;
			if (sp->alias)
				sp = sp->alias;

			if (relocatable_output)
				relocation = addend;
			else if (!RELOC_EXTERN_P(r)) {
				relocation = addend +
					data_relocation - text_relocation;
			} else
				relocation = addend +
					claim_rrs_jmpslot(r, sp, addend);

		} else if (RELOC_BASEREL_P(r)) {

			int		   symindex = RELOC_SYMBOL(r);
			struct localsymbol *lsp = &entry->symbols[symindex];

			if (symindex >= entry->nsymbols)
				fatal_with_file(
				"relocation symbolnum out of range in ", entry);

			if (relocatable_output)
				relocation = addend;
			else if (!RELOC_EXTERN_P(r))
				relocation = claim_rrs_internal_gotslot(entry,
								r, lsp, addend);
			else
				relocation = claim_rrs_gotslot(r, lsp, addend);

		} else if (RELOC_EXTERN_P(r)) {

			int     symindex = RELOC_SYMBOL(r);
			symbol  *sp;

			if (symindex >= entry->nsymbols)
				fatal_with_file(
				"relocation symbolnum out of range in ", entry);

			sp = entry->symbols[symindex].symbol;
			if (sp->alias)
				sp = sp->alias;

			if (relocatable_output) {
				relocation = addend + sp->value;
			} else if (sp->defined) {
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
					if (claim_rrs_reloc(r, sp, &relocation))
						continue;
				} else if (sp->defined == N_SIZE) {
					/*
					 * If size is known, arrange a
					 * run-time copy.
					 */
					if (!sp->size)
						fatal("Copy item isn't: %s",
							sp->name);

					relocation = addend + sp->value;
					r->r_address = sp->value;
					claim_rrs_cpy_reloc(r, sp);
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
				 *
				 * In case the symbol is defined in a shared
				 * object as N_TEXT or N_DATA, an appropriate
				 * jmpslot or copy relocation is generated.
				 */
				switch (sp->so_defined) {

				case N_TEXT+N_EXT:
					/*
					 * Claim a jmpslot if one was
					 * allocated (dependent on
					 * `force_alias_flag').
					 */

					if (sp->jmpslot_offset == -1)
						goto undefined;

					relocation = addend +
						claim_rrs_jmpslot(r, sp, addend);
					break;

				case N_DATA+N_EXT:
					/*FALLTHROUGH*/
				case 0:
				undefined:
					r->r_address += dataseg?
						entry->data_start_address:
						entry->text_start_address;
					relocation = addend;
					if (claim_rrs_reloc(r, sp, &relocation))
						continue;
					break;

				case N_BSS+N_EXT:
printf("%s: BSS found in so_defined\n", sp->name);
					/*break;*/

				default:
					fatal("%s: shobj symbol with unknown type %#x", sp->name, sp->so_defined);
					break;
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
				fatal_with_file(
				"nonexternal relocation code invalid in ", entry);
			}

			/*
			 * When building a shared object, these segment
			 * relocations need a "load address relative"
			 * RRS fixup.
			 */
			if (building_shared_object) {
				r->r_address += dataseg?
					entry->data_start_address:
					entry->text_start_address;
				claim_rrs_segment_reloc(r);
			}
		}

		if (RELOC_PCREL_P(r))
			relocation -= pc_relocation;

		md_relocate(r, relocation, data+addr, relocatable_output);

	}
}

/* For relocatable_output only: write out the relocation,
   relocating the addresses-to-be-relocated.  */

void coptxtrel (), copdatrel ();

void
write_rel ()
{
	register int count = 0;

	if (trace_files)
		fprintf (stderr, "Writing text relocation:\n\n");

	/*
	 * Assign each global symbol a sequence number, giving the order
	 * in which `write_syms' will write it.
	 * This is so we can store the proper symbolnum fields
	 * in relocation entries we write.
	 *

	/* BLECH - Assign number 0 to __DYNAMIC (!! Sun compatibility) */

	if (dynamic_symbol->referenced)
		dynamic_symbol->symbolnum = count++;
	FOR_EACH_SYMBOL(i, sp) {
		if (sp != dynamic_symbol && sp->referenced) {
			sp->symbolnum = count++;
		}
	} END_EACH_SYMBOL;

	/* Correct, because if (relocatable_output), we will also be writing
	whatever indirect blocks we have.  */
	if (count != defined_global_sym_count + undefined_global_sym_count
							+ special_sym_count)
		fatal ("internal error: write_rel: count = %d", count);

	/* Write out the relocations of all files, remembered from copy_text. */
	each_full_file (coptxtrel, 0);

	if (trace_files)
		fprintf (stderr, "\nWriting data relocation:\n\n");

	each_full_file (copdatrel, 0);

	if (trace_files)
		fprintf (stderr, "\n");
}

void
coptxtrel(entry)
	struct file_entry *entry;
{
	register struct relocation_info *r, *end;
	register int    reloc = entry->text_start_address;

	r = entry->textrel;
	end = r + entry->ntextrel;

	for (; r < end; r++) {
		register int  symindex;
		symbol       *sp;

		RELOC_ADDRESS(r) += reloc;

		if (!RELOC_EXTERN_P(r))
			continue;

		symindex = RELOC_SYMBOL(r);
		sp = entry->symbols[symindex].symbol;

		if (symindex >= entry->nsymbols)
			fatal_with_file(
			"relocation symbolnum out of range in ", entry);

#ifdef N_INDR
		/* Resolve indirection.  */
		if ((sp->defined & ~N_EXT) == N_INDR) {
			if (sp->alias == NULL)
				fatal("internal error: alias in hyperspace");
			sp = sp->alias;
		}
#endif

		/*
		 * If the symbol is now defined, change the external
		 * relocation to an internal one.
		 */

		if (sp->defined) {
			RELOC_EXTERN_P(r) = 0;
			RELOC_SYMBOL(r) = (sp->defined & N_TYPE);
#ifdef RELOC_ADD_EXTRA
			/*
			 * If we aren't going to be adding in the
			 * value in memory on the next pass of the
			 * loader, then we need to add it in from the
			 * relocation entry.  Otherwise the work we
			 * did in this pass is lost.
			 */
			if (!RELOC_MEMORY_ADD_P(r))
				RELOC_ADD_EXTRA(r) += sp->value;
#endif
		} else
			/*
			 * Global symbols come first.
			 */
			RELOC_SYMBOL(r) = sp->symbolnum;
	}
	md_swapout_reloc(entry->textrel, entry->ntextrel);
	mywrite(entry->textrel, entry->ntextrel,
				sizeof(struct relocation_info), outdesc);
}

void
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

		if (!RELOC_EXTERN_P(r))
			continue;

		symindex = RELOC_SYMBOL(r);
		sp = entry->symbols[symindex].symbol;

		if (symindex >= entry->header.a_syms)
			fatal_with_file(
			"relocation symbolnum out of range in ", entry);

#ifdef N_INDR
		/* Resolve indirection.  */
		if ((sp->defined & ~N_EXT) == N_INDR) {
			if (sp->alias == NULL)
				fatal("internal error: alias in hyperspace");
			sp = sp->alias;
		}
#endif

		symtype = sp->defined & N_TYPE;

		if (force_common_definition ||
					symtype == N_DATA ||
					symtype == N_TEXT ||
					symtype == N_ABS) {
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
				sizeof(struct relocation_info), outdesc);
}

void write_file_syms ();
void write_string_table ();

/* Offsets and current lengths of symbol and string tables in output file. */

int symbol_table_offset;
int symbol_table_len;

/* Address in output file where string table starts.  */
int string_table_offset;

/* Offset within string table
   where the strings in `strtab_vector' should be written.  */
int string_table_len;

/* Total size of string table strings allocated so far,
   including strings in `strtab_vector'.  */
int strtab_size;

/* Vector whose elements are strings to be added to the string table.  */
char **strtab_vector;

/* Vector whose elements are the lengths of those strings.  */
int *strtab_lens;

/* Index in `strtab_vector' at which the next string will be stored.  */
int strtab_index;

/*
 * Add the string NAME to the output file string table. Record it in
 * `strtab_vector' to be output later. Return the index within the string
 * table that this string will have.
 */

int
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

FILE           *outstream = (FILE *) 0;

/*
 * Write the contents of `strtab_vector' into the string table. This is done
 * once for each file's local&debugger symbols and once for the global
 * symbols.
 */
void
write_string_table ()
{
	register int i;

	lseek (outdesc, string_table_offset + string_table_len, 0);

	if (!outstream)
		outstream = fdopen (outdesc, "w");

	for (i = 0; i < strtab_index; i++) {
		fwrite (strtab_vector[i], 1, strtab_lens[i], outstream);
		string_table_len += strtab_lens[i];
	}

	fflush (outstream);

	/* Report I/O error such as disk full.  */
	if (ferror (outstream))
		perror_name (output_filename);
}

/* Write the symbol table and string table of the output file.  */

void
write_syms()
{
	/* Number of symbols written so far.  */
	int		non_local_syms = defined_global_sym_count
					+ undefined_global_sym_count
					+ global_alias_count
					+ special_sym_count;
	int		syms_written = 0;
	struct nlist	nl;

	/*
	 * Buffer big enough for all the global symbols.  One extra struct
	 * for each indirect symbol to hold the extra reference following.
	 */
	struct nlist   *buf
		= (struct nlist *)alloca(non_local_syms * sizeof(struct nlist));
	/* Pointer for storing into BUF.  */
	register struct nlist *bufp = buf;

	/* Size of string table includes the bytes that store the size.  */
	strtab_size = sizeof strtab_size;

	symbol_table_offset = N_SYMOFF(outheader);
	symbol_table_len = 0;
	string_table_offset = N_STROFF(outheader);
	string_table_len = strtab_size;

	if (strip_symbols == STRIP_ALL)
		return;

	/* First, write out the global symbols.  */

	/*
	 * Allocate two vectors that record the data to generate the string
	 * table from the global symbols written so far.  This must include
	 * extra space for the references following indirect outputs.
	 */

	strtab_vector = (char **) alloca((non_local_syms) * sizeof(char *));
	strtab_lens = (int *) alloca((non_local_syms) * sizeof(int));
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
	if (dynamic_symbol->referenced) {
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

		if (!sp->referenced)
			/* Came from shared object but was not used */
			continue;

		if (sp->so_defined)
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
			if (!building_shared_object)
				error("symbol %s remains undefined", sp->name);
			continue;
		}

		/* Construct a `struct nlist' for the symbol.  */

		nl.n_other = 0;
		nl.n_desc = 0;

		/*
		 * common condition needs to be before undefined
		 * condition because unallocated commons are set
		 * undefined in digest_symbols
		 */
		if (sp->defined > 1) {
			/* defined with known type */

			if (!relocatable_output && sp->alias &&
						sp->alias->defined > 1) {
				/*
				 * If the target of an indirect symbol has
				 * been defined and we are outputting an
				 * executable, resolve the indirection; it's
				 * no longer needed
				 */
				nl.n_type = sp->alias->defined;
				nl.n_type = sp->alias->value;
			} else if (sp->defined == N_SIZE)
				nl.n_type = N_DATA | N_EXT;
			else
				nl.n_type = sp->defined;
			nl.n_value = sp->value;
		} else if (sp->max_common_size) {
			/*
			 * defined as common but not allocated,
			 * happens only with -r and not -d, write out
			 * a common definition
			 */
			nl.n_type = N_UNDF | N_EXT;
			nl.n_value = sp->max_common_size;
		} else if (!sp->defined) {
			/* undefined -- legit only if -r */
			nl.n_type = N_UNDF | N_EXT;
			nl.n_value = 0;
		} else
			fatal(
			      "internal error: %s defined in mysterious way",
			      sp->name);

		/*
		 * Allocate string table space for the symbol name.
		 */

		nl.n_un.n_strx = assign_string_table_index(sp->name);

		/* Output to the buffer and count it.  */

		if (syms_written >= non_local_syms)
			fatal(
			"internal error: number of symbols exceeds allocated %d",
				non_local_syms);
		*bufp++ = nl;
		syms_written++;

		if (nl.n_type == N_INDR + N_EXT) {
			if (sp->alias == NULL)
				fatal("internal error: alias in hyperspace");
			nl.n_type = N_UNDF + N_EXT;
			nl.n_un.n_strx =
				assign_string_table_index(sp->alias->name);
			nl.n_value = 0;
			*bufp++ = nl;
			syms_written++;
		}

#ifdef DEBUG
printf("writesym(#%d): %s, type %x\n", syms_written, sp->name, sp->defined);
#endif
	} END_EACH_SYMBOL;

	if (syms_written != strtab_index || strtab_index != non_local_syms)
		fatal("internal error:\
wrong number (%d) of global symbols written into output file, should be %d",
				syms_written, non_local_syms);

	/* Output the buffer full of `struct nlist's.  */

	lseek(outdesc, symbol_table_offset + symbol_table_len, 0);
	md_swapout_symbols(buf, bufp - buf);
	mywrite(buf, bufp - buf, sizeof(struct nlist), outdesc);
	symbol_table_len += sizeof(struct nlist) * (bufp - buf);

	/* Write the strings for the global symbols.  */
	write_string_table();

	/* Write the local symbols defined by the various files.  */
	each_file(write_file_syms, &syms_written);
	file_close();

	if (syms_written != nsyms)
		fatal("internal error:\
wrong number of symbols (%d) written into output file, should be %d",
				syms_written, nsyms);

	if (symbol_table_offset + symbol_table_len != string_table_offset)
		fatal(
		"internal error: inconsistent symbol table length: %d vs %s",
		symbol_table_offset + symbol_table_len, string_table_offset);

	lseek(outdesc, string_table_offset, 0);
	strtab_size = md_swap_long(strtab_size);
	mywrite(&strtab_size, sizeof(int), 1, outdesc);
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
	struct file_entry *entry;
	int            *syms_written_addr;
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

	if (entry->is_dynamic)
		return;

	/*
	 * Make tables that record, for each symbol, its name and its name's
	 * length. The elements are filled in by `assign_string_table_index'.
	 */

	strtab_vector = (char **) alloca(max_syms * sizeof(char *));
	strtab_lens = (int *) alloca(max_syms * sizeof(int));
	strtab_index = 0;

	/* Generate a local symbol for the start of this file's text.  */

	if (discard_locals != DISCARD_ALL) {
		struct nlist    nl;

		nl.n_type = N_FN | N_EXT;
		nl.n_un.n_strx = assign_string_table_index(entry->local_sym_name);
		nl.n_value = entry->text_start_address;
		nl.n_desc = 0;
		nl.n_other = 0;
		*bufp++ = nl;
		(*syms_written_addr)++;
		entry->local_syms_offset = *syms_written_addr * sizeof(struct nlist);
	}
	/* Read the file's string table.  */

	entry->strings = (char *) alloca(entry->string_size);
	read_entry_strings(file_open(entry), entry);

	lspend = entry->symbols + entry->nsymbols;

	for (lsp = entry->symbols; lsp < lspend; lsp++) {
		register struct nlist *p = &lsp->nzlist.nlist;
		register int    type = p->n_type;
		register int    write = 0;

		/*
		 * WRITE gets 1 for a non-global symbol that should be
		 * written.
		 */

		if (SET_ELEMENT_P (type))
			/*
			 * This occurs even if global. These types of
			 * symbols are never written globally, though
			 * they are stored globally.
			 */
			write = relocatable_output;
		else if (!(type & (N_STAB | N_EXT)))
			/* ordinary local symbol */
			write = ((discard_locals != DISCARD_ALL)
				 && !(discard_locals == DISCARD_L &&
			    (p->n_un.n_strx + entry->strings)[0] == LPREFIX)
				 && type != N_WARNING);
		else if (!(type & N_EXT))
			/* debugger symbol */
			write = (strip_symbols == STRIP_NONE) &&
				!(discard_locals == DISCARD_L &&
				(p->n_un.n_strx + entry->strings)[0] == LPREFIX);

		if (write) {
			/*
			 * If this symbol has a name, allocate space for it
			 * in the output string table.
			 */

			if (p->n_un.n_strx)
				p->n_un.n_strx = assign_string_table_index(
						p->n_un.n_strx + entry->strings);

			/* Output this symbol to the buffer and count it.  */

			*bufp++ = *p;
			(*syms_written_addr)++;
		}
	}

	/* All the symbols are now in BUF; write them.  */

	lseek(outdesc, symbol_table_offset + symbol_table_len, 0);
	md_swapout_symbols(buf, bufp - buf);
	mywrite(buf, bufp - buf, sizeof(struct nlist), outdesc);
	symbol_table_len += sizeof(struct nlist) * (bufp - buf);

	/*
	 * Write the string-table data for the symbols just written, using
	 * the data in vectors `strtab_vector' and `strtab_lens'.
	 */

	write_string_table();
	entry->strings = 0;	/* Since it will disappear anyway.  */
}
