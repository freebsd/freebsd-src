/* Copyright (C) 1991, 92, 93, 94 Free Software Foundation, Inc.

This file is part of GLD, the Gnu Linker.

GLD is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 1, or (at your option)
any later version.

GLD is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GLD; see the file COPYING.  If not, write to
the Free Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

/*
 ldfile.c

 look after all the file stuff

 */

#include "bfd.h"
#include "sysdep.h"
#include "bfdlink.h"
#include "ld.h"
#include "ldmisc.h"
#include "ldexp.h"
#include "ldlang.h"
#include "ldfile.h"
#include "ldmain.h"
#include "ldgram.h"
#include "ldlex.h"
#include "ldemul.h"

#include <ctype.h>

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
char *slash = "/";
#endif
#else /* MPW */
/* The MPW path char is a colon. */
char *slash = ":";
#endif /* MPW */

/* LOCAL */

static search_dirs_type **search_tail_ptr = &search_head;

typedef struct search_arch 
{
  char *name; 
  struct search_arch *next;
} search_arch_type;

static search_arch_type *search_arch_head;
static search_arch_type **search_arch_tail_ptr = &search_arch_head;
 
static boolean ldfile_open_file_search
  PARAMS ((const char *arch, lang_input_statement_type *,
	   const char *lib, const char *suffix));
static FILE *try_open PARAMS ((const char *name, const char *exten));

void
ldfile_add_library_path (name, cmdline)
     const char *name;
     boolean cmdline;
{
  search_dirs_type *new;

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
    info_msg ("attempt to open %s %s\n", attempt,
	      entry->the_bfd == NULL ? "failed" : "succeeded");

  if (entry->the_bfd != NULL)
    return true;
  else
    {
      if (bfd_get_error () == bfd_error_invalid_target)
	einfo ("%F%P: invalid BFD target `%s'\n", entry->target);
      return false;
    }
}

/* Search for and open the file specified by ENTRY.  If it is an
   archive, use ARCH, LIB and SUFFIX to modify the file name.  */

static boolean
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
       search != (search_dirs_type *)NULL;
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
      else if (entry->filename[0] == '/' || entry->filename[0] == '.')
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
    }
  else
    {
      search_arch_type *arch;

      /* Try to open <filename><suffix> or lib<filename><suffix>.a */
      for (arch = search_arch_head;
	   arch != (search_arch_type *) NULL;
	   arch = arch->next)
	{
	  if (ldfile_open_file_search (arch->name, entry, "lib", ".a"))
	    return;
#ifdef VMS
	  if (ldfile_open_file_search (arch->name, entry, ":lib", ".a"))
	    return;
#endif
	}
    }

  einfo("%F%P: cannot open %s: %E\n", entry->local_sym_name);
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
	info_msg ("cannot find script file ");
      else
	info_msg ("opened script file ");
      info_msg ("%s\n",name);
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
	    info_msg ("cannot find script file ");
	  else
	    info_msg ("opened script file ");
	  info_msg ("%s\n", buff);
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

  /* First try raw name */
  result = try_open(name,"");
  if (result == (FILE *)NULL) {
    /* Try now prefixes */
    for (search = search_head;
	 search != (search_dirs_type *)NULL;
	 search = search->next) {
      sprintf(buffer,"%s/%s", search->name, name);
      result = try_open(buffer, extend);
      if (result)break;
    }
  }
  return result;
}

void
ldfile_open_command_file (name)
     const char *name;
{
  FILE *ldlex_input_stack;
  ldlex_input_stack = ldfile_find_command_file(name, "");

  if (ldlex_input_stack == (FILE *)NULL) {
    bfd_set_error (bfd_error_system_call);
    einfo("%P%F: cannot open linker script file %s: %E\n",name);
  }
  lex_push_file(ldlex_input_stack, name);
  
  ldfile_input_filename = name;
  lineno = 1;
  had_script = true;
}





#ifdef GNU960
static
char *
gnu960_map_archname( name )
char *name;
{
  struct tabentry { char *cmd_switch; char *arch; };
  static struct tabentry arch_tab[] = {
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
  

  for ( tp = arch_tab; tp->cmd_switch != NULL; tp++ ){
    if ( !strcmp(name,tp->cmd_switch) ){
      break;
    }
  }

  if ( tp->cmd_switch == NULL ){
    einfo("%P%F: unknown architecture: %s\n",name);
  }
  return tp->arch;
}



void
ldfile_add_arch(name)
char *name;
{
  search_arch_type *new =
    (search_arch_type *)xmalloc((bfd_size_type)(sizeof(search_arch_type)));


  if (*name != '\0') {
    if (ldfile_output_machine_name[0] != '\0') {
      einfo("%P%F: target architecture respecified\n");
      return;
    }
    ldfile_output_machine_name = name;
  }

  new->next = (search_arch_type*)NULL;
  new->name = gnu960_map_archname( name );
  *search_arch_tail_ptr = new;
  search_arch_tail_ptr = &new->next;

}

#else	/* not GNU960 */


void
ldfile_add_arch (in_name)
     CONST char * in_name;
{
  char *name = buystring(in_name);
  search_arch_type *new =
    (search_arch_type *) xmalloc (sizeof (search_arch_type));

  ldfile_output_machine_name = in_name;

  new->name = name;
  new->next = (search_arch_type*)NULL;
  while (*name) {
    if (isupper(*name)) *name = tolower(*name);
    name++;
  }
  *search_arch_tail_ptr = new;
  search_arch_tail_ptr = &new->next;

}
#endif

/* Set the output architecture */
void
ldfile_set_output_arch (string)
     CONST char *string;
{
  const bfd_arch_info_type *arch = bfd_scan_arch(string);

  if (arch) {
    ldfile_output_architecture = arch->arch;
    ldfile_output_machine = arch->mach;
    ldfile_output_machine_name = arch->printable_name;
  }
  else {
    einfo("%P%F: cannot represent machine `%s'\n", string);
  }
}
