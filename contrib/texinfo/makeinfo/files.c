/* files.c -- file-related functions for makeinfo.
   $Id: files.c,v 1.10 2002/01/16 15:52:45 karl Exp $

   Copyright (C) 1998, 99, 2000, 01, 02 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#include "system.h"
#include "files.h"
#include "macro.h"
#include "makeinfo.h"

FSTACK *filestack = NULL;

static int node_filename_stack_index = 0;
static int node_filename_stack_size = 0;
static char **node_filename_stack = NULL;


/* Looking for include files.  */

/* Given a string containing units of information separated by colons,
   return the next one pointed to by INDEX, or NULL if there are no more.
   Advance INDEX to the character after the colon. */
static char *
extract_colon_unit (string, index)
     char *string;
     int *index;
{
  int start;
  int path_sep_char = PATH_SEP[0];
  int i = *index;

  if (!string || (i >= strlen (string)))
    return NULL;

  /* Each call to this routine leaves the index pointing at a colon if
     there is more to the path.  If i > 0, then increment past the
     `:'.  If i == 0, then the path has a leading colon.  Trailing colons
     are handled OK by the `else' part of the if statement; an empty
     string is returned in that case. */
  if (i && string[i] == path_sep_char)
    i++;

  start = i;
  while (string[i] && string[i] != path_sep_char) i++;
  *index = i;

  if (i == start)
    {
      if (string[i])
        (*index)++;

      /* Return "" in the case of a trailing `:'. */
      return xstrdup ("");
    }
  else
    {
      char *value;

      value = xmalloc (1 + (i - start));
      memcpy (value, &string[start], (i - start));
      value [i - start] = 0;

      return value;
    }
}

/* Return the full pathname for FILENAME by searching along PATH.
   When found, return the stat () info for FILENAME in FINFO.
   If PATH is NULL, only the current directory is searched.
   If the file could not be found, return a NULL pointer. */
static char *
get_file_info_in_path (filename, path, finfo)
     char *filename, *path;
     struct stat *finfo;
{
  char *dir;
  int result, index = 0;

  if (path == NULL)
    path = ".";

  /* Handle absolute pathnames.  */
  if (IS_ABSOLUTE (filename)
      || (*filename == '.'
          && (IS_SLASH (filename[1])
              || (filename[1] == '.' && IS_SLASH (filename[2])))))
    {
      if (stat (filename, finfo) == 0)
        return xstrdup (filename);
      else
        return NULL;
    }

  while ((dir = extract_colon_unit (path, &index)))
    {
      char *fullpath;

      if (!*dir)
        {
          free (dir);
          dir = xstrdup (".");
        }

      fullpath = xmalloc (2 + strlen (dir) + strlen (filename));
      sprintf (fullpath, "%s/%s", dir, filename);
      free (dir);

      result = stat (fullpath, finfo);

      if (result == 0)
        return fullpath;
      else
        free (fullpath);
    }
  return NULL;
}

/* Find and load the file named FILENAME.  Return a pointer to
   the loaded file, or NULL if it can't be loaded. */
char *
find_and_load (filename)
     char *filename;
{
  struct stat fileinfo;
  long file_size;
  int file = -1, count = 0;
  char *fullpath, *result;
  int n, bytes_to_read;

  result = fullpath = NULL;

  fullpath = get_file_info_in_path (filename, include_files_path, &fileinfo);

  if (!fullpath)
    goto error_exit;

  filename = fullpath;
  file_size = (long) fileinfo.st_size;

  file = open (filename, O_RDONLY);
  if (file < 0)
    goto error_exit;

  /* Load the file, with enough room for a newline and a null. */
  result = xmalloc (file_size + 2);

  /* VMS stat lies about the st_size value.  The actual number of
     readable bytes is always less than this value.  The arcane
     mysteries of VMS/RMS are too much to probe, so this hack
    suffices to make things work.  It's also needed on Cygwin.  And so
    we might as well use it everywhere.  */
  bytes_to_read = file_size;
  while ((n = read (file, result + count, bytes_to_read)) > 0)
    {
      count += n;
      bytes_to_read -= n;
    }
  if (0 < count && count < file_size)
    result = xrealloc (result, count + 2); /* why waste the slack? */
  else if (n == -1)
error_exit:
    {
      if (result)
        free (result);

      if (fullpath)
        free (fullpath);

      if (file != -1)
        close (file);

      return NULL;
    }
  close (file);

  /* Set the globals to the new file. */
  input_text = result;
  input_text_length = count;
  input_filename = fullpath;
  node_filename = xstrdup (fullpath);
  input_text_offset = 0;
  line_number = 1;
  /* Not strictly necessary.  This magic prevents read_token () from doing
     extra unnecessary work each time it is called (that is a lot of times).
     INPUT_TEXT_LENGTH is one past the actual end of the text. */
  input_text[input_text_length] = '\n';
  /* This, on the other hand, is always necessary.  */
  input_text[input_text_length+1] = 0;
  return result;
}

/* Pushing and popping files.  */
void
push_node_filename ()
{
  if (node_filename_stack_index + 1 > node_filename_stack_size)
    node_filename_stack = xrealloc
    (node_filename_stack, (node_filename_stack_size += 10) * sizeof (char *));

  node_filename_stack[node_filename_stack_index] = node_filename;
  node_filename_stack_index++;
}

void
pop_node_filename ()
{
  node_filename = node_filename_stack[--node_filename_stack_index];
}

/* Save the state of the current input file. */
void
pushfile ()
{
  FSTACK *newstack = xmalloc (sizeof (FSTACK));
  newstack->filename = input_filename;
  newstack->text = input_text;
  newstack->size = input_text_length;
  newstack->offset = input_text_offset;
  newstack->line_number = line_number;
  newstack->next = filestack;

  filestack = newstack;
  push_node_filename ();
}

/* Make the current file globals be what is on top of the file stack. */
void
popfile ()
{
  FSTACK *tos = filestack;

  if (!tos)
    abort ();                   /* My fault.  I wonder what I did? */

  if (macro_expansion_output_stream)
    {
      maybe_write_itext (input_text, input_text_offset);
      forget_itext (input_text);
    }

  /* Pop the stack. */
  filestack = filestack->next;

  /* Make sure that commands with braces have been satisfied. */
  if (!executing_string && !me_executing_string)
    discard_braces ();

  /* Get the top of the stack into the globals. */
  input_filename = tos->filename;
  input_text = tos->text;
  input_text_length = tos->size;
  input_text_offset = tos->offset;
  line_number = tos->line_number;
  free (tos);

  /* Go back to the (now) current node. */
  pop_node_filename ();
}

/* Flush all open files on the file stack. */
void
flush_file_stack ()
{
  while (filestack)
    {
      char *fname = input_filename;
      char *text = input_text;
      popfile ();
      free (fname);
      free (text);
    }
}

/* Return the index of the first character in the filename
   which is past all the leading directory characters.  */
static int
skip_directory_part (filename)
     char *filename;
{
  int i = strlen (filename) - 1;

  while (i && !IS_SLASH (filename[i]))
    i--;
  if (IS_SLASH (filename[i]))
    i++;
  else if (filename[i] && HAVE_DRIVE (filename))
    i = 2;

  return i;
}

char *
filename_non_directory (name)
     char *name;
{
  return xstrdup (name + skip_directory_part (name));
}

/* Return just the simple part of the filename; i.e. the
   filename without the path information, or extensions.
   This conses up a new string. */
char *
filename_part (filename)
     char *filename;
{
  char *basename = filename_non_directory (filename);

#ifdef REMOVE_OUTPUT_EXTENSIONS
  /* See if there is an extension to remove.  If so, remove it. */
  {
    char *temp;

    temp = strrchr (basename, '.');
    if (temp)
      *temp = 0;
  }
#endif /* REMOVE_OUTPUT_EXTENSIONS */
  return basename;
}

/* Return the pathname part of filename.  This can be NULL. */
char *
pathname_part (filename)
     char *filename;
{
  char *expand_filename ();
  char *result = NULL;
  int i;

  filename = expand_filename (filename, "");

  i = skip_directory_part (filename);
  if (i)
    {
      result = xmalloc (1 + i);
      strncpy (result, filename, i);
      result[i] = 0;
    }
  free (filename);
  return result;
}

/* Return the expansion of FILENAME. */
char *
expand_filename (filename, input_name)
     char *filename, *input_name;
{
  int i;
  char *full_pathname ();

  if (filename)
    {
      filename = full_pathname (filename);
      if (IS_ABSOLUTE (filename)
	  || (*filename == '.' &&
	      (IS_SLASH (filename[1]) ||
	       (filename[1] == '.' && IS_SLASH (filename[2])))))
	return filename;
    }
  else
    {
      filename = filename_non_directory (input_name);

      if (!*filename)
        {
          free (filename);
          filename = xstrdup ("noname.texi");
        }

      for (i = strlen (filename) - 1; i; i--)
        if (filename[i] == '.')
          break;

      if (!i)
        i = strlen (filename);

      if (i + 6 > (strlen (filename)))
        filename = xrealloc (filename, i + 6);
      strcpy (filename + i, html ? ".html" : ".info");
      return filename;
    }

  if (IS_ABSOLUTE (input_name))
    {
      /* Make it so that relative names work. */
      char *result;
      
      i = strlen (input_name) - 1;

      result = xmalloc (1 + strlen (input_name) + strlen (filename));
      strcpy (result, input_name);

      while (!IS_SLASH (result[i]) && i)
        i--;
      if (IS_SLASH (result[i]))
        i++;

      strcpy (&result[i], filename);
      free (filename);
      return result;
    }
  return filename;
}

/* Return the full path to FILENAME. */
char *
full_pathname (filename)
     char *filename;
{
  int initial_character;
  char *result;

  /* No filename given? */
  if (!filename || !*filename)
    return xstrdup ("");
  
  /* Already absolute? */
  if (IS_ABSOLUTE (filename) ||
      (*filename == '.' &&
       (IS_SLASH (filename[1]) ||
        (filename[1] == '.' && IS_SLASH (filename[2])))))
    return xstrdup (filename);

  initial_character = *filename;
  if (initial_character != '~')
    {
      char *localdir = xmalloc (1025);
#ifdef HAVE_GETCWD
      if (!getcwd (localdir, 1024))
#else
      if (!getwd (localdir))
#endif
        {
          fprintf (stderr, _("%s: getwd: %s, %s\n"),
                   progname, filename, localdir);
          xexit (1);
        }

      strcat (localdir, "/");
      strcat (localdir, filename);
      result = xstrdup (localdir);
      free (localdir);
    }
  else
    { /* Does anybody know why WIN32 doesn't want to support $HOME?
         If the reason is they don't have getpwnam, they should
         only disable the else clause below.  */
#ifndef WIN32
      if (IS_SLASH (filename[1]))
        {
          /* Return the concatenation of the environment variable HOME
             and the rest of the string. */
          char *temp_home;

          temp_home = (char *) getenv ("HOME");
          result = xmalloc (strlen (&filename[1])
                                    + 1
                                    + temp_home ? strlen (temp_home)
                                    : 0);
          *result = 0;

          if (temp_home)
            strcpy (result, temp_home);

          strcat (result, &filename[1]);
        }
      else
        {
          struct passwd *user_entry;
          int i, c;
          char *username = xmalloc (257);

          for (i = 1; (c = filename[i]); i++)
            {
              if (IS_SLASH (c))
                break;
              else
                username[i - 1] = c;
            }
          if (c)
            username[i - 1] = 0;

          user_entry = getpwnam (username);

          if (!user_entry)
            return xstrdup (filename);

          result = xmalloc (1 + strlen (user_entry->pw_dir)
                                    + strlen (&filename[i]));
          strcpy (result, user_entry->pw_dir);
          strcat (result, &filename[i]);
        }
#endif /* not WIN32 */
    }
  return result;
}

char *
output_name_from_input_name (name)
     char *name;
{
  return expand_filename (NULL, name);
}


/* Modify the file name FNAME so that it fits the limitations of the
   underlying filesystem.  In particular, truncate the file name as it
   would be truncated by the filesystem.  We assume the result can
   never be longer than the original, otherwise we couldn't be sure we
   have enough space in the original string to modify it in place.  */
char *
normalize_filename (fname)
     char *fname;
{
  int maxlen;
  char orig[PATH_MAX + 1];
  int i;
  char *lastdot, *p;

#ifdef _PC_NAME_MAX
  maxlen = pathconf (fname, _PC_NAME_MAX);
  if (maxlen < 1)
#endif
    maxlen = PATH_MAX;

  i = skip_directory_part (fname);
  if (fname[i] == '\0')
    return fname;	/* only a directory name -- don't modify */
  strcpy (orig, fname + i);

  switch (maxlen)
    {
      case 12:	/* MS-DOS 8+3 filesystem */
	if (orig[0] == '.')	/* leading dots are not allowed */
	  orig[0] = '_';
	lastdot = strrchr (orig, '.');
	if (!lastdot)
	  lastdot = orig + strlen (orig);
	strncpy (fname + i, orig, lastdot - orig);
	for (p = fname + i;
	     p < fname + i + (lastdot - orig) && p < fname + i + 8;
	     p++)
	  if (*p == '.')
	    *p = '_';
	*p = '\0';
	if (*lastdot == '.')
	  strncat (fname + i, lastdot, 4);
	break;
      case 14:	/* old Unix systems with 14-char limitation */
	strcpy (fname + i, orig);
	if (strlen (fname + i) > 14)
	  fname[i + 14] = '\0';
	break;
      default:
	strcpy (fname + i, orig);
	if (strlen (fname) > maxlen - 1)
	  fname[maxlen - 1] = '\0';
	break;
    }

  return fname;
}
