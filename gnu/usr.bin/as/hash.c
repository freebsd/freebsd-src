/* hash.c - hash table lookup strings -
   Copyright (C) 1987, 1990, 1991, 1992 Free Software Foundation, Inc.
   
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
   the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

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
 *  Before you do anything else, you must call hash_new() which will
 *  return the address of a hash-table-control-block (or NULL if there
 *  is not enough memory). You then use this address as a handle of the
 *  symbol table by passing it to all the other hash_...() functions.
 *  The only approved way to recover the memory used by the symbol table
 *  is to call hash_die() with the handle of the symbol table.
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
 *  internals.
 */

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

#ifndef lint
static char rcsid[] = "$Id: hash.c,v 1.2 1993/11/03 00:51:44 paul Exp $";
#endif

#include <stdio.h>

#ifndef FALSE
#define FALSE	(0)
#define TRUE	(!FALSE)
#endif /* no FALSE yet */

#include <ctype.h>
#define min(a, b)	((a) < (b) ? (a) : (b))

#include "as.h"

#define error	as_fatal

#define DELETED     ((char *)1)	/* guarenteed invalid address */
#define START_POWER    (11)	/* power of two: size of new hash table *//* JF was 6 */
/* JF These next two aren't used any more. */
/* #define START_SIZE    (64)	/ * 2 ** START_POWER */
/* #define START_FULL    (32)      / * number of entries before table expands */
#define islive(ptr) (ptr->hash_string && ptr->hash_string != DELETED)
/* above TRUE if a symbol is in entry @ ptr */

#define STAT_SIZE      (0)      /* number of slots in hash table */
/* the wall does not count here */
/* we expect this is always a power of 2 */
#define STAT_ACCESS    (1)	/* number of hash_ask()s */
#define STAT__READ     (0)      /* reading */
#define STAT__WRITE    (1)      /* writing */
#define STAT_COLLIDE   (3)	/* number of collisions (total) */
/* this may exceed STAT_ACCESS if we have */
/* lots of collisions/access */
#define STAT_USED      (5)	/* slots used right now */
#define STATLENGTH     (6)	/* size of statistics block */
#if STATLENGTH != HASH_STATLENGTH
Panic! Please make #include "stat.h" agree with previous definitions!
#endif
    
    /* #define SUSPECT to do runtime checks */
    /* #define TEST to be a test jig for hash...() */
    
#ifdef TEST			/* TEST: use smaller hash table */
#undef  START_POWER
#define START_POWER (3)
#undef  START_SIZE
#define START_SIZE  (8)
#undef  START_FULL
#define START_FULL  (4)
#endif

/*------------------ plan ---------------------------------- i = internal
  
  struct hash_control * c;
  struct hash_entry   * e;                                                    i
  int                   b[z];     buffer for statistics
  z         size of b
  char                * s;        symbol string (address) [ key ]
  char                * v;        value string (address)  [datum]
  boolean               f;        TRUE if we found s in hash table            i
  char                * t;        error string; "" means OK
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

static char hash_found;		/* returned by hash_ask() to stop extra */
/* testing. hash_ask() wants to return both */
/* a slot and a status. This is the status. */
/* TRUE: found symbol */
/* FALSE: absent: empty or deleted slot */
/* Also returned by hash_jam(). */
/* TRUE: we replaced a value */
/* FALSE: we inserted a value */

static struct hash_entry * hash_ask();
static int hash_code ();
static char * hash_grow();

/*
 *             h a s h _ n e w ( )
 *
 */
struct hash_control *
    hash_new()			/* create a new hash table */
/* return NULL if failed */
/* return handle (address of struct hash) */
{
	register struct hash_control * retval;
	register struct hash_entry *   room;	/* points to hash table */
	register struct hash_entry *   wall;
	register struct hash_entry *   entry;
	register int *                 ip;	/* scan stats block of struct hash_control */
	register int *                 nd;	/* limit of stats block */
	
	if (( room = (struct hash_entry *) malloc( sizeof(struct
							  hash_entry)*((1<<START_POWER) + 1) ) ) != NULL)
	    /* +1 for the wall entry */
	    {
		    if (( retval = (struct hash_control *) malloc(sizeof(struct
									 hash_control)) ) != NULL)
			{
				nd = retval->hash_stat + STATLENGTH;
				for (ip=retval->hash_stat; ip<nd; ip++)
				    {
					    *ip = 0;
				    }
				
				retval->hash_stat[STAT_SIZE]  = 1<<START_POWER;
				retval->hash_mask             = (1<<START_POWER) - 1;
				retval->hash_sizelog	  = START_POWER;
				/* works for 1's compl ok */
				retval->hash_where            = room;
				retval->hash_wall             =
				    wall                          = room + (1<<START_POWER);
				retval->hash_full             = (1<<START_POWER)/2;
				for (entry=room; entry <= wall; entry++)
				    {
					    entry->hash_string = NULL;
				    }
			}
	    }
	else
	    {
		    retval = NULL;		/* no room for table: fake a failure */
	    }
	return(retval);		/* return NULL or set-up structs */
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
    hash_die(handle)
struct hash_control * handle;
{
	free((char *)handle->hash_where);
	free((char *)handle);
}

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
void
    hash_say(handle,buffer,bufsiz)
register struct hash_control * handle;
register int                   buffer[/*bufsiz*/];
register int                   bufsiz;
{
	register int * nd;			/* limit of statistics block */
	register int * ip;			/* scan statistics */
	
	ip = handle->hash_stat;
	nd = ip + min(bufsiz-1,STATLENGTH);
	if (bufsiz>0)			/* trust nothing! bufsiz <= 0 is dangerous */
	    {
		    *buffer++ = STATLENGTH;
		    for (; ip<nd; ip++,buffer++)
			{
				*buffer = *ip;
			}
	    }
}

/*
 *           h a s h _ d e l e t e ( )
 *
 * Try to delete a symbol from the table.
 * If it was there, return its value (and adjust STAT_USED).
 * Otherwise, return NULL.
 * Anyway, the symbol is not present after this function.
 *
 */
char *				/* NULL if string not in table, else */
    /* returns value of deleted symbol */
    hash_delete(handle,string)
register struct hash_control * handle;
register char *                string;
{
	register char *                   retval; /* NULL if string not in table */
	register struct hash_entry *      entry; /* NULL or entry of this symbol */
	
	entry = hash_ask(handle,string,STAT__WRITE);
	if (hash_found)
	    {
		    retval = entry->hash_value;
		    entry->hash_string = DELETED; /* mark as deleted */
		    handle->hash_stat[STAT_USED] -= 1; /* slots-in-use count */
#ifdef SUSPECT
		    if (handle->hash_stat[STAT_USED]<0)
			{
				error("hash_delete");
			}
#endif /* def SUSPECT */
	    }
	else
	    {
		    retval = NULL;
	    }
	return(retval);
}

/*
 *                   h a s h _ r e p l a c e ( )
 *
 * Try to replace the old value of a symbol with a new value.
 * Normally return the old value.
 * Return NULL and don't change the table if the symbol is not already
 * in the table.
 */
char *
    hash_replace(handle,string,value)
register struct hash_control * handle;
register char *                string;
register char *                value;
{
	register struct hash_entry *      entry;
	register char *                   retval;
	
	entry = hash_ask(handle,string,STAT__WRITE);
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
	return (retval);
}

/*
 *                   h a s h _ i n s e r t ( )
 *
 * Insert a (symbol-string, value) into the hash table.
 * Return an error string, "" means OK.
 * It is an 'error' to insert an existing symbol.
 */

char *				/* return error string */
    hash_insert(handle,string,value)
register struct hash_control * handle;
register char *                string;
register char *                value;
{
	register struct hash_entry * entry;
	register char *              retval;
	
	retval = "";
	if (handle->hash_stat[STAT_USED] > handle->hash_full)
	    {
		    retval = hash_grow(handle);
	    }
	if ( ! * retval)
	    {
		    entry = hash_ask(handle,string,STAT__WRITE);
		    if (hash_found)
			{
				retval = "exists";
			}
		    else
			{
				entry->hash_value  = value;
				entry->hash_string = string;
				handle->hash_stat[STAT_USED]  += 1;
			}
	    }
	return(retval);
}

/*
 *               h a s h _ j a m ( )
 *
 * Regardless of what was in the symbol table before, after hash_jam()
 * the named symbol has the given value. The symbol is either inserted or
 * (its value is) relpaced.
 * An error message string is returned, "" means OK.
 *
 * WARNING: this may decide to grow the hashed symbol table.
 * To do this, we call hash_grow(), WHICH WILL recursively CALL US.
 *
 * We report status internally: hash_found is TRUE if we replaced, but
 * false if we inserted.
 */
char *
    hash_jam(handle,string,value)
register struct hash_control * handle;
register char *                string;
register char *                value;
{
	register char *                   retval;
	register struct hash_entry *      entry;
	
	retval = "";
	if (handle->hash_stat[STAT_USED] > handle->hash_full)
	    {
		    retval = hash_grow(handle);
	    }
	if (! * retval)
	    {
		    entry = hash_ask(handle,string,STAT__WRITE);
		    if ( ! hash_found)
			{
				entry->hash_string = string;
				handle->hash_stat[STAT_USED] += 1;
			}
		    entry->hash_value = value;
	    }
	return(retval);
}

/*
 *               h a s h _ g r o w ( )
 *
 * Grow a new (bigger) hash table from the old one.
 * We choose to double the hash table's size.
 * Return a human-scrutible error string: "" if OK.
 * Warning! This uses hash_jam(), which had better not recurse
 * back here! Hash_jam() conditionally calls us, but we ALWAYS
 * call hash_jam()!
 * Internal.
 */
static char *
    hash_grow(handle)			/* make a hash table grow */
struct hash_control * handle;
{
	register struct hash_entry *      newwall;
	register struct hash_entry *      newwhere;
	struct hash_entry *      newtrack;
	register struct hash_entry *      oldtrack;
	register struct hash_entry *      oldwhere;
	register struct hash_entry *      oldwall;
	register int                      temp;
	int                      newsize;
	char *                   string;
	char *                   retval;
#ifdef SUSPECT
	int                      oldused;
#endif
	
	/*
	 * capture info about old hash table
	 */
	oldwhere = handle->hash_where;
	oldwall  = handle->hash_wall;
#ifdef SUSPECT
	oldused  = handle->hash_stat[STAT_USED];
#endif
	/*
	 * attempt to get enough room for a hash table twice as big
	 */
	temp = handle->hash_stat[STAT_SIZE];
	if (( newwhere = (struct hash_entry *)
	     xmalloc((long)((temp+temp+1)*sizeof(struct hash_entry)))) != NULL)
	    /* +1 for wall slot */
	    {
		    retval = "";		/* assume success until proven otherwise */
		    /*
		     * have enough room: now we do all the work.
		     * double the size of everything in handle,
		     * note: hash_mask frob works for 1's & for 2's complement machines
		     */
		    handle->hash_mask              = handle->hash_mask + handle->hash_mask + 1;
		    handle->hash_stat[STAT_SIZE] <<= 1;
		    newsize                        = handle->hash_stat[STAT_SIZE];
		    handle->hash_where             = newwhere;
		    handle->hash_full            <<= 1;
		    handle->hash_sizelog	    += 1;
		    handle->hash_stat[STAT_USED]   = 0;
		    handle->hash_wall              =
			newwall                        = newwhere + newsize;
		    /*
		     * set all those pesky new slots to vacant.
		     */
		    for (newtrack=newwhere; newtrack <= newwall; newtrack++)
			{
				newtrack->hash_string = NULL;
			}
		    /*
		     * we will do a scan of the old table, the hard way, using the
		     * new control block to re-insert the data into new hash table.
		     */
		    handle->hash_stat[STAT_USED] = 0;	/* inserts will bump it up to correct */
		    for (oldtrack=oldwhere; oldtrack < oldwall; oldtrack++)
			{
				if (((string = oldtrack->hash_string) != NULL) && string != DELETED)
				    {
					    if ( * (retval = hash_jam(handle,string,oldtrack->hash_value) ) )
						{
							break;
						}
				    }
			}
#ifdef SUSPECT
		    if ( !*retval && handle->hash_stat[STAT_USED] != oldused)
			{
				retval = "hash_used";
			}
#endif
		    if (!*retval)
			{
				/*
				 * we have a completely faked up control block.
				 * return the old hash table.
				 */
				free((char *)oldwhere);
				/*
				 * Here with success. retval is already "".
				 */
			}
	    }
	else
	    {
		    retval = "no room";
	    }
	return(retval);
}

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
 * To be safe, please make your functions of type char *. If you always
 * return NULL, then the scan will complete, visiting every symbol in
 * the table exactly once. ALL OTHER RETURNED VALUES have no meaning yet!
 * Caveat Actor!
 *
 * The function you supply should be of the form:
 *      char * myfunct(string,value)
 *              char * string;        |* the symbol's name *|
 *              char * value;         |* the symbol's value *|
 *      {
 *        |* ... *|
 *        return(NULL);
 *      }
 *
 * The returned value of hash_apply() is (char*)NULL. In future it may return
 * other values. NULL means "completed scan OK". Other values have no meaning
 * yet. (The function has no graceful failures.)
 */
char *
    hash_apply(handle,function)
struct hash_control * handle;
char*                 (*function)();
{
	register struct hash_entry *      entry;
	register struct hash_entry *      wall;
	
	wall = handle->hash_wall;
	for (entry = handle->hash_where; entry < wall; entry++)
	    {
		    if (islive(entry))	/* silly code: tests entry->string twice! */
			{
				(*function)(entry->hash_string,entry->hash_value);
			}
	    }
	return (NULL);
}

/*
 *          h a s h _ f i n d ( )
 *
 * Given symbol string, find value (if any).
 * Return found value or NULL.
 */
char *
    hash_find(handle,string)	/* return char* or NULL */
struct hash_control * handle;
char *                string;
{
	register struct hash_entry *      entry;
	register char *                   retval;
	
	entry = hash_ask(handle,string,STAT__READ);
	if (hash_found)
	    {
		    retval = entry->hash_value;
	    }
	else
	    {
		    retval = NULL;
	    }
	return(retval);
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
    hash_ask(handle,string,access)
struct hash_control * handle;
char *                string;
int                   access; /* access type */
{
	register char	*string1;	/* JF avoid strcmp calls */
	register char *                   s;
	register int                      c;
	register struct hash_entry *      slot;
	register int                      collision; /* count collisions */
	
	slot = handle->hash_where + hash_code(handle,string); /* start looking here */
	handle->hash_stat[STAT_ACCESS+access] += 1;
	collision = 0;
	hash_found = FALSE;
	while (((s = slot->hash_string) != NULL) && s != DELETED)
	    {
		    for (string1=string;;) {
			    if ((c= *s++) == 0) {
				    if (!*string1)
					hash_found = TRUE;
				    break;
			    }
			    if (*string1++ != c)
				break;
		    }
		    if (hash_found)
			break;
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
		    slot = handle->hash_where; /* now look again */
		    while (((s = slot->hash_string) != NULL) && s != DELETED)
			{
				for (string1=string;*s;string1++,s++) {
					if (*string1 != *s)
					    break;
				}
				if (*s == *string1) {
					hash_found = TRUE;
					break;
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
	/*   fprintf(stderr,"hash_ask(%s)->%d(%d)\n",string,hash_code(handle,string),collision); */
	handle->hash_stat[STAT_COLLIDE+access] += collision;
	return(slot);			/* also return hash_found */
}

/*
 *           h a s h _ c o d e
 *
 * Does hashing of symbol string to hash number.
 * Internal.
 */
static int
    hash_code(handle,string)
struct hash_control * handle;
register char *                string;
{
	register long                 h;      /* hash code built here */
	register long                 c;      /* each character lands here */
	register int			   n;      /* Amount to shift h by */
	
	n = (handle->hash_sizelog - 3);
	h = 0;
	while ((c = *string++) != 0)
	    {
		    h += c;
		    h = (h<<3) + (h>>n) + c;
	    }
	return (h & handle->hash_mask);
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
char * hashtable[TABLES];	/* we test many hash tables at once */
char * h;			/* points to curent hash_control */
char ** pp;
char *  p;
char *  name;
char *  value;
int     size;
int     used;
char    command;
int     number;			/* number 0:TABLES-1 of current hashed */
/* symbol table */

main()
{
	char (*applicatee());
	char * hash_find();
	char * destroy();
	char * what();
	struct hash_control * hash_new();
	char * hash_replace();
	int *  ip;
	
	number = 0;
	h = 0;
	printf("type h <RETURN> for help\n");
	for (;;)
	    {
		    printf("hash_test command: ");
		    gets(answer);
		    command = answer[0];
		    if (isupper(command)) command = tolower(command);	/* ecch! */
		    switch (command)
			{
			case '#':
				printf("old hash table #=%d.\n",number);
				whattable();
				break;
			case '?':
				for (pp=hashtable; pp<hashtable+TABLES; pp++)
				    {
					    printf("address of hash table #%d control block is %xx\n"
						   ,pp-hashtable,*pp);
				    }
				break;
			case 'a':
				hash_apply(h,applicatee);
				break;
			case 'd':
				hash_apply(h,destroy);
				hash_die(h);
				break;
			case 'f':
				p = hash_find(h,name=what("symbol"));
				printf("value of \"%s\" is \"%s\"\n",name,p?p:"NOT-PRESENT");
				break;
			case 'h':
				printf("# show old, select new default hash table number\n");
				printf("? display all hashtable control block addresses\n");
				printf("a apply a simple display-er to each symbol in table\n");
				printf("d die: destroy hashtable\n");
				printf("f find value of nominated symbol\n");
				printf("h this help\n");
				printf("i insert value into symbol\n");
				printf("j jam value into symbol\n");
				printf("n new hashtable\n");
				printf("r replace a value with another\n");
				printf("s say what %% of table is used\n");
				printf("q exit this program\n");
				printf("x delete a symbol from table, report its value\n");
				break;
			case 'i':
				p = hash_insert(h,name=what("symbol"),value=what("value"));
				if (*p)
				    {
					    printf("symbol=\"%s\"  value=\"%s\"  error=%s\n",name,value,p);
				    }
				break;
			case 'j':
				p = hash_jam(h,name=what("symbol"),value=what("value"));
				if (*p)
				    {
					    printf("symbol=\"%s\"  value=\"%s\"  error=%s\n",name,value,p);
				    }
				break;
			case 'n':
				h = hashtable[number] = (char *) hash_new();
				break;
			case 'q':
				exit();
			case 'r':
				p = hash_replace(h,name=what("symbol"),value=what("value"));
				printf("old value was \"%s\"\n",p?p:"{}");
				break;
			case 's':
				hash_say(h,statbuf,STATBUFSIZE);
				for (ip=statbuf; ip<statbuf+STATBUFSIZE; ip++)
				    {
					    printf("%d ",*ip);
				    }
				printf("\n");
				break;
			case 'x':
				p = hash_delete(h,name=what("symbol"));
				printf("old value was \"%s\"\n",p?p:"{}");
				break;
			default:
				printf("I can't understand command \"%c\"\n",command);
				break;
			}
	    }
}

char *
    what(description)
char * description;
{
	char * retval;
	char * malloc();
	
	printf("   %s : ",description);
	gets(answer);
	/* will one day clean up answer here */
	retval = malloc(strlen(answer)+1);
	if (!retval)
	    {
		    error("room");
	    }
	(void)strcpy(retval,answer);
	return(retval);
}

char *
    destroy(string,value)
char * string;
char * value;
{
	free(string);
	free(value);
	return(NULL);
}


char *
    applicatee(string,value)
char * string;
char * value;
{
	printf("%.20s-%.20s\n",string,value);
	return(NULL);
}

whattable()			/* determine number: what hash table to use */
/* also determine h: points to hash_control */
{
	
	for (;;)
	    {
		    printf("   what hash table (%d:%d) ?  ",0,TABLES-1);
		    gets(answer);
		    sscanf(answer,"%d",&number);
		    if (number >= 0 && number<TABLES)
			{
				h = hashtable[number];
				if (!h)
				    {
					    printf("warning: current hash-table-#%d. has no hash-control\n",number);
				    }
				return;
			}
		    else
			{
				printf("invalid hash table number: %d\n",number);
			}
	    }
}



#endif /* #ifdef TEST */

/* end of hash.c */
