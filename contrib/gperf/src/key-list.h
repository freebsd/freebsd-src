/* This may look like C code, but it is really -*- C++ -*- */

/* Data and function member declarations for the keyword list class.

   Copyright (C) 1989-1998 Free Software Foundation, Inc.
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

/* The key word list is a useful abstraction that keeps track of
   various pieces of information that enable that fast generation
   of the Gen_Perf.hash function.  A Key_List is a singly-linked
   list of List_Nodes. */

#ifndef key_list_h
#define key_list_h 1

#include "list-node.h"
#include "vectors.h"
#include "read-line.h"

/* OSF/1 cxx needs these forward declarations. */
struct Output_Constants;
struct Output_Compare;

class Key_List : private Read_Line, public Vectors
{
private:
  const char *array_type;                            /* Pointer to the type for word list. */
  const char *return_type;                           /* Pointer to return type for lookup function. */
  const char *struct_tag;                            /* Shorthand for user-defined struct tag type. */
  const char *include_src;                           /* C source code to be included verbatim. */
  int         max_key_len;                           /* Maximum length of the longest keyword. */
  int         min_key_len;                           /* Minimum length of the shortest keyword. */
  int         min_hash_value;                        /* Minimum hash value for all keywords. */
  int         max_hash_value;                        /* Maximum hash value for all keywords. */
  int         occurrence_sort;                       /* True if sorting by occurrence. */
  int         hash_sort;                             /* True if sorting by hash value. */
  int         additional_code;                       /* True if any additional C code is included. */
  int         list_len;                              /* Length of head's Key_List, not counting duplicates. */
  int         total_keys;                            /* Total number of keys, counting duplicates. */
  static int  determined[MAX_ALPHA_SIZE];            /* Used in function reorder, below. */
  static int  get_occurrence (List_Node *ptr);
#ifndef strcspn
  static int  strcspn (const char *s, const char *reject);
#endif
  static int  already_determined (List_Node *ptr);
  static void set_determined (List_Node *ptr);
  void        compute_min_max (void);
  int         num_hash_values (void);
  void        output_constants (struct Output_Constants&);
  void        output_hash_function (void);
  void        output_keylength_table (void);
  void        output_keyword_table (void);
  void        output_lookup_array (void);
  void        output_lookup_tables (void);
  void        output_lookup_function_body (const struct Output_Compare&);
  void        output_lookup_function (void);
  void        set_output_types (void);
  void        dump (void);
  const char *get_array_type (void);
  const char *save_include_src (void);
  const char *get_special_input (char delimiter);
  List_Node  *merge (List_Node *list1, List_Node *list2);
  List_Node  *merge_sort (List_Node *head);

protected:
  List_Node  *head;                                  /* Points to the head of the linked list. */
  int         total_duplicates;                      /* Total number of duplicate hash values. */

public:
              Key_List   (void);
             ~Key_List  (void);
  int         keyword_list_length (void);
  int         max_key_length (void);
  void        reorder (void);
  void        sort (void);
  void        read_keys (void);
  void        output (void);
};

#endif
