/* GNU dump extensions to tar.
   Copyright (C) 1988, 1992, 1993 Free Software Foundation

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
#include <ctype.h>
#include <errno.h>
#ifndef STDC_HEADERS
extern int errno;
#endif
#include <time.h>
time_t time ();

#include "tar.h"
#include "port.h"

#ifndef S_ISLNK
#define lstat stat
#endif

extern time_t new_time;
extern FILE *msg_file;

void addname ();
int check_exclude ();
extern PTR ck_malloc ();
extern PTR ck_realloc ();
int confirm ();
extern PTR init_buffer ();
extern char *get_buffer ();
int is_dot_or_dotdot ();
extern void add_buffer ();
extern void flush_buffer ();
void name_gather ();
int recursively_delete ();
void skip_file ();
char *un_quote_string ();

extern char *new_name ();

static void add_dir_name ();

struct dirname
  {
    struct dirname *next;
    char *name;
    char *dir_text;
    int dev;
    int ino;
    int allnew;
  };
static struct dirname *dir_list;
static time_t this_time;

void
add_dir (name, dev, ino, text)
     char *name;
     char *text;
     dev_t dev;
     ino_t ino;
{
  struct dirname *dp;

  dp = (struct dirname *) ck_malloc (sizeof (struct dirname));
  if (!dp)
    abort ();
  dp->next = dir_list;
  dir_list = dp;
  dp->dev = dev;
  dp->ino = ino;
  dp->name = ck_malloc (strlen (name) + 1);
  strcpy (dp->name, name);
  dp->dir_text = text;
  dp->allnew = 0;
}

void
read_dir_file ()
{
  int dev;
  int ino;
  char *strp;
  FILE *fp;
  char buf[512];
  static char *path = 0;

  if (path == 0)
    path = ck_malloc (PATH_MAX);
  time (&this_time);
  if (gnu_dumpfile[0] != '/')
    {
#if defined(__MSDOS__) || defined(HAVE_GETCWD) || defined(_POSIX_VERSION)
      if (!getcwd (path, PATH_MAX))
	{
	  msg ("Couldn't get current directory.");
	  exit (EX_SYSTEM);
	}
#else
      char *getwd ();

      if (!getwd (path))
	{
	  msg ("Couldn't get current directory: %s", path);
	  exit (EX_SYSTEM);
	}
#endif
      /* If this doesn't fit, we're in serious trouble */
      strcat (path, "/");
      strcat (path, gnu_dumpfile);
      gnu_dumpfile = path;
    }
  fp = fopen (gnu_dumpfile, "r");
  if (fp == 0 && errno != ENOENT)
    {
      msg_perror ("Can't open %s", gnu_dumpfile);
      return;
    }
  if (!fp)
    return;
  fgets (buf, sizeof (buf), fp);
  if (!f_new_files)
    {
      f_new_files++;
      new_time = atol (buf);
    }
  while (fgets (buf, sizeof (buf), fp))
    {
      strp = &buf[strlen (buf)];
      if (strp[-1] == '\n')
	strp[-1] = '\0';
      strp = buf;
      dev = atol (strp);
      while (isdigit (*strp))
	strp++;
      ino = atol (strp);
      while (isspace (*strp))
	strp++;
      while (isdigit (*strp))
	strp++;
      strp++;
      add_dir (un_quote_string (strp), dev, ino, (char *) 0);
    }
  fclose (fp);
}

void
write_dir_file ()
{
  FILE *fp;
  struct dirname *dp;
  char *str;
  extern char *quote_copy_string ();

  fp = fopen (gnu_dumpfile, "w");
  if (fp == 0)
    {
      msg_perror ("Can't write to %s", gnu_dumpfile);
      return;
    }
  fprintf (fp, "%lu\n", this_time);
  for (dp = dir_list; dp; dp = dp->next)
    {
      if (!dp->dir_text)
	continue;
      str = quote_copy_string (dp->name);
      if (str)
	{
	  fprintf (fp, "%u %u %s\n", dp->dev, dp->ino, str);
	  free (str);
	}
      else
	fprintf (fp, "%u %u %s\n", dp->dev, dp->ino, dp->name);
    }
  fclose (fp);
}

struct dirname *
get_dir (name)
     char *name;
{
  struct dirname *dp;

  for (dp = dir_list; dp; dp = dp->next)
    {
      if (!strcmp (dp->name, name))
	return dp;
    }
  return 0;
}


/* Collect all the names from argv[] (or whatever), then expand them into
   a directory tree, and put all the directories at the beginning. */
void
collect_and_sort_names ()
{
  struct name *n, *n_next;
  int num_names;
  struct stat statbuf;
  int name_cmp ();
  char *merge_sort ();

  name_gather ();

  if (gnu_dumpfile)
    read_dir_file ();
  if (!namelist)
    addname (".");
  for (n = namelist; n; n = n_next)
    {
      n_next = n->next;
      if (n->found || n->dir_contents)
	continue;
      if (n->regexp)		/* FIXME just skip regexps for now */
	continue;
      if (n->change_dir)
	if (chdir (n->change_dir) < 0)
	  {
	    msg_perror ("can't chdir to %s", n->change_dir);
	    continue;
	  }

#ifdef AIX
      if (statx (n->name, &statbuf, STATSIZE, STX_HIDDEN | STX_LINK))
#else
      if (lstat (n->name, &statbuf) < 0)
#endif /* AIX */
	{
	  msg_perror ("can't stat %s", n->name);
	  continue;
	}
      if (S_ISDIR (statbuf.st_mode))
	{
	  n->found++;
	  add_dir_name (n->name, statbuf.st_dev);
	}
    }

  num_names = 0;
  for (n = namelist; n; n = n->next)
    num_names++;
  namelist = (struct name *) merge_sort ((PTR) namelist, num_names, (char *) (&(namelist->next)) - (char *) namelist, name_cmp);

  for (n = namelist; n; n = n->next)
    {
      n->found = 0;
    }
  if (gnu_dumpfile)
    write_dir_file ();
}

int
name_cmp (n1, n2)
     struct name *n1, *n2;
{
  if (n1->found)
    {
      if (n2->found)
	return strcmp (n1->name, n2->name);
      else
	return -1;
    }
  else if (n2->found)
    return 1;
  else
    return strcmp (n1->name, n2->name);
}

int
dirent_cmp (p1, p2)
     const PTR p1;
     const PTR p2;
{
  char *frst, *scnd;

  frst = (*(char **) p1) + 1;
  scnd = (*(char **) p2) + 1;

  return strcmp (frst, scnd);
}

char *
get_dir_contents (p, device)
     char *p;
     int device;
{
  DIR *dirp;
  register struct dirent *d;
  char *new_buf;
  char *namebuf;
  int bufsiz;
  int len;
  PTR the_buffer;
  char *buf;
  size_t n_strs;
  /*	int n_size;*/
  char *p_buf;
  char **vec, **p_vec;

  extern int errno;

  errno = 0;
  dirp = opendir (p);
  bufsiz = strlen (p) + NAMSIZ;
  namebuf = ck_malloc (bufsiz + 2);
  if (!dirp)
    {
      if (errno)
	msg_perror ("can't open directory %s", p);
      else
	msg ("error opening directory %s", p);
      new_buf = NULL;
    }
  else
    {
      struct dirname *dp;
      int all_children;

      dp = get_dir (p);
      all_children = dp ? dp->allnew : 0;
      (void) strcpy (namebuf, p);
      if (p[strlen (p) - 1] != '/')
	(void) strcat (namebuf, "/");
      len = strlen (namebuf);

      the_buffer = init_buffer ();
      while (d = readdir (dirp))
	{
	  struct stat hs;

	  /* Skip . and .. */
	  if (is_dot_or_dotdot (d->d_name))
	    continue;
	  if (NLENGTH (d) + len >= bufsiz)
	    {
	      bufsiz += NAMSIZ;
	      namebuf = ck_realloc (namebuf, bufsiz + 2);
	    }
	  (void) strcpy (namebuf + len, d->d_name);
#ifdef AIX
	  if (0 != f_follow_links ?
	      statx (namebuf, &hs, STATSIZE, STX_HIDDEN) :
	      statx (namebuf, &hs, STATSIZE, STX_HIDDEN | STX_LINK))
#else
	  if (0 != f_follow_links ? stat (namebuf, &hs) : lstat (namebuf, &hs))
#endif
	    {
	      msg_perror ("can't stat %s", namebuf);
	      continue;
	    }
	  if ((f_local_filesys && device != hs.st_dev)
	      || (f_exclude && check_exclude (namebuf)))
	    add_buffer (the_buffer, "N", 1);
#ifdef AIX
	  else if (S_ISHIDDEN (hs.st_mode))
	    {
	      add_buffer (the_buffer, "D", 1);
	      strcat (d->d_name, "A");
	      d->d_namlen++;
	    }
#endif /* AIX */
	  else if (S_ISDIR (hs.st_mode))
	    {
	      if (dp = get_dir (namebuf))
		{
		  if (dp->dev != hs.st_dev
		      || dp->ino != hs.st_ino)
		    {
		      if (f_verbose)
			msg ("directory %s has been renamed.", namebuf);
		      dp->allnew = 1;
		      dp->dev = hs.st_dev;
		      dp->ino = hs.st_ino;
		    }
		  dp->dir_text = "";
		}
	      else
		{
		  if (f_verbose)
		    msg ("Directory %s is new", namebuf);
		  add_dir (namebuf, hs.st_dev, hs.st_ino, "");
		  dp = get_dir (namebuf);
		  dp->allnew = 1;
		}
	      if (all_children)
		dp->allnew = 1;

	      add_buffer (the_buffer, "D", 1);
	    }
	  else if (!all_children
		   && f_new_files
		   && new_time > hs.st_mtime
		   && (f_new_files > 1
		       || new_time > hs.st_ctime))
	    add_buffer (the_buffer, "N", 1);
	  else
	    add_buffer (the_buffer, "Y", 1);
	  add_buffer (the_buffer, d->d_name, (int) (NLENGTH (d) + 1));
	}
      add_buffer (the_buffer, "\000\000", 2);
      closedir (dirp);

      /* Well, we've read in the contents of the dir, now sort them */
      buf = get_buffer (the_buffer);
      if (buf[0] == '\0')
	{
	  flush_buffer (the_buffer);
	  new_buf = NULL;
	}
      else
	{
	  n_strs = 0;
	  for (p_buf = buf; *p_buf;)
	    {
	      int tmp;

	      tmp = strlen (p_buf) + 1;
	      n_strs++;
	      p_buf += tmp;
	    }
	  vec = (char **) ck_malloc (sizeof (char *) * (n_strs + 1));
	  for (p_vec = vec, p_buf = buf; *p_buf; p_buf += strlen (p_buf) + 1)
	    *p_vec++ = p_buf;
	  *p_vec = 0;
	  qsort ((PTR) vec, n_strs, sizeof (char *), dirent_cmp);
	  new_buf = (char *) ck_malloc (p_buf - buf + 2);
	  for (p_vec = vec, p_buf = new_buf; *p_vec; p_vec++)
	    {
	      char *p_tmp;

	      for (p_tmp = *p_vec; *p_buf++ = *p_tmp++;)
		;
	    }
	  *p_buf++ = '\0';
	  free (vec);
	  flush_buffer (the_buffer);
	}
    }
  free (namebuf);
  return new_buf;
}

/* p is a directory.  Add all the files in P to the namelist.  If any of the
   files is a directory, recurse on the subdirectory. . . */
static void
add_dir_name (p, device)
     char *p;
     int device;
{
  char *new_buf;
  char *p_buf;

  char *namebuf;
  int buflen;
  register int len;
  int sublen;

  /*	PTR the_buffer;*/

  /*	char *buf;*/
  /*	char **vec,**p_vec;*/
  /*	int n_strs,n_size;*/

  struct name *n;

  int dirent_cmp ();

  new_buf = get_dir_contents (p, device);

  for (n = namelist; n; n = n->next)
    {
      if (!strcmp (n->name, p))
	{
	  n->dir_contents = new_buf ? new_buf : "\0\0\0\0";
	  break;
	}
    }

  if (new_buf)
    {
      len = strlen (p);
      buflen = NAMSIZ <= len ? len + NAMSIZ : NAMSIZ;
      namebuf = ck_malloc (buflen + 1);

      (void) strcpy (namebuf, p);
      if (namebuf[len - 1] != '/')
	{
	  namebuf[len++] = '/';
	  namebuf[len] = '\0';
	}
      for (p_buf = new_buf; *p_buf; p_buf += sublen + 1)
	{
	  sublen = strlen (p_buf);
	  if (*p_buf == 'D')
	    {
	      if (len + sublen >= buflen)
		{
		  buflen += NAMSIZ;
		  namebuf = ck_realloc (namebuf, buflen + 1);
		}
	      (void) strcpy (namebuf + len, p_buf + 1);
	      addname (namebuf);
	      add_dir_name (namebuf, device);
	    }
	}
      free (namebuf);
    }
}

/* Returns non-zero if p is . or ..   This could be a macro for speed. */
int
is_dot_or_dotdot (p)
     char *p;
{
  return (p[0] == '.' && (p[1] == '\0' || (p[1] == '.' && p[2] == '\0')));
}






void
gnu_restore (skipcrud)
     int skipcrud;
{
  char *current_dir;
  /*	int current_dir_length; */

  char *archive_dir;
  /*	int archive_dir_length; */
  PTR the_buffer;
  char *p;
  DIR *dirp;
  struct dirent *d;
  char *cur, *arc;
  extern struct stat hstat;	/* Stat struct corresponding */
  long size, copied;
  char *from, *to;
  extern union record *head;

  dirp = opendir (skipcrud + current_file_name);

  if (!dirp)
    {
      /* The directory doesn't exist now.  It'll be created.
			   In any case, we don't have to delete any files out
			   of it */
      skip_file ((long) hstat.st_size);
      return;
    }

  the_buffer = init_buffer ();
  while (d = readdir (dirp))
    {
      if (is_dot_or_dotdot (d->d_name))
	continue;

      add_buffer (the_buffer, d->d_name, (int) (NLENGTH (d) + 1));
    }
  closedir (dirp);
  add_buffer (the_buffer, "", 1);

  current_dir = get_buffer (the_buffer);
  archive_dir = (char *) ck_malloc (hstat.st_size);
  if (archive_dir == 0)
    {
      msg ("Can't allocate %d bytes for restore", hstat.st_size);
      skip_file ((long) hstat.st_size);
      return;
    }
  to = archive_dir;
  for (size = hstat.st_size; size > 0; size -= copied)
    {
      from = findrec ()->charptr;
      if (!from)
	{
	  msg ("Unexpected EOF in archive\n");
	  break;
	}
      copied = endofrecs ()->charptr - from;
      if (copied > size)
	copied = size;
      bcopy ((PTR) from, (PTR) to, (int) copied);
      to += copied;
      userec ((union record *) (from + copied - 1));
    }

  for (cur = current_dir; *cur; cur += strlen (cur) + 1)
    {
      for (arc = archive_dir; *arc; arc += strlen (arc) + 1)
	{
	  arc++;
	  if (!strcmp (arc, cur))
	    break;
	}
      if (*arc == '\0')
	{
	  p = new_name (skipcrud + current_file_name, cur);
	  if (f_confirm && !confirm ("delete", p))
	    {
	      free (p);
	      continue;
	    }
	  if (f_verbose)
	    fprintf (msg_file, "%s: deleting %s\n", tar, p);
	  if (recursively_delete (p))
	    {
	      msg ("%s: Error while deleting %s\n", tar, p);
	    }
	  free (p);
	}

    }
  flush_buffer (the_buffer);
  free (archive_dir);
}

int
recursively_delete (path)
     char *path;
{
  struct stat sbuf;
  DIR *dirp;
  struct dirent *dp;
  char *path_buf;
  /* int path_len; */


  if (lstat (path, &sbuf) < 0)
    return 1;
  if (S_ISDIR (sbuf.st_mode))
    {

      /* path_len=strlen(path); */
      dirp = opendir (path);
      if (dirp == 0)
	return 1;
      while (dp = readdir (dirp))
	{
	  if (is_dot_or_dotdot (dp->d_name))
	    continue;
	  path_buf = new_name (path, dp->d_name);
	  if (recursively_delete (path_buf))
	    {
	      free (path_buf);
	      closedir (dirp);
	      return 1;
	    }
	  free (path_buf);
	}
      closedir (dirp);

      if (rmdir (path) < 0)
	return 1;
      return 0;
    }
  if (unlink (path) < 0)
    return 1;
  return 0;
}
