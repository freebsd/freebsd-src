/* File-name wildcard pattern matching for GNU.
   Copyright (C) 1985, 1988, 1989, 1990, 1991 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 1, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

/* To whomever it may concern: I have never seen the code which most
   Unix programs use to perform this function.  I wrote this from scratch
   based on specifications for the pattern matching.  --RMS.  */

#ifdef SHELL
#include "config.h"
#endif /* SHELL */

#include <sys/types.h>

#if defined (USGr3) && !defined (DIRENT)
#define DIRENT
#endif /* USGr3 */
#if defined (Xenix) && !defined (SYSNDIR)
#define SYSNDIR
#endif /* Xenix */

#if defined (POSIX) || defined (DIRENT) || defined (__GNU_LIBRARY__)
#include <dirent.h>
#define direct dirent
#define D_NAMLEN(d) strlen((d)->d_name)
#else /* not POSIX or DIRENT or __GNU_LIBRARY__ */
#define D_NAMLEN(d) ((d)->d_namlen)
#ifdef USG
#if defined (SYSNDIR)
#include <sys/ndir.h>
#else /* SYSNDIR */
#include "ndir.h"
#endif /* not SYSNDIR */
#else /* not USG */
#include <sys/dir.h>
#endif /* USG */
#endif /* POSIX or DIRENT or __GNU_LIBRARY__ */

#if defined (_POSIX_SOURCE)
/* Posix does not require that the d_ino field be present, and some
   systems do not provide it. */
#define REAL_DIR_ENTRY(dp) 1
#else
#define REAL_DIR_ENTRY(dp) (dp->d_ino != 0)
#endif /* _POSIX_SOURCE */

#if defined (STDC_HEADERS) || defined (__GNU_LIBRARY__)
#include <stdlib.h>
#include <string.h>
#define STDC_STRINGS
#else /* STDC_HEADERS or __GNU_LIBRARY__ */

#if defined (USG)
#include <string.h>
#ifndef POSIX
#include <memory.h>
#endif /* POSIX */
#define STDC_STRINGS
#else /* not USG */
#ifdef NeXT
#include <string.h>
#else /* NeXT */
#include <strings.h>
#endif /* NeXT */
/* Declaring bcopy causes errors on systems whose declarations are different.
   If the declaration is omitted, everything works fine.  */
#endif /* not USG */

extern char *malloc ();
extern char *realloc ();
extern void free ();

#ifndef NULL
#define NULL 0
#endif
#endif	/* Not STDC_HEADERS or __GNU_LIBRARY__.  */

#ifdef STDC_STRINGS
#define bcopy(s, d, n) memcpy ((d), (s), (n))
#define index strchr
#define rindex strrchr
#endif /* STDC_STRINGS */

#ifndef	alloca
#ifdef __GNUC__
#define alloca __builtin_alloca
#else /* Not GCC.  */
#ifdef sparc
#include <alloca.h>
#else /* Not sparc.  */
extern char *alloca ();
#endif /* sparc.  */
#endif /* GCC.  */
#endif

/* Nonzero if '*' and '?' do not match an initial '.' for glob_filename.  */
int noglob_dot_filenames = 1;

static int glob_match_after_star ();

#ifdef __FreeBSD__
static int collate_range_cmp (a, b)
	int a, b;
{
	int r;
	static char s[2][2];

	if ((unsigned char)a == (unsigned char)b)
		return 0;
	s[0][0] = a;
	s[1][0] = b;
	if ((r = strcoll(s[0], s[1])) == 0)
		r = (unsigned char)a - (unsigned char)b;
	return r;
}
#endif

/* Return nonzero if PATTERN has any special globbing chars in it.  */

int
glob_pattern_p (pattern)
     char *pattern;
{
  register char *p = pattern;
  register char c;
  int open = 0;

  while ((c = *p++) != '\0')
    switch (c)
      {
      case '?':
      case '*':
	return 1;

      case '[':		/* Only accept an open brace if there is a close */
	open++;		/* brace to match it.  Bracket expressions must be */
	continue;	/* complete, according to Posix.2 */
      case ']':
	if (open)
	  return 1;
	continue;

      case '\\':
	if (*p++ == '\0')
	  return 0;
      }

  return 0;
}


/* Match the pattern PATTERN against the string TEXT;
   return 1 if it matches, 0 otherwise.

   A match means the entire string TEXT is used up in matching.

   In the pattern string, `*' matches any sequence of characters,
   `?' matches any character, [SET] matches any character in the specified set,
   [!SET] matches any character not in the specified set.

   A set is composed of characters or ranges; a range looks like
   character hyphen character (as in 0-9 or A-Z).
   [0-9a-zA-Z_] is the set of characters allowed in C identifiers.
   Any other character in the pattern must be matched exactly.

   To suppress the special syntactic significance of any of `[]*?!-\',
   and match the character exactly, precede it with a `\'.

   If DOT_SPECIAL is nonzero,
   `*' and `?' do not match `.' at the beginning of TEXT.  */

int
glob_match (pattern, text, dot_special)
     char *pattern, *text;
     int dot_special;
{
  register char *p = pattern, *t = text;
  register char c;

  while ((c = *p++) != '\0')
    switch (c)
      {
      case '?':
	if (*t == '\0' || (dot_special && t == text && *t == '.'))
	  return 0;
	else
	  ++t;
	break;

      case '\\':
	if (*p++ != *t++)
	  return 0;
	break;

      case '*':
	if (dot_special && t == text && *t == '.')
	  return 0;
	return glob_match_after_star (p, t);

      case '[':
	{
	  register char c1 = *t++;
	  int invert;
	  char *cp1 = p;

	  if (c1 == '\0')
	    return 0;

	  invert = (*p == '!');

	  if (invert)
	    p++;

	  c = *p++;
	  while (1)
	    {
	      register char cstart = c, cend = c;

	      if (c == '\\')
		{
		  cstart = *p++;
		  cend = cstart;
		}

	      if (cstart == '\0')
		{
		  /* Missing ']'. */
		  if (c1 != '[')
		    return 0;
		  /* matched a single bracket */
		  p = cp1;
		  goto breakbracket;
		}

	      c = *p++;

	      if (c == '-')
		{
		  cend = *p++;
		  if (cend == '\\')
		    cend = *p++;
		  if (cend == '\0')
		    return 0;
		  c = *p++;
		}
#ifdef __FreeBSD__
	      if (   collate_range_cmp (c1, cstart) >= 0
		  && collate_range_cmp (c1, cend) <= 0
		 )
#else
	      if (c1 >= cstart && c1 <= cend)
#endif
		goto match;
	      if (c == ']')
		break;
	    }
	  if (!invert)
	    return 0;
	  break;

	match:
	  /* Skip the rest of the [...] construct that already matched.  */
	  while (c != ']')
	    {
	      if (c == '\0')
		return 0;
	      c = *p++;
	      if (c == '\0')
		return 0;
	      if (c == '\\')
		p++;
	    }
	  if (invert)
	    return 0;
	  breakbracket:
	  break;
	}

      default:
	if (c != *t++)
	  return 0;
      }

  return *t == '\0';
}

/* Like glob_match, but match PATTERN against any final segment of TEXT.  */

static int
glob_match_after_star (pattern, text)
     char *pattern, *text;
{
  register char *p = pattern, *t = text;
  register char c, c1;

  while ((c = *p++) == '?' || c == '*')
    if (c == '?' && *t++ == '\0')
      return 0;

  if (c == '\0')
    return 1;

  if (c == '\\')
    c1 = *p;
  else
    c1 = c;

  --p;
  while (1)
    {
      if ((c == '[' || *t == c1) && glob_match (p, t, 0))
	return 1;
      if (*t++ == '\0')
	return 0;
    }
}

/* Return a vector of names of files in directory DIR
   whose names match glob pattern PAT.
   The names are not in any particular order.
   Wildcards at the beginning of PAT do not match an initial period
   if noglob_dot_filenames is nonzero.

   The vector is terminated by an element that is a null pointer.

   To free the space allocated, first free the vector's elements,
   then free the vector.

   Return NULL if cannot get enough memory to hold the pointer
   and the names.

   Return -1 if cannot access directory DIR.
   Look in errno for more information.  */

char **
glob_vector (pat, dir)
     char *pat;
     char *dir;
{
  struct globval
  {
    struct globval *next;
    char *name;
  };

  DIR *d;
  register struct direct *dp;
  struct globval *lastlink;
  register struct globval *nextlink;
  register char *nextname;
  unsigned int count;
  int lose;
  register char **name_vector;
  register unsigned int i;
#ifdef ALLOCA_MISSING
  struct globval *templink;
#endif

  d = opendir (dir);
  if (d == NULL)
    return (char **) -1;

  lastlink = NULL;
  count = 0;
  lose = 0;

  /* Scan the directory, finding all names that match.
     For each name that matches, allocate a struct globval
     on the stack and store the name in it.
     Chain those structs together; lastlink is the front of the chain.  */
  while (1)
    {
#if defined (SHELL)
      /* Make globbing interruptible in the bash shell. */
      extern int interrupt_state;

      if (interrupt_state)
	{
	  closedir (d);
	  lose = 1;
	  goto lost;
	}
#endif /* SHELL */

      dp = readdir (d);
      if (dp == NULL)
	break;
      if (REAL_DIR_ENTRY (dp)
	  && glob_match (pat, dp->d_name, noglob_dot_filenames))
	{
#ifdef ALLOCA_MISSING
	  nextlink = (struct globval *) malloc (sizeof (struct globval));
#else
	  nextlink = (struct globval *) alloca (sizeof (struct globval));
#endif
	  nextlink->next = lastlink;
	  i = D_NAMLEN (dp) + 1;
	  nextname = (char *) malloc (i);
	  if (nextname == NULL)
	    {
	      lose = 1;
	      break;
	    }
	  lastlink = nextlink;
	  nextlink->name = nextname;
	  bcopy (dp->d_name, nextname, i);
	  count++;
	}
    }
  closedir (d);

  if (!lose)
    {
      name_vector = (char **) malloc ((count + 1) * sizeof (char *));
      lose |= name_vector == NULL;
    }

  /* Have we run out of memory?  */
#ifdef	SHELL
 lost:
#endif
  if (lose)
    {
      /* Here free the strings we have got.  */
      while (lastlink)
	{
	  free (lastlink->name);
#ifdef ALLOCA_MISSING
	  templink = lastlink->next;
	  free ((char *) lastlink);
	  lastlink = templink;
#else
	  lastlink = lastlink->next;
#endif
	}
      return NULL;
    }

  /* Copy the name pointers from the linked list into the vector.  */
  for (i = 0; i < count; ++i)
    {
      name_vector[i] = lastlink->name;
#ifdef ALLOCA_MISSING
      templink = lastlink->next;
      free ((char *) lastlink);
      lastlink = templink;
#else
      lastlink = lastlink->next;
#endif
    }

  name_vector[count] = NULL;
  return name_vector;
}

/* Return a new array, replacing ARRAY, which is the concatenation
   of each string in ARRAY to DIR.
   Return NULL if out of memory.  */

static char **
glob_dir_to_array (dir, array)
     char *dir, **array;
{
  register unsigned int i, l;
  int add_slash = 0;
  char **result;

  l = strlen (dir);
  if (l == 0)
    return array;

  if (dir[l - 1] != '/')
    add_slash++;

  for (i = 0; array[i] != NULL; i++)
    ;

  result = (char **) malloc ((i + 1) * sizeof (char *));
  if (result == NULL)
    return NULL;

  for (i = 0; array[i] != NULL; i++)
    {
      result[i] = (char *) malloc (1 + l + add_slash + strlen (array[i]));
      if (result[i] == NULL)
	return NULL;
      strcpy (result[i], dir);
      if (add_slash)
	result[i][l] = '/';
      strcpy (result[i] + l + add_slash, array[i]);
    }
  result[i] = NULL;

  /* Free the input array.  */
  for (i = 0; array[i] != NULL; i++)
    free (array[i]);
  free ((char *) array);
  return result;
}

/* Do globbing on PATHNAME.  Return an array of pathnames that match,
   marking the end of the array with a null-pointer as an element.
   If no pathnames match, then the array is empty (first element is null).
   If there isn't enough memory, then return NULL.
   If a file system error occurs, return -1; `errno' has the error code.

   Wildcards at the beginning of PAT, or following a slash,
   do not match an initial period if noglob_dot_filenames is nonzero.  */

char **
glob_filename (pathname)
     char *pathname;
{
  char **result;
  unsigned int result_size;
  char *directory_name, *filename;
  unsigned int directory_len;

  result = (char **) malloc (sizeof (char *));
  result_size = 1;
  if (result == NULL)
    return NULL;

  result[0] = NULL;

  /* Find the filename.  */
  filename = rindex (pathname, '/');
  if (filename == NULL)
    {
      filename = pathname;
      directory_name = "";
      directory_len = 0;
    }
  else
    {
      directory_len = (filename - pathname) + 1;
#ifdef ALLOCA_MISSING
      directory_name = (char *) malloc (directory_len + 1);
#else
      directory_name = (char *) alloca (directory_len + 1);
#endif
      bcopy (pathname, directory_name, directory_len);
      directory_name[directory_len] = '\0';
      ++filename;
    }

  /* If directory_name contains globbing characters, then we
     have to expand the previous levels.  Just recurse. */
  if (glob_pattern_p (directory_name))
    {
      char **directories;
      register unsigned int i;

      if (directory_name[directory_len - 1] == '/')
	directory_name[directory_len - 1] = '\0';

      directories = glob_filename (directory_name);
#ifdef ALLOCA_MISSING
      free ((char *) directory_name);
#endif
      if (directories == NULL)
	goto memory_error;
      else if (directories == (char **) -1)
	return (char **) -1;
      else if (*directories == NULL)
	{
	  free ((char *) directories);
	  return (char **) -1;
	}

      /* We have successfully globbed the preceding directory name.
	 For each name in DIRECTORIES, call glob_vector on it and
	 FILENAME.  Concatenate the results together.  */
      for (i = 0; directories[i] != NULL; i++)
	{
	  char **temp_results = glob_vector (filename, directories[i]);
	  if (temp_results == NULL)
	    goto memory_error;
	  else if (temp_results == (char **) -1)
	    /* This filename is probably not a directory.  Ignore it.  */
	    ;
	  else
	    {
	      char **array = glob_dir_to_array (directories[i], temp_results);
	      register unsigned int l;

	      l = 0;
	      while (array[l] != NULL)
		++l;

	      result = (char **) realloc (result,
					  (result_size + l) * sizeof (char *));
	      if (result == NULL)
		goto memory_error;

	      for (l = 0; array[l] != NULL; ++l)
		result[result_size++ - 1] = array[l];
	      result[result_size - 1] = NULL;
	      free ((char *) array);
	    }
	}
      /* Free the directories.  */
      for (i = 0; directories[i] != NULL; i++)
	free (directories[i]);
      free ((char *) directories);

      return result;
    }

  /* If there is only a directory name, return it. */
  if (*filename == '\0')
    {
      result = (char **) realloc ((char *) result, 2 * sizeof (char *));
      if (result != NULL)
	{
	  result[0] = (char *) malloc (directory_len + 1);
	  if (result[0] == NULL)
	    {
#ifdef ALLOCA_MISSING
	      free ((char *) directory_name);
#endif
	      goto memory_error;
	    }
	  bcopy (directory_name, result[0], directory_len + 1);
	  result[1] = NULL;
	}
#ifdef ALLOCA_MISSING
      free ((char *) directory_name);
#endif
      return result;
    }
  else
    {
      /* Otherwise, just return what glob_vector
	 returns appended to the directory name. */
      char **temp_results = glob_vector (filename,
					 (directory_len == 0
					  ? "." : directory_name));

      if (temp_results == NULL || temp_results == (char **) -1)
	{
#ifdef NO_ALLOCA
	  free ((char *) directory_name);
#endif
	  return temp_results;
	}

      temp_results = glob_dir_to_array (directory_name, temp_results);
#ifdef NO_ALLOCA
      free ((char *) directory_name);
#endif
      return temp_results;
    }

  /* We get to memory error if the program has run out of memory, or
     if this is the shell, and we have been interrupted. */
 memory_error:
  if (result != NULL)
    {
      register unsigned int i;
      for (i = 0; result[i] != NULL; ++i)
	free (result[i]);
      free ((char *) result);
    }
#if defined (SHELL)
  {
    extern int interrupt_state;

    if (interrupt_state)
      throw_to_top_level ();
  }
#endif /* SHELL */
  return NULL;
}

#ifdef TEST

main (argc, argv)
     int argc;
     char **argv;
{
  char **value;
  int i, optind;

  for (optind = 1; optind < argc; optind++)
    {
      value = glob_filename (argv[optind]);
      if (value == NULL)
	puts ("virtual memory exhausted");
      else if (value == (char **) -1)
	perror (argv[optind]);
      else
	for (i = 0; value[i] != NULL; i++)
	  puts (value[i]);
    }
  exit (0);
}

#endif /* TEST */
