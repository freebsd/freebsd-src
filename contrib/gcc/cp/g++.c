/* G++ preliminary semantic processing for the compiler driver.
   Copyright (C) 1993, 1994, 1995 Free Software Foundation, Inc.
   Contributed by Brendan Kehoe (brendan@cygnus.com).

This file is part of GNU CC.

GNU CC is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GNU CC is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU CC; see the file COPYING.  If not, write to
the Free Software Foundation, 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.  */

/* This program is a wrapper to the main `gcc' driver.  For GNU C++,
   we need to do two special things: a) append `-lg++' in situations
   where it's appropriate, to link in libg++, and b) add `-xc++'..`-xnone'
   around file arguments named `foo.c' or `foo.i'.  So, we do all of
   this semantic processing then just exec gcc with the new argument
   list.

   We used to do all of this in a small shell script, but many users
   found the performance of this as a shell script to be unacceptable.
   In situations where your PATH has a lot of NFS-mounted directories,
   using a script that runs sed and other things would be a nasty
   performance hit.  With this program, we never search the PATH at all.  */

#include "config.h"
#ifdef __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif
#include <stdio.h>
#include <sys/types.h>
#if !defined(_WIN32)
#include <sys/file.h>   /* May get R_OK, etc. on some systems.  */
#else
#include <process.h>
#endif
#include <errno.h>

/* Defined to the name of the compiler; if using a cross compiler, the
   Makefile should compile this file with the proper name
   (e.g., "i386-aout-gcc").  */
#ifndef GCC_NAME
#define GCC_NAME "gcc"
#endif

/* This bit is set if we saw a `-xfoo' language specification.  */
#define LANGSPEC	(1<<1)
/* This bit is set if they did `-lm' or `-lmath'.  */
#define MATHLIB		(1<<2)

#ifndef MATH_LIBRARY
#define MATH_LIBRARY "-lm"
#endif

/* On MSDOS, write temp files in current dir
   because there's no place else we can expect to use.  */
#ifdef __MSDOS__
#ifndef P_tmpdir
#define P_tmpdir "."
#endif
#ifndef R_OK
#define R_OK 4
#define W_OK 2
#define X_OK 1
#endif
#endif

#ifndef VPROTO
#ifdef __STDC__
#define PVPROTO(ARGS)		ARGS
#define VPROTO(ARGS)		ARGS
#define VA_START(va_list,var)	va_start(va_list,var)
#else
#define PVPROTO(ARGS)		()
#define VPROTO(ARGS)		(va_alist) va_dcl
#define VA_START(va_list,var)	va_start(va_list)
#endif
#endif

#ifndef errno
extern int errno;
#endif

extern int sys_nerr;
#ifndef HAVE_STRERROR
#if defined(bsd4_4)
extern const char *const sys_errlist[];
#else
extern char *sys_errlist[];
#endif
#else
extern char *strerror();
#endif

/* Name with which this program was invoked.  */
static char *programname;

char *
my_strerror(e)
     int e;
{

#ifdef HAVE_STRERROR
  return strerror(e);

#else

  static char buffer[30];
  if (!e)
    return "";

  if (e > 0 && e < sys_nerr)
    return sys_errlist[e];

  sprintf (buffer, "Unknown error %d", e);
  return buffer;
#endif
}

#ifdef HAVE_VPRINTF
/* Output an error message and exit */

static void
fatal VPROTO((char *format, ...))
{
#ifndef __STDC__
  char *format;
#endif
  va_list ap;

  VA_START (ap, format);

#ifndef __STDC__
  format = va_arg (ap, char*);
#endif

  fprintf (stderr, "%s: ", programname);
  vfprintf (stderr, format, ap);
  va_end (ap);
  fprintf (stderr, "\n");
#if 0
  /* XXX Not needed for g++ driver.  */
  delete_temp_files ();
#endif
  exit (1);
}

static void
error VPROTO((char *format, ...))
{
#ifndef __STDC__
  char *format;
#endif
  va_list ap;

  VA_START (ap, format);

#ifndef __STDC__
  format = va_arg (ap, char*);
#endif

  fprintf (stderr, "%s: ", programname);
  vfprintf (stderr, format, ap);
  va_end (ap);

  fprintf (stderr, "\n");
}

#else /* not HAVE_VPRINTF */

static void
error (msg, arg1, arg2)
     char *msg, *arg1, *arg2;
{
  fprintf (stderr, "%s: ", programname);
  fprintf (stderr, msg, arg1, arg2);
  fprintf (stderr, "\n");
}

static void
fatal (msg, arg1, arg2)
     char *msg, *arg1, *arg2;
{
  error (msg, arg1, arg2);
#if 0
  /* XXX Not needed for g++ driver.  */
  delete_temp_files ();
#endif
  exit (1);
}

#endif /* not HAVE_VPRINTF */

/* More 'friendly' abort that prints the line and file.
   config.h can #define abort fancy_abort if you like that sort of thing.  */

void
fancy_abort ()
{
  fatal ("Internal g++ abort.");
}

char *
xmalloc (size)
     unsigned size;
{
  register char *value = (char *) malloc (size);
  if (value == 0)
    fatal ("virtual memory exhausted");
  return value;
}

/* Return a newly-allocated string whose contents concatenate those
   of s1, s2, s3.  */
static char *
concat (s1, s2, s3)
     char *s1, *s2, *s3;
{
  int len1 = strlen (s1), len2 = strlen (s2), len3 = strlen (s3);
  char *result = xmalloc (len1 + len2 + len3 + 1);

  strcpy (result, s1);
  strcpy (result + len1, s2);
  strcpy (result + len1 + len2, s3);
  *(result + len1 + len2 + len3) = 0;

  return result;
}

static void
pfatal_with_name (name)
     char *name;
{
  fatal (concat ("%s: ", my_strerror (errno), ""), name);
}

#ifdef __MSDOS__
/* This is the common prefix we use to make temp file names.  */
char *temp_filename;

/* Length of the prefix.  */
int temp_filename_length;

/* Compute a string to use as the base of all temporary file names.  */
static char *
choose_temp_base_try (try, base)
char *try;
char *base;
{
  char *rv;
  if (base)
    rv = base;
  else if (try == (char *)0)
    rv = 0;
  else if (access (try, R_OK | W_OK) != 0)
    rv = 0;
  else
    rv = try;
  return rv;
}

static void
choose_temp_base ()
{
  char *base = 0;
  int len;

  base = choose_temp_base_try (getenv ("TMPDIR"), base);
  base = choose_temp_base_try (getenv ("TMP"), base);
  base = choose_temp_base_try (getenv ("TEMP"), base);

#ifdef P_tmpdir
  base = choose_temp_base_try (P_tmpdir, base);
#endif

  base = choose_temp_base_try ("/usr/tmp", base);
  base = choose_temp_base_try ("/tmp", base);

  /* If all else fails, use the current directory! */  
  if (base == (char *)0)
    base = "./";

  len = strlen (base);
  temp_filename = xmalloc (len + sizeof("/ccXXXXXX"));
  strcpy (temp_filename, base);
  if (len > 0 && temp_filename[len-1] != '/')
    temp_filename[len++] = '/';
  strcpy (temp_filename + len, "ccXXXXXX");

  mktemp (temp_filename);
  temp_filename_length = strlen (temp_filename);
  if (temp_filename_length == 0)
    abort ();
}

static void
perror_exec (name)
     char *name;
{
  char *s;

  if (errno < sys_nerr)
    s = concat ("installation problem, cannot exec %s: ",
		my_strerror( errno ), "");
  else
    s = "installation problem, cannot exec %s";
  error (s, name);
}

/* This is almost exactly what's in gcc.c:pexecute for MSDOS.  */
void
run_dos (program, argv)
     char *program;
     char *argv[];
{
  char *scmd, *rf;
  FILE *argfile;
  int i;

  choose_temp_base (); /* not in gcc.c */

  scmd = (char *) malloc (strlen (program) + strlen (temp_filename) + 10);
  rf = scmd + strlen (program) + 6;
  sprintf (scmd, "%s.exe @%s.gp", program, temp_filename);

  argfile = fopen (rf, "w");
  if (argfile == 0)
    pfatal_with_name (rf);

  for (i=1; argv[i]; i++)
    {
      char *cp;
      for (cp = argv[i]; *cp; cp++)
	{
	  if (*cp == '"' || *cp == '\'' || *cp == '\\' || isspace (*cp))
	    fputc ('\\', argfile);
	  fputc (*cp, argfile);
	}
      fputc ('\n', argfile);
    }
  fclose (argfile);

  i = system (scmd);

  remove (rf);
  
  if (i == -1)
    perror_exec (program);
}
#endif /* __MSDOS__ */

int
main (argc, argv)
     int argc;
     char **argv;
{
  register int i, j = 0;
  register char *p;
  int verbose = 0;

  /* This will be 0 if we encounter a situation where we should not
     link in libstdc++, or 2 if we should link in libg++ as well.  */
  int library = 1;

  /* Used to track options that take arguments, so we don't go wrapping
     those with -xc++/-xnone.  */
  char *quote = NULL;

  /* The new argument list will be contained in this.  */
  char **arglist;

  /* The name of the compiler we will want to run---by default, it
     will be the definition of `GCC_NAME', e.g., `gcc'.  */
  char *gcc = GCC_NAME;

  /* Non-zero if we saw a `-xfoo' language specification on the
     command line.  Used to avoid adding our own -xc++ if the user
     already gave a language for the file.  */
  int saw_speclang = 0;

  /* Non-zero if we saw `-lm' or `-lmath' on the command line.  */
  char *saw_math = 0;

  /* The number of arguments being added to what's in argv, other than
     libraries.  We use this to track the number of times we've inserted
     -xc++/-xnone.  */
  int added = 0;

  /* An array used to flag each argument that needs a bit set for
     LANGSPEC or MATHLIB.  */
  int *args;

  p = argv[0] + strlen (argv[0]);

  /* If we're called as g++ (or i386-aout-g++), link in libg++ as well.  */

  if (strcmp (p - 3, "g++") == 0)
    {
      library = 2;
    }

  while (p != argv[0] && p[-1] != '/')
    --p;
  programname = p;

  if (argc == 1)
    fatal ("No input files specified.\n");

#ifndef __MSDOS__
  /* We do a little magic to find out where the main gcc executable
     is.  If they ran us as /usr/local/bin/g++, then we will look
     for /usr/local/bin/gcc; similarly, if they just ran us as `g++',
     we'll just look for `gcc'.  */
  if (p != argv[0])
    {
      *--p = '\0';
      gcc = (char *) malloc ((strlen (argv[0]) + 1 + strlen (GCC_NAME) + 1)
			     * sizeof (char));
      sprintf (gcc, "%s/%s", argv[0], GCC_NAME);
    }
#endif

  args = (int *) malloc (argc * sizeof (int));
  bzero ((char *) args, argc * sizeof (int));

  for (i = 1; i < argc; i++)
    {
      /* If the previous option took an argument, we swallow it here.  */
      if (quote)
	{
	  quote = NULL;
	  continue;
	}

      if (argv[i][0] == '\0' || argv[i][1] == '\0')
	continue;

      if (argv[i][0] == '-')
	{
	  if (library != 0 && strcmp (argv[i], "-nostdlib") == 0)
	    {
	      library = 0;
	    }
	  else if (strcmp (argv[i], "-lm") == 0
		   || strcmp (argv[i], "-lmath") == 0)
	    args[i] |= MATHLIB;
	  else if (strcmp (argv[i], "-v") == 0)
	    {
	      verbose = 1;
	      if (argc == 2)
		{
		  /* If they only gave us `-v', don't try to link
		     in libg++.  */ 
		  library = 0;
		}
	    }
	  else if (strncmp (argv[i], "-x", 2) == 0)
	    saw_speclang = 1;
	  else if (((argv[i][2] == '\0'
		     && (char *)strchr ("bBVDUoeTuIYmLiA", argv[i][1]) != NULL)
		    || strcmp (argv[i], "-Tdata") == 0))
	    quote = argv[i];
	  else if (library != 0 && ((argv[i][2] == '\0'
		     && (char *) strchr ("cSEM", argv[i][1]) != NULL)
		    || strcmp (argv[i], "-MM") == 0))
	    {
	      /* Don't specify libraries if we won't link, since that would
		 cause a warning.  */
	      library = 0;
	    }
	  else
	    /* Pass other options through.  */
	    continue;
	}
      else
	{
	  int len; 

	  if (saw_speclang)
	    {
	      saw_speclang = 0;
	      continue;
	    }

	  /* If the filename ends in .c or .i, put options around it.
	     But not if a specified -x option is currently active.  */
	  len = strlen (argv[i]);
	  if (len > 2
	      && (argv[i][len - 1] == 'c' || argv[i][len - 1] == 'i')
	      && argv[i][len - 2] == '.')
	    {
	      args[i] |= LANGSPEC;
	      added += 2;
	    }
	}
    }

  if (quote)
    fatal ("argument to `%s' missing\n", quote);

  if (added || library)
    {
      arglist = (char **) malloc ((argc + added + 4) * sizeof (char *));

      for (i = 1, j = 1; i < argc; i++, j++)
	{
	  arglist[j] = argv[i];

	  /* Make sure -lg++ is before the math library, since libg++
	     itself uses those math routines.  */
	  if (!saw_math && (args[i] & MATHLIB) && library)
	    {
	      --j;
	      saw_math = argv[i];
	    }

	  /* Wrap foo.c and foo.i files in a language specification to
	     force the gcc compiler driver to run cc1plus on them.  */
	  if (args[i] & LANGSPEC)
	    {
	      int len = strlen (argv[i]);
	      if (argv[i][len - 1] == 'i')
		arglist[j++] = "-xc++-cpp-output";
	      else
		arglist[j++] = "-xc++";
	      arglist[j++] = argv[i];
	      arglist[j] = "-xnone";
	    }
	}

      /* Add `-lg++' if we haven't already done so.  */
      if (library == 2)
	arglist[j++] = "-lg++";
      if (library)
	arglist[j++] = "-lstdc++";
      if (saw_math)
	arglist[j++] = saw_math;
      else if (library)
	arglist[j++] = MATH_LIBRARY;

      arglist[j] = NULL;
    }
  else
    /* No need to copy 'em all.  */
    arglist = argv;

  arglist[0] = gcc;

  if (verbose)
    {
      if (j == 0)
	j = argc;

      for (i = 0; i < j; i++)
	fprintf (stderr, " %s", arglist[i]);
      fprintf (stderr, "\n");
    }
#if !defined(OS2) && !defined (_WIN32)
#ifdef __MSDOS__
  run_dos (gcc, arglist);
#else /* !__MSDOS__ */
  if (execvp (gcc, arglist) < 0)
    pfatal_with_name (gcc);
#endif /* __MSDOS__ */
#else /* OS2 or _WIN32 */
  if (spawnvp (1, gcc, arglist) < 0)
    pfatal_with_name (gcc);
#endif

  return 0;
}
