/* Provides high-level routines to manipulate the keywork list 
   structures the code generation output.
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

#include <stdio.h>
#include <assert.h>
#include <ctype.h>
#include "options.h"
#include "perfect.h"
#include "stderr.h"

/* Current release version. */
extern char *version_string;

/* Counts occurrences of each key set character. */
int occurrences[ALPHABET_SIZE]; 

/* Value associated with each character. */
int asso_values[ALPHABET_SIZE]; 

/* Locally visible PERFECT object. */
PERFECT perfect;

/* Efficiently returns the least power of two greater than or equal to X! */
#define POW(X) ((!X)?1:(X-=1,X|=X>>1,X|=X>>2,X|=X>>4,X|=X>>8,X|=X>>16,(++X)))

/* Reads input keys, possibly applies the reordering heuristic, sets the
   maximum associated value size (rounded up to the nearest power of 2),
   may initialize the associated values array, and determines the maximum
   hash table size.  Note: using the random numbers is often helpful,
   though not as deterministic, of course! */

void
perfect_init ()
{
  int asso_value_max;
  int len;

  perfect.num_done = 1;
  perfect.fewest_collisions = 0;
  read_keys ();
  if (OPTION_ENABLED (option, ORDER))
    reorder ();
  asso_value_max = GET_ASSO_MAX (option);
  len            = keyword_list_length ();
  asso_value_max = (asso_value_max ? asso_value_max * len : len);
  SET_ASSO_MAX (option, POW (asso_value_max));
  
  if (OPTION_ENABLED (option, RANDOM))
    {
      int i;

      srandom (time (0));
      
      for (i = 0; i < ALPHABET_SIZE; i++)
        asso_values[i] = (random () & asso_value_max - 1);
    }
  else
    {
      int asso_value = INITIAL_VALUE (option);
      if (asso_value)           /* Initialize array if user requests non-zero default. */
        {
          int i;

          for (i = ALPHABET_SIZE - 1; i >= 0; i--)
            asso_values[i] = asso_value & GET_ASSO_MAX (option) - 1;
        }
    }
  perfect.max_hash_value = max_key_length () + GET_ASSO_MAX (option) * 
    GET_CHARSET_SIZE (option);
  
  printf ("/* C code produced by gperf version %s */\n", version_string);
  print_options ();

  if (OPTION_ENABLED (option, DEBUG))
    {
      int i;
      fprintf (stderr, "\nnumber of keys = %d\nmaximum associated value is %d\
\nmaximum possible size of generated hash table is %d\n", 
               len, asso_value_max, perfect.max_hash_value);
    }
}

/* Merge two hash key multisets to form the ordered disjoint union of the sets.
   (In a multiset, an element can occur multiple times). Precondition: both 
   set_1 and set_2 must be ordered. Returns the length of the combined set. */

static int 
compute_disjoint_union (set_1, set_2, set_3)
     char *set_1;
     char *set_2;
     char *set_3;
{
  char *base = set_3;
  
  while (*set_1 && *set_2)
    if (*set_1 == *set_2)
      set_1++, set_2++; 
    else
      {
        *set_3 = *set_1 < *set_2 ? *set_1++ : *set_2++;
        if (set_3 == base || *set_3 != *(set_3-1)) set_3++;
      }
   
  while (*set_1)
    {
      *set_3 = *set_1++; 
      if (set_3 == base || *set_3 != *(set_3-1)) set_3++;
    }
   
  while (*set_2)
    {
      *set_3 = *set_2++; 
      if (set_3 == base || *set_3 != *(set_3-1)) set_3++;
    }
  *set_3 = '\0';
  return set_3 - base;
}

/* Sort the UNION_SET in increasing frequency of occurrence.
   This speeds up later processing since we may assume the resulting
   set (Set_3, in this case), is ordered. Uses insertion sort, since
   the UNION_SET is typically short. */
  
static void 
sort_set (union_set, len)
     char *union_set;
     int   len;
{
  int i, j;
  
  for (i = 0, j = len - 1; i < j; i++)
    {
      char curr, tmp;

      for (curr = i+1, tmp = union_set[curr]; 
           curr > 0 && occurrences[tmp] < occurrences[union_set[curr-1]]; 
           curr--)
        union_set[curr] = union_set[curr - 1];
      
      union_set[curr] = tmp;
    }
}

/* Generate a key set's hash value. */

static int 
hash (key_node)
     LIST_NODE *key_node;
{                             
  int   sum = OPTION_ENABLED (option, NOLENGTH) ? 0 : key_node->length;
  char *ptr;

  for (ptr = key_node->char_set; *ptr; ptr++)
      sum += asso_values[*ptr];
  
  return key_node->hash_value = sum;
}

/* Find out how associated value changes affect successfully hashed items.
   Returns FALSE if no other hash values are affected, else returns TRUE.
   Note that because GET_ASSO_MAX (option) is a power of two we can guarantee
   that all legal ASSO_VALUES are visited without repetition since
   GET_JUMP (option) was forced to be an odd value! */

static bool 
affects_prev (c, curr)
     char c;
     LIST_NODE *curr;
{
  int original_char = asso_values[c];
  int i = !OPTION_ENABLED (option, FAST) ? GET_ASSO_MAX (option) : 
    GET_ITERATIONS (option) == 0 ? key_list.list_len : GET_ITERATIONS (option);

  /* Try all asso_values. */

  while (--i >= 0)
    { 
      int        collisions = 0;
      LIST_NODE *ptr;

      asso_values[c] = asso_values[c] + (GET_JUMP (option) ? GET_JUMP (option) : random ())
        & GET_ASSO_MAX (option) - 1;
      bool_array_reset ();

      /* See how this asso_value change affects previous keywords.  If
         it does better than before we'll take it! */

      for (ptr = key_list.head; 
           !lookup (hash (ptr)) || ++collisions < perfect.fewest_collisions; 
           ptr = ptr->next)
        if (ptr == curr)
          {
            perfect.fewest_collisions = collisions;
            return FALSE;        
          }    
    }
  
  asso_values[c] = original_char; /* Restore original values, no more tries. */
  return TRUE; /* If we're this far it's time to try the next character.... */
}

/* Change a character value, try least-used characters first. */

static void 
change (prior, curr)
     LIST_NODE *prior;
     LIST_NODE *curr;
{
  char        *xmalloc ();
  static char *union_set = 0;
  char        *temp;
  LIST_NODE   *ptr;

  if (!union_set)
    union_set = xmalloc (2 * GET_CHARSET_SIZE (option) + 1);

  if (OPTION_ENABLED (option, DEBUG)) /* Very useful for debugging. */
    {
      fprintf (stderr, "collision on keyword #%d, prior=\"%s\", curr=\"%s\", hash=%d\n",
               perfect.num_done, prior->key, curr->key, curr->hash_value);
      fflush (stderr);
    }
  sort_set (union_set, compute_disjoint_union (prior->char_set, curr->char_set, union_set));
  
  /* Try changing some values, if change doesn't alter other values continue normal action. */
  
  perfect.fewest_collisions++;
  
  for (temp = union_set; *temp; temp++)
    if (!affects_prev (*temp, curr))
      {
        if (OPTION_ENABLED (option, DEBUG))
          {
            fprintf (stderr, "- resolved by changing asso_value['%c'] (char #%d) to %d\n", 
                     *temp, temp - union_set + 1, asso_values[*temp]);
            fflush (stderr);
          }
        return; /* Good, doesn't affect previous hash values, we'll take it. */
      }

  for (ptr = key_list.head; ptr != curr; ptr = ptr->next)
    hash (ptr);

  hash (curr);

  if (OPTION_ENABLED (option, DEBUG))
    {
      fprintf (stderr, "** collision not resolved, %d duplicates remain, continuing...\n", 
               perfect.fewest_collisions); 
      fflush (stderr);
    }
}

/* Does the hard stuff....
   Initializes the Iteration Number boolean array, and then trys to find a 
   perfect function that will hash all the key words without getting any
   duplications.  This is made much easier since we aren't attempting
   to generate *minimum* functions, only perfect ones.
   If we can't generate a perfect function in one pass *and* the user
   hasn't enabled the DUP option, we'll inform the user to try the
   randomization option, use -D, or choose alternative key positions.  
   The alternatives (e.g., back-tracking) are too time-consuming, i.e,
   exponential in the number of keys. */

int
perfect_generate ()
{
  LIST_NODE *curr;
  bool_array_init (perfect.max_hash_value);
  
  for (curr = key_list.head; curr; curr = curr->next)
    {
      LIST_NODE *ptr;
      hash (curr);
      
      for (ptr = key_list.head; ptr != curr; ptr = ptr->next)
        if (ptr->hash_value == curr->hash_value)
          {
            change (ptr, curr);
            break;
          }
      perfect.num_done++;
    } 
  

  /* Make one final check, just to make sure nothing weird happened.... */
  bool_array_reset ();

  for (curr = key_list.head; curr; curr = curr->next)
    if (lookup (hash (curr)))
      if (OPTION_ENABLED (option, DUP)) /* We'll try to deal with this later..... */
        break;
      else /* Yow, big problems.  we're outta here! */
        { 
          report_error ("\nInternal error, duplicate value %d:\n\
try options -D or -r, or use new key positions.\n\n", 
                        hash (curr));
          return 1;
        }

  bool_array_destroy ();

  /* First sorts the key word list by hash value, and the outputs the
     list to the proper ostream. The generated hash table code is only 
     output if the early stage of processing turned out O.K. */

  sort ();
  print_output ();
  return 0;
}

/* Prints out some diagnostics upon completion. */

void 
perfect_destroy ()
{                             
  if (OPTION_ENABLED (option, DEBUG))
    {
      int i;

      fprintf (stderr, "\ndumping occurrence and associated values tables\n");
      
      for (i = 0; i < ALPHABET_SIZE; i++)
        if (occurrences[i])
          fprintf (stderr, "asso_values[%c] = %3d, occurrences[%c] = %3d\n",
                   i, asso_values[i], i, occurrences[i]);
      
      fprintf (stderr, "end table dumping\n");
      
    }
}

