/* This may look like C code, but it is really -*- C++ -*- */

/* Handles parsing the Options provided to the user.

   Copyright (C) 1989-1998, 2000 Free Software Foundation, Inc.
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
along with GNU GPERF; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111, USA.  */

/* This module provides a uniform interface to the various options available
   to a user of the gperf hash function generator.  In addition to the
   run-time options, found in the Option_Type below, there is also the
   hash table Size and the Keys to be used in the hashing.
   The overall design of this module was an experiment in using C++
   classes as a mechanism to enhance centralization of option and
   and error handling, which tend to get out of hand in a C program. */

#ifndef options_h
#define options_h 1

#include <stdio.h>

/* Enumerate the potential debugging Options. */

enum Option_Type
{
  DEBUG        = 01,            /* Enable debugging (prints diagnostics to stderr). */
  ORDER        = 02,            /* Apply ordering heuristic to speed-up search time. */
  ALLCHARS     = 04,            /* Use all characters in hash function. */
  TYPE         = 010,           /* Handle user-defined type structured keyword input. */
  RANDOM       = 020,           /* Randomly initialize the associated values table. */
  DEFAULTCHARS = 040,           /* Make default char positions be 1,$ (end of keyword). */
  SWITCH       = 0100,          /* Generate switch output to save space. */
  NOLENGTH     = 0200,          /* Don't include keyword length in hash computations. */
  LENTABLE     = 0400,          /* Generate a length table for string comparison. */
  DUP          = 01000,         /* Handle duplicate hash values for keywords. */
  FAST         = 02000,         /* Generate the hash function ``fast.'' */
  NOTYPE       = 04000,         /* Don't include user-defined type definition in output -- it's already defined elsewhere. */
  COMP         = 010000,        /* Generate strncmp rather than strcmp. */
  GLOBAL       = 020000,        /* Make the keyword table a global variable. */
  CONST        = 040000,        /* Make the generated tables readonly (const). */
  KRC          = 0100000,       /* Generate K&R C code: no prototypes, no const. */
  C            = 0200000,       /* Generate C code: no prototypes, but const (user can #define it away). */
  ANSIC        = 0400000,       /* Generate ISO/ANSI C code: prototypes and const, but no class. */
  CPLUSPLUS    = 01000000,      /* Generate C++ code: prototypes, const, class, inline, enum. */
  ENUM         = 02000000,      /* Use enum for constants. */
  INCLUDE      = 04000000,      /* Generate #include statements. */
  SEVENBIT     = 010000000      /* Assume 7-bit, not 8-bit, characters. */
};

/* Define some useful constants (these don't really belong here, but I'm
   not sure where else to put them!).  These should be consts, but g++
   doesn't seem to do the right thing with them at the moment... ;-( */

enum
{
  MAX_KEY_POS = 128 - 1,    /* Max size of each word's key set. */
  WORD_START = 1,           /* Signals the start of a word. */
  WORD_END = 0,             /* Signals the end of a word. */
  EOS = MAX_KEY_POS         /* Signals end of the key list. */
};

/* Class manager for gperf program Options. */

class Options
{
public:
                      Options (void);
                     ~Options (void);
  int                 operator[] (Option_Type option);
  void                operator() (int argc, char *argv[]);
  void                operator= (enum Option_Type);
  void                operator!= (enum Option_Type);
  static void         print_options (void);
  static void         set_asso_max (int r);
  static int          get_asso_max (void);
  static void         reset (void);
  static int          get (void);
  static int          get_iterations (void);
  static int          get_max_keysig_size (void);
  static void         set_keysig_size (int);
  static int          get_jump (void);
  static int          initial_value (void);
  static int          get_total_switches (void);
  static const char  *get_function_name (void);
  static const char  *get_key_name (void);
  static const char  *get_initializer_suffix (void);
  static const char  *get_class_name (void);
  static const char  *get_hash_name (void);
  static const char  *get_wordlist_name (void);
  static const char  *get_delimiter (void);

private:
  static int          option_word;                        /* Holds the user-specified Options. */
  static int          total_switches;                     /* Number of switch statements to generate. */
  static int          total_keysig_size;                  /* Total number of distinct key_positions. */
  static int          size;                               /* Range of the hash table. */
  static int          key_pos;                            /* Tracks current key position for Iterator. */
  static int          jump;                               /* Jump length when trying alternative values. */
  static int          initial_asso_value;                 /* Initial value for asso_values table. */
  static int          argument_count;                     /* Records count of command-line arguments. */
  static int          iterations;                         /* Amount to iterate when a collision occurs. */
  static char       **argument_vector;                    /* Stores a pointer to command-line vector. */
  static const char  *function_name;                      /* Names used for generated lookup function. */
  static const char  *key_name;                           /* Name used for keyword key. */
  static const char  *initializer_suffix;                 /* Suffix for empty struct initializers. */
  static const char  *class_name;                         /* Name used for generated C++ class. */
  static const char  *hash_name;                          /* Name used for generated hash function. */
  static const char  *wordlist_name;                      /* Name used for hash table array. */
  static const char  *delimiters;                         /* Separates keywords from other attributes. */
  static char         key_positions[MAX_KEY_POS];         /* Contains user-specified key choices. */
  static int          key_sort (char *base, int len);     /* Sorts key positions in REVERSE order. */
  static void         short_usage (FILE * strm);          /* Prints proper program usage. */
  static void         long_usage (FILE * strm);           /* Prints proper program usage. */
};

/* Global option coordinator for the entire program. */
extern Options option;

/* Set to 1 if your want to stack-allocate some large arrays.
   This requires compiler support for variable-size arrays on the stack
   (not ANSI). */
#ifndef LARGE_STACK_ARRAYS
#if defined(__GNUG__) && !defined(__STRICT_ANSI__)
#define LARGE_STACK_ARRAYS 1
#else
#define LARGE_STACK_ARRAYS 0
#endif
#endif

/* Set to 1 if the stack is large enough for holding a text line. */
#ifndef LARGE_STACK
#define LARGE_STACK 1
#endif

#ifdef __OPTIMIZE__

#include "trace.h"
#define INLINE inline
#include "options.icc"
#undef INLINE

#endif

#endif
