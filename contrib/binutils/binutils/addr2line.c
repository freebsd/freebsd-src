/* addr2line.c -- convert addresses to line number and function name
   Copyright 1997, 1998 Free Software Foundation, Inc.
   Contributed by Ulrich Lauther <Ulrich.Lauther@zfe.siemens.de>

   This file is part of GNU Binutils.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

/* Derived from objdump.c and nm.c by Ulrich.Lauther@zfe.siemens.de

   Usage: 
   addr2line [options] addr addr ...
   or
   addr2line [options] 

   both forms write results to stdout, the second form reads addresses
   to be converted from stdin.  */

#include <ctype.h>
#include <string.h>

#include "bfd.h"
#include "getopt.h"
#include "libiberty.h"
#include "demangle.h"
#include "bucomm.h"

extern char *program_version;

static boolean with_functions;	/* -f, show function names.  */
static boolean do_demangle;	/* -C, demangle names.  */
static boolean base_names;	/* -s, strip directory names.  */

static int naddr;		/* Number of addresses to process.  */
static char **addr;		/* Hex addresses to process.  */

static asymbol **syms;		/* Symbol table.  */

static struct option long_options[] =
{
  {"basenames", no_argument, NULL, 's'},
  {"demangle", no_argument, NULL, 'C'},
  {"exe", required_argument, NULL, 'e'},
  {"functions", no_argument, NULL, 'f'},
  {"target", required_argument, NULL, 'b'},
  {"help", no_argument, NULL, 'H'},
  {"version", no_argument, NULL, 'V'},
  {0, no_argument, 0, 0}
};

static void usage PARAMS ((FILE *, int));
static void slurp_symtab PARAMS ((bfd *));
static void find_address_in_section PARAMS ((bfd *, asection *, PTR));
static void translate_addresses PARAMS ((bfd *));
static void process_file PARAMS ((const char *, const char *));

/* Print a usage message to STREAM and exit with STATUS.  */

static void
usage (stream, status)
     FILE *stream;
     int status;
{
  fprintf (stream, "\
Usage: %s [-CfsHV] [-b bfdname] [--target=bfdname]\n\
       [-e executable] [--exe=executable] [--demangle]\n\
       [--basenames] [--functions] [addr addr ...]\n",
	   program_name);
  list_supported_targets (program_name, stream);
  if (status == 0)
    fprintf (stream, "Report bugs to bug-gnu-utils@gnu.org\n");
  exit (status);
}

/* Read in the symbol table.  */

static void
slurp_symtab (abfd)
     bfd *abfd;
{
  long storage;
  long symcount;

  if ((bfd_get_file_flags (abfd) & HAS_SYMS) == 0)
    return;

  storage = bfd_get_symtab_upper_bound (abfd);
  if (storage < 0)
    bfd_fatal (bfd_get_filename (abfd));

  syms = (asymbol **) xmalloc (storage);

  symcount = bfd_canonicalize_symtab (abfd, syms);
  if (symcount < 0)
    bfd_fatal (bfd_get_filename (abfd));
}

/* These global variables are used to pass information between
   translate_addresses and find_address_in_section.  */

static bfd_vma pc;
static const char *filename;
static const char *functionname;
static unsigned int line;
static boolean found;

/* Look for an address in a section.  This is called via
   bfd_map_over_sections.  */

static void
find_address_in_section (abfd, section, data)
     bfd *abfd;
     asection *section;
     PTR data;
{
  bfd_vma vma;

  if (found)
    return;

  if ((bfd_get_section_flags (abfd, section) & SEC_ALLOC) == 0)
    return;

  vma = bfd_get_section_vma (abfd, section);
  if (pc < vma)
    return;

  found = bfd_find_nearest_line (abfd, section, syms, pc - vma,
				 &filename, &functionname, &line);
}

/* Read hexadecimal addresses from stdin, translate into
   file_name:line_number and optionally function name.  */

static void
translate_addresses (abfd)
     bfd *abfd;
{
  int read_stdin = (naddr == 0);

  for (;;)
    {
      if (read_stdin)
	{
	  char addr_hex[100];

	  if (fgets (addr_hex, sizeof addr_hex, stdin) == NULL)
	    break;
	  pc = bfd_scan_vma (addr_hex, NULL, 16);
	}
      else
	{
	  if (naddr <= 0)
	    break;
	  --naddr;
	  pc = bfd_scan_vma (*addr++, NULL, 16);
	}

      found = false;
      bfd_map_over_sections (abfd, find_address_in_section, (PTR) NULL);

      if (! found)
	{
	  if (with_functions)
	    printf ("??\n");
	  printf ("??:0\n");
	}
      else
	{
	  if (with_functions)
	    {
	      if (*functionname == '\0')
		printf ("??\n");
	      else if (! do_demangle)
		printf ("%s\n", functionname);
	      else
		{
		  char *res;

		  res = cplus_demangle (functionname, DMGL_ANSI | DMGL_PARAMS);
		  if (res == NULL)
		    printf ("%s\n", functionname);
		  else
		    {
		      printf ("%s\n", res);
		      free (res);
		    }
		}
	    }

	  if (base_names)
	    {
	      char *h;

	      h = strrchr (filename, '/');
	      if (h != NULL)
		filename = h + 1;
	    }

	  printf ("%s:%u\n", filename, line);
	}

      /* fflush() is essential for using this command as a server
         child process that reads addresses from a pipe and responds
         with line number information, processing one address at a
         time.  */
      fflush (stdout);
    }
}

/* Process a file.  */

static void
process_file (filename, target)
     const char *filename;
     const char *target;
{
  bfd *abfd;
  char **matching;

  abfd = bfd_openr (filename, target);
  if (abfd == NULL)
    bfd_fatal (filename);

  if (bfd_check_format (abfd, bfd_archive))
    fatal ("%s: can not get addresses from archive", filename);

  if (! bfd_check_format_matches (abfd, bfd_object, &matching))
    {
      bfd_nonfatal (bfd_get_filename (abfd));
      if (bfd_get_error () == bfd_error_file_ambiguously_recognized)
	{
	  list_matching_formats (matching);
	  free (matching);
	}
      xexit (1);
    }

  slurp_symtab (abfd);

  translate_addresses (abfd);

  if (syms != NULL)
    {
      free (syms);
      syms = NULL;
    }

  bfd_close (abfd);
}

int
main (argc, argv)
     int argc;
     char **argv;
{
  char *filename;
  char *target;
  int c;

  program_name = *argv;
  xmalloc_set_program_name (program_name);

  bfd_init ();
  set_default_bfd_target ();

  filename = NULL;
  target = NULL;
  while ((c = getopt_long (argc, argv, "b:Ce:sfHV", long_options, (int *) 0))
	 != EOF)
    {
      switch (c)
	{
	case 0:
	  break;		/* we've been given a long option */
	case 'b':
	  target = optarg;
	  break;
	case 'C':
	  do_demangle = true;
	  break;
	case 'e':
	  filename = optarg;
	  break;
	case 's':
	  base_names = true;
	  break;
	case 'f':
	  with_functions = true;
	  break;
	case 'V':
	  print_version ("addr2line");
	  break;
	case 'H':
	  usage (stdout, 0);
	  break;
	default:
	  usage (stderr, 1);
	  break;
	}
    }

  if (filename == NULL)
    filename = "a.out";

  addr = argv + optind;
  naddr = argc - optind;

  process_file (filename, target);

  return 0;
}
