/*
 * $Id: warnings.c,v 1.4 1993/12/22 23:28:12 jkh Exp $
 */

#include <sys/param.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/time.h>
#include <sys/errno.h>
#include <fcntl.h>
#include <ar.h>
#include <ranlib.h>
#include <a.out.h>
#include <stab.h>
#include <string.h>
#if __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

#include "ld.h"

/*
 * Print the filename of ENTRY on OUTFILE (a stdio stream),
 * and then a newline.
 */

void
prline_file_name (entry, outfile)
     struct file_entry *entry;
     FILE *outfile;
{
	print_file_name (entry, outfile);
	fprintf (outfile, "\n");
}

/*
 * Print the filename of ENTRY on OUTFILE (a stdio stream).
 */

void
print_file_name (entry, outfile)
     struct file_entry *entry;
     FILE *outfile;
{
	if (entry->superfile) {
		print_file_name (entry->superfile, outfile);
		fprintf (outfile, "(%s)", entry->filename);
	} else
		fprintf (outfile, "%s", entry->filename);
}

/*
 * Return the filename of entry as a string (malloc'd for the purpose)
 */

char *
get_file_name (entry)
     struct file_entry *entry;
{
	char *result, *supfile;

	if (entry == NULL) {
		return (xmalloc("NULL"));
	}

	if (entry->superfile) {
		supfile = get_file_name (entry->superfile);
		result = (char *) xmalloc (strlen(supfile)
					+ strlen(entry->filename) + 3);
		sprintf (result, "%s(%s)", supfile, entry->filename);
		free (supfile);

	} else {
		result = (char *) xmalloc (strlen (entry->filename) + 1);
		strcpy (result, entry->filename);
	}
	return result;
}

/*
 * Report a fatal error.  The error message is STRING followed by the
 * filename of ENTRY.
 */
void
#if __STDC__
fatal_with_file (char *fmt, struct file_entry *entry, ...)
#else
fatal_with_file (fmt, entry, va_alist)
	char			*fmt;
	struct file_entry	*entry;
	va_dcl
#endif
{
	va_list	ap;
#if __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
#endif
	(void)fprintf(stderr, "%s: ", progname);
	(void)vfprintf(stderr, fmt, ap);
	print_file_name (entry, stderr);
	(void)fprintf(stderr, "\n");

	va_end(ap);
	exit (1);
}

/*
 * Report a fatal error using the message for the last failed system call,
 * followed by the string NAME.
 */
void
perror_name (name)
	char *name;
{
	char *s;

	if (errno < sys_nerr)
		s = concat ("", sys_errlist[errno], " for %s");
	else
		s = "cannot open %s";
	fatal (s, name);
}

/*
 * Report a fatal error using the message for the last failed system call,
 * followed by the name of file ENTRY.
 */
void
perror_file (entry)
     struct file_entry *entry;
{
	char *s;

	if (errno < sys_nerr)
		s = concat ("", sys_errlist[errno], " for ");
	else
		s = "cannot open ";
	fatal_with_file (s, entry);
}


/* Print a complete or partial map of the output file. */

static void	describe_file_sections __P((struct file_entry *, FILE *));
static void	list_file_locals __P((struct file_entry *, FILE *));

void
print_symbols(outfile)
	FILE           *outfile;
{
	register int    i;

	fprintf(outfile, "\nFiles:\n\n");

	each_file(describe_file_sections, outfile);

	fprintf(outfile, "\nGlobal symbols:\n\n");

	for (i = 0; i < TABSIZE; i++) {
		register symbol *sp;
		for (sp = symtab[i]; sp; sp = sp->link) {
			if (sp->defined == (N_UNDF|N_EXT))
				fprintf(outfile, "  %s: common, length %#x\n",
						sp->name, sp->max_common_size);
			if (!sp->referenced)
				fprintf(outfile, "  %s: unreferenced\n",
								sp->name);
			else if (!sp->defined)
				fprintf(outfile, "  %s: undefined\n", sp->name);
			else
				fprintf(outfile, "  %s: %#x, size %#x\n",
						sp->name, sp->value, sp->size);
		}
	}

	each_file(list_file_locals, outfile);
}

static void
describe_file_sections(entry, outfile)
	struct file_entry *entry;
	FILE           *outfile;
{
	fprintf(outfile, "  ");
	print_file_name(entry, outfile);
	if (entry->just_syms_flag || entry->is_dynamic)
		fprintf(outfile, " symbols only\n", 0);
	else
		fprintf(outfile, " text %x(%x), data %x(%x), bss %x(%x) hex\n",
			entry->text_start_address, entry->header.a_text,
			entry->data_start_address, entry->header.a_data,
			entry->bss_start_address, entry->header.a_bss);
}

static void
list_file_locals (entry, outfile)
     struct file_entry *entry;
     FILE *outfile;
{
	struct localsymbol	*lsp, *lspend;

	entry->strings = (char *) alloca (entry->string_size);
	read_entry_strings (file_open (entry), entry);

	fprintf (outfile, "\nLocal symbols of ");
	print_file_name (entry, outfile);
	fprintf (outfile, ":\n\n");

	lspend = entry->symbols + entry->nsymbols;
	for (lsp = entry->symbols; lsp < lspend; lsp++) {
		register struct nlist *p = &lsp->nzlist.nlist;
		/*
		 * If this is a definition,
		 * update it if necessary by this file's start address.
		 */
		if (!(p->n_type & (N_STAB | N_EXT)))
			fprintf(outfile, "  %s: 0x%x\n",
				entry->strings + p->n_un.n_strx, p->n_value);
	}

	entry->strings = 0;		/* All done with them.  */
}


/* Static vars for do_warnings and subroutines of it */
static int list_unresolved_refs;	/* List unresolved refs */
static int list_warning_symbols;	/* List warning syms */
static int list_multiple_defs;		/* List multiple definitions */

static struct line_debug_entry *init_debug_scan __P((int, struct file_entry *));
static int		next_debug_entry __P((int, struct line_debug_entry *));

/*
 * Structure for communication between do_file_warnings and it's
 * helper routines.  Will in practice be an array of three of these:
 * 0) Current line, 1) Next line, 2) Source file info.
 */
struct line_debug_entry
{
	int			line;
	char			*filename;
	struct localsymbol	*sym;
};

/*
 * Helper routines for do_file_warnings.
 */

/* Return an integer less than, equal to, or greater than 0 as per the
   relation between the two relocation entries.  Used by qsort.  */

static int
relocation_entries_relation (rel1, rel2)
     struct relocation_info *rel1, *rel2;
{
	return RELOC_ADDRESS(rel1) - RELOC_ADDRESS(rel2);
}

/* Moves to the next debugging symbol in the file.  USE_DATA_SYMBOLS
   determines the type of the debugging symbol to look for (DSLINE or
   SLINE).  STATE_POINTER keeps track of the old and new locatiosn in
   the file.  It assumes that state_pointer[1] is valid; ie
   that it.sym points into some entry in the symbol table.  If
   state_pointer[1].sym == 0, this routine should not be called.  */

static int
next_debug_entry (use_data_symbols, state_pointer)
     register int use_data_symbols;
     /* Next must be passed by reference! */
     struct line_debug_entry state_pointer[3];
{
	register struct line_debug_entry
				*current = state_pointer,
				*next = state_pointer + 1,
				/* Used to store source file */
				*source = state_pointer + 2;

	struct file_entry	*entry = (struct file_entry *) source->sym;
	struct localsymbol	*endp = entry->symbols + entry->nsymbols;


	current->sym = next->sym;
	current->line = next->line;
	current->filename = next->filename;

	while (++(next->sym) < endp) {

		struct nlist	*np = &next->sym->nzlist.nlist;

		/*
		 * n_type is a char, and N_SOL, N_EINCL and N_BINCL are > 0x80,
		 * so may look negative...therefore, must mask to low bits
		 */
		switch (np->n_type & 0xff) {
		case N_SLINE:
			if (use_data_symbols) continue;
			next->line = np->n_desc;
			return 1;
		case N_DSLINE:
			if (!use_data_symbols) continue;
			next->line = np->n_desc;
			return 1;
#ifdef HAVE_SUN_STABS
		case N_EINCL:
			next->filename = source->filename;
			continue;
#endif
		case N_SO:
			source->filename = np->n_un.n_strx + entry->strings;
			source->line++;
#ifdef HAVE_SUN_STABS
		case N_BINCL:
#endif
		case N_SOL:
			next->filename = np->n_un.n_strx + entry->strings;
		default:
			continue;
		}
	}
	next->sym = (struct localsymbol *)0;
	return 0;
}

/*
 * Create a structure to save the state of a scan through the debug symbols.
 * USE_DATA_SYMBOLS is set if we should be scanning for DSLINE's instead of
 * SLINE's.  entry is the file entry which points at the symbols to use.
 */

static struct line_debug_entry *
init_debug_scan(use_data_symbols, entry)
	int             use_data_symbols;
	struct file_entry *entry;
{
	struct localsymbol	*lsp;
	struct line_debug_entry *state_pointer = (struct line_debug_entry *)
				xmalloc(3 * sizeof(struct line_debug_entry));

	register struct line_debug_entry
		*current = state_pointer,
		*next = state_pointer + 1,
		*source = state_pointer + 2;	/* Used to store source file */


	for (lsp = entry->symbols; lsp < entry->symbols+entry->nsymbols; lsp++)
		if (lsp->nzlist.nlist.n_type == N_SO)
			break;

	if (lsp >= entry->symbols + entry->nsymbols) {
		/* I believe this translates to "We lose" */
		current->filename = next->filename = entry->filename;
		current->line = next->line = -1;
		current->sym = next->sym = (struct localsymbol *) 0;
		return state_pointer;
	}
	next->line = source->line = 0;
	next->filename = source->filename
			= (lsp->nzlist.nlist.n_un.n_strx + entry->strings);
	source->sym = (struct localsymbol *) entry;
	next->sym = lsp;

	/* To setup next */
	next_debug_entry(use_data_symbols, state_pointer);

	if (!next->sym) {	/* No line numbers for this section; */
		/* setup output results as appropriate */
		if (source->line) {
			current->filename = source->filename = entry->filename;
			current->line = -1;	/* Don't print lineno */
		} else {
			current->filename = source->filename;
			current->line = 0;
		}
		return state_pointer;
	}
	/* To setup current */
	next_debug_entry(use_data_symbols, state_pointer);

	return state_pointer;
}

/*
 * Takes an ADDRESS (in either text or data space) and a STATE_POINTER which
 * describes the current location in the implied scan through the debug
 * symbols within the file which ADDRESS is within, and returns the source
 * line number which corresponds to ADDRESS.
 */

static int
address_to_line(address, state_pointer)
	unsigned long   address;
/* Next must be passed by reference! */
	struct line_debug_entry state_pointer[3];
{
	struct line_debug_entry
	               *current = state_pointer, *next = state_pointer + 1;
	struct line_debug_entry *tmp_pointer;

	int             use_data_symbols;

	if (next->sym)
		use_data_symbols =
			(next->sym->nzlist.nlist.n_type & N_TYPE) == N_DATA;
	else
		return current->line;

	/* Go back to the beginning if we've already passed it.  */
	if (current->sym->nzlist.nlist.n_value > address) {
		tmp_pointer = init_debug_scan(use_data_symbols,
					      (struct file_entry *)
					      ((state_pointer + 2)->sym));
		state_pointer[0] = tmp_pointer[0];
		state_pointer[1] = tmp_pointer[1];
		state_pointer[2] = tmp_pointer[2];
		free(tmp_pointer);
	}
	/* If we're still in a bad way, return -1, meaning invalid line.  */
	if (current->sym->nzlist.nlist.n_value > address)
		return -1;

	while (next->sym
	       && next->sym->nzlist.nlist.n_value <= address
	       && next_debug_entry(use_data_symbols, state_pointer));
	return current->line;
}


/* Macros for manipulating bitvectors.  */
#define	BIT_SET_P(bv, index)	((bv)[(index) >> 3] & 1 << ((index) & 0x7))
#define	SET_BIT(bv, index)	((bv)[(index) >> 3] |= 1 << ((index) & 0x7))

/*
 * This routine will scan through the relocation data of file ENTRY, printing
 * out references to undefined symbols and references to symbols defined in
 * files with N_WARNING symbols.  If DATA_SEGMENT is non-zero, it will scan
 * the data relocation segment (and use N_DSLINE symbols to track line
 * number); otherwise it will scan the text relocation segment.  Warnings
 * will be printed on the output stream OUTFILE.  Eventually, every nlist
 * symbol mapped through will be marked in the NLIST_BITVECTOR, so we don't
 * repeat ourselves when we scan the nlists themselves.
 */

static void
do_relocation_warnings(entry, data_segment, outfile, nlist_bitvector)
	struct file_entry *entry;
	int             data_segment;
	FILE           *outfile;
	unsigned char  *nlist_bitvector;
{
	struct relocation_info *reloc, *reloc_start =
			data_segment ? entry->datarel : entry->textrel;

	int	reloc_size = (data_segment ? entry->ndatarel : entry->ntextrel);
	int	start_of_segment = (data_segment ?
					entry->data_start_address :
					entry->text_start_address);
	struct localsymbol	*start_of_syms = entry->symbols;
	struct line_debug_entry	*state_pointer =
				init_debug_scan(data_segment != 0, entry);

	register struct line_debug_entry	*current = state_pointer;

	/* Assigned to generally static values; should not be written into.  */
	char *errfmt;

	/*
	 * Assigned to alloca'd values cand copied into; should be freed when
	 * done.
	 */
	char *errmsg;
	int  invalidate_line_number;

	/*
	 * We need to sort the relocation info here.  Sheesh, so much effort
	 * for one lousy error optimization.
	 */

	qsort(reloc_start, reloc_size, sizeof(struct relocation_info),
	      relocation_entries_relation);

	for (reloc = reloc_start;
	     reloc < (reloc_start + reloc_size);
	     reloc++) {
		register struct localsymbol *s;
		register symbol *g;

		/*
		 * If the relocation isn't resolved through a symbol,
		 * continue
		 */
		if (!RELOC_EXTERN_P(reloc))
			continue;

		s = &entry->symbols[RELOC_SYMBOL(reloc)];

		/*
		 * Local symbols shouldn't ever be used by relocation info,
		 * so the next should be safe. This is, of course, wrong.
		 * References to local BSS symbols can be the targets of
		 * relocation info, and they can (must) be resolved through
		 * symbols.  However, these must be defined properly, (the
		 * assembler would have caught it otherwise), so we can
		 * ignore these cases.
		 */
		if (!(s->nzlist.nz_type & N_EXT))
			continue;

		g = s->symbol;
		errmsg = 0;

		if (!g->defined && !g->so_defined && list_unresolved_refs) {	/* Reference */
			/* Mark as being noted by relocation warning pass.  */
			SET_BIT(nlist_bitvector, s - start_of_syms);

			if (g->undef_refs >= MAX_UREFS_PRINTED)	/* Listed too many */
				continue;

			/* Undefined symbol which we should mention */

			if (++(g->undef_refs) == MAX_UREFS_PRINTED) {
				errfmt = "More undefined symbol %s refs follow";
				invalidate_line_number = 1;
			} else {
				errfmt = "Undefined symbol %s referenced from %s segment";
				invalidate_line_number = 0;
			}
		} else {	/* Defined */
			/* Potential symbol warning here */
			if (!g->warning)
				continue;

			/* Mark as being noted by relocation warning pass.  */
			SET_BIT(nlist_bitvector, s - start_of_syms);

			errfmt = 0;
			errmsg = g->warning;
			invalidate_line_number = 0;
		}


		/* If errfmt == 0, errmsg has already been defined.  */
		if (errfmt != 0) {
			char           *nm;

			nm = g->name;
			errmsg = (char *) xmalloc(strlen(errfmt) + strlen(nm) + 1);
			sprintf(errmsg, errfmt, nm, data_segment ? "data" : "text");
			if (nm != g->name)
				free(nm);
		}
		address_to_line(RELOC_ADDRESS(reloc) + start_of_segment,
				state_pointer);

		if (current->line >= 0)
			fprintf(outfile, "%s:%d: %s\n", current->filename,
			invalidate_line_number ? 0 : current->line, errmsg);
		else
			fprintf(outfile, "%s: %s\n", current->filename, errmsg);

		if (errfmt != 0)
			free(errmsg);
	}

	free(state_pointer);
}

/*
 * Print on OUTFILE a list of all warnings generated by references and/or
 * definitions in the file ENTRY.  List source file and line number if
 * possible, just the .o file if not.
 */

void
do_file_warnings (entry, outfile)
     struct file_entry *entry;
     FILE *outfile;
{
	int number_of_syms = entry->nsymbols;
	unsigned char *nlist_bitvector = (unsigned char *)
					alloca ((number_of_syms >> 3) + 1);
	struct line_debug_entry *text_scan, *data_scan;
	int	i;
	char	*errfmt, *file_name;
	int	line_number;
	int	dont_allow_symbol_name;

	bzero (nlist_bitvector, (number_of_syms >> 3) + 1);

	/* Read in the files strings if they aren't available */
	if (!entry->strings) {
		int desc;

		entry->strings = (char *) alloca (entry->string_size);
		desc = file_open (entry);
		read_entry_strings (desc, entry);
	}

	if (! entry->is_dynamic) {
		/* Do text warnings based on a scan through the relocation info. */
		do_relocation_warnings (entry, 0, outfile, nlist_bitvector);

		/* Do data warnings based on a scan through the relocation info. */
		do_relocation_warnings (entry, 1, outfile, nlist_bitvector);
	}

	/* Scan through all of the nlist entries in this file and pick up
	anything that the scan through the relocation stuff didn't. */

	text_scan = init_debug_scan (0, entry);
	data_scan = init_debug_scan (1, entry);

	for (i = 0; i < number_of_syms; i++) {
		struct nlist *s;
		struct glosym *g;

	        g = entry->symbols[i].symbol;
		s = &entry->symbols[i].nzlist.nlist;

		if (!(s->n_type & N_EXT))
			continue;

		if (!g->referenced) {
#if 0
			/* Check for undefined shobj symbols */
			struct localsymbol	*lsp;
			register int		type;

			for (lsp = g->dynrefs; lsp; lsp = lsp->next) {
				type = lsp->nlist.n_type;
				if ((type & N_EXT) &&
						type != (N_UNDF | N_EXT)) {
					break;
				}
			}
			if (type == (N_UNDF | N_EXT)) {
				fprintf(stderr,
					"Undefined symbol %s referenced from %s\n",
					g->name,
					get_file_name(entry));
			}
#endif
			continue;
		}

		dont_allow_symbol_name = 0;

		if (list_multiple_defs && g->multiply_defined) {
			errfmt = "Definition of symbol %s (multiply defined)";
			switch (s->n_type) {

			case N_TEXT | N_EXT:
				line_number = address_to_line (s->n_value, text_scan);
				file_name = text_scan[0].filename;
				break;

			case N_DATA | N_EXT:
				line_number = address_to_line (s->n_value, data_scan);
				file_name = data_scan[0].filename;
				break;

			case N_SETA | N_EXT:
			case N_SETT | N_EXT:
			case N_SETD | N_EXT:
			case N_SETB | N_EXT:
				if (g->multiply_defined == 2)
					continue;
				errfmt = "First set element definition of symbol %s (multiply defined)";
				break;

			default:
printf("Multiple def: %s, type %#x\n", g->name, s->n_type);
				/* Don't print out multiple defs at references.*/
				continue;
			}

		} else if (BIT_SET_P (nlist_bitvector, i)) {
			continue;
		} else if (list_unresolved_refs && !g->defined && !g->so_defined) {
			if (g->undef_refs >= MAX_UREFS_PRINTED)
				continue;

			if (++(g->undef_refs) == MAX_UREFS_PRINTED)
				errfmt = "More undefined \"%s\" refs follow";
			else
				errfmt = "Undefined symbol \"%s\" referenced";
			line_number = -1;
		} else if (g->warning) {
			/*
			 * There are two cases in which we don't want to do
			 * this. The first is if this is a definition instead
			 * do a reference. The second is if it's the reference
			 * used by the warning stabs itself.
			 */
			if (s->n_type != (N_EXT | N_UNDF)
					|| (i && (s-1)->n_type == N_WARNING))
				continue;

			errfmt = g->warning;
			line_number = -1;
			dont_allow_symbol_name = 1;
		} else
			continue;

		if (line_number == -1)
			fprintf (outfile, "%s: ", entry->filename);
		else
			fprintf (outfile, "%s:%d: ", file_name, line_number);

		if (dont_allow_symbol_name)
			fprintf (outfile, "%s", errfmt);
		else
			fprintf (outfile, errfmt, g->name);

		fputc ('\n', outfile);
	}
	free (text_scan);
	free (data_scan);
	entry->strings = 0;	/* Since it will dissapear anyway. */
}

int
do_warnings(outfile)
	FILE	*outfile;
{
	list_unresolved_refs = !relocatable_output &&
		(undefined_global_sym_count || undefined_shobj_sym_count);
	list_warning_symbols = warning_count;
	list_multiple_defs = multiple_def_count != 0;

	if (!(list_unresolved_refs ||
			list_warning_symbols || list_multiple_defs))
		/* No need to run this routine */
		return 1;

	if (entry_symbol && !entry_symbol->defined)
		fprintf (outfile, "Undefined entry symbol %s\n",
						entry_symbol->name);
	each_file (do_file_warnings, outfile);

	if (list_unresolved_refs || list_multiple_defs)
		return 0;

	return 1;
}

