/* ldemul.c -- clearing house for ld emulation states
   Copyright 1991, 1992, 1993, 1994, 1995, 1996, 1997, 1998, 2000
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

#include "bfd.h"
#include "sysdep.h"

#include "ld.h"
#include "ldmisc.h"
#include "ldexp.h"
#include "ldlang.h"
#include "ldfile.h"
#include "ldemul.h"
#include "ldmain.h"
#include "ldemul-list.h"

ld_emulation_xfer_type *ld_emulation;

void
ldemul_hll (name)
     char *name;
{
  ld_emulation->hll (name);
}

void
ldemul_syslib (name)
     char *name;
{
  ld_emulation->syslib (name);
}

void
ldemul_after_parse ()
{
  ld_emulation->after_parse ();
}

void
ldemul_before_parse ()
{
  ld_emulation->before_parse ();
}

void
ldemul_after_open ()
{
  ld_emulation->after_open ();
}

void
ldemul_after_allocation ()
{
  ld_emulation->after_allocation ();
}

void
ldemul_before_allocation ()
{
  if (ld_emulation->before_allocation)
    ld_emulation->before_allocation ();
}

void
ldemul_set_output_arch ()
{
  ld_emulation->set_output_arch ();
}

void
ldemul_finish ()
{
  if (ld_emulation->finish)
    ld_emulation->finish ();
}

void
ldemul_set_symbols ()
{
  if (ld_emulation->set_symbols)
    ld_emulation->set_symbols ();
}

void
ldemul_create_output_section_statements ()
{
  if (ld_emulation->create_output_section_statements)
    ld_emulation->create_output_section_statements ();
}

char *
ldemul_get_script (isfile)
     int *isfile;
{
  return ld_emulation->get_script (isfile);
}

boolean
ldemul_open_dynamic_archive (arch, search, entry)
     const char *arch;
     search_dirs_type *search;
     lang_input_statement_type *entry;
{
  if (ld_emulation->open_dynamic_archive)
    return (*ld_emulation->open_dynamic_archive) (arch, search, entry);
  return false;
}

boolean
ldemul_place_orphan (file, s)
     lang_input_statement_type *file;
     asection *s;
{
  if (ld_emulation->place_orphan)
    return (*ld_emulation->place_orphan) (file, s);
  return false;
}

int
ldemul_parse_args (argc, argv)
     int argc;
     char **argv;
{
  /* Try and use the emulation parser if there is one.  */
  if (ld_emulation->parse_args)
    {
      return ld_emulation->parse_args (argc, argv);
    }
  return 0;
}

/* Let the emulation code handle an unrecognized file.  */

boolean
ldemul_unrecognized_file (entry)
     lang_input_statement_type *entry;
{
  if (ld_emulation->unrecognized_file)
    return (*ld_emulation->unrecognized_file) (entry);
  return false;
}

/* Let the emulation code handle a recognized file.  */

boolean
ldemul_recognized_file (entry)
     lang_input_statement_type *entry;
{
  if (ld_emulation->recognized_file)
    return (*ld_emulation->recognized_file) (entry);
  return false;
}

char *
ldemul_choose_target ()
{
  return ld_emulation->choose_target ();
}

/* The default choose_target function.  */

char *
ldemul_default_target ()
{
  char *from_outside = getenv (TARGET_ENVIRON);
  if (from_outside != (char *) NULL)
    return from_outside;
  return ld_emulation->target_name;
}

void
after_parse_default ()
{
}

void
after_open_default ()
{
}

void
after_allocation_default ()
{
}

void
before_allocation_default ()
{
}

void
set_output_arch_default ()
{
  /* Set the output architecture and machine if possible.  */
  bfd_set_arch_mach (output_bfd,
		     ldfile_output_architecture, ldfile_output_machine);
}

void
syslib_default (ignore)
     char *ignore ATTRIBUTE_UNUSED;
{
  info_msg (_("%S SYSLIB ignored\n"));
}

void
hll_default (ignore)
     char *ignore ATTRIBUTE_UNUSED;
{
  info_msg (_("%S HLL ignored\n"));
}

ld_emulation_xfer_type *ld_emulations[] = { EMULATION_LIST };

void
ldemul_choose_mode (target)
     char *target;
{
  ld_emulation_xfer_type **eptr = ld_emulations;
  /* Ignore "gld" prefix.  */
  if (target[0] == 'g' && target[1] == 'l' && target[2] == 'd')
    target += 3;
  for (; *eptr; eptr++)
    {
      if (strcmp (target, (*eptr)->emulation_name) == 0)
	{
	  ld_emulation = *eptr;
	  return;
	}
    }
  einfo (_("%P: unrecognised emulation mode: %s\n"), target);
  einfo (_("Supported emulations: "));
  ldemul_list_emulations (stderr);
  einfo ("%F\n");
}

void
ldemul_list_emulations (f)
     FILE *f;
{
  ld_emulation_xfer_type **eptr = ld_emulations;
  boolean first = true;

  for (; *eptr; eptr++)
    {
      if (first)
	first = false;
      else
	fprintf (f, " ");
      fprintf (f, "%s", (*eptr)->emulation_name);
    }
}

void
ldemul_list_emulation_options (f)
     FILE *f;
{
  ld_emulation_xfer_type **eptr;
  int options_found = 0;

  for (eptr = ld_emulations; *eptr; eptr++)
    {
      ld_emulation_xfer_type *emul = *eptr;

      if (emul->list_options)
	{
	  fprintf (f, "%s: \n", emul->emulation_name);

	  emul->list_options (f);

	  options_found = 1;
	}
    }

  if (! options_found)
    fprintf (f, _("  no emulation specific options.\n"));
}

int
ldemul_find_potential_libraries (name, entry)
     char *name;
     lang_input_statement_type *entry;
{
  if (ld_emulation->find_potential_libraries)
    return ld_emulation->find_potential_libraries (name, entry);

  return 0;
}
