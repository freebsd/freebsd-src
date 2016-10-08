/* LibHnj is dual licensed under LGPL and MPL. Boilerplate for both
 * licenses follows.
 */

/* LibHnj - a library for high quality hyphenation and justification
 * Copyright (C) 1998 Raph Levien, (C) 2001 ALTLinux, Moscow
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the 
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330, 
 * Boston, MA  02111-1307  USA.
*/

/*
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.0 (the "MPL"); you may not use this file except in
 * compliance with the MPL.  You may obtain a copy of the MPL at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the MPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the MPL
 * for the specific language governing rights and limitations under the
 * MPL.
 *
 */

/*
 * Changes by Gunnar Ritter, Freiburg i. Br., Germany, August 2005.
 *
 * Sccsid @(#)hnjalloc.c	1.3 (gritter) 8/26/05
 */

/* wrappers for malloc */

#include <stdlib.h>
#include <stdio.h>
#include "hyphen.h"

void *
hnj_malloc (int size, HyphenDict *hp)
{
  void *p;

#if 0
  if (hp && hp->space == NULL)
	  hp->spptr = hp->space = malloc(hp->spacesize = 262144);
  if (hp && hp->space && &hp->spptr[size] < &hp->space[hp->spacesize]) {
	  p = hp->spptr;
	  hp->spptr += size;
  } else
#endif
  {
	  if ((p = malloc(size)) == NULL)
    		{
      		fprintf (stderr, "can't allocate %d bytes\n", size);
      		exit (1);
    		}
  }
  return p;
}

void *
hnj_realloc (void *p, int size, HyphenDict *hp)
{
#if 0
  if (hp && p >= (void *)hp->space && p < (void *)&hp->space[hp->spacesize])
	  abort();
#endif
  p = realloc (p, size);
  if (p == NULL)
    {
      fprintf (stderr, "can't allocate %d bytes\n", size);
      exit (1);
    }
  return p;
}

void
hnj_free (void *p, HyphenDict *hp)
{
#if 0
  if (hp && p >= (void *)hp->space && p < (void *)&hp->space[hp->spacesize])
	  return;
#endif
  free (p);
}

