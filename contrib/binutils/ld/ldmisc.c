/* ldmisc.c
   Copyright (C) 1991, 92, 93, 94, 95, 96, 97, 98, 99, 2000
   Free Software Foundation, Inc.
   Written by Steve Chamberlain of Cygnus Support.

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
#include "libiberty.h"
#include "demangle.h"

#ifdef ANSI_PROTOTYPES
#include <stdarg.h>
#define USE_STDARG 1
#else
#include <varargs.h>
#define USE_STDARG 0
#endif

#include "ld.h"
#include "ldmisc.h"
#include "ldexp.h"
#include "ldlang.h"
#include "ldgram.h"
#include "ldlex.h"
#include "ldmain.h"
#include "ldfile.h"

static void vfinfo PARAMS ((FILE *, const char *, va_list));

/*
 %% literal %
 %F error is fatal
 %P print program name
 %S print script file and linenumber
 %E current bfd error or errno
 %I filename from a lang_input_statement_type
 %B filename from a bfd
 %T symbol name
 %X no object output, fail return
 %V hex bfd_vma
 %v hex bfd_vma, no leading zeros
 %W hex bfd_vma with 0x with no leading zeros taking up 8 spaces
 %C clever filename:linenumber with function
 %D like %C, but no function name
 %G like %D, but only function name
 %R info about a relent
 %s arbitrary string, like printf
 %d integer, like printf
 %u integer, like printf
*/

char *
demangle (string)
     const char *string;
{
  char *res;

  if (output_bfd != NULL
      && bfd_get_symbol_leading_char (output_bfd) == string[0])
    ++string;

  /* This is a hack for better error reporting on XCOFF, or the MS PE
     format.  Xcoff has a single '.', while the NT PE for PPC has
     '..'.  So we remove all of them.  */
  while (string[0] == '.')
    ++string;

  res = cplus_demangle (string, DMGL_ANSI | DMGL_PARAMS);
  return res ? res : xstrdup (string);
}

static void
vfinfo (fp, fmt, arg)
     FILE *fp;
     const char *fmt;
     va_list arg;
{
  boolean fatal = false;

  while (*fmt != '\0')
    {
      while (*fmt != '%' && *fmt != '\0')
	{
	  putc (*fmt, fp);
	  fmt++;
	}

      if (*fmt == '%')
	{
	  fmt++;
	  switch (*fmt++)
	    {
	    default:
	      fprintf (fp, "%%%c", fmt[-1]);
	      break;

	    case '%':
	      /* literal % */
	      putc ('%', fp);
	      break;

	    case 'X':
	      /* no object output, fail return */
	      config.make_executable = false;
	      break;

	    case 'V':
	      /* hex bfd_vma */
	      {
		bfd_vma value = va_arg (arg, bfd_vma);
		fprintf_vma (fp, value);
	      }
	      break;

	    case 'v':
	      /* hex bfd_vma, no leading zeros */
	      {
		char buf[100];
		char *p = buf;
		bfd_vma value = va_arg (arg, bfd_vma);
		sprintf_vma (p, value);
		while (*p == '0')
		  p++;
		if (!*p)
		  p--;
		fputs (p, fp);
	      }
	      break;

	    case 'W':
	      /* hex bfd_vma with 0x with no leading zeroes taking up
                 8 spaces.  */
	      {
		char buf[100];
		bfd_vma value;
		char *p;
		int len;

		value = va_arg (arg, bfd_vma);
		sprintf_vma (buf, value);
		for (p = buf; *p == '0'; ++p)
		  ;
		if (*p == '\0')
		  --p;
		len = strlen (p);
		while (len < 8)
		  {
		    putc (' ', fp);
		    ++len;
		  }
		fprintf (fp, "0x%s", p);
	      }
	      break;

	    case 'T':
	      /* Symbol name.  */
	      {
		const char *name = va_arg (arg, const char *);

		if (name == (const char *) NULL || *name == 0)
		  fprintf (fp, _("no symbol"));
		else if (! demangling)
		  fprintf (fp, "%s", name);
		else
		  {
		    char *demangled;

		    demangled = demangle (name);
		    fprintf (fp, "%s", demangled);
		    free (demangled);
		  }
	      }
	      break;

	    case 'B':
	      /* filename from a bfd */
	      {
		bfd *abfd = va_arg (arg, bfd *);
		if (abfd->my_archive)
		  fprintf (fp, "%s(%s)", abfd->my_archive->filename,
			   abfd->filename);
		else
		  fprintf (fp, "%s", abfd->filename);
	      }
	      break;

	    case 'F':
	      /* Error is fatal.  */
	      fatal = true;
	      break;

	    case 'P':
	      /* Print program name.  */
	      fprintf (fp, "%s", program_name);
	      break;

	    case 'E':
	      /* current bfd error or errno */
	      fprintf (fp, "%s", bfd_errmsg (bfd_get_error ()));
	      break;

	    case 'I':
	      /* filename from a lang_input_statement_type */
	      {
		lang_input_statement_type *i;

		i = va_arg (arg, lang_input_statement_type *);
		if (bfd_my_archive (i->the_bfd) != NULL)
		  fprintf (fp, "(%s)",
			   bfd_get_filename (bfd_my_archive (i->the_bfd)));
		fprintf (fp, "%s", i->local_sym_name);
		if (bfd_my_archive (i->the_bfd) == NULL
		    && strcmp (i->local_sym_name, i->filename) != 0)
		  fprintf (fp, " (%s)", i->filename);
	      }
	      break;

	    case 'S':
	      /* Print script file and linenumber.  */
	      if (parsing_defsym)
		fprintf (fp, "--defsym %s", lex_string);
	      else if (ldfile_input_filename != NULL)
		fprintf (fp, "%s:%u", ldfile_input_filename, lineno);
	      else
		fprintf (fp, _("built in linker script:%u"), lineno);
	      break;

	    case 'R':
	      /* Print all that's interesting about a relent.  */
	      {
		arelent *relent = va_arg (arg, arelent *);

		lfinfo (fp, "%s+0x%v (type %s)",
			(*(relent->sym_ptr_ptr))->name,
			relent->addend,
			relent->howto->name);
	      }
	      break;

	    case 'C':
	    case 'D':
	    case 'G':
	      /* Clever filename:linenumber with function name if possible,
		 or section name as a last resort.  The arguments are a BFD,
		 a section, and an offset.  */
	      {
		static bfd *last_bfd;
		static char *last_file = NULL;
		static char *last_function = NULL;
		bfd *abfd;
		asection *section;
		bfd_vma offset;
		lang_input_statement_type *entry;
		asymbol **asymbols;
		const char *filename;
		const char *functionname;
		unsigned int linenumber;
		boolean discard_last;

		abfd = va_arg (arg, bfd *);
		section = va_arg (arg, asection *);
		offset = va_arg (arg, bfd_vma);

		entry = (lang_input_statement_type *) abfd->usrdata;
		if (entry != (lang_input_statement_type *) NULL
		    && entry->asymbols != (asymbol **) NULL)
		  asymbols = entry->asymbols;
		else
		  {
		    long symsize;
		    long symbol_count;

		    symsize = bfd_get_symtab_upper_bound (abfd);
		    if (symsize < 0)
		      einfo (_("%B%F: could not read symbols\n"), abfd);
		    asymbols = (asymbol **) xmalloc (symsize);
		    symbol_count = bfd_canonicalize_symtab (abfd, asymbols);
		    if (symbol_count < 0)
		      einfo (_("%B%F: could not read symbols\n"), abfd);
		    if (entry != (lang_input_statement_type *) NULL)
		      {
			entry->asymbols = asymbols;
			entry->symbol_count = symbol_count;
		      }
		  }

		discard_last = true;
		if (bfd_find_nearest_line (abfd, section, asymbols, offset,
					   &filename, &functionname,
					   &linenumber))
		  {
		    if (functionname != NULL && fmt[-1] == 'G')
		      {
			lfinfo (fp, "%B:", abfd);
			if (filename != NULL
			    && strcmp (filename, bfd_get_filename (abfd)) != 0)
			  fprintf (fp, "%s:", filename);
			lfinfo (fp, "%T", functionname);
		      }
		    else if (functionname != NULL && fmt[-1] == 'C')
		      {
			if (filename == (char *) NULL)
			  filename = abfd->filename;

			if (last_bfd == NULL
			    || last_file == NULL
			    || last_function == NULL
			    || last_bfd != abfd
			    || strcmp (last_file, filename) != 0
			    || strcmp (last_function, functionname) != 0)
			  {
			    /* We use abfd->filename in this initial line,
			       in case filename is a .h file or something
			       similarly unhelpful.  */
			    lfinfo (fp, _("%B: In function `%T':\n"),
				    abfd, functionname);

			    last_bfd = abfd;
			    if (last_file != NULL)
			      free (last_file);
			    last_file = buystring (filename);
			    if (last_function != NULL)
			      free (last_function);
			    last_function = buystring (functionname);
			  }
			discard_last = false;
			if (linenumber != 0)
			  fprintf (fp, "%s:%u", filename, linenumber);
			else
			  lfinfo (fp, "%s(%s+0x%v)", filename, section->name,
				  offset);
		      }
		    else if (filename == NULL
			     || strcmp (filename, abfd->filename) == 0)
		      {
			lfinfo (fp, "%B(%s+0x%v)", abfd, section->name,
				offset);
			if (linenumber != 0)
			  lfinfo (fp, ":%u", linenumber);
		      }
		    else if (linenumber != 0)
		      lfinfo (fp, "%B:%s:%u", abfd, filename, linenumber);
		    else
		      lfinfo (fp, "%B(%s+0x%v):%s", abfd, section->name,
			      offset, filename);
		  }
		else
		  lfinfo (fp, "%B(%s+0x%v)", abfd, section->name, offset);

		if (discard_last)
		  {
		    last_bfd = NULL;
		    if (last_file != NULL)
		      {
			free (last_file);
			last_file = NULL;
		      }
		    if (last_function != NULL)
		      {
			free (last_function);
			last_function = NULL;
		      }
		  }
	      }
	      break;

	    case 's':
	      /* arbitrary string, like printf */
	      fprintf (fp, "%s", va_arg (arg, char *));
	      break;

	    case 'd':
	      /* integer, like printf */
	      fprintf (fp, "%d", va_arg (arg, int));
	      break;

	    case 'u':
	      /* unsigned integer, like printf */
	      fprintf (fp, "%u", va_arg (arg, unsigned int));
	      break;
	    }
	}
    }

  if (fatal == true)
    xexit (1);
}

/* Format info message and print on stdout.  */

/* (You would think this should be called just "info", but then you
   would hosed by LynxOS, which defines that name in its libc.)  */

void
#if USE_STDARG
info_msg (const char *fmt, ...)
#else
info_msg (va_alist)
     va_dcl
#endif
{
  va_list arg;

#if ! USE_STDARG
  const char *fmt;

  va_start (arg);
  fmt = va_arg (arg, const char *);
#else
  va_start (arg, fmt);
#endif

  vfinfo (stdout, fmt, arg);
  va_end (arg);
}

/* ('e' for error.) Format info message and print on stderr.  */

void
#if USE_STDARG
einfo (const char *fmt, ...)
#else
einfo (va_alist)
     va_dcl
#endif
{
  va_list arg;

#if ! USE_STDARG
  const char *fmt;

  va_start (arg);
  fmt = va_arg (arg, const char *);
#else
  va_start (arg, fmt);
#endif

  vfinfo (stderr, fmt, arg);
  va_end (arg);
}

void
info_assert (file, line)
     const char *file;
     unsigned int line;
{
  einfo (_("%F%P: internal error %s %d\n"), file, line);
}

char *
buystring (x)
     CONST char *CONST x;
{
  size_t l = strlen (x) + 1;
  char *r = xmalloc (l);
  memcpy (r, x, l);
  return r;
}

/* ('m' for map) Format info message and print on map.  */

void
#if USE_STDARG
minfo (const char *fmt, ...)
#else
minfo (va_alist)
     va_dcl
#endif
{
  va_list arg;

#if ! USE_STDARG
  const char *fmt;
  va_start (arg);
  fmt = va_arg (arg, const char *);
#else
  va_start (arg, fmt);
#endif

  vfinfo (config.map_file, fmt, arg);
  va_end (arg);
}

void
#if USE_STDARG
lfinfo (FILE *file, const char *fmt, ...)
#else
lfinfo (va_alist)
     va_dcl
#endif
{
  va_list arg;

#if ! USE_STDARG
  FILE *file;
  const char *fmt;

  va_start (arg);
  file = va_arg (arg, FILE *);
  fmt = va_arg (arg, const char *);
#else
  va_start (arg, fmt);
#endif

  vfinfo (file, fmt, arg);
  va_end (arg);
}

/* Functions to print the link map.  */

void
print_space ()
{
  fprintf (config.map_file, " ");
}

void
print_nl ()
{
  fprintf (config.map_file, "\n");
}

/* A more or less friendly abort message.  In ld.h abort is defined to
   call this function.  */

void
ld_abort (file, line, fn)
     const char *file;
     int line;
     const char *fn;
{
  if (fn != NULL)
    einfo (_("%P: internal error: aborting at %s line %d in %s\n"),
	   file, line, fn);
  else
    einfo (_("%P: internal error: aborting at %s line %d\n"),
	   file, line);
  einfo (_("%P%F: please report this bug\n"));
  xexit (1);
}
