/* Various processing of names.

   Copyright 1988, 1992, 1994, 1996, 1997, 1998, 1999, 2000, 2001 Free
   Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the
   Free Software Foundation; either version 2, or (at your option) any later
   version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General
   Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation, Inc.,
   59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

/* $FreeBSD$ */

#include "system.h"

#include <fnmatch.h>
#include <grp.h>
#include <hash.h>
#include <pwd.h>
#include <quotearg.h>

#include "common.h"

/* User and group names.  */

struct group *getgrnam ();
struct passwd *getpwnam ();
#if ! HAVE_DECL_GETPWUID
struct passwd *getpwuid ();
#endif
#if ! HAVE_DECL_GETGRGID
struct group *getgrgid ();
#endif

/* Make sure you link with the proper libraries if you are running the
   Yellow Peril (thanks for the good laugh, Ian J.!), or, euh... NIS.
   This code should also be modified for non-UNIX systems to do something
   reasonable.  */

static char cached_uname[UNAME_FIELD_SIZE];
static char cached_gname[GNAME_FIELD_SIZE];

static uid_t cached_uid;	/* valid only if cached_uname is not empty */
static gid_t cached_gid;	/* valid only if cached_gname is not empty */

/* These variables are valid only if nonempty.  */
static char cached_no_such_uname[UNAME_FIELD_SIZE];
static char cached_no_such_gname[GNAME_FIELD_SIZE];

/* These variables are valid only if nonzero.  It's not worth optimizing
   the case for weird systems where 0 is not a valid uid or gid.  */
static uid_t cached_no_such_uid;
static gid_t cached_no_such_gid;

/* Given UID, find the corresponding UNAME.  */
void
uid_to_uname (uid_t uid, char uname[UNAME_FIELD_SIZE])
{
  struct passwd *passwd;

  if (uid != 0 && uid == cached_no_such_uid)
    {
      *uname = '\0';
      return;
    }

  if (!cached_uname[0] || uid != cached_uid)
    {
      passwd = getpwuid (uid);
      if (passwd)
	{
	  cached_uid = uid;
	  strncpy (cached_uname, passwd->pw_name, UNAME_FIELD_SIZE);
	}
      else
	{
	  cached_no_such_uid = uid;
	  *uname = '\0';
	  return;
	}
    }
  strncpy (uname, cached_uname, UNAME_FIELD_SIZE);
}

/* Given GID, find the corresponding GNAME.  */
void
gid_to_gname (gid_t gid, char gname[GNAME_FIELD_SIZE])
{
  struct group *group;

  if (gid != 0 && gid == cached_no_such_gid)
    {
      *gname = '\0';
      return;
    }

  if (!cached_gname[0] || gid != cached_gid)
    {
      group = getgrgid (gid);
      if (group)
	{
	  cached_gid = gid;
	  strncpy (cached_gname, group->gr_name, GNAME_FIELD_SIZE);
	}
      else
	{
	  cached_no_such_gid = gid;
	  *gname = '\0';
	  return;
	}
    }
  strncpy (gname, cached_gname, GNAME_FIELD_SIZE);
}

/* Given UNAME, set the corresponding UID and return 1, or else, return 0.  */
int
uname_to_uid (char uname[UNAME_FIELD_SIZE], uid_t *uidp)
{
  struct passwd *passwd;

  if (cached_no_such_uname[0]
      && strncmp (uname, cached_no_such_uname, UNAME_FIELD_SIZE) == 0)
    return 0;

  if (!cached_uname[0]
      || uname[0] != cached_uname[0]
      || strncmp (uname, cached_uname, UNAME_FIELD_SIZE) != 0)
    {
      passwd = getpwnam (uname);
      if (passwd)
	{
	  cached_uid = passwd->pw_uid;
	  strncpy (cached_uname, uname, UNAME_FIELD_SIZE);
	}
      else
	{
	  strncpy (cached_no_such_uname, uname, UNAME_FIELD_SIZE);
	  return 0;
	}
    }
  *uidp = cached_uid;
  return 1;
}

/* Given GNAME, set the corresponding GID and return 1, or else, return 0.  */
int
gname_to_gid (char gname[GNAME_FIELD_SIZE], gid_t *gidp)
{
  struct group *group;

  if (cached_no_such_gname[0]
      && strncmp (gname, cached_no_such_gname, GNAME_FIELD_SIZE) == 0)
    return 0;

  if (!cached_gname[0]
      || gname[0] != cached_gname[0]
      || strncmp (gname, cached_gname, GNAME_FIELD_SIZE) != 0)
    {
      group = getgrnam (gname);
      if (group)
	{
	  cached_gid = group->gr_gid;
	  strncpy (cached_gname, gname, GNAME_FIELD_SIZE);
	}
      else
	{
	  strncpy (cached_no_such_gname, gname, GNAME_FIELD_SIZE);
	  return 0;
	}
    }
  *gidp = cached_gid;
  return 1;
}

/* Names from the command call.  */

static struct name *namelist;	/* first name in list, if any */
static struct name **nametail = &namelist;	/* end of name list */
static const char **name_array;	/* store an array of names */
static int allocated_names;	/* how big is the array? */
static int names;		/* how many entries does it have? */
static int name_index;		/* how many of the entries have we scanned? */

/* Initialize structures.  */
void
init_names (void)
{
  allocated_names = 10;
  name_array = xmalloc (sizeof (const char *) * allocated_names);
  names = 0;
}

/* Add NAME at end of name_array, reallocating it as necessary.  */
void
name_add (const char *name)
{
  if (names == allocated_names)
    {
      allocated_names *= 2;
      name_array =
	xrealloc (name_array, sizeof (const char *) * allocated_names);
    }
  name_array[names++] = name;
}

/* Names from external name file.  */

static FILE *name_file;		/* file to read names from */
static char *name_buffer;	/* buffer to hold the current file name */
static size_t name_buffer_length; /* allocated length of name_buffer */

/* FIXME: I should better check more closely.  It seems at first glance that
   is_pattern is only used when reading a file, and ignored for all
   command line arguments.  */

static inline int
is_pattern (const char *string)
{
  return strchr (string, '*') || strchr (string, '[') || strchr (string, '?');
}

/* Set up to gather file names for tar.  They can either come from a
   file or were saved from decoding arguments.  */
void
name_init (int argc, char *const *argv)
{
  name_buffer = xmalloc (NAME_FIELD_SIZE + 2);
  name_buffer_length = NAME_FIELD_SIZE;

  if (files_from_option)
    {
      if (!strcmp (files_from_option, "-"))
	{
	  request_stdin ("-T");
	  name_file = stdin;
	}
      else if (name_file = fopen (files_from_option, "r"), !name_file)
	open_fatal (files_from_option);
    }
}

void
name_term (void)
{
  free (name_buffer);
  free (name_array);
}

/* Read the next filename from name_file and null-terminate it.  Put
   it into name_buffer, reallocating and adjusting name_buffer_length
   if necessary.  Return 0 at end of file, 1 otherwise.  */
static int
read_name_from_file (void)
{
  int character;
  size_t counter = 0;

  /* FIXME: getc may be called even if character was EOF the last time here.  */

  /* FIXME: This + 2 allocation might serve no purpose.  */

  while (character = getc (name_file),
	 character != EOF && character != filename_terminator)
    {
      if (counter == name_buffer_length)
	{
	  if (name_buffer_length * 2 < name_buffer_length)
	    xalloc_die ();
	  name_buffer_length *= 2;
	  name_buffer = xrealloc (name_buffer, name_buffer_length + 2);
	}
      name_buffer[counter++] = character;
    }

  if (counter == 0 && character == EOF)
    return 0;

  if (counter == name_buffer_length)
    {
      if (name_buffer_length * 2 < name_buffer_length)
	xalloc_die ();
      name_buffer_length *= 2;
      name_buffer = xrealloc (name_buffer, name_buffer_length + 2);
    }
  name_buffer[counter] = '\0';

  return 1;
}

/* Get the next name from ARGV or the file of names.  Result is in
   static storage and can't be relied upon across two calls.

   If CHANGE_DIRS is true, treat a filename of the form "-C" as
   meaning that the next filename is the name of a directory to change
   to.  If filename_terminator is NUL, CHANGE_DIRS is effectively
   always false.  */
char *
name_next (int change_dirs)
{
  const char *source;
  char *cursor;
  int chdir_flag = 0;

  if (filename_terminator == '\0')
    change_dirs = 0;

  while (1)
    {
      /* Get a name, either from file or from saved arguments.  */

      if (name_index == names)
	{
	  if (! name_file)
	    break;
	  if (! read_name_from_file ())
	    break;
	}
      else
	{
	  size_t source_len;
	  source = name_array[name_index++];
	  source_len = strlen (source);
	  if (name_buffer_length < source_len)
	    {
	      do
		{
		  name_buffer_length *= 2;
		  if (! name_buffer_length)
		    xalloc_die ();
		}
	      while (name_buffer_length < source_len);

	      free (name_buffer);
	      name_buffer = xmalloc (name_buffer_length + 2);
	    }
	  strcpy (name_buffer, source);
	}

      /* Zap trailing slashes.  */

      cursor = name_buffer + strlen (name_buffer) - 1;
      while (cursor > name_buffer && ISSLASH (*cursor))
	*cursor-- = '\0';

      if (chdir_flag)
	{
	  if (chdir (name_buffer) < 0)
	    chdir_fatal (name_buffer);
	  chdir_flag = 0;
	}
      else if (change_dirs && strcmp (name_buffer, "-C") == 0)
	chdir_flag = 1;
      else
	{
	  unquote_string (name_buffer);
	  return name_buffer;
	}
    }

  /* No more names in file.  */

  if (name_file && chdir_flag)
    FATAL_ERROR ((0, 0, _("Missing file name after -C")));

  return 0;
}

/* Close the name file, if any.  */
void
name_close (void)
{
  if (name_file && name_file != stdin)
    if (fclose (name_file) != 0)
      close_error (name_buffer);
}

/* Gather names in a list for scanning.  Could hash them later if we
   really care.

   If the names are already sorted to match the archive, we just read
   them one by one.  name_gather reads the first one, and it is called
   by name_match as appropriate to read the next ones.  At EOF, the
   last name read is just left in the buffer.  This option lets users
   of small machines extract an arbitrary number of files by doing
   "tar t" and editing down the list of files.  */

void
name_gather (void)
{
  /* Buffer able to hold a single name.  */
  static struct name *buffer;
  static size_t allocated_size;

  char const *name;

  if (same_order_option)
    {
      static int change_dir;

      if (allocated_size == 0)
	{
	  allocated_size = offsetof (struct name, name) + NAME_FIELD_SIZE + 1;
	  buffer = xmalloc (allocated_size);
	  /* FIXME: This memset is overkill, and ugly...  */
	  memset (buffer, 0, allocated_size);
	}

      while ((name = name_next (0)) && strcmp (name, "-C") == 0)
	{
	  char const *dir = name_next (0);
	  if (! dir)
	    FATAL_ERROR ((0, 0, _("Missing file name after -C")));
	  change_dir = chdir_arg (xstrdup (dir));
	}

      if (name)
	{
	  size_t needed_size;
	  buffer->length = strlen (name);
	  needed_size = offsetof (struct name, name) + buffer->length + 1;
	  if (allocated_size < needed_size)
	    {
	      do
		{
		  allocated_size *= 2;
		  if (! allocated_size)
		    xalloc_die ();
		}
	      while (allocated_size < needed_size);

	      buffer = xrealloc (buffer, allocated_size);
	    }
	  buffer->change_dir = change_dir;
	  strcpy (buffer->name, name);
	  buffer->next = 0;
	  buffer->found = 0;

	  namelist = buffer;
	  nametail = &namelist->next;
	}
    }
  else
    {
      /* Non sorted names -- read them all in.  */
      int change_dir = 0;

      for (;;)
	{
	  int change_dir0 = change_dir;
	  while ((name = name_next (0)) && strcmp (name, "-C") == 0)
	    {
	      char const *dir = name_next (0);
	      if (! dir)
		FATAL_ERROR ((0, 0, _("Missing file name after -C")));
	      change_dir = chdir_arg (xstrdup (dir));
	    }
	  if (name)
	    addname (name, change_dir);
	  else
	    {
	      if (change_dir != change_dir0)
		addname (0, change_dir);
	      break;
	    }
	}
    }
}

/*  Add a name to the namelist.  */
struct name *
addname (char const *string, int change_dir)
{
  size_t length = string ? strlen (string) : 0;
  struct name *name = xmalloc (offsetof (struct name, name) + length + 1);

  if (string)
    {
      name->fake = 0;
      strcpy (name->name, string);
    }
  else
    {
      name->fake = 1;

      /* FIXME: This initialization (and the byte of memory that it
	 initializes) is probably not needed, but we are currently in
	 bug-fix mode so we'll leave it in for now.  */
      name->name[0] = 0;
    }

  name->next = 0;
  name->length = length;
  name->found = 0;
  name->regexp = 0;		/* assume not a regular expression */
  name->firstch = 1;		/* assume first char is literal */
  name->change_dir = change_dir;
  name->dir_contents = 0;

  if (string && is_pattern (string))
    {
      name->regexp = 1;
      if (string[0] == '*' || string[0] == '[' || string[0] == '?')
	name->firstch = 0;
    }

  *nametail = name;
  nametail = &name->next;
  return name;
}

/* Find a match for PATH (whose string length is LENGTH) in the name
   list.  */
static struct name *
namelist_match (char const *path, size_t length)
{
  struct name *p;

  for (p = namelist; p; p = p->next)
    {
      /* If first chars don't match, quick skip.  */

      if (p->firstch && p->name[0] != path[0])
	continue;

      if (p->regexp
	  ? fnmatch (p->name, path, recursion_option) == 0
	  : (p->length <= length
	     && (path[p->length] == '\0' || ISSLASH (path[p->length]))
	     && memcmp (path, p->name, p->length) == 0))
	return p;
    }

  return 0;
}

/* Return true if and only if name PATH (from an archive) matches any
   name from the namelist.  */
int
name_match (const char *path)
{
  size_t length = strlen (path);

  while (1)
    {
      struct name *cursor = namelist;
      struct name *tmpnlp;

      if (!cursor)
	return ! files_from_option;

      if (cursor->fake)
	{
	  chdir_do (cursor->change_dir);
	  namelist = 0;
	  nametail = &namelist;
	  return ! files_from_option;
	}

      cursor = namelist_match (path, length);
      if (cursor)
	{
	  cursor->found = 1; /* remember it matched */
	  if (starting_file_option)
	    {
	      free (namelist);
	      namelist = 0;
	      nametail = &namelist;
	    }
	  chdir_do (cursor->change_dir);
	  if (fast_read_option)
	    {
	    /* remove the current entry, since we found a match */
	      if (namelist->next == NULL)
	        {
	          /* the list contains one element */
	          free(namelist);
	          namelist = 0;
	          nametail = &namelist;
	          /* set a boolean to decide wether we started with a */
	          /* non-empty  namelist, that was emptied */
	          namelist_freed = 1;
	        }
	      else
	        {
	          if (cursor == namelist)
	            {
	              /* the first element is the one */
	              tmpnlp = namelist->next;
	              free(namelist);
	              namelist = tmpnlp;
	            }
	          else
	            {
	              tmpnlp = namelist;
	              while (tmpnlp->next != cursor)
	                tmpnlp = tmpnlp->next;
	              tmpnlp->next = cursor->next;
	              free(cursor);
	            }
	        }
	    }
  
	  /* We got a match.  */
	  return 1;
	}

      /* Filename from archive not found in namelist.  If we have the whole
	 namelist here, just return 0.  Otherwise, read the next name in and
	 compare it.  If this was the last name, namelist->found will remain
	 on.  If not, we loop to compare the newly read name.  */

      if (same_order_option && namelist->found)
	{
	  name_gather ();	/* read one more */
	  if (namelist->found)
	    return 0;
	}
      else
	return 0;
    }
}

/* Print the names of things in the namelist that were not matched.  */
void
names_notfound (void)
{
  struct name const *cursor;

  for (cursor = namelist; cursor; cursor = cursor->next)
    if (!cursor->found && !cursor->fake)
      ERROR ((0, 0, _("%s: Not found in archive"),
	      quotearg_colon (cursor->name)));

  /* Don't bother freeing the name list; we're about to exit.  */
  namelist = 0;
  nametail = &namelist;

  if (same_order_option)
    {
      char *name;

      while (name = name_next (1), name)
	ERROR ((0, 0, _("%s: Not found in archive"),
		quotearg_colon (name)));
    }
}

/* Sorting name lists.  */

/* Sort linked LIST of names, of given LENGTH, using COMPARE to order
   names.  Return the sorted list.  Apart from the type `struct name'
   and the definition of SUCCESSOR, this is a generic list-sorting
   function, but it's too painful to make it both generic and portable
   in C.  */

static struct name *
merge_sort (struct name *list, int length,
	    int (*compare) (struct name const*, struct name const*))
{
  struct name *first_list;
  struct name *second_list;
  int first_length;
  int second_length;
  struct name *result;
  struct name **merge_point;
  struct name *cursor;
  int counter;

# define SUCCESSOR(name) ((name)->next)

  if (length == 1)
    return list;

  if (length == 2)
    {
      if ((*compare) (list, SUCCESSOR (list)) > 0)
	{
	  result = SUCCESSOR (list);
	  SUCCESSOR (result) = list;
	  SUCCESSOR (list) = 0;
	  return result;
	}
      return list;
    }

  first_list = list;
  first_length = (length + 1) / 2;
  second_length = length / 2;
  for (cursor = list, counter = first_length - 1;
       counter;
       cursor = SUCCESSOR (cursor), counter--)
    continue;
  second_list = SUCCESSOR (cursor);
  SUCCESSOR (cursor) = 0;

  first_list = merge_sort (first_list, first_length, compare);
  second_list = merge_sort (second_list, second_length, compare);

  merge_point = &result;
  while (first_list && second_list)
    if ((*compare) (first_list, second_list) < 0)
      {
	cursor = SUCCESSOR (first_list);
	*merge_point = first_list;
	merge_point = &SUCCESSOR (first_list);
	first_list = cursor;
      }
    else
      {
	cursor = SUCCESSOR (second_list);
	*merge_point = second_list;
	merge_point = &SUCCESSOR (second_list);
	second_list = cursor;
      }
  if (first_list)
    *merge_point = first_list;
  else
    *merge_point = second_list;

  return result;

#undef SUCCESSOR
}

/* A comparison function for sorting names.  Put found names last;
   break ties by string comparison.  */

static int
compare_names (struct name const *n1, struct name const *n2)
{
  int found_diff = n2->found - n1->found;
  return found_diff ? found_diff : strcmp (n1->name, n2->name);
}

/* Add all the dirs under NAME, which names a directory, to the namelist.
   If any of the files is a directory, recurse on the subdirectory.
   DEVICE is the device not to leave, if the -l option is specified.  */

static void
add_hierarchy_to_namelist (struct name *name, dev_t device)
{
  char *path = name->name;
  char *buffer = get_directory_contents (path, device);

  if (! buffer)
    name->dir_contents = "\0\0\0\0";
  else
    {
      size_t name_length = name->length;
      size_t allocated_length = (name_length >= NAME_FIELD_SIZE
				 ? name_length + NAME_FIELD_SIZE
				 : NAME_FIELD_SIZE);
      char *name_buffer = xmalloc (allocated_length + 1);
				/* FIXME: + 2 above?  */
      char *string;
      size_t string_length;
      int change_dir = name->change_dir;

      name->dir_contents = buffer;
      strcpy (name_buffer, path);
      if (! ISSLASH (name_buffer[name_length - 1]))
	{
	  name_buffer[name_length++] = '/';
	  name_buffer[name_length] = '\0';
	}

      for (string = buffer; *string; string += string_length + 1)
	{
	  string_length = strlen (string);
	  if (*string == 'D')
	    {
	      if (allocated_length <= name_length + string_length)
		{
		  do
		    {
		      allocated_length *= 2;
		      if (! allocated_length)
			xalloc_die ();
		    }
		  while (allocated_length <= name_length + string_length);

		  name_buffer = xrealloc (name_buffer, allocated_length + 1);
		}
	      strcpy (name_buffer + name_length, string + 1);
	      add_hierarchy_to_namelist (addname (name_buffer, change_dir),
					 device);
	    }
	}

      free (name_buffer);
    }
}

/* Collect all the names from argv[] (or whatever), expand them into a
   directory tree, and sort them.  This gets only subdirectories, not
   all files.  */

void
collect_and_sort_names (void)
{
  struct name *name;
  struct name *next_name;
  int num_names;
  struct stat statbuf;

  name_gather ();

  if (listed_incremental_option)
    read_directory_file ();

  if (!namelist)
    addname (".", 0);

  for (name = namelist; name; name = next_name)
    {
      next_name = name->next;
      if (name->found || name->dir_contents)
	continue;
      if (name->regexp)		/* FIXME: just skip regexps for now */
	continue;
      chdir_do (name->change_dir);
      if (name->fake)
	continue;

      if (deref_stat (dereference_option, name->name, &statbuf) != 0)
	{
	  if (ignore_failed_read_option)
	    stat_warn (name->name);
	  else
	    stat_error (name->name);
	  continue;
	}
      if (S_ISDIR (statbuf.st_mode))
	{
	  name->found = 1;
	  add_hierarchy_to_namelist (name, statbuf.st_dev);
	}
    }

  num_names = 0;
  for (name = namelist; name; name = name->next)
    num_names++;
  namelist = merge_sort (namelist, num_names, compare_names);

  for (name = namelist; name; name = name->next)
    name->found = 0;
}

/* This is like name_match, except that it returns a pointer to the
   name it matched, and doesn't set FOUND in structure.  The caller
   will have to do that if it wants to.  Oh, and if the namelist is
   empty, it returns null, unlike name_match, which returns TRUE.  */
struct name *
name_scan (const char *path)
{
  size_t length = strlen (path);

  while (1)
    {
      struct name *cursor = namelist_match (path, length);
      if (cursor)
	return cursor;

      /* Filename from archive not found in namelist.  If we have the whole
	 namelist here, just return 0.  Otherwise, read the next name in and
	 compare it.  If this was the last name, namelist->found will remain
	 on.  If not, we loop to compare the newly read name.  */

      if (same_order_option && namelist && namelist->found)
	{
	  name_gather ();	/* read one more */
	  if (namelist->found)
	    return 0;
	}
      else
	return 0;
    }
}

/* This returns a name from the namelist which doesn't have ->found
   set.  It sets ->found before returning, so successive calls will
   find and return all the non-found names in the namelist.  */
struct name *gnu_list_name;

char *
name_from_list (void)
{
  if (!gnu_list_name)
    gnu_list_name = namelist;
  while (gnu_list_name && (gnu_list_name->found | gnu_list_name->fake))
    gnu_list_name = gnu_list_name->next;
  if (gnu_list_name)
    {
      gnu_list_name->found = 1;
      chdir_do (gnu_list_name->change_dir);
      return gnu_list_name->name;
    }
  return 0;
}

void
blank_name_list (void)
{
  struct name *name;

  gnu_list_name = 0;
  for (name = namelist; name; name = name->next)
    name->found = 0;
}

/* Yield a newly allocated file name consisting of PATH concatenated to
   NAME, with an intervening slash if PATH does not already end in one.  */
char *
new_name (const char *path, const char *name)
{
  size_t pathlen = strlen (path);
  size_t namesize = strlen (name) + 1;
  int slash = pathlen && ! ISSLASH (path[pathlen - 1]);
  char *buffer = xmalloc (pathlen + slash + namesize);
  memcpy (buffer, path, pathlen);
  buffer[pathlen] = '/';
  memcpy (buffer + pathlen + slash, name, namesize);
  return buffer;
}

/* Return nonzero if file NAME is excluded.  Exclude a name if its
   prefix matches a pattern that contains slashes, or if one of its
   components matches a pattern that contains no slashes.  */
bool
excluded_name (char const *name)
{
  return excluded_filename (excluded, name + FILESYSTEM_PREFIX_LEN (name));
}

/* Names to avoid dumping.  */
static Hash_table *avoided_name_table;

/* Calculate the hash of an avoided name.  */
static unsigned
hash_avoided_name (void const *name, unsigned n_buckets)
{
  return hash_string (name, n_buckets);
}

/* Compare two avoided names for equality.  */
static bool
compare_avoided_names (void const *name1, void const *name2)
{
  return strcmp (name1, name2) == 0;
}

/* Remember to not archive NAME.  */
void
add_avoided_name (char const *name)
{
  if (! ((avoided_name_table
	  || (avoided_name_table = hash_initialize (0, 0, hash_avoided_name,
						    compare_avoided_names, 0)))
	 && hash_insert (avoided_name_table, xstrdup (name))))
    xalloc_die ();
}

/* Should NAME be avoided when archiving?  */
int
is_avoided_name (char const *name)
{
  return avoided_name_table && hash_lookup (avoided_name_table, name);
}
