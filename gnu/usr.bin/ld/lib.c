/*
 * $Id: lib.c,v 1.9 1994/02/13 20:41:37 jkh Exp $	- library routines
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

static void		linear_library __P((int, struct file_entry *));
static void		symdef_library __P((int, struct file_entry *, int));
static struct file_entry	*decode_library_subfile __P((int,
							struct file_entry *,
							int, int *));

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

static struct file_entry *
decode_library_subfile(desc, library_entry, subfile_offset, length_loc)
	int             desc;
	struct file_entry *library_entry;
	int             subfile_offset;
	int            *length_loc;
{
	int             bytes_read;
	register int    namelen;
	int             member_length, content_length;
	int		starting_offset;
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

	starting_offset = subfile_offset + sizeof hdr1;
	content_length = member_length;

#ifdef AR_EFMT1
	/*
	 * BSD 4.4 extended AR format: #1/<namelen>, with name as the
	 * first <namelen> bytes of the file
	 */
	if (strncmp(hdr1.ar_name, AR_EFMT1, sizeof(AR_EFMT1) - 1) == 0 &&
			isdigit(hdr1.ar_name[sizeof(AR_EFMT1) - 1])) {

		namelen = atoi(&hdr1.ar_name[sizeof(AR_EFMT1) - 1]);
		name = (char *)xmalloc(namelen + 1);
		if (read(desc, name, namelen) != namelen)
			fatal_with_file(
			"malformatted header of archive member in ",
				library_entry);
		name[namelen] = 0;
		content_length -= namelen;
		starting_offset += namelen;
	} else 

#endif
	{
		name = (char *)xmalloc(namelen + 1);
		strncpy(name, hdr1.ar_name, namelen);
		name[namelen] = 0;
	}

	subentry->filename = name;
	subentry->local_sym_name = name;
	subentry->symbols = 0;
	subentry->strings = 0;
	subentry->subfiles = 0;
	subentry->starting_offset = starting_offset;
	subentry->superfile = library_entry;
	subentry->chain = 0;
	subentry->flags = 0;
	subentry->total_size = content_length;

	(*length_loc) = member_length;

	return subentry;
}

static int	subfile_wanted_p __P((struct file_entry *));

/*
 * Search a library that has a __.SYMDEF member. DESC is a descriptor on
 * which the library is open. The file pointer is assumed to point at the
 * __.SYMDEF data. ENTRY is the library's file_entry. MEMBER_LENGTH is the
 * length of the __.SYMDEF data.
 */

static void
symdef_library(desc, entry, member_length)
	int             desc;
	struct file_entry *entry;
	int             member_length;
{
	int            *symdef_data = (int *) xmalloc(member_length);
	register struct ranlib *symdef_base;
	char           *sym_name_base;
	int             nsymdefs;
	int             length_of_strings;
	int             not_finished;
	int             bytes_read;
	register int    i;
	struct file_entry *prev = 0;
	int             prev_offset = 0;

	bytes_read = read(desc, symdef_data, member_length);
	if (bytes_read != member_length)
		fatal_with_file("malformatted __.SYMDEF in ", entry);

	nsymdefs = md_swap_long(*symdef_data) / sizeof(struct ranlib);
	if (nsymdefs < 0 ||
	    nsymdefs * sizeof(struct ranlib) + 2 * sizeof(int) > member_length)
		fatal_with_file("malformatted __.SYMDEF in ", entry);

	symdef_base = (struct ranlib *) (symdef_data + 1);
	length_of_strings = md_swap_long(*(int *) (symdef_base + nsymdefs));

	if (length_of_strings < 0
	    || nsymdefs * sizeof(struct ranlib) + length_of_strings
	    + 2 * sizeof(int) > member_length)
		fatal_with_file("malformatted __.SYMDEF in ", entry);

	sym_name_base = sizeof(int) + (char *) (symdef_base + nsymdefs);

	/* Check all the string indexes for validity.  */
	md_swapin_ranlib_hdr(symdef_base, nsymdefs);
	for (i = 0; i < nsymdefs; i++) {
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

		for (i = 0; (i < nsymdefs &&
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
					(!(sp->flags & GS_REFERENCED) &&
						!sp->sorefs)))
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

				for (j = 0; j < nsymdefs; j++) {
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

static void
linear_library(desc, entry)
	int             desc;
	struct file_entry *entry;
{
	register struct file_entry *prev = 0;
	register int    this_subfile_offset = SARMAG;

	while ((link_mode & FORCEARCHIVE) ||
		undefined_global_sym_count || common_defined_global_count) {

		int				member_length;
		register struct file_entry	*subentry;

		subentry = decode_library_subfile(desc, entry,
					this_subfile_offset, &member_length);

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

static int
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
		register int	type = p->n_type;
		register char	*name = p->n_un.n_strx + entry->strings;
		register symbol	*sp = getsym_soft(name);

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
			if (sp->flags & SP_REFERENCED) {
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
		if ((sp->flags & GS_REFERENCED) && !sp->defined) {
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
				if (!sp->common_size)
					common_defined_global_count++;

				if (sp->common_size < p->n_value)
					sp->common_size = p->n_value;
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
			/*
			 * Check for undefined symbols or commons
			 * in shared objects.
			 */
			struct localsymbol *lsp;
			int wascommon = sp->defined && sp->common_size;
			int iscommon = type == (N_UNDF|N_EXT) && p->n_value;

			if (wascommon) {
				/*
				 * sp was defined as common by shared object.
				 */
				if (iscommon && p->n_value < sp->common_size)
					sp->common_size = p->n_value;
				continue;
			}

			if (sp->sorefs == NULL)
				continue;

			for (lsp = sp->sorefs; lsp; lsp = lsp->next) {
				int type = lsp->nzlist.nlist.n_type;
				if (	(type & N_EXT) &&
					(type & N_STAB) == 0 &&
					type != (N_UNDF | N_EXT))
					break; /* We don't need it */
			}
			if (lsp != NULL) {
				/* There's a real definition */
				if (iscommon)
					/*
					 * But this member wants it to be
					 * a common; ignore it.
					continue;
			}

			if (iscommon) {
				/*
				 * New symbol is common, just takes its
				 * size, but don't load.
				 */
				sp->common_size = p->n_value;
				sp->defined = type;
				continue;
			}

			/*
			 * THIS STILL MISSES the case where one shared
			 * object defines a common and the next defines
			 * more strongly; fix this someday by making
			 * `struct glosym' and enter_global_ref() more
			 * symmetric.
			 */

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
	struct _dynamic			dyn;
	struct section_dispatch_table	sdt;
	struct nlist			*np;
	struct nzlist			*nzp;
	int				n, i, has_nz = 0;

	if (!(entry->flags & E_HEADER_VALID))
		read_header (desc, entry);

	/* Read DYNAMIC structure (first in data segment) */
	lseek (desc,
		text_offset (entry) + entry->header.a_text,
		L_SET);
	if (read(desc, &dyn, sizeof dyn) != sizeof dyn) {
		fatal_with_file (
			"premature eof in data segment of ", entry);
	}
	md_swapin__dynamic(&dyn);

	/* Check version */
	switch (dyn.d_version) {
	default:
		fatal_with_file( "unsupported _DYNAMIC version ", entry);
		break;
	case LD_VERSION_SUN:
		break;
	case LD_VERSION_BSD:
		has_nz = 1;
		break;
	}

	/* Read Section Dispatch Table (from data segment) */
	lseek (desc,
		text_offset(entry) + (long)dyn.d_un.d_sdt -
			(DATA_START(entry->header) - N_DATOFF(entry->header)),
		L_SET);
	if (read(desc, &sdt, sizeof sdt) != sizeof sdt) {
		fatal_with_file( "premature eof in data segment of ", entry);
	}
	md_swapin_section_dispatch_table(&sdt);

	/* Read symbols (text segment) */
	n = sdt.sdt_strings - sdt.sdt_nzlist;
	entry->nsymbols = n /
		(has_nz ? sizeof(struct nzlist) : sizeof(struct nlist));
	nzp = (struct nzlist *)(np = (struct nlist *) alloca (n));
	entry->symbols = (struct localsymbol *)
			xmalloc(entry->nsymbols * sizeof(struct localsymbol));
	lseek(desc, text_offset(entry) + (long)sdt.sdt_nzlist -
			(TEXT_START(entry->header) - N_TXTOFF(entry->header)),
		L_SET);
	if (read(desc, (char *)nzp, n) != n) {
		fatal_with_file(
			"premature eof while reading object symbols ", entry);
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
		entry->symbols[i].entry = entry;
		entry->symbols[i].gotslot_offset = -1;
		entry->symbols[i].flags = 0;
	}

	/* Read strings (text segment) */
	n = entry->string_size = sdt.sdt_str_sz;
	entry->strings = (char *) alloca(n);
	entry->strings_offset = text_offset(entry) + sdt.sdt_strings;
	lseek(desc, entry->strings_offset -
			(TEXT_START(entry->header) - N_TXTOFF(entry->header)),
		L_SET);
	if (read(desc, entry->strings, n) != n) {
		fatal_with_file(
			"premature eof while reading object strings ", entry);
	}
	enter_file_symbols (entry);
	entry->strings = 0;

	/*
	 * Load any subsidiary shared objects.
	 */
	if (sdt.sdt_sods) {
		struct sod		sod;
		off_t			offset;
		struct file_entry	*prev = NULL;

		offset = (off_t)sdt.sdt_sods;
		while (1) {
			struct file_entry *subentry;
			char *libname, name[MAXPATHLEN]; /*XXX*/

			subentry = (struct file_entry *)
				xmalloc(sizeof(struct file_entry));
			bzero(subentry, sizeof(struct file_entry));
			subentry->superfile = entry;

			lseek(desc, offset -
			   (TEXT_START(entry->header) - N_TXTOFF(entry->header)),
				L_SET);
			if (read(desc, &sod, sizeof(sod)) != sizeof(sod)) {
				fatal_with_file(
				"premature eof while reading sod ",
						entry);
			}
			md_swapin_sod(&sod, 1);
			(void)lseek(desc, (off_t)sod.sod_name -
			   (TEXT_START(entry->header) - N_TXTOFF(entry->header)),
			   L_SET);
			(void)read(desc, name, sizeof(name)); /*XXX*/
			if (sod.sod_library) {
				int sod_major = sod.sod_major;
				int sod_minor = sod.sod_minor;

				libname = findshlib(name,
						&sod_major, &sod_minor, 0);
				if (libname == NULL)
					fatal("no shared -l%s.%d.%d available",
					name, sod.sod_major, sod.sod_minor);
				subentry->filename = libname;
				subentry->local_sym_name = concat("-l", name, "");
			} else {
				subentry->filename = strdup(name);
				subentry->local_sym_name = strdup(name);
			}
			read_file_symbols(subentry);

			if (prev)
				prev->chain = subentry;
			else
				entry->subfiles = subentry;
			prev = subentry;
			desc = file_open(entry);
			if ((offset = (off_t)sod.sod_next) == 0)
				break;
		}
	}
#ifdef SUN_COMPAT
	if (link_mode & SILLYARCHIVE) {
		char			*cp, *sa_name;
		char			armag[SARMAG];
		int			fd;
		struct file_entry	*subentry;

		sa_name = strdup(entry->filename);
		if (sa_name == NULL)
			goto out;
		cp = sa_name + strlen(sa_name) - 1;
		while (cp > sa_name) {
			if (!isdigit(*cp) && *cp != '.')
				break;
			--cp;
		}
		if (cp <= sa_name || *cp != 'o') {
			/* Not in `libxxx.so.n.m' form */
			free(sa_name);
			goto out;
		}

		*cp = 'a';
		if ((fd = open(sa_name, O_RDONLY, 0)) < 0)
			goto out;

		/* Read archive magic */
		bzero(armag, SARMAG);
		(void)read(fd, armag, SARMAG);
		(void)close(fd);
		if (strncmp(armag, ARMAG, SARMAG) != 0) {
			error("%s: malformed silly archive",
					get_file_name(entry));
			goto out;
		}

		subentry = (struct file_entry *)
				xmalloc(sizeof(struct file_entry));
		bzero(subentry, sizeof(struct file_entry));

		entry->silly_archive = subentry;
		subentry->superfile = entry;
		subentry->filename = sa_name;
		subentry->local_sym_name = sa_name;
		subentry->flags |= E_IS_LIBRARY;
		search_library(file_open(subentry), subentry);
out:
		;
	}
#endif
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

	if (!(p->flags & E_SEARCH_DYNAMIC))
		goto dot_a;

	fname = findshlib(p->filename, &major, &minor, 1);

	if (fname && (desc = open (fname, O_RDONLY, 0)) > 0) {
		p->filename = fname;
		p->lib_major = major;
		p->lib_minor = minor;
		p->flags &= ~E_SEARCH_DIRS;
		return desc;
	}
	free (fname);

dot_a:
	p->flags &= ~E_SEARCH_DYNAMIC;
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
			p->flags &= ~E_SEARCH_DIRS;
			break;
		}
		free (string);
	}
	return desc;
}

