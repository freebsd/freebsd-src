/* Caching code.  Typically used by remote back ends for
   caching remote memory.

   Copyright 1992, 1993, 1995 Free Software Foundation, Inc.

   This file is part of GDB.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#include "defs.h"
#include "dcache.h"
#include "gdbcmd.h"
#include "gdb_string.h"


/* 
   The data cache could lead to incorrect results because it doesn't know
   about volatile variables, thus making it impossible to debug
   functions which use memory mapped I/O devices.

   set remotecache 0

   In those cases.

   In general the dcache speeds up performance, some speed improvement
   comes from the actual caching mechanism, but the major gain is in
   the reduction of the remote protocol overhead; instead of reading
   or writing a large area of memory in 4 byte requests, the cache
   bundles up the requests into 32 byte (actually LINE_SIZE) chunks.
   Reducing the overhead to an eighth of what it was.  This is very
   obvious when displaying a large amount of data,

   eg, x/200x 0 

   caching     |   no    yes 
   ---------------------------- 
   first time  |   4 sec  2 sec improvement due to chunking 
   second time |   4 sec  0 sec improvement due to caching

   The cache structure is unusual, we keep a number of cache blocks
   (DCACHE_SIZE) and each one caches a LINE_SIZEed area of memory.
   Within each line we remember the address of the line (always a
   multiple of the LINE_SIZE) and a vector of bytes over the range.
   There's another vector which contains the state of the bytes.

   ENTRY_BAD means that the byte is just plain wrong, and has no
   correspondence with anything else (as it would when the cache is
   turned on, but nothing has been done to it.

   ENTRY_DIRTY means that the byte has some data in it which should be
   written out to the remote target one day, but contains correct
   data.  ENTRY_OK means that the data is the same in the cache as it
   is in remote memory.


   The ENTRY_DIRTY state is necessary because GDB likes to write large
   lumps of memory in small bits.  If the caching mechanism didn't
   maintain the DIRTY information, then something like a two byte
   write would mean that the entire cache line would have to be read,
   the two bytes modified and then written out again.  The alternative
   would be to not read in the cache line in the first place, and just
   write the two bytes directly into target memory.  The trouble with
   that is that it really nails performance, because of the remote
   protocol overhead.  This way, all those little writes are bundled
   up into an entire cache line write in one go, without having to
   read the cache line in the first place.


  */


/* This value regulates the number of cache blocks stored.
   Smaller values reduce the time spent searching for a cache
   line, and reduce memory requirements, but increase the risk
   of a line not being in memory */

#define DCACHE_SIZE 64 

/* This value regulates the size of a cache line.  Smaller values
   reduce the time taken to read a single byte, but reduce overall
   throughput.  */

#define LINE_SIZE_POWER (5) 
#define LINE_SIZE (1 << LINE_SIZE_POWER)

/* Each cache block holds LINE_SIZE bytes of data
   starting at a multiple-of-LINE_SIZE address.  */

#define LINE_SIZE_MASK  ((LINE_SIZE - 1))	
#define XFORM(x) 	((x) & LINE_SIZE_MASK)
#define MASK(x)         ((x) & ~LINE_SIZE_MASK)


#define ENTRY_BAD   0  /* data at this byte is wrong */
#define ENTRY_DIRTY 1  /* data at this byte needs to be written back */
#define ENTRY_OK    2  /* data at this byte is same as in memory */


struct dcache_block
{
  struct dcache_block *p;	/* next in list */
  unsigned int addr;		/* Address for which data is recorded.  */
  char data[LINE_SIZE];		/* bytes at given address */
  unsigned char state[LINE_SIZE]; /* what state the data is in */

  /* whether anything in state is dirty - used to speed up the 
     dirty scan. */
  int anydirty;			

  int refs;
};


struct dcache_struct 
{
  /* Function to actually read the target memory. */
  memxferfunc read_memory;

  /* Function to actually write the target memory */
  memxferfunc write_memory;

  /* free list */
  struct dcache_block *free_head;
  struct dcache_block *free_tail;

  /* in use list */
  struct dcache_block *valid_head;
  struct dcache_block *valid_tail;

  /* The cache itself. */
  struct dcache_block *the_cache;

  /* potentially, if the cache was enabled, and then turned off, and
     then turned on again, the stuff in it could be stale, so this is
     used to mark it */
  int cache_has_stuff;
} ;

int remote_dcache = 0;

DCACHE *last_cache; /* Used by info dcache */



/* Free all the data cache blocks, thus discarding all cached data.  */

void
dcache_flush (dcache)
     DCACHE *dcache;
{
  int i;
  dcache->valid_head = 0;
  dcache->valid_tail = 0;

  dcache->free_head = 0;
  dcache->free_tail = 0;

  for (i = 0; i < DCACHE_SIZE; i++)
    {
      struct dcache_block *db = dcache->the_cache + i;

      if (!dcache->free_head)
	dcache->free_head = db;
      else
	dcache->free_tail->p = db;
      dcache->free_tail = db;
      db->p = 0;
    }

  dcache->cache_has_stuff = 0;

  return;
}

/* If addr is present in the dcache, return the address of the block
   containing it. */
static
struct dcache_block *
dcache_hit (dcache, addr)
     DCACHE *dcache;
     unsigned int addr;
{
  register struct dcache_block *db;

  /* Search all cache blocks for one that is at this address.  */
  db = dcache->valid_head;

  while (db)
    {
      if (MASK(addr) == db->addr)
	{
	  db->refs++;
	  return db;
	}
      db = db->p;
    }

  return NULL;
}

/* Make sure that anything in this line which needs to
   be written is. */

static int
dcache_write_line (dcache, db)
     DCACHE *dcache;
     register struct dcache_block *db;
{
  int s;
  int e;
  s = 0;
  if (db->anydirty)
    {
      for (s = 0; s < LINE_SIZE; s++)
	{
	  if (db->state[s] == ENTRY_DIRTY)
	    {
	      int len = 0;
	      for (e = s ; e < LINE_SIZE; e++, len++)
		if (db->state[e] != ENTRY_DIRTY)
		  break;
	      {
		/* all bytes from s..s+len-1 need to
		   be written out */
		int done = 0;
		while (done < len) {
		  int t = dcache->write_memory (db->addr + s + done,
						db->data + s + done,
						len - done);
		  if (t == 0)
		    return 0;
		  done += t;
		}
		memset (db->state + s, ENTRY_OK, len);
		s = e;
	      }
	    }
	}
      db->anydirty = 0;
    }
  return 1;
}


/* Get a free cache block, put or keep it on the valid list,
   and return its address.  The caller should store into the block
   the address and data that it describes, then remque it from the
   free list and insert it into the valid list.  This procedure
   prevents errors from creeping in if a memory retrieval is
   interrupted (which used to put garbage blocks in the valid
   list...).  */
static
struct dcache_block *
dcache_alloc (dcache)
     DCACHE *dcache;
{
  register struct dcache_block *db;

  if (remote_dcache == 0)
    abort ();

  /* Take something from the free list */
  db = dcache->free_head;
  if (db)
    {
      dcache->free_head = db->p;
    }
  else
    {
      /* Nothing left on free list, so grab one from the valid list */
      db = dcache->valid_head;
      dcache->valid_head = db->p;

      dcache_write_line (dcache, db);
    }

  /* append this line to end of valid list */
  if (!dcache->valid_head)
    dcache->valid_head = db;
  else
    dcache->valid_tail->p = db;
  dcache->valid_tail = db;
  db->p = 0;

  return db;
}

/* Using the data cache DCACHE return the contents of the byte at
   address ADDR in the remote machine.  

   Returns 0 on error. */

int
dcache_peek_byte (dcache, addr, ptr)
     DCACHE *dcache;
     CORE_ADDR addr;
     char *ptr;
{
  register struct dcache_block *db = dcache_hit (dcache, addr);
  int ok=1;
  int done = 0;
  if (db == 0
      || db->state[XFORM (addr)] == ENTRY_BAD)
    {
      if (db)
	{
	  dcache_write_line (dcache, db);
	}
    else
      db = dcache_alloc (dcache);
      immediate_quit++;
	    db->addr = MASK (addr);
      while (done < LINE_SIZE) 
	{
	  int try =
	    (*dcache->read_memory)
	      (db->addr + done,
	       db->data + done,
	       LINE_SIZE - done);
	  if (try == 0)
	    return 0;
	  done += try;
	}
      immediate_quit--;
     
      memset (db->state, ENTRY_OK, sizeof (db->data));
      db->anydirty = 0;
    }
  *ptr = db->data[XFORM (addr)];
  return ok;
}

/* Using the data cache DCACHE return the contents of the word at
   address ADDR in the remote machine.  

   Returns 0 on error. */

int
dcache_peek (dcache, addr, data)
     DCACHE *dcache;
     CORE_ADDR addr;
     int *data;
{
  char *dp = (char *) data;
  int i;
  for (i = 0; i < (int) sizeof (int); i++)
    {
      if (!dcache_peek_byte (dcache, addr + i, dp + i))
	return 0;
    }
  return 1;
}


/* Writeback any dirty lines to the remote. */
static int
dcache_writeback (dcache)
     DCACHE *dcache;
{
  struct dcache_block *db;

  db = dcache->valid_head;

  while (db)
    {
      if (!dcache_write_line (dcache, db))
	return 0;
      db = db->p;
    }
  return 1;
}


/* Using the data cache DCACHE return the contents of the word at
   address ADDR in the remote machine.  */
int
dcache_fetch (dcache, addr)
     DCACHE *dcache;
     CORE_ADDR addr;
{
  int res;
  dcache_peek (dcache, addr, &res);
  return res;
}


/* Write the byte at PTR into ADDR in the data cache.
   Return zero on write error.
 */

int
dcache_poke_byte (dcache, addr, ptr)
     DCACHE *dcache;
     CORE_ADDR addr;
     char *ptr;
{
  register struct dcache_block *db = dcache_hit (dcache, addr);

  if (!db)
    {
      db = dcache_alloc (dcache);
      db->addr = MASK (addr);
      memset (db->state, ENTRY_BAD, sizeof (db->data));
    }

  db->data[XFORM (addr)] = *ptr;
  db->state[XFORM (addr)] = ENTRY_DIRTY;
  db->anydirty = 1;
  return 1;
}

/* Write the word at ADDR both in the data cache and in the remote machine.  
   Return zero on write error.
 */

int
dcache_poke (dcache, addr, data)
     DCACHE *dcache;
     CORE_ADDR addr;
     int data;
{
  char *dp = (char *) (&data);
  int i;
  for (i = 0; i < (int) sizeof (int); i++)
    {
      if (!dcache_poke_byte (dcache, addr + i, dp + i))
	return 0;
    }
  dcache_writeback (dcache);
  return 1;
}


/* Initialize the data cache.  */
DCACHE *
dcache_init (reading, writing)
     memxferfunc reading;
     memxferfunc writing;
{
  int csize = sizeof (struct dcache_block) * DCACHE_SIZE;
  DCACHE *dcache;

  dcache = (DCACHE *) xmalloc (sizeof (*dcache));
  dcache->read_memory = reading;
  dcache->write_memory = writing;

  dcache->the_cache = (struct dcache_block *) xmalloc (csize);
  memset (dcache->the_cache, 0, csize);

  dcache_flush (dcache);

  last_cache = dcache;
  return dcache;
}

/* Read or write LEN bytes from inferior memory at MEMADDR, transferring
   to or from debugger address MYADDR.  Write to inferior if SHOULD_WRITE is
   nonzero. 

   Returns length of data written or read; 0 for error.  

   This routine is indended to be called by remote_xfer_ functions. */

int
dcache_xfer_memory (dcache, memaddr, myaddr, len, should_write)
     DCACHE *dcache;
     CORE_ADDR memaddr;
     char *myaddr;
     int len;
     int should_write;
{
  int i;

  if (remote_dcache) 
    {
      int (*xfunc) ()
	= should_write ? dcache_poke_byte : dcache_peek_byte;

      for (i = 0; i < len; i++)
	{
	  if (!xfunc (dcache, memaddr + i, myaddr + i))
	    return 0;
	}
      dcache->cache_has_stuff = 1;
      dcache_writeback (dcache);
    }
  else 
    {
      int (*xfunc) () 
	= should_write ? dcache->write_memory : dcache->read_memory;

      if (dcache->cache_has_stuff)
	dcache_flush (dcache);

      len = xfunc (memaddr, myaddr, len);
    }
  return len;
}

static void 
dcache_info (exp, tty)
     char *exp;
     int tty;
{
  struct dcache_block *p;

  if (!remote_dcache)
    {
      printf_filtered ("Dcache not enabled\n");
      return;
    }
  printf_filtered ("Dcache enabled, line width %d, depth %d\n",
		   LINE_SIZE, DCACHE_SIZE);

  printf_filtered ("Cache state:\n");

  for (p = last_cache->valid_head; p; p = p->p)
    {
      int j;
      printf_filtered ("Line at %08xd, referenced %d times\n",
		       p->addr, p->refs);

      for (j = 0; j < LINE_SIZE; j++)
	printf_filtered ("%02x", p->data[j] & 0xFF);
      printf_filtered ("\n");

      for (j = 0; j < LINE_SIZE; j++)
	printf_filtered (" %2x", p->state[j]);
      printf_filtered ("\n");
    }
}

void
_initialize_dcache ()
{
  add_show_from_set
    (add_set_cmd ("remotecache", class_support, var_boolean,
		  (char *) &remote_dcache,
		  "\
Set cache use for remote targets.\n\
When on, use data caching for remote targets.  For many remote targets\n\
this option can offer better throughput for reading target memory.\n\
Unfortunately, gdb does not currently know anything about volatile\n\
registers and thus data caching will produce incorrect results with\n\
volatile registers are in use.  By default, this option is on.",
		  &setlist),
     &showlist);

  add_info ("dcache", dcache_info,
	    "Print information on the dcache performance.");

}
