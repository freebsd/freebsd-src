/* Renesas / SuperH specific support for Symbian 32-bit ELF files
   Copyright 2004, 2005, 2006
   Free Software Foundation, Inc.
   Contributed by Red Hat

   This file is part of BFD, the Binary File Descriptor library.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street - Fifth Floor, Boston, MA 02110-1301, USA.  */

/* Stop elf32-sh.c from defining any target vectors.  */
#define SH_TARGET_ALREADY_DEFINED
#define sh_find_elf_flags           sh_symbian_find_elf_flags
#define sh_elf_get_flags_from_mach  sh_symbian_elf_get_flags_from_mach 
#include "elf32-sh.c"


//#define SYMBIAN_DEBUG 1
#define SYMBIAN_DEBUG 0

#define DIRECTIVE_HEADER	"#<SYMEDIT>#\n"
#define DIRECTIVE_IMPORT	"IMPORT "
#define DIRECTIVE_EXPORT	"EXPORT "
#define DIRECTIVE_AS		"AS "

/* Macro to advance 's' until either it reaches 'e' or the
   character pointed to by 's' is equal to 'c'.  If 'e' is
   reached and SYMBIAN_DEBUG is enabled then the error message 'm'
   is displayed.  */
#define SKIP_UNTIL(s,e,c,m)					\
  do								\
    {								\
      while (s < e && *s != c)					\
	++ s;							\
      if (s >= e)						\
	{							\
          if (SYMBIAN_DEBUG)					\
	    fprintf (stderr, "Corrupt directive: %s\n", m);	\
	  result = FALSE;					\
	}							\
    }								\
  while (0);							\
  if (!result)							\
     break;

/* Like SKIP_UNTIL except there are two terminator characters
   c1 and c2.  */
#define SKIP_UNTIL2(s,e,c1,c2,m)				\
  do								\
    {								\
      while (s < e && *s != c1 && *s != c2)			\
	++ s;							\
      if (s >= e)						\
	{							\
          if (SYMBIAN_DEBUG)					\
	    fprintf (stderr, "Corrupt directive: %s\n", m);	\
	  result = FALSE;					\
	}							\
    }								\
  while (0);							\
  if (!result)							\
     break;

/* Macro to advance 's' until either it reaches 'e' or the
   character pointed to by 's' is not equal to 'c'.  If 'e'
   is reached and SYMBIAN_DEBUG is enabled then the error message
   'm' is displayed.  */
#define SKIP_WHILE(s,e,c,m)					\
  do								\
    {								\
      while (s < e && *s == c)					\
	++ s;							\
      if (s >= e)						\
	{							\
          if (SYMBIAN_DEBUG)					\
	    fprintf (stderr, "Corrupt directive: %s\n", m);	\
	  result = FALSE;					\
	}							\
    }								\
  while (0);							\
  if (!result)							\
     break;


typedef struct symbol_rename
{
  struct symbol_rename *       next;
  char *                       current_name;
  char *                       new_name;
  struct elf_link_hash_entry * current_hash;
  unsigned long                new_symndx;
}
symbol_rename;

static symbol_rename * rename_list = NULL;

/* Accumulate a list of symbols to be renamed.  */

static bfd_boolean
sh_symbian_import_as (struct bfd_link_info *info, bfd * abfd,
		      char * current_name, char * new_name)
{
  struct elf_link_hash_entry * new_hash;
  symbol_rename * node;

  if (SYMBIAN_DEBUG)
    fprintf (stderr, "IMPORT '%s' AS '%s'\n", current_name, new_name);

  for (node = rename_list; node; node = node->next)
    if (strcmp (node->current_name, current_name) == 0)
      {
	if (strcmp (node->new_name, new_name) == 0)
	  /* Already added to rename list.  */
	  return TRUE;

	bfd_set_error (bfd_error_invalid_operation);
	_bfd_error_handler (_("%B: IMPORT AS directive for %s conceals previous IMPORT AS"),
			    abfd, current_name);
	return FALSE;	    
      }

  if ((node = bfd_malloc (sizeof * node)) == NULL)
    {
      if (SYMBIAN_DEBUG)
	fprintf (stderr, "IMPORT AS: No mem for new rename node\n");
      return FALSE;
    }

  if ((node->current_name = bfd_malloc (strlen (current_name) + 1)) == NULL)
    {
      if (SYMBIAN_DEBUG)
	fprintf (stderr, "IMPORT AS: No mem for current name field in rename node\n");
      free (node);
      return FALSE;
    }
  else
    strcpy (node->current_name, current_name);
  
  if ((node->new_name = bfd_malloc (strlen (new_name) + 1)) == NULL)
    {
      if (SYMBIAN_DEBUG)
	fprintf (stderr, "IMPORT AS: No mem for new name field in rename node\n");
      free (node->current_name);
      free (node);
      return FALSE;
    }
  else
    strcpy (node->new_name, new_name);

  node->next = rename_list;
  node->current_hash = NULL;
  node->new_symndx = 0;
  rename_list = node;

  new_hash = elf_link_hash_lookup (elf_hash_table (info), node->new_name, TRUE, FALSE, TRUE);
  bfd_elf_link_record_dynamic_symbol (info, new_hash);
  if (new_hash->root.type == bfd_link_hash_new)
    new_hash->root.type = bfd_link_hash_undefined;

  return TRUE;
}


static bfd_boolean
sh_symbian_import (bfd * abfd ATTRIBUTE_UNUSED, char * name)
{
  if (SYMBIAN_DEBUG)
    fprintf (stderr, "IMPORT '%s'\n", name);

  /* XXX: Generate an import somehow ?  */

  return TRUE;
}

static bfd_boolean
sh_symbian_export (bfd * abfd ATTRIBUTE_UNUSED, char * name)
{
  if (SYMBIAN_DEBUG)
    fprintf (stderr, "EXPORT '%s'\n", name);

  /* XXX: Generate an export somehow ?  */

  return TRUE;
}

/* Process any magic embedded commands in the .directive. section.
   Returns TRUE upon sucecss, but if it fails it sets bfd_error and
   returns FALSE.  */

static bfd_boolean
sh_symbian_process_embedded_commands (struct bfd_link_info *info, bfd * abfd,
				      asection * sec, bfd_byte * contents)
{
  char *s;
  char *e;
  bfd_boolean result = TRUE;
  bfd_size_type sz = sec->rawsize ? sec->rawsize : sec->size;

  for (s = (char *) contents, e = s + sz; s < e;)
    {
      char * directive = s;

      switch (*s)
	{
	  /* I want to use "case DIRECTIVE_HEADER [0]:" here but gcc won't let me :-(  */
	case '#':
	  if (strcmp (s, DIRECTIVE_HEADER))
	    result = FALSE;
	  else
	    /* Just ignore the header.
	       XXX: Strictly speaking we ought to check that the header
	       is present and that it is the first thing in the file.  */
	    s += strlen (DIRECTIVE_HEADER) + 1;
	  break;

	case 'I':
	  if (! CONST_STRNEQ (s, DIRECTIVE_IMPORT))
	    result = FALSE;
	  else
	    {
	      char * new_name;
	      char * new_name_end;
	      char   name_end_char;

	      /* Skip the IMPORT directive.  */
	      s += strlen (DIRECTIVE_IMPORT);

	      new_name = s;
	      /* Find the end of the new name.  */
	      while (s < e && *s != ' ' && *s != '\n')
		++ s;
	      if (s >= e)
		{
		  /* We have reached the end of the .directive section
		     without encountering a string terminator.  This is
		     allowed for IMPORT directives.  */
		  new_name_end   = e - 1;
		  name_end_char  = * new_name_end;
		  * new_name_end = 0;
		  result = sh_symbian_import (abfd, new_name);
		  * new_name_end = name_end_char;
		  break;
		}

	      /* Remember where the name ends.  */
	      new_name_end = s;
	      /* Skip any whitespace before the 'AS'.  */
	      SKIP_WHILE (s, e, ' ', "IMPORT: Name just followed by spaces");
	      /* Terminate the new name.  (Do this after skiping...)  */
	      name_end_char = * new_name_end;
	      * new_name_end = 0;

	      /* Check to see if 'AS '... is present.  If so we have an
		 IMPORT AS directive, otherwise we have an IMPORT directive.  */
	      if (! CONST_STRNEQ (s, DIRECTIVE_AS))
		{
		  /* Skip the new-line at the end of the name.  */
		  if (SYMBIAN_DEBUG && name_end_char != '\n')
		    fprintf (stderr, "IMPORT: No newline at end of directive\n");
		  else
		    s ++;

		  result = sh_symbian_import (abfd, new_name);

		  /* Skip past the NUL character.  */
		  if (* s ++ != 0)
		    {
		      if (SYMBIAN_DEBUG)
			fprintf (stderr, "IMPORT: No NUL at end of directive\n");
		    }
		}
	      else
		{
		  char * current_name;
		  char * current_name_end;
		  char   current_name_end_char;

		  /* Skip the 'AS '.  */
		  s += strlen (DIRECTIVE_AS);
		  /* Skip any white space after the 'AS '.  */
		  SKIP_WHILE (s, e, ' ', "IMPORT AS: Nothing after AS");
		  current_name = s;
		  /* Find the end of the current name.  */
		  SKIP_UNTIL2 (s, e, ' ', '\n', "IMPORT AS: No newline at the end of the current name");
		  /* Skip (backwards) over spaces at the end of the current name.  */
		  current_name_end = s;
		  current_name_end_char = * current_name_end;

		  SKIP_WHILE (s, e, ' ', "IMPORT AS: Current name just followed by spaces");
		  /* Skip past the newline character.  */
		  if (* s ++ != '\n')
		    if (SYMBIAN_DEBUG)
		      fprintf (stderr, "IMPORT AS: No newline at end of directive\n");

		  /* Terminate the current name after having performed the skips.  */
		  * current_name_end = 0;

		  result = sh_symbian_import_as (info, abfd, current_name, new_name);

		  /* The next character should be a NUL.  */
		  if (* s != 0)
		    {
		      if (SYMBIAN_DEBUG)
			fprintf (stderr, "IMPORT AS: Junk at end of directive\n");
		      result = FALSE;
		    }
		  s ++;

		  * current_name_end = current_name_end_char;
		}

	      /* Restore the characters we overwrote, since
		 the .directive section will be emitted.  */
	      * new_name_end = name_end_char;
	    }
	  break;

	case 'E':
	  if (! CONST_STRNEQ (s, DIRECTIVE_EXPORT))
	    result = FALSE;
	  else
	    {
	      char * name;
	      char * name_end;
	      char   name_end_char;

	      /* Skip the directive.  */
	      s += strlen (DIRECTIVE_EXPORT);
	      name = s;
	      /* Find the end of the name to be exported.  */
	      SKIP_UNTIL (s, e, '\n', "EXPORT: no newline at end of directive");
	      /* Skip (backwards) over spaces at end of exported name.  */
	      for (name_end = s; name_end[-1] == ' '; name_end --)
		;
	      /* name_end now points at the first character after the
		 end of the exported name, so we can termiante it  */
	      name_end_char = * name_end;
	      * name_end = 0;
	      /* Skip passed the newline character.  */
	      s ++;

	      result = sh_symbian_export (abfd, name);

	      /* The next character should be a NUL.  */
	      if (* s != 0)
		{
		  if (SYMBIAN_DEBUG)
		    fprintf (stderr, "EXPORT: Junk at end of directive\n");
		  result = FALSE;
		}
	      s++;

	      /* Restore the character we deleted.  */
	      * name_end = name_end_char;
	    }
	  break;

	default:
	  result = FALSE;
	  break;
	}

      if (! result)
	{
	  if (SYMBIAN_DEBUG)
	    fprintf (stderr, "offset into .directive section: %ld\n",
		     (long) (directive - (char *) contents));
	  
	  bfd_set_error (bfd_error_invalid_operation);
	  _bfd_error_handler (_("%B: Unrecognised .directive command: %s"),
			      abfd, directive);
	  break;
	}
    }

  return result;
}


/* Scan a bfd for a .directive section, and if found process it.
   Returns TRUE upon success, FALSE otherwise.  */
bfd_boolean bfd_elf32_sh_symbian_process_directives (struct bfd_link_info *info, bfd * abfd);

bfd_boolean
bfd_elf32_sh_symbian_process_directives (struct bfd_link_info *info, bfd * abfd)
{
  bfd_boolean result = FALSE;
  bfd_byte *  contents;
  asection *  sec = bfd_get_section_by_name (abfd, ".directive");
  bfd_size_type sz;

  if (!sec)
    return TRUE;

  sz = sec->rawsize ? sec->rawsize : sec->size;
  contents = bfd_malloc (sz);

  if (!contents)
    bfd_set_error (bfd_error_no_memory);
  else 
    {
      if (bfd_get_section_contents (abfd, sec, contents, 0, sz))
	result = sh_symbian_process_embedded_commands (info, abfd, sec, contents);
      free (contents);
    }

  return result;
}

/* Intercept the normal sh_relocate_section() function
   and magle the relocs to allow for symbol renaming.  */

static bfd_boolean
sh_symbian_relocate_section (bfd *                  output_bfd,
			     struct bfd_link_info * info,
			     bfd *                  input_bfd,
			     asection *             input_section,
			     bfd_byte *             contents,
			     Elf_Internal_Rela *    relocs,
			     Elf_Internal_Sym *     local_syms,
			     asection **            local_sections)
{
  /* When performing a final link we implement the IMPORT AS directives.  */
  if (!info->relocatable)
    {
      Elf_Internal_Rela *            rel;
      Elf_Internal_Rela *            relend;
      Elf_Internal_Shdr *            symtab_hdr;
      struct elf_link_hash_entry **  sym_hashes;
      struct elf_link_hash_entry **  sym_hashes_end;
      struct elf_link_hash_table *   hash_table;
      symbol_rename *                ptr;
      bfd_size_type                  num_global_syms;
      unsigned long		     num_local_syms;
      
      BFD_ASSERT (! elf_bad_symtab (input_bfd));
 
      symtab_hdr       = & elf_tdata (input_bfd)->symtab_hdr;
      hash_table       = elf_hash_table (info);
      num_local_syms   = symtab_hdr->sh_info;
      num_global_syms  = symtab_hdr->sh_size / sizeof (Elf32_External_Sym);
      num_global_syms -= num_local_syms;
      sym_hashes       = elf_sym_hashes (input_bfd);
      sym_hashes_end   = sym_hashes + num_global_syms;

      /* First scan the rename table, caching the hash entry and the new index.  */
      for (ptr = rename_list; ptr; ptr = ptr->next)
	{
	  struct elf_link_hash_entry *   new_hash;
	  struct elf_link_hash_entry **  h;

	  ptr->current_hash = elf_link_hash_lookup (hash_table, ptr->current_name, FALSE, FALSE, TRUE);

	  if (ptr->current_hash == NULL)
	    {
	      if (SYMBIAN_DEBUG)
		fprintf (stderr, "IMPORT AS: current symbol '%s' does not exist\n", ptr->current_name);
	      continue;
	    }
	  
	  new_hash = elf_link_hash_lookup (hash_table, ptr->new_name, FALSE, FALSE, TRUE);

	  /* If we could not find the symbol then it is a new, undefined symbol.
	     Symbian want this behaviour - ie they want to be able to rename the
	     reference in a reloc from one undefined symbol to another, new and
	     undefined symbol.  So we create that symbol here.  */
	  if (new_hash == NULL)
	    {
	      asection *                     psec = bfd_und_section_ptr;
	      Elf_Internal_Sym               new_sym;
	      bfd_vma                        new_value = 0;
	      bfd_boolean                    skip;
	      bfd_boolean                    override;
	      bfd_boolean                    type_change_ok;
	      bfd_boolean                    size_change_ok;

	      new_sym.st_value = 0;
	      new_sym.st_size  = 0;
	      new_sym.st_name  = -1;
	      new_sym.st_info  = ELF_ST_INFO (STB_GLOBAL, STT_FUNC);
	      new_sym.st_other = ELF_ST_VISIBILITY (STV_DEFAULT);
	      new_sym.st_shndx = SHN_UNDEF;

	      if (! _bfd_elf_merge_symbol (input_bfd, info,
					   ptr->new_name, & new_sym,
					   & psec, & new_value, NULL,
					   & new_hash, & skip,
					   & override, & type_change_ok,
					   & size_change_ok))
		{
		  _bfd_error_handler (_("%B: Failed to add renamed symbol %s"),
				      input_bfd, ptr->new_name);
		  continue;
		}
	      /* XXX - should we check psec, skip, override etc ?  */

	      new_hash->root.type = bfd_link_hash_undefined;

	      /* Allow the symbol to become local if necessary.  */
	      if (new_hash->dynindx == -1)
		new_hash->def_regular = 1;

	      if (SYMBIAN_DEBUG)
		fprintf (stderr, "Created new symbol %s\n", ptr->new_name);
	    }

	  /* Convert the new_hash value into a index into the table of symbol hashes.  */
	  for (h = sym_hashes; h < sym_hashes_end; h ++)
	    {
	      if (* h == new_hash)
		{
		  ptr->new_symndx = h - sym_hashes + num_local_syms;
		  if (SYMBIAN_DEBUG)
		    fprintf (stderr, "Converted new hash to index of %ld\n", ptr->new_symndx);
		  break;
		}
	    }
	  /* If the new symbol is not in the hash table then it must be
	     because it is one of the newly created undefined symbols
	     manufactured above.  So we extend the sym has table here to
	     include this extra symbol.  */
	  if (h == sym_hashes_end)
	    {
	      struct elf_link_hash_entry **  new_sym_hashes;

	      /* This is not very efficient, but it works.  */
	      ++ num_global_syms;
	      new_sym_hashes = bfd_alloc (input_bfd, num_global_syms * sizeof * sym_hashes);
	      if (new_sym_hashes == NULL)
		{
		  if (SYMBIAN_DEBUG)
		    fprintf (stderr, "Out of memory extending hash table\n");
		  continue;
		}
	      memcpy (new_sym_hashes, sym_hashes, (num_global_syms - 1) * sizeof * sym_hashes);
	      new_sym_hashes[num_global_syms - 1] = new_hash;
	      elf_sym_hashes (input_bfd) = sym_hashes = new_sym_hashes;
	      sym_hashes_end = sym_hashes + num_global_syms;
	      symtab_hdr->sh_size  = (num_global_syms + num_local_syms) * sizeof (Elf32_External_Sym);

	      ptr->new_symndx = num_global_syms - 1 + num_local_syms;

	      if (SYMBIAN_DEBUG)
		fprintf (stderr, "Extended symbol hash table to insert new symbol as index %ld\n",
			 ptr->new_symndx);
	    }
	}

      /* Walk the reloc list looking for references to renamed symbols.
	 When we find one, we alter the index in the reloc to point to the new symbol.  */
      for (rel = relocs, relend = relocs + input_section->reloc_count;
	   rel < relend;
	   rel ++)
	{
	  int                          r_type;
	  unsigned long                r_symndx;
	  struct elf_link_hash_entry * h;
      
	  r_symndx = ELF32_R_SYM (rel->r_info);
	  r_type = ELF32_R_TYPE (rel->r_info);

	  /* Ignore unused relocs.  */
	  if ((r_type >= (int) R_SH_GNU_VTINHERIT
	       && r_type <= (int) R_SH_LABEL)
	      || r_type == (int) R_SH_NONE
	      || r_type < 0
	      || r_type >= R_SH_max)
	    continue;

	  /* Ignore relocs against local symbols.  */
	  if (r_symndx < num_local_syms)
	    continue;

	  BFD_ASSERT (r_symndx < (num_global_syms + num_local_syms));
	  h = sym_hashes[r_symndx - num_local_syms];
	  BFD_ASSERT (h != NULL);

	  while (   h->root.type == bfd_link_hash_indirect
		 || h->root.type == bfd_link_hash_warning)
	    h = (struct elf_link_hash_entry *) h->root.u.i.link;

	  /* If the symbol is defined there is no need to rename it.
	     XXX - is this true ?  */
	  if (   h->root.type == bfd_link_hash_defined
	      || h->root.type == bfd_link_hash_defweak
	      || h->root.type == bfd_link_hash_undefweak)
	    continue;

	  for (ptr = rename_list; ptr; ptr = ptr->next)
	    if (h == ptr->current_hash)
	      {
		BFD_ASSERT (ptr->new_symndx);
		if (SYMBIAN_DEBUG)
		  fprintf (stderr, "convert reloc %lx from using index %ld to using index %ld\n",
			   (long) rel->r_info, (long) ELF32_R_SYM (rel->r_info), ptr->new_symndx);
		rel->r_info = ELF32_R_INFO (ptr->new_symndx, r_type);
		break;
	      }
	}
    }
  
  return sh_elf_relocate_section (output_bfd, info, input_bfd, input_section,
				  contents, relocs, local_syms, local_sections);
}

static bfd_boolean
sh_symbian_check_directives (bfd *abfd, struct bfd_link_info *info)
{
  return bfd_elf32_sh_symbian_process_directives (info, abfd);
}

#define TARGET_LITTLE_SYM	bfd_elf32_shl_symbian_vec
#define TARGET_LITTLE_NAME      "elf32-shl-symbian"

#undef  elf_backend_relocate_section
#define elf_backend_relocate_section	sh_symbian_relocate_section
#undef  elf_backend_check_directives
#define elf_backend_check_directives    sh_symbian_check_directives

#include "elf32-target.h"
