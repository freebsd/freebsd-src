/*-
 * This code is derived from software copyrighted by the Free Software
 * Foundation.
 *
 * Modified 1991 by Donn Seeley at UUNET Technologies, Inc.
 * Modified 1990 by Van Jacobson at Lawrence Berkeley Laboratory.
 */

#ifndef lint
static char sccsid[] = "@(#)source.c	6.3 (Berkeley) 5/8/91";
#endif /* not lint */

/* List lines of source files for GDB, the GNU debugger.
   Copyright (C) 1986, 1987, 1988, 1989 Free Software Foundation, Inc.

This file is part of GDB.

GDB is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 1, or (at your option)
any later version.

GDB is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GDB; see the file COPYING.  If not, write to
the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

#include <stdio.h>
#include <string.h>
#include "defs.h"
#include "param.h"
#include "symtab.h"

#ifdef USG
#include <sys/types.h>
#include <fcntl.h>
#endif

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/file.h>

/* Path of directories to search for source files.
   Same format as the PATH environment variable's value.  */

static char *source_path;

/* Symtab of default file for listing lines of.  */

struct symtab *current_source_symtab;

/* Default next line to list.  */

int current_source_line;

/* Line number of last line printed.  Default for various commands.
   current_source_line is usually, but not always, the same as this.  */

static int last_line_listed;

/* First line number listed by last listing command.  */

static int first_line_listed;


struct symtab *psymtab_to_symtab ();

/* Set the source file default for the "list" command, specifying a
   symtab.  Sigh.  Behaivior specification: If it is called with a
   non-zero argument, that is the symtab to select.  If it is not,
   first lookup "main"; if it exists, use the symtab and line it
   defines.  If not, take the last symtab in the symtab_list (if it
   exists) or the last symtab in the psytab_list (if *it* exists).  If
   none of this works, report an error.   */

void
select_source_symtab (s)
     register struct symtab *s;
{
  struct symtabs_and_lines sals;
  struct symtab_and_line sal;
  struct partial_symtab *ps, *cs_pst;
  
  if (s)
    {
      current_source_symtab = s;
      current_source_line = 1;
      return;
    }

  /* Make the default place to list be the function `main'
     if one exists.  */
  if (lookup_symbol ("main", 0, VAR_NAMESPACE, 0))
    {
      sals = decode_line_spec ("main", 1);
      sal = sals.sals[0];
      free (sals.sals);
      current_source_symtab = sal.symtab;
      current_source_line = max (sal.line - 9, 1);
      return;
    }
  
  /* All right; find the last file in the symtab list (ignoring .h's).  */

  if (s = symtab_list)
    {
      do
	{
	  char *name = s->filename;
	  int len = strlen (name);
	  if (! (len > 2 && !strcmp (&name[len - 2], ".h")))
	    current_source_symtab = s;
	  s = s->next;
	}
      while (s);
      current_source_line = 1;
    }
  else if (partial_symtab_list)
    {
      ps = partial_symtab_list;
      while (ps)
	{
	  char *name = ps->filename;
	  int len = strlen (name);
	  if (! (len > 2 && !strcmp (&name[len - 2], ".h")))
	    cs_pst = ps;
	  ps = ps->next;
	}
      if (cs_pst)
	if (cs_pst->readin)
	  fatal ("Internal: select_source_symtab: readin pst found and no symtabs.");
	else
	  current_source_symtab = psymtab_to_symtab (cs_pst);
      else
	current_source_symtab = 0;
      current_source_line = 1;
    }
}

static void
directories_info ()
{
  printf ("Source directories searched: %s\n", source_path);
}

void
init_source_path ()
{
  register struct symtab *s;

  source_path = savestring (current_directory, strlen (current_directory));

  /* Forget what we learned about line positions in source files;
     must check again now since files may be found in
     a different directory now.  */
  for (s = symtab_list; s; s = s->next)
    if (s->line_charpos != 0)
      {
	free (s->line_charpos);
	s->line_charpos = 0;
      }
}

void
directory_command (dirname, from_tty)
     char *dirname;
     int from_tty;
{
  char *old = source_path;

  dont_repeat ();

  if (dirname == 0)
    {
      if (query ("Reinitialize source path to %s? ", current_directory))
	{
	  init_source_path ();
	  free (old);
	}
    }
  else
    {
      dirname = tilde_expand (dirname);
      make_cleanup (free, dirname);

      do
	{
	  char *name = dirname;
	  register char *p;
	  struct stat st;

	  {
	    char *colon = index (name, ':');
	    char *space = index (name, ' ');
	    char *tab = index (name, '\t');
	    if (colon == 0 && space == 0 && tab ==  0)
	      p = dirname = name + strlen (name);
	    else
	      {
		p = 0;
		if (colon != 0 && (p == 0 || colon < p))
		  p = colon;
		if (space != 0 && (p == 0 || space < p))
		  p = space;
		if (tab != 0 && (p == 0 || tab < p))
		  p = tab;
		dirname = p + 1;
		while (*dirname == ':' || *dirname == ' ' || *dirname == '\t')
		  ++dirname;
	      }
	  }

	  if (p[-1] == '/')
	    /* Sigh. "foo/" => "foo" */
	    --p;
	  *p = '\0';

	  while (p[-1] == '.')
	    {
	      if (p - name == 1)
		{
		  /* "." => getwd ().  */
		  name = current_directory;
		  goto append;
		}
	      else if (p[-2] == '/')
		{
		  if (p - name == 2)
		    {
		      /* "/." => "/".  */
		      *--p = '\0';
		      goto append;
		    }
		  else
		    {
		      /* "...foo/." => "...foo".  */
		      p -= 2;
		      *p = '\0';
		      continue;
		    }
		}
	      else
		break;
	    }

	  if (*name != '/')
	    name = concat (current_directory, "/", name);
	  else
	    name = savestring (name, p - name);
	  make_cleanup (free, name);

	  if (stat (name, &st) < 0)
	    perror_with_name (name);
	  if ((st.st_mode & S_IFMT) != S_IFDIR)
	    error ("%s is not a directory.", name);

	append:
	  {
	    register unsigned int len = strlen (name);

	    p = source_path;
	    while (1)
	      {
		if (!strncmp (p, name, len)
		    && (p[len] == '\0' || p[len] == ':'))
		  {
		    if (from_tty)
		      printf ("\"%s\" is already in the source path.\n", name);
		    break;
		  }
		p = index (p, ':');
		if (p != 0)
		  ++p;
		else
		  break;
	      }
	    if (p == 0)
	      {
		source_path = concat (old, ":", name);
		free (old);
		old = source_path;
	      }
	  }
	} while (*dirname != '\0');
      if (from_tty)
	directories_info ();
    }
}

/* Open a file named STRING, searching path PATH (dir names sep by colons)
   using mode MODE and protection bits PROT in the calls to open.
   If TRY_CWD_FIRST, try to open ./STRING before searching PATH.
   (ie pretend the first element of PATH is ".")
   If FILENAMED_OPENED is non-null, set it to a newly allocated string naming
   the actual file opened (this string will always start with a "/"

   If a file is found, return the descriptor.
   Otherwise, return -1, with errno set for the last name we tried to open.  */

/*  >>>> This should only allow files of certain types,
    >>>>  eg executable, non-directory */
int
openp (path, try_cwd_first, string, mode, prot, filename_opened)
     char *path;
     int try_cwd_first;
     char *string;
     int mode;
     int prot;
     char **filename_opened;
{
  register int fd;
  register char *filename;
  register char *p, *p1;
  register int len;

  if (!path)
    path = ".";

  /* ./foo => foo */
  while (string[0] == '.' && string[1] == '/')
    string += 2;

  if (try_cwd_first || string[0] == '/')
    {
      filename = string;
      fd = open (filename, mode, prot);
      if (fd >= 0 || string[0] == '/')
	goto done;
    }

  filename = (char *) alloca (strlen (path) + strlen (string) + 2);
  fd = -1;
  for (p = path; p; p = p1 ? p1 + 1 : 0)
    {
      p1 = (char *) index (p, ':');
      if (p1)
	len = p1 - p;
      else
	len = strlen (p);

      strncpy (filename, p, len);
      filename[len] = 0;
      strcat (filename, "/");
      strcat (filename, string);

      fd = open (filename, mode, prot);
      if (fd >= 0) break;
    }

 done:
  if (filename_opened)
    if (fd < 0)
      *filename_opened = (char *) 0;
    else if (filename[0] == '/')
      *filename_opened = savestring (filename, strlen (filename));
    else
      {
	*filename_opened = concat (current_directory, "/", filename);
      }

  return fd;
}

/* Create and initialize the table S->line_charpos that records
   the positions of the lines in the source file, which is assumed
   to be open on descriptor DESC.
   All set S->nlines to the number of such lines.  */

static void
find_source_lines (s, desc)
     struct symtab *s;
     int desc;
{
  struct stat st;
  register char *data, *p, *end;
  int nlines = 0;
  int lines_allocated = 1000;
  int *line_charpos = (int *) xmalloc (lines_allocated * sizeof (int));
  extern int exec_mtime;

  if (fstat (desc, &st) < 0)
    perror_with_name (s->filename);
  if (get_exec_file (0) != 0 && exec_mtime < st.st_mtime)
    printf ("Source file is more recent than executable.\n");

  data = (char *) alloca (st.st_size);
  if (myread (desc, data, st.st_size) < 0)
    perror_with_name (s->filename);
  end = data + st.st_size;
  p = data;
  line_charpos[0] = 0;
  nlines = 1;
  while (p != end)
    {
      if (*p++ == '\n'
	  /* A newline at the end does not start a new line.  */
	  && p != end)
	{
	  if (nlines == lines_allocated)
	    {
	      lines_allocated *= 2;
	      line_charpos = (int *) xrealloc (line_charpos,
					       sizeof (int) * lines_allocated);
	    }
	  line_charpos[nlines++] = p - data;
	}
    }
  s->nlines = nlines;
  s->line_charpos = (int *) xrealloc (line_charpos, nlines * sizeof (int));
}

/* Return the character position of a line LINE in symtab S.
   Return 0 if anything is invalid.  */

int
source_line_charpos (s, line)
     struct symtab *s;
     int line;
{
  if (!s) return 0;
  if (!s->line_charpos || line <= 0) return 0;
  if (line > s->nlines)
    line = s->nlines;
  return s->line_charpos[line - 1];
}

/* Return the line number of character position POS in symtab S.  */

int
source_charpos_line (s, chr)
    register struct symtab *s;
    register int chr;
{
  register int line = 0;
  register int *lnp;
    
  if (s == 0 || s->line_charpos == 0) return 0;
  lnp = s->line_charpos;
  /* Files are usually short, so sequential search is Ok */
  while (line < s->nlines  && *lnp <= chr)
    {
      line++;
      lnp++;
    }
  if (line >= s->nlines)
    line = s->nlines;
  return line;
}

/* Get full pathname and line number positions for a symtab.
   Return nonzero if line numbers may have changed.
   Set *FULLNAME to actual name of the file as found by `openp',
   or to 0 if the file is not found.  */

int
get_filename_and_charpos (s, line, fullname)
     struct symtab *s;
     int line;
     char **fullname;
{
  register int desc, linenums_changed = 0;
  
  desc = openp (source_path, 0, s->filename, O_RDONLY, 0, &s->fullname);
  if (desc < 0)
    {
      if (fullname)
	*fullname = NULL;
      return 0;
    }  
  if (fullname)
    *fullname = s->fullname;
  if (s->line_charpos == 0) linenums_changed = 1;
  if (linenums_changed) find_source_lines (s, desc);
  close (desc);
  return linenums_changed;
}

/* Print text describing the full name of the source file S
   and the line number LINE and its corresponding character position.
   The text starts with two Ctrl-z so that the Emacs-GDB interface
   can easily find it.

   MID_STATEMENT is nonzero if the PC is not at the beginning of that line.

   Return 1 if successful, 0 if could not find the file.  */

int
identify_source_line (s, line, mid_statement)
     struct symtab *s;
     int line;
     int mid_statement;
{
  if (s->line_charpos == 0)
    get_filename_and_charpos (s, line, 0);
  if (s->fullname == 0)
    return 0;
  printf ("\032\032%s:%d:%d:%s:0x%x\n", s->fullname,
	  line, s->line_charpos[line - 1],
	  mid_statement ? "middle" : "beg",
	  get_frame_pc (get_current_frame()));
  current_source_line = line;
  first_line_listed = line;
  last_line_listed = line;
  current_source_symtab = s;
  return 1;
}

/* Print source lines from the file of symtab S,
   starting with line number LINE and stopping before line number STOPLINE.  */

void
print_source_lines (s, line, stopline, noerror)
     struct symtab *s;
     int line, stopline;
     int noerror;
{
  register int c;
  register int desc;
  register FILE *stream;
  int nlines = stopline - line;

  desc = openp (source_path, 0, s->filename, O_RDONLY, 0, &s->fullname);
  if (desc < 0)
    {
      extern int errno;
      if (noerror && line + 1 == stopline)
	{
	  /* can't find the file - tell user where we are anyway */
	  current_source_symtab = s;
	  current_source_line = line;
	  first_line_listed = line;
	  last_line_listed = line;
	  printf_filtered ("%d\t(%s)\n", current_source_line++, s->filename);
	}
      else
	{
	  if (! noerror)
	    perror_with_name (s->filename);
	  print_sys_errmsg (s->filename, errno);
	}
      return;
    }

  if (s->line_charpos == 0)
    find_source_lines (s, desc);

  if (line < 1 || line > s->nlines)
    {
      close (desc);
      error ("Line number %d out of range; %s has %d lines.",
	     line, s->filename, s->nlines);
    }

  if (lseek (desc, s->line_charpos[line - 1], 0) < 0)
    {
      close (desc);
      perror_with_name (s->filename);
    }

  current_source_symtab = s;
  current_source_line = line;
  first_line_listed = line;
  
  stream = fdopen (desc, "r");
  clearerr (stream);

  while (nlines-- > 0)
    {
      c = fgetc (stream);
      if (c == EOF) break;
      last_line_listed = current_source_line;
      printf_filtered ("%d\t", current_source_line++);
      do
	{
	  if (c < 040 && c != '\t' && c != '\n')
	      printf_filtered ("^%c", c + 0100);
	  else if (c == 0177)
	    printf_filtered ("^?");
	  else
	    printf_filtered ("%c", c);
	} while (c != '\n' && (c = fgetc (stream)) >= 0);
    }

  fclose (stream);
}



/* 
  C++
  Print a list of files and line numbers which a user may choose from
  in order to list a function which was specified ambiguously
  (as with `list classname::overloadedfuncname', for example).
  The vector in SALS provides the filenames and line numbers.
  */
static void
ambiguous_line_spec (sals)
     struct symtabs_and_lines *sals;
{
  int i;

  for (i = 0; i < sals->nelts; ++i)
    printf("file: \"%s\", line number: %d\n",
	   sals->sals[i].symtab->filename, sals->sals[i].line);
}


static void
file_command(arg, from_tty)
  char *arg;
  int from_tty;
{
  struct symtabs_and_lines sals;
  struct symtab_and_line sal;
  struct symbol *sym;
  char *arg1;
  int linenum_beg = 0;
  char *p;

  if (symtab_list == 0 && partial_symtab_list == 0)
    error ("No symbol table is loaded.  Use the \"symbol-file\" command.");

  /* Pull in a current source symtab if necessary */
  if (arg == 0 || arg[0] == 0) {
	  if (current_source_symtab == 0)
		  select_source_symtab(0);
    	  else
		  printf("%s\n", current_source_symtab->filename);
	  return;
  }
  arg1 = arg;
  sals = decode_line_1 (&arg1, 0, 0, 0);

  if (! sals.nelts)
    return;  /*  C++  */

  if (sals.nelts > 1)
    {
      ambiguous_line_spec (&sals);
      free (sals.sals);
      return;
    }

  sal = sals.sals[0];
  free (sals.sals);

  /* Record whether the BEG arg is all digits.  */

  for (p = arg; p != arg1 && *p >= '0' && *p <= '9'; ++p)
    ;
  linenum_beg = (p == arg1);

  /* if line was specified by address,
     print exactly which line, and which file.
     In this case, sal.symtab == 0 means address is outside
     of all known source files, not that user failed to give a filename.  */
  if (*arg == '*')
    {
      if (sal.symtab == 0)
	error ("No source file for address 0x%x.", sal.pc);
      sym = find_pc_function (sal.pc);
      if (sym)
	printf ("0x%x is in %s (%s, line %d).\n",
		sal.pc, SYMBOL_NAME (sym), sal.symtab->filename, sal.line);
      else
	printf ("0x%x is in %s, line %d.\n",
		sal.pc, sal.symtab->filename, sal.line);
    }

  /* If line was not specified by just a line number,
     and it does not imply a symtab, it must be an undebuggable symbol
     which means no source code.  */

  if (sal.symtab == 0)
    {
      if (! linenum_beg)
	error ("No line number known for %s.", arg);
      else
	error ("No default source file yet.  Do \"help list\".");
    }
  else
    {
      current_source_symtab = sal.symtab;
      current_source_line = sal.line;
      first_line_listed = sal.line;
    }
}

#define PUSH_STACK_SIZE 32
static struct {
	struct symtab *symtab;
	int line;
} push_stack[PUSH_STACK_SIZE];

static unsigned int push_stack_ptr;

static void
push_to_file_command (arg, from_tty)
	char *arg;
	int from_tty;
{
	struct symtab *cursym = current_source_symtab;
	int curline = current_source_line;
	register unsigned int i;

	file_command(arg, from_tty);

	/* if we got back, command was successful */
	i = push_stack_ptr;
	push_stack[i].symtab = cursym;
	push_stack[i].line = curline;
	push_stack_ptr = (i + 1) & (PUSH_STACK_SIZE - 1);
}

static void
pop_file_command (arg, from_tty)
	char *arg;
	int from_tty;
{
	register unsigned int i = push_stack_ptr;

	/* if there's something on the stack, pop it & clear the slot. */
	i = (i + (PUSH_STACK_SIZE - 1)) & (PUSH_STACK_SIZE - 1);
	if (push_stack[i].symtab) {
		current_source_symtab = push_stack[i].symtab;
		first_line_listed = current_source_line = push_stack[i].line;
		push_stack[i].symtab = NULL;
		push_stack_ptr = i;
	}
}


static void
list_command (arg, from_tty)
     char *arg;
     int from_tty;
{
  struct symtabs_and_lines sals, sals_end;
  struct symtab_and_line sal, sal_end;
  struct symbol *sym;
  char *arg1;
  int no_end = 1;
  int dummy_end = 0;
  int dummy_beg = 0;
  int linenum_beg = 0;
  char *p;

  if (symtab_list == 0 && partial_symtab_list == 0)
    error ("No symbol table is loaded.  Use the \"symbol-file\" command.");

  /* Pull in a current source symtab if necessary */
  if (current_source_symtab == 0 &&
      (arg == 0 || arg[0] == '+' || arg[0] == '-'))
    select_source_symtab (0);

  /* "l" or "l +" lists next ten lines.  */

  if (arg == 0 || !strcmp (arg, "+"))
    {
      if (current_source_symtab == 0)
	error ("No default source file yet.  Do \"help list\".");
      print_source_lines (current_source_symtab, current_source_line,
			  current_source_line + 10, 0);
      return;
    }

  /* "l -" lists previous ten lines, the ones before the ten just listed.  */
  if (!strcmp (arg, "-"))
    {
      if (current_source_symtab == 0)
	error ("No default source file yet.  Do \"help list\".");
      print_source_lines (current_source_symtab,
			  max (first_line_listed - 10, 1),
			  first_line_listed, 0);
      return;
    }

  /* Now if there is only one argument, decode it in SAL
     and set NO_END.
     If there are two arguments, decode them in SAL and SAL_END
     and clear NO_END; however, if one of the arguments is blank,
     set DUMMY_BEG or DUMMY_END to record that fact.  */

  arg1 = arg;
  if (*arg1 == ',')
    dummy_beg = 1;
  else
    {
      sals = decode_line_1 (&arg1, 0, 0, 0);

      if (! sals.nelts) return;  /*  C++  */
      if (sals.nelts > 1)
	{
	  ambiguous_line_spec (&sals);
	  free (sals.sals);
	  return;
	}

      sal = sals.sals[0];
      free (sals.sals);
    }

  /* Record whether the BEG arg is all digits.  */

  for (p = arg; p != arg1 && *p >= '0' && *p <= '9'; p++);
  linenum_beg = (p == arg1);

  while (*arg1 == ' ' || *arg1 == '\t')
    arg1++;
  if (*arg1 == ',')
    {
      no_end = 0;
      arg1++;
      while (*arg1 == ' ' || *arg1 == '\t')
	arg1++;
      if (*arg1 == 0)
	dummy_end = 1;
      else
	{
	  if (dummy_beg)
	    sals_end = decode_line_1 (&arg1, 0, 0, 0);
	  else
	    sals_end = decode_line_1 (&arg1, 0, sal.symtab, sal.line);
	  if (sals_end.nelts == 0) 
	    return;
	  if (sals_end.nelts > 1)
	    {
	      ambiguous_line_spec (&sals_end);
	      free (sals_end.sals);
	      return;
	    }
	  sal_end = sals_end.sals[0];
	  free (sals_end.sals);
	}
    }

  if (*arg1)
    error ("Junk at end of line specification.");

  if (!no_end && !dummy_beg && !dummy_end
      && sal.symtab != sal_end.symtab)
    error ("Specified start and end are in different files.");
  if (dummy_beg && dummy_end)
    error ("Two empty args do not say what lines to list.");
 
  /* if line was specified by address,
     first print exactly which line, and which file.
     In this case, sal.symtab == 0 means address is outside
     of all known source files, not that user failed to give a filename.  */
  if (*arg == '*')
    {
      if (sal.symtab == 0)
	error ("No source file for address 0x%x.", sal.pc);
      sym = find_pc_function (sal.pc);
      if (sym)
	printf ("0x%x is in %s (%s, line %d).\n",
		sal.pc, SYMBOL_NAME (sym), sal.symtab->filename, sal.line);
      else
	printf ("0x%x is in %s, line %d.\n",
		sal.pc, sal.symtab->filename, sal.line);
    }

  /* If line was not specified by just a line number,
     and it does not imply a symtab, it must be an undebuggable symbol
     which means no source code.  */

  if (! linenum_beg && sal.symtab == 0)
    error ("No line number known for %s.", arg);

  /* If this command is repeated with RET,
     turn it into the no-arg variant.  */

  if (from_tty)
    *arg = 0;

  if (dummy_beg && sal_end.symtab == 0)
    error ("No default source file yet.  Do \"help list\".");
  if (dummy_beg)
    print_source_lines (sal_end.symtab, max (sal_end.line - 9, 1),
			sal_end.line + 1, 0);
  else if (sal.symtab == 0)
    error ("No default source file yet.  Do \"help list\".");
  else if (no_end)
    print_source_lines (sal.symtab, max (sal.line - 5, 1), sal.line + 5, 0);
  else
    print_source_lines (sal.symtab, sal.line,
			dummy_end ? sal.line + 10 : sal_end.line + 1,
			0);
}

/* Print info on range of pc's in a specified line.  */

static void
line_info (arg, from_tty)
     char *arg;
     int from_tty;
{
  struct symtabs_and_lines sals;
  struct symtab_and_line sal;
  int start_pc, end_pc;
  int i;

  if (arg == 0)
    {
      sal.symtab = current_source_symtab;
      sal.line = last_line_listed;
      sals.nelts = 1;
      sals.sals = (struct symtab_and_line *)
	xmalloc (sizeof (struct symtab_and_line));
      sals.sals[0] = sal;
    }
  else
    {
      sals = decode_line_spec_1 (arg, 0);
      
      /* If this command is repeated with RET,
	 turn it into the no-arg variant.  */
      if (from_tty)
	*arg = 0;
    }

  /* C++  More than one line may have been specified, as when the user
     specifies an overloaded function name. Print info on them all. */
  for (i = 0; i < sals.nelts; i++)
    {
      sal = sals.sals[i];
      
      if (sal.symtab == 0)
	error ("No source file specified.");

      if (sal.line > 0
	  && find_line_pc_range (sal.symtab, sal.line, &start_pc, &end_pc))
	{
	  if (start_pc == end_pc)
	    printf ("Line %d of \"%s\" is at pc 0x%x but contains no code.\n",
		    sal.line, sal.symtab->filename, start_pc);
	  else
	    printf ("Line %d of \"%s\" starts at pc 0x%x and ends at 0x%x.\n",
		    sal.line, sal.symtab->filename, start_pc, end_pc);
	  /* x/i should display this line's code.  */
	  set_next_address (start_pc);
	  /* Repeating "info line" should do the following line.  */
	  last_line_listed = sal.line + 1;
	}
      else
	printf ("Line number %d is out of range for \"%s\".\n",
		sal.line, sal.symtab->filename);
    }
}

/* Commands to search the source file for a regexp.  */

static void
forward_search_command (regex, from_tty)
     char *regex;
{
  register int c;
  register int desc;
  register FILE *stream;
  int line = last_line_listed + 1;
  char *msg;

  msg = (char *) re_comp (regex);
  if (msg)
    error (msg);

  if (current_source_symtab == 0)
    select_source_symtab (0);

  /* Search from last_line_listed+1 in current_source_symtab */

  desc = openp (source_path, 0, current_source_symtab->filename,
		O_RDONLY, 0, &current_source_symtab->fullname);
  if (desc < 0)
    perror_with_name (current_source_symtab->filename);

  if (current_source_symtab->line_charpos == 0)
    find_source_lines (current_source_symtab, desc);

  if (line < 1 || line > current_source_symtab->nlines)
    {
      close (desc);
      error ("Expression not found");
    }

  if (lseek (desc, current_source_symtab->line_charpos[line - 1], 0) < 0)
    {
      close (desc);
      perror_with_name (current_source_symtab->filename);
    }

  stream = fdopen (desc, "r");
  clearerr (stream);
  while (1) {
    char buf[4096];		/* Should be reasonable??? */
    register char *p = buf;

    c = fgetc (stream);
    if (c == EOF)
      break;
    do {
      *p++ = c;
    } while (c != '\n' && (c = fgetc (stream)) >= 0);

    /* we now have a source line in buf, null terminate and match */
    *p = 0;
    if (re_exec (buf) > 0)
      {
	/* Match! */
	fclose (stream);
	print_source_lines (current_source_symtab,
			   line, line+1, 0);
	current_source_line = max (line - 5, 1);
	return;
      }
    line++;
  }

  printf ("Expression not found\n");
  fclose (stream);
}

static void
reverse_search_command (regex, from_tty)
     char *regex;
{
  register int c;
  register int desc;
  register FILE *stream;
  int line = last_line_listed - 1;
  char *msg;

  msg = (char *) re_comp (regex);
  if (msg)
    error (msg);

  if (current_source_symtab == 0)
    select_source_symtab (0);

  /* Search from last_line_listed-1 in current_source_symtab */

  desc = openp (source_path, 0, current_source_symtab->filename,
		O_RDONLY, 0, &current_source_symtab->fullname);
  if (desc < 0)
    perror_with_name (current_source_symtab->filename);

  if (current_source_symtab->line_charpos == 0)
    find_source_lines (current_source_symtab, desc);

  if (line < 1 || line > current_source_symtab->nlines)
    {
      close (desc);
      error ("Expression not found");
    }

  if (lseek (desc, current_source_symtab->line_charpos[line - 1], 0) < 0)
    {
      close (desc);
      perror_with_name (current_source_symtab->filename);
    }

  stream = fdopen (desc, "r");
  clearerr (stream);
  while (1)
    {
      char buf[4096];		/* Should be reasonable??? */
      register char *p = buf;

      c = fgetc (stream);
      if (c == EOF)
	break;
      do {
	*p++ = c;
      } while (c != '\n' && (c = fgetc (stream)) >= 0);

      /* We now have a source line in buf; null terminate and match.  */
      *p = 0;
      if (re_exec (buf) > 0)
	{
	  /* Match! */
	  fclose (stream);
	  print_source_lines (current_source_symtab,
			      line, line+1, 0);
	  current_source_line = max (line - 5, 1);
	  return;
	}
      line--;
      if (fseek (stream, current_source_symtab->line_charpos[line - 1], 0) < 0)
	{
	  fclose (stream);
	  perror_with_name (current_source_symtab->filename);
	}
    }

  printf ("Expression not found\n");
  fclose (stream);
  return;
}

void
_initialize_source ()
{
  current_source_symtab = 0;
  init_source_path ();

  add_com ("directory", class_files, directory_command,
	   "Add directory DIR to end of search path for source files.\n\
With no argument, reset the search path to just the working directory\n\
and forget cached info on line positions in source files.");

  add_info ("directories", directories_info,
	    "Current search path for finding source files.");

  add_info ("line", line_info,
	    "Core addresses of the code for a source line.\n\
Line can be specified as\n\
  LINENUM, to list around that line in current file,\n\
  FILE:LINENUM, to list around that line in that file,\n\
  FUNCTION, to list around beginning of that function,\n\
  FILE:FUNCTION, to distinguish among like-named static functions.\n\
Default is to describe the last source line that was listed.\n\n\
This sets the default address for \"x\" to the line's first instruction\n\
so that \"x/i\" suffices to start examining the machine code.\n\
The address is also stored as the value of \"$_\".");

  add_com ("forward-search", class_files, forward_search_command,
	   "Search for regular expression (see regex(3)) from last line listed.");
  add_com_alias ("search", "forward-search", class_files, 0);

  add_com ("reverse-search", class_files, reverse_search_command,
	   "Search backward for regular expression (see regex(3)) from last line listed.");

  add_com ("list", class_files, list_command,
	   "List specified function or line.\n\
With no argument, lists ten more lines after or around previous listing.\n\
\"list -\" lists the ten lines before a previous ten-line listing.\n\
One argument specifies a line, and ten lines are listed around that line.\n\
Two arguments with comma between specify starting and ending lines to list.\n\
Lines can be specified in these ways:\n\
  LINENUM, to list around that line in current file,\n\
  FILE:LINENUM, to list around that line in that file,\n\
  FUNCTION, to list around beginning of that function,\n\
  FILE:FUNCTION, to distinguish among like-named static functions.\n\
  *ADDRESS, to list around the line containing that address.\n\
With two args if one is empty it stands for ten lines away from the other arg.");
  add_com ("file", class_files, file_command,
	   "Select current file, function and line for display or list.\n\
Specification can have the form:\n\
  LINENUM, to select that line in current file,\n\
  FILE:LINENUM, to select that line in that file,\n\
  FUNCTION, to select beginning of that function,\n\
  FILE:FUNCTION, to distinguish among like-named static functions.\n\
  *ADDRESS, to select the line containing that address.");
  add_com ("push-to-file", class_files, push_to_file_command,
	   "Like \"file\" command but remembers current file & line on a stack.\n\
Can later return to current file with \"pop-file\" command.\n\
Up to 32 file positions can be pushed on stack.");
  add_com ("pop-file", class_files, pop_file_command,
	   "Pops back to file position saved by most recent \"push-to-file\".\n\
If everything has been popped from stack, command does nothing.");
}

