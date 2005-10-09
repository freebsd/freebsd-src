/* hash - hashing table processing.
   Copyright (C) 1998, 1999 Free Software Foundation, Inc.
   Written by Jim Meyering <meyering@ascend.com>, 1998.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

/* A generic hash table package.  */

/* Make sure USE_OBSTACK is defined to 1 if you want the allocator to use
   obstacks instead of malloc, and recompile `hash.c' with same setting.  */

#ifndef PARAMS
# if PROTOTYPES || __STDC__
#  define PARAMS(Args) Args
# else
#  define PARAMS(Args) ()
# endif
#endif

typedef unsigned (*Hash_hasher) PARAMS ((const void *, unsigned));
typedef bool (*Hash_comparator) PARAMS ((const void *, const void *));
typedef void (*Hash_data_freer) PARAMS ((void *));
typedef bool (*Hash_processor) PARAMS ((void *, void *));

struct hash_entry
  {
    void *data;
    struct hash_entry *next;
  };

struct hash_tuning
  {
    /* This structure is mainly used for `hash_initialize', see the block
       documentation of `hash_reset_tuning' for more complete comments.  */

    float shrink_threshold;	/* ratio of used buckets to trigger a shrink */
    float shrink_factor;	/* ratio of new smaller size to original size */
    float growth_threshold;	/* ratio of used buckets to trigger a growth */
    float growth_factor;	/* ratio of new bigger size to original size */
    bool is_n_buckets;		/* if CANDIDATE really means table size */
  };

typedef struct hash_tuning Hash_tuning;

struct hash_table
  {
    /* The array of buckets starts at BUCKET and extends to BUCKET_LIMIT-1,
       for a possibility of N_BUCKETS.  Among those, N_BUCKETS_USED buckets
       are not empty, there are N_ENTRIES active entries in the table.  */
    struct hash_entry *bucket;
    struct hash_entry *bucket_limit;
    unsigned n_buckets;
    unsigned n_buckets_used;
    unsigned n_entries;

    /* Tuning arguments, kept in a physicaly separate structure.  */
    const Hash_tuning *tuning;

    /* Three functions are given to `hash_initialize', see the documentation
       block for this function.  In a word, HASHER randomizes a user entry
       into a number up from 0 up to some maximum minus 1; COMPARATOR returns
       true if two user entries compare equally; and DATA_FREER is the cleanup
       function for a user entry.  */
    Hash_hasher hasher;
    Hash_comparator comparator;
    Hash_data_freer data_freer;

    /* A linked list of freed struct hash_entry structs.  */
    struct hash_entry *free_entry_list;

#if USE_OBSTACK
    /* Whenever obstacks are used, it is possible to allocate all overflowed
       entries into a single stack, so they all can be freed in a single
       operation.  It is not clear if the speedup is worth the trouble.  */
    struct obstack entry_stack;
#endif
  };

typedef struct hash_table Hash_table;

/* Information and lookup.  */
unsigned hash_get_n_buckets PARAMS ((const Hash_table *));
unsigned hash_get_n_buckets_used PARAMS ((const Hash_table *));
unsigned hash_get_n_entries PARAMS ((const Hash_table *));
unsigned hash_get_max_bucket_length PARAMS ((const Hash_table *));
bool hash_table_ok PARAMS ((const Hash_table *));
void hash_print_statistics PARAMS ((const Hash_table *, FILE *));
void *hash_lookup PARAMS ((const Hash_table *, const void *));

/* Walking.  */
void *hash_get_first PARAMS ((const Hash_table *));
void *hash_get_next PARAMS ((const Hash_table *, const void *));
unsigned hash_get_entries PARAMS ((const Hash_table *, void **, unsigned));
unsigned hash_do_for_each PARAMS ((const Hash_table *, Hash_processor, void *));

/* Allocation and clean-up.  */
unsigned hash_string PARAMS ((const char *, unsigned));
void hash_reset_tuning PARAMS ((Hash_tuning *));
Hash_table *hash_initialize PARAMS ((unsigned, const Hash_tuning *,
				     Hash_hasher, Hash_comparator,
				     Hash_data_freer));
void hash_clear PARAMS ((Hash_table *));
void hash_free PARAMS ((Hash_table *));

/* Insertion and deletion.  */
bool hash_rehash PARAMS ((Hash_table *, unsigned));
void *hash_insert PARAMS ((Hash_table *, const void *));
void *hash_delete PARAMS ((Hash_table *, const void *));
