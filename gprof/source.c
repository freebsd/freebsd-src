/* source.c - Keep track of source files.

   Copyright 2000, 2001, 2002 Free Software Foundation, Inc.

   This file is part of GNU Binutils.

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
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA.  */

#include "gprof.h"
#include "libiberty.h"
#include "filenames.h"
#include "search_list.h"
#include "source.h"

#define EXT_ANNO "-ann"		/* Postfix of annotated files.  */

/* Default option values.  */
bfd_boolean create_annotation_files = FALSE;

Search_List src_search_list = {0, 0};
Source_File *first_src_file = 0;


Source_File *
source_file_lookup_path (path)
     const char *path;
{
  Source_File *sf;

  for (sf = first_src_file; sf; sf = sf->next)
    {
      if (FILENAME_CMP (path, sf->name) == 0)
	break;
    }

  if (!sf)
    {
      /* Create a new source file descriptor.  */
      sf = (Source_File *) xmalloc (sizeof (*sf));

      memset (sf, 0, sizeof (*sf));

      sf->name = xstrdup (path);
      sf->next = first_src_file;
      first_src_file = sf;
    }

  return sf;
}


Source_File *
source_file_lookup_name (filename)
     const char *filename;
{
  const char *fname;
  Source_File *sf;

  /* The user cannot know exactly how a filename will be stored in
     the debugging info (e.g., ../include/foo.h
     vs. /usr/include/foo.h).  So we simply compare the filename
     component of a path only.  */
  for (sf = first_src_file; sf; sf = sf->next)
    {
      fname = strrchr (sf->name, '/');

      if (fname)
	++fname;
      else
	fname = sf->name;

      if (FILENAME_CMP (filename, fname) == 0)
	break;
    }

  return sf;
}


FILE *
annotate_source (sf, max_width, annote, arg)
     Source_File *sf;
     unsigned int max_width;
     void (*annote) PARAMS ((char *, unsigned int, int, void *));
     void *arg;
{
  static bfd_boolean first_file = TRUE;
  int i, line_num, nread;
  bfd_boolean new_line;
  char buf[8192];
  char fname[PATH_MAX];
  char *annotation, *name_only;
  FILE *ifp, *ofp;
  Search_List_Elem *sle = src_search_list.head;

  /* Open input file.  If open fails, walk along search-list until
     open succeeds or reaching end of list.  */
  strcpy (fname, sf->name);

  if (IS_ABSOLUTE_PATH (sf->name))
    sle = 0;			/* Don't use search list for absolute paths.  */

  name_only = 0;
  while (TRUE)
    {
      DBG (SRCDEBUG, printf ("[annotate_source]: looking for %s, trying %s\n",
			     sf->name, fname));

      ifp = fopen (fname, FOPEN_RB);
      if (ifp)
	break;

      if (!sle && !name_only)
	{
	  name_only = strrchr (sf->name, '/');
#ifdef HAVE_DOS_BASED_FILE_SYSTEM
	  {
	    char *bslash = strrchr (sf->name, '\\');
	    if (name_only == NULL || (bslash != NULL && bslash > name_only))
	      name_only = bslash;
	    if (name_only == NULL && sf->name[0] != '\0' && sf->name[1] == ':')
	      name_only = (char *)sf->name + 1;
	  }
#endif
	  if (name_only)
	    {
	      /* Try search-list again, but this time with name only.  */
	      ++name_only;
	      sle = src_search_list.head;
	    }
	}

      if (sle)
	{
	  strcpy (fname, sle->path);
#ifdef HAVE_DOS_BASED_FILE_SYSTEM
	  /* d:foo is not the same thing as d:/foo!  */
	  if (fname[strlen (fname) - 1] == ':')
	    strcat (fname, ".");
#endif
	  strcat (fname, "/");

	  if (name_only)
	    strcat (fname, name_only);
	  else
	    strcat (fname, sf->name);

	  sle = sle->next;
	}
      else
	{
	  if (errno == ENOENT)
	    fprintf (stderr, _("%s: could not locate `%s'\n"),
		     whoami, sf->name);
	  else
	    perror (sf->name);

	  return 0;
	}
    }

  ofp = stdout;

  if (create_annotation_files)
    {
      /* Try to create annotated source file.  */
      const char *filename;

      /* Create annotation files in the current working directory.  */
      filename = strrchr (sf->name, '/');
#ifdef HAVE_DOS_BASED_FILE_SYSTEM
	{
	  char *bslash = strrchr (sf->name, '\\');
	  if (filename == NULL || (bslash != NULL && bslash > filename))
	    filename = bslash;
	  if (filename == NULL && sf->name[0] != '\0' && sf->name[1] == ':')
	    filename = sf->name + 1;
	}
#endif
      if (filename)
	++filename;
      else
	filename = sf->name;

      strcpy (fname, filename);
      strcat (fname, EXT_ANNO);
#ifdef __MSDOS__
      {
	/* foo.cpp-ann can overwrite foo.cpp due to silent truncation of
	   file names on 8+3 filesystems.  Their `stat' better be good...  */
	struct stat buf1, buf2;

	if (stat (filename, &buf1) == 0
	    && stat (fname, &buf2) == 0
	    && buf1.st_ino == buf2.st_ino)
	  {
	    char *dot = strrchr (fname, '.');

	    if (dot)
	      *dot = '\0';
	    strcat (fname, ".ann");
	  }
      }
#endif
      ofp = fopen (fname, "w");

      if (!ofp)
	{
	  perror (fname);
	  return 0;
	}
    }

  /* Print file names if output goes to stdout
     and there are more than one source file.  */
  if (ofp == stdout)
    {
      if (first_file)
	first_file = FALSE;
      else
	fputc ('\n', ofp);

      if (first_output)
	first_output = FALSE;
      else
	fprintf (ofp, "\f\n");

      fprintf (ofp, _("*** File %s:\n"), sf->name);
    }

  annotation = xmalloc (max_width + 1);
  line_num = 1;
  new_line = TRUE;

  while ((nread = fread (buf, 1, sizeof (buf), ifp)) > 0)
    {
      for (i = 0; i < nread; ++i)
	{
	  if (new_line)
	    {
	      (*annote) (annotation, max_width, line_num, arg);
	      fputs (annotation, ofp);
	      ++line_num;
	      new_line = FALSE;
	    }

	  new_line = (buf[i] == '\n');
	  fputc (buf[i], ofp);
	}
    }

  free (annotation);
  return ofp;
}
