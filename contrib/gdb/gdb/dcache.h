/* Declarations for caching.  Typically used by remote back ends for
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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#ifndef DCACHE_H
#define DCACHE_H

typedef int (*memxferfunc) PARAMS((CORE_ADDR memaddr,
			     char *myaddr,
			     int len));

typedef struct dcache_struct DCACHE;

/* Using the data cache DCACHE return the contents of the word at
   address ADDR in the remote machine.  */
int dcache_fetch PARAMS((DCACHE *dcache, CORE_ADDR addr));

/* Flush DCACHE. */
void dcache_flush PARAMS((DCACHE *dcache));

/* Initialize DCACHE. */
DCACHE *dcache_init PARAMS((memxferfunc reading, memxferfunc writing));

/* Write the word at ADDR both in the data cache and in the remote machine.  */
int dcache_poke PARAMS((DCACHE *dcache, CORE_ADDR addr, int data));

/* Simple to call from <remote>_xfer_memory */

int dcache_xfer_memory PARAMS((DCACHE *cache, CORE_ADDR mem, char *my, int len, int should_write));

/* Write the bytes at ADDR into the data cache and the remote machine. */
int dcache_poke_block PARAMS((DCACHE *cache, CORE_ADDR mem, char* my, int len));
#endif /* DCACHE_H */
