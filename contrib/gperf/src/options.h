/* Handles parsing the Options provided to the user.

   Copyright (C) 1989 Free Software Foundation, Inc.
   written by Douglas C. Schmidt (schmidt@ics.uci.edu)

This file is part of GNU GPERF.

GNU GPERF is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 1, or (at your option)
any later version.

GNU GPERF is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU GPERF; see the file COPYING.  If not, write to
the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

/* This module provides a uniform interface to the various Options available
   to a user of the Perfect.hash function generator.  In addition to the
   run-time Options, found in the Option_Type below, there is also the
   hash table Size and the Keys to be used in the hashing.
   The overall design of this module was an experiment in using C++
   classes as a mechanism to enhance centralization of option and
   and error handling, which tend to get out of hand in a C program. */

#ifndef _options_h
#define _options_h

#include <stdio.h>
#include "prototype.h"

/* Enumerate the potential debugging Options. */

enum option_type 
{
  DEBUG        = 01,            /* Enable debugging (prints diagnostics to Std_Err). */
  ORDER        = 02,            /* Apply ordering heuristic to speed-up search time. */
  ANSI         = 04,            /* Generate ANSI prototypes. */
  ALLCHARS     = 010,           /* Use all characters in hash function. */
  GNU          = 020,           /* Assume GNU extensions (primarily function inline). */
  TYPE         = 040,           /* Handle user-defined type structured keyword input. */
  RANDOM       = 0100,          /* Randomly initialize the associated values table. */
  DEFAULTCHARS = 0200,          /* Make default char positions be 1,$ (end of keyword). */
  SWITCH       = 0400,          /* Generate switch output to save space. */
  POINTER      = 01000,         /* Have in_word_set function return pointer, not boolean. */
  NOLENGTH     = 02000,         /* Don't include keyword length in hash computations. */
  LENTABLE     = 04000,         /* Generate a length table for string comparison. */
  DUP          = 010000,        /* Handle duplicate hash values for keywords. */
  FAST         = 020000,        /* Generate the hash function ``fast.'' */
  NOTYPE       = 040000,	      /* Don't include user-defined type definition
                                   in output -- it's already defined elsewhere. */
  COMP         = 0100000,       /* Generate strncmp rather than strcmp. */
  GLOBAL       = 0200000,       /* Make the keyword table a global variable. */
  CONST        = 0400000,       /* Make the generated tables readonly (const). */
};

/* Define some useful constants. */

/* Max size of each word's key set. */
#define MAX_KEY_POS (128 - 1)

/* Signals the start of a word. */
#define WORD_START 1           

/* Signals the end of a word. */
#define WORD_END 0             

/* Signals end of the key list. */
#define EOS MAX_KEY_POS        

/* Returns TRUE if option O is enabled. */
#define OPTION_ENABLED(OW,O) (OW.option_word & (int)O)

/* Enables option O in OPTION_WORD. */
#define SET_OPTION(OW,O) (OW.option_word |= (int)O)

/* Disable option O in OPTION_WORD. */
#define UNSET_OPTION(OW,O) (OW.option_word &= ~(int)(O))

/* Returns total distinct key positions. */
#define GET_CHARSET_SIZE(O) (O.total_charset_size)

/* Set the total distinct key positions. */
#define SET_CHARSET_SIZE(O,S) (O.total_charset_size = (S))

/* Initializes the key Iterator. */
#define RESET(O) (O.key_pos = 0)

/* Returns current key_position and advances index. */
#define GET(O) (O.key_positions[O.key_pos++])

/* Sets the size of the table size. */
#define SET_ASSO_MAX(O,R) (O.size = (R))

/* Returns the size of the table size. */
#define GET_ASSO_MAX(O) (O.size)

/* Returns the jump value. */
#define GET_JUMP(O) (O.jump)

/* Returns the iteration value. */
#define GET_ITERATIONS(O) (O.iterations)

/* Returns the lookup function name. */
#define GET_FUNCTION_NAME(O) (O.function_name)

/* Returns the keyword key name. */
#define GET_KEY_NAME(O) (O.key_name)

/* Returns the hash function name. */
#define GET_HASH_NAME(O) (O.hash_name)

/* Returns the initial associated character value. */
#define INITIAL_VALUE(O) (O.initial_asso_value)

/* Returns the string used to delimit keywords from other attributes. */
#define GET_DELIMITER(O) (O.delimiters)

/* Sets the keyword/attribute delimiters with value of D. */
#define SET_DELIMITERS(O,D) (O.delimiters = (D))

/* Gets the total number of switch statements to generate. */
#define GET_TOTAL_SWITCHES(O) (O.total_switches)

/* Class manager for gperf program options. */

typedef struct options
{
  int    option_word;           /* Holds the user-specified Options. */
  int    total_charset_size;   /* Total number of distinct key_positions. */
  int    size;                  /* Range of the hash table. */
  int    key_pos;               /* Tracks current key position for Iterator. */
  int    jump;                  /* Jump length when trying alternative values. */
  int    initial_asso_value;    /* Initial value for asso_values table. */
  int    argument_count;        /* Records count of command-line arguments. */
  int    iterations;            /* Amount to iterate when a collision occurs. */
  int    total_switches;        /* Number of switch statements to generate. */     
  char **argument_vector;       /* Stores a pointer to command-line vector. */
  char  *function_name;         /* Name used for generated lookup function. */
  char  *key_name;              /* Name used for keyword key. */
  char  *hash_name;             /* Name used for generated hash function. */
  char  *delimiters;            /* Separates keywords from other attributes. */
  char   key_positions[MAX_KEY_POS]; /* Contains user-specified key choices. */
} OPTIONS;

extern void    options_init P ((int argc, char *argv[]));
extern void    options_destroy P ((void));
extern OPTIONS option;       
#endif /* _options_h */
