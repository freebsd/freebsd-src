/* Symbol table manager for Bison,
   Copyright (C) 1984, 1989 Free Software Foundation, Inc.

This file is part of Bison, the GNU Compiler Compiler.

Bison is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

Bison is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Bison; see the file COPYING.  If not, write to
the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.  */


#include <stdio.h>
#include "system.h"
#include "alloc.h"
#include "symtab.h"
#include "gram.h"


bucket **symtab;
bucket *firstsymbol;
bucket *lastsymbol;

void tabinit PARAMS((void));
void free_symtab PARAMS((void));


static int
hash (char *key)
{
  register char *cp;
  register int k;

  cp = key;
  k = 0;
  while (*cp)
    k = ((k << 1) ^ (*cp++)) & 0x3fff;

  return (k % TABSIZE);
}



static char *
copys (char *s)
{
  register int i;
  register char *cp;
  register char *result;

  i = 1;
  for (cp = s; *cp; cp++)
    i++;

  result = xmalloc((unsigned int)i);
  strcpy(result, s);
  return (result);
}


void
tabinit (void)
{
/*   register int i; JF unused */

  symtab = NEW2(TABSIZE, bucket *);

  firstsymbol = NULL;
  lastsymbol = NULL;
}


bucket *
getsym (char *key)
{
  register int hashval;
  register bucket *bp;
  register int found;

  hashval = hash(key);
  bp = symtab[hashval];

  found = 0;
  while (bp != NULL && found == 0)
    {
      if (strcmp(key, bp->tag) == 0)
	found = 1;
      else
	bp = bp->link;
    }

  if (found == 0)
    {
      nsyms++;

      bp = NEW(bucket);
      bp->link = symtab[hashval];
      bp->next = NULL;
      bp->tag = copys(key);
      bp->class = SUNKNOWN;

      if (firstsymbol == NULL)
	{
	  firstsymbol = bp;
	  lastsymbol = bp;
	}
      else
	{
	  lastsymbol->next = bp;
	  lastsymbol = bp;
	}

      symtab[hashval] = bp;
    }

  return (bp);
}


void
free_symtab (void)
{
  register int i;
  register bucket *bp,*bptmp;/* JF don't use ptr after free */

  for (i = 0; i < TABSIZE; i++)
    {
      bp = symtab[i];
      while (bp)
	{
	  bptmp = bp->link;
#if 0 /* This causes crashes because one string can appear more than once.  */
	  if (bp->type_name)
	    FREE(bp->type_name);
#endif
	  FREE(bp);
	  bp = bptmp;
	}
    }
  FREE(symtab);
}
