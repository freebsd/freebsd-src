/* arsup.c - Archive support for MRI compatibility
   Copyright 1992, 1994, 1995, 1996, 1997, 2000
   Free Software Foundation, Inc.

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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */


/* Contributed by Steve Chamberlain
   		  sac@cygnus.com

This file looks after requests from arparse.y, to provide the MRI
style librarian command syntax + 1 word LIST

*/

#include "bfd.h"
#include "arsup.h"
#include "libiberty.h"
#include "bucomm.h"
#include "filenames.h"

static void map_over_list
  PARAMS ((bfd *, void (*function) (bfd *, bfd *), struct list *));
static void ar_directory_doer PARAMS ((bfd *, bfd *));
static void ar_addlib_doer PARAMS ((bfd *, bfd *));

extern int verbose;

static void
map_over_list (arch, function, list)
     bfd *arch;
     void (*function) PARAMS ((bfd *, bfd *));
     struct list *list;
{
  bfd *head;

  if (list == NULL)
    {
      bfd *next;

      head = arch->next;
      while (head != NULL)
	{
	  next = head->next;
	  function (head, (bfd *) NULL);
	  head = next;
	}
    }
  else
    {
      struct list *ptr;

      /* This may appear to be a baroque way of accomplishing what we
	 want.  however we have to iterate over the filenames in order
	 to notice where a filename is requested but does not exist in
	 the archive.  Ditto mapping over each file each time -- we
	 want to hack multiple references.  */
      for (ptr = list; ptr; ptr = ptr->next)
	{
	  boolean found = false;
	  bfd *prev = arch;

	  for (head = arch->next; head; head = head->next) 
	    {
	      if (head->filename != NULL
		  && FILENAME_CMP (ptr->name, head->filename) == 0)
		{
		  found = true;
		  function (head, prev);
		}
	      prev = head;
	    }
	  if (! found)
	    fprintf (stderr, _("No entry %s in archive.\n"), ptr->name);
	}
    }
}


FILE *outfile;

/*ARGSUSED*/
static void
ar_directory_doer (abfd, ignore)
     bfd *abfd;
     bfd *ignore ATTRIBUTE_UNUSED;
{
    print_arelt_descr(outfile, abfd, verbose);
}

void
ar_directory (ar_name, list, output)
     char *ar_name;
     struct list *list;
     char *output;
{
  bfd *arch;

  arch = open_inarch (ar_name, (char *) NULL);
  if (output)
    {
      outfile = fopen(output,"w");
      if (outfile == 0)
	{
	  outfile = stdout;
	  fprintf (stderr,_("Can't open file %s\n"), output);
	  output = 0;
	}
    }
  else 
    outfile = stdout;

  map_over_list (arch, ar_directory_doer, list);

  bfd_close (arch);

  if (output)
   fclose (outfile);
}

void
DEFUN_VOID(prompt)
{
  extern int interactive;
  if (interactive) 
  {
    printf("AR >");
    fflush(stdout); 
  }
}

void
maybequit ()
{
  if (! interactive) 
    xexit (9);
}


bfd *obfd;
char *real_name ; 
void
DEFUN(ar_open,(name, t),
      char *name AND
      int t)

{
  char *tname = (char *) xmalloc (strlen (name) + 10);
  const char *bname = lbasename (name);
  real_name = name;
  /* Prepend tmp- to the beginning, to avoid file-name clashes after
     truncation on filesystems with limited namespaces (DOS).  */
  sprintf(tname, "%.*stmp-%s", (int) (bname - name), name, bname);
  obfd = bfd_openw(tname, NULL);

  if (!obfd) {
    fprintf(stderr,_("%s: Can't open output archive %s\n"), program_name,
	    tname);

    maybequit();
  }
  else {
    if (!t) {
      bfd **ptr;
      bfd *element;
      bfd *ibfd;
      ibfd = bfd_openr(name, NULL);
      if (!ibfd) {
	fprintf(stderr,_("%s: Can't open input archive %s\n"),
		program_name, name);
	maybequit();
	return;
      }
      if (bfd_check_format(ibfd, bfd_archive) != true) {
	fprintf(stderr,_("%s: file %s is not an archive\n"), program_name,
		name);
	maybequit();
	return;
      }
      ptr = &(obfd->archive_head);
      element = bfd_openr_next_archived_file(ibfd, NULL);

      while (element) {
	*ptr = element;
	ptr = &element->next;
	element = bfd_openr_next_archived_file(ibfd, element);
      }
    }

    bfd_set_format(obfd, bfd_archive);

    obfd->has_armap = 1;
  }
}


static void
ar_addlib_doer (abfd, prev)
     bfd *abfd;
     bfd *prev;
{
  /* Add this module to the output bfd */
  if (prev != NULL)
    prev->next = abfd->next;
  abfd->next = obfd->archive_head;
  obfd->archive_head = abfd;
}

void
ar_addlib (name, list)
     char *name;
     struct list *list;
{
  if (obfd == NULL)
    {
      fprintf (stderr, _("%s: no output archive specified yet\n"), program_name);
      maybequit ();
    }
  else
    {
      bfd *arch;

      arch = open_inarch (name, (char *) NULL);
      if (arch != NULL)
	map_over_list (arch, ar_addlib_doer, list);

      /* Don't close the bfd, since it will make the elements disasppear */
    }
}

void
DEFUN(ar_addmod, (list),
      struct list *list)
{
  if (!obfd) {
    fprintf(stderr, _("%s: no open output archive\n"), program_name);
    maybequit();
  }
  else 
  {
    while (list) {
      bfd *abfd = bfd_openr(list->name, NULL);
      if (!abfd)  {
	fprintf(stderr,_("%s: can't open file %s\n"), program_name,
		list->name);
	maybequit();
      }
      else {
	abfd->next = obfd->archive_head;
	obfd->archive_head = abfd;
      }
      list = list->next;
    }
  }
}



void
DEFUN_VOID(ar_clear)
{
if (obfd) 
 obfd->archive_head = 0;
}

void
DEFUN(ar_delete, (list),
      struct list *list)
{
  if (!obfd) {
    fprintf(stderr, _("%s: no open output archive\n"), program_name);
    maybequit();
  }
  else 
  {
    while (list) {
      /* Find this name in the archive */
      bfd *member = obfd->archive_head;
      bfd **prev = &(obfd->archive_head);
      int found = 0;
      while (member) {
	if (FILENAME_CMP(member->filename, list->name) == 0) {
	  *prev = member->next;
	  found = 1;
	}
	else {
	  prev = &(member->next);
	}
	  member = member->next;
      }
      if (!found)  {
	fprintf(stderr,_("%s: can't find module file %s\n"), program_name,
		list->name);
	maybequit();
      }
      list = list->next;
    }
  }
}


void
DEFUN_VOID(ar_save)
{

  if (!obfd) {
    fprintf(stderr, _("%s: no open output archive\n"), program_name);
    maybequit();
  }
  else {
    char *ofilename = xstrdup (bfd_get_filename (obfd));
    bfd_close(obfd);
    
    rename (ofilename, real_name);
    obfd = 0;
    free(ofilename);
  }
}



void
DEFUN(ar_replace, (list),
      struct list *list)
{
  if (!obfd) {
    fprintf(stderr, _("%s: no open output archive\n"), program_name);
    maybequit();
  }
  else 
  {
    while (list) {
      /* Find this name in the archive */
      bfd *member = obfd->archive_head;
      bfd **prev = &(obfd->archive_head);
      int found = 0;
      while (member) 
      {
	if (FILENAME_CMP(member->filename, list->name) == 0) 
	{
	  /* Found the one to replace */
	  bfd *abfd = bfd_openr(list->name, 0);
	  if (!abfd) 
	  {
	    fprintf(stderr, _("%s: can't open file %s\n"), program_name, list->name);
	    maybequit();
	  }
	  else {
	    *prev = abfd;
	    abfd->next = member->next;
	    found = 1;
	  }
	}
	else {
	  prev = &(member->next);
	}
	member = member->next;
      }
      if (!found)  {
	bfd *abfd = bfd_openr(list->name, 0);
	fprintf(stderr,_("%s: can't find module file %s\n"), program_name,
		list->name);
	if (!abfd) 
	{
	  fprintf(stderr, _("%s: can't open file %s\n"), program_name, list->name);
	  maybequit();
	}
	else 
	{
	  *prev = abfd;
	}
      }

    list = list->next;
    }
  }
}

/* And I added this one */
void
DEFUN_VOID(ar_list)
{
  if (!obfd) 
  {
    fprintf(stderr, _("%s: no open output archive\n"), program_name);
    maybequit();
  }
  else {
    bfd *abfd;
    outfile = stdout;
    verbose =1 ;
    printf(_("Current open archive is %s\n"), bfd_get_filename (obfd));
    for (abfd = obfd->archive_head;
	 abfd != (bfd *)NULL;
	 abfd = abfd->next) 
    {
      ar_directory_doer (abfd, (bfd *) NULL);
    }
  }
}


void 
DEFUN_VOID(ar_end)
{
  if (obfd)
  {
    fclose((FILE *)(obfd->iostream));
    unlink(bfd_get_filename (obfd));
  }
}
void
DEFUN(ar_extract,(list),
      struct list *list)
{
  if (!obfd) 
  {

    fprintf(stderr, _("%s: no open  archive\n"), program_name);
    maybequit();
  }
  else 
  {
    while (list) {
      /* Find this name in the archive */
      bfd *member = obfd->archive_head;
      int found = 0;
      while (member && !found) 
      {
	if (FILENAME_CMP(member->filename, list->name) == 0) 
	{
	  extract_file(member);
	  found = 1;
	  }

	member = member->next;
      }
      if (!found)  {
	bfd_openr(list->name, 0);
	fprintf(stderr,_("%s: can't find module file %s\n"), program_name,
		list->name);

      }
      list = list->next;
    }
  }
}
