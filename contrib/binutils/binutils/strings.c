/* strings -- print the strings of printable characters in files
   Copyright (C) 1993, 94, 95, 96, 97, 1998 Free Software Foundation, Inc.

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
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA.  */

/* Usage: strings [options] file...

   Options:
   --all
   -a
   -		Do not scan only the initialized data section of object files.

   --print-file-name
   -f		Print the name of the file before each string.

   --bytes=min-len
   -n min-len
   -min-len	Print graphic char sequences, MIN-LEN or more bytes long,
		that are followed by a NUL or a newline.  Default is 4.

   --radix={o,x,d}
   -t {o,x,d}	Print the offset within the file before each string,
		in octal/hex/decimal.

   -o		Like -to.  (Some other implementations have -o like -to,
		others like -td.  We chose one arbitrarily.)

   --target=BFDNAME
		Specify a non-default object file format.

   --help
   -h		Print the usage message on the standard output.

   --version
   -v		Print the program version number.

   Written by Richard Stallman <rms@gnu.ai.mit.edu>
   and David MacKenzie <djm@gnu.ai.mit.edu>.  */

#include "bfd.h"
#include <stdio.h>
#include <getopt.h>
#include <ctype.h>
#include <errno.h>
#include "bucomm.h"
#include "libiberty.h"

#ifdef isascii
#define isgraphic(c) (isascii (c) && isprint (c))
#else
#define isgraphic(c) (isprint (c))
#endif

#ifndef errno
extern int errno;
#endif

/* The BFD section flags that identify an initialized data section.  */
#define DATA_FLAGS (SEC_ALLOC | SEC_LOAD | SEC_HAS_CONTENTS)

/* Radix for printing addresses (must be 8, 10 or 16).  */
static int address_radix;

/* Minimum length of sequence of graphic chars to trigger output.  */
static int string_min;

/* true means print address within file for each string.  */
static boolean print_addresses;

/* true means print filename for each string.  */
static boolean print_filenames;

/* true means for object files scan only the data section.  */
static boolean datasection_only;

/* true if we found an initialized data section in the current file.  */
static boolean got_a_section;

/* The BFD object file format.  */
static char *target;

static struct option long_options[] =
{
  {"all", no_argument, NULL, 'a'},
  {"print-file-name", no_argument, NULL, 'f'},
  {"bytes", required_argument, NULL, 'n'},
  {"radix", required_argument, NULL, 't'},
  {"target", required_argument, NULL, 'T'},
  {"help", no_argument, NULL, 'h'},
  {"version", no_argument, NULL, 'v'},
  {NULL, 0, NULL, 0}
};

static void strings_a_section PARAMS ((bfd *, asection *, PTR));
static boolean strings_object_file PARAMS ((const char *));
static boolean strings_file PARAMS ((char *file));
static int integer_arg PARAMS ((char *s));
static void print_strings PARAMS ((const char *filename, FILE *stream,
				  file_ptr address, int stop_point,
				  int magiccount, char *magic));
static void usage PARAMS ((FILE *stream, int status));

int
main (argc, argv)
     int argc;
     char **argv;
{
  int optc;
  int exit_status = 0;
  boolean files_given = false;

  program_name = argv[0];
  xmalloc_set_program_name (program_name);
  string_min = -1;
  print_addresses = false;
  print_filenames = false;
  datasection_only = true;
  target = NULL;

  while ((optc = getopt_long (argc, argv, "afn:ot:v0123456789",
			      long_options, (int *) 0)) != EOF)
    {
      switch (optc)
	{
	case 'a':
	  datasection_only = false;
	  break;

	case 'f':
	  print_filenames = true;
	  break;

	case 'h':
	  usage (stdout, 0);

	case 'n':
	  string_min = integer_arg (optarg);
	  if (string_min < 1)
	    {
	      fprintf (stderr, "%s: invalid number %s\n",
		       program_name, optarg);
	      exit (1);
	    }
	  break;

	case 'o':
	  print_addresses = true;
	  address_radix = 8;
	  break;

	case 't':
	  print_addresses = true;
	  if (optarg[1] != '\0')
	    usage (stderr, 1);
	  switch (optarg[0])
	    {
	    case 'o':
	      address_radix = 8;
	      break;

	    case 'd':
	      address_radix = 10;
	      break;

	    case 'x':
	      address_radix = 16;
	      break;

	    default:
	      usage (stderr, 1);
	    }
	  break;

	case 'T':
	  target = optarg;
	  break;

	case 'v':
	  print_version ("strings");
	  break;

	case '?':
	  usage (stderr, 1);

	default:
	  if (string_min < 0)
	    string_min = optc;
	  else
	    string_min = string_min * 10 + optc - '0';
	  break;
	}
    }

  if (string_min < 0)
    string_min = 4;

  bfd_init ();
  set_default_bfd_target ();

  if (optind >= argc)
    {
      datasection_only = false;
      print_strings ("{standard input}", stdin, 0, 0, 0, (char *) NULL);
      files_given = true;
    }
  else
    {
      for (; optind < argc; ++optind)
	{
	  if (strcmp (argv[optind], "-") == 0)
	    datasection_only = false;
	  else
	    {
	      files_given = true;
	      exit_status |= (strings_file (argv[optind]) == false);
	    }
	}
    }

  if (files_given == false)
    usage (stderr, 1);

  return (exit_status);
}

/* Scan section SECT of the file ABFD, whose printable name is FILE.
   If it contains initialized data,
   set `got_a_section' and print the strings in it.  */

static void
strings_a_section (abfd, sect, filearg)
     bfd *abfd;
     asection *sect;
     PTR filearg;
{
  const char *file = (const char *) filearg;

  if ((sect->flags & DATA_FLAGS) == DATA_FLAGS)
    {
      bfd_size_type sz = bfd_get_section_size_before_reloc (sect);
      PTR mem = xmalloc (sz);
      if (bfd_get_section_contents (abfd, sect, mem, (file_ptr) 0, sz))
	{
	  got_a_section = true;
	  print_strings (file, (FILE *) NULL, sect->filepos, 0, sz, mem);
	}
      free (mem);
    }
}

/* Scan all of the sections in FILE, and print the strings
   in the initialized data section(s).

   Return true if successful,
   false if not (such as if FILE is not an object file).  */

static boolean
strings_object_file (file)
     const char *file;
{
  bfd *abfd = bfd_openr (file, target);

  if (abfd == NULL)
    {
      /* Treat the file as a non-object file.  */
      return false;
    }

  /* This call is mainly for its side effect of reading in the sections.
     We follow the traditional behavior of `strings' in that we don't
     complain if we don't recognize a file to be an object file.  */
  if (bfd_check_format (abfd, bfd_object) == false)
    {
      bfd_close (abfd);
      return false;
    }

  got_a_section = false;
  bfd_map_over_sections (abfd, strings_a_section, (PTR) file);

  if (!bfd_close (abfd))
    {
      bfd_nonfatal (file);
      return false;
    }

  return got_a_section;
}

/* Print the strings in FILE.  Return true if ok, false if an error occurs.  */

static boolean
strings_file (file)
     char *file;
{
  /* If we weren't told to scan the whole file,
     try to open it as an object file and only look at
     initialized data sections.  If that fails, fall back to the
     whole file.  */
  if (!datasection_only || !strings_object_file (file))
    {
      FILE *stream;

      stream = fopen (file, "rb");
      /* Not all systems permit "rb", so try "r" if it failed.  */
      if (stream == NULL)
	stream = fopen (file, "r");
      if (stream == NULL)
	{
	  fprintf (stderr, "%s: ", program_name);
	  perror (file);
	  return false;
	}

      print_strings (file, stream, (file_ptr) 0, 0, 0, (char *) 0);

      if (fclose (stream) == EOF)
	{
	  fprintf (stderr, "%s: ", program_name);
	  perror (file);
	  return false;
	}
    }

  return true;
}

/* Find the strings in file FILENAME, read from STREAM.
   Assume that STREAM is positioned so that the next byte read
   is at address ADDRESS in the file.
   Stop reading at address STOP_POINT in the file, if nonzero.

   If STREAM is NULL, do not read from it.
   The caller can supply a buffer of characters
   to be processed before the data in STREAM.
   MAGIC is the address of the buffer and
   MAGICCOUNT is how many characters are in it.
   Those characters come at address ADDRESS and the data in STREAM follow.  */

static void
print_strings (filename, stream, address, stop_point, magiccount, magic)
     const char *filename;
     FILE *stream;
     file_ptr address;
     int stop_point;
     int magiccount;
     char *magic;
{
  char *buf = (char *) xmalloc (string_min + 1);

  while (1)
    {
      file_ptr start;
      int i;
      int c;

      /* See if the next `string_min' chars are all graphic chars.  */
    tryline:
      if (stop_point && address >= stop_point)
	break;
      start = address;
      for (i = 0; i < string_min; i++)
	{
	  if (magiccount)
	    {
	      magiccount--;
	      c = *magic++;
	    }
	  else
	    {
	      if (stream == NULL)
		return;
	      c = getc (stream);
	      if (c == EOF)
		return;
	    }
	  address++;
	  if (!isgraphic (c))
	    /* Found a non-graphic.  Try again starting with next char.  */
	    goto tryline;
	  buf[i] = c;
	}

      /* We found a run of `string_min' graphic characters.  Print up
         to the next non-graphic character.  */

      if (print_filenames)
	printf ("%s: ", filename);
      if (print_addresses)
	switch (address_radix)
	  {
	  case 8:
	    printf ("%7lo ", (unsigned long) start);
	    break;

	  case 10:
	    printf ("%7ld ", (long) start);
	    break;

	  case 16:
	    printf ("%7lx ", (unsigned long) start);
	    break;
	  }

      buf[i] = '\0';
      fputs (buf, stdout);

      while (1)
	{
	  if (magiccount)
	    {
	      magiccount--;
	      c = *magic++;
	    }
	  else
	    {
	      if (stream == NULL)
		break;
	      c = getc (stream);
	      if (c == EOF)
		break;
	    }
	  address++;
	  if (! isgraphic (c))
	    break;
	  putchar (c);
	}

      putchar ('\n');
    }
}

/* Parse string S as an integer, using decimal radix by default,
   but allowing octal and hex numbers as in C.  */

static int
integer_arg (s)
     char *s;
{
  int value;
  int radix = 10;
  char *p = s;
  int c;

  if (*p != '0')
    radix = 10;
  else if (*++p == 'x')
    {
      radix = 16;
      p++;
    }
  else
    radix = 8;

  value = 0;
  while (((c = *p++) >= '0' && c <= '9')
	 || (radix == 16 && (c & ~40) >= 'A' && (c & ~40) <= 'Z'))
    {
      value *= radix;
      if (c >= '0' && c <= '9')
	value += c - '0';
      else
	value += (c & ~40) - 'A';
    }

  if (c == 'b')
    value *= 512;
  else if (c == 'B')
    value *= 1024;
  else
    p--;

  if (*p)
    {
      fprintf (stderr, "%s: invalid integer argument %s\n", program_name, s);
      exit (1);
    }
  return value;
}

static void
usage (stream, status)
     FILE *stream;
     int status;
{
  fprintf (stream, "\
Usage: %s [-afov] [-n min-len] [-min-len] [-t {o,x,d}] [-]\n\
       [--all] [--print-file-name] [--bytes=min-len] [--radix={o,x,d}]\n\
       [--target=bfdname] [--help] [--version] file...\n",
	   program_name);
  list_supported_targets (program_name, stream);
  if (status == 0)
    fprintf (stream, "Report bugs to bug-gnu-utils@gnu.org\n");
  exit (status);
}
