/* Provides high-level routines to manipulate the keywork list
   structures the code generation output.
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

#include <stdio.h>
#include <stdlib.h> /* declares rand(), srand() */
#include <time.h> /* declares time() */
#include "options.h"
#include "gen-perf.h"
#include "trace.h"

/* Efficiently returns the least power of two greater than or equal to X! */
#define POW(X) ((!X)?1:(X-=1,X|=X>>1,X|=X>>2,X|=X>>4,X|=X>>8,X|=X>>16,(++X)))

/* Reads input keys, possibly applies the reordering heuristic, sets the
   maximum associated value size (rounded up to the nearest power of 2),
   may initialize the associated values array, and determines the maximum
   hash table size.  Note: using the random numbers is often helpful,
   though not as deterministic, of course! */

Gen_Perf::Gen_Perf (void)
{
  T (Trace t ("Gen_Perf::Gen_Perf");)
  int asso_value_max;
  int non_linked_length;

  Key_List::read_keys ();
  if (option[ORDER])
    reorder ();
  asso_value_max    = option.get_asso_max ();
  non_linked_length = Key_List::keyword_list_length ();
  num_done          = 1;
  fewest_collisions = 0;
  if (asso_value_max == 0)
    asso_value_max = non_linked_length;
  else if (asso_value_max > 0)
    asso_value_max *= non_linked_length;
  else /* if (asso_value_max < 0) */
    asso_value_max = non_linked_length / -asso_value_max;
  option.set_asso_max (POW (asso_value_max));

  if (option[RANDOM])
    {
      srand ((long) time (0));

      for (int i = 0; i < ALPHA_SIZE; i++)
        asso_values[i] = (rand () & asso_value_max - 1);
    }
  else
    {
      int asso_value = option.initial_value ();

      if (asso_value)           /* Initialize array if user requests non-zero default. */
        for (int i = ALPHA_SIZE - 1; i >= 0; i--)
          asso_values[i] = asso_value & option.get_asso_max () - 1;
    }
  max_hash_value = Key_List::max_key_length () + option.get_asso_max () *
    option.get_max_keysig_size ();

  if (option[DEBUG])
    fprintf (stderr, "total non-linked keys = %d\nmaximum associated value is %d"
             "\nmaximum size of generated hash table is %d\n",
             non_linked_length, asso_value_max, max_hash_value);
}

/* Merge two disjoint hash key multisets to form the ordered disjoint union of the sets.
   (In a multiset, an element can occur multiple times.)
   Precondition: both set_1 and set_2 must be ordered. Returns the length
   of the combined set. */

inline int
Gen_Perf::compute_disjoint_union  (const char *set_1, int size_1, const char *set_2, int size_2, char *set_3)
{
  T (Trace t ("Gen_Perf::compute_disjoint_union");)
  char *base = set_3;

  while (size_1 > 0 && size_2 > 0)
    if (*set_1 == *set_2)
      set_1++, size_1--, set_2++, size_2--;
    else
      {
        char next;
        if (*set_1 < *set_2)
          next = *set_1++, size_1--;
        else
          next = *set_2++, size_2--;
        if (set_3 == base || next != set_3[-1])
          *set_3++ = next;
      }

  while (size_1 > 0)
    {
      char next;
      next = *set_1++, size_1--;
      if (set_3 == base || next != set_3[-1])
        *set_3++ = next;
    }

  while (size_2 > 0)
    {
      char next;
      next = *set_2++, size_2--;
      if (set_3 == base || next != set_3[-1])
        *set_3++ = next;
    }
  return set_3 - base;
}

/* Sort the UNION_SET in increasing frequency of occurrence.
   This speeds up later processing since we may assume the resulting
   set (Set_3, in this case), is ordered. Uses insertion sort, since
   the UNION_SET is typically short. */

inline void
Gen_Perf::sort_set (char *union_set, int len)
{
  T (Trace t ("Gen_Perf::sort_set");)
  int i, j;

  for (i = 0, j = len - 1; i < j; i++)
    {
      int curr;
      char tmp;

      for (curr = i + 1, tmp = union_set[curr];
           curr > 0 && occurrences[(unsigned char)tmp] < occurrences[(unsigned char)(union_set[curr-1])];
           curr--)
        union_set[curr] = union_set[curr - 1];

      union_set[curr] = tmp;
    }
}

/* Generate a key set's hash value. */

inline int
Gen_Perf::hash (List_Node *key_node)
{
  T (Trace t ("Gen_Perf::hash");)
  int sum = option[NOLENGTH] ? 0 : key_node->key_length;

  const char *p = key_node->char_set;
  int i = key_node->char_set_length;
  for (; i > 0; p++, i--)
      sum += asso_values[(unsigned char)(*p)];

  return key_node->hash_value = sum;
}

/* Find out how character value change affects successfully hashed items.
   Returns FALSE if no other hash values are affected, else returns TRUE.
   Note that because Option.Get_Asso_Max is a power of two we can guarantee
   that all legal Asso_Values are visited without repetition since
   Option.Get_Jump was forced to be an odd value! */

inline int
Gen_Perf::affects_prev (char c, List_Node *curr)
{
  T (Trace t ("Gen_Perf::affects_prev");)
  int original_char = asso_values[(unsigned char)c];
  int total_iterations = !option[FAST]
    ? option.get_asso_max () : option.get_iterations () ? option.get_iterations () : keyword_list_length ();

  /* Try all legal associated values. */

  for (int i = total_iterations - 1; i >= 0; i--)
    {
      int collisions = 0;

      asso_values[(unsigned char)c] =
        (asso_values[(unsigned char)c] + (option.get_jump () ? option.get_jump () : rand ()))
        & (option.get_asso_max () - 1);

      /* Iteration Number array is a win, O(1) intialization time! */
      reset ();

      /* See how this asso_value change affects previous keywords.  If
         it does better than before we'll take it! */

      for (List_Node *ptr = head;
           !Bool_Array::find (hash (ptr)) || ++collisions < fewest_collisions;
           ptr = ptr->next)
        if (ptr == curr)
          {
            fewest_collisions = collisions;
            if (option[DEBUG])
              fprintf (stderr, "- resolved after %d iterations", total_iterations - i);
            return 0;
          }
    }

  /* Restore original values, no more tries. */
  asso_values[(unsigned char)c] = original_char;
  /* If we're this far it's time to try the next character.... */
  return 1;
}

/* Change a character value, try least-used characters first. */

void
Gen_Perf::change (List_Node *prior, List_Node *curr)
{
  T (Trace t ("Gen_Perf::change");)
  static char *union_set;
  int union_set_length;

  if (!union_set)
    union_set = new char [2 * option.get_max_keysig_size ()];

  if (option[DEBUG])
    {
      fprintf (stderr, "collision on keyword #%d, prior = \"%.*s\", curr = \"%.*s\" hash = %d\n",
               num_done,
               prior->key_length, prior->key,
               curr->key_length, curr->key,
               curr->hash_value);
      fflush (stderr);
    }
  union_set_length = compute_disjoint_union (prior->char_set, prior->char_set_length, curr->char_set, curr->char_set_length, union_set);
  sort_set (union_set, union_set_length);

  /* Try changing some values, if change doesn't alter other values continue normal action. */
  fewest_collisions++;

  const char *p = union_set;
  int i = union_set_length;
  for (; i > 0; p++, i--)
    if (!affects_prev (*p, curr))
      {
        if (option[DEBUG])
          {
            fprintf (stderr, " by changing asso_value['%c'] (char #%zd) to %d\n",
                     *p, p - union_set + 1, asso_values[(unsigned char)(*p)]);
            fflush (stderr);
          }
        return; /* Good, doesn't affect previous hash values, we'll take it. */
      }

  for (List_Node *ptr = head; ptr != curr; ptr = ptr->next)
    hash (ptr);

  hash (curr);

  if (option[DEBUG])
    {
      fprintf (stderr, "** collision not resolved after %d iterations, %d duplicates remain, continuing...\n",
               !option[FAST] ? option.get_asso_max () : option.get_iterations () ? option.get_iterations () : keyword_list_length (),
               fewest_collisions + total_duplicates);
      fflush (stderr);
    }
}

/* Does the hard stuff....
   Initializes the Iteration Number array, and attempts to find a perfect
   function that will hash all the key words without getting any
   duplications.  This is made much easier since we aren't attempting
   to generate *minimum* functions, only perfect ones.
   If we can't generate a perfect function in one pass *and* the user
   hasn't enabled the DUP option, we'll inform the user to try the
   randomization option, use -D, or choose alternative key positions.
   The alternatives (e.g., back-tracking) are too time-consuming, i.e,
   exponential in the number of keys. */

int
Gen_Perf::operator() (void)
{
  T (Trace t ("Gen_Perf::operator()");)
#if LARGE_STACK_ARRAYS
  STORAGE_TYPE buffer[max_hash_value + 1];
#else
  // Note: we don't use new, because that invokes a custom operator new.
  STORAGE_TYPE *buffer
    = (STORAGE_TYPE*) malloc (sizeof(STORAGE_TYPE) * (max_hash_value + 1));
  if (buffer == NULL)
    abort ();
#endif

  Bool_Array::init (buffer, max_hash_value + 1);

  List_Node *curr;
  for (curr = head; curr; curr = curr->next)
    {
      hash (curr);

      for (List_Node *ptr = head; ptr != curr; ptr = ptr->next)
        if (ptr->hash_value == curr->hash_value)
          {
            change (ptr, curr);
            break;
          }
      num_done++;
    }

  /* Make one final check, just to make sure nothing weird happened.... */

  Bool_Array::reset ();

  for (curr = head; curr; curr = curr->next)
    if (Bool_Array::find (hash (curr)))
      if (option[DUP]) /* Keep track of this number... */
        total_duplicates++;
      else /* Yow, big problems.  we're outta here! */
        {
          fprintf (stderr, "\nInternal error, duplicate value %d:\n"
                           "try options -D or -r, or use new key positions.\n\n", hash (curr));
#if !LARGE_STACK_ARRAYS
          free ((char *) buffer);
#endif
          return 1;
        }

  /* Sorts the key word list by hash value, and then outputs the list.
     The generated hash table code is only output if the early stage of
     processing turned out O.K. */

  sort ();
  output ();
#if !LARGE_STACK_ARRAYS
  free ((char *) buffer);
#endif
  return 0;
}

/* Prints out some diagnostics upon completion. */

Gen_Perf::~Gen_Perf (void)
{
  T (Trace t ("Gen_Perf::~Gen_Perf");)
  if (option[DEBUG])
    {
      fprintf (stderr, "\ndumping occurrence and associated values tables\n");

      for (int i = 0; i < ALPHA_SIZE; i++)
        if (occurrences[i])
          fprintf (stderr, "asso_values[%c] = %6d, occurrences[%c] = %6d\n",
                   i, asso_values[i], i, occurrences[i]);

      fprintf (stderr, "end table dumping\n");

    }
}

