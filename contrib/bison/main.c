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
the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */


#include <stdio.h>
#include "system.h"
#include "machine.h"	/* for MAXSHORT */

extern	int lineno;
extern	int verboseflag;

/* Nonzero means failure has been detected; don't write a parser file.  */
int failure;

/* The name this program was run with, for messages.  */
char *program_name;

extern void getargs(), openfiles(), reader(), reduce_grammar();
extern void set_derives(), set_nullable(), generate_states();
extern void lalr(), initialize_conflicts(), verbose(), terse();
extern void output(), done();


/* VMS complained about using `int'.  */

int
main(argc, argv)
     int argc;
     char *argv[];
{
  program_name = argv[0];
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
}

/* functions to report errors which prevent a parser from being generated */


/* Return a string containing a printable version of C:
   either C itself, or the corresponding \DDD code.  */

char *
printable_version(c)
     char c;
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
int_to_string(i)
     int i;
{
  static char buf[20];
  sprintf(buf, "%d", i);
  return buf;
}

/* Print the message S for a fatal error.  */

void
fatal(s)
     char *s;
{
  extern char *infile;

  if (infile == 0)
    fprintf(stderr, "fatal error: %s\n", s);
  else
    fprintf(stderr, "\"%s\", line %d: %s\n", infile, lineno, s);
  done(1);
}


/* Print a message for a fatal error.  Use FMT to construct the message
   and incorporate string X1.  */

void
fatals(fmt, x1)
     char *fmt, *x1;
{
  char buffer[200];
  sprintf(buffer, fmt, x1);
  fatal(buffer);
}

/* Print a warning message S.  */

void
warn(s)
     char *s;
{
  extern char *infile;

  if (infile == 0)
    fprintf(stderr, "error: %s\n", s);
  else
    fprintf(stderr, "(\"%s\", line %d) error: %s\n", 
	    infile, lineno, s);

  failure = 1;
}

/* Print a warning message containing the string for the integer X1.
   The message is given by the format FMT.  */

void
warni(fmt, x1)
     char *fmt;
     int x1;
{
  char buffer[200];
  sprintf(buffer, fmt, x1);
  warn(buffer);
}

/* Print a warning message containing the string X1.
   The message is given by the format FMT.  */

void
warns(fmt, x1)
     char *fmt, *x1;
{
  char buffer[200];
  sprintf(buffer, fmt, x1);
  warn(buffer);
}

/* Print a warning message containing the two strings X1 and X2.
	The message is given by the format FMT.  */

void
warnss(fmt, x1, x2)
     char *fmt, *x1, *x2;
{
  char buffer[200];
  sprintf(buffer, fmt, x1, x2);
  warn(buffer);
}

/* Print a warning message containing the 3 strings X1, X2, X3.
   The message is given by the format FMT.  */

void
warnsss(fmt, x1, x2, x3)
     char *fmt, *x1, *x2, *x3;
{
  char buffer[200];
  sprintf(buffer, fmt, x1, x2, x3);
  warn(buffer);
}

/* Print a message for the fatal occurence of more than MAXSHORT
   instances of whatever is denoted by the string S.  */

void
toomany(s)
     char *s;
{
  char buffer[200];
  sprintf(buffer, "limit of %d exceeded, too many %s", MAXSHORT, s);
  fatal(buffer);
}

/* Abort for an internal error denoted by string S.  */

void
berror(s)
     char *s;
{
  fprintf(stderr, "internal error, %s\n", s);
  abort();
}
