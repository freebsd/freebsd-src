/* mangle.c -- encode long filenames
   Copyright (C) 1988, 1992 Free Software Foundation

This file is part of GNU Tar.

GNU Tar is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GNU Tar is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU Tar; see the file COPYING.  If not, write to
the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

#include <stdio.h>
#include <sys/types.h>
#include <time.h>
time_t time ();

#include "tar.h"
#include "port.h"

void add_buffer ();
extern PTR ck_malloc ();
void finish_header ();
extern PTR init_buffer ();
extern char *quote_copy_string ();
extern char *get_buffer ();
char *un_quote_string ();

extern union record *start_header ();

extern struct stat hstat;	/* Stat struct corresponding */

struct mangled
  {
    struct mangled *next;
    int type;
    char mangled[NAMSIZ];
    char *linked_to;
    char normal[1];
  };


/* Should use a hash table, etc. .  */
struct mangled *first_mangle;
int mangled_num = 0;

#if 0				/* Deleted because there is now a better way to do all this */

char *
find_mangled (name)
     char *name;
{
  struct mangled *munge;

  for (munge = first_mangle; munge; munge = munge->next)
    if (!strcmp (name, munge->normal))
      return munge->mangled;
  return 0;
}


#ifdef S_ISLNK
void
add_symlink_mangle (symlink, linkto, buffer)
     char *symlink;
     char *linkto;
     char *buffer;
{
  struct mangled *munge, *kludge;

  munge = (struct mangled *) ck_malloc (sizeof (struct mangled) + strlen (symlink) + strlen (linkto) + 2);
  if (!first_mangle)
    first_mangle = munge;
  else
    {
      for (kludge = first_mangle; kludge->next; kludge = kludge->next)
	;
      kludge->next = munge;
    }
  munge->type = 1;
  munge->next = 0;
  strcpy (munge->normal, symlink);
  munge->linked_to = munge->normal + strlen (symlink) + 1;
  strcpy (munge->linked_to, linkto);
  sprintf (munge->mangled, "@@MaNgLeD.%d", mangled_num++);
  strncpy (buffer, munge->mangled, NAMSIZ);
}

#endif

void
add_mangle (name, buffer)
     char *name;
     char *buffer;
{
  struct mangled *munge, *kludge;

  munge = (struct mangled *) ck_malloc (sizeof (struct mangled) + strlen (name));
  if (!first_mangle)
    first_mangle = munge;
  else
    {
      for (kludge = first_mangle; kludge->next; kludge = kludge->next)
	;
      kludge->next = munge;
    }
  munge->next = 0;
  munge->type = 0;
  strcpy (munge->normal, name);
  sprintf (munge->mangled, "@@MaNgLeD.%d", mangled_num++);
  strncpy (buffer, munge->mangled, NAMSIZ);
}

void
write_mangled ()
{
  struct mangled *munge;
  struct stat hstat;
  union record *header;
  char *ptr1, *ptr2;
  PTR the_buffer;
  int size;
  int bufsize;

  if (!first_mangle)
    return;
  the_buffer = init_buffer ();
  for (munge = first_mangle, size = 0; munge; munge = munge->next)
    {
      ptr1 = quote_copy_string (munge->normal);
      if (!ptr1)
	ptr1 = munge->normal;
      if (munge->type)
	{
	  add_buffer (the_buffer, "Symlink ", 8);
	  add_buffer (the_buffer, ptr1, strlen (ptr1));
	  add_buffer (the_buffer, " to ", 4);

	  if (ptr2 = quote_copy_string (munge->linked_to))
	    {
	      add_buffer (the_buffer, ptr2, strlen (ptr2));
	      free (ptr2);
	    }
	  else
	    add_buffer (the_buffer, munge->linked_to, strlen (munge->linked_to));
	}
      else
	{
	  add_buffer (the_buffer, "Rename ", 7);
	  add_buffer (the_buffer, munge->mangled, strlen (munge->mangled));
	  add_buffer (the_buffer, " to ", 4);
	  add_buffer (the_buffer, ptr1, strlen (ptr1));
	}
      add_buffer (the_buffer, "\n", 1);
      if (ptr1 != munge->normal)
	free (ptr1);
    }

  bzero (&hstat, sizeof (struct stat));
  hstat.st_atime = hstat.st_mtime = hstat.st_ctime = time (0);
  ptr1 = get_buffer (the_buffer);
  hstat.st_size = strlen (ptr1);

  header = start_header ("././@MaNgLeD_NaMeS", &hstat);
  header->header.linkflag = LF_NAMES;
  finish_header (header);
  size = hstat.st_size;
  header = findrec ();
  bufsize = endofrecs ()->charptr - header->charptr;

  while (bufsize < size)
    {
      bcopy (ptr1, header->charptr, bufsize);
      ptr1 += bufsize;
      size -= bufsize;
      userec (header + (bufsize - 1) / RECORDSIZE);
      header = findrec ();
      bufsize = endofrecs ()->charptr - header->charptr;
    }
  bcopy (ptr1, header->charptr, size);
  bzero (header->charptr + size, bufsize - size);
  userec (header + (size - 1) / RECORDSIZE);
}

#endif

void
extract_mangle (head)
     union record *head;
{
  char *buf;
  char *fromtape;
  char *to;
  char *ptr, *ptrend;
  char *nam1, *nam1end;
  int size;
  int copied;

  size = hstat.st_size;
  buf = to = ck_malloc (size + 1);
  buf[size] = '\0';
  while (size > 0)
    {
      fromtape = findrec ()->charptr;
      if (fromtape == 0)
	{
	  msg ("Unexpected EOF in mangled names!");
	  return;
	}
      copied = endofrecs ()->charptr - fromtape;
      if (copied > size)
	copied = size;
      bcopy (fromtape, to, copied);
      to += copied;
      size -= copied;
      userec ((union record *) (fromtape + copied - 1));
    }
  for (ptr = buf; *ptr; ptr = ptrend)
    {
      ptrend = index (ptr, '\n');
      *ptrend++ = '\0';

      if (!strncmp (ptr, "Rename ", 7))
	{
	  nam1 = ptr + 7;
	  nam1end = index (nam1, ' ');
	  while (strncmp (nam1end, " to ", 4))
	    {
	      nam1end++;
	      nam1end = index (nam1end, ' ');
	    }
	  *nam1end = '\0';
	  if (ptrend[-2] == '/')
	    ptrend[-2] = '\0';
	  un_quote_string (nam1end + 4);
	  if (rename (nam1, nam1end + 4))
	    msg_perror ("Can't rename %s to %s", nam1, nam1end + 4);
	  else if (f_verbose)
	    msg ("Renamed %s to %s", nam1, nam1end + 4);
	}
#ifdef S_ISLNK
      else if (!strncmp (ptr, "Symlink ", 8))
	{
	  nam1 = ptr + 8;
	  nam1end = index (nam1, ' ');
	  while (strncmp (nam1end, " to ", 4))
	    {
	      nam1end++;
	      nam1end = index (nam1end, ' ');
	    }
	  *nam1end = '\0';
	  un_quote_string (nam1);
	  un_quote_string (nam1end + 4);
	  if (symlink (nam1, nam1end + 4) && (unlink (nam1end + 4) || symlink (nam1, nam1end + 4)))
	    msg_perror ("Can't symlink %s to %s", nam1, nam1end + 4);
	  else if (f_verbose)
	    msg ("Symlinkd %s to %s", nam1, nam1end + 4);
	}
#endif
      else
	msg ("Unknown demangling command %s", ptr);
    }
}
