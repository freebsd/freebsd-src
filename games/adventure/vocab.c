/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * The game adventure was originally written in Fortran by Will Crowther
 * and Don Woods.  It was later translated to C and enhanced by Jim
 * Gillogly.  This code is derived from software contributed to Berkeley
 * by Jim Gillogly at The Rand Corporation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
#if 0
static char sccsid[] = "@(#)vocab.c	8.1 (Berkeley) 5/31/93";
#endif
static const char rcsid[] =
 "$FreeBSD$";
#endif /* not lint */

/*      Re-coding of advent in C: data structure routines               */

#include <stdio.h>
#include <stdlib.h>
#include <err.h>
#include "hdr.h"

void
dstroy(object)
int object;
{       move(object,0);
}

void
juggle(object)
int object;
{       int i,j;

	i=place[object];
	j=fixed[object];
	move(object,i);
	move(object+100,j);
}


void
move(object,where)
int object,where;
{       int from;

	if (object<=100)
		from=place[object];
	else
		from=fixed[object-100];
	if (from>0 && from<=300) carry(object,from);
	drop(object,where);
}


int
put(object,where,pval)
int object,where,pval;
{       move(object,where);
	return(-1-pval);
}

void
carry(object,where)
int object,where;
{       int temp;

	if (object<=100)
	{       if (place[object]== -1) return;
		place[object] = -1;
		holdng++;
	}
	if (atloc[where]==object)
	{       atloc[where]=linkx[object];
		return;
	}
	for (temp=atloc[where]; linkx[temp]!=object; temp=linkx[temp]);
	linkx[temp]=linkx[object];
}


void
drop(object,where)
int object,where;
{	if (object>100) fixed[object-100]=where;
	else
	{       if (place[object]== -1) holdng--;
		place[object]=where;
	}
	if (where<=0) return;
	linkx[object]=atloc[where];
	atloc[where]=object;
}


int
vocab(word,type,value)                  /* look up or store a word      */
const char *word;
int type;       /* -2 for store, -1 for user word, >=0 for canned lookup*/
int value;                              /* used for storing only        */
{       int adr;
	const char *s;
	char *t;
	int hash, i;
	struct hashtab *h;

	for (hash=0,s=word,i=0; i<5 &&*s; i++)  /* some kind of hash    */
		hash += *s++;           /* add all chars in the word    */
	hash = (hash*3719)&077777;      /* pulled that one out of a hat */
	hash %= HTSIZE;                 /* put it into range of table   */

	for(adr=hash;; adr++)           /* look for entry in table      */
	{       if (adr==HTSIZE) adr=0; /* wrap around                  */
		h = &voc[adr];          /* point at the entry           */
		switch(type)
		{   case -2:            /* fill in entry                */
			if (h->val)     /* already got an entry?        */
				goto exitloop2;
			h->val=value;
			h->atab=malloc(strlen(word)+1);
			if (h->atab == NULL)
				errx(1, "Out of memory!");
			for (s=word,t=h->atab; *s;)
				*t++ = *s++ ^ '=';
			*t=0^'=';
			/* encrypt slightly to thwart core reader       */
		/*      printf("Stored \"%s\" (%d ch) as entry %d\n",   */
		/*              word, strlen(word)+1, adr);               */
			return(0);      /* entry unused                 */
		    case -1:            /* looking up user word         */
			if (h->val==0) return(-1);   /* not found    */
			for (s=word, t=h->atab;*t ^ '=';)
				if ((*s++ ^ '=') != *t++)
					goto exitloop2;
			if ((*s ^ '=') != *t && s-word<5) goto exitloop2;
			/* the word matched o.k.                        */
			return(h->val);
		    default:            /* looking up known word        */
			if (h->val==0)
			{       errx(1, "Unable to find %s in vocab", word);
			}
			for (s=word, t=h->atab;*t ^ '=';)
				if ((*s++ ^ '=') != *t++) goto exitloop2;
			/* the word matched o.k.                        */
			if (h->val/1000 != type) continue;
			return(h->val%1000);
		}

	    exitloop2:                  /* hashed entry does not match  */
		if (adr+1==hash || (adr==HTSIZE && hash==0))
		{       errx(1, "Hash table overflow");
		}
	}
}
