/* tcexparg.c - Unix-style command line wildcards for Turbo C 2.0

   This file is in the public domain.

   Compile your main program with -Dmain=_main and link with this file.

   After that, it is just as if the operating system had expanded the
   arguments, except that they are not sorted.  The program name and all
   arguments that are expanded from wildcards are lowercased.

   Syntax for wildcards:
   *		Matches zero or more of any character (except a '.' at
		the beginning of a name).
   ?		Matches any single character.
   [r3z]	Matches 'r', '3', or 'z'.
   [a-d]	Matches a single character in the range 'a' through 'd'.
   [!a-d]	Matches any single character except a character in the
		range 'a' through 'd'.

   The period between the filename root and its extension need not be
   given explicitly.  Thus, the pattern `a*e' will match 'abacus.exe'
   and 'axyz.e' as well as 'apple'.  Comparisons are not case sensitive.

   Authors:
   The expargs code is a modification of wildcard expansion code
   written for Turbo C 1.0 by
   Richard Hargrove
   Texas Instruments, Inc.
   P.O. Box 869305, m/s 8473
   Plano, Texas 75086
   214/575-4128
   and posted to USENET in September, 1987.

   The wild_match code was written by Rich Salz, rsalz@bbn.com,
   posted to net.sources in November, 1986.

   The code connecting the two is by Mike Slomin, bellcore!lcuxa!mike2,
   posted to comp.sys.ibm.pc in November, 1988.

   Major performance enhancements and bug fixes, and source cleanup,
   by David MacKenzie, djm@gnu.ai.mit.edu.  */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dos.h>
#include <dir.h>

/* Number of new arguments to allocate space for at a time.  */
#define ARGS_INCREMENT 10

/* The name this program was run with, for error messages.  */
static char *program_name;

static char **grow_argv (char **new_argv, int new_argc);
static void fatal_error (const char *message);

int wild_match (char *string, char *pattern);
char *basename (char *path);

char **expargs (int *, char **);

#ifdef main
#undef main
#endif

int
main (int argc, char **argv, char **envp)
{
  argv = expargs (&argc, argv);
  return _main (argc, argv, envp);
}

char **
expargs (int *pargc, char **argv)
{
  char path[MAXPATH + 1];
  char **new_argv;
  struct ffblk block;
  char *path_base;
  char *arg_base;
  int argind;
  int new_argc;
  int path_length;
  int matched;

  program_name = argv[0];
  if (program_name && *program_name)
    strlwr (program_name);
  new_argv = grow_argv (NULL, 0);
  new_argv[0] = argv[0];
  new_argc = 1;

  for (argind = 1; argind < *pargc; ++argind)
    {
      matched = 0;
      if (strpbrk (argv[argind], "?*[") != NULL)
	{
	  strncpy (path, argv[argind], MAXPATH - 3);
	  path_base = basename (path);
	  strcpy (path_base, "*.*");
	  arg_base = argv[argind] + (path_base - path);

	  if (!findfirst (path, &block, FA_DIREC))
	    {
	      strlwr (path);
	      do
		{
		  /* Only match "." and ".." explicitly.  */
		  if (*block.ff_name == '.' && *arg_base != '.')
		    continue;
		  path_length = stpcpy (path_base, block.ff_name) - path + 1;
		  strlwr (path_base);
		  if (wild_match (path, argv[argind]))
		    {
		      matched = 1;
		      new_argv[new_argc] = (char *) malloc (path_length);
		      if (new_argv[new_argc] == NULL)
			fatal_error ("memory exhausted");
		      strcpy (new_argv[new_argc++], path);
		      new_argv = grow_argv (new_argv, new_argc);
		    }
	      } while (!findnext (&block));
	    }
	}
      if (matched == 0)
	new_argv[new_argc++] = argv[argind];
      new_argv = grow_argv (new_argv, new_argc);
    }

  *pargc = new_argc;
  new_argv[new_argc] = NULL;
  return &new_argv[0];
}

/* Return a pointer to the last element of PATH.  */

char *
basename (char *path)
{
  char *tail;

  for (tail = path; *path; ++path)
    if (*path == ':' || *path == '\\')
      tail = path + 1;
  return tail;
}

static char **
grow_argv (char **new_argv, int new_argc)
{
  if (new_argc % ARGS_INCREMENT == 0)
    {
      new_argv = (char **) realloc
	(new_argv, sizeof (char *) * (new_argc + ARGS_INCREMENT));
      if (new_argv == NULL)
	fatal_error ("memory exhausted");
    }
  return new_argv;
}

static void
fatal_error (const char *message)
{
  putc ('\n', stderr);
  if (program_name && *program_name)
    {
      fputs (program_name, stderr);
      fputs (": ", stderr);
    }
  fputs (message, stderr);
  putc ('\n', stderr);
  exit (1);
}

/* Shell-style pattern matching for ?, \, [], and * characters.
   I'm putting this replacement in the public domain.

   Written by Rich $alz, mirror!rs, Wed Nov 26 19:03:17 EST 1986.  */

/* The character that inverts a character class; '!' or '^'.  */
#define INVERT '!'

static int star (char *string, char *pattern);

/* Return nonzero if `string' matches Unix-style wildcard pattern
   `pattern'; zero if not.  */

int
wild_match (char *string, char *pattern)
{
  int prev;		/* Previous character in character class.  */
  int matched;		/* If 1, character class has been matched.  */
  int reverse;		/* If 1, character class is inverted.  */

  for (; *pattern; string++, pattern++)
    switch (*pattern)
      {
      case '\\':
	/* Literal match with following character; fall through.  */
	pattern++;
      default:
	if (*string != *pattern)
	  return 0;
	continue;
      case '?':
	/* Match anything.  */
	if (*string == '\0')
	  return 0;
	continue;
      case '*':
	/* Trailing star matches everything.  */
	return *++pattern ? star (string, pattern) : 1;
      case '[':
	/* Check for inverse character class.  */
	reverse = pattern[1] == INVERT;
	if (reverse)
	  pattern++;
	for (prev = 256, matched = 0; *++pattern && *pattern != ']';
	     prev = *pattern)
	  if (*pattern == '-'
	      ? *string <= *++pattern && *string >= prev
	      : *string == *pattern)
	    matched = 1;
	if (matched == reverse)
	  return 0;
	continue;
      }

  return *string == '\0';
}

static int
star (char *string, char *pattern)
{
  while (wild_match (string, pattern) == 0)
    if (*++string == '\0')
      return 0;
  return 1;
}
