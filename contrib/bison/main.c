/* Top level entry point of bison,
   Copyright (C) 1984, 1986, 1989, 1992, 1995 Free Software Foundation, Inc.

This file is part of Bison, the GNU Compiler Compiler.

Bison is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

Bison is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Bison; see the file COPYING.  If not, write to
the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.  */


#include <stdio.h>
#include "system.h"
#include "machine.h"	/* for MAXSHORT */

extern	int lineno;
extern	int verboseflag;
extern	char *infile;

/* Nonzero means failure has been detected; don't write a parser file.  */
int failure;

/* The name this program was run with, for messages.  */
char *program_name;

char *printable_version PARAMS((int));
char *int_to_string PARAMS((int));
void fatal PARAMS((char *));
void fatals PARAMS((char *, char *));
void warn PARAMS((char *));
void warni PARAMS((char *, int));
void warns PARAMS((char *, char *));
void warnss PARAMS((char *, char *, char *));
void warnsss PARAMS((char *, char *, char *, char *));
void toomany PARAMS((char *));
void berror PARAMS((char *));

extern void getargs PARAMS((int, char *[]));
extern void openfiles PARAMS((void));
extern void reader PARAMS((void));
extern void reduce_grammar PARAMS((void));
extern void set_derives PARAMS((void));
extern void set_nullable PARAMS((void));
extern void generate_states PARAMS((void));
extern void lalr PARAMS((void));
extern void initialize_conflicts PARAMS((void));
extern void verbose PARAMS((void));
extern void terse PARAMS((void));
extern void output PARAMS((void));
extern void done PARAMS((int));


/* VMS complained about using `int'.  */

int
main (int argc, char *argv[])
{
  program_name = argv[0];
  setlocale (LC_ALL, "");
  bindtextdomain (PACKAGE, LOCALEDIR);
  textdomain (PACKAGE);

  failure = 0;
  lineno = 0;
  getargs(argc, argv);
  openfiles();

  /* read the input.  Copy some parts of it to fguard, faction, ftable and fattrs.
     In file reader.c.
     The other parts are recorded in the grammar; see gram.h.  */
  reader();
  if (failure)
	done(failure);

  /* find useless nonterminals and productions and reduce the grammar.  In
     file reduce.c */
  reduce_grammar();

  /* record other info about the grammar.  In files derives and nullable.  */
  set_derives();
  set_nullable();

  /* convert to nondeterministic finite state machine.  In file LR0.
     See state.h for more info.  */
  generate_states();

  /* make it deterministic.  In file lalr.  */
  lalr();

  /* Find and record any conflicts: places where one token of lookahead is not
     enough to disambiguate the parsing.  In file conflicts.
     Also resolve s/r conflicts based on precedence declarations.  */
  initialize_conflicts();

  /* print information about results, if requested.  In file print. */
  if (verboseflag)
    verbose();
  else
    terse();

  /* output the tables and the parser to ftable.  In file output. */
  output();
  done(failure);
  return failure;
}

/* functions to report errors which prevent a parser from being generated */


/* Return a string containing a printable version of C:
   either C itself, or the corresponding \DDD code.  */

char *
printable_version (int c)
{
  static char buf[10];
  if (c < ' ' || c >= '\177')
    sprintf(buf, "\\%o", c);
  else
    {
      buf[0] = c;
      buf[1] = '\0';
    }
  return buf;
}

/* Generate a string from the integer I.
   Return a ptr to internal memory containing the string.  */

char *
int_to_string (int i)
{
  static char buf[20];
  sprintf(buf, "%d", i);
  return buf;
}

static void
fatal_banner (void)
{
  if (infile == 0)
    fprintf(stderr, _("%s: fatal error: "), program_name);
  else
    fprintf(stderr, _("%s:%d: fatal error: "), infile, lineno);
}

/* Print the message S for a fatal error.  */

void
fatal (char *s)
{
  fatal_banner ();
  fputs (s, stderr);
  fputc ('\n', stderr);
  done (1);
}


/* Print a message for a fatal error.  Use FMT to construct the message
   and incorporate string X1.  */

void
fatals (char *fmt, char *x1)
{
  fatal_banner ();
  fprintf (stderr, fmt, x1);
  fputc ('\n', stderr);
  done (1);
}

static void
warn_banner (void)
{
  if (infile == 0)
    fprintf(stderr, _("%s: "), program_name);
  else
    fprintf(stderr, _("%s:%d: "), infile, lineno);
  failure = 1;
}

/* Print a warning message S.  */

void
warn (char *s)
{
  warn_banner ();
  fputs (s, stderr);
  fputc ('\n', stderr);
}

/* Print a warning message containing the string for the integer X1.
   The message is given by the format FMT.  */

void
warni (char *fmt, int x1)
{
  warn_banner ();
  fprintf (stderr, fmt, x1);
  fputc ('\n', stderr);
}

/* Print a warning message containing the string X1.
   The message is given by the format FMT.  */

void
warns (char *fmt, char *x1)
{
  warn_banner ();
  fprintf (stderr, fmt, x1);
  fputc ('\n', stderr);
}

/* Print a warning message containing the two strings X1 and X2.
	The message is given by the format FMT.  */

void
warnss (char *fmt, char *x1, char *x2)
{
  warn_banner ();
  fprintf (stderr, fmt, x1, x2);
  fputc ('\n', stderr);
}

/* Print a warning message containing the 3 strings X1, X2, X3.
   The message is given by the format FMT.  */

void
warnsss (char *fmt, char *x1, char *x2, char *x3)
{
  warn_banner ();
  fprintf (stderr, fmt, x1, x2, x3);
  fputc ('\n', stderr);
}

/* Print a message for the fatal occurence of more than MAXSHORT
   instances of whatever is denoted by the string S.  */

void
toomany (char *s)
{
  fatal_banner ();
  fprintf (stderr, _("too many %s (max %d)"), s, MAXSHORT);
  fputc ('\n', stderr);
  done (1);
}

/* Abort for an internal error denoted by string S.  */

void
berror (char *s)
{
  fprintf(stderr, _("%s: internal error: %s\n"), program_name, s);
  abort();
}
