/* cmds.h -- declarations for cmds.c.
   $Id: cmds.h,v 1.4 1999/04/25 20:43:51 karl Exp $

   Copyright (C) 1998, 99 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation, Inc.,
   59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#ifndef CMDS_H
#define CMDS_H

/* The three arguments a command can get are a flag saying whether it is
   before argument parsing (START) or after (END), the starting position
   of the arguments, and the ending position.  */
typedef void COMMAND_FUNCTION (); /* So we can say COMMAND_FUNCTION *foo; */

/* Each command has an associated function.  When the command is
   encountered in the text, the associated function is called with START
   as the argument.  If the function expects arguments in braces, it
   remembers itself on the stack.  When the corresponding close brace is
   encountered, the function is called with END as the argument. */
#define START 0
#define END 1

/* Does the command expect braces?  */
#define NO_BRACE_ARGS 0
#define BRACE_ARGS 1
#define MAYBE_BRACE_ARGS 2

typedef struct
{
  char *name;
  COMMAND_FUNCTION *proc;
  int argument_in_braces;
} COMMAND;

extern COMMAND command_table[];

#endif /* !CMDS_H */
