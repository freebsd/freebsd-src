/* Linker file opening and searching.
   Copyright 1991, 1992, 1993, 1994, 1995, 1998, 1999, 2000, 2001, 2002
   Free Software Foundation, Inc.

This file is part of GLD, the Gnu Linker.

GLD is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GLD is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GLD; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA
02111-1307, USA.  */

/* ldfile.c:  look after all the file stuff.  */

#include "bfd.h"
#include "sysdep.h"
#include "bfdlink.h"
#include "safe-ctype.h"
#include "ld.h"
#include "ldmisc.h"
#include "ldexp.h"
#include "ldlang.h"
#include "ldfile.h"
#include "ldmain.h"
#include "ldgram.h"
#include "ldlex.h"
#include "ldemul.h"
#include "libiberty.h"

const char *ldfile_input_filename;
boolean ldfile_assumed_script = false;
const char *ldfile_output_machine_name = "";
unsigned long ldfile_output_machine;
enum bfd_architecture ldfile_output_architecture;
search_dirs_type *search_head;

#ifndef MPW
#ifdef VMS
char *slash = "";
#else
#if defined (_WIN32) && ! defined (__CYGWIN32__)
char *slash = "\\";
#else
char *slash = "/";
#endif
#endif
#else /* MPW */
/* The MPW path char is a colon.  */
char *slash = ":";
#endif /* MPW */

/* LOCAL */

static search_dirs_type **search_tail_ptr = &search_head;

typedef struct search_arch {
  char *name;
  struct search_arch *next;
} search_arch_type;

static search_arch_type *search_arch_head;
static search_arch_type **search_arch_tail_ptr = &search_arch_head;

static FILE *try_open PARAMS ((const char *name, const char *exten));

void
ldfile_add_library_path (name, cmdline)
     const char *name;
     boolean cmdline;
{
  search_dirs_type *new;

  if (!cmdline && config.only_cmd_line_lib_dirs)
    return;

  new = (search_dirs_type *) xmalloc (sizeof (search_dirs_type));
  new->next = NULL;
  new->name = name;
  new->cmdline = cmdline;
  *search_tail_ptr = new;
  search_tail_ptr = &new->next;
}

/* Try to open a BFD for a lang_input_statement.  */

boolean
ldfile_try_open_bfd (attempt, entry)
     const char *attempt;
     lang_input_statement_type *entry;
{
  entry->the_bfd = bfd_openr (attempt, entry->target);

  if (trace_file_tries)
    {
      if (entry->the_bfd == NULL)
	info_msg (_("attempt to open %s failed\n"), attempt);
      else
	info_msg (_("attempt to open %s succeeded\n"), attempt);
    }

  if (entry->the_bfd == NULL)
    {
      if (bfd_get_error () == bfd_error_invalid_target)
	einfo (_("%F%P: invalid BFD target `%s'\n"), entry->target);
      return false;
    }

  /* If we are searching for this file, see if the architecture is
     compatible with the output file.  If it isn't, keep searching.
     If we can't open the file as an object file, stop the search
     here.  */

  if (entry->search_dirs_flag)
    {
      bfd *check;

      if (bfd_check_format (entry->the_bfd, bfd_archive))
	check = bfd_openr_next_archived_file (entry->the_bfd, NULL);
      else
	check = entry->the_bfd;

      if (check != NULL)
	{
	  if (! bfd_check_format (check, bfd_object))
	    return true;

	  if ((bfd_arch_get_compatible (check, output_bfd) == NULL)
	      /* XCOFF archives can have 32 and 64 bit objects */
	      && ! (bfd_get_flavour (check) == bfd_target_xcoff_flavour
		    && bfd_get_flavour (output_bfd) == bfd_target_xcoff_flavour
		    && bfd_check_format (entry->the_bfd, bfd_archive)))
	    {
	      einfo (_("%P: skipping incompatible %s when searching for %s\n"),
		     attempt, entry->local_sym_name);
	      bfd_close (entry->the_bfd);
	      entry->the_bfd = NULL;
	      return false;
	    }
	}
    }

  return true;
}

/* Search for and open the file specified by ENTRY.  If it is an
   archive, use ARCH, LIB and SUFFIX to modify the file name.  */

boolean
ldfile_open_file_search (arch, entry, lib, suffix)
     const char *arch;
     lang_input_statement_type *entry;
     const char *lib;
     const char *suffix;
{
  search_dirs_type *search;

  /* If this is not an archive, try to open it in the current
     directory first.  */
  if (! entry->is_archive)
    {
      if (ldfile_try_open_bfd (entry->filename, entry))
	return true;
    }

  for (search = search_head;
       search != (search_dirs_type *) NULL;
       search = search->next)
    {
      char *string;

      if (entry->dynamic && ! link_info.relocateable)
	{
	  if (ldemul_open_dynamic_archive (arch, search, entry))
	    return true;
	}

      string = (char *) xmalloc (strlen (search->name)
				 + strlen (slash)
				 + strlen (lib)
				 + strlen (entry->filename)
				 + strlen (arch)
				 + strlen (suffix)
				 + 1);

      if (entry->is_archive)
	sprintf (string, "%s%s%s%s%s%s", search->name, slash,
		 lib, entry->filename, arch, suffix);
      else if (entry->filename[0] == '/' || entry->filename[0] == '.'
#if defined (__MSDOS__) || defined (_WIN32)
	       || entry->filename[0] == '\\'
	       || (ISALPHA (entry->filename[0])
	           && entry->filename[1] == ':')
#endif
	  )
	strcpy (string, entry->filename);
      else
	sprintf (string, "%s%s%s", search->name, slash, entry->filename);

      if (ldfile_try_open_bfd (string, entry))
	{
	  entry->filename = string;
	  return true;
	}

      free (string);
    }

  return false;
}

/* Open the input file specified by ENTRY.  */

void
ldfile_open_file (entry)
     lang_input_statement_type *entry;
{
  if (entry->the_bfd != NULL)
    return;

  if (! entry->search_dirs_flag)
    {
      if (ldfile_try_open_bfd (entry->filename, entry))
	return;
      if (strcmp (entry->filename, entry->local_sym_name) != 0)
	einfo (_("%F%P: cannot open %s for %s: %E\n"),
	       entry->filename, entry->local_sym_name);
      else
	einfo (_("%F%P: cannot open %s: %E\n"), entry->local_sym_name);
    }
  else
    {
      search_arch_type *arch;
      boolean found = false;

      /* Try to open <filename><suffix> or lib<filename><suffix>.a */
      for (arch = search_arch_head;
	   arch != (search_arch_type *) NULL;
	   arch = arch->next)
	{
	  found = ldfile_open_file_search (arch->name, entry, "lib", ".a");
	  if (found)
	    break;
#ifdef VMS
	  found = ldfile_open_file_search (arch->name, entry, ":lib", ".a");
	  if (found)
	    break;
#endif
	  found = ldemul_find_potential_libraries (arch->name, entry);
	  if (found)
	    break;
	}

      /* If we have found the file, we don't need to search directories
	 again.  */
      if (found)
	entry->search_dirs_flag = false;
      else
	einfo (_("%F%P: cannot find %s\n"), entry->local_sym_name);
    }
}

/* Try to open NAME; if that fails, try NAME with EXTEN appended to it.  */

static FILE *
try_open (name, exten)
     const char *name;
     const char *exten;
{
  FILE *result;
  char buff[1000];

  result = fopen (name, "r");

  if (trace_file_tries)
    {
      if (result == NULL)
	info_msg (_("cannot find script file %s\n"), name);
      else
	info_msg (_("opened script file %s\n"), name);
    }

  if (result != NULL)
    return result;

  if (*exten)
    {
      sprintf (buff, "%s%s", name, exten);
      result = fopen (buff, "r");

      if (trace_file_tries)
	{
	  if (result == NULL)
	    info_msg (_("cannot find script file %s\n"), buff);
	  else
	    info_msg (_("opened script file %s\n"), buff);
	}
    }

  return result;
}

/* Try to open NAME; if that fails, look for it in any directories
   specified with -L, without and with EXTEND apppended.  */

FILE *
ldfile_find_command_file (name, extend)
     const char *name;
     const char *extend;
{
  search_dirs_type *search;
  FILE *result;
  char buffer[1000];

  /* First try raw name.  */
  result = try_open (name, "");
  if (result == (FILE *) NULL)
    {
      /* Try now prefixes.  */
      for (search = search_head;
	   search != (search_dirs_type *) NULL;
	   search = search->next)
	{
	  sprintf (buffer, "%s%s%s", search->name, slash, name);

	  result = try_open (buffer, extend);
	  if (result)
	    break;
	}
    }

  return result;
}

void
ldfile_open_command_file (name)
     const char *name;
{
  FILE *ldlex_input_stack;
  ldlex_input_stack = ldfile_find_command_file (name, "");

  if (ldlex_input_stack == (FILE *) NULL)
    {
      bfd_set_error (bfd_error_system_call);
      einfo (_("%P%F: cannot open linker script file %s: %E\n"), name);
    }

  lex_push_file (ldlex_input_stack, name);

  ldfile_input_filename = name;
  lineno = 1;

  saved_script_handle = ldlex_input_stack;
}

#ifdef GNU960
static char *
gnu960_map_archname (name)
     char *name;
{
  struct tabentry { char *cmd_switch; char *arch; };
  static struct tabentry arch_tab[] =
  {
	"",   "",
	"KA", "ka",
	"KB", "kb",
	"KC", "mc",	/* Synonym for MC */
	"MC", "mc",
	"CA", "ca",
	"SA", "ka",	/* Functionally equivalent to KA */
	"SB", "kb",	/* Functionally equivalent to KB */
	NULL, ""
  };
  struct tabentry *tp;

  for (tp = arch_tab; tp->cmd_switch != NULL; tp++)
    {
      if (! strcmp (name,tp->cmd_switch))
	break;
    }

  if (tp->cmd_switch == NULL)
    einfo (_("%P%F: unknown architecture: %s\n"), name);

  return tp->arch;
}

void
ldfile_add_arch (name)
     char *name;
{
  search_arch_type *new =
    (search_arch_type *) xmalloc ((bfd_size_type) (sizeof (search_arch_type)));

  if (*name != '\0')
    {
      if (ldfile_output_machine_name[0] != '\0')
	{
	  einfo (_("%P%F: target architecture respecified\n"));
	  return;
	}

      ldfile_output_machine_name = name;
    }

  new->next = (search_arch_type *) NULL;
  new->name = gnu960_map_archname (name);
  *search_arch_tail_ptr = new;
  search_arch_tail_ptr = &new->next;
}

#else /* not GNU960 */

void
ldfile_add_arch (in_name)
     const char *in_name;
{
  char *name = xstrdup (in_name);
  search_arch_type *new =
    (search_arch_type *) xmalloc (sizeof (search_arch_type));

  ldfile_output_machine_name = in_name;

  new->name = name;
  new->next = (search_arch_type *) NULL;
  while (*name)
    {
      *name = TOLOWER (*name);
      name++;
    }
  *search_arch_tail_ptr = new;
  search_arch_tail_ptr = &new->next;

}
#endif

/* Set the output architecture.  */

void
ldfile_set_output_arch (string)
     const char *string;
{
  const bfd_arch_info_type *arch = bfd_scan_arch (string);

  if (arch)
    {
      ldfile_output_architecture = arch->arch;
      ldfile_output_machine = arch->mach;
      ldfile_output_machine_name = arch->printable_name;
    }
  else
    {
      einfo (_("%P%F: cannot represent machine `%s'\n"), string);
    }
}
