/*
 * $Id: lib.c,v 1.2 1993/11/09 04:19:00 paul Exp $	- library routines
 */

#include <sys/param.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/time.h>
#include <fcntl.h>
#include <ar.h>
#include <ranlib.h>
#include <a.out.h>
#include <stab.h>
#include <string.h>
#include <dirent.h>

#include "ld.h"

char          **search_dirs;

/* Length of the vector `search_dirs'.  */
int             n_search_dirs;

struct file_entry	*decode_library_subfile();
void			linear_library(), symdef_library();

/*
 * Search the library ENTRY, already open on descriptor DESC. This means
 * deciding which library members to load, making a chain of `struct
 * file_entry' for those members, and entering their global symbols in the
 * hash table.
 */

void
search_library(desc, entry)
	int             desc;
	struct file_entry *entry;
{
	int             member_length;
	register char  *name;
	register struct file_entry *subentry;

	if (!(link_mode & FORCEARCHIVE) && !undefined_global_sym_count)
		return;

	/* Examine its first member, which starts SARMAG bytes in.  */
	subentry = decode_library_subfile(desc, entry, SARMAG, &member_length);
	if (!subentry)
		return;

	name = subentry->filename;
	free(subentry);

	/* Search via __.SYMDEF if that exists, else linearly.  */

	if (!strcmp(name, "__.SYMDEF"))
		symdef_library(desc, entry, member_length);
	else
		linear_library(desc, entry);
}

/*
 * Construct and return a file_entry for a library member. The library's
 * file_entry is library_entry, and the library is open on DESC.
 * SUBFILE_OFFSET is the byte index in the library of this member's header.
 * We store the length of the member into *LENGTH_LOC.
 */

struct file_entry *
decode_library_subfile(desc, library_entry, subfile_offset, length_loc)
	int             desc;
	struct file_entry *library_entry;
	int             subfile_offset;
	int            *length_loc;
{
	int             bytes_read;
	register int    namelen;
	int             member_length;
	register char  *name;
	struct ar_hdr   hdr1;
	register struct file_entry *subentry;

	lseek(desc, subfile_offset, 0);

	bytes_read = read(desc, &hdr1, sizeof hdr1);
	if (!bytes_read)
		return 0;	/* end of archive */

	if (sizeof hdr1 != bytes_read)
		fatal_with_file("malformed library archive ", library_entry);

	if (sscanf(hdr1.ar_size, "%d", &member_length) != 1)
		fatal_with_file("malformatted header of archive member in ", library_entry);

	subentry = (struct file_entry *) xmalloc(sizeof(struct file_entry));
	bzero(subentry, sizeof(struct file_entry));

	for (namelen = 0;
	     namelen < sizeof hdr1.ar_name
	     && hdr1.ar_name[namelen] != 0 && hdr1.ar_name[namelen] != ' '
	     && hdr1.ar_name[namelen] != '/';
	     namelen++);

	name = (char *) xmalloc(namelen + 1);
	strncpy(name, hdr1.ar_name, namelen);
	name[namelen] = 0;

	subentry->filename = name;
	subentry->local_sym_name = name;
	subentry->symbols = 0;
	subentry->strings = 0;
	subentry->subfiles = 0;
	subentry->starting_offset = subfile_offset + sizeof hdr1;
	subentry->superfile = library_entry;
	subentry->library_flag = 0;
	subentry->header_read_flag = 0;
	subentry->just_syms_flag = 0;
	subentry->chain = 0;
	subentry->total_size = member_length;

	(*length_loc) = member_length;

	return subentry;
}

int             subfile_wanted_p();

/*
 * Search a library that has a __.SYMDEF member. DESC is a descriptor on
 * which the library is open. The file pointer is assumed to point at the
 * __.SYMDEF data. ENTRY is the library's file_entry. MEMBER_LENGTH is the
 * length of the __.SYMDEF data.
 */

void
symdef_library(desc, entry, member_length)
	int             desc;
	struct file_entry *entry;
	int             member_length;
{
	int            *symdef_data = (int *) xmalloc(member_length);
	register struct ranlib *symdef_base;
	char           *sym_name_base;
	int             number_of_symdefs;
	int             length_of_strings;
	int             not_finished;
	int             bytes_read;
	register int    i;
	struct file_entry *prev = 0;
	int             prev_offset = 0;

	bytes_read = read(desc, symdef_data, member_length);
	if (bytes_read != member_length)
		fatal_with_file("malformatted __.SYMDEF in ", entry);

	number_of_symdefs = md_swap_long(*symdef_data) / sizeof(struct ranlib);
	if (number_of_symdefs < 0 ||
	    number_of_symdefs * sizeof(struct ranlib) + 2 * sizeof(int) > member_length)
		fatal_with_file("malformatted __.SYMDEF in ", entry);

	symdef_base = (struct ranlib *) (symdef_data + 1);
	length_of_strings = md_swap_long(*(int *) (symdef_base + number_of_symdefs));

	if (length_of_strings < 0
	    || number_of_symdefs * sizeof(struct ranlib) + length_of_strings
	    + 2 * sizeof(int) > member_length)
		fatal_with_file("malformatted __.SYMDEF in ", entry);

	sym_name_base = sizeof(int) + (char *) (symdef_base + number_of_symdefs);

	/* Check all the string indexes for validity.  */
	md_swapin_ranlib_hdr(symdef_base, number_of_symdefs);
	for (i = 0; i < number_of_symdefs; i++) {
		register int    index = symdef_base[i].ran_un.ran_strx;
		if (index < 0 || index >= length_of_strings
		    || (index && *(sym_name_base + index - 1)))
			fatal_with_file("malformatted __.SYMDEF in ", entry);
	}

	/*
	 * Search the symdef data for members to load. Do this until one
	 * whole pass finds nothing to load.
	 */

	not_finished = 1;
	while (not_finished) {

		not_finished = 0;

		/*
		 * Scan all the symbols mentioned in the symdef for ones that
		 * we need. Load the library members that contain such
		 * symbols.
		 */

		for (i = 0; (i < number_of_symdefs &&
					((link_mode & FORCEARCHIVE) ||
					undefined_global_sym_count ||
					common_defined_global_count)); i++) {

			register symbol *sp;
			int             junk;
			register int    j;
			register int    offset = symdef_base[i].ran_off;
			struct file_entry *subentry;


			if (symdef_base[i].ran_un.ran_strx < 0)
				continue;

			sp = getsym_soft(sym_name_base
					 + symdef_base[i].ran_un.ran_strx);

			/*
			 * If we find a symbol that appears to be needed,
			 * think carefully about the archive member that the
			 * symbol is in.
			 */

			/*
			 * Per Mike Karels' recommendation, we no longer load
			 * library files if the only reference(s) that would
			 * be satisfied are 'common' references.  This
			 * prevents some problems with name pollution (e.g. a
			 * global common 'utime' linked to a function).
			 */
			if (!(link_mode & FORCEARCHIVE) &&
					(!sp || sp->defined ||
					(!sp->referenced && !sp->sorefs)) )
				continue;

			/*
			 * Don't think carefully about any archive member
			 * more than once in a given pass.
			 */

			if (prev_offset == offset)
				continue;
			prev_offset = offset;

			/*
			 * Read the symbol table of the archive member.
			 */

			subentry = decode_library_subfile(desc,
						      entry, offset, &junk);
			if (subentry == 0)
				fatal(
				      "invalid offset for %s in symbol table of %s",
				      sym_name_base
					      + symdef_base[i].ran_un.ran_strx,
				      entry->filename);
			read_entry_symbols(desc, subentry);
			subentry->strings = (char *)
				malloc(subentry->string_size);
			read_entry_strings(desc, subentry);

			/*
			 * Now scan the symbol table and decide whether to
			 * load.
			 */

			if (!(link_mode & FORCEARCHIVE) &&
					!subfile_wanted_p(subentry)) {
				free(subentry->symbols);
				free(subentry);
			} else {
				/*
				 * This member is needed; load it. Since we
				 * are loading something on this pass, we
				 * must make another pass through the symdef
				 * data.
				 */

				not_finished = 1;

				read_entry_relocation(desc, subentry);
				enter_file_symbols(subentry);

				if (prev)
					prev->chain = subentry;
				else
					entry->subfiles = subentry;
				prev = subentry;

				/*
				 * Clear out this member's symbols from the
				 * symdef data so that following passes won't
				 * waste time on them.
				 */

				for (j = 0; j < number_of_symdefs; j++) {
					if (symdef_base[j].ran_off == offset)
						symdef_base[j].ran_un.ran_strx = -1;
				}
			}

			/*
			 * We'll read the strings again if we need them
			 * again.
			 */
			free(subentry->strings);
			subentry->strings = 0;
		}
	}

	free(symdef_data);
}

/*
 * Search a library that has no __.SYMDEF. ENTRY is the library's file_entry.
 * DESC is the descriptor it is open on.
 */

void
linear_library(desc, entry)
	int             desc;
	struct file_entry *entry;
{
	register struct file_entry *prev = 0;
	register int    this_subfile_offset = SARMAG;

	while ((link_mode & FORCEARCHIVE) ||
		undefined_global_sym_count || common_defined_global_count) {

		int             member_length;
		register struct file_entry *subentry;

		subentry = decode_library_subfile(desc, entry, this_subfile_offset,
						  &member_length);

		if (!subentry)
			return;

		read_entry_symbols(desc, subentry);
		subentry->strings = (char *) alloca(subentry->string_size);
		read_entry_strings(desc, subentry);

		if (!(link_mode & FORCEARCHIVE) &&
					!subfile_wanted_p(subentry)) {
			free(subentry->symbols);
			free(subentry);
		} else {
			read_entry_relocation(desc, subentry);
			enter_file_symbols(subentry);

			if (prev)
				prev->chain = subentry;
			else
				entry->subfiles = subentry;
			prev = subentry;
			subentry->strings = 0;	/* Since space will dissapear
						 * on return */
		}

		this_subfile_offset += member_length + sizeof(struct ar_hdr);
		if (this_subfile_offset & 1)
			this_subfile_offset++;
	}
}

/*
 * ENTRY is an entry for a library member. Its symbols have been read into
 * core, but not entered. Return nonzero if we ought to load this member.
 */

int
subfile_wanted_p(entry)
	struct file_entry *entry;
{
	struct localsymbol	*lsp, *lspend;
#ifdef DOLLAR_KLUDGE
	register int    dollar_cond = 0;
#endif

	lspend = entry->symbols + entry->nsymbols;

	for (lsp = entry->symbols; lsp < lspend; lsp++) {
		register struct nlist *p = &lsp->nzlist.nlist;
		register int    type = p->n_type;
		register char  *name = p->n_un.n_strx + entry->strings;
		register symbol *sp = getsym_soft(name);

		/*
		 * If the symbol has an interesting definition, we could
		 * potentially want it.
		 */
		if (! (type & N_EXT)
		    || (type == (N_UNDF | N_EXT) && p->n_value == 0

#ifdef DOLLAR_KLUDGE
			&& name[1] != '$'
#endif
			)
#ifdef SET_ELEMENT_P
		    || SET_ELEMENT_P(type)
		    || set_element_prefixed_p(name)
#endif
		)
			continue;


#ifdef DOLLAR_KLUDGE
		if (name[1] == '$') {
			sp = getsym_soft(&name[2]);
			dollar_cond = 1;
			if (!sp)
				continue;
			if (sp->referenced) {
				if (write_map) {
					print_file_name(entry, stdout);
					fprintf(stdout, " needed due to $-conditional %s\n", name);
				}
				return 1;
			}
			continue;
		}
#endif

		/*
		 * If this symbol has not been hashed, we can't be
		 * looking for it.
		 */

		if (!sp)
			continue;

		/*
		 * We don't load a file if it merely satisfies a
		 * common reference (see explanation above in
		 * symdef_library()).
		 */
		if (sp->referenced && !sp->defined) {
			/*
			 * This is a symbol we are looking for.  It
			 * is either not yet defined or defined as a
			 * common.
			 */
#ifdef DOLLAR_KLUDGE
			if (dollar_cond)
				continue;
#endif
			if (type == (N_UNDF | N_EXT)) {
				/*
				 * Symbol being defined as common.
				 * Remember this, but don't load
				 * subfile just for this.
				 */

				/*
				 * If it didn't used to be common, up
				 * the count of common symbols.
				 */
				if (!sp->max_common_size)
					common_defined_global_count++;

				if (sp->max_common_size < p->n_value)
					sp->max_common_size = p->n_value;
				if (!sp->defined)
					undefined_global_sym_count--;
				sp->defined = type;
				continue;
			}
			if (write_map) {
				print_file_name(entry, stdout);
				fprintf(stdout, " needed due to %s\n", sp->name);
			}
			return 1;
		} else {
			struct localsymbol *lsp;
			int             defs = 0;

			/* Check for undefined symbols in shared objects */
			for (lsp = sp->sorefs; lsp; lsp = lsp->next) {
				type = lsp->nzlist.nlist.n_type;
				if (	(type & N_EXT) &&
					(type & N_STAB) == 0 &&
					type != (N_UNDF | N_EXT))
					break; /* We need it */
			}
			if (lsp != NULL)
				continue; /* We don't need it */

			if (write_map) {
				print_file_name(entry, stdout);
				fprintf(stdout, " needed due to shared lib ref %s\n", sp->name);
			}
			return 1;
		}
	}

	return 0;
}

/*
 * Read the symbols of dynamic entity ENTRY into core. Assume it is already
 * open, on descriptor DESC.
 */
void
read_shared_object (desc, entry)
     struct file_entry *entry;
     int desc;
{
	struct link_dynamic	dyn;
	struct link_dynamic_2	dyn2;
	struct nlist		*np;
	struct nzlist		*nzp;
	int			n, i, has_nz = 0;

	if (!entry->header_read_flag)
		read_header (desc, entry);

	/* Read DYNAMIC structure (first in data segment) */
	lseek (desc,
		text_offset (entry) + entry->header.a_text,
		L_SET);
	if (read(desc, &dyn, sizeof dyn) != sizeof dyn) {
		fatal_with_file (
			"premature eof in data segment of ", entry);
	}
	md_swapin_link_dynamic(&dyn);

	/* Check version */
	switch (dyn.ld_version) {
	default:
		fatal_with_file( "unsupported _DYNAMIC version ", entry);
		break;
	case LD_VERSION_SUN:
		break;
	case LD_VERSION_BSD:
		has_nz = 1;
		break;
	}

	/* Read link_dynamic_2 struct (from data segment) */
	lseek (desc,
		text_offset(entry) + dyn.ld_un.ld_2,
		L_SET);
	if (read(desc, &dyn2, sizeof dyn2) != sizeof dyn2) {
		fatal_with_file( "premature eof in data segment of ", entry);
	}
	md_swapin_link_dynamic_2(&dyn2);

	/* Read symbols (text segment) */
	n = dyn2.ld_strings - dyn2.ld_symbols;
	entry->nsymbols = n /
		(has_nz ? sizeof(struct nzlist) : sizeof(struct nlist));
	nzp = (struct nzlist *)(np = (struct nlist *) alloca (n));
	entry->symbols = (struct localsymbol *)
			xmalloc(entry->nsymbols * sizeof(struct localsymbol));
	lseek(desc, text_offset (entry) + dyn2.ld_symbols, L_SET);
	if (read(desc, (char *)nzp, n) != n) {
		fatal_with_file(
			"premature eof while reading dyn syms ", entry);
	}
	if (has_nz)
		md_swapin_zsymbols(nzp, entry->nsymbols);
	else
		md_swapin_symbols(np, entry->nsymbols);

	/* Convert to structs localsymbol */
	for (i = 0; i < entry->nsymbols; i++) {
		if (has_nz) {
			entry->symbols[i].nzlist = *nzp++;
		} else {
			entry->symbols[i].nzlist.nlist = *np++;
			entry->symbols[i].nzlist.nz_size = 0;
		}
		entry->symbols[i].symbol = NULL;
		entry->symbols[i].next = NULL;
		entry->symbols[i].gotslot_offset = -1;
		entry->symbols[i].gotslot_claimed = 0;
		entry->symbols[i].rename = 0;
	}

	/* Read strings (text segment) */
	n = entry->string_size = dyn2.ld_str_sz;
	entry->strings = (char *) alloca(n);
	entry->strings_offset = text_offset (entry) + dyn2.ld_strings;
	lseek(desc, entry->strings_offset, L_SET);
	if (read(desc, entry->strings, n) != n) {
		fatal_with_file(
			"premature eof while reading dyn strings ", entry);
	}
	enter_file_symbols (entry);
	entry->strings = 0;

	/* TODO: examine needed shared objects */
	if (dyn2.ld_need) {
	}
}

#undef major
#undef minor

int
findlib(p)
struct file_entry	*p;
{
	int		desc;
	int		i;
	int		len;
	int		major = -1, minor = -1;
	char		*cp, *fname = NULL;

	if (p->search_dynamic_flag == 0)
		goto dot_a;

	fname = findshlib(p->filename, &major, &minor);

	if (fname && (desc = open (fname, O_RDONLY, 0)) > 0) {
		p->filename = fname;
		p->lib_major = major;
		p->lib_minor = minor;
		p->search_dirs_flag = 0;
		return desc;
	}
	free (fname);

dot_a:
	p->search_dynamic_flag = 0;
	if (cp = strrchr(p->filename, '/')) {
		*cp++ = '\0';
		fname = concat(concat(p->filename, "/lib", cp), ".a", "");
		*(--cp) = '/';
	} else
		fname = concat("lib", p->filename, ".a");

	for (i = 0; i < n_search_dirs; i++) {
		register char *string
			= concat (search_dirs[i], "/", fname);
		desc = open (string, O_RDONLY, 0);
		if (desc > 0) {
			p->filename = string;
			p->search_dirs_flag = 0;
			break;
		}
		free (string);
	}
	return desc;
}

