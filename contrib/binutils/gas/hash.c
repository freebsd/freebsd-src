/* hash.c - hash table lookup strings -
   Copyright (C) 1987, 90, 91, 92, 93, 94, 95, 96, 1998
   Free Software Foundation, Inc.

   This file is part of GAS, the GNU Assembler.

   GAS is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   GAS is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GAS; see the file COPYING.  If not, write to
   the Free Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

/*
 * BUGS, GRIPES, APOLOGIA etc.
 *
 * A typical user doesn't need ALL this: I intend to make a library out
 * of it one day - Dean Elsner.
 * Also, I want to change the definition of a symbol to (address,length)
 * so I can put arbitrary binary in the names stored. [see hsh.c for that]
 *
 * This slime is common coupled inside the module. Com-coupling (and other
 * vandalism) was done to speed running time. The interfaces at the
 * module's edges are adequately clean.
 *
 * There is no way to (a) run a test script through this heap and (b)
 * compare results with previous scripts, to see if we have broken any
 * code. Use GNU (f)utilities to do this. A few commands assist test.
 * The testing is awkward: it tries to be both batch & interactive.
 * For now, interactive rules!
 */

/*
 *  The idea is to implement a symbol table. A test jig is here.
 *  Symbols are arbitrary strings; they can't contain '\0'.
 *	[See hsh.c for a more general symbol flavour.]
 *  Each symbol is associated with a char*, which can point to anything
 *  you want, allowing an arbitrary property list for each symbol.
 *
 *  The basic operations are:
 *
 *    new                     creates symbol table, returns handle
 *    find (symbol)           returns char*
 *    insert (symbol,char*)   error if symbol already in table
 *    delete (symbol)         returns char* if symbol was in table
 *    apply                   so you can delete all symbols before die()
 *    die                     destroy symbol table (free up memory)
 *
 *  Supplementary functions include:
 *
 *    say                     how big? what % full?
 *    replace (symbol,newval) report previous value
 *    jam (symbol,value)      assert symbol:=value
 *
 *  You, the caller, have control over errors: this just reports them.
 *
 *  This package requires malloc(), free().
 *  Malloc(size) returns NULL or address of char[size].
 *  Free(address) frees same.
 */

/*
 *  The code and its structures are re-enterent.
 *
 *  Before you do anything else, you must call hash_new() which will
 *  return the address of a hash-table-control-block.  You then use
 *  this address as a handle of the symbol table by passing it to all
 *  the other hash_...() functions.  The only approved way to recover
 *  the memory used by the symbol table is to call hash_die() with the
 *  handle of the symbol table.
 *
 *  Before you call hash_die() you normally delete anything pointed to
 *  by individual symbols. After hash_die() you can't use that symbol
 *  table again.
 *
 *  The char* you associate with a symbol may not be NULL (0) because
 *  NULL is returned whenever a symbol is not in the table. Any other
 *  value is OK, except DELETED, #defined below.
 *
 *  When you supply a symbol string for insertion, YOU MUST PRESERVE THE
 *  STRING until that symbol is deleted from the table. The reason is that
 *  only the address you supply, NOT the symbol string itself, is stored
 *  in the symbol table.
 *
 *  You may delete and add symbols arbitrarily.
 *  Any or all symbols may have the same 'value' (char *). In fact, these
 *  routines don't do anything with your symbol values.
 *
 *  You have no right to know where the symbol:char* mapping is stored,
 *  because it moves around in memory; also because we may change how it
 *  works and we don't want to break your code do we? However the handle
 *  (address of struct hash_control) is never changed in
 *  the life of the symbol table.
 *
 *  What you CAN find out about a symbol table is:
 *    how many slots are in the hash table?
 *    how many slots are filled with symbols?
 *    (total hashes,collisions) for (reads,writes) (*)
 *  All of the above values vary in time.
 *  (*) some of these numbers will not be meaningful if we change the
 *  internals. */

/*
 *  I N T E R N A L
 *
 *  Hash table is an array of hash_entries; each entry is a pointer to a
 *  a string and a user-supplied value 1 char* wide.
 *
 *  The array always has 2 ** n elements, n>0, n integer.
 *  There is also a 'wall' entry after the array, which is always empty
 *  and acts as a sentinel to stop running off the end of the array.
 *  When the array gets too full, we create a new array twice as large
 *  and re-hash the symbols into the new array, then forget the old array.
 *  (Of course, we copy the values into the new array before we junk the
 *  old array!)
 *
 */

#include <stdio.h>

#ifndef FALSE
#define FALSE	(0)
#define TRUE	(!FALSE)
#endif /* no FALSE yet */

#include <ctype.h>
#define min(a, b)	((a) < (b) ? (a) : (b))

#include "as.h"

#define error	as_fatal

static char _deleted_[1];
#define DELETED     ((PTR)_deleted_)	/* guarenteed unique address */
#define START_POWER    (10)	/* power of two: size of new hash table */

/* TRUE if a symbol is in entry @ ptr.  */
#define islive(ptr) (ptr->hash_string && ptr->hash_string!=DELETED)

enum stat_enum {
  /* Number of slots in hash table.  The wall does not count here.
     We expect this is always a power of 2.  */
  STAT_SIZE = 0,
  /* Number of hash_ask calls.  */
  STAT_ACCESS,
  STAT_ACCESS_w,
  /* Number of collisions (total).  This may exceed STAT_ACCESS if we
     have lots of collisions/access.  */
  STAT_COLLIDE,
  STAT_COLLIDE_w,
  /* Slots used right now.  */
  STAT_USED,
  /* How many string compares?  */
  STAT_STRCMP,
  STAT_STRCMP_w,
  /* Size of statistics block... this must be last.  */
  STATLENGTH
};
#define STAT__READ     (0)	/* reading */
#define STAT__WRITE    (1)	/* writing */

/* When we grow a hash table, by what power of two do we increase it?  */
#define GROW_FACTOR 1
/* When should we grow it?  */
#define FULL_VALUE(N)	((N) / 2)

/* #define SUSPECT to do runtime checks */
/* #define TEST to be a test jig for hash...() */

#ifdef TEST
/* TEST: use smaller hash table */
#undef  START_POWER
#define START_POWER (3)
#undef  START_SIZE
#define START_SIZE  (8)
#undef  START_FULL
#define START_FULL  (4)
#endif

struct hash_entry
{
  const char *hash_string;	/* points to where the symbol string is */
  /* NULL means slot is not used */
  /* DELETED means slot was deleted */
  PTR hash_value;		/* user's datum, associated with symbol */
  unsigned long h;
};

struct hash_control {
  struct hash_entry *hash_where;/* address of hash table */
  int hash_sizelog;		/* Log of ( hash_mask + 1 ) */
  int hash_mask;		/* masks a hash into index into table */
  int hash_full;		/* when hash_stat[STAT_USED] exceeds this, */
  /* grow table */
  struct hash_entry *hash_wall;	/* point just after last (usable) entry */
  /* here we have some statistics */
  int hash_stat[STATLENGTH];	/* lies & statistics */
};

/*------------------ plan ---------------------------------- i = internal

  struct hash_control * c;
  struct hash_entry   * e;                                                    i
  int                   b[z];     buffer for statistics
  z         size of b
  char                * s;        symbol string (address) [ key ]
  char                * v;        value string (address)  [datum]
  boolean               f;        TRUE if we found s in hash table            i
  char                * t;        error string; 0 means OK
  int                   a;        access type [0...n)                         i

  c=hash_new       ()             create new hash_control

  hash_die         (c)            destroy hash_control (and hash table)
  table should be empty.
  doesn't check if table is empty.
  c has no meaning after this.

  hash_say         (c,b,z)        report statistics of hash_control.
  also report number of available statistics.

  v=hash_delete    (c,s)          delete symbol, return old value if any.
  ask()                       NULL means no old value.
  f

  v=hash_replace   (c,s,v)        replace old value of s with v.
  ask()                       NULL means no old value: no table change.
  f

  t=hash_insert    (c,s,v)        insert (s,v) in c.
  ask()                       return error string.
  f                           it is an error to insert if s is already
  in table.
  if any error, c is unchanged.

  t=hash_jam       (c,s,v)        assert that new value of s will be v.       i
  ask()                       it may decide to GROW the table.            i
  f                                                                       i
  grow()                                                                  i
  t=hash_grow      (c)            grow the hash table.                        i
  jam()                       will invoke JAM.                            i

  ?=hash_apply     (c,y)          apply y() to every symbol in c.
  y                           evtries visited in 'unspecified' order.

  v=hash_find      (c,s)          return value of s, or NULL if s not in c.
  ask()
  f

  f,e=hash_ask()   (c,s,a)        return slot where s SHOULD live.            i
  code()                      maintain collision stats in c.              i

  .=hash_code      (c,s)          compute hash-code for s,                    i
  from parameters of c.                       i

  */

/* Returned by hash_ask() to stop extra testing. hash_ask() wants to
   return both a slot and a status. This is the status.  TRUE: found
   symbol FALSE: absent: empty or deleted slot Also returned by
   hash_jam().  TRUE: we replaced a value FALSE: we inserted a value.  */
static char hash_found;

static struct hash_entry *hash_ask PARAMS ((struct hash_control *,
					    const char *, int));
static int hash_code PARAMS ((struct hash_control *, const char *));
static const char *hash_grow PARAMS ((struct hash_control *));

/* Create a new hash table.  Return NULL if failed; otherwise return handle
   (address of struct hash).  */
struct hash_control *
hash_new ()
{
  struct hash_control *retval;
  struct hash_entry *room;	/* points to hash table */
  struct hash_entry *wall;
  struct hash_entry *entry;
  int *ip;		/* scan stats block of struct hash_control */
  int *nd;		/* limit of stats block */

  room = (struct hash_entry *) xmalloc (sizeof (struct hash_entry)
					/* +1 for the wall entry */
					* ((1 << START_POWER) + 1));
  retval = (struct hash_control *) xmalloc (sizeof (struct hash_control));

  nd = retval->hash_stat + STATLENGTH;
  for (ip = retval->hash_stat; ip < nd; ip++)
    *ip = 0;

  retval->hash_stat[STAT_SIZE] = 1 << START_POWER;
  retval->hash_mask = (1 << START_POWER) - 1;
  retval->hash_sizelog = START_POWER;
  /* works for 1's compl ok */
  retval->hash_where = room;
  retval->hash_wall =
    wall = room + (1 << START_POWER);
  retval->hash_full = FULL_VALUE (1 << START_POWER);
  for (entry = room; entry <= wall; entry++)
    entry->hash_string = NULL;
  return retval;
}

/*
 *           h a s h _ d i e ( )
 *
 * Table should be empty, but this is not checked.
 * To empty the table, try hash_apply()ing a symbol deleter.
 * Return to free memory both the hash table and it's control
 * block.
 * 'handle' has no meaning after this function.
 * No errors are recoverable.
 */
void
hash_die (handle)
     struct hash_control *handle;
{
  free ((char *) handle->hash_where);
  free ((char *) handle);
}

#ifdef TEST
/*
 *           h a s h _ s a y ( )
 *
 * Return the size of the statistics table, and as many statistics as
 * we can until either (a) we have run out of statistics or (b) caller
 * has run out of buffer.
 * NOTE: hash_say treats all statistics alike.
 * These numbers may change with time, due to insertions, deletions
 * and expansions of the table.
 * The first "statistic" returned is the length of hash_stat[].
 * Then contents of hash_stat[] are read out (in ascending order)
 * until your buffer or hash_stat[] is exausted.
 */
static void
hash_say (handle, buffer, bufsiz)
     struct hash_control *handle;
     int buffer[ /*bufsiz*/ ];
     int bufsiz;
{
  int *nd;		/* limit of statistics block */
  int *ip;		/* scan statistics */

  ip = handle->hash_stat;
  nd = ip + min (bufsiz - 1, STATLENGTH);
  if (bufsiz > 0)		/* trust nothing! bufsiz<=0 is dangerous */
    {
      *buffer++ = STATLENGTH;
      for (; ip < nd; ip++, buffer++)
	{
	  *buffer = *ip;
	}
    }
}
#endif

/*
 *           h a s h _ d e l e t e ( )
 *
 * Try to delete a symbol from the table.
 * If it was there, return its value (and adjust STAT_USED).
 * Otherwise, return NULL.
 * Anyway, the symbol is not present after this function.
 *
 */
PTR				/* NULL if string not in table, else */
/* returns value of deleted symbol */
hash_delete (handle, string)
     struct hash_control *handle;
     const char *string;
{
  PTR retval;
  struct hash_entry *entry;

  entry = hash_ask (handle, string, STAT__WRITE);
  if (hash_found)
    {
      retval = entry->hash_value;
      entry->hash_string = DELETED;
      handle->hash_stat[STAT_USED] -= 1;
#ifdef SUSPECT
      if (handle->hash_stat[STAT_USED] < 0)
	{
	  error ("hash_delete");
	}
#endif /* def SUSPECT */
    }
  else
    {
      retval = NULL;
    }
  return (retval);
}

/*
 *                   h a s h _ r e p l a c e ( )
 *
 * Try to replace the old value of a symbol with a new value.
 * Normally return the old value.
 * Return NULL and don't change the table if the symbol is not already
 * in the table.
 */
PTR
hash_replace (handle, string, value)
     struct hash_control *handle;
     const char *string;
     PTR value;
{
  struct hash_entry *entry;
  char *retval;

  entry = hash_ask (handle, string, STAT__WRITE);
  if (hash_found)
    {
      retval = entry->hash_value;
      entry->hash_value = value;
    }
  else
    {
      retval = NULL;
    }
  ;
  return retval;
}

/*
 *                   h a s h _ i n s e r t ( )
 *
 * Insert a (symbol-string, value) into the hash table.
 * Return an error string, 0 means OK.
 * It is an 'error' to insert an existing symbol.
 */

const char *			/* return error string */
hash_insert (handle, string, value)
     struct hash_control *handle;
     const char *string;
     PTR value;
{
  struct hash_entry *entry;
  const char *retval;

  retval = 0;
  if (handle->hash_stat[STAT_USED] > handle->hash_full)
    {
      retval = hash_grow (handle);
    }
  if (!retval)
    {
      entry = hash_ask (handle, string, STAT__WRITE);
      if (hash_found)
	{
	  retval = "exists";
	}
      else
	{
	  entry->hash_value = value;
	  entry->hash_string = string;
	  handle->hash_stat[STAT_USED] += 1;
	}
    }
  return retval;
}

/*
 *               h a s h _ j a m ( )
 *
 * Regardless of what was in the symbol table before, after hash_jam()
 * the named symbol has the given value. The symbol is either inserted or
 * (its value is) replaced.
 * An error message string is returned, 0 means OK.
 *
 * WARNING: this may decide to grow the hashed symbol table.
 * To do this, we call hash_grow(), WHICH WILL recursively CALL US.
 *
 * We report status internally: hash_found is TRUE if we replaced, but
 * false if we inserted.
 */
const char *
hash_jam (handle, string, value)
     struct hash_control *handle;
     const char *string;
     PTR value;
{
  const char *retval;
  struct hash_entry *entry;

  retval = 0;
  if (handle->hash_stat[STAT_USED] > handle->hash_full)
    {
      retval = hash_grow (handle);
    }
  if (!retval)
    {
      entry = hash_ask (handle, string, STAT__WRITE);
      if (!hash_found)
	{
	  entry->hash_string = string;
	  handle->hash_stat[STAT_USED] += 1;
	}
      entry->hash_value = value;
    }
  return retval;
}

/*
 *               h a s h _ g r o w ( )
 *
 * Grow a new (bigger) hash table from the old one.
 * We choose to double the hash table's size.
 * Return a human-scrutible error string: 0 if OK.
 * Warning! This uses hash_jam(), which had better not recurse
 * back here! Hash_jam() conditionally calls us, but we ALWAYS
 * call hash_jam()!
 * Internal.
 */
static const char *
hash_grow (handle)		/* make a hash table grow */
     struct hash_control *handle;
{
  struct hash_entry *newwall;
  struct hash_entry *newwhere;
  struct hash_entry *newtrack;
  struct hash_entry *oldtrack;
  struct hash_entry *oldwhere;
  struct hash_entry *oldwall;
  int temp;
  int newsize;
  const char *string;
  const char *retval;
#ifdef SUSPECT
  int oldused;
#endif

  /*
   * capture info about old hash table
   */
  oldwhere = handle->hash_where;
  oldwall = handle->hash_wall;
#ifdef SUSPECT
  oldused = handle->hash_stat[STAT_USED];
#endif
  /*
   * attempt to get enough room for a hash table twice as big
   */
  temp = handle->hash_stat[STAT_SIZE];
  newwhere = ((struct hash_entry *)
	      xmalloc ((unsigned long) ((temp << (GROW_FACTOR + 1))
					/* +1 for wall slot */
					* sizeof (struct hash_entry))));
  if (newwhere == NULL)
    return "no_room";

  /*
   * have enough room: now we do all the work.
   * double the size of everything in handle.
   */
  handle->hash_mask = ((handle->hash_mask + 1) << GROW_FACTOR) - 1;
  handle->hash_stat[STAT_SIZE] <<= GROW_FACTOR;
  newsize = handle->hash_stat[STAT_SIZE];
  handle->hash_where = newwhere;
  handle->hash_full <<= GROW_FACTOR;
  handle->hash_sizelog += GROW_FACTOR;
  handle->hash_wall = newwall = newwhere + newsize;
  /* Set all those pesky new slots to vacant.  */
  for (newtrack = newwhere; newtrack <= newwall; newtrack++)
    newtrack->hash_string = NULL;
  /* We will do a scan of the old table, the hard way, using the
   * new control block to re-insert the data into new hash table.  */
  handle->hash_stat[STAT_USED] = 0;
  for (oldtrack = oldwhere; oldtrack < oldwall; oldtrack++)
    if (((string = oldtrack->hash_string) != NULL) && string != DELETED)
      if ((retval = hash_jam (handle, string, oldtrack->hash_value)))
	return retval;

#ifdef SUSPECT
  if (handle->hash_stat[STAT_USED] != oldused)
    return "hash_used";
#endif

  /* We have a completely faked up control block.
     Return the old hash table.  */
  free ((char *) oldwhere);

  return 0;
}

#ifdef TEST
/*
 *          h a s h _ a p p l y ( )
 *
 * Use this to scan each entry in symbol table.
 * For each symbol, this calls (applys) a nominated function supplying the
 * symbol's value (and the symbol's name).
 * The idea is you use this to destroy whatever is associted with
 * any values in the table BEFORE you destroy the table with hash_die.
 * Of course, you can use it for other jobs; whenever you need to
 * visit all extant symbols in the table.
 *
 * We choose to have a call-you-back idea for two reasons:
 *  asthetic: it is a neater idea to use apply than an explicit loop
 *  sensible: if we ever had to grow the symbol table (due to insertions)
 *            then we would lose our place in the table when we re-hashed
 *            symbols into the new table in a different order.
 *
 * The order symbols are visited depends entirely on the hashing function.
 * Whenever you insert a (symbol, value) you risk expanding the table. If
 * you do expand the table, then the hashing function WILL change, so you
 * MIGHT get a different order of symbols visited. In other words, if you
 * want the same order of visiting symbols as the last time you used
 * hash_apply() then you better not have done any hash_insert()s or
 * hash_jam()s since the last time you used hash_apply().
 *
 * In future we may use the value returned by your nominated function.
 * One idea is to abort the scan if, after applying the function to a
 * certain node, the function returns a certain code.
 *
 * The function you supply should be of the form:
 *      void myfunct(string,value)
 *              char * string;        |* the symbol's name *|
 *              char * value;         |* the symbol's value *|
 *      {
 *        |* ... *|
 *      }
 *
 */
void
hash_apply (handle, function)
     struct hash_control *handle;
     void (*function) ();
{
  struct hash_entry *entry;
  struct hash_entry *wall;

  wall = handle->hash_wall;
  for (entry = handle->hash_where; entry < wall; entry++)
    {
      if (islive (entry))	/* silly code: tests entry->string twice! */
	{
	  (*function) (entry->hash_string, entry->hash_value);
	}
    }
}
#endif

/*
 *          h a s h _ f i n d ( )
 *
 * Given symbol string, find value (if any).
 * Return found value or NULL.
 */
PTR
hash_find (handle, string)
     struct hash_control *handle;
     const char *string;
{
  struct hash_entry *entry;

  entry = hash_ask (handle, string, STAT__READ);
  if (hash_found)
    return entry->hash_value;
  else
    return NULL;
}

/*
 *          h a s h _ a s k ( )
 *
 * Searches for given symbol string.
 * Return the slot where it OUGHT to live. It may be there.
 * Return hash_found: TRUE only if symbol is in that slot.
 * Access argument is to help keep statistics in control block.
 * Internal.
 */
static struct hash_entry *	/* string slot, may be empty or deleted */
hash_ask (handle, string, access_type)
     struct hash_control *handle;
     const char *string;
     int access_type;
{
  const char *s;
  struct hash_entry *slot;
  int collision;	/* count collisions */
  int strcmps;
  int hcode;

  /* start looking here */
  hcode = hash_code (handle, string);
  slot = handle->hash_where + (hcode & handle->hash_mask);

  handle->hash_stat[STAT_ACCESS + access_type] += 1;
  collision = strcmps = 0;
  hash_found = FALSE;
  while (((s = slot->hash_string) != NULL) && s != DELETED)
    {
      if (string == s)
	{
	  hash_found = TRUE;
	  break;
	}
      if (slot->h == (unsigned long) hcode)
	{
	  if (!strcmp (string, s))
	    {
	      hash_found = TRUE;
	      break;
	    }
	  strcmps++;
	}
      collision++;
      slot++;
    }
  /*
   * slot:                                                      return:
   *       in use:     we found string                           slot
   *       at empty:
   *                   at wall:        we fell off: wrap round   ????
   *                   in table:       dig here                  slot
   *       at DELETED: dig here                                  slot
   */
  if (slot == handle->hash_wall)
    {
      slot = handle->hash_where;/* now look again */
      while (((s = slot->hash_string) != NULL) && s != DELETED)
	{
	  if (string == s)
	    {
	      hash_found = TRUE;
	      break;
	    }
	  if (slot->h == (unsigned long) hcode)
	    {
	      if (!strcmp (string, s))
		{
		  hash_found = TRUE;
		  break;
		}
	      strcmps++;
	    }
	  collision++;
	  slot++;
	}
      /*
       * slot:                                                   return:
       *       in use: we found it                                slot
       *       empty:  wall:         ERROR IMPOSSIBLE             !!!!
       *               in table:     dig here                     slot
       *       DELETED:dig here                                   slot
       */
    }
  handle->hash_stat[STAT_COLLIDE + access_type] += collision;
  handle->hash_stat[STAT_STRCMP + access_type] += strcmps;
  if (!hash_found)
    slot->h = hcode;
  return slot;			/* also return hash_found */
}

/*
 *           h a s h _ c o d e
 *
 * Does hashing of symbol string to hash number.
 * Internal.
 */
static int
hash_code (handle, string)
     struct hash_control *handle;
     const char *string;
{
#if 1 /* There seems to be some interesting property of this function
	 that prevents the bfd version below from being an adequate
	 substitute.  @@ Figure out what this property is!  */
  long h;		/* hash code built here */
  long c;		/* each character lands here */
  int n;		/* Amount to shift h by */

  n = (handle->hash_sizelog - 3);
  h = 0;
  while ((c = *string++) != 0)
    {
      h += c;
      h = (h << 3) + (h >> n) + c;
    }
  return h;
#else
  /* from bfd */
  unsigned long h = 0;
  unsigned int len = 0;
  unsigned int c;

  while ((c = *string++) != 0)
    {
      h += c + (c << 17);
      h ^= h >> 2;
      ++len;
    }
  h += len + (len << 17);
  h ^= h >> 2;
  return h;
#endif
}

void
hash_print_statistics (file, name, h)
     FILE *file;
     const char *name;
     struct hash_control *h;
{
  unsigned long sz, used, pct;

  if (h == 0)
    return;

  sz = h->hash_stat[STAT_SIZE];
  used = h->hash_stat[STAT_USED];
  pct = (used * 100 + sz / 2) / sz;

  fprintf (file, "%s hash statistics:\n\t%lu/%lu slots used (%lu%%)\n",
	   name, used, sz, pct);

#define P(name, off)							\
  fprintf (file, "\t%-16s %6dr + %6dw = %7d\n", name,			\
	   h->hash_stat[off+STAT__READ],				\
	   h->hash_stat[off+STAT__WRITE],				\
	   h->hash_stat[off+STAT__READ] + h->hash_stat[off+STAT__WRITE])

  P ("accesses:", STAT_ACCESS);
  P ("collisions:", STAT_COLLIDE);
  P ("string compares:", STAT_STRCMP);

#undef P
}

/*
 * Here is a test program to exercise above.
 */
#ifdef TEST

#define TABLES (6)		/* number of hash tables to maintain */
/* (at once) in any testing */
#define STATBUFSIZE (12)	/* we can have 12 statistics */

int statbuf[STATBUFSIZE];	/* display statistics here */
char answer[100];		/* human farts here */
char *hashtable[TABLES];	/* we test many hash tables at once */
char *h;			/* points to curent hash_control */
char **pp;
char *p;
char *name;
char *value;
int size;
int used;
char command;
int number;			/* number 0:TABLES-1 of current hashed */
/* symbol table */

main ()
{
  void applicatee ();
  void destroy ();
  char *what ();
  int *ip;

  number = 0;
  h = 0;
  printf ("type h <RETURN> for help\n");
  for (;;)
    {
      printf ("hash_test command: ");
      gets (answer);
      command = answer[0];
      if (isupper (command))
	command = tolower (command);	/* ecch! */
      switch (command)
	{
	case '#':
	  printf ("old hash table #=%d.\n", number);
	  whattable ();
	  break;
	case '?':
	  for (pp = hashtable; pp < hashtable + TABLES; pp++)
	    {
	      printf ("address of hash table #%d control block is %xx\n"
		      ,pp - hashtable, *pp);
	    }
	  break;
	case 'a':
	  hash_apply (h, applicatee);
	  break;
	case 'd':
	  hash_apply (h, destroy);
	  hash_die (h);
	  break;
	case 'f':
	  p = hash_find (h, name = what ("symbol"));
	  printf ("value of \"%s\" is \"%s\"\n", name, p ? p : "NOT-PRESENT");
	  break;
	case 'h':
	  printf ("# show old, select new default hash table number\n");
	  printf ("? display all hashtable control block addresses\n");
	  printf ("a apply a simple display-er to each symbol in table\n");
	  printf ("d die: destroy hashtable\n");
	  printf ("f find value of nominated symbol\n");
	  printf ("h this help\n");
	  printf ("i insert value into symbol\n");
	  printf ("j jam value into symbol\n");
	  printf ("n new hashtable\n");
	  printf ("r replace a value with another\n");
	  printf ("s say what %% of table is used\n");
	  printf ("q exit this program\n");
	  printf ("x delete a symbol from table, report its value\n");
	  break;
	case 'i':
	  p = hash_insert (h, name = what ("symbol"), value = what ("value"));
	  if (p)
	    {
	      printf ("symbol=\"%s\"  value=\"%s\"  error=%s\n", name, value,
		      p);
	    }
	  break;
	case 'j':
	  p = hash_jam (h, name = what ("symbol"), value = what ("value"));
	  if (p)
	    {
	      printf ("symbol=\"%s\"  value=\"%s\"  error=%s\n", name, value, p);
	    }
	  break;
	case 'n':
	  h = hashtable[number] = (char *) hash_new ();
	  break;
	case 'q':
	  exit (EXIT_SUCCESS);
	case 'r':
	  p = hash_replace (h, name = what ("symbol"), value = what ("value"));
	  printf ("old value was \"%s\"\n", p ? p : "{}");
	  break;
	case 's':
	  hash_say (h, statbuf, STATBUFSIZE);
	  for (ip = statbuf; ip < statbuf + STATBUFSIZE; ip++)
	    {
	      printf ("%d ", *ip);
	    }
	  printf ("\n");
	  break;
	case 'x':
	  p = hash_delete (h, name = what ("symbol"));
	  printf ("old value was \"%s\"\n", p ? p : "{}");
	  break;
	default:
	  printf ("I can't understand command \"%c\"\n", command);
	  break;
	}
    }
}

char *
what (description)
     char *description;
{
  char *retval;
  char *malloc ();

  printf ("   %s : ", description);
  gets (answer);
  /* will one day clean up answer here */
  retval = malloc (strlen (answer) + 1);
  if (!retval)
    {
      error ("room");
    }
  (void) strcpy (retval, answer);
  return (retval);
}

void
destroy (string, value)
     char *string;
     char *value;
{
  free (string);
  free (value);
}


void
applicatee (string, value)
     char *string;
     char *value;
{
  printf ("%.20s-%.20s\n", string, value);
}

whattable ()			/* determine number: what hash table to use */
     /* also determine h: points to hash_control */
{

  for (;;)
    {
      printf ("   what hash table (%d:%d) ?  ", 0, TABLES - 1);
      gets (answer);
      sscanf (answer, "%d", &number);
      if (number >= 0 && number < TABLES)
	{
	  h = hashtable[number];
	  if (!h)
	    {
	      printf ("warning: current hash-table-#%d. has no hash-control\n", number);
	    }
	  return;
	}
      else
	{
	  printf ("invalid hash table number: %d\n", number);
	}
    }
}



#endif /* #ifdef TEST */

/* end of hash.c */
