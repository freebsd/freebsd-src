/* Caching code.  Typically used by remote back ends for
   caching remote memory.

   Copyright 1992, 1993 Free Software Foundation, Inc.

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
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

#include "defs.h"
#include "dcache.h"
#include "gdbcmd.h"

extern int insque();
extern int remque();

int remote_dcache = 0;

/* The data cache records all the data read from the remote machine
   since the last time it stopped.

   Each cache block holds LINE_SIZE bytes of data
   starting at a multiple-of-LINE_SIZE address.  */

#define LINE_SIZE_MASK ((LINE_SIZE - 1))	/* eg 7*2+1= 111*/
#define XFORM(x)  (((x) & LINE_SIZE_MASK) >> 2)

/* Free all the data cache blocks, thus discarding all cached data.  */
void
dcache_flush (dcache)
     DCACHE *dcache;
{
  register struct dcache_block *db;

  if (remote_dcache > 0)
    while ((db = dcache->dcache_valid.next) != &dcache->dcache_valid)
      {
	remque (db);
	insque (db, &dcache->dcache_free);
      }

  return;
}

/*
 * If addr is present in the dcache, return the address of the block
 * containing it.
 */
static
struct dcache_block *
dcache_hit (dcache, addr)
     DCACHE *dcache;
     unsigned int addr;
{
  register struct dcache_block *db;

  if (addr & 3
      || remote_dcache == 0)
    abort ();

  /* Search all cache blocks for one that is at this address.  */
  db = dcache->dcache_valid.next;
  while (db != &dcache->dcache_valid)
    {
      if ((addr & ~LINE_SIZE_MASK) == db->addr)
	return db;
      db = db->next;
    }

  return NULL;
}

/*  Return the int data at address ADDR in dcache block DC.  */
static
int
dcache_value (db, addr)
     struct dcache_block *db;
     unsigned int addr;
{
  if (addr & 3
      || remote_dcache == 0)
    abort ();
  return (db->data[XFORM (addr)]);
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
    abort();

  if ((db = dcache->dcache_free.next) == &dcache->dcache_free)
    {
      /* If we can't get one from the free list, take last valid and put
	 it on the free list.  */
      db = dcache->dcache_valid.last;
      remque (db);
      insque (db, &dcache->dcache_free);
    }

  remque (db);
  insque (db, &dcache->dcache_valid);
  return (db);
}

/* Using the data cache DCACHE return the contents of the word at
   address ADDR in the remote machine.  */
int
dcache_fetch (dcache, addr)
     DCACHE *dcache;
     CORE_ADDR addr;
{
  register struct dcache_block *db;

  if (remote_dcache == 0)
    {
      int i;

      (*dcache->read_memory) (addr, (unsigned char *) &i, 4);
      return(i);
    }

  db = dcache_hit (dcache, addr);
  if (db == 0)
    {
      db = dcache_alloc (dcache);
      immediate_quit++;
      (*dcache->read_memory) (addr & ~LINE_SIZE_MASK, (unsigned char *) db->data, LINE_SIZE);
      immediate_quit--;
      db->addr = addr & ~LINE_SIZE_MASK;
      remque (db);		/* Off the free list */
      insque (db, &dcache->dcache_valid);	/* On the valid list */
    }
  return (dcache_value (db, addr));
}

/* Write the word at ADDR both in the data cache and in the remote machine.  */
void
dcache_poke (dcache, addr, data)
     DCACHE *dcache;
     CORE_ADDR addr;
     int data;
{
  register struct dcache_block *db;

  if (remote_dcache == 0)
    {
      (*dcache->write_memory) (addr, (unsigned char *) &data, 4);
      return;
    }

  /* First make sure the word is IN the cache.  DB is its cache block.  */
  db = dcache_hit (dcache, addr);
  if (db == 0)
    {
      db = dcache_alloc (dcache);
      immediate_quit++;
      (*dcache->write_memory) (addr & ~LINE_SIZE_MASK, (unsigned char *) db->data, LINE_SIZE);
      immediate_quit--;
      db->addr = addr & ~LINE_SIZE_MASK;
      remque (db); /* Off the free list */
      insque (db, &dcache->dcache_valid); /* On the valid list */
    }

  /* Modify the word in the cache.  */
  db->data[XFORM (addr)] = data;

  /* Send the changed word.  */
  immediate_quit++;
  (*dcache->write_memory) (addr, (unsigned char *) &data, 4);
  immediate_quit--;
}

/* Initialize the data cache.  */
DCACHE *
dcache_init (reading, writing)
     memxferfunc reading;
     memxferfunc writing;
{
  register i;
  register struct dcache_block *db;
  DCACHE *dcache;

  dcache = (DCACHE *) xmalloc (sizeof (*dcache));
  dcache->read_memory = reading;
  dcache->write_memory = writing;
  dcache->the_cache = (struct dcache_block *)
    xmalloc (sizeof (*dcache->the_cache) * DCACHE_SIZE);

  dcache->dcache_free.next = dcache->dcache_free.last = &dcache->dcache_free;
  dcache->dcache_valid.next = dcache->dcache_valid.last = &dcache->dcache_valid;
  for (db = dcache->the_cache, i = 0; i < DCACHE_SIZE; i++, db++)
    insque (db, &dcache->dcache_free);

  return(dcache);
}

void
_initialitize_dcache ()
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
volatile registers are in use.  By default, this option is off.", 
		  &setlist),
     &showlist);
}
