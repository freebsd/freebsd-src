/* filesys.c -- filesystem specific functions.
   $Id: filesys.c,v 1.14 2002/03/02 15:05:04 karl Exp $

   Copyright (C) 1993, 97, 98, 2000 Free Software Foundation, Inc.

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
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

   Written by Brian Fox (bfox@ai.mit.edu). */

#include "info.h"

#include "tilde.h"
#include "filesys.h"

/* Local to this file. */
static char *info_file_in_path (), *lookup_info_filename ();
static char *info_absolute_file ();
static void remember_info_filename (), maybe_initialize_infopath ();

typedef struct
{
  char *suffix;
  char *decompressor;
} COMPRESSION_ALIST;

static char *info_suffixes[] = {
  ".info",
  "-info",
  "/index",
  ".inf",       /* 8+3 file on filesystem which supports long file names */
#ifdef __MSDOS__
  /* 8+3 file names strike again...  */
  ".in",        /* for .inz, .igz etc. */
  ".i",
#endif
  "",
  NULL
};

static COMPRESSION_ALIST compress_suffixes[] = {
  { ".gz", "gunzip" },
  { ".bz2", "bunzip2" },
  { ".z", "gunzip" },
  { ".Z", "uncompress" },
  { ".Y", "unyabba" },
#ifdef __MSDOS__
  { "gz", "gunzip" },
  { "z", "gunzip" },
#endif
  { (char *)NULL, (char *)NULL }
};

/* The path on which we look for info files.  You can initialize this
   from the environment variable INFOPATH if there is one, or you can
   call info_add_path () to add paths to the beginning or end of it.
   You can call zap_infopath () to make the path go away. */
char *infopath = (char *)NULL;
static int infopath_size = 0;

/* Expand the filename in PARTIAL to make a real name for this operating
   system.  This looks in INFO_PATHS in order to find the correct file.
   If it can't find the file, it returns NULL. */
static char *local_temp_filename = (char *)NULL;
static int local_temp_filename_size = 0;

char *
info_find_fullpath (partial)
     char *partial;
{
  int initial_character;
  char *temp;

  filesys_error_number = 0;

  maybe_initialize_infopath ();

  if (partial && (initial_character = *partial))
    {
      char *expansion;

      expansion = lookup_info_filename (partial);

      if (expansion)
        return (expansion);

      /* If we have the full path to this file, we still may have to add
         various extensions to it.  I guess we have to stat this file
         after all. */
      if (IS_ABSOLUTE (partial))
	temp = info_absolute_file (partial);
      else if (initial_character == '~')
        {
          expansion = tilde_expand_word (partial);
          if (IS_ABSOLUTE (expansion))
            {
              temp = info_absolute_file (expansion);
              free (expansion);
            }
          else
            temp = expansion;
        }
      else if (initial_character == '.' &&
               (IS_SLASH (partial[1]) ||
		(partial[1] == '.' && IS_SLASH (partial[2]))))
        {
          if (local_temp_filename_size < 1024)
            local_temp_filename = (char *)xrealloc
              (local_temp_filename, (local_temp_filename_size = 1024));
#if defined (HAVE_GETCWD)
          if (!getcwd (local_temp_filename, local_temp_filename_size))
#else /*  !HAVE_GETCWD */
          if (!getwd (local_temp_filename))
#endif /* !HAVE_GETCWD */
            {
              filesys_error_number = errno;
              return (partial);
            }

          strcat (local_temp_filename, "/");
          strcat (local_temp_filename, partial);
	  temp = info_absolute_file (local_temp_filename); /* try extensions */
	  if (!temp)
	    partial = local_temp_filename;
        }
      else
        temp = info_file_in_path (partial, infopath);

      if (temp)
        {
          remember_info_filename (partial, temp);
          if (strlen (temp) > local_temp_filename_size)
            local_temp_filename = (char *) xrealloc
              (local_temp_filename,
               (local_temp_filename_size = (50 + strlen (temp))));
          strcpy (local_temp_filename, temp);
          free (temp);
          return (local_temp_filename);
        }
    }
  return (partial);
}

/* Scan the list of directories in PATH looking for FILENAME.  If we find
   one that is a regular file, return it as a new string.  Otherwise, return
   a NULL pointer. */
static char *
info_file_in_path (filename, path)
     char *filename, *path;
{
  struct stat finfo;
  char *temp_dirname;
  int statable, dirname_index;

  /* Reject ridiculous cases up front, to prevent infinite recursion
     later on.  E.g., someone might say "info '(.)foo'"...  */
  if (!*filename || STREQ (filename, ".") || STREQ (filename, ".."))
    return NULL;

  dirname_index = 0;

  while ((temp_dirname = extract_colon_unit (path, &dirname_index)))
    {
      register int i, pre_suffix_length;
      char *temp;

      /* Expand a leading tilde if one is present. */
      if (*temp_dirname == '~')
        {
          char *expanded_dirname;

          expanded_dirname = tilde_expand_word (temp_dirname);
          free (temp_dirname);
          temp_dirname = expanded_dirname;
        }

      temp = (char *)xmalloc (30 + strlen (temp_dirname) + strlen (filename));
      strcpy (temp, temp_dirname);
      if (!IS_SLASH (temp[(strlen (temp)) - 1]))
        strcat (temp, "/");
      strcat (temp, filename);

      pre_suffix_length = strlen (temp);

      free (temp_dirname);

      for (i = 0; info_suffixes[i]; i++)
        {
          strcpy (temp + pre_suffix_length, info_suffixes[i]);

          statable = (stat (temp, &finfo) == 0);

          /* If we have found a regular file, then use that.  Else, if we
             have found a directory, look in that directory for this file. */
          if (statable)
            {
              if (S_ISREG (finfo.st_mode))
                {
                  return (temp);
                }
              else if (S_ISDIR (finfo.st_mode))
                {
                  char *newpath, *filename_only, *newtemp;

                  newpath = xstrdup (temp);
                  filename_only = filename_non_directory (filename);
                  newtemp = info_file_in_path (filename_only, newpath);

                  free (newpath);
                  if (newtemp)
                    {
                      free (temp);
                      return (newtemp);
                    }
                }
            }
          else
            {
              /* Add various compression suffixes to the name to see if
                 the file is present in compressed format. */
              register int j, pre_compress_suffix_length;

              pre_compress_suffix_length = strlen (temp);

              for (j = 0; compress_suffixes[j].suffix; j++)
                {
                  strcpy (temp + pre_compress_suffix_length,
                          compress_suffixes[j].suffix);

                  statable = (stat (temp, &finfo) == 0);
                  if (statable && (S_ISREG (finfo.st_mode)))
                    return (temp);
                }
            }
        }
      free (temp);
    }
  return ((char *)NULL);
}

/* Assume FNAME is an absolute file name, and check whether it is
   a regular file.  If it is, return it as a new string; otherwise
   return a NULL pointer.  We do it by taking the file name apart
   into its directory and basename parts, and calling info_file_in_path.*/
static char *
info_absolute_file (fname)
     char *fname;
{
  char *containing_dir = xstrdup (fname);
  char *base = filename_non_directory (containing_dir);

  if (base > containing_dir)
    base[-1] = '\0';

  return info_file_in_path (filename_non_directory (fname), containing_dir);
}

/* Given a string containing units of information separated by
   the PATH_SEP character, return the next one pointed to by
   IDX, or NULL if there are no more.
   Advance IDX to the character after the colon. */
char *
extract_colon_unit (string, idx)
     char *string;
     int *idx;
{
  register int i, start;

  i = start = *idx;
  if ((i >= strlen (string)) || !string)
    return ((char *) NULL);

  while (string[i] && string[i] != PATH_SEP[0])
    i++;
  if (i == start)
    {
      return ((char *) NULL);
    }
  else
    {
      char *value;

      value = (char *) xmalloc (1 + (i - start));
      strncpy (value, &string[start], (i - start));
      value[i - start] = '\0';
      if (string[i])
        ++i;
      *idx = i;
      return (value);
    }
}

/* A structure which associates a filename with its expansion. */
typedef struct {
  char *filename;
  char *expansion;
} FILENAME_LIST;

/* An array of remembered arguments and results. */
static FILENAME_LIST **names_and_files = (FILENAME_LIST **)NULL;
static int names_and_files_index = 0;
static int names_and_files_slots = 0;

/* Find the result for having already called info_find_fullpath () with
   FILENAME. */
static char *
lookup_info_filename (filename)
     char *filename;
{
  if (filename && names_and_files)
    {
      register int i;
      for (i = 0; names_and_files[i]; i++)
        {
          if (FILENAME_CMP (names_and_files[i]->filename, filename) == 0)
            return (names_and_files[i]->expansion);
        }
    }
  return (char *)NULL;;
}

/* Add a filename and its expansion to our list. */
static void
remember_info_filename (filename, expansion)
     char *filename, *expansion;
{
  FILENAME_LIST *new;

  if (names_and_files_index + 2 > names_and_files_slots)
    {
      int alloc_size;
      names_and_files_slots += 10;

      alloc_size = names_and_files_slots * sizeof (FILENAME_LIST *);

      names_and_files =
        (FILENAME_LIST **) xrealloc (names_and_files, alloc_size);
    }

  new = (FILENAME_LIST *)xmalloc (sizeof (FILENAME_LIST));
  new->filename = xstrdup (filename);
  new->expansion = expansion ? xstrdup (expansion) : (char *)NULL;

  names_and_files[names_and_files_index++] = new;
  names_and_files[names_and_files_index] = (FILENAME_LIST *)NULL;
}

static void
maybe_initialize_infopath ()
{
  if (!infopath_size)
    {
      infopath = (char *)
        xmalloc (infopath_size = (1 + strlen (DEFAULT_INFOPATH)));

      strcpy (infopath, DEFAULT_INFOPATH);
    }
}

/* Add PATH to the list of paths found in INFOPATH.  2nd argument says
   whether to put PATH at the front or end of INFOPATH. */
void
info_add_path (path, where)
     char *path;
     int where;
{
  int len;

  if (!infopath)
    {
      infopath = (char *)xmalloc (infopath_size = 200 + strlen (path));
      infopath[0] = '\0';
    }

  len = strlen (path) + strlen (infopath);

  if (len + 2 >= infopath_size)
    infopath = (char *)xrealloc (infopath, (infopath_size += (2 * len) + 2));

  if (!*infopath)
    strcpy (infopath, path);
  else if (where == INFOPATH_APPEND)
    {
      strcat (infopath, PATH_SEP);
      strcat (infopath, path);
    }
  else if (where == INFOPATH_PREPEND)
    {
      char *temp = xstrdup (infopath);
      strcpy (infopath, path);
      strcat (infopath, PATH_SEP);
      strcat (infopath, temp);
      free (temp);
    }
}

/* Make INFOPATH have absolutely nothing in it. */
void
zap_infopath ()
{
  if (infopath)
    free (infopath);

  infopath = (char *)NULL;
  infopath_size = 0;
}

/* Given a chunk of text and its length, convert all CRLF pairs at every
   end-of-line into a single Newline character.  Return the length of
   produced text.

   This is required because the rest of code is too entrenched in having
   a single newline at each EOL; in particular, searching for various
   Info headers and cookies can become extremely tricky if that assumption
   breaks.

   FIXME: this could also support Mac-style text files with a single CR
   at the EOL, but what about random CR characters in non-Mac files?  Can
   we afford converting them into newlines as well?  Maybe implement some
   heuristics here, like in Emacs 20.

   FIXME: is it a good idea to show the EOL type on the modeline?  */
long
convert_eols (text, textlen)
     char *text;
     long textlen;
{
  register char *s = text;
  register char *d = text;

  while (textlen--)
    {
      if (*s == '\r' && textlen && s[1] == '\n')
	{
	  s++;
	  textlen--;
	}
      *d++ = *s++;
    }

  return (long)(d - text);
}

/* Read the contents of PATHNAME, returning a buffer with the contents of
   that file in it, and returning the size of that buffer in FILESIZE.
   FINFO is a stat struct which has already been filled in by the caller.
   If the file turns out to be compressed, set IS_COMPRESSED to non-zero.
   If the file cannot be read, return a NULL pointer. */
char *
filesys_read_info_file (pathname, filesize, finfo, is_compressed)
     char *pathname;
     long *filesize;
     struct stat *finfo;
     int *is_compressed;
{
  long st_size;

  *filesize = filesys_error_number = 0;

  if (compressed_filename_p (pathname))
    {
      *is_compressed = 1;
      return (filesys_read_compressed (pathname, filesize, finfo));
    }
  else
    {
      int descriptor;
      char *contents;

      *is_compressed = 0;
      descriptor = open (pathname, O_RDONLY | O_BINARY, 0666);

      /* If the file couldn't be opened, give up. */
      if (descriptor < 0)
        {
          filesys_error_number = errno;
          return ((char *)NULL);
        }

      /* Try to read the contents of this file. */
      st_size = (long) finfo->st_size;
      contents = (char *)xmalloc (1 + st_size);
      if ((read (descriptor, contents, st_size)) != st_size)
        {
	  filesys_error_number = errno;
	  close (descriptor);
	  free (contents);
	  return ((char *)NULL);
        }

      close (descriptor);

      /* Convert any DOS-style CRLF EOLs into Unix-style NL.
	 Seems like a good idea to have even on Unix, in case the Info
	 files are coming from some Windows system across a network.  */
      *filesize = convert_eols (contents, st_size);

      /* EOL conversion can shrink the text quite a bit.  We don't
	 want to waste storage.  */
      if (*filesize < st_size)
	contents = (char *)xrealloc (contents, 1 + *filesize);

      return (contents);
    }
}

/* Typically, pipe buffers are 4k. */
#define BASIC_PIPE_BUFFER (4 * 1024)

/* We use some large multiple of that. */
#define FILESYS_PIPE_BUFFER_SIZE (16 * BASIC_PIPE_BUFFER)

char *
filesys_read_compressed (pathname, filesize, finfo)
     char *pathname;
     long *filesize;
     struct stat *finfo;
{
  FILE *stream;
  char *command, *decompressor;
  char *contents = (char *)NULL;

  *filesize = filesys_error_number = 0;

  decompressor = filesys_decompressor_for_file (pathname);

  if (!decompressor)
    return ((char *)NULL);

  command = (char *)xmalloc (15 + strlen (pathname) + strlen (decompressor));
  /* Explicit .exe suffix makes the diagnostics of `popen'
     better on systems where COMMAND.COM is the stock shell.  */
  sprintf (command, "%s%s < %s",
	   decompressor, STRIP_DOT_EXE ? ".exe" : "", pathname);

#if !defined (BUILDING_LIBRARY)
  if (info_windows_initialized_p)
    {
      char *temp;

      temp = (char *)xmalloc (5 + strlen (command));
      sprintf (temp, "%s...", command);
      message_in_echo_area ("%s", temp);
      free (temp);
    }
#endif /* !BUILDING_LIBRARY */

  stream = popen (command, FOPEN_RBIN);
  free (command);

  /* Read chunks from this file until there are none left to read. */
  if (stream)
    {
      long offset, size;
      char *chunk;
    
      offset = size = 0;
      chunk = (char *)xmalloc (FILESYS_PIPE_BUFFER_SIZE);

      while (1)
        {
          int bytes_read;

          bytes_read = fread (chunk, 1, FILESYS_PIPE_BUFFER_SIZE, stream);

          if (bytes_read + offset >= size)
            contents = (char *)xrealloc
              (contents, size += (2 * FILESYS_PIPE_BUFFER_SIZE));

          memcpy (contents + offset, chunk, bytes_read);
          offset += bytes_read;
          if (bytes_read != FILESYS_PIPE_BUFFER_SIZE)
            break;
        }

      free (chunk);
      if (pclose (stream) == -1)
	{
	  if (contents)
	    free (contents);
	  contents = (char *)NULL;
	  filesys_error_number = errno;
	}
      else
	{
	  *filesize = convert_eols (contents, offset);
	  contents = (char *)xrealloc (contents, 1 + *filesize);
	}
    }
  else
    {
      filesys_error_number = errno;
    }

#if !defined (BUILDING_LIBARARY)
  if (info_windows_initialized_p)
    unmessage_in_echo_area ();
#endif /* !BUILDING_LIBRARY */
  return (contents);
}

/* Return non-zero if FILENAME belongs to a compressed file. */
int
compressed_filename_p (filename)
     char *filename;
{
  char *decompressor;

  /* Find the final extension of this filename, and see if it matches one
     of our known ones. */
  decompressor = filesys_decompressor_for_file (filename);

  if (decompressor)
    return (1);
  else
    return (0);
}

/* Return the command string that would be used to decompress FILENAME. */
char *
filesys_decompressor_for_file (filename)
     char *filename;
{
  register int i;
  char *extension = (char *)NULL;

  /* Find the final extension of FILENAME, and see if it appears in our
     list of known compression extensions. */
  for (i = strlen (filename) - 1; i > 0; i--)
    if (filename[i] == '.')
      {
        extension = filename + i;
        break;
      }

  if (!extension)
    return ((char *)NULL);

  for (i = 0; compress_suffixes[i].suffix; i++)
    if (FILENAME_CMP (extension, compress_suffixes[i].suffix) == 0)
      return (compress_suffixes[i].decompressor);

#if defined (__MSDOS__)
  /* If no other suffix matched, allow any extension which ends
     with `z' to be decompressed by gunzip.  Due to limited 8+3 DOS
     file namespace, we can expect many such cases, and supporting
     every weird suffix thus produced would be a pain.  */
  if (extension[strlen (extension) - 1] == 'z' ||
      extension[strlen (extension) - 1] == 'Z')
    return "gunzip";
#endif

  return ((char *)NULL);
}

/* The number of the most recent file system error. */
int filesys_error_number = 0;

/* A function which returns a pointer to a static buffer containing
   an error message for FILENAME and ERROR_NUM. */
static char *errmsg_buf = (char *)NULL;
static int errmsg_buf_size = 0;

char *
filesys_error_string (filename, error_num)
     char *filename;
     int error_num;
{
  int len;
  char *result;

  if (error_num == 0)
    return ((char *)NULL);

  result = strerror (error_num);

  len = 4 + strlen (filename) + strlen (result);
  if (len >= errmsg_buf_size)
    errmsg_buf = (char *)xrealloc (errmsg_buf, (errmsg_buf_size = 2 + len));

  sprintf (errmsg_buf, "%s: %s", filename, result);
  return (errmsg_buf);
}


/* Check for "dir" with all the possible info and compression suffixes,
   in combination.  */

int
is_dir_name (filename)
    char *filename;
{
  unsigned i;

  for (i = 0; info_suffixes[i]; i++)
    {
      unsigned c;
      char trydir[50];
      strcpy (trydir, "dir");
      strcat (trydir, info_suffixes[i]);
      
      if (strcasecmp (filename, trydir) == 0)
        return 1;

      for (c = 0; compress_suffixes[c].suffix; c++)
        {
          char dir_compressed[50]; /* can be short */
          strcpy (dir_compressed, trydir); 
          strcat (dir_compressed, compress_suffixes[c].suffix);
          if (strcasecmp (filename, dir_compressed) == 0)
            return 1;
        }
    }  

  return 0;
}
